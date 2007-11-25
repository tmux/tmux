/* $Id: screen.c,v 1.46 2007-11-25 11:13:46 nicm Exp $ */

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
	s->hlimit = history_limit;

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
		/* 
		 * If getting smaller, nuke any data in lines over the new
		 * size.
		 */
		if (sx < ox) {
			for (i = s->hsize; i < s->hsize + oy; i++) {
				if (s->grid_size[i] > sx)
					screen_reduce_line(s, i, sx);
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

/* Expand line. */
void
screen_expand_line(struct screen *s, u_int py, u_int nx)
{
	u_int	ox;

	ox = s->grid_size[py];
	s->grid_size[py] = nx;

	s->grid_data[py] = xrealloc(s->grid_data[py], 1, nx);
	memset(&s->grid_data[py][ox], SCREEN_DEFDATA, nx - ox);
	s->grid_attr[py] = xrealloc(s->grid_attr[py], 1, nx);
	memset(&s->grid_attr[py][ox], SCREEN_DEFATTR, nx - ox);
	s->grid_colr[py] = xrealloc(s->grid_colr[py], 1, nx);
	memset(&s->grid_colr[py][ox], SCREEN_DEFCOLR, nx - ox);
}

/* Reduce line. */
void
screen_reduce_line(struct screen *s, u_int py, u_int nx)
{
	s->grid_size[py] = nx;

	s->grid_data[py] = xrealloc(s->grid_data[py], 1, nx);
	s->grid_attr[py] = xrealloc(s->grid_attr[py], 1, nx);
	s->grid_colr[py] = xrealloc(s->grid_colr[py], 1, nx);
}

/* Get cell. */
void
screen_get_cell(struct screen *s,
    u_int cx, u_int cy, u_char *data, u_char *attr, u_char *colr)
{
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

/* Set a cell. */
void
screen_set_cell(struct screen *s,
    u_int cx, u_int cy, u_char data, u_char attr, u_char colr)
{
	if (cx >= s->grid_size[cy]) {
		if (data == SCREEN_DEFDATA &&
		    attr == SCREEN_DEFATTR && colr == SCREEN_DEFCOLR)
			return;
		screen_expand_line(s, cy, cx + 1);
	}

	s->grid_data[cy][cx] = data;
	s->grid_attr[cy][cx] = attr;
	s->grid_colr[cy][cx] = colr;
}

/* Destroy a screen. */
void
screen_destroy(struct screen *s)
{
	screen_free_lines(s, 0, s->dy + s->hsize);
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

	/* Resetting the scroll region homes the cursor so start at 0,0. */
	ctx->cx = 0;
	ctx->cy = 0;

	ctx->sel.flag = 0;

	ctx->attr = s->attr;
	ctx->colr = s->colr;

	input_store_two(b, CODE_SCROLLREGION, 1, screen_size_y(s));
	input_store_zero(b, CODE_CURSOROFF);
}

/* Set selection. */
void
screen_draw_set_selection(struct screen_draw_ctx *ctx, 
    int flag, u_int sx, u_int sy, u_int ex, u_int ey)
{
	struct screen_draw_sel	*sel = &ctx->sel;

	sel->flag = flag;
	if (!sel->flag)
		return;

	if (ey < sy || (sy == ey && ex < sx)) {
		sel->sx = ex; sel->sy = ey;
		sel->ex = sx; sel->ey = sy;
	} else {
		sel->sx = sx; sel->sy = sy;
		sel->ex = ex; sel->ey = ey;
	}
}

/* Check if cell in selection. */
int
screen_draw_check_selection(struct screen_draw_ctx *ctx, u_int px, u_int py)
{
	struct screen_draw_sel	*sel = &ctx->sel;

	if (!sel->flag)
		return (0);

	if (py < sel->sy || py > sel->ey)
		return (0);

	if (py == sel->sy && py == sel->ey) {
		if (px < sel->sx || px > sel->ex)
			return (0);
		return (1);
	}

	if ((py == sel->sy && px < sel->sx) || (py == sel->ey && px > sel->ex))
		return (0);
	return (1);
}

/* Get cell data during drawing. */
void
screen_draw_get_cell(struct screen_draw_ctx *ctx,
    u_int px, u_int py, u_char *data, u_char *attr, u_char *colr)
{
	struct screen	*s = ctx->s;
	u_int		 cx, cy;
	
	cx = ctx->ox + px;
	cy = screen_y(s, py) - ctx->oy;

	screen_get_cell(s, cx, cy, data, attr, colr);

	if (screen_draw_check_selection(ctx, cx, cy))
		*attr |= ATTR_REVERSE;
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

	if (s->mode & MODE_BACKGROUND) {
		if (s->mode & MODE_BGCURSOR)
			input_store_zero(b, CODE_CURSORON);
	} else {
		if (s->mode & MODE_CURSOR)
			input_store_zero(b, CODE_CURSORON);
	}
}

/* Move cursor. */
void
screen_draw_move(struct screen_draw_ctx *ctx, u_int px, u_int py)
{
	if (px == ctx->cx && py == ctx->cy)
		return;

	if (px == 0 && py == ctx->cy)
		input_store8(ctx->b, '\r');
	else if (px == ctx->cx && py == ctx->cy + 1)
		input_store8(ctx->b, '\n');
	else if (px == 0 && py == ctx->cy + 1) {
		input_store8(ctx->b, '\r');
		input_store8(ctx->b, '\n');
	} else 
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
	u_int	cx, cy;

	cy = screen_y(ctx->s, py) - ctx->oy;
	cx = ctx->s->grid_size[cy];

	if (screen_size_x(ctx->s) < 3 || cx >= screen_size_x(ctx->s) - 3)
		screen_draw_cells(ctx, 0, py, screen_size_x(ctx->s));
	else {
		screen_draw_cells(ctx, 0, py, cx);
		screen_draw_move(ctx, cx, py);
		input_store_zero(ctx->b, CODE_CLEARENDOFLINE);
	}
}

/* Draw set of lines. */
void
screen_draw_lines(struct screen_draw_ctx *ctx, u_int py, u_int ny)
{
	u_int	i;

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
		s->grid_data[i] = NULL;
		s->grid_attr[i] = NULL;
		s->grid_colr[i] = NULL;
		s->grid_size[i] = 0;
	}
}

/* Free a range of ny lines at py. */
void
screen_free_lines(struct screen *s, u_int py, u_int ny)
{
	u_int	i;

	for (i = py; i < py + ny; i++) {
		if (s->grid_data[i] != NULL)
			xfree(s->grid_data[i]);
		s->grid_data[i] = NULL;
		if (s->grid_attr[i] != NULL)
			xfree(s->grid_attr[i]);
		s->grid_attr[i] = NULL;
		if (s->grid_colr[i] != NULL)
			xfree(s->grid_colr[i]);
		s->grid_colr[i] = NULL;
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
	u_int	i;

	for (i = px; i < px + nx; i++)
		screen_set_cell(s, i, py, data, attr, colr);
}
