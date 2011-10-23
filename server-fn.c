/* $Id$ */

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

struct session *server_next_session(struct session *);
void		server_callback_identify(int, short, void *);

void
server_fill_environ(struct session *s, struct environ *env)
{
	char	var[MAXPATHLEN], *term;
	u_int	idx;
	long	pid;

	if (s != NULL) {
		term = options_get_string(&s->options, "default-terminal");
		environ_set(env, "TERM", term);

		idx = s->idx;
	} else
		idx = -1;
	pid = getpid();
	xsnprintf(var, sizeof var, "%s,%ld,%d", socket_path, pid, idx);
	environ_set(env, "TMUX", var);
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
	server_update_event(c);
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
server_redraw_session_group(struct session *s)
{
	struct session_group	*sg;

	if ((sg = session_group_find(s)) == NULL)
		server_redraw_session(s);
	else {
		TAILQ_FOREACH(s, &sg->sessions, gentry)
			server_redraw_session(s);
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
server_status_session_group(struct session *s)
{
	struct session_group	*sg;

	if ((sg = session_group_find(s)) == NULL)
		server_status_session(s);
	else {
		TAILQ_FOREACH(s, &sg->sessions, gentry)
			server_status_session(s);
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
server_redraw_window_borders(struct window *w)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session->curw->window == w)
			c->flags |= CLIENT_BORDERS;
	}
}

void
server_status_window(struct window *w)
{
	struct session	*s;

	/*
	 * This is slightly different. We want to redraw the status line of any
	 * clients containing this window rather than any where it is the
	 * current window.
	 */

	RB_FOREACH(s, sessions, &sessions) {
		if (session_has(s, w) != NULL)
			server_status_session(s);
	}
}

void
server_lock(void)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		server_lock_client(c);
	}
}

void
server_lock_session(struct session *s)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL || c->session != s)
			continue;
		server_lock_client(c);
	}
}

void
server_lock_client(struct client *c)
{
	const char		*cmd;
	size_t			 cmdlen;
	struct msg_lock_data	 lockdata;

	if (c->flags & CLIENT_SUSPENDED)
		return;

	cmd = options_get_string(&c->session->options, "lock-command");
	cmdlen = strlcpy(lockdata.cmd, cmd, sizeof lockdata.cmd);
	if (cmdlen >= sizeof lockdata.cmd)
		return;

	tty_stop_tty(&c->tty);
	tty_raw(&c->tty, tty_term_string(c->tty.term, TTYC_SMCUP));
	tty_raw(&c->tty, tty_term_string(c->tty.term, TTYC_CLEAR));
	tty_raw(&c->tty, tty_term_string(c->tty.term, TTYC_E3));

	c->flags |= CLIENT_SUSPENDED;
	server_write_client(c, MSG_LOCK, &lockdata, sizeof lockdata);
}

void
server_kill_window(struct window *w)
{
	struct session	*s, *next_s;
	struct winlink	*wl;

	next_s = RB_MIN(sessions, &sessions);
	while (next_s != NULL) {
		s = next_s;
		next_s = RB_NEXT(sessions, &sessions, s);

		if (session_has(s, w) == NULL)
			continue;
		while ((wl = winlink_find_by_window(&s->windows, w)) != NULL) {
			if (session_detach(s, wl)) {
				server_destroy_session_group(s);
				break;
			} else
				server_redraw_session_group(s);
		}
	}
}

int
server_link_window(struct session *src, struct winlink *srcwl,
    struct session *dst, int dstidx, int killflag, int selectflag, char **cause)
{
	struct winlink		*dstwl;
	struct session_group	*srcsg, *dstsg;

	srcsg = session_group_find(src);
	dstsg = session_group_find(dst);
	if (src != dst && srcsg != NULL && dstsg != NULL && srcsg == dstsg) {
		xasprintf(cause, "sessions are grouped");
		return (-1);
	}

	dstwl = NULL;
	if (dstidx != -1)
		dstwl = winlink_find_by_index(&dst->windows, dstidx);
	if (dstwl != NULL) {
		if (dstwl->window == srcwl->window) {
			xasprintf(cause, "same index: %d", dstidx);
			return (-1);
		}
		if (killflag) {
			/*
			 * Can't use session_detach as it will destroy session
			 * if this makes it empty.
			 */
			dstwl->flags &= ~WINLINK_ALERTFLAGS;
			winlink_stack_remove(&dst->lastw, dstwl);
			winlink_remove(&dst->windows, dstwl);

			/* Force select/redraw if current. */
			if (dstwl == dst->curw) {
				selectflag = 1;
				dst->curw = NULL;
			}
		}
	}

	if (dstidx == -1)
		dstidx = -1 - options_get_number(&dst->options, "base-index");
	dstwl = session_attach(dst, srcwl->window, dstidx, cause);
	if (dstwl == NULL)
		return (-1);

	if (selectflag)
		session_select(dst, dstwl->idx);
	server_redraw_session_group(dst);

	return (0);
}

void
server_unlink_window(struct session *s, struct winlink *wl)
{
	if (session_detach(s, wl))
		server_destroy_session_group(s);
	else
		server_redraw_session_group(s);
}

void
server_destroy_pane(struct window_pane *wp)
{
	struct window		*w = wp->window;
	int			 old_fd;
	struct screen_write_ctx	 ctx;
	struct grid_cell	 gc;

	old_fd = wp->fd;
	if (wp->fd != -1) {
		close(wp->fd);
		bufferevent_free(wp->event);
		wp->fd = -1;
	}

	if (options_get_number(&w->options, "remain-on-exit")) {
		if (old_fd == -1)
			return;
		screen_write_start(&ctx, wp, &wp->base);
		screen_write_scrollregion(&ctx, 0, screen_size_y(ctx.s) - 1);
		screen_write_cursormove(&ctx, 0, screen_size_y(ctx.s) - 1);
		screen_write_linefeed(&ctx, 1);
		memcpy(&gc, &grid_default_cell, sizeof gc);
		gc.attr |= GRID_ATTR_BRIGHT;
		screen_write_puts(&ctx, &gc, "Pane is dead");
		screen_write_stop(&ctx);
		wp->flags |= PANE_REDRAW;
		return;
	}

	layout_close_pane(wp);
	window_remove_pane(w, wp);

	if (TAILQ_EMPTY(&w->panes))
		server_kill_window(w);
	else
		server_redraw_window(w);
}

void
server_destroy_session_group(struct session *s)
{
	struct session_group	*sg;

	if ((sg = session_group_find(s)) == NULL)
		server_destroy_session(s);
	else {
		TAILQ_FOREACH(s, &sg->sessions, gentry)
			server_destroy_session(s);
		TAILQ_REMOVE(&session_groups, sg, entry);
		xfree(sg);
	}
}

struct session *
server_next_session(struct session *s)
{
	struct session *s_loop, *s_out;

	s_out = NULL;
	RB_FOREACH(s_loop, sessions, &sessions) {
		if (s_loop == s)
			continue;
		if (s_out == NULL ||
		    timercmp(&s_loop->activity_time, &s_out->activity_time, <))
			s_out = s_loop;
	}
	return (s_out);
}

void
server_destroy_session(struct session *s)
{
	struct client	*c;
	struct session	*s_new;
	u_int		 i;

	if (!options_get_number(&s->options, "detach-on-destroy"))
		s_new = server_next_session(s);
	else
		s_new = NULL;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session != s)
			continue;
		if (s_new == NULL) {
			c->session = NULL;
			c->flags |= CLIENT_EXIT;
		} else {
			c->last_session = NULL;
			c->session = s_new;
			session_update_activity(s_new);
			server_redraw_client(c);
		}
	}
	recalculate_sizes();
}

void
server_check_unattached (void)
{
	struct session	*s;

	/*
	 * If any sessions are no longer attached and have destroy-unattached
	 * set, collect them.
	 */
	RB_FOREACH(s, sessions, &sessions) {
		if (!(s->flags & SESSION_UNATTACHED))
			continue;
		if (options_get_number (&s->options, "destroy-unattached"))
			session_destroy(s);
	}
}

void
server_set_identify(struct client *c)
{
	struct timeval	tv;
	int		delay;

	delay = options_get_number(&c->session->options, "display-panes-time");
	tv.tv_sec = delay / 1000;
	tv.tv_usec = (delay % 1000) * 1000L;

	evtimer_del(&c->identify_timer);
	evtimer_set(&c->identify_timer, server_callback_identify, c);
	evtimer_add(&c->identify_timer, &tv);

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

/* ARGSUSED */
void
server_callback_identify(unused int fd, unused short events, void *data)
{
	struct client	*c = data;

	server_clear_identify(c);
}

void
server_update_event(struct client *c)
{
	short	events;

	events = 0;
	if (!(c->flags & CLIENT_BAD))
		events |= EV_READ;
	if (c->ibuf.w.queued > 0)
		events |= EV_WRITE;
	event_del(&c->event);
	event_set(&c->event, c->ibuf.fd, events, server_client_callback, c);
	event_add(&c->event, NULL);
}
