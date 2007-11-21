/* $Id: screen.c,v 1.32 2007-11-21 18:24:49 nicm Exp $ */

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
 * Virtual screen.
 */

/* Colour to string. */
const char *
screen_colourstring(u_char c)
{
	switch (c) {
	case 0:
		return ("black");
	case 1:
		return ("red");
	case 2:
		return ("green");
	case 3:
		return ("yellow");
	case 4:
		return ("blue");
	case 5:
		return ("magenta");
	case 6:
		return ("cyan");
	case 7:
		return ("white");
	case 8:
		return ("default");
	}
	return (NULL);
}

/* String to colour. */
u_char
screen_stringcolour(const char *s)
{
	if (strcasecmp(s, "black") == 0 || (s[0] == '0' && s[1] == '\0'))
		return (0);
	if (strcasecmp(s, "red") == 0 || (s[0] == '1' && s[1] == '\0'))
		return (1);
	if (strcasecmp(s, "green") == 0 || (s[0] == '2' && s[1] == '\0'))
		return (2);
	if (strcasecmp(s, "yellow") == 0 || (s[0] == '3' && s[1] == '\0'))
		return (3);
	if (strcasecmp(s, "blue") == 0 || (s[0] == '4' && s[1] == '\0'))
		return (4);
	if (strcasecmp(s, "magenta") == 0 || (s[0] == '5' && s[1] == '\0'))
		return (5);
	if (strcasecmp(s, "cyan") == 0 || (s[0] == '6' && s[1] == '\0'))
		return (6);
	if (strcasecmp(s, "white") == 0 || (s[0] == '7' && s[1] == '\0'))
		return (7);
	if (strcasecmp(s, "default") == 0 || (s[0] == '8' && s[1] == '\0'))
		return (8);
	return (255);
}

/* Create a new screen. */
void
screen_create(struct screen *s, u_int dx, u_int dy)
{
	s->dx = dx;
	s->dy = dy;
	s->cx = 0;
	s->cy = 0;

	s->rupper = 0;
	s->rlower = s->dy - 1;

	s->hsize = 0;
	s->hlimit = SHRT_MAX;

	s->attr = SCREEN_DEFATTR;
	s->colr = SCREEN_DEFCOLR;

	s->mode = MODE_CURSOR;
	*s->title = '\0';

	s->grid_data = xmalloc(dy * (sizeof *s->grid_data));
	s->grid_attr = xmalloc(dy * (sizeof *s->grid_attr));
	s->grid_colr = xmalloc(dy * (sizeof *s->grid_colr));
	s->grid_size = xmalloc(dy * (sizeof *s->grid_size));
	screen_make_lines(s, 0, dy);
}

/* Resize screen. */
void
screen_resize(struct screen *s, u_int sx, u_int sy)
{
	u_int	i, ox, oy, ny, my;

	if (sx < 1)
		sx = 1;
	if (sy < 1)
		sy = 1;

	ox = s->dx;
	oy = s->dy;
	if (sx == ox && sy == oy)
		return;

	/* 
	 * X dimension.
	 */
	if (sx != ox) {
		/* Resize on-screen lines. */
		for (i = s->hsize; i < s->hsize + oy; i++) {
			if (sx > s->grid_size[i]) {
				s->grid_data[i] = 
				    xrealloc(s->grid_data[i], sx, 1);
				s->grid_attr[i] =
				    xrealloc(s->grid_attr[i], sx, 1);
				s->grid_colr[i] =
				    xrealloc(s->grid_colr[i], sx, 1);
				s->grid_size[i] = sx;
			}
			if (sx > ox) {
				screen_fill_cells(s, ox, i, sx - ox,
				    SCREEN_DEFDATA, SCREEN_DEFATTR,
				    SCREEN_DEFCOLR);
			}
		}
		if (s->cx >= sx)
			s->cx = sx - 1;
		s->dx = sx;
	}

	/*
	 * Y dimension.
	 */
	if (sy == oy)
		return;

	/* Size decreasing. */
	if (sy < oy) {
 		ny = oy - sy;
		if (s->cy != 0) {
			/*
			 * The cursor is not at the start. Try to remove as
			 * many lines as possible from the top. (Up to the
			 * cursor line.)
			 */
			my = s->cy;
			if (my > ny)
				my = ny;

			screen_free_lines(s, s->hsize, my);
			screen_move_lines(s, s->hsize, s->hsize + my, oy - my);
				
			s->cy -= my;
			oy -= my;
		} 

 		ny = oy - sy;
		if (ny > 0) {
			/*
			 * Remove any remaining lines from the bottom.
			 */
			screen_free_lines(s, s->hsize + oy - ny, ny);
			if (s->cy >= sy)
				s->cy = sy - 1;
		}
	}
	
	/* Resize line arrays. */
	ny = s->hsize + sy;
	s->grid_data = xrealloc(s->grid_data, ny, sizeof *s->grid_data);
	s->grid_attr = xrealloc(s->grid_attr, ny, sizeof *s->grid_attr);
	s->grid_colr = xrealloc(s->grid_colr, ny, sizeof *s->grid_colr);
	s->grid_size = xrealloc(s->grid_size, ny, sizeof *s->grid_size);
	s->dy = sy;

	/* Size increasing. */
	if (sy > oy)
		screen_make_lines(s, s->hsize + oy, sy - oy);

	s->rupper = 0;
	s->rlower = s->dy - 1;
}

/* Destroy a screen. */
void
screen_destroy(struct screen *s)
{
	screen_free_lines(s, 0, s->dy);
	xfree(s->grid_data);
	xfree(s->grid_attr);
	xfree(s->grid_colr);
	xfree(s->grid_size);
}

/* Initialise drawing. */
void
screen_draw_start(struct screen_draw_ctx *ctx,
    struct screen *s, struct buffer *b, u_int ox, u_int oy)
{
	ctx->s = s;
	ctx->b = b;

	ctx->ox = ox;
	ctx->oy = oy;

	ctx->cx = s->cx;
	ctx->cy = s->cy;

	ctx->attr = s->attr;
	ctx->colr = s->colr;

	input_store_two(b, CODE_SCROLLREGION, 1, screen_size_y(s));

	if (s->mode & MODE_CURSOR)
		input_store_zero(b, CODE_CURSOROFF);
}

/* Finalise drawing. */
void
screen_draw_stop(struct screen_draw_ctx *ctx)
{
	struct screen	*s = ctx->s;
	struct buffer	*b = ctx->b;

	input_store_two(b, CODE_SCROLLREGION, s->rupper + 1, s->rlower + 1);

	if (ctx->cx != s->cx || ctx->cy != s->cy)
		input_store_two(b, CODE_CURSORMOVE, s->cy + 1, s->cx + 1);

	if (ctx->attr != s->attr || ctx->colr != s->colr)
		input_store_two(b, CODE_ATTRIBUTES, s->attr, s->colr);
	
	if (s->mode & MODE_CURSOR)
		input_store_zero(b, CODE_CURSORON);
}

/* Get cell data. */
void
screen_draw_get_cell(struct screen_draw_ctx *ctx,
    u_int px, u_int py, u_char *data, u_char *attr, u_char *colr)
{
	struct screen	*s = ctx->s;
	u_int		 cx, cy;
	
	cx = ctx->ox + px;
	cy = screen_y(s, py) - ctx->oy;

	if (cx >= s->grid_size[cy]) {
		*data = SCREEN_DEFDATA;
		*attr = SCREEN_DEFATTR;
		*colr = SCREEN_DEFCOLR;
	} else {
		*data = s->grid_data[cy][cx];
		*attr = s->grid_attr[cy][cx];
		*colr = s->grid_colr[cy][cx];
	}
}

/* Move cursor. */
void
screen_draw_move(struct screen_draw_ctx *ctx, u_int px, u_int py)
{
	if (px == ctx->cx && py == ctx->cy)
		return;

	input_store_two(ctx->b, CODE_CURSORMOVE, py + 1, px + 1);

	ctx->cx = px;
	ctx->cy = py;
}

/* Set attributes. */
void
screen_draw_set_attributes(
    struct screen_draw_ctx *ctx, u_char attr, u_char colr)
{
	if (attr != ctx->attr || colr != ctx->colr) {
		input_store_two(ctx->b, CODE_ATTRIBUTES, attr, colr);
		ctx->attr = attr;
		ctx->colr = colr;
	}
}

/* Draw single cell. */
void
screen_draw_cell(struct screen_draw_ctx *ctx, u_int px, u_int py)
{
	struct buffer	*b = ctx->b;
	u_char		 data, attr, colr;

	screen_draw_move(ctx, px, py);

	screen_draw_get_cell(ctx, px, py, &data, &attr, &colr);
	screen_draw_set_attributes(ctx, attr, colr);
	input_store8(b, data);

	/*
	 * Don't try to wrap as it will cause problems when screen is smaller
	 * than client.
	 */
	ctx->cx++;
}

/* Draw range of cells. */
void
screen_draw_cells(struct screen_draw_ctx *ctx, u_int px, u_int py, u_int nx)
{
	u_int	i;

	for (i = px; i < px + nx; i++)
		screen_draw_cell(ctx, i, py);
}

/* Draw single column. */
void
screen_draw_column(struct screen_draw_ctx *ctx, u_int px)
{
	u_int	i;

	for (i = 0; i < screen_size_y(ctx->s); i++)
		screen_draw_cell(ctx, px, i);
}

/* Draw single line. */
void
screen_draw_line(struct screen_draw_ctx *ctx, u_int py)
{
	screen_draw_cells(ctx, 0, py, screen_size_x(ctx->s));
}

/* Draw set of lines. */
void
screen_draw_lines(struct screen_draw_ctx *ctx, u_int py, u_int ny)
{
	u_int		 i;

	for (i = py; i < py + ny; i++)
		screen_draw_line(ctx, i);
}

/* Draw entire screen. */
void
screen_draw_screen(struct screen_draw_ctx *ctx)
{
	screen_draw_lines(ctx, 0, screen_size_y(ctx->s));
}

/* Create a range of lines. */
void
screen_make_lines(struct screen *s, u_int py, u_int ny)
{
	u_int	i;

	for (i = py; i < py + ny; i++) {
		s->grid_data[i] = xmalloc(s->dx);
		s->grid_attr[i] = xmalloc(s->dx);
		s->grid_colr[i] = xmalloc(s->dx);
		s->grid_size[i] = s->dx;
	}
	screen_fill_lines(
	    s, py, ny, SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}


/* Free a range of ny lines at py. */
void
screen_free_lines(struct screen *s, u_int py, u_int ny)
{
	u_int	i;

	for (i = py; i < py + ny; i++) {
		xfree(s->grid_data[i]);
		xfree(s->grid_attr[i]);
		xfree(s->grid_colr[i]);
		s->grid_size[i] = 0;
	}
}

/* Move a range of lines. */
void
screen_move_lines(struct screen *s, u_int dy, u_int py, u_int ny)
{
	memmove(
	    &s->grid_data[dy], &s->grid_data[py], ny * (sizeof *s->grid_data));
	memmove(
	    &s->grid_attr[dy], &s->grid_attr[py], ny * (sizeof *s->grid_attr));
	memmove(
	    &s->grid_colr[dy], &s->grid_colr[py], ny * (sizeof *s->grid_colr));
	memmove(
	    &s->grid_size[dy], &s->grid_size[py], ny * (sizeof *s->grid_size));
}

/* Fill a range of lines. */
void
screen_fill_lines(
    struct screen *s, u_int py, u_int ny, u_char data, u_char attr, u_char colr)
{
	u_int	i;

	for (i = py; i < py + ny; i++)
		screen_fill_cells(s, 0, i, s->dx, data, attr, colr);
}

/* Fill a range of cells. */
void
screen_fill_cells(struct screen *s,
    u_int px, u_int py, u_int nx, u_char data, u_char attr, u_char colr)
{
	memset(&s->grid_data[py][px], data, nx);
	memset(&s->grid_attr[py][px], attr, nx);
	memset(&s->grid_colr[py][px], colr, nx);
}
