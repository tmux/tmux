/* $Id: screen-display.c,v 1.11 2007-12-06 10:16:36 nicm Exp $ */

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

/* Set a cell. */
void
screen_display_set_cell(
    struct screen *s, u_int px, u_int py, u_char data, u_char attr, u_char colr)
{
	screen_set_cell(s, screen_x(s, px), screen_y(s, py), data, attr, colr);
}

/* Create a region of lines. */
void
screen_display_make_lines(struct screen *s, u_int py, u_int ny)
{
	if (ny == 0 || !screen_in_y(s, py) || !screen_in_y(s, py + ny - 1))
		return;
	screen_make_lines(s, screen_y(s, py), ny);
}

/* Free a region of lines. */
void
screen_display_free_lines(struct screen *s, u_int py, u_int ny)
{
	if (ny == 0 || !screen_in_y(s, py) || !screen_in_y(s, py + ny - 1))
		return;
	screen_free_lines(s, screen_y(s, py), ny);
}

/* Move a set of lines. */
void
screen_display_move_lines(struct screen *s, u_int dy, u_int py, u_int ny)
{
	if (ny == 0 || !screen_in_y(s, py) || !screen_in_y(s, py + ny - 1))
		return;
	if (!screen_in_y(s, dy) || !screen_in_y(s, dy + ny - 1) || dy == py)
		return;
	screen_move_lines(s, screen_y(s, dy), screen_y(s, py), ny);
}

/* Fill a set of cells. */
void
screen_display_fill_area(struct screen *s, u_int px, u_int py,
    u_int nx, u_int ny, u_char data, u_char attr, u_char colr)
{
	if (nx == 0 || ny == 0)
		return;
	if (!screen_in_x(s, px) || !screen_in_y(s, py))
		return;
	if (!screen_in_x(s, px + nx - 1) || !screen_in_y(s, py + ny - 1))
		return;
	screen_fill_area(
	    s, screen_x(s, px), screen_y(s, py), nx, ny, data, attr, colr);
}

/* Scroll region up. */
void
screen_display_scroll_region_up(struct screen *s)
{
	u_int	ny, sy;

	/*
	 * If the region is the entire screen, this is easy-peasy. Allocate
	 * a new line and adjust the history size.
	 * XXX should this be done somewhere else?
	 */
	if (s->rupper == 0 && s->rlower == screen_last_y(s)) {
		if (s->hsize == s->hlimit) {
			/* If the limit is hit, free 10% and shift up. */
			ny = s->hlimit / 10;
			if (ny < 1)
				ny = 1;

			sy = screen_size_y(s) + s->hsize;
			screen_free_lines(s, 0, ny);
			screen_move_lines(s, 0, ny, sy - ny);

			s->hsize -= ny;
		}
		s->hsize++;

		sy = screen_size_y(s) + s->hsize;
		s->grid_data = xrealloc(s->grid_data, sy, sizeof *s->grid_data);
		s->grid_attr = xrealloc(s->grid_attr, sy, sizeof *s->grid_attr);
		s->grid_colr = xrealloc(s->grid_colr, sy, sizeof *s->grid_colr);
		s->grid_size = xrealloc(s->grid_size, sy, sizeof *s->grid_size);
		screen_display_make_lines(s, screen_last_y(s), 1);
		return;
	}

	/*
	 * Scroll scrolling region up:
	 * 	- delete rupper
	 *	- move rupper + 1 to rlower to rupper
	 *	- make new line at rlower
	 *
	 * Example: region is 12 to 24.
	 *	rlower = 24, rupper = 12
	 *	screen_free_lines(s, 12, 1);
	 *	screen_move_lines(s, 12, 13, 12);
	 *	screen_make_lines(s, 24, 1);
	 */

	screen_display_free_lines(s, s->rupper, 1);

	if (s->rupper != s->rlower) {
		screen_display_move_lines(s,
		    s->rupper, s->rupper + 1, s->rlower - s->rupper);
	}

	screen_display_make_lines(s, s->rlower, 1);
}

/* Scroll region down. */
void
screen_display_scroll_region_down(struct screen *s)
{
	/*
	 * Scroll scrolling region down:
	 * 	- delete rlower
	 *	- move rupper to rlower - 1 to rupper + 1
	 *	- make new line at rupper
	 *
	 * Example: region is 12 to 24.
	 *	rlower = 24, rupper = 12
	 *	screen_free_lines(s, 24, 1);
	 *	screen_move_lines(s, 13, 12, 12);
	 *	screen_make_lines(s, 12, 1);
	 */

	screen_display_free_lines(s, s->rlower, 1);

	if (s->rupper != s->rlower) {
		screen_display_move_lines(s,
		    s->rupper + 1, s->rupper, s->rlower - s->rupper);
	}

	screen_display_make_lines(s, s->rupper, 1);
}

/* Insert lines. */
void
screen_display_insert_lines(struct screen *s, u_int py, u_int ny)
{
	if (!screen_in_y(s, py))
		return;
	if (ny == 0)
		return;

	if (py + ny > screen_last_y(s))
		ny = screen_size_y(s) - py;
	if (ny == 0)
		return;

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

	screen_display_free_lines(s, screen_size_y(s) - ny, ny);

	if (py + ny != screen_size_y(s)) {
		screen_display_move_lines(
		    s, py + ny, py, screen_size_y(s) - py - ny);
	}

	screen_display_make_lines(s, py, ny);
}

/* Insert lines in region. */
void
screen_display_insert_lines_region(struct screen *s, u_int py, u_int ny)
{
	if (!screen_in_region(s, py))
		return;
	if (ny == 0)
		return;

	if (py + ny > s->rlower)
		ny = (s->rlower + 1) - py;
	if (ny == 0)
		return;

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

	screen_display_free_lines(s, (s->rlower + 1) - ny, ny);

	if (py + ny != s->rlower + 1) {
		screen_display_move_lines(
		    s, py + ny, py, (s->rlower + 1) - py - ny);
	}

	screen_display_make_lines(s, py, ny);
}

/* Delete lines. */
void
screen_display_delete_lines(struct screen *s, u_int py, u_int ny)
{
	if (!screen_in_y(s, py))
		return;
	if (ny == 0)
		return;

	if (py + ny > screen_last_y(s))
		ny = screen_size_y(s) - py;
	if (ny == 0)
		return;

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

	screen_display_free_lines(s, py, ny);

	if (py + ny != screen_size_y(s)) {
		screen_display_move_lines(
		    s, py, py + ny, screen_size_y(s) - py - ny);
	}

	screen_display_make_lines(s, screen_size_y(s) - ny, ny);
}

/* Delete lines inside scroll region. */
void
screen_display_delete_lines_region(struct screen *s, u_int py, u_int ny)
{
	if (!screen_in_region(s, py))
		return;
	if (ny == 0)
		return;

	if (py + ny > s->rlower)
		ny = (s->rlower + 1) - py;
	if (ny == 0)
		return;

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

	screen_display_free_lines(s, py, ny);

	if (py + ny != s->rlower + 1) {
		screen_display_move_lines(
		    s, py, py + ny, (s->rlower + 1) - py - ny);
	}

	screen_display_make_lines(s, (s->rlower + 1) - ny, ny);
}

/* Insert characters. */
void
screen_display_insert_characters(struct screen *s, u_int px, u_int py, u_int nx)
{
	u_int	mx;

	if (!screen_in_x(s, px) || !screen_in_y(s, py))
		return;

	if (px + nx > screen_last_x(s))
		nx = screen_last_x(s) - px;

	py = screen_y(s, py);

	/* XXX Cheat and make the line a full line. */
	if (s->grid_size[py] < screen_size_x(s))
		screen_expand_line(s, py, screen_size_x(s));

	/*
	 * Inserting a range of nx at px.
	 *
	 * - Move sx - (px + nx) from px to px + nx.
	 * - Clear the range at px to px + nx.
	 */

	if (px + nx != screen_last_x(s)) {
		mx = screen_last_x(s) - (px + nx);
		memmove(&s->grid_data[py][px + nx], &s->grid_data[py][px], mx);
		memmove(&s->grid_attr[py][px + nx], &s->grid_attr[py][px], mx);
		memmove(&s->grid_colr[py][px + nx], &s->grid_colr[py][px], mx);
	}

	memset(&s->grid_data[py][px], SCREEN_DEFDATA, nx);
	memset(&s->grid_attr[py][px], SCREEN_DEFATTR, nx);
	memset(&s->grid_colr[py][px], SCREEN_DEFCOLR, nx);
}

/* Delete characters. */
void
screen_display_delete_characters(struct screen *s, u_int px, u_int py, u_int nx)
{
	u_int	mx;

	if (!screen_in_x(s, px) || !screen_in_y(s, py))
		return;

	if (px + nx > screen_last_x(s))
		nx = screen_last_x(s) - px;

	py = screen_y(s, py);

	/* XXX Cheat and make the line a full line. */
	if (s->grid_size[py] < screen_size_x(s))
		screen_expand_line(s, py, screen_size_x(s));

	/*
	 * Deleting the range from px to px + nx.
	 *
	 * - Move sx - (px + nx) from px + nx to px.
	 * - Clear the range from sx - nx to sx.
	 */

	if (px + nx != screen_last_x(s)) {
		mx = screen_last_x(s) - (px + nx);
		memmove(&s->grid_data[py][px], &s->grid_data[py][px + nx], mx);
		memmove(&s->grid_attr[py][px], &s->grid_attr[py][px + nx], mx);
		memmove(&s->grid_colr[py][px], &s->grid_colr[py][px + nx], mx);
	}

	memset(&s->grid_data[py][screen_size_x(s) - nx], SCREEN_DEFDATA, nx);
	memset(&s->grid_attr[py][screen_size_x(s) - nx], SCREEN_DEFATTR, nx);
	memset(&s->grid_colr[py][screen_size_x(s) - nx], SCREEN_DEFCOLR, nx);
}

/* Fill cells from another screen, with an offset. */
void
screen_display_copy_area(struct screen *dst, struct screen *src,
    u_int px, u_int py, u_int nx, u_int ny, u_int ox, u_int oy)
{
	u_int	i, j;
	u_char	data, attr, colr;

	if (nx == 0 || ny == 0)
		return;
	if (!screen_in_x(dst, px) || !screen_in_y(dst, py))
		return;
	if (!screen_in_x(dst, px + nx - 1) || !screen_in_y(dst, py + ny - 1))
		return;

	for (i = py; i < py + ny; i++) {
		for (j = px; j < px + nx; j++) {
			screen_get_cell(src,
			    screen_x(src, j) + ox, screen_y(src, i) - oy,
			    &data, &attr, &colr);
			screen_display_set_cell(dst, j, i, data, attr, colr);
		}
	}
}
