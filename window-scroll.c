/* $Id: window-scroll.c,v 1.3 2007-11-21 14:39:46 nicm Exp $ */

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
	u_int	off;
	u_int	size;
};

void
window_scroll_init(struct window *w)
{
	struct window_scroll_mode_data	*data;

	w->modedata = data = xmalloc(sizeof *data);
	data->off = 0;
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
		data->off += s->hsize - data->size;
		data->size = s->hsize;
	}

	screen_draw(s, b, py, ny, data->off);
	input_store_zero(b, CODE_CURSOROFF);

	if (py == 0 && ny > 0) {
		len = screen_size_x(s);
		if (len > (sizeof buf) - 1)
			len = (sizeof buf) - 1;
		len = xsnprintf(buf, len + 1, "{%u/%u}",
		    data->off, s->hsize);

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
	u_int				 off, sy = screen_size_y(&w->screen);

	off = data->off;
	switch (key) {
	case 'Q':
	case 'q':
		w->mode = NULL;
		xfree(w->modedata);

		recalculate_sizes();
		server_redraw_window_all(w);
		break;
	case 'k':
	case 'K':
	case KEYC_UP:
		if (data->off <  data->size)
			data->off++;
		break;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		if (data->off > 0)
			data->off--;
		break;
	case '\025':
	case KEYC_PPAGE:
		if (data->off + sy > data->size)
			data->off = data->size;
		else
			data->off += sy;
		break;
	case '\006':
	case KEYC_NPAGE:
		if (data->off < sy)
			data->off = 0;
		else
			data->off -= sy;
		break;
	}
	if (off != data->off)
		server_redraw_window_all(w);
}
