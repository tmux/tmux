/* $Id: window-copy.c,v 1.29 2008-09-08 17:40:51 nicm Exp $ */

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

struct screen *window_copy_init(struct window *);
void	window_copy_free(struct window *);
void	window_copy_resize(struct window *, u_int, u_int);
void	window_copy_key(struct window *, struct client *, int);

void	window_copy_redraw_lines(struct window *, u_int, u_int);
void	window_copy_redraw_screen(struct window *);
void	window_copy_write_line(
    	    struct window *, struct screen_write_ctx *, u_int);
void	window_copy_write_lines(
    	    struct window *, struct screen_write_ctx *, u_int, u_int);
void	window_copy_write_column(
    	    struct window *, struct screen_write_ctx *, u_int);
void	window_copy_write_columns(
    	    struct window *, struct screen_write_ctx *, u_int, u_int);

void	window_copy_update_cursor(struct window *);
void	window_copy_start_selection(struct window *);
int	window_copy_update_selection(struct window *);
void	window_copy_copy_selection(struct window *, struct client *);
void	window_copy_copy_line(
	    struct window *, char **, size_t *, u_int, u_int, u_int);
u_int	window_copy_find_length(struct window *, u_int);
void	window_copy_cursor_start_of_line(struct window *);
void	window_copy_cursor_end_of_line(struct window *);
void	window_copy_cursor_left(struct window *);
void	window_copy_cursor_right(struct window *);
void	window_copy_cursor_up(struct window *);
void	window_copy_cursor_down(struct window *);
void	window_copy_cursor_next_word(struct window *);
void	window_copy_cursor_previous_word(struct window *);
void	window_copy_scroll_left(struct window *, u_int);
void	window_copy_scroll_right(struct window *, u_int);
void	window_copy_scroll_up(struct window *, u_int);
void	window_copy_scroll_down(struct window *, u_int);

const char *space_characters = " -_@";
#define window_copy_is_space(w, x, y) \
	(strchr(space_characters, (w)->base.grid_data[y][x]) != NULL)

const struct window_mode window_copy_mode = {
	window_copy_init,
	window_copy_free,
	window_copy_resize,
	window_copy_key
};

struct window_copy_mode_data {
	struct screen	screen;

	u_int	ox;
	u_int	oy;

	u_int	selx;
	u_int	sely;

	u_int	cx;
	u_int	cy;
};

struct screen *
window_copy_init(struct window *w)
{
	struct window_copy_mode_data	*data;
	struct screen			*s;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	w->modedata = data = xmalloc(sizeof *data);
	data->ox = 0;
	data->oy = 0;
	data->cx = w->base.cx;
	data->cy = w->base.cy;

	s = &data->screen;
	screen_create(s, screen_size_x(&w->base), screen_size_y(&w->base), 0);
	s->cx = data->cx;
	s->cy = data->cy;

	screen_write_start(&ctx, s, NULL, NULL);
	for (i = 0; i < screen_size_y(s); i++)
		window_copy_write_line(w, &ctx, i);
	screen_write_move_cursor(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);

	return (s);
}

void
window_copy_free(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;

	screen_destroy(&data->screen);
	xfree(data);
}

void
window_copy_resize(struct window *w, u_int sx, u_int sy)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	screen_resize(s, sx, sy);
	screen_display_copy_area(&data->screen, &w->base,
	    0, 0, screen_size_x(s), screen_size_y(s), data->ox, data->oy);
	window_copy_update_selection(w);
}

void
window_copy_key(struct window *w, struct client *c, int key)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	int				 table;

	table = options_get_number(&c->session->options, "mode-keys");
	switch (mode_key_lookup(table, key)) {
	case MODEKEY_QUIT:
		window_reset_mode(w);
		break;
	case MODEKEY_LEFT:
		window_copy_cursor_left(w);
		return;
	case MODEKEY_RIGHT:
		window_copy_cursor_right(w);
 		return;
	case MODEKEY_UP:
		window_copy_cursor_up(w);
		return;
	case MODEKEY_DOWN:
		window_copy_cursor_down(w);
		return;
	case MODEKEY_PPAGE:
		if (data->oy + screen_size_y(s) > w->base.hsize)
			data->oy = w->base.hsize;
		else
			data->oy += screen_size_y(s);
		window_copy_update_selection(w);
		window_copy_redraw_screen(w);
		break;
	case MODEKEY_NPAGE:
		if (data->oy < screen_size_y(s))
			data->oy = 0;
		else
			data->oy -= screen_size_y(s);
		window_copy_update_selection(w);
		window_copy_redraw_screen(w);
		break;
	case MODEKEY_STARTSEL:
		window_copy_start_selection(w);
		break;
	case MODEKEY_CLEARSEL:
		screen_clear_selection(&data->screen);
		window_copy_redraw_screen(w);
		break;
	case MODEKEY_COPYSEL:
		if (c != NULL && c->session != NULL) {
			window_copy_copy_selection(w, c);
			window_reset_mode(w);
		}
		break;
	case MODEKEY_BOL:
		window_copy_cursor_start_of_line(w);
		break;
	case MODEKEY_EOL:
		window_copy_cursor_end_of_line(w);
		break;
	case MODEKEY_NWORD:
		window_copy_cursor_next_word(w);
		break;
	case MODEKEY_PWORD:
		window_copy_cursor_previous_word(w);
		break;
	default:
		break;
	}
}

void
window_copy_write_line(
    struct window *w, struct screen_write_ctx *ctx, u_int py)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	size_t	 			 size;

	if (py == 0) {
		screen_write_set_attributes(
		    ctx, ATTR_BRIGHT|ATTR_REVERSE, 8, 8);
		screen_write_move_cursor(ctx, 0, 0);
		size = screen_write_put_string_rjust(
		    ctx, "[%u,%u/%u]", data->ox, data->oy, w->base.hsize);
		screen_write_set_attributes(ctx, 0, 8, 8);
	} else
		size = 0;
	screen_write_move_cursor(ctx, 0, py);
	screen_write_copy_area(
	    ctx, &w->base, screen_size_x(s) - size, 1, data->ox, data->oy);
}

void
window_copy_write_lines(
    struct window *w, struct screen_write_ctx *ctx, u_int py, u_int ny)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	if (py == 0) {
		window_copy_write_line(w, ctx, 0);
		if (ny == 1)
			return;
		py++;
		ny--;
	}
	screen_write_move_cursor(ctx, 0, py);
	screen_write_copy_area(
	    ctx, &w->base, screen_size_x(s), ny, data->ox, data->oy);
}

void
window_copy_write_column(
    struct window *w, struct screen_write_ctx *ctx, u_int px)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	screen_write_move_cursor(ctx, px, 0);
	screen_write_copy_area(
	    ctx, &w->base, 1, screen_size_y(s), data->ox, data->oy);
}

void
window_copy_write_columns(
    struct window *w, struct screen_write_ctx *ctx, u_int px, u_int nx)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	screen_write_move_cursor(ctx, px, 0);
	screen_write_copy_area(
	    ctx, &w->base, nx, screen_size_y(s), data->ox, data->oy);
}

void
window_copy_redraw_lines(struct window *w, u_int py, u_int ny)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen_write_ctx	 	 ctx;
	u_int				 i;

	screen_write_start_window(&ctx, w);
	for (i = py; i < py + ny; i++)
		window_copy_write_line(w, &ctx, i);
	screen_write_move_cursor(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_redraw_screen(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;

	window_copy_redraw_lines(w, 0, screen_size_x(&data->screen));
}

void
window_copy_update_cursor(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen_write_ctx		 ctx;

	screen_write_start_window(&ctx, w);
	screen_write_move_cursor(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_start_selection(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;

	data->selx = screen_x(&w->base, data->cx) + data->ox;
	data->sely = screen_y(&w->base, data->cy) - data->oy;

	s->sel.flag = 1;
	window_copy_update_selection(w);
}

int
window_copy_update_selection(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	u_int				 sx, sy, tx, ty;

	if (!s->sel.flag)
		return (0);

	/* Find top-left of screen. */
	tx = screen_x(&w->base, 0) + data->ox;
	ty = screen_y(&w->base, 0) - data->oy;

	/* Adjust the selection. */
	sx = data->selx;
	sy = data->sely;
	if (sy < ty) {
		/* Above it. */
		sx = 0;
		sy = 0;
	} else if (sy > ty + screen_last_y(s)) {
		/* Below it. */
		sx = screen_last_x(s);
		sy = screen_last_y(s);
	} else if (sx < tx) {
		/* To the left. */
		sx = 0;
	} else if (sx > tx + screen_last_x(s)) {
		/* To the right. */
		sx = 0;
		sy++;
		if (sy > screen_last_y(s))
			sy = screen_last_y(s);
	} else {
		sx -= tx;
		sy -= ty;
	}
	sx = screen_x(s, sx);
	sy = screen_x(s, sy);

	screen_set_selection(
	    s, sx, sy, screen_x(s, data->cx), screen_y(s, data->cy));
	return (1);
}

void
window_copy_copy_selection(struct window *w, struct client *c)
{
	struct window_copy_mode_data	*data = w->modedata;
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
	xx = screen_x(&w->base, data->cx) + data->ox;
	yy = screen_y(&w->base, data->cy) - data->oy;
	if (xx < data->selx || (yy == data->sely && xx < data->selx)) {
		sx = xx; sy = yy;
		ex = data->selx; ey = data->sely;
	} else {
		sx = data->selx; sy = data->sely;
		ex = xx; ey = yy;
	}

	/* Trim ex to end of line. */
	xx = window_copy_find_length(w, ey);
	if (ex > xx)
		ex = xx;

	/* Copy the lines. */
	if (sy == ey)
		window_copy_copy_line(w, &buf, &off, sy, sx, ex);
	else {
		xx = window_copy_find_length(w, sy);
		window_copy_copy_line(w, &buf, &off, sy, sx, xx);
		if (ey - sy > 1) {
			for (i = sy + 1; i < ey - 1; i++) {
				xx = window_copy_find_length(w, i);
				window_copy_copy_line(w, &buf, &off, i, 0, xx);
			}
		}
		window_copy_copy_line(w, &buf, &off, ey, 0, ex);
	}

	/* Terminate buffer, overwriting final \n. */
	if (off != 0)
		buf[off - 1] = '\0';

	/* Add the buffer to the stack. */
	limit = options_get_number(&c->session->options, "buffer-limit");
	paste_add(&c->session->buffers, buf, limit);
	xfree(buf);
}

void
window_copy_copy_line(
    struct window *w, char **buf, size_t *off, u_int sy, u_int sx, u_int ex)
{
	u_char	i, xx;

	if (sx > ex)
		return;

	xx = window_copy_find_length(w, sy);
	if (ex > xx)
		ex = xx;
	if (sx > xx)
		sx = xx;

	if (sx < ex) {
		for (i = sx; i < ex; i++) {
			*buf = xrealloc(*buf, 1, (*off) + 1);
			(*buf)[*off] = w->base.grid_data[sy][i];
			(*off)++;
		}
	}

	*buf = xrealloc(*buf, 1, (*off) + 1);
	(*buf)[*off] = '\n';
	(*off)++;
}

u_int
window_copy_find_length(struct window *w, u_int py)
{
	u_int	px;

	px = w->base.grid_size[py];
	while (px > 0 && w->base.grid_data[py][px - 1] == ' ')
		px--;
	return (px);
}

void
window_copy_cursor_start_of_line(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;

	if (data->ox != 0)
		window_copy_scroll_right(w, data->ox);
	data->cx = 0;

	if (window_copy_update_selection(w))
		window_copy_redraw_lines(w, data->cy, 1);
	else
		window_copy_update_cursor(w);
}

void
window_copy_cursor_end_of_line(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	u_int				 px, py;

	py = screen_y(&w->base, data->cy) - data->oy;
	px = window_copy_find_length(w, py);

	/* On screen. */
	if (px > data->ox && px <= data->ox + screen_last_x(s))
		data->cx = px - data->ox;

	/* Off right of screen. */
	if (px > data->ox + screen_last_x(s)) {
		/* Move cursor to last and scroll screen. */
		window_copy_scroll_left(w,
		    px - data->ox - screen_last_x(s));
		data->cx = screen_last_x(s);
	}

	/* Off left of screen. */
	if (px <= data->ox) {
		if (px < screen_last_x(s)) {
			/* Short enough to fit on screen. */
			window_copy_scroll_right(w, data->ox);
			data->cx = px;
		} else {
			/* Too long to fit on screen. */
			window_copy_scroll_right(
			    w, data->ox - (px - screen_last_x(s)));
			data->cx = screen_last_x(s);
		}
 	}

	if (window_copy_update_selection(w))
		window_copy_redraw_lines(w, data->cy, 1);
	else
		window_copy_update_cursor(w);
}

void
window_copy_cursor_left(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;

	if (data->cx == 0) {
		if (data->ox > 0)
			window_copy_scroll_right(w, 1);
		else {
			window_copy_cursor_up(w);
			window_copy_cursor_end_of_line(w);
		}
	} else {
		data->cx--;
		if (window_copy_update_selection(w))
			window_copy_redraw_lines(w, data->cy, 1);
		else
			window_copy_update_cursor(w);
	}
}

void
window_copy_cursor_right(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	u_int				 px, py;

	py = screen_y(&w->base, data->cy) - data->oy;
	px = window_copy_find_length(w, py);

	if (data->cx >= px) {
		window_copy_cursor_start_of_line(w);
		window_copy_cursor_down(w);
	} else {
		data->cx++;
		if (window_copy_update_selection(w))
			window_copy_redraw_lines(w, data->cy, 1);
		else
			window_copy_update_cursor(w);
	}
}

void
window_copy_cursor_up(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	u_int				 ox, oy, px, py;

	oy = screen_y(&w->base, data->cy) - data->oy;
	ox = window_copy_find_length(w, oy);

	if (data->cy == 0)
		window_copy_scroll_down(w, 1);
	else {
		data->cy--;
		if (window_copy_update_selection(w))
			window_copy_redraw_lines(w, data->cy, 2);
		else
			window_copy_update_cursor(w);
	}

	py = screen_y(&w->base, data->cy) - data->oy;
	px = window_copy_find_length(w, py);

	if (data->cx + data->ox >= px || data->cx + data->ox >= ox)
		window_copy_cursor_end_of_line(w);
}

void
window_copy_cursor_down(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	u_int				 ox, oy, px, py;

	oy = screen_y(&w->base, data->cy) - data->oy;
	ox = window_copy_find_length(w, oy);

	if (data->cy == screen_last_y(s))
		window_copy_scroll_up(w, 1);
	else {
		data->cy++;
		if (window_copy_update_selection(w))
			window_copy_redraw_lines(w, data->cy - 1, 2);
		else
			window_copy_update_cursor(w);
	}

	py = screen_y(&w->base, data->cy) - data->oy;
	px = window_copy_find_length(w, py);

	if (data->cx + data->ox >= px || data->cx + data->ox >= ox)
		window_copy_cursor_end_of_line(w);
}

void
window_copy_cursor_next_word(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	u_int				 px, py, xx, skip;

	px = data->ox + data->cx;
	py = screen_y(&w->base, data->cy) - data->oy;
	xx = window_copy_find_length(w, py);

	skip = 1;
	if (px < xx) {
		/* If currently on a space, skip space. */
		if (window_copy_is_space(w, px, py))
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
				if (data->cy == screen_last_y(s)) {
					if (data->oy == 0)
						goto out;
				}
				
				px = 0;
				window_copy_cursor_down(w);
				
				py = screen_y(&w->base, data->cy) - data->oy;
				xx = window_copy_find_length(w, py);
			}
		}
		
		if (skip) {
			/* Currently skipping non-space (until space). */
			if (window_copy_is_space(w, px, py))
				break;
		} else {
			/* Currently skipping space (until non-space). */
			if (!window_copy_is_space(w, px, py))
				skip = 1;
		}

		px++;
	}
out:

	/* On screen. */
	if (px > data->ox && px <= data->ox + screen_last_x(s))
		data->cx = px - data->ox;

	/* Off right of screen. */
	if (px > data->ox + screen_last_x(s)) {
		/* Move cursor to last and scroll screen. */
		window_copy_scroll_left(w,
		    px - data->ox - screen_last_x(s));
		data->cx = screen_last_x(s);
	}

	/* Off left of screen. */
	if (px <= data->ox) {
		if (px < screen_last_x(s)) {
			/* Short enough to fit on screen. */
			window_copy_scroll_right(w, data->ox);
			data->cx = px;
		} else {
			/* Too long to fit on screen. */
			window_copy_scroll_right(
			    w, data->ox - (px - screen_last_x(s)));
			data->cx = screen_last_x(s);
		}
 	}
 
	if (window_copy_update_selection(w))
		window_copy_redraw_lines(w, data->cy, 1);
	else
		window_copy_update_cursor(w);
}

void
window_copy_cursor_previous_word(struct window *w)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	u_int				 ox, px, py, skip;

	ox = px = data->ox + data->cx;
	py = screen_y(&w->base, data->cy) - data->oy;

	skip = 1;
	if (px != 0) {
		/* If currently on a space, skip space. */
		if (window_copy_is_space(w, px - 1, py))
			skip = 0;
	}
	for (;;) {
		if (px == 0) {
			if (ox != 0)
				break;

			while (px == 0) {
				if (data->cy == 0 && (w->base.hsize == 0 ||
				    data->oy >= w->base.hsize - 1))
					goto out;
				
				window_copy_cursor_up(w);
				
				py = screen_y(&w->base, data->cy) - data->oy;
				px = window_copy_find_length(w, py);
			}
			goto out;
		}
		
		if (skip) {
			/* Currently skipping non-space (until space). */
			if (window_copy_is_space(w, px - 1, py))
				skip = 0;
		} else {
			/* Currently skipping space (until non-space). */
			if (!window_copy_is_space(w, px - 1, py))
				break;
		}
		
		px--;
	}
out:

	/* On screen. */
	if (px > data->ox && px <= data->ox + screen_last_x(s))
		data->cx = px - data->ox;

	/* Off right of screen. */
	if (px > data->ox + screen_last_x(s)) {
		/* Move cursor to last and scroll screen. */
		window_copy_scroll_left(w,
		    px - data->ox - screen_last_x(s));
		data->cx = screen_last_x(s);
	}

	/* Off left of screen. */
	if (px <= data->ox) {
		if (px < screen_last_x(s)) {
			/* Short enough to fit on screen. */
			window_copy_scroll_right(w, data->ox);
			data->cx = px;
		} else {
			/* Too long to fit on screen. */
			window_copy_scroll_right(
			    w, data->ox - (px - screen_last_x(s)));
			data->cx = screen_last_x(s);
		}
 	}
 
	if (window_copy_update_selection(w))
		window_copy_redraw_lines(w, data->cy, 1);
	else
		window_copy_update_cursor(w);
}

void
window_copy_scroll_left(struct window *w, u_int nx)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int				 i;

	if (data->ox > SHRT_MAX - nx)
		nx = SHRT_MAX - data->ox;
	if (nx == 0)
		return;
	data->ox += nx;
	window_copy_update_selection(w);

	screen_write_start_window(&ctx, w);
	for (i = 1; i < screen_size_y(s); i++) {
		screen_write_move_cursor(&ctx, 0, i);
		screen_write_delete_characters(&ctx, nx);
	}
	window_copy_write_columns(w, &ctx, screen_size_x(s) - nx, nx);
	window_copy_write_line(w, &ctx, 0);
	if (s->sel.flag) {
		window_copy_update_selection(w);
		window_copy_write_lines(w, &ctx, data->cy, 1);
	}
	screen_write_move_cursor(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_scroll_right(struct window *w, u_int nx)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;
	u_int		 		 i;

	if (data->ox < nx)
		nx = data->ox;
	if (nx == 0)
		return;
	data->ox -= nx;
	window_copy_update_selection(w);

	screen_write_start_window(&ctx, w);
	for (i = 1; i < screen_size_y(s); i++) {
		screen_write_move_cursor(&ctx, 0, i);
		screen_write_insert_characters(&ctx, nx);
	}
	window_copy_write_columns(w, &ctx, 0, nx);
	window_copy_write_line(w, &ctx, 0);
	if (s->sel.flag)
		window_copy_write_lines(w, &ctx, data->cy, 1);
	screen_write_move_cursor(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_scroll_up(struct window *w, u_int ny)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (data->oy < ny)
		ny = data->oy;
	if (ny == 0)
		return;
	data->oy -= ny;
	window_copy_update_selection(w);

	screen_write_start_window(&ctx, w);
	screen_write_move_cursor(&ctx, 0, 0);
	screen_write_delete_lines(&ctx, ny);
	window_copy_write_lines(w, &ctx, screen_size_y(s) - ny, ny);
	window_copy_write_line(w, &ctx, 0);
	if (s->sel.flag && screen_size_y(s) > ny)
		window_copy_write_lines(w, &ctx, screen_size_y(s) - ny - 1, 1);
	screen_write_move_cursor(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}

void
window_copy_scroll_down(struct window *w, u_int ny)
{
	struct window_copy_mode_data	*data = w->modedata;
	struct screen			*s = &data->screen;
	struct screen_write_ctx		 ctx;

	if (ny > w->base.hsize)
		return;

	if (data->oy > w->base.hsize - ny)
		ny = w->base.hsize - data->oy;
	if (ny == 0)
		return;
	data->oy += ny;
	window_copy_update_selection(w);

	screen_write_start_window(&ctx, w);
	screen_write_move_cursor(&ctx, 0, 0);
	screen_write_insert_lines(&ctx, ny);
	window_copy_write_lines(w, &ctx, 0, ny);
	if (s->sel.flag && screen_size_y(s) > ny)
		window_copy_write_lines(w, &ctx, ny, 1);
	else if (ny == 1) /* nuke position */
		window_copy_write_line(w, &ctx, 1);
	screen_write_move_cursor(&ctx, data->cx, data->cy);
	screen_write_stop(&ctx);
}
