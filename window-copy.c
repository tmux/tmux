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

struct screen *window_copy_init(struct window_pane *);
void	window_copy_free(struct window_pane *);
void	window_copy_resize(struct window_pane *, u_int, u_int);
void	window_copy_key(struct window_pane *, struct client *, int);
void	window_copy_mouse(
    	    struct window_pane *, struct client *, u_char, u_char, u_char);

void	window_copy_redraw_lines(struct window_pane *, u_int, u_int);
void	window_copy_redraw_screen(struct window_pane *);
void	window_copy_write_line(
    	    struct window_pane *, struct screen_write_ctx *, u_int);
void	window_copy_write_lines(
    	    struct window_pane *, struct screen_write_ctx *, u_int, u_int);
void	window_copy_write_column(
    	    struct window_pane *, struct screen_write_ctx *, u_int);
void	window_copy_write_columns(
    	    struct window_pane *, struct screen_write_ctx *, u_int, u_int);

void	window_copy_update_cursor(struct window_pane *);
void	window_copy_start_selection(struct window_pane *);
int	window_copy_update_selection(struct window_pane *);
void	window_copy_copy_selection(struct window_pane *, struct client *);
void	window_copy_copy_line(
	    struct window_pane *, char **, size_t *, u_int, u_int, u_int);
int	window_copy_is_space(struct window_pane *, u_int, u_int);
u_int	window_copy_find_length(struct window_pane *, u_int);
void	window_copy_cursor_start_of_line(struct window_pane *);
void	window_copy_cursor_end_of_line(struct window_pane *);
void	window_copy_cursor_left(struct window_pane *);
void	window_copy_cursor_right(struct window_pane *);
void	window_copy_cursor_up(struct window_pane *);
void	window_copy_cursor_down(struct window_pane *);
void	window_copy_cursor_next_word(struct window_pane *);
void	window_copy_cursor_previous_word(struct window_pane *);
void	window_copy_scroll_left(struct window_pane *, u_int);
void	window_copy_scroll_right(struct window_pane *, u_int);
void	window_copy_scroll_up(struct window_pane *, u_int);
void	window_copy_scroll_down(struct window_pane *, u_int);

const struct window_mode window_copy_mode = {
	window_copy_init,
	window_copy_free,
	window_copy_resize,
	window_copy_key,
	window_copy_mouse,
	NULL,
};

struct window_copy_mode_data {
	struct screen	screen;

	struct mode_key_data	mdata;

	u_int	ox;
	u_int	oy;

	u_int	selx;
	u_int	sely;

	u_int	cx;
	u_int	cy;
};

struct screen *
window_copy_init(struct window_pane *wp)
{
	struct window_copy_mode_data	*data;
	struct screen			*s;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	wp->modedata = data = xmalloc(sizeof *data);
	data->ox = 0;
	data->oy = 0;
	data->cx = wp->base.cx;
	data->cy = wp->base.cy;

	s = &data->screen;
	screen_init(s, screen_size_x(&wp->base), screen_size_y(&wp->base), 0);
	s->mode |= MODE_MOUSE;

	mode_key_init(&data->mdata,
	    options_get_number(&wp->window->options, "mode-keys"), 0);

	s->cx = data->cx;
	s->cy = data->cy;

	screen_write_start(&ctx, NULL, s);
	for (i = 0; i < screen_size_y(s); i++)
		window_copy_write_line(wp, &ctx, i);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);

	return (s);
}

void
window_copy_free(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

 	mode_key_free(&data->mdata);

	screen_free(&data->screen);
	xfree(data);
}

void
window_copy_pageup(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	if (data->oy + screen_size_y(s) > screen_hsize(&wp->base))
		data->oy = screen_hsize(&wp->base);
	else
		data->oy += screen_size_y(s);
	window_copy_update_selection(wp);
	window_copy_redraw_screen(wp);
}

void
window_copy_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx	 	 ctx;

	screen_resize(s, sx, sy);
	screen_write_start(&ctx, NULL, s);
	window_copy_write_lines(wp, &ctx, 0, screen_size_y(s) - 1);
	screen_write_stop(&ctx);
	window_copy_update_selection(wp);
}

void
window_copy_key(struct window_pane *wp, struct client *c, int key)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	switch (mode_key_lookup(&data->mdata, key)) {
	case MODEKEYCMD_QUIT:
		window_pane_reset_mode(wp);
		break;
	case MODEKEYCMD_LEFT:
		window_copy_cursor_left(wp);
		return;
	case MODEKEYCMD_RIGHT:
		window_copy_cursor_right(wp);
 		return;
	case MODEKEYCMD_UP:
		window_copy_cursor_up(wp);
		return;
	case MODEKEYCMD_DOWN:
		window_copy_cursor_down(wp);
		return;
	case MODEKEYCMD_PREVIOUSPAGE:
		window_copy_pageup(wp);
		break;
	case MODEKEYCMD_NEXTPAGE:
		if (data->oy < screen_size_y(s))
			data->oy = 0;
		else
			data->oy -= screen_size_y(s);
		window_copy_update_selection(wp);
		window_copy_redraw_screen(wp);
		break;
	case MODEKEYCMD_STARTSELECTION:
 		window_copy_start_selection(wp);
		break;
	case MODEKEYCMD_CLEARSELECTION:
		screen_clear_selection(&data->screen);
		window_copy_redraw_screen(wp);
		break;
	case MODEKEYCMD_COPYSELECTION:
		if (c != NULL && c->session != NULL) {
			window_copy_copy_selection(wp, c);
			window_pane_reset_mode(wp);
		}
		break;
	case MODEKEYCMD_STARTOFLINE:
		window_copy_cursor_start_of_line(wp);
		break;
	case MODEKEYCMD_ENDOFLINE:
		window_copy_cursor_end_of_line(wp);
		break;
	case MODEKEYCMD_NEXTWORD:
		window_copy_cursor_next_word(wp);
		break;
	case MODEKEYCMD_PREVIOUSWORD:
		window_copy_cursor_previous_word(wp);
		break;
	default:
		break;
	}
}

void
window_copy_mouse(struct window_pane *wp,
    unused struct client *c, u_char b, u_char x, u_char y)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	if ((b & 3) == 3)
		return;
	if (x >= screen_size_x(s))
		return;
	if (y >= screen_size_y(s))
		return;

	data->cx = x;
	data->cy = y;

	if (window_copy_update_selection(wp))
 		window_copy_redraw_screen(wp);
	window_copy_update_cursor(wp);
}

void
window_copy_write_line(struct window_pane *wp, struct screen_write_ctx *ctx, u_int py)
{
	struct window_copy_mode_data	*data = wp->modedata;
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
	} else
		size = 0;

	screen_write_cursormove(ctx, 0, py);
	screen_write_copy(ctx, &wp->base, data->ox, (screen_hsize(&wp->base) -
	    data->oy) + py, screen_size_x(s) - size, 1);
}

void
window_copy_write_lines(
    struct window_pane *wp, struct screen_write_ctx *ctx, u_int py, u_int ny)
{
	u_int	yy;

	for (yy = py; yy < py + ny; yy++)
		window_copy_write_line(wp, ctx, py);
}

void
window_copy_write_column(
    struct window_pane *wp, struct screen_write_ctx *ctx, u_int px)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	screen_write_cursormove(ctx, px, 0);
	screen_write_copy(ctx, &wp->base,
	    data->ox + px, screen_hsize(&wp->base) - data->oy, 1, screen_size_y(s));
}

void
window_copy_write_columns(
    struct window_pane *wp, struct screen_write_ctx *ctx, u_int px, u_int nx)
{
	u_int	xx;

	for (xx = px; xx < px + nx; xx++)
		window_copy_write_column(wp, ctx, xx);
}

void
window_copy_redraw_lines(struct window_pane *wp, u_int py, u_int ny)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start(&ctx, wp, NULL);
	for (i = py; i < py + ny; i++)
		window_copy_write_line(wp, &ctx, i);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_redraw_screen(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

	window_copy_redraw_lines(wp, 0, screen_size_y(&data->screen));
}

void
window_copy_update_cursor(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen_write_ctx		 ctx;

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_start_selection(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;

	data->selx = data->cx + data->ox;
	data->sely = screen_hsize(&wp->base) + data->cy - data->oy;

	s->sel.flag = 1;
	window_copy_update_selection(wp);
}

int
window_copy_update_selection(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct grid_cell		 gc;
	u_int				 sx, sy, tx, ty;

	if (!s->sel.flag)
		return (0);

	/* Set colours. */
	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.bg = options_get_number(&wp->window->options, "mode-fg");
	gc.fg = options_get_number(&wp->window->options, "mode-bg");
	gc.attr |= options_get_number(&wp->window->options, "mode-attr");

	/* Find top-left of screen. */
	tx = data->ox;
	ty = screen_hsize(&wp->base) - data->oy;

	/* Adjust the selection. */
	sx = data->selx;
	sy = data->sely;
	if (sy < ty) {
		/* Above it. */
		sx = 0;
		sy = 0;
	} else if (sy > ty + screen_size_y(s) - 1) {
		/* Below it. */
		sx = screen_size_x(s) - 1;
		sy = screen_size_y(s) - 1;
	} else if (sx < tx) {
		/* To the left. */
		sx = 0;
	} else if (sx > tx + screen_size_x(s) - 1) {
		/* To the right. */
		sx = 0;
		sy++;
		if (sy > screen_size_y(s) - 1)
			sy = screen_size_y(s) - 1;
	} else {
		sx -= tx;
		sy -= ty;
	}
	sy = screen_hsize(s) + sy;

	screen_set_selection(
	    s, sx, sy, data->cx, screen_hsize(s) + data->cy, &gc);
	return (1);
}

void
window_copy_copy_selection(struct window_pane *wp, struct client *c)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	char				*buf;
	size_t	 			 off;
	u_int	 			 i, xx, yy, sx, sy, ex, ey, limit;

	if (!s->sel.flag)
		return;

	buf = xmalloc(1);
	off = 0;

	*buf = '\0';

	/*
	 * The selection extends from selx,sely to (adjusted) cx,cy on
	 * the base screen.
	 */

	/* Find start and end. */
	xx = data->cx + data->ox;
	yy = screen_hsize(&wp->base) + data->cy - data->oy;
	if (xx < data->selx || (yy == data->sely && xx < data->selx)) {
		sx = xx; sy = yy;
		ex = data->selx; ey = data->sely;
	} else {
		sx = data->selx; sy = data->sely;
		ex = xx; ey = yy;
	}

	/* Trim ex to end of line. */
	xx = window_copy_find_length(wp, ey);
	if (ex > xx)
		ex = xx;

	/* Copy the lines. */
	if (sy == ey)
		window_copy_copy_line(wp, &buf, &off, sy, sx, ex);
	else {
		xx = window_copy_find_length(wp, sy);
		window_copy_copy_line(wp, &buf, &off, sy, sx, xx);
		if (ey - sy > 1) {
			for (i = sy + 1; i < ey - 1; i++) {
				xx = window_copy_find_length(wp, i);
				window_copy_copy_line(wp, &buf, &off, i, 0, xx);
			}
		}
		window_copy_copy_line(wp, &buf, &off, ey, 0, ex);
	}

	/* Terminate buffer, overwriting final \n. */
	if (off != 0)
		buf[off - 1] = '\0';

	/* Add the buffer to the stack. */
	limit = options_get_number(&c->session->options, "buffer-limit");
	paste_add(&c->session->buffers, buf, limit);
}

void
window_copy_copy_line(struct window_pane *wp,
    char **buf, size_t *off, u_int sy, u_int sx, u_int ex)
{
 	const struct grid_cell	*gc;
 	const struct grid_utf8	*gu;
	u_int			 i, j, xx;

	if (sx > ex)
		return;

	xx = window_copy_find_length(wp, sy);
	if (ex > xx)
		ex = xx;
	if (sx > xx)
		sx = xx;

	if (sx < ex) {
		for (i = sx; i < ex; i++) {
			gc = grid_peek_cell(wp->base.grid, i, sy);
			if (gc->flags & GRID_FLAG_PADDING)
				continue;
			if (!(gc->flags & GRID_FLAG_UTF8)) {
				*buf = xrealloc(*buf, 1, (*off) + 1);
				(*buf)[(*off)++] = gc->data;
			} else {
				gu = grid_peek_utf8(wp->base.grid, i, sy);
				*buf = xrealloc(*buf, 1, (*off) + UTF8_SIZE);
				for (j = 0; j < UTF8_SIZE; j++) {
					if (gu->data[j] == 0xff)
						break;
					(*buf)[(*off)++] = gu->data[j];
				}
			}
		}
	}

	*buf = xrealloc(*buf, 1, (*off) + 1);
	(*buf)[*off] = '\n';
	(*off)++;
}

int
window_copy_is_space(struct window_pane *wp, u_int px, u_int py)
{
	const struct grid_cell	*gc;
	const char     		*spaces = " -_@";

	gc = grid_peek_cell(wp->base.grid, px, py);
	if (gc->flags & (GRID_FLAG_PADDING|GRID_FLAG_UTF8))
		return (0);
	if (gc->data == 0x00 || gc->data == 0x7f)
		return (0);
	return (strchr(spaces, gc->data) != NULL);
}

u_int
window_copy_find_length(struct window_pane *wp, u_int py)
{
	const struct grid_cell	*gc;
	u_int			 px;

	px = wp->base.grid->size[py];
	while (px > 0) {
		gc = grid_peek_cell(wp->base.grid, px - 1, py);
		if (gc->flags & GRID_FLAG_UTF8)
			break;
		if (gc->data != ' ')
			break;
		px--;
	}
	return (px);
}

void
window_copy_cursor_start_of_line(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

	if (data->ox != 0)
		window_copy_scroll_right(wp, data->ox);
	data->cx = 0;

	if (window_copy_update_selection(wp))
		window_copy_redraw_lines(wp, data->cy, 1);
	else
		window_copy_update_cursor(wp);
}

void
window_copy_cursor_end_of_line(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 px, py;

	py = screen_hsize(&wp->base) + data->cy - data->oy;
	px = window_copy_find_length(wp, py);

	/* On screen. */
	if (px > data->ox && px <= data->ox + screen_size_x(s) - 1)
		data->cx = px - data->ox;

	/* Off right of screen. */
	if (px > data->ox + screen_size_x(s) - 1) {
		/* Move cursor to last and scroll screen. */
		window_copy_scroll_left(
		    wp, px - data->ox - (screen_size_x(s) - 1));
		data->cx = screen_size_x(s) - 1;
	}

	/* Off left of screen. */
	if (px <= data->ox) {
		if (px < screen_size_x(s) - 1) {
			/* Short enough to fit on screen. */
			window_copy_scroll_right(wp, data->ox);
			data->cx = px;
		} else {
			/* Too long to fit on screen. */
			window_copy_scroll_right(
			    wp, data->ox - (px - (screen_size_x(s) - 1)));
			data->cx = screen_size_x(s) - 1;
		}
 	}

	if (window_copy_update_selection(wp))
		window_copy_redraw_lines(wp, data->cy, 1);
	else
		window_copy_update_cursor(wp);
}

void
window_copy_cursor_left(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;

	if (data->cx == 0) {
		if (data->ox > 0)
			window_copy_scroll_right(wp, 1);
		else {
			window_copy_cursor_up(wp);
			window_copy_cursor_end_of_line(wp);
		}
	} else {
		data->cx--;
		if (window_copy_update_selection(wp))
			window_copy_redraw_lines(wp, data->cy, 1);
		else
			window_copy_update_cursor(wp);
	}
}

void
window_copy_cursor_right(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 px, py;

	py = screen_hsize(&wp->base) + data->cy - data->oy;
	px = window_copy_find_length(wp, py);

	if (data->cx >= px) {
		window_copy_cursor_start_of_line(wp);
		window_copy_cursor_down(wp);
	} else {
		data->cx++;
		if (window_copy_update_selection(wp))
			window_copy_redraw_lines(wp, data->cy, 1);
		else
			window_copy_update_cursor(wp);
	}
}

void
window_copy_cursor_up(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	u_int				 ox, oy, px, py;

	oy = screen_hsize(&wp->base) + data->cy - data->oy;
	ox = window_copy_find_length(wp, oy);

	if (data->cy == 0)
		window_copy_scroll_down(wp, 1);
	else {
		data->cy--;
		if (window_copy_update_selection(wp))
			window_copy_redraw_lines(wp, data->cy, 2);
		else
			window_copy_update_cursor(wp);
	}

	py = screen_hsize(&wp->base) + data->cy - data->oy;
	px = window_copy_find_length(wp, py);

	if (data->cx + data->ox >= px || data->cx + data->ox >= ox)
		window_copy_cursor_end_of_line(wp);
}

void
window_copy_cursor_down(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 ox, oy, px, py;

	oy = screen_hsize(&wp->base) + data->cy - data->oy;
	ox = window_copy_find_length(wp, oy);

	if (data->cy == screen_size_y(s) - 1)
		window_copy_scroll_up(wp, 1);
	else {
		data->cy++;
		if (window_copy_update_selection(wp))
			window_copy_redraw_lines(wp, data->cy - 1, 2);
		else
			window_copy_update_cursor(wp);
	}

	py = screen_hsize(&wp->base) + data->cy - data->oy;
	px = window_copy_find_length(wp, py);

	if (data->cx + data->ox >= px || data->cx + data->ox >= ox)
		window_copy_cursor_end_of_line(wp);
}

void
window_copy_cursor_next_word(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 px, py, xx, skip;

	px = data->ox + data->cx;
	py = screen_hsize(&wp->base) + data->cy - data->oy;
	xx = window_copy_find_length(wp, py);

	skip = 1;
	if (px < xx) {
		/* If currently on a space, skip space. */
		if (window_copy_is_space(wp, px, py))
			skip = 0;
	} else
		skip = 0;
	for (;;) {
		if (px >= xx) {
			if (skip) {
				px = xx;
				break;
			}

			while (px >= xx) {
				if (data->cy == screen_size_y(s) - 1) {
					if (data->oy == 0)
						goto out;
				}

				px = 0;
				window_copy_cursor_down(wp);

				py =screen_hsize(
				    &wp->base) + data->cy - data->oy;
				xx = window_copy_find_length(wp, py);
			}
		}

		if (skip) {
			/* Currently skipping non-space (until space). */
			if (window_copy_is_space(wp, px, py))
				break;
		} else {
			/* Currently skipping space (until non-space). */
			if (!window_copy_is_space(wp, px, py))
				skip = 1;
		}

		px++;
	}
out:

	/* On screen. */
	if (px > data->ox && px <= data->ox + screen_size_x(s) - 1)
		data->cx = px - data->ox;

	/* Off right of screen. */
	if (px > data->ox + screen_size_x(s) - 1) {
		/* Move cursor to last and scroll screen. */
		window_copy_scroll_left(
		    wp, px - data->ox - (screen_size_x(s) - 1));
		data->cx = screen_size_x(s) - 1;
	}

	/* Off left of screen. */
	if (px <= data->ox) {
		if (px < screen_size_x(s) - 1) {
			/* Short enough to fit on screen. */
			window_copy_scroll_right(wp, data->ox);
			data->cx = px;
		} else {
			/* Too long to fit on screen. */
			window_copy_scroll_right(
			    wp, data->ox - (px - (screen_size_x(s) - 1)));
			data->cx = screen_size_x(s) - 1;
		}
 	}

	if (window_copy_update_selection(wp))
		window_copy_redraw_lines(wp, data->cy, 1);
	else
		window_copy_update_cursor(wp);
}

void
window_copy_cursor_previous_word(struct window_pane *wp)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	u_int				 ox, px, py, skip;

	ox = px = data->ox + data->cx;
	py = screen_hsize(&wp->base) + data->cy - data->oy;

	skip = 1;
	if (px != 0) {
		/* If currently on a space, skip space. */
		if (window_copy_is_space(wp, px - 1, py))
			skip = 0;
	}
	for (;;) {
		if (px == 0) {
			if (ox != 0)
				break;

			while (px == 0) {
				if (data->cy == 0 &&
				    (screen_hsize(&wp->base) == 0 ||
				    data->oy >= screen_hsize(&wp->base) - 1))
					goto out;

				window_copy_cursor_up(wp);

				py = screen_hsize(
				    &wp->base) + data->cy - data->oy;
				px = window_copy_find_length(wp, py);
			}
			goto out;
		}

		if (skip) {
			/* Currently skipping non-space (until space). */
			if (window_copy_is_space(wp, px - 1, py))
				skip = 0;
		} else {
			/* Currently skipping space (until non-space). */
			if (!window_copy_is_space(wp, px - 1, py))
				break;
		}

		px--;
	}
out:

	/* On screen. */
	if (px > data->ox && px <= data->ox + screen_size_x(s) - 1)
		data->cx = px - data->ox;

	/* Off right of screen. */
	if (px > data->ox + screen_size_x(s) - 1) {
		/* Move cursor to last and scroll screen. */
		window_copy_scroll_left(
		    wp, px - data->ox - (screen_size_x(s) - 1));
		data->cx = screen_size_x(s) - 1;
	}

	/* Off left of screen. */
	if (px <= data->ox) {
		if (px < screen_size_x(s) - 1) {
			/* Short enough to fit on screen. */
			window_copy_scroll_right(wp, data->ox);
			data->cx = px;
		} else {
			/* Too long to fit on screen. */
			window_copy_scroll_right(
			    wp, data->ox - (px - (screen_size_x(s) - 1)));
			data->cx = screen_size_x(s) - 1;
		}
 	}

	if (window_copy_update_selection(wp))
		window_copy_redraw_lines(wp, data->cy, 1);
	else
		window_copy_update_cursor(wp);
}

void
window_copy_scroll_left(struct window_pane *wp, u_int nx)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int				 i;

	if (data->ox > SHRT_MAX - nx)
		nx = SHRT_MAX - data->ox;
	if (nx == 0)
		return;
	data->ox += nx;
	window_copy_update_selection(wp);

	screen_write_start(&ctx, wp, NULL);
	for (i = 1; i < screen_size_y(s); i++) {
		screen_write_cursormove(&ctx, 0, i);
		screen_write_deletecharacter(&ctx, nx);
	}
	window_copy_write_columns(wp, &ctx, screen_size_x(s) - nx, nx);
	window_copy_write_line(wp, &ctx, 0);
	if (s->sel.flag) {
		window_copy_update_selection(wp);
		window_copy_write_lines(wp, &ctx, data->cy, 1);
	}
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_scroll_right(struct window_pane *wp, u_int nx)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int		 		 i;

	if (data->ox < nx)
		nx = data->ox;
	if (nx == 0)
		return;
	data->ox -= nx;
	window_copy_update_selection(wp);

	screen_write_start(&ctx, wp, NULL);
	for (i = 1; i < screen_size_y(s); i++) {
		screen_write_cursormove(&ctx, 0, i);
		screen_write_insertcharacter(&ctx, nx);
	}
	window_copy_write_columns(wp, &ctx, 0, nx);
	window_copy_write_line(wp, &ctx, 0);
	if (s->sel.flag)
		window_copy_write_line(wp, &ctx, data->cy);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_scroll_up(struct window_pane *wp, u_int ny)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->oy < ny)
		ny = data->oy;
	if (ny == 0)
		return;
	data->oy -= ny;
	window_copy_update_selection(wp);

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_deleteline(&ctx, ny);
	window_copy_write_lines(wp, &ctx, screen_size_y(s) - ny, ny);
	window_copy_write_line(wp, &ctx, 0);
	window_copy_write_line(wp, &ctx, 1);
	if (s->sel.flag && screen_size_y(s) > ny)
		window_copy_write_line(wp, &ctx, screen_size_y(s) - ny - 1);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_scroll_down(struct window_pane *wp, u_int ny)
{
	struct window_copy_mode_data	*data = wp->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (ny > screen_hsize(&wp->base))
		return;

	if (data->oy > screen_hsize(&wp->base) - ny)
		ny = screen_hsize(&wp->base) - data->oy;
	if (ny == 0)
		return;
	data->oy += ny;
	window_copy_update_selection(wp);

	screen_write_start(&ctx, wp, NULL);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_insertline(&ctx, ny);
	window_copy_write_lines(wp, &ctx, 0, ny);
	if (s->sel.flag && screen_size_y(s) > ny)
		window_copy_write_line(wp, &ctx, ny);
	else if (ny == 1) /* nuke position */
		window_copy_write_line(wp, &ctx, 1);
	screen_write_cursormove(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}
