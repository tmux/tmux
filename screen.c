/* $Id: screen.c,v 1.65 2008-07-24 07:01:57 nicm Exp $ */

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
 *
 * A screen is stored as three arrays of lines of 8-bit values, one for the
 * actual characters (data), one for attributes and one for colours. Three
 * seperate blocks means memset and friends can be used. Each array is y by x
 * in size, row then column order. Sizes are 0-based. There is an additional
 * array of u_ints with the size of each line.
 *
 * Each screen has a history starting at the beginning of the arrays and
 * extending for hsize lines. Beyond that is the screen display of size
 * dy:
 *
 * ----------- array base
 * |         |
 * | history |
 * ----------- array base + hsize
 * |         |
 * | display |
 * |         |
 * ----------- array base + hsize + dy
 *
 * The screen_x/screen_y macros are used to convert a cell on the displayed
 * area to an absolute position in the arrays.
 *
 * Screen handling code is split into four files:
 *
 *	screen.c: Creation/deletion, utility functions, and basic functions to
 *		  manipulate the screen based on offsets from the base.
 *	screen-display.c: Basic functions for manipulating the displayed
 *			  part of the screen. x,y coordinates passed to these
 *			  are relative to the display. These are largely
 *			  utility functions for screen-write.c.
 *	screen-redraw.c: Functions for redrawing all or part of a screen to
 *			 one or more ttys. A context is filled via one of the
 *			 screen_redraw_start* variants which sets up (removes
 *			 cursor etc) and figures out which tty_write_* function
 *			 to use to write to the terminals, then the other
 *			 screen_redraw_* functions are used to draw the screen,
 *			 and screen_redraw_stop used to reset the cursor and
 *			 clean up. These are used when changing window and a
 *			 few other bits (status line).
 * 	screen-write.c: Functions for modifying (writing into) the screen and
 *			optionally simultaneously updating one or more ttys.
 *			These are used in much the same way as the redraw
 *			functions. These are used to update when parsing
 *			input from the window (input.c) and for the various
 *			other modes which maintain private screens.
 *
 * If you're thinking this all seems too complicated, that's because it is :-/.
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
screen_create(struct screen *s, u_int dx, u_int dy, u_int hlimit)
{
	s->dx = dx;
	s->dy = dy;
	s->cx = 0;
	s->cy = 0;

	s->rupper = 0;
	s->rlower = s->dy - 1;

	s->hsize = 0;
	s->hlimit = hlimit;

	s->attr = SCREEN_DEFATTR;
	s->colr = SCREEN_DEFCOLR;

	s->mode = MODE_CURSOR;
	s->title = xstrdup("");

	s->grid_data = xmalloc(dy * (sizeof *s->grid_data));
	s->grid_attr = xmalloc(dy * (sizeof *s->grid_attr));
	s->grid_colr = xmalloc(dy * (sizeof *s->grid_colr));
	s->grid_size = xmalloc(dy * (sizeof *s->grid_size));
	screen_make_lines(s, 0, dy);

	screen_clear_selection(s);
}

/* Reinitialise screen. */
void
screen_reset(struct screen *s)
{
	s->cx = 0;
	s->cy = 0;

	s->rupper = 0;
	s->rlower = s->dy - 1;

	s->attr = SCREEN_DEFATTR;
	s->colr = SCREEN_DEFCOLR;

	s->mode = MODE_CURSOR;

	screen_display_fill_area(s, 0, 0, 
	    screen_size_x(s), screen_size_y(s), ' ', 0, 0x88);
	
	screen_clear_selection(s);	
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

	if (screen_check_selection(s, cx, cy))
		*attr |= ATTR_REVERSE;
}

/* Set a cell. */
void
screen_set_cell(struct screen *s,
    u_int cx, u_int cy, u_char data, u_char attr, u_char colr)
{
	if (cx >= s->grid_size[cy])
		screen_expand_line(s, cy, cx + 1);

	s->grid_data[cy][cx] = data;
	s->grid_attr[cy][cx] = attr;
	s->grid_colr[cy][cx] = colr;
}

/* Destroy a screen. */
void
screen_destroy(struct screen *s)
{
	xfree(s->title);
	screen_free_lines(s, 0, s->dy + s->hsize);
	xfree(s->grid_data);
	xfree(s->grid_attr);
	xfree(s->grid_colr);
	xfree(s->grid_size);
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

/* Fill an area. */
void
screen_fill_area(struct screen *s, u_int px, u_int py,
    u_int nx, u_int ny, u_char data, u_char attr, u_char colr)
{
	u_int	i, j;

	for (i = py; i < py + ny; i++) {
		for (j = px; j < px + nx; j++)
			screen_set_cell(s, j, i, data, attr, colr);
	}
}

/* Set selection. */
void
screen_set_selection(struct screen *s, u_int sx, u_int sy, u_int ex, u_int ey)
{
	struct screen_sel	*sel = &s->sel;

	sel->flag = 1;
	if (ey < sy || (sy == ey && ex < sx)) {
		sel->sx = ex; sel->sy = ey;
		sel->ex = sx; sel->ey = sy;
	} else {
		sel->sx = sx; sel->sy = sy;
		sel->ex = ex; sel->ey = ey;
	}
}

/* Clear selection. */
void
screen_clear_selection(struct screen *s)
{
	struct screen_sel	*sel = &s->sel;

	sel->flag = 0;
}

/* Check if cell in selection. */
int
screen_check_selection(struct screen *s, u_int px, u_int py)
{
	struct screen_sel	*sel = &s->sel;

	if (!sel->flag || py < sel->sy || py > sel->ey)
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
