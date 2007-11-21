/* $Id: window-scroll.c,v 1.5 2007-11-21 15:35:53 nicm Exp $ */

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

struct window_scroll_mode_data {
	u_int	ox;
	u_int	oy;
	u_int	size;
};

void
window_scroll_init(struct window *w)
{
	struct window_scroll_mode_data	*data;

	w->modedata = data = xmalloc(sizeof *data);
	data->ox = data->oy = 0;
	data->size = w->screen.hsize;
}

void
window_scroll_resize(unused struct window *w, unused u_int sx, unused u_int sy)
{
}

void
window_scroll_draw(struct window *w, struct buffer *b, u_int py, u_int ny)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;
	char    			 buf[32];
	size_t		 		 len;

	if (s->hsize != data->size) {
		data->ox += s->hsize - data->size;
		data->size = s->hsize;
	}

	screen_draw(s, b, py, ny, data->ox, data->oy);
	input_store_zero(b, CODE_CURSOROFF);

	if (py == 0 && ny > 0) {
		len = screen_size_x(s);
		if (len > (sizeof buf) - 1)
			len = (sizeof buf) - 1;
		len = xsnprintf(
		    buf, len + 1, "[%u,%u/%u]", data->ox, data->oy, s->hsize);

		input_store_two(
		    b, CODE_CURSORMOVE, 0, screen_size_x(s) - len + 1);
		input_store_two(b, CODE_ATTRIBUTES, 0, status_colour);
		buffer_write(b, buf, len);
	}
}

void
window_scroll_key(struct window *w, int key)
{
	struct window_scroll_mode_data	*data = w->modedata;
	u_int				 ox, oy, sx, sy;
	
	sx = screen_size_x(&w->screen);
	sy = screen_size_y(&w->screen);

	ox = data->ox;
	oy = data->oy;

	switch (key) {
	case 'Q':
	case 'q':
		w->mode = NULL;
		xfree(w->modedata);

		recalculate_sizes();
		server_redraw_window_all(w);
		return;
	case 'h':
	case KEYC_LEFT:
		if (data->ox > 0)
			data->ox--;
		break;
	case 'l':
	case KEYC_RIGHT:
		if (data->ox < SHRT_MAX)
			data->ox++;
		break;
	case 'k':
	case 'K':
	case KEYC_UP:
		if (data->oy < data->size)
			data->oy++;
		break;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		if (data->oy > 0)
			data->oy--;
		break;
	case '\025':
	case KEYC_PPAGE:
		if (data->oy + sy > data->size)
			data->oy = data->size;
		else
			data->oy += sy;
		break;
	case '\006':
	case KEYC_NPAGE:
		if (data->oy < sy)
			data->oy = 0;
		else
			data->oy -= sy;
		break;
	}
	if (ox != data->ox || oy != data->oy)
		server_redraw_window_all(w);
}
