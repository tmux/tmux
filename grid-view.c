/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
 * Grid view functions. These work using coordinates relative to the visible
 * screen area.
 */

#define grid_view_x(gd, x) (x)
#define grid_view_y(gd, y) ((gd)->hsize + (y))

/* Get cell for reading. */
const struct grid_cell *
grid_view_peek_cell(struct grid *gd, u_int px, u_int py)
{
	return (grid_peek_cell(gd, grid_view_x(gd, px), grid_view_y(gd, py)));
}

/* Get cell for writing. */
struct grid_cell *
grid_view_get_cell(struct grid *gd, u_int px, u_int py)
{
	return (grid_get_cell(gd, grid_view_x(gd, px), grid_view_y(gd, py)));
}

/* Set cell. */
void
grid_view_set_cell(
    struct grid *gd, u_int px, u_int py, const struct grid_cell *gc)
{
	grid_set_cell(gd, grid_view_x(gd, px), grid_view_y(gd, py), gc);
}

/* Clear into history. */
void
grid_view_clear_history(struct grid *gd)
{
	struct grid_line	*gl;
	u_int			 yy, last;

	/* Find the last used line. */
	last = 0;
	for (yy = 0; yy < gd->sy; yy++) {
		gl = &gd->linedata[grid_view_y(gd, yy)];
		if (gl->cellsize != 0)
			last = yy + 1;
	}
	if (last == 0)
		return;

	/* Scroll the lines into the history. */
	for (yy = 0; yy < last; yy++) {
		grid_collect_history(gd);
		grid_scroll_history(gd);
	}
}

/* Clear area. */
void
grid_view_clear(struct grid *gd, u_int px, u_int py, u_int nx, u_int ny)
{
	px = grid_view_x(gd, px);
	py = grid_view_y(gd, py);

	grid_clear(gd, px, py, nx, ny);
}

/* Scroll region up. */
void
grid_view_scroll_region_up(struct grid *gd, u_int rupper, u_int rlower)
{
	if (gd->flags & GRID_HISTORY) {
		grid_collect_history(gd);
		if (rupper == 0 && rlower == gd->sy - 1)
			grid_scroll_history(gd);
		else {
			rupper = grid_view_y(gd, rupper);
			rlower = grid_view_y(gd, rlower);
			grid_scroll_history_region(gd, rupper, rlower);
		}
	} else {
		rupper = grid_view_y(gd, rupper);
		rlower = grid_view_y(gd, rlower);
		grid_move_lines(gd, rupper, rupper + 1, rlower - rupper);
	}
}

/* Scroll region down. */
void
grid_view_scroll_region_down(struct grid *gd, u_int rupper, u_int rlower)
{
	rupper = grid_view_y(gd, rupper);
	rlower = grid_view_y(gd, rlower);

	grid_move_lines(gd, rupper + 1, rupper, rlower - rupper);
}

/* Insert lines. */
void
grid_view_insert_lines(struct grid *gd, u_int py, u_int ny)
{
	u_int	sy;

	py = grid_view_y(gd, py);

	sy = grid_view_y(gd, gd->sy);

	grid_move_lines(gd, py + ny, py, sy - py - ny);
}

/* Insert lines in region. */
void
grid_view_insert_lines_region(struct grid *gd, u_int rlower, u_int py,
    u_int ny)
{
	u_int	ny2;

	rlower = grid_view_y(gd, rlower);

	py = grid_view_y(gd, py);

	ny2 = rlower + 1 - py - ny;
	grid_move_lines(gd, rlower + 1 - ny2, py, ny2);
	grid_clear(gd, 0, py + ny2, gd->sx, ny - ny2);
}

/* Delete lines. */
void
grid_view_delete_lines(struct grid *gd, u_int py, u_int ny)
{
	u_int	sy;

	py = grid_view_y(gd, py);

	sy = grid_view_y(gd, gd->sy);

	grid_move_lines(gd, py, py + ny, sy - py - ny);
	grid_clear(gd, 0, sy - ny, gd->sx, py + ny - (sy - ny));
}

/* Delete lines inside scroll region. */
void
grid_view_delete_lines_region(struct grid *gd, u_int rlower, u_int py,
    u_int ny)
{
	u_int	ny2;

	rlower = grid_view_y(gd, rlower);

	py = grid_view_y(gd, py);

	ny2 = rlower + 1 - py - ny;
	grid_move_lines(gd, py, py + ny, ny2);
	grid_clear(gd, 0, py + ny2, gd->sx, ny - ny2);
}

/* Insert characters. */
void
grid_view_insert_cells(struct grid *gd, u_int px, u_int py, u_int nx)
{
	u_int	sx;

	px = grid_view_x(gd, px);
	py = grid_view_y(gd, py);

	sx = grid_view_x(gd, gd->sx);

	if (px == sx - 1)
		grid_clear(gd, px, py, 1, 1);
	else
		grid_move_cells(gd, px + nx, px, py, sx - px - nx);
}

/* Delete characters. */
void
grid_view_delete_cells(struct grid *gd, u_int px, u_int py, u_int nx)
{
	u_int	sx;

	px = grid_view_x(gd, px);
	py = grid_view_y(gd, py);

	sx = grid_view_x(gd, gd->sx);

	grid_move_cells(gd, px, px + nx, py, sx - px - nx);
	grid_clear(gd, sx - nx, py, px + nx - (sx - nx), 1);
}

/* Convert cells into a string. */
char *
grid_view_string_cells(struct grid *gd, u_int px, u_int py, u_int nx)
{
	px = grid_view_x(gd, px);
	py = grid_view_y(gd, py);

	return (grid_string_cells(gd, px, py, nx, NULL, 0, 0, 0));
}
