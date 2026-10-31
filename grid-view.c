/* $OpenBSD: grid-view.c,v 1.40 2026/10/20 12:47:57 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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

/* Get cell. */
void
grid_view_get_cell(struct grid *gd, u_int px, u_int py, struct grid_cell *gc)
{
	grid_get_cell(gd, grid_view_x(gd, px), grid_view_y(gd, py), gc);
}

/* Set cell. */
void
grid_view_set_cell(struct grid *gd, u_int px, u_int py,
    const struct grid_cell *gc)
{
	grid_set_cell(gd, grid_view_x(gd, px), grid_view_y(gd, py), gc);
}

/* Set padding. */
void
grid_view_set_padding(struct grid *gd, u_int px, u_int py, int bg)
{
	grid_set_padding(gd, grid_view_x(gd, px), grid_view_y(gd, py), bg);
}

/* Set cells. */
void
grid_view_set_cells(struct grid *gd, u_int px, u_int py,
    const struct grid_cell *gc, const char *s, size_t slen)
{
	grid_set_cells(gd, grid_view_x(gd, px), grid_view_y(gd, py), gc, s,
	    slen);
}

/* Clear into history. */
void
grid_view_clear_history(struct grid *gd, u_int bg)
{
	struct grid_line	*gl;
	u_int			 yy, last;

	/* Find the last used line. */
	last = 0;
	for (yy = 0; yy < gd->sy; yy++) {
		gl = grid_get_line(gd, grid_view_y(gd, yy));
		if (gl->cellused != 0)
			last = yy + 1;
#ifdef ENABLE_IMAGES
		else if (image_grid_line_has_images(gl))
			last = yy + 1;
#endif
	}
	if (last == 0) {
		grid_view_clear(gd, 0, 0, gd->sx, gd->sy, bg);
		return;
	}

	/* Scroll the lines into the history. */
	for (yy = 0; yy < last; yy++) {
		grid_collect_history(gd, 0);
		grid_scroll_history(gd, bg);
		if (gd->hlimit_bytes != 0)
			grid_collect_history_bytes(gd);
	}
	if (last < gd->sy)
		grid_view_clear(gd, 0, 0, gd->sx, gd->sy - last, bg);
	gd->hscrolled = 0;
}

/* Clear area. */
void
grid_view_clear(struct grid *gd, u_int px, u_int py, u_int nx, u_int ny,
    u_int bg)
{
	px = grid_view_x(gd, px);
	py = grid_view_y(gd, py);

	grid_clear(gd, px, py, nx, ny, bg);
}

/* Scroll region up. */
void
grid_view_scroll_region_up(struct grid *gd, u_int rupper, u_int rlower,
    u_int bg)
{
#ifdef ENABLE_IMAGES
	struct image_saved	*saved;
#endif
	u_int			 n = rlower - rupper;

	if (gd->flags & GRID_HISTORY) {
		grid_collect_history(gd, 0);
		if (rupper == 0 && rlower == gd->sy - 1)
			grid_scroll_history(gd, bg);
		else {
			rupper = grid_view_y(gd, rupper);
			rlower = grid_view_y(gd, rlower);
#ifdef ENABLE_IMAGES
			saved = image_grid_save_lines(gd, rupper, rupper + 1, n, 1);
#endif
			grid_scroll_history_region(gd, rupper, rlower, bg);
#ifdef ENABLE_IMAGES
			image_grid_restore_lines(gd, saved);
#endif
		}
		if (gd->hlimit_bytes != 0)
			grid_collect_history_bytes(gd);
	} else {
		rupper = grid_view_y(gd, rupper);
		rlower = grid_view_y(gd, rlower);
#ifdef ENABLE_IMAGES
		saved = image_grid_save_lines(gd, rupper, rupper + 1, n, 1);
#endif
		grid_move_lines(gd, rupper, rupper + 1, n, bg);
#ifdef ENABLE_IMAGES
		image_grid_restore_lines(gd, saved);
#endif
	}
}

/* Scroll region down. */
void
grid_view_scroll_region_down(struct grid *gd, u_int rupper, u_int rlower,
    u_int bg)
{
#ifdef ENABLE_IMAGES
	struct image_saved	*saved;
#endif

	rupper = grid_view_y(gd, rupper);
	rlower = grid_view_y(gd, rlower);

#ifdef ENABLE_IMAGES
	saved = image_grid_save_lines(gd, rupper + 1, rupper, rlower - rupper, 1);
#endif
	grid_move_lines(gd, rupper + 1, rupper, rlower - rupper, bg);
#ifdef ENABLE_IMAGES
	image_grid_restore_lines(gd, saved);
#endif
}

/* Insert lines. */
void
grid_view_insert_lines(struct grid *gd, u_int py, u_int ny, u_int bg)
{
#ifdef ENABLE_IMAGES
	struct image_saved	*saved;
#endif
	u_int			 sy;

	py = grid_view_y(gd, py);

	sy = grid_view_y(gd, gd->sy);

#ifdef ENABLE_IMAGES
	saved = image_grid_save_lines(gd, py + ny, py, sy - py - ny, 0);
#endif
	grid_move_lines(gd, py + ny, py, sy - py - ny, bg);
#ifdef ENABLE_IMAGES
	image_grid_restore_lines(gd, saved);
#endif
}

/* Insert lines in region. */
void
grid_view_insert_lines_region(struct grid *gd, u_int rlower, u_int py,
    u_int ny, u_int bg)
{
#ifdef ENABLE_IMAGES
	struct image_saved	*saved;
#endif
	u_int			 ny2;

	rlower = grid_view_y(gd, rlower);

	py = grid_view_y(gd, py);

	ny2 = rlower + 1 - py - ny;
#ifdef ENABLE_IMAGES
	saved = image_grid_save_lines(gd, rlower + 1 - ny2, py, ny2, 0);
#endif
	grid_move_lines(gd, rlower + 1 - ny2, py, ny2, bg);
#ifdef ENABLE_IMAGES
	image_grid_restore_lines(gd, saved);
#endif
	grid_clear(gd, 0, py + ny2, gd->sx, ny - ny2, bg);
}

/* Delete lines. */
void
grid_view_delete_lines(struct grid *gd, u_int py, u_int ny, u_int bg)
{
#ifdef ENABLE_IMAGES
	struct image_saved	*saved;
#endif
	u_int			 sy;

	py = grid_view_y(gd, py);

	sy = grid_view_y(gd, gd->sy);

#ifdef ENABLE_IMAGES
	saved = image_grid_save_lines(gd, py, py + ny, sy - py - ny, 0);
#endif
	grid_move_lines(gd, py, py + ny, sy - py - ny, bg);
#ifdef ENABLE_IMAGES
	image_grid_restore_lines(gd, saved);
#endif
	grid_clear(gd, 0, sy - ny, gd->sx, ny, bg);
}

/* Delete lines inside scroll region. */
void
grid_view_delete_lines_region(struct grid *gd, u_int rlower, u_int py,
    u_int ny, u_int bg)
{
#ifdef ENABLE_IMAGES
	struct image_saved	*saved;
#endif
	u_int			 ny2;

	rlower = grid_view_y(gd, rlower);

	py = grid_view_y(gd, py);

	ny2 = rlower + 1 - py - ny;
#ifdef ENABLE_IMAGES
	saved = image_grid_save_lines(gd, py, py + ny, ny2, 0);
#endif
	grid_move_lines(gd, py, py + ny, ny2, bg);
#ifdef ENABLE_IMAGES
	image_grid_restore_lines(gd, saved);
#endif
	grid_clear(gd, 0, py + ny2, gd->sx, ny - ny2, bg);
}

/* Insert characters. */
void
grid_view_insert_cells(struct grid *gd, u_int px, u_int py, u_int nx, u_int bg)
{
	u_int	sx;

	px = grid_view_x(gd, px);
	py = grid_view_y(gd, py);

	sx = grid_view_x(gd, gd->sx);

	grid_move_cells(gd, px + nx, px, py, sx - px - nx, bg);
	grid_clear(gd, px, py, nx, 1, bg);
}

/* Delete characters. */
void
grid_view_delete_cells(struct grid *gd, u_int px, u_int py, u_int nx, u_int bg)
{
	u_int	sx;

	px = grid_view_x(gd, px);
	py = grid_view_y(gd, py);

	sx = grid_view_x(gd, gd->sx);

	grid_move_cells(gd, px, px + nx, py, sx - px - nx, bg);
	grid_clear(gd, sx - nx, py, nx, 1, bg);
}

/* Convert cells into a string. */
char *
grid_view_string_cells(struct grid *gd, u_int px, u_int py, u_int nx)
{
	px = grid_view_x(gd, px);
	py = grid_view_y(gd, py);

	return (grid_string_cells(gd, px, py, nx, NULL, 0, NULL));
}
