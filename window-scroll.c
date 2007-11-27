/* $Id: window-scroll.c,v 1.13 2007-11-27 19:23:34 nicm Exp $ */

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

void	window_scroll_init(struct window *);
void	window_scroll_resize(struct window *, u_int, u_int);
void	window_scroll_draw(
    	    struct window *, struct screen_draw_ctx *, u_int, u_int);
void	window_scroll_key(struct window *, int);

void	window_scroll_draw_position(struct window *, struct screen_draw_ctx *);

void	window_scroll_up_1(struct window *);
void	window_scroll_down_1(struct window *);
void	window_scroll_left_1(struct window *);
void	window_scroll_right_1(struct window *);

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

	w->screen.mode |= MODE_BACKGROUND;
	w->screen.mode &= ~MODE_BGCURSOR;
}

void
window_scroll_resize(unused struct window *w, unused u_int sx, unused u_int sy)
{
}

void
window_scroll_draw_position(struct window *w, struct screen_draw_ctx *ctx)
{
	struct window_scroll_mode_data	*data = w->modedata;
	char				*ptr, buf[32];
	size_t	 			 len;

	len = xsnprintf(
	    buf, sizeof buf, "[%u,%u/%u]", data->ox, data->oy, data->size);
	if (len <= screen_size_x(ctx->s))
		ptr = buf;
	else {
		ptr = buf + len - screen_size_x(ctx->s);
		len -= len - screen_size_x(ctx->s);
	}
	
	screen_draw_cells(ctx, 0, 0, screen_size_x(ctx->s) - len);
	
	screen_draw_move_cursor(ctx, screen_size_x(ctx->s) - len, 0);
	screen_draw_set_attributes(ctx, 0, status_colour);
}

void
window_scroll_draw(
    struct window *w, struct screen_draw_ctx *ctx, u_int py, u_int ny)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &w->screen;

	if (s->hsize != data->size) {
		data->oy += s->hsize - data->size;
		data->size = s->hsize;
	}

	if (py != 0)
		screen_draw_lines(ctx, py, ny);
	else {
		if (ny > 1)
			screen_draw_lines(ctx, py + 1, ny - 1);
		window_scroll_draw_position(w, ctx);
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

		w->screen.mode &= ~MODE_BACKGROUND;

		recalculate_sizes();
		server_redraw_window(w);
		return;
	case 'h':
	case KEYC_LEFT:
		window_scroll_left_1(w);
		return;
	case 'l':
	case KEYC_RIGHT:
		window_scroll_right_1(w);
		return;
	case 'k':
	case 'K':
	case KEYC_UP:
		window_scroll_up_1(w);
		return;
	case 'j':
	case 'J':
	case KEYC_DOWN:
		window_scroll_down_1(w);
		return;
	case '\025':	/* C-u */
	case KEYC_PPAGE:
		if (data->oy + sy > data->size)
			data->oy = data->size;
		else
			data->oy += sy;
		break;
	case '\006':	/* C-f */
	case KEYC_NPAGE:
		if (data->oy < sy)
			data->oy = 0;
		else
			data->oy -= sy;
		break;
	}
	if (ox != data->ox || oy != data->oy)
		server_redraw_window(w);
}

void
window_scroll_up_1(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen_draw_ctx		 ctx;

	if (data->oy >= data->size)
		return;
	data->oy++;

	screen_draw_start_window(&ctx, w, data->ox, data->oy);
	screen_draw_move_cursor(&ctx, 0, 0);
	screen_draw_insert_lines(&ctx, 1);
	window_scroll_draw_position(w, &ctx);
	screen_draw_line(&ctx, 1);
	screen_draw_stop(&ctx);
}

void
window_scroll_down_1(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen_draw_ctx		 ctx;

	if (data->oy == 0)
		return;
	data->oy--;

	screen_draw_start_window(&ctx, w, data->ox, data->oy);
	screen_draw_move_cursor(&ctx, 0, 0);
	screen_draw_delete_lines(&ctx, 1);
	screen_draw_line(&ctx, screen_last_y(&w->screen));
	window_scroll_draw_position(w, &ctx);
	screen_draw_stop(&ctx);
}

void
window_scroll_right_1(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen_draw_ctx		 ctx;
	u_int		 		 i;

	if (data->ox >= SHRT_MAX)
		return;
	data->ox++;

	screen_draw_start_window(&ctx, w, data->ox, data->oy);
	for (i = 1; i < screen_size_y(&w->screen); i++) {
		screen_draw_move_cursor(&ctx, 0, i);
		screen_draw_delete_characters(&ctx, 1);
	}
	screen_draw_column(&ctx, screen_last_x(&w->screen));
	window_scroll_draw_position(w, &ctx);
	screen_draw_stop(&ctx);
}

void
window_scroll_left_1(struct window *w)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen_draw_ctx		 ctx;
	u_int		 		 i;

	if (data->ox == 0)
		return;
	data->ox--;


	screen_draw_start_window(&ctx, w, data->ox, data->oy);
	for (i = 1; i < screen_size_y(&w->screen); i++) {
		screen_draw_move_cursor(&ctx, 0, i);
		screen_draw_insert_characters(&ctx, 1);
	}
	screen_draw_column(&ctx, 0);
	window_scroll_draw_position(w, &ctx);
	screen_draw_stop(&ctx);
}
