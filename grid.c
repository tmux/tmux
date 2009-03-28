/* $Id: grid.c,v 1.13 2009-03-28 16:57:03 nicm Exp $ */

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
 * Grid data. This is the basic data structure that represents what is shown on
 * screen.
 *
 * A grid is a grid of cells (struct grid_cell). Lines are not allocated until
 * cells in that line are written to. The grid is split into history and
 * viewable data with the history starting at row (line) 0 and extending to
 * (hsize - 1); from hsize to hsize + (sy - 1) is the viewable data. All
 * functions in this file work on absolute coordinates, grid-view.c has
 * functions which work on the screen data.
 */

/* Default grid cell data. */
const struct grid_cell grid_default_cell = { 0, 0, 8, 8 };

#define grid_put_cell(gd, px, py, gc) do {			\
	memcpy(&gd->data[py][px], gc, sizeof gd->data[py][px]);	\
} while (0)
#define grid_put_text(gd, px, py, xtext) do {			\
	gd->text[py][px] = xtext;				\
} while (0)

int	grid_check_x(struct grid *, u_int);
int	grid_check_y(struct grid *, u_int);

#ifdef DEBUG
int
grid_check_x(struct grid *gd, u_int px)
{
	if ((px) >= (gd)->sx)
		log_fatalx("x out of range: %u", px);
	return (0);
}

int
grid_check_y(struct grid *gd, u_int py)
{
	if ((py) >= (gd)->hsize + (gd)->sy)
		log_fatalx("y out of range: %u", py);
	return (0);
}
#else
int
grid_check_x(struct grid *gd, u_int px)
{
	if ((px) >= (gd)->sx) {
		log_debug("x out of range: %u", px);
		return (-1);
	}
	return (0);
}

int
grid_check_y(struct grid *gd, u_int py)
{
	if ((py) >= (gd)->hsize + (gd)->sy) {
		log_debug("y out of range: %u", py);
		return (-1);
	}
	return (0);
}
#endif

/* Create a new grid. */
struct grid *
grid_create(u_int sx, u_int sy, u_int hlimit)
{
	struct grid	*gd;

	gd = xmalloc(sizeof *gd);
	gd->sx = sx;
	gd->sy = sy;

	gd->hsize = 0;
	gd->hlimit = hlimit;

	gd->size = xcalloc(gd->sy, sizeof *gd->size);
	gd->data = xcalloc(gd->sy, sizeof *gd->data);
	gd->text = xcalloc(gd->sy, sizeof *gd->text);

	return (gd);
}

/* Destroy grid. */
void
grid_destroy(struct grid *gd)
{
	u_int	yy;

	for (yy = 0; yy < gd->hsize + gd->sy; yy++) {
		if (gd->text[yy] != NULL)
			xfree(gd->text[yy]);
		if (gd->data[yy] != NULL)
			xfree(gd->data[yy]);
	}

	if (gd->text != NULL)
		xfree(gd->text);
	if (gd->data != NULL)
		xfree(gd->data);
	if (gd->size != NULL)
		xfree(gd->size);
	xfree(gd);
}

/* Compare grids. */
int
grid_compare(struct grid *ga, struct grid *gb)
{
	struct grid_cell	*gca, *gcb;
	u_int			 xx, yy;

	if (ga->sx != gb->sx || ga->sy != ga->sy)
		return (1);

	for (yy = 0; yy < ga->sy; yy++) {
		if (ga->size[yy] != gb->size[yy])
			return (1);
		for (xx = 0; xx < ga->sx; xx++) {
			gca = &ga->data[yy][xx];
			gcb = &gb->data[yy][xx];
			if (memcmp(gca, gcb, sizeof (struct grid_cell)) != 0)
				return (1);
		}
	}

	return (0);
}

/* Scroll a line into the history. */
void
grid_scroll_line(struct grid *gd)
{
	u_int	yy;

 	GRID_DEBUG(gd, "");

	if (gd->hsize >= gd->hlimit - 1) {
		/* If the limit is hit, free the bottom 10% and shift up. */
		yy = gd->hlimit / 10;
		if (yy < 1)
			yy = 1;

		grid_move_lines(gd, 0, yy, gd->hsize + gd->sy - yy);
		gd->hsize -= yy;
	}

	yy = gd->hsize + gd->sy;
	gd->size = xrealloc(gd->size, yy + 1, sizeof *gd->size);
	gd->data = xrealloc(gd->data, yy + 1, sizeof *gd->data);
	gd->text = xrealloc(gd->text, yy + 1, sizeof *gd->text);

	gd->data[yy] = NULL;
	gd->text[yy] = NULL;
	gd->size[yy] = 0;

	gd->hsize++;
}

/* Reduce line to fit to cell. */
void
grid_reduce_line(struct grid *gd, u_int py, u_int sx)
{
	if (sx >= gd->size[py])
		return;

	gd->data[py] = xrealloc(gd->data[py], sx, sizeof **gd->data);
	gd->text[py] = xrealloc(gd->text[py], sx, sizeof **gd->text);
	gd->size[py] = sx;
}

/* Expand line to fit to cell. */
void
grid_expand_line(struct grid *gd, u_int py, u_int sx)
{
	u_int	xx;

	if (sx <= gd->size[py])
		return;

	gd->data[py] = xrealloc(gd->data[py], sx, sizeof **gd->data);
	gd->text[py] = xrealloc(gd->text[py], sx, sizeof **gd->text);
	for (xx = gd->size[py]; xx < sx; xx++) {
		grid_put_cell(gd, xx, py, &grid_default_cell);
		grid_put_text(gd, xx, py, ' ');
	}
	gd->size[py] = sx;
}

/* Get cell for reading. */
const struct grid_cell *
grid_peek_cell(struct grid *gd, u_int px, u_int py)
{
	if (grid_check_x(gd, px) != 0)
		return (&grid_default_cell);
	if (grid_check_y(gd, py) != 0)
		return (&grid_default_cell);

	if (px >= gd->size[py])
		return (&grid_default_cell);
	return (&gd->data[py][px]);
}

/* Get text for reading. */
uint64_t
grid_peek_text(struct grid *gd, u_int px, u_int py)
{
	if (grid_check_x(gd, px) != 0)
		return (' ');
	if (grid_check_y(gd, py) != 0)
		return (' ');

	if (px >= gd->size[py])
		return (' ');
	return (gd->text[py][px]);
}

/* Get cell at relative position (for writing). */
struct grid_cell *
grid_get_cell(struct grid *gd, u_int px, u_int py)
{
	if (grid_check_x(gd, px) != 0)
		return (NULL);
	if (grid_check_y(gd, py) != 0)
		return (NULL);

	grid_expand_line(gd, py, px + 1);
	return (&gd->data[py][px]);
}

/* Set cell at relative position. */
void
grid_set_cell(
    struct grid *gd, u_int px, u_int py, const struct grid_cell *gc)
{
	if (grid_check_x(gd, px) != 0)
		return;
	if (grid_check_y(gd, py) != 0)
		return;

	grid_expand_line(gd, py, px + 1);
	grid_put_cell(gd, px, py, gc);
}

/* Set text at relative position. */
void
grid_set_text(struct grid *gd, u_int px, u_int py, uint64_t text)
{
	if (grid_check_x(gd, px) != 0)
		return;
	if (grid_check_y(gd, py) != 0)
		return;

	grid_expand_line(gd, py, px + 1);
	grid_put_text(gd, px, py, text);
}

/*
 * Clear area. Note this is different from a fill as it just omits unallocated
 * cells.
 */
void
grid_clear(struct grid *gd, u_int px, u_int py, u_int nx, u_int ny)
{
	u_int	xx, yy;

 	GRID_DEBUG(gd, "px=%u, py=%u, nx=%u, ny=%u", px, py, nx, ny);

	if (nx == 0 || ny == 0)
		return;

	if (px == 0 && nx == gd->sx) {
		grid_clear_lines(gd, py, ny);
		return;
	}

	if (grid_check_x(gd, px) != 0)
		return;
	if (grid_check_x(gd, px + nx - 1) != 0)
		return;
	if (grid_check_y(gd, py) != 0)
		return;
	if (grid_check_y(gd, py + ny - 1) != 0)
		return;

	for (yy = py; yy < py + ny; yy++) {
		for (xx = px; xx < px + nx; xx++) {
			if (xx >= gd->size[yy])
				break;
			grid_put_cell(gd, xx, yy, &grid_default_cell);
			grid_put_text(gd, xx, yy, ' ');
		}
	}
}

/* Clear lines. This just frees and truncates the lines. */
void
grid_clear_lines(struct grid *gd, u_int py, u_int ny)
{
	u_int	yy;

 	GRID_DEBUG(gd, "py=%u, ny=%u", py, ny);

	if (ny == 0)
		return;

	if (grid_check_y(gd, py) != 0)
		return;
	if (grid_check_y(gd, py + ny - 1) != 0)
		return;

	for (yy = py; yy < py + ny; yy++) {
		if (gd->data[yy] != NULL || gd->text[yy] != NULL) {
			xfree(gd->data[yy]);
			xfree(gd->text[yy]);
			gd->data[yy] = NULL;	
			gd->text[yy] = NULL;
			gd->size[yy] = 0;
		}
	}
}

/* Move a group of lines. */
void
grid_move_lines(struct grid *gd, u_int dy, u_int py, u_int ny)
{
	u_int	yy;

 	GRID_DEBUG(gd, "dy=%u, py=%u, ny=%u", dy, py, ny);

	if (ny == 0 || py == dy)
		return;

	if (grid_check_y(gd, py) != 0)
		return;
	if (grid_check_y(gd, py + ny - 1) != 0)
		return;
	if (grid_check_y(gd, dy) != 0)
		return;
	if (grid_check_y(gd, dy + ny - 1) != 0)
		return;

	/* Free any lines which are being replaced. */
	for (yy = dy; yy < dy + ny; yy++) {
		if (yy >= py && yy < py + ny)
			continue;
		grid_clear_lines(gd, yy, 1);
	}

	memmove(&gd->data[dy], &gd->data[py], ny * (sizeof *gd->data));	
	memmove(&gd->text[dy], &gd->text[py], ny * (sizeof *gd->text));
	memmove(&gd->size[dy], &gd->size[py], ny * (sizeof *gd->size));

	/* Wipe any lines that have been moved (without freeing them). */
	for (yy = py; yy < py + ny; yy++) {
		if (yy >= dy && yy < dy + ny)
			continue;
		gd->data[yy] = NULL;
		gd->text[yy] = NULL;
		gd->size[yy] = 0;
	}
}

/* Clear a group of cells. */
void
grid_clear_cells(struct grid *gd, u_int px, u_int py, u_int nx)
{
	u_int	xx;

 	GRID_DEBUG(gd, "px=%u, py=%u, nx=%u", px, py, nx);

	if (nx == 0)
		return;

	if (grid_check_x(gd, px) != 0)
		return;
	if (grid_check_x(gd, px + nx - 1) != 0)
		return;
	if (grid_check_y(gd, py) != 0)
		return;

	for (xx = px; xx < px + nx; xx++) {
		if (xx >= gd->size[py])
			break;
		grid_put_cell(gd, xx, py, &grid_default_cell);
		grid_put_text(gd, xx, py, ' ');
	}
}

/* Move a group of cells. */
void
grid_move_cells(struct grid *gd, u_int dx, u_int px, u_int py, u_int nx)
{
	u_int	xx;

 	GRID_DEBUG(gd, "dx=%u, px=%u, py=%u, nx=%u", dx, px, py, nx);

	if (nx == 0 || px == dx)
		return;

	if (grid_check_x(gd, px) != 0)
		return;
	if (grid_check_x(gd, px + nx - 1) != 0)
		return;
	if (grid_check_x(gd, dx + nx - 1) != 0)
		return;
	if (grid_check_y(gd, py) != 0)
		return;

	grid_expand_line(gd, py, px + nx);
	grid_expand_line(gd, py, dx + nx);
	memmove(&gd->data[py][dx], &gd->data[py][px], nx * (sizeof **gd->data));
	memmove(&gd->text[py][dx], &gd->text[py][px], nx * (sizeof **gd->text));

	/* Wipe any cells that have been moved. */
	for (xx = px; xx < px + nx; xx++) {
		if (xx >= dx && xx < dx + nx)
			continue;
		grid_put_cell(gd, xx, py, &grid_default_cell);
		grid_put_text(gd, xx, py, ' ' );
	}
}

