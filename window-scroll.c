/* $OpenBSD$ */

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

struct screen *window_scroll_init(struct window_pane *);
void	window_scroll_free(struct window_pane *);
void	window_scroll_resize(struct window_pane *, u_int, u_int);
void	window_scroll_key(struct window_pane *, struct client *, int);

void	window_scroll_redraw_screen(struct window_pane *);
void	window_scroll_write_line(
    	    struct window_pane *, struct screen_write_ctx *, u_int);
void	window_scroll_write_column(
    	    struct window_pane *, struct screen_write_ctx *, u_int);

void	window_scroll_scroll_up(struct window_pane *);
void	window_scroll_scroll_down(struct window_pane *);
void	window_scroll_scroll_left(struct window_pane *);
void	window_scroll_scroll_right(struct window_pane *);

const struct window_mode window_scroll_mode = {
	window_scroll_init,
	window_scroll_free,
	window_scroll_resize,
	window_scroll_key,
	NULL,
	NULL,
};

struct window_scroll_mode_data {
	struct screen	screen;

	struct mode_key_data	mdata;

	u_int		ox;
	u_int		oy;
};

struct screen *
window_scroll_init(struct window_pane *wp)
{
	struct window_scroll_mode_data	*data;
	struct screen			*s;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	wp->modedata = data = xmalloc(sizeof *data);
	data->ox = 0;
	data->oy = 0;

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	s->mode &= ~MODE_CURSOR;

	mode_key_init(&data->mdata,
	    options_get_number(&wp->window->options, "mode-keys"), 0);

	screen_write_start(&ctx, NULL, s);
	for (i = 0; i < screen_size_y(s); i++)
		window_scroll_write_line(wp, &ctx, i);
	screen_write_stop(&ctx);

	return (s);
}

void
window_scroll_free(struct window_pane *wp)
{
	struct window_scroll_mode_data	*data = wp->modedata;

	mode_key_free(&data->mdata);

	screen_free(&data->screen);
	xfree(data);
}

void
window_scroll_pageup(struct window_pane *wp)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	if (data->oy + screen_size_y(s) > screen_hsize(&wp->base))
		data->oy = screen_hsize(&wp->base);
	else
		data->oy += screen_size_y(s);

	window_scroll_redraw_screen(wp);
}

void
window_scroll_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_resize(s, sx, sy);
	screen_write_start(&ctx, NULL, s);
	for (i = 0; i < screen_size_y(s); i++)
		window_scroll_write_line(wp, &ctx, i);
	screen_write_stop(&ctx);
}

void
window_scroll_key(struct window_pane *wp, unused struct client *c, int key)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	switch (mode_key_lookup(&data->mdata, key)) {
	case MODEKEYCMD_QUIT:
		window_pane_reset_mode(wp);
		break;
	case MODEKEYCMD_LEFT:
		window_scroll_scroll_left(wp);
		break;
	case MODEKEYCMD_RIGHT:
		window_scroll_scroll_right(wp);
		break;
	case MODEKEYCMD_UP:
		window_scroll_scroll_up(wp);
		break;
	case MODEKEYCMD_DOWN:
		window_scroll_scroll_down(wp);
		break;
	case MODEKEYCMD_PREVIOUSPAGE:
		window_scroll_pageup(wp);
		break;
	case MODEKEYCMD_NEXTPAGE:
		if (data->oy < screen_size_y(s))
			data->oy = 0;
		else
			data->oy -= screen_size_y(s);
		window_scroll_redraw_screen(wp);
		break;
	default:
		break;
	}
}

void
window_scroll_write_line(
    struct window_pane *wp, struct screen_write_ctx *ctx, u_int py)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct grid_cell		 gc;
	char				 hdr[32];
	size_t	 			 size;

	if (py == 0) {
		memcpy(&gc, &grid_default_cell, sizeof gc);
		size = xsnprintf(hdr, sizeof hdr,
		    "[%u,%u/%u]", data->ox, data->oy, screen_hsize(&wp->base));
		gc.bg = options_get_number(&wp->window->options, "mode-fg");
		gc.fg = options_get_number(&wp->window->options, "mode-bg");
		gc.attr |= options_get_number(&wp->window->options, "mode-attr");
		screen_write_cursormove(ctx, screen_size_x(s) - size, 0);
		screen_write_puts(ctx, &gc, "%s", hdr);
		memcpy(&gc, &grid_default_cell, sizeof gc);
	} else
		size = 0;

	screen_write_cursormove(ctx, 0, py);
	screen_write_copy(ctx, &wp->base, data->ox, (screen_hsize(&wp->base) -
	    data->oy) + py, screen_size_x(s) - size, 1);
}

void
window_scroll_write_column(
    struct window_pane *wp, struct screen_write_ctx *ctx, u_int px)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	screen_write_cursormove(ctx, px, 0);
	screen_write_copy(ctx, &wp->base, data->ox + px,
	    screen_hsize(&wp->base) - data->oy, 1, screen_size_y(s));
}

void
window_scroll_redraw_screen(struct window_pane *wp)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start(&ctx, wp, NULL);
	for (i = 0; i < screen_size_y(s); i++)
		window_scroll_write_line(wp, &ctx, i);
	screen_write_stop(&ctx);
}

void
window_scroll_scroll_up(struct window_pane *wp)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen_write_ctx		 ctx;

	if (data->oy >= screen_hsize(&wp->base))
		return;
	data->oy++;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_insertline(&ctx, 1);
	window_scroll_write_line(wp, &ctx, 0);
	window_scroll_write_line(wp, &ctx, 1);
	screen_write_stop(&ctx);
}

void
window_scroll_scroll_down(struct window_pane *wp)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->oy == 0)
		return;
	data->oy--;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_deleteline(&ctx, 1);
	window_scroll_write_line(wp, &ctx, screen_size_y(s) - 1);
	window_scroll_write_line(wp, &ctx, 0);
	screen_write_stop(&ctx);
}

void
window_scroll_scroll_right(struct window_pane *wp)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int		 		 i;

	if (data->ox >= SHRT_MAX)
		return;
	data->ox++;

	screen_write_start(&ctx, wp, NULL);
	for (i = 1; i < screen_size_y(s); i++) {
		screen_write_cursormove(&ctx, 0, i);
		screen_write_deletecharacter(&ctx, 1);
	}
	window_scroll_write_column(wp, &ctx, screen_size_x(s) - 1);
	window_scroll_write_line(wp, &ctx, 0);
	screen_write_stop(&ctx);
}

void
window_scroll_scroll_left(struct window_pane *wp)
{
	struct window_scroll_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int		 		 i;

	if (data->ox == 0)
		return;
	data->ox--;

	screen_write_start(&ctx, wp, NULL);
	for (i = 1; i < screen_size_y(s); i++) {
		screen_write_cursormove(&ctx, 0, i);
		screen_write_insertcharacter(&ctx, 1);
	}
	window_scroll_write_column(wp, &ctx, 0);
	window_scroll_write_line(wp, &ctx, 0);
	screen_write_stop(&ctx);
}
