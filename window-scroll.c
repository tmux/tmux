/* $Id: window-scroll.c,v 1.1 2007-11-21 13:11:41 nicm Exp $ */

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

#include "tmux.h"

void	window_scroll_init(struct window *);
void	window_scroll_resize(struct window *, u_int, u_int);
void	window_scroll_draw(struct window *, struct buffer *, u_int, u_int);
void	window_scroll_key(struct window *, int);

const struct window_mode window_scroll_mode = {
	window_scroll_init,
	window_scroll_resize,
	window_scroll_draw,
	window_scroll_key
};

u_int	window_scroll_offset;
u_int	window_scroll_size;

void
window_scroll_init(struct window *w)
{
	window_scroll_offset = 0;
	window_scroll_size = w->screen.hsize;
}

void
window_scroll_resize(struct window *w, u_int sx, u_int sy)
{
}

void
window_scroll_draw(struct window *w, struct buffer *b, u_int py, u_int ny)
{
	struct screen	*s = &w->screen;
	char    	 buf[32];
	size_t		 len;

	if (s->hsize != window_scroll_size) {
		window_scroll_offset += s->hsize - window_scroll_size;
		window_scroll_size = s->hsize;
	}

	screen_draw(s, b, py, ny, window_scroll_offset);
	input_store_zero(b, CODE_CURSOROFF);

	if (py == 0 && ny > 0) {
		len = screen_size_x(s);
		if (len > (sizeof buf) - 1)
			len = (sizeof buf) - 1;
		len = xsnprintf(buf, len + 1, "{%u/%u}",
		    window_scroll_offset, s->hsize);

		input_store_two(
		    b, CODE_CURSORMOVE, 0, screen_size_x(s) - len + 1);
		input_store_two(b, CODE_ATTRIBUTES, 0, status_colour);
		buffer_write(b, buf, len);
	}
}

void
window_scroll_key(struct window *w, int key)
{
	u_int	sy = screen_size_y(&w->screen);

	switch (key) {
	case 'Q':
	case 'q':
		w->mode = NULL;
		recalculate_sizes();
		server_redraw_window_all(w);
		break;
	case 'k':
	case 'K':
	case KEYC_UP:
		if (window_scroll_offset <  window_scroll_size)
			window_scroll_offset++;
		server_redraw_window_all(w);
		break;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		if (window_scroll_offset > 0)
			window_scroll_offset--;
		server_redraw_window_all(w);
		break;
	case '\025':
	case KEYC_PPAGE:
		if (window_scroll_offset + sy > window_scroll_size)
			window_scroll_offset = window_scroll_size;
		else
			window_scroll_offset += sy;
		server_redraw_window_all(w);
		break;
	case '\006':
	case KEYC_NPAGE:
		if (window_scroll_offset < sy)
			window_scroll_offset = 0;
		else
			window_scroll_offset -= sy;
		server_redraw_window_all(w);
		break;
	}
}
