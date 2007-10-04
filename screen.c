/* $Id: screen.c,v 1.20 2007-10-04 19:03:51 nicm Exp $ */

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
 * Virtual screen and basic terminal emulator.
 *
 * XXX Much of this file sucks.
 */

void	 screen_free_lines(struct screen *, u_int, u_int);
void	 screen_make_lines(struct screen *, u_int, u_int);
void	 screen_move_lines(struct screen *, u_int, u_int, u_int);
void	 screen_fill_lines(
	     struct screen *, u_int, u_int, u_char, u_char, u_char);

#define screen_last_y(s) ((s)->sy - 1)
#define screen_last_x(s) ((s)->sx - 1)

#define screen_range_y(lx, rx) (((rx) - (lx)) + 1)
#define screen_range_x(ux, lx) (((lx) - (ux)) + 1)

#define screen_offset_y(py, ny) ((py) + (ny) - 1)
#define screen_offset_x(px, nx) ((px) + (nx) - 1)

/* Create a new screen. */
void
screen_create(struct screen *s, u_int sx, u_int sy)
{
	s->sx = sx;
	s->sy = sy;
	s->cx = 0;
	s->cy = 0;

	s->ry_upper = 0;
	s->ry_lower = screen_last_y(s);

	s->attr = SCREEN_DEFATTR;
	s->colr = SCREEN_DEFCOLR;

	s->mode = MODE_CURSOR;
	*s->title = '\0';

	s->grid_data = xmalloc(sy * (sizeof *s->grid_data));
	s->grid_attr = xmalloc(sy * (sizeof *s->grid_attr));
	s->grid_colr = xmalloc(sy * (sizeof *s->grid_colr));
	screen_make_lines(s, 0, sy);
	screen_fill_screen(s, SCREEN_DEFDATA, 0, SCREEN_DEFCOLR);
}

/* Resize screen. */
void
screen_resize(struct screen *s, u_int sx, u_int sy)
{
	u_int	i, ox, oy, ny;

	if (sx == s->sx || sy == s->sy)
		return;

	if (sx < 1)
		sx = 1;
	if (sy < 1)
		sy = 1;

	ox = s->sx;
	oy = s->sy;
	s->sx = sx;
	s->sy = sy;

	s->ry_upper = 0;
	s->ry_lower = screen_last_y(s);

	log_debug("resizing screen (%u, %u) -> (%u, %u)", ox, oy, sx, sy);

	if (sy < oy) {
		ny = oy - sy;
		if (ny > s->cy)
			ny = s->cy;

		if (ny != 0) {
			log_debug("removing %u lines from top", ny);
			for (i = 0; i < ny; i++) {
				log_debug("freeing line %u", i);
				xfree(s->grid_data[i]);
				xfree(s->grid_attr[i]);
				xfree(s->grid_colr[i]);
			}
			memmove(s->grid_data, s->grid_data + ny,
			    (oy - ny) * (sizeof *s->grid_data));
			memmove(s->grid_attr, s->grid_attr + ny,
			    (oy - ny) * (sizeof *s->grid_attr));
			memmove(s->grid_colr, s->grid_colr + ny,
			    (oy - ny) * (sizeof *s->grid_colr));
			s->cy -= ny;
		}
		if (ny < oy - sy) {
			log_debug(
			    "removing %u lines from bottom", oy - sy - ny);
			for (i = sy; i < oy - ny; i++) {
				log_debug("freeing line %u", i);
				  xfree(s->grid_data[i]);
				  xfree(s->grid_attr[i]);
				  xfree(s->grid_colr[i]);
			}
			if (s->cy >= sy)
				s->cy = sy - 1;
		}
	}
	if (sy != oy) {
		s->grid_data = xrealloc(s->grid_data, sy, sizeof *s->grid_data);
		s->grid_attr = xrealloc(s->grid_attr, sy, sizeof *s->grid_attr);
		s->grid_colr = xrealloc(s->grid_colr, sy, sizeof *s->grid_colr);
	}
	if (sy > oy) {
		for (i = oy; i < sy; i++) {
			log_debug("allocating line %u", i);
			s->grid_data[i] = xmalloc(sx);
			s->grid_attr[i] = xmalloc(sx);
			s->grid_colr[i] = xmalloc(sx);
			screen_fill_line(s, i,
			    SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
		}
		sy = oy;
	}

	if (sx != ox) {
		for (i = 0; i < sy; i++) {
			log_debug("adjusting line %u to %u", i, sx);
			s->grid_data[i] = xrealloc(s->grid_data[i], sx, 1);
			s->grid_attr[i] = xrealloc(s->grid_attr[i], sx, 1);
			s->grid_colr[i] = xrealloc(s->grid_colr[i], sx, 1);
			if (sx > ox) {
				screen_fill_end_of_line(s, ox, i,
				    SCREEN_DEFDATA, SCREEN_DEFATTR,
				    SCREEN_DEFCOLR);
			}
		}
		if (s->cx >= sx)
			s->cx = sx - 1;
	}
}

/* Destroy a screen. */
void
screen_destroy(struct screen *s)
{
	screen_free_lines(s, 0, s->sy);
	xfree(s->grid_data);
	xfree(s->grid_attr);
	xfree(s->grid_colr);
}

/* Draw a set of lines on the screen. */
void
screen_draw(struct screen *s, struct buffer *b, u_int uy, u_int ly)
{
	u_char		 attr, colr;
	u_int		 i, j;

	if (uy > screen_last_y(s) || ly > screen_last_y(s) || ly < uy)
		fatalx("bad range");

	/* XXX. This is naive and rough right now. */
	attr = 0;
	colr = SCREEN_DEFCOLR;

	input_store_two(b, CODE_SCROLLREGION, s->ry_upper + 1, s->ry_lower + 1);

	input_store_zero(b, CODE_CURSOROFF);
	input_store_two(b, CODE_ATTRIBUTES, attr, colr);

	for (j = uy; j <= ly; j++) {
		input_store_two(b, CODE_CURSORMOVE, j + 1, 1);

		for (i = 0; i <= screen_last_x(s); i++) {
			if (s->grid_attr[j][i] != attr ||
			    s->grid_colr[j][i] != colr) {
				input_store_two(b, CODE_ATTRIBUTES,
				    s->grid_attr[j][i], s->grid_colr[j][i]);
				attr = s->grid_attr[j][i];
				colr = s->grid_colr[j][i];
			}
			input_store8(b, s->grid_data[j][i]);
		}
	}
	input_store_two(b, CODE_CURSORMOVE, s->cy + 1, s->cx + 1);

	input_store_two(b, CODE_ATTRIBUTES, s->attr, s->colr);
	if (s->mode & MODE_CURSOR)
		input_store_zero(b, CODE_CURSORON);
}

/* Make a range of lines. */
void
screen_make_lines(struct screen *s, u_int py, u_int ny)
{
	u_int	i;

	log_debug("making lines %u,%u", py, ny);

	if (py > screen_last_y(s) || py + ny - 1 > screen_last_y(s))
		fatalx("bad range");
	
	for (i = py; i < py + ny; i++) {
		s->grid_data[i] = xmalloc(s->sx);
		s->grid_attr[i] = xmalloc(s->sx);
		s->grid_colr[i] = xmalloc(s->sx);
	}
}

/* Free a range of lines. */
void
screen_free_lines(struct screen *s, u_int py, u_int ny)
{
	u_int	i;

	log_debug("freeing lines %u,%u", py, ny);

	if (py > screen_last_y(s) || py + ny - 1 > screen_last_y(s))
		fatalx("bad range");

	for (i = py; i < py + ny; i++) {
		xfree(s->grid_data[i]);
		xfree(s->grid_attr[i]);
		xfree(s->grid_colr[i]);
	}
}

/* Move a range of lines. */
void
screen_move_lines(struct screen *s, u_int dy, u_int py, u_int ny)
{
	log_debug("moving lines %u,%u to %u", py, ny, dy);

	if (py > screen_last_y(s) || py + ny - 1 > screen_last_y(s))
		fatalx("bad range");
	if (dy > screen_last_y(s) || dy == py)
		fatalx("bad destination");
	if (dy + ny - 1 > screen_last_y(s))
		fatalx("bad size");

	memmove(
	    &s->grid_data[dy], &s->grid_data[py], ny * (sizeof *s->grid_data));
	memmove(
	    &s->grid_attr[dy], &s->grid_attr[py], ny * (sizeof *s->grid_attr));
	memmove(
	    &s->grid_colr[dy], &s->grid_colr[py], ny * (sizeof *s->grid_colr));
}

/* Fill a range of lines. */
void
screen_fill_lines(
    struct screen *s, u_int py, u_int ny, u_char data, u_char attr, u_char colr)
{
	u_int	i;

	log_debug("filling lines %u,%u", py, ny);

	if (py > screen_last_y(s) || py + ny - 1 > screen_last_y(s))
		fatalx("bad range");

	for (i = py; i < py + ny; i++)
		screen_fill_line(s, i, data, attr, colr);
}

/* Write a single character to the screen at the cursor and move forward. */
void
screen_write_character(struct screen *s, u_char ch)
{
	if (s->cx > screen_last_x(s)) {
		s->cx = 0;
		screen_cursor_down_scroll(s);
	}

	s->grid_data[s->cy][s->cx] = ch;
	s->grid_attr[s->cy][s->cx] = s->attr;
	s->grid_colr[s->cy][s->cx] = s->colr;

	s->cx++;
}

/* Move cursor up and scroll if necessary. */
void
screen_cursor_up_scroll(struct screen *s)
{
	if (s->cy == s->ry_upper)
		screen_scroll_region_down(s);
	else if (s->cy > 0)
		s->cy--;
}

/* Move cursor down and scroll if necessary. */
void
screen_cursor_down_scroll(struct screen *s)
{
	if (s->cy == s->ry_lower)
		screen_scroll_region_up(s);
	else if (s->cy < screen_last_y(s))
		s->cy++;
}

/* Scroll region up. */
void
screen_scroll_region_up(struct screen *s)
{
	log_debug("scrolling region up: %u:%u", s->ry_upper, s->ry_lower);

	/* 
	 * Scroll scrolling region up:
	 * 	- delete ry_upper
	 *	- move ry_upper + 1 to ry_lower to ry_upper
	 *	- make new line at ry_lower
	 *
	 * Example: region is 12 to 24.
	 *	ry_lower = 24, ry_upper = 12
	 *	screen_free_lines(s, 12, 1);
	 *	screen_move_lines(s, 12, 13, 12);
	 *	screen_make_lines(s, 24, 1);
	 */

	screen_free_lines(s, s->ry_upper, 1);

	if (s->ry_upper != s->ry_lower) {
		screen_move_lines(s, 
		    s->ry_upper, s->ry_upper + 1, s->ry_lower - s->ry_upper);
	}

	screen_make_lines(s, s->ry_lower, 1);
	screen_fill_lines(
	    s, s->ry_lower, 1, SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}

/* Scroll region down. */
void
screen_scroll_region_down(struct screen *s)
{
	log_debug("scrolling region down: %u:%u", s->ry_upper, s->ry_lower);

	/* 
	 * Scroll scrolling region down:
	 * 	- delete ry_lower
	 *	- move ry_upper to ry_lower - 1 to ry_upper + 1
	 *	- make new line at ry_upper
	 *
	 * Example: region is 12 to 24.
	 *	ry_lower = 24, ry_upper = 12
	 *	screen_free_lines(s, 24, 1);
	 *	screen_move_lines(s, 13, 12, 12);
	 *	screen_make_lines(s, 12, 1);
	 */

	screen_free_lines(s, s->ry_lower, 1);

	if (s->ry_upper != s->ry_lower) {
		screen_move_lines(s,
		    s->ry_upper + 1, s->ry_upper, s->ry_lower - s->ry_upper);
	}

	screen_make_lines(s, s->ry_upper, 1);
	screen_fill_lines(
	    s, s->ry_upper, 1, SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}

/* Scroll screen up. */
void
screen_scroll_up(struct screen *s, u_int ny)
{
	screen_delete_lines(s, 0, ny);
}

/* Scroll screen down. */
void
screen_scroll_down(struct screen *s, u_int ny)
{
	screen_insert_lines(s, 0, ny);
}

/* Fill entire screen. */
void
screen_fill_screen(struct screen *s, u_char data, u_char attr, u_char colr)
{
	screen_fill_end_of_screen(s, 0, 0, data, attr, colr);
}

/* Fill single line. */
void
screen_fill_line(
    struct screen *s, u_int py, u_char data, u_char attr, u_char colr)
{
	screen_fill_end_of_line(s, 0, py, data, attr, colr);
}

/* Fill to end of screen. */
void
screen_fill_end_of_screen(
    struct screen *s, u_int px, u_int py, u_char data, u_char attr, u_char colr)
{
	if (py > screen_last_y(s))
		return;

	if (px != 0) {
		screen_fill_end_of_line(s, px, py, data, attr, colr);
		if (py++ > screen_last_y(s))
			return;
	}

	while (py <= screen_last_y(s)) {
		screen_fill_line(s, py, data, attr, colr);
		py++;
	}
}

/* Fill to end of line. */
void
screen_fill_end_of_line(
    struct screen *s, u_int px, u_int py, u_char data, u_char attr, u_char colr)
{
	if (px > screen_last_x(s))
		return;
	if (py > screen_last_y(s))
		return;

	memset(&s->grid_data[py][px], data, s->sx - px);
	memset(&s->grid_attr[py][px], attr, s->sx - px);
	memset(&s->grid_colr[py][px], colr, s->sx - px);
}

/* Fill to start of line. */
void
screen_fill_start_of_line(
    struct screen *s, u_int px, u_int py, u_char data, u_char attr, u_char colr)
{
	if (px > screen_last_x(s))
		return;
	if (py > screen_last_y(s))
		return;

	memset(s->grid_data[py], data, px);
	memset(s->grid_attr[py], attr, px);
	memset(s->grid_colr[py], colr, px);
}

/* Insert lines. */
void
screen_insert_lines(struct screen *s, u_int py, u_int ny)
{
	if (py > screen_last_y(s))
		return;

	if (py + ny > screen_last_y(s))
		ny = screen_last_y(s) - py;
	log_debug("inserting lines: %u,%u", py, ny);

	/*
	 * Insert range of ny lines at py:
	 *	- Free ny lines from end of screen.
	 *	- Move from py to end of screen - ny to py + ny.
	 *	- Create ny lines at py.
	 *
	 * Example: insert 2 lines at 4.
	 *	sy = 10, py = 4, ny = 2
	 *	screen_free_lines(s, 8, 2);	- delete lines 8,9
	 *	screen_move_lines(s, 6, 4, 4);	- move 4,5,6,7 to 6,7,8,9
	 *	screen_make_lines(s, 4, 2);	- make lines 4,5
	 */

	screen_free_lines(s, s->sy - ny, ny);

	if (py != screen_last_y(s))
		screen_move_lines(s, py + ny, py, s->sy - py - ny);

	screen_make_lines(s, py, ny);
	screen_fill_lines(
	    s, py, ny, SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}

/* Insert lines in region. */
void
screen_insert_lines_region(struct screen *s, u_int py, u_int ny)
{
	if (py < s->ry_upper || py > s->ry_lower)
		return;
	if (py + ny > s->ry_lower)
		ny = s->ry_lower - py;
	log_debug("inserting lines in region: %u,%u (%u,%u)", py, ny,
	    s->ry_upper, s->ry_lower);

	/*
	 * Insert range of ny lines at py:
	 *	- Free ny lines from end of screen.
	 *	- Move from py to end of screen - ny to py + ny.
	 *	- Create ny lines at py.
	 *
	 * Example: insert 2 lines at 4.
	 *	ryu = 11, ryl = 16, py = 13, ny = 2
	 *	screen_free_lines(s, 15, 2);	- delete lines 15,16
	 *	screen_move_lines(s, 13, 15, 2);- move 13,14 to 15,16
	 *	screen_make_lines(s, 13, 2);	- make lines 13,14
	 */

	screen_free_lines(s, (s->ry_lower + 1) - ny, ny);

	if (py != s->ry_lower)
		screen_move_lines(s, py + ny, py, (s->ry_lower + 1) - py - ny);

	screen_make_lines(s, py, ny);
	screen_fill_lines(
	    s, py, ny, SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}

/* Delete lines. */
void
screen_delete_lines(struct screen *s, u_int py, u_int ny)
{
	if (py > screen_last_y(s))
		return;

	if (py + ny > screen_last_y(s))
		ny = screen_last_y(s) - py;
	log_debug("deleting lines: %u,%u", py, ny);

	/*
	 * Delete range of ny lines at py:
	 * 	- Free ny lines at py.
	 *	- Move from py + ny to end of screen to py.
	 *	- Free and recreate last ny lines.
	 *
	 * Example: delete lines 3,4.
	 *	sy = 10, py = 3, ny = 2
	 *	screen_free_lines(s, 3, 2);	- delete lines 3,4
	 *	screen_move_lines(s, 3, 5, 5);	- move 5,6,7,8,9 to 3
	 *	screen_make_lines(s, 8, 2);	- make lines 8,9
	 */

	screen_free_lines(s, py, ny);

	if (py != screen_last_y(s))
		screen_move_lines(s, py, py + ny, s->sy - py - ny);

	screen_make_lines(s, s->sy - ny, ny);
	screen_fill_lines(
	    s, s->sy - ny, ny, SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}

/* Delete lines inside scroll region. */
void
screen_delete_lines_region(struct screen *s, u_int py, u_int ny)
{
	if (py < s->ry_upper || py > s->ry_lower)
		return;
	if (py + ny > s->ry_lower)
		ny = s->ry_lower - py;
	log_debug("deleting lines in region: %u,%u (%u,%u)", py, ny,
	    s->ry_upper, s->ry_lower);

	/*
	 * Delete range of ny lines at py:
	 * 	- Free ny lines at py.
	 *	- Move from py + ny to end of region to py.
	 *	- Free and recreate last ny lines.
	 *
	 * Example: delete lines 13,14.
	 *	ryu = 11, ryl = 16, py = 13, ny = 2
	 *	screen_free_lines(s, 13, 2);	- delete lines 13,14
	 *	screen_move_lines(s, 15, 16, 2);- move 15,16 to 13
	 *	screen_make_lines(s, 15, 16);	- make lines 15,16
	 */

	screen_free_lines(s, py, ny);

	if (py != s->ry_lower)
		screen_move_lines(s, py, py + ny, (s->ry_lower + 1) - py - ny);

	screen_make_lines(s, (s->ry_lower + 1) - ny, ny);
	screen_fill_lines(s, (s->ry_lower + 1) - ny,
	    ny, SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}

/* Insert characters. */
void
screen_insert_characters(struct screen *s, u_int px, u_int py, u_int nx)
{
	u_int	lx, rx;

	if (px > screen_last_x(s) || py > screen_last_y(s))
		return;

	lx = px;
	rx = screen_offset_x(px, nx);
	if (rx > screen_last_x(s))
		rx = screen_last_x(s);

	/*
	 * Inserting a range from lx to rx, inclusive.
	 *
	 * - If rx is not the last x, move from lx to rx + 1.
	 * - Clear the range from lx to rx.
	 */
	if (rx != screen_last_x(s)) {
		nx = screen_range_x(rx + 1, screen_last_x(s));
		memmove(&s->grid_data[py][rx + 1], &s->grid_data[py][lx], nx);
		memmove(&s->grid_attr[py][rx + 1], &s->grid_attr[py][lx], nx);
		memmove(&s->grid_colr[py][rx + 1], &s->grid_colr[py][lx], nx);
	}
	memset(&s->grid_data[py][lx], SCREEN_DEFDATA, screen_range_x(lx, rx));
	memset(&s->grid_attr[py][lx], SCREEN_DEFATTR, screen_range_x(lx, rx));
	memset(&s->grid_colr[py][lx], SCREEN_DEFCOLR, screen_range_x(lx, rx));
}

/* Delete characters. */
void
screen_delete_characters(struct screen *s, u_int px, u_int py, u_int nx)
{
	u_int	lx, rx;

	if (px > screen_last_x(s) || py > screen_last_y(s))
		return;

	lx = px;
	rx = screen_offset_x(px, nx);
	if (rx > screen_last_x(s))
		rx = screen_last_x(s);

	/*
	 * Deleting the range from lx to rx, inclusive.
	 *
	 * - If rx is not the last x, move the range from rx + 1 to lx.
	 * - Clear the range from the last x - (rx - lx)  to the last x.
	 */

	if (rx != screen_last_x(s)) {
		nx = screen_range_x(rx + 1, screen_last_x(s));
		memmove(&s->grid_data[py][lx], &s->grid_data[py][rx + 1], nx);
		memmove(&s->grid_attr[py][lx], &s->grid_attr[py][rx + 1], nx);
		memmove(&s->grid_colr[py][lx], &s->grid_colr[py][rx + 1], nx);
	}

	/* If lx == rx, then nx = 1. */ 
	nx = screen_range_x(lx, rx);
	memset(&s->grid_data[py][s->sx - nx], SCREEN_DEFDATA, nx);
	memset(&s->grid_attr[py][s->sx - nx], SCREEN_DEFATTR, nx);
	memset(&s->grid_colr[py][s->sx - nx], SCREEN_DEFCOLR, nx);
}
