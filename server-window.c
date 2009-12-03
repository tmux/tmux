/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <event.h>
#include <unistd.h>

#include "tmux.h"

int	server_window_backoff(struct window_pane *);
int	server_window_check_bell(struct session *, struct window *);
int	server_window_check_activity(struct session *, struct window *);
int	server_window_check_content(
	    struct session *, struct window *, struct window_pane *);

/* Check if this window should suspend reading. */
int
server_window_backoff(struct window_pane *wp)
{
	struct client	*c;
	u_int		 i;

	if (!window_pane_visible(wp))
		return (0);

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if ((c->flags & (CLIENT_SUSPENDED|CLIENT_DEAD)) != 0)
			continue;
		if (c->session->curw->window != wp->window)
			continue;

		if (EVBUFFER_LENGTH(c->tty.event->output) > BACKOFF_THRESHOLD)
			return (1);
	}
	return (0);
}

/* Window functions that need to happen every loop. */
void
server_window_loop(void)
{
	struct window		*w;
	struct window_pane	*wp;
	struct session		*s;
	u_int		 	 i, j;

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;

		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd != -1) {
				if (server_window_backoff(wp))
					bufferevent_disable(wp->event, EV_READ);
				else
					bufferevent_enable(wp->event, EV_READ);
			}
		}

		for (j = 0; j < ARRAY_LENGTH(&sessions); j++) {
			s = ARRAY_ITEM(&sessions, j);
			if (s == NULL || !session_has(s, w))
				continue;

			if (server_window_check_bell(s, w) ||
			    server_window_check_activity(s, w))
				server_status_session(s);
			TAILQ_FOREACH(wp, &w->panes, entry)
				server_window_check_content(s, w, wp);
		}
		w->flags &= ~(WINDOW_BELL|WINDOW_ACTIVITY|WINDOW_CONTENT);
	}
}

/* Check for bell in window. */
int
server_window_check_bell(struct session *s, struct window *w)
{
	struct client	*c;
	u_int		 i;
	int		 action, visual;

	if (!(w->flags & WINDOW_BELL))
		return (0);

	if (session_alert_has_window(s, w, WINDOW_BELL))
		return (0);
	session_alert_add(s, w, WINDOW_BELL);

	action = options_get_number(&s->options, "bell-action");
	switch (action) {
	case BELL_ANY:
		if (s->flags & SESSION_UNATTACHED)
			break;
		visual = options_get_number(&s->options, "visual-bell");
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session != s)
				continue;
			if (!visual) {
				tty_putcode(&c->tty, TTYC_BEL);
				continue;
			}
			if (c->session->curw->window == w) {
				status_message_set(c, "Bell in current window");
				continue;
			}
			status_message_set(c, "Bell in window %u",
			    winlink_find_by_window(&s->windows, w)->idx);
		}
		break;
	case BELL_CURRENT:
		if (s->flags & SESSION_UNATTACHED)
			break;
		visual = options_get_number(&s->options, "visual-bell");
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session != s)
				continue;
			if (c->session->curw->window != w)
				continue;
			if (!visual) {
				tty_putcode(&c->tty, TTYC_BEL);
				continue;
			}
			status_message_set(c, "Bell in current window");
		}
		break;
	}

	return (1);
}

/* Check for activity in window. */
int
server_window_check_activity(struct session *s, struct window *w)
{
	struct client	*c;
	u_int		 i;

	if (!(w->flags & WINDOW_ACTIVITY))
		return (0);
	if (s->curw->window == w)
		return (0);

	if (!options_get_number(&w->options, "monitor-activity"))
		return (0);

	if (session_alert_has_window(s, w, WINDOW_ACTIVITY))
		return (0);
	session_alert_add(s, w, WINDOW_ACTIVITY);

	if (s->flags & SESSION_UNATTACHED)
		return (0);
	if (options_get_number(&s->options, "visual-activity")) {
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session != s)
				continue;
			status_message_set(c, "Activity in window %u",
			    winlink_find_by_window(&s->windows, w)->idx);
		}
	}

	return (1);
}

/* Check for content change in window. */
int
server_window_check_content(
    struct session *s, struct window *w, struct window_pane *wp)
{
	struct client	*c;
	u_int		 i;
	char		*found, *ptr;

	if (!(w->flags & WINDOW_ACTIVITY))	/* activity for new content */
		return (0);
	if (s->curw->window == w)
		return (0);

	ptr = options_get_string(&w->options, "monitor-content");
	if (ptr == NULL || *ptr == '\0')
		return (0);

	if (session_alert_has_window(s, w, WINDOW_CONTENT))
		return (0);

	if ((found = window_pane_search(wp, ptr, NULL)) == NULL)
		return (0);
	xfree(found);

	session_alert_add(s, w, WINDOW_CONTENT);
	if (s->flags & SESSION_UNATTACHED)
		return (0);
	if (options_get_number(&s->options, "visual-content")) {
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c == NULL || c->session != s)
				continue;
			status_message_set(c, "Content in window %u",
			    winlink_find_by_window(&s->windows, w)->idx);
		}
	}

	return (1);
}
