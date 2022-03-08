/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/wait.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

static struct session	*server_next_session(struct session *);
static void		 server_destroy_session_group(struct session *);

void
server_redraw_client(struct client *c)
{
	c->flags |= CLIENT_ALLREDRAWFLAGS;
}

void
server_status_client(struct client *c)
{
	c->flags |= CLIENT_REDRAWSTATUS;
}

void
server_redraw_session(struct session *s)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == s)
			server_redraw_client(c);
	}
}

void
server_redraw_session_group(struct session *s)
{
	struct session_group	*sg;

	if ((sg = session_group_contains(s)) == NULL)
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

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == s)
			server_status_client(c);
	}
}

void
server_status_session_group(struct session *s)
{
	struct session_group	*sg;

	if ((sg = session_group_contains(s)) == NULL)
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

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL && c->session->curw->window == w)
			server_redraw_client(c);
	}
}

void
server_redraw_window_borders(struct window *w)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL && c->session->curw->window == w)
			c->flags |= CLIENT_REDRAWBORDERS;
	}
}

void
server_status_window(struct window *w)
{
	struct session	*s;

	/*
	 * This is slightly different. We want to redraw the status line of any
	 * clients containing this window rather than anywhere it is the
	 * current window.
	 */

	RB_FOREACH(s, sessions, &sessions) {
		if (session_has(s, w))
			server_status_session(s);
	}
}

void
server_lock(void)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != NULL)
			server_lock_client(c);
	}
}

void
server_lock_session(struct session *s)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == s)
			server_lock_client(c);
	}
}

void
server_lock_client(struct client *c)
{
	const char	*cmd;

	if (c->flags & CLIENT_CONTROL)
		return;

	if (c->flags & CLIENT_SUSPENDED)
		return;

	cmd = options_get_string(c->session->options, "lock-command");
	if (*cmd == '\0' || strlen(cmd) + 1 > MAX_IMSGSIZE - IMSG_HEADER_SIZE)
		return;

	tty_stop_tty(&c->tty);
	tty_raw(&c->tty, tty_term_string(c->tty.term, TTYC_SMCUP));
	tty_raw(&c->tty, tty_term_string(c->tty.term, TTYC_CLEAR));
	tty_raw(&c->tty, tty_term_string(c->tty.term, TTYC_E3));

	c->flags |= CLIENT_SUSPENDED;
	proc_send(c->peer, MSG_LOCK, -1, cmd, strlen(cmd) + 1);
}

void
server_kill_pane(struct window_pane *wp)
{
	struct window	*w = wp->window;

	if (window_count_panes(w) == 1) {
		server_kill_window(w, 1);
		recalculate_sizes();
	} else {
		server_unzoom_window(w);
		server_client_remove_pane(wp);
		layout_close_pane(wp);
		window_remove_pane(w, wp);
		server_redraw_window(w);
	}
}

void
server_kill_window(struct window *w, int renumber)
{
	struct session	*s, *s1;
	struct winlink	*wl;

	RB_FOREACH_SAFE(s, sessions, &sessions, s1) {
		if (!session_has(s, w))
			continue;

		server_unzoom_window(w);
		while ((wl = winlink_find_by_window(&s->windows, w)) != NULL) {
			if (session_detach(s, wl)) {
				server_destroy_session_group(s);
				break;
			} else
				server_redraw_session_group(s);
		}

		if (renumber)
			server_renumber_session(s);
	}
	recalculate_sizes();
}

void
server_renumber_session(struct session *s)
{
	struct session_group	*sg;

	if (options_get_number(s->options, "renumber-windows")) {
		if ((sg = session_group_contains(s)) != NULL) {
			TAILQ_FOREACH(s, &sg->sessions, gentry)
			    session_renumber_windows(s);
		} else
			session_renumber_windows(s);
	}
}

void
server_renumber_all(void)
{
	struct session	*s;

	RB_FOREACH(s, sessions, &sessions)
		server_renumber_session(s);
}

int
server_link_window(struct session *src, struct winlink *srcwl,
    struct session *dst, int dstidx, int killflag, int selectflag,
    char **cause)
{
	struct winlink		*dstwl;
	struct session_group	*srcsg, *dstsg;

	srcsg = session_group_contains(src);
	dstsg = session_group_contains(dst);
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
			notify_session_window("window-unlinked", dst,
			    dstwl->window);
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
		dstidx = -1 - options_get_number(dst->options, "base-index");
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
server_destroy_pane(struct window_pane *wp, int notify)
{
	struct window		*w = wp->window;
	struct screen_write_ctx	 ctx;
	struct grid_cell	 gc;
	int			 remain_on_exit;
	const char		*s;
	char			*expanded;
	u_int			 sx = screen_size_x(&wp->base);
	u_int			 sy = screen_size_y(&wp->base);

	if (wp->fd != -1) {
#ifdef HAVE_UTEMPTER
		utempter_remove_record(wp->fd);
#endif
		bufferevent_free(wp->event);
		wp->event = NULL;
		close(wp->fd);
		wp->fd = -1;
	}

	remain_on_exit = options_get_number(wp->options, "remain-on-exit");
	if (remain_on_exit != 0 && (~wp->flags & PANE_STATUSREADY))
		return;
	switch (remain_on_exit) {
	case 0:
		break;
	case 2:
		if (WIFEXITED(wp->status) && WEXITSTATUS(wp->status) == 0)
			break;
		/* FALLTHROUGH */
	case 1:
		if (wp->flags & PANE_STATUSDRAWN)
			return;
		wp->flags |= PANE_STATUSDRAWN;

		gettimeofday(&wp->dead_time, NULL);
		if (notify)
			notify_pane("pane-died", wp);

		s = options_get_string(wp->options, "remain-on-exit-format");
		if (*s != '\0') {
			screen_write_start_pane(&ctx, wp, &wp->base);
			screen_write_scrollregion(&ctx, 0, sy - 1);
			screen_write_cursormove(&ctx, 0, sy - 1, 0);
			screen_write_linefeed(&ctx, 1, 8);
			memcpy(&gc, &grid_default_cell, sizeof gc);

			expanded = format_single(NULL, s, NULL, NULL, NULL, wp);
			format_draw(&ctx, &gc, sx, expanded, NULL, 0);
			free(expanded);

			screen_write_stop(&ctx);
		}
		wp->base.mode &= ~MODE_CURSOR;

		wp->flags |= PANE_REDRAW;
		return;
	}

	if (notify)
		notify_pane("pane-exited", wp);

	server_unzoom_window(w);
	server_client_remove_pane(wp);
	layout_close_pane(wp);
	window_remove_pane(w, wp);

	if (TAILQ_EMPTY(&w->panes))
		server_kill_window(w, 1);
	else
		server_redraw_window(w);
}

static void
server_destroy_session_group(struct session *s)
{
	struct session_group	*sg;
	struct session		*s1;

	if ((sg = session_group_contains(s)) == NULL)
		server_destroy_session(s);
	else {
		TAILQ_FOREACH_SAFE(s, &sg->sessions, gentry, s1) {
			server_destroy_session(s);
			session_destroy(s, 1, __func__);
		}
	}
}

static struct session *
server_next_session(struct session *s)
{
	struct session *s_loop, *s_out = NULL;

	RB_FOREACH(s_loop, sessions, &sessions) {
		if (s_loop == s)
			continue;
		if (s_out == NULL ||
		    timercmp(&s_loop->activity_time, &s_out->activity_time, <))
			s_out = s_loop;
	}
	return (s_out);
}

static struct session *
server_next_detached_session(struct session *s)
{
	struct session *s_loop, *s_out = NULL;

	RB_FOREACH(s_loop, sessions, &sessions) {
		if (s_loop == s || s_loop->attached)
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
	int		 detach_on_destroy;

	detach_on_destroy = options_get_number(s->options, "detach-on-destroy");
	if (detach_on_destroy == 0)
		s_new = server_next_session(s);
	else if (detach_on_destroy == 2)
		s_new = server_next_detached_session(s);
	else
		s_new = NULL;
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != s)
			continue;
		server_client_set_session(c, s_new);
		if (s_new == NULL)
			c->flags |= CLIENT_EXIT;
	}
	recalculate_sizes();
}

void
server_check_unattached(void)
{
	struct session	*s;

	/*
	 * If any sessions are no longer attached and have destroy-unattached
	 * set, collect them.
	 */
	RB_FOREACH(s, sessions, &sessions) {
		if (s->attached != 0)
			continue;
		if (options_get_number (s->options, "destroy-unattached"))
			session_destroy(s, 1, __func__);
	}
}

void
server_unzoom_window(struct window *w)
{
	if (window_unzoom(w) == 0)
		server_redraw_window(w);
}
