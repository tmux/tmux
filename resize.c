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
	u_int		 	 i, j, ssx, ssy, has, limit;
	int		 	 flag;

	RB_FOREACH(s, sessions, &sessions) {
		ssx = ssy = UINT_MAX;
		for (j = 0; j < ARRAY_LENGTH(&clients); j++) {
			c = ARRAY_ITEM(&clients, j);
			if (c == NULL || c->flags & CLIENT_SUSPENDED)
				continue;
			if (c->session == s) {
				if (c->tty.sx < ssx)
					ssx = c->tty.sx;
				if (c->tty.sy < ssy)
					ssy = c->tty.sy;
			}
		}
		if (ssx == UINT_MAX || ssy == UINT_MAX) {
			s->flags |= SESSION_UNATTACHED;
			continue;
		}
		s->flags &= ~SESSION_UNATTACHED;

		if (options_get_number(&s->options, "status")) {
			if (ssy == 0)
				ssy = 1;
			else
				ssy--;
		}
		if (s->sx == ssx && s->sy == ssy)
			continue;

		log_debug(
		    "session size %u,%u (was %u,%u)", ssx, ssy, s->sx, s->sy);

		s->sx = ssx;
		s->sy = ssy;
	}

	for (i = 0; i < ARRAY_LENGTH(&windows); i++) {
		w = ARRAY_ITEM(&windows, i);
		if (w == NULL)
			continue;
		flag = options_get_number(&w->options, "aggressive-resize");

		ssx = ssy = UINT_MAX;
		RB_FOREACH(s, sessions, &sessions) {
			if (s->flags & SESSION_UNATTACHED)
				continue;
			if (flag)
				has = s->curw->window == w;
			else
				has = session_has(s, w) != NULL;
			if (has) {
				if (s->sx < ssx)
					ssx = s->sx;
				if (s->sy < ssy)
					ssy = s->sy;
			}
		}
		if (ssx == UINT_MAX || ssy == UINT_MAX)
			continue;

		limit = options_get_number(&w->options, "force-width");
		if (limit != 0 && ssx > limit)
			ssx = limit;
		limit = options_get_number(&w->options, "force-height");
		if (limit != 0 && ssy > limit)
			ssy = limit;

		if (w->sx == ssx && w->sy == ssy)
			continue;

		log_debug(
		    "window size %u,%u (was %u,%u)", ssx, ssy, w->sx, w->sy);

		layout_resize(w, ssx, ssy);
		window_resize(w, ssx, ssy);

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

		server_redraw_window(w);
		notify_window_layout_changed(w);
	}
}
