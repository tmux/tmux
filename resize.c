/* $Id: resize.c,v 1.5 2007-11-20 21:42:29 nicm Exp $ */

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
 * windows that are attached both to them and to other sessions which are
 * attached.
 */

void
recalculate_sizes(void)
{
	struct session	*s;
	struct client	*c;
	struct window	*w;
	u_int		 i, j, ssx, ssy;
	
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL)
			continue;

		ssx = ssy = UINT_MAX;
		for (j = 0; j < ARRAY_LENGTH(&clients); j++) {
			c = ARRAY_ITEM(&clients, j);
			if (c == NULL)
				continue;
			if (c->session == s) {
				if (c->sx < ssx)
					ssx = c->sx;
				if (c->sy < ssy)
					ssy = c->sy;
			}
		}
		if (ssx == UINT_MAX || ssy == UINT_MAX) {
			s->flags |= SESSION_UNATTACHED;
			continue;
		}
		s->flags &= ~SESSION_UNATTACHED;

		if (ssy < status_lines)
			ssy = status_lines + 1;
		ssy -= status_lines;
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

		ssx = ssy = UINT_MAX;
		for (j = 0; j < ARRAY_LENGTH(&sessions); j++) {
			s = ARRAY_ITEM(&sessions, j);
			if (s == NULL || s->flags & SESSION_UNATTACHED)
				continue;
			if (session_has(s, w)) {
				if (s->sx < ssx)
					ssx = s->sx;
				if (s->sy < ssy)
					ssy = s->sy;
			}
		}
		if (ssx == UINT_MAX || ssy == UINT_MAX)
			continue;

		if (screen_size_x(&w->screen) == ssx &&
		    screen_size_y(&w->screen) == ssy)
			continue;

		log_debug("window size %u,%u (was %u,%u)", ssx, ssy,
		    screen_size_x(&w->screen), screen_size_y(&w->screen));

		server_clear_window_cur(w);
		window_resize(w, ssx, ssy);
		server_redraw_window_cur(w);
	}
}
