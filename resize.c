/* $Id: resize.c,v 1.1 2007-10-04 19:03:51 nicm Exp $ */

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
			if (c == NULL || c->session != s)
				continue;
			if (c->sx < ssx)
				ssx = c->sx;
			if (c->sy < ssy)
				ssy = c->sy;
		}
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
			if (s == NULL || !session_has(s, w))
				continue;
			if (s->sx < ssx)
				ssx = s->sx;
			if (s->sy < ssy)
				ssy = s->sy;
		}
		if (w->screen.sx == ssx && w->screen.sy == ssy)
			continue;

		log_debug("window size %u,%u (was %u,%u)", 
		    ssx, ssy, w->screen.sx, w->screen.sy);

		server_clear_window(w);
		window_resize(w, ssx, ssy);
		server_redraw_window(w);
	}
}
