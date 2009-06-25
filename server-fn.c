/* $OpenBSD: server-fn.c,v 1.2 2009/06/25 06:15:04 nicm Exp $ */

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
#include <sys/time.h>

#include <string.h>
#include <unistd.h>

#include "tmux.h"

int	server_lock_callback(void *, const char *);

const char **
server_fill_environ(struct session *s)
{
	static const char *env[] = { NULL /* TMUX= */, "TERM=screen", NULL };
	static char	tmuxvar[MAXPATHLEN + 256];
	u_int		idx;

	if (session_index(s, &idx) != 0)
		fatalx("session not found");

	xsnprintf(tmuxvar, sizeof tmuxvar,
	    "TMUX=%s,%ld,%u", socket_path, (long) getpid(), idx);
	env[0] = tmuxvar;

	return (env);
}

void
server_write_client(
    struct client *c, enum hdrtype type, const void *buf, size_t len)
{
	struct hdr	 hdr;

	log_debug("writing %d to client %d", type, c->fd);

	hdr.type = type;
	hdr.size = len;

	buffer_write(c->out, &hdr, sizeof hdr);
	if (buf != NULL && len > 0)
		buffer_write(c->out, buf, len);
}

void
server_write_session(
    struct session *s, enum hdrtype type, const void *buf, size_t len)
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
		status_prompt_set(
		    c, "Password: ", server_lock_callback, c, PROMPT_HIDDEN);
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
	return (0);

wrong:
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL)
			continue;

		*c->prompt_buffer = '\0';
		c->prompt_index = 0;
  		server_status_client(c);
	}

	return (-1);
}
