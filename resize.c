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

#include <string.h>

#include "tmux.h"

/*
 * Recalculate window and session sizes.
 *
 * Every session has the size of the smallest client it is attached to and
 * every window the size of the smallest session it is attached to.
 *
 * So, when a client is resized or a session attached to or detached from a
 * client, the window sizes must be recalculated. For each session, find the
 * smallest client it is attached to, and resize it to that size. Then for
 * every window, find the smallest session it is attached to, resize it to that
 * size and clear and redraw every client with it as the current window.
 *
 * This is quite inefficient - better/additional data structures are needed
 * to make it better.
 *
 * As a side effect, this function updates the SESSION_UNATTACHED flag. This
 * flag is necessary to make sure unattached sessions do not limit the size of
 * windows that are attached both to them and to other (attached) sessions.
 */

void
recalculate_sizes(void)
{
	struct session		*s;
	struct client		*c;
	struct window		*w;
	struct window_pane	*wp;
	u_int			 ssx, ssy, has, limit;
	int			 flag, has_status, is_zoomed, forced;

	RB_FOREACH(s, sessions, &sessions) {
		has_status = options_get_number(s->options, "status");

		s->attached = 0;
		ssx = ssy = UINT_MAX;
		TAILQ_FOREACH(c, &clients, entry) {
			if (c->flags & CLIENT_SUSPENDED)
				continue;
			if ((c->flags & (CLIENT_CONTROL|CLIENT_SIZECHANGED)) ==
			    CLIENT_CONTROL)
				continue;
			if (c->session == s) {
				if (c->tty.sx < ssx)
					ssx = c->tty.sx;
				if (has_status &&
				    !(c->flags & CLIENT_CONTROL) &&
				    c->tty.sy > 1 && c->tty.sy - 1 < ssy)
					ssy = c->tty.sy - 1;
				else if (c->tty.sy < ssy)
					ssy = c->tty.sy;
				s->attached++;
			}
		}
		if (ssx == UINT_MAX || ssy == UINT_MAX) {
			s->flags |= SESSION_UNATTACHED;
			continue;
		}
		s->flags &= ~SESSION_UNATTACHED;

		if (has_status && ssy == 0)
			ssy = 1;

		if (s->sx == ssx && s->sy == ssy)
			continue;

		log_debug("session $%u size %u,%u (was %u,%u)", s->id, ssx, ssy,
		    s->sx, s->sy);

		s->sx = ssx;
		s->sy = ssy;

		status_update_saved(s);
	}

	RB_FOREACH(w, windows, &windows) {
		if (w->active == NULL)
			continue;
		flag = options_get_number(w->options, "aggressive-resize");

		ssx = ssy = UINT_MAX;
		RB_FOREACH(s, sessions, &sessions) {
			if (s->flags & SESSION_UNATTACHED)
				continue;
			if (flag)
				has = s->curw->window == w;
			else
				has = session_has(s, w);
			if (has) {
				if (s->sx < ssx)
					ssx = s->sx;
				if (s->sy < ssy)
					ssy = s->sy;
			}
		}
		if (ssx == UINT_MAX || ssy == UINT_MAX)
			continue;

		forced = 0;
		limit = options_get_number(w->options, "force-width");
		if (limit >= PANE_MINIMUM && ssx > limit) {
			ssx = limit;
			forced |= WINDOW_FORCEWIDTH;
		}
		limit = options_get_number(w->options, "force-height");
		if (limit >= PANE_MINIMUM && ssy > limit) {
			ssy = limit;
			forced |= WINDOW_FORCEHEIGHT;
		}

		if (w->sx == ssx && w->sy == ssy)
			continue;
		log_debug("window @%u size %u,%u (was %u,%u)", w->id, ssx, ssy,
		    w->sx, w->sy);

		w->flags &= ~(WINDOW_FORCEWIDTH|WINDOW_FORCEHEIGHT);
		w->flags |= forced;

		is_zoomed = w->flags & WINDOW_ZOOMED;
		if (is_zoomed)
			window_unzoom(w);
		layout_resize(w, ssx, ssy);
		window_resize(w, ssx, ssy);
		if (is_zoomed && window_pane_visible(w->active))
			window_zoom(w->active);

		/*
		 * If the current pane is now not visible, move to the next
		 * that is.
		 */
		wp = w->active;
		while (!window_pane_visible(w->active)) {
			w->active = TAILQ_PREV(w->active, window_panes, entry);
			if (w->active == NULL)
				w->active = TAILQ_LAST(&w->panes, window_panes);
			if (w->active == wp)
			       break;
		}
		if (w->active == w->last)
			w->last = NULL;

		server_redraw_window(w);
		notify_window("window-layout-changed", w);
	}
}
