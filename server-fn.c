/* $Id: server-fn.c,v 1.82 2009-08-31 22:30:15 tcunha Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

int	server_lock_callback(void *, const char *);

void
server_fill_environ(struct session *s, struct environ *env)
{
	char		 tmuxvar[MAXPATHLEN], *term;
	u_int		 idx;

	if (session_index(s, &idx) != 0)
		fatalx("session not found");
	xsnprintf(tmuxvar, sizeof tmuxvar,
	    "%s,%ld,%u", socket_path, (long) getpid(), idx);
	environ_set(env, "TMUX", tmuxvar);

	term = options_get_string(&s->options, "default-terminal");
	environ_set(env, "TERM", term);
}

void
server_write_error(struct client *c, const char *msg)
{
	struct msg_print_data	printdata;

	strlcpy(printdata.msg, msg, sizeof printdata.msg);
	server_write_client(c, MSG_ERROR, &printdata, sizeof printdata);
}

void
server_write_client(
    struct client *c, enum msgtype type, const void *buf, size_t len)
{
	struct imsgbuf	*ibuf = &c->ibuf;

	if (c->flags & CLIENT_BAD)
		return;
	log_debug("writing %d to client %d", type, c->ibuf.fd);
	imsg_compose(ibuf, type, PROTOCOL_VERSION, -1, -1, (void *) buf, len);
}

void
server_write_session(
    struct session *s, enum msgtype type, const void *buf, size_t len)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session == s)
			server_write_client(c, type, buf, len);
	}
}

void
server_redraw_client(struct client *c)
{
	c->flags |= CLIENT_REDRAW;
}

void
server_status_client(struct client *c)
{
	c->flags |= CLIENT_STATUS;
}

void
server_redraw_session(struct session *s)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session == s)
			server_redraw_client(c);
	}
}

void
server_status_session(struct session *s)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session == s)
			server_status_client(c);
	}
}

void
server_redraw_window(struct window *w)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session->curw->window == w)
			server_redraw_client(c);
	}
	w->flags |= WINDOW_REDRAW;
}

void
server_status_window(struct window *w)
{
	struct session	*s;
	u_int		 i;

	/*
	 * This is slightly different. We want to redraw the status line of any
	 * clients containing this window rather than any where it is the
	 * current window.
	 */

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s != NULL && session_has(s, w))
			server_status_session(s);
	}
}

void
server_lock(void)
{
	struct client	*c;
	u_int		 i;

	if (server_locked)
		return;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;

		status_prompt_clear(c);
		status_prompt_set(c,
		    "Password: ", server_lock_callback, NULL, c, PROMPT_HIDDEN);
  		server_redraw_client(c);
	}
	server_locked = 1;
}

int
server_lock_callback(unused void *data, const char *s)
{
	return (server_unlock(s));
}

int
server_unlock(const char *s)
{
	struct client	*c;
	u_int		 i;
	char		*out;

	if (!server_locked)
		return (0);
	server_activity = time(NULL);

	if (server_password != NULL) {
		if (s == NULL)
			return (-1);
		out = crypt(s, server_password);
		if (strcmp(out, server_password) != 0)
			goto wrong;
	}

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL)
			continue;

		status_prompt_clear(c);
  		server_redraw_client(c);
	}

	server_locked = 0;
	password_failures = 0;
	return (0);

wrong:
	password_failures++;
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->prompt_buffer == NULL)
			continue;

		*c->prompt_buffer = '\0';
		c->prompt_index = 0;
  		server_redraw_client(c);
	}

	return (-1);
}

void
server_kill_window(struct window *w)
{
	struct session	*s;
	struct winlink	*wl;
	struct client	*c;
	u_int		 i, j;
	int		 destroyed;

	
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL || !session_has(s, w))
			continue;
		if ((wl = winlink_find_by_window(&s->windows, w)) == NULL)
			continue;
		
		destroyed = session_detach(s, wl);
		for (j = 0; j < ARRAY_LENGTH(&clients); j++) {
			c = ARRAY_ITEM(&clients, j);
			if (c == NULL || c->session != s)
				continue;
			
			if (destroyed) {
				c->session = NULL;
				server_write_client(c, MSG_EXIT, NULL, 0);
			} else
				server_redraw_client(c);
		}
	}
	recalculate_sizes();
}

void
server_set_identify(struct client *c)
{
	struct timeval	tv;
	int		delay;

	delay = options_get_number(&c->session->options, "display-panes-time");
	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;

	if (gettimeofday(&c->identify_timer, NULL) != 0)
		fatal("gettimeofday");
	timeradd(&c->identify_timer, &tv, &c->identify_timer);

	c->flags |= CLIENT_IDENTIFY;
	c->tty.flags |= (TTY_FREEZE|TTY_NOCURSOR);
	server_redraw_client(c);
}

void
server_clear_identify(struct client *c)
{
	if (c->flags & CLIENT_IDENTIFY) {
		c->flags &= ~CLIENT_IDENTIFY;
		c->tty.flags &= ~(TTY_FREEZE|TTY_NOCURSOR);
		server_redraw_client(c);
	}
}
