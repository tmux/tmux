/* $Id: window-scroll.c,v 1.17 2008-01-03 21:32:11 nicm Exp $ */

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

struct screen *window_scroll_init(struct window *);
void	window_scroll_free(struct window *);
void	window_scroll_resize(struct window *, u_int, u_int);
void	window_scroll_key(struct window *, int);

void	window_scroll_redraw_screen(struct window *);
void	window_scroll_write_line(
    	    struct window *, struct screen_write_ctx *, u_int);
void	window_scroll_write_column(
    	    struct window *, struct screen_write_ctx *, u_int);

void	window_scroll_scroll_up(struct window *);
void	window_scroll_scroll_down(struct window *);
void	window_scroll_scroll_left(struct window *);
void	window_scroll_scroll_right(struct window *);

const struct window_mode window_scroll_mode = {
	window_scroll_init,
	window_scroll_free,
	window_scroll_resize,
	window_scroll_key
};

struct window_scroll_mode_data {
	struct screen	screen;

	u_int		ox;
	u_int		oy;
};

struct screen *
window_scroll_init(struct window *w)
{
	struct window_scroll_mode_data	*data;
	struct screen			*s;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	w->modedata = data = xmalloc(sizeof *data);
	data->ox = 0;
	data->oy = 0;

	s = &data->screen;
	screen_create(s, screen_size_x(&w->base), screen_size_y(&w->base));
	s->mode &= ~MODE_CURSOR;

	screen_write_start(&ctx, s, NULL, NULL);
	for (i = 0; i < screen_size_y(s); i++)
		window_scroll_write_line(w, &ctx, i);
	screen_write_stop(&ctx);

	return (s);
}

void
window_scroll_free(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;

	screen_destroy(&data->screen);
	xfree(data);
}

void
window_scroll_resize(struct window *w, u_int sx, u_int sy)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	screen_resize(s, sx, sy);
	screen_display_copy_area(&data->screen, &w->base,
	    0, 0, screen_size_x(s), screen_size_y(s), data->ox, data->oy);
}

void
window_scroll_key(struct window *w, int key)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	switch (key) {
	case 'Q':
	case 'q':
		window_reset_mode(w);
		break;
	case 'h':
	case KEYC_LEFT:
		window_scroll_scroll_left(w);
		break;
	case 'l':
	case KEYC_RIGHT:
		window_scroll_scroll_right(w);
		break;
	case 'k':
	case 'K':
	case KEYC_UP:
		window_scroll_scroll_up(w);
		break;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		window_scroll_scroll_down(w);
		break;
	case '\025':	/* C-u */
	case KEYC_PPAGE:
		if (data->oy + screen_size_y(s) > w->base.hsize)
			data->oy = w->base.hsize;
		else
			data->oy += screen_size_y(s);
		window_scroll_redraw_screen(w);
		break;
	case '\006':	/* C-f */
	case KEYC_NPAGE:
		if (data->oy < screen_size_y(s))
			data->oy = 0;
		else
			data->oy -= screen_size_y(s);
		window_scroll_redraw_screen(w);
		break;
	}
}

void
window_scroll_write_line(
    struct window *w, struct screen_write_ctx *ctx, u_int py)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	size_t	 			 size;

	if (py == 0) {
		screen_write_set_attributes(ctx, 0, status_colour);
		screen_write_move_cursor(ctx, 0, 0);
		size = screen_write_put_string_rjust(
		    ctx, "[%u,%u/%u]", data->ox, data->oy, w->base.hsize);
	} else
		size = 0;
	screen_write_move_cursor(ctx, 0, py);
	screen_write_copy_area(
	    ctx, &w->base, screen_size_x(s) - size, 1, data->ox, data->oy);
}

void
window_scroll_write_column(
    struct window *w, struct screen_write_ctx *ctx, u_int px)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	screen_write_move_cursor(ctx, px, 0);
	screen_write_copy_area(
	    ctx, &w->base, 1, screen_size_y(s), data->ox, data->oy);
}

void
window_scroll_redraw_screen(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start_window(&ctx, w);
	for (i = 0; i < screen_size_y(s); i++)
		window_scroll_write_line(w, &ctx, i);
	screen_write_stop(&ctx);
}

void
window_scroll_scroll_up(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen_write_ctx		 ctx;

	if (data->oy >= w->base.hsize)
		return;
	data->oy++;

	screen_write_start_window(&ctx, w);
	screen_write_move_cursor(&ctx, 0, 0);
	screen_write_insert_lines(&ctx, 1);
	window_scroll_write_line(w, &ctx, 0);
	window_scroll_write_line(w, &ctx, 1);
	screen_write_stop(&ctx);
}

void
window_scroll_scroll_down(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->oy == 0)
		return;
	data->oy--;

	screen_write_start_window(&ctx, w);
	screen_write_move_cursor(&ctx, 0, 0);
	screen_write_delete_lines(&ctx, 1);
	window_scroll_write_line(w, &ctx, screen_last_y(s));
	window_scroll_write_line(w, &ctx, 0);
	screen_write_stop(&ctx);
}

void
window_scroll_scroll_right(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int		 		 i;

	if (data->ox >= SHRT_MAX)
		return;
	data->ox++;

	screen_write_start_window(&ctx, w);
	for (i = 1; i < screen_size_y(s); i++) {
		screen_write_move_cursor(&ctx, 0, i);
		screen_write_delete_characters(&ctx, 1);
	}
	window_scroll_write_column(w, &ctx, screen_last_x(s));
	window_scroll_write_line(w, &ctx, 0);
	screen_write_stop(&ctx);
}

void
window_scroll_scroll_left(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int		 		 i;

	if (data->ox == 0)
		return;
	data->ox--;

	screen_write_start_window(&ctx, w);
	for (i = 1; i < screen_size_y(s); i++) {
		screen_write_move_cursor(&ctx, 0, i);
		screen_write_insert_characters(&ctx, 1);
	}
	window_scroll_write_column(w, &ctx, 0);
	window_scroll_write_line(w, &ctx, 0);
	screen_write_stop(&ctx);
}
