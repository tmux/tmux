/* $Id: window-scroll.c,v 1.26 2009-01-10 19:35:40 nicm Exp $ */

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
void	window_scroll_key(struct window *, struct client *, int);

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
	window_scroll_key,
	NULL
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
	screen_init(s, screen_size_x(&w->base), screen_size_y(&w->base), 0);
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

	screen_free(&data->screen);
	xfree(data);
}

void
window_scroll_resize(struct window *w, u_int sx, u_int sy)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_resize(s, sx, sy);
	screen_write_start(&ctx, s, NULL, NULL);
	for (i = 0; i < screen_size_y(s); i++)
		window_scroll_write_line(w, &ctx, i);
	screen_write_stop(&ctx);
}

void
window_scroll_key(struct window *w, unused struct client *c, int key)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	int				 table;

	table = options_get_number(&w->options, "mode-keys");
	switch (mode_key_lookup(table, key)) {
	case MODEKEY_QUIT:
		window_reset_mode(w);
		break;
	case MODEKEY_LEFT:
		window_scroll_scroll_left(w);
		break;
	case MODEKEY_RIGHT:
		window_scroll_scroll_right(w);
		break;
	case MODEKEY_UP:
		window_scroll_scroll_up(w);
		break;
	case MODEKEY_DOWN:
		window_scroll_scroll_down(w);
		break;
	case MODEKEY_PPAGE:
		if (data->oy + screen_size_y(s) > screen_hsize(&w->base))
			data->oy = screen_hsize(&w->base);
		else
			data->oy += screen_size_y(s);
		window_scroll_redraw_screen(w);
		break;
	case MODEKEY_NPAGE:
		if (data->oy < screen_size_y(s))
			data->oy = 0;
		else
			data->oy -= screen_size_y(s);
		window_scroll_redraw_screen(w);
		break;
	default:
		break;
	}
}

void
window_scroll_write_line(
    struct window *w, struct screen_write_ctx *ctx, u_int py)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct grid_cell		 gc;
	char				 hdr[32];
	size_t	 			 size;

	if (py == 0) {
		memcpy(&gc, &grid_default_cell, sizeof gc);
		size = xsnprintf(hdr, sizeof hdr,
		    "[%u,%u/%u]", data->ox, data->oy, screen_hsize(&w->base));
		gc.fg = options_get_number(&w->options, "mode-fg");
		gc.bg = options_get_number(&w->options, "mode-bg");
		screen_write_cursormove(ctx, screen_size_x(s) - size, 0);
		screen_write_puts(ctx, &gc, "%s", hdr);
		memcpy(&gc, &grid_default_cell, sizeof gc);
	} else
		size = 0;

	screen_write_cursormove(ctx, 0, py);
	screen_write_copy(ctx, &w->base, data->ox, (screen_hsize(&w->base) -
	    data->oy) + py, screen_size_x(s) - size, 1);
}

void
window_scroll_write_column(
    struct window *w, struct screen_write_ctx *ctx, u_int px)
{
	struct window_scroll_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	screen_write_cursormove(ctx, px, 0);
	screen_write_copy(ctx, &w->base, data->ox + px,
	    screen_hsize(&w->base) - data->oy, 1, screen_size_y(s));
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

	if (data->oy >= screen_hsize(&w->base))
		return;
	data->oy++;

	screen_write_start_window(&ctx, w);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_insertline(&ctx, 1);
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
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_deleteline(&ctx, 1);
	window_scroll_write_line(w, &ctx, screen_size_y(s) - 1);
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
		screen_write_cursormove(&ctx, 0, i);
		screen_write_deletecharacter(&ctx, 1);
	}
	window_scroll_write_column(w, &ctx, screen_size_x(s) - 1);
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
		screen_write_cursormove(&ctx, 0, i);
		screen_write_insertcharacter(&ctx, 1);
	}
	window_scroll_write_column(w, &ctx, 0);
	window_scroll_write_line(w, &ctx, 0);
	screen_write_stop(&ctx);
}
