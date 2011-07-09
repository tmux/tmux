/* $Id$ */

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
const struct grid_cell grid_default_cell = { 0, 0, 8, 8, ' ' };

#define grid_put_cell(gd, px, py, gc) do {			\
	memcpy(&gd->linedata[py].celldata[px], 			\
	    gc, sizeof gd->linedata[py].celldata[px]);		\
} while (0)
#define grid_put_utf8(gd, px, py, gc) do {			\
	memcpy(&gd->linedata[py].utf8data[px], 			\
	    gc, sizeof gd->linedata[py].utf8data[px]);		\
} while (0)

int	grid_check_y(struct grid *, u_int);

#ifdef DEBUG
int
grid_check_y(struct grid *gd, u_int py)
{
	if ((py) >= (gd)->hsize + (gd)->sy)
		log_fatalx("y out of range: %u", py);
	return (0);
}
#else
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

	gd->flags = GRID_HISTORY;

	gd->hsize = 0;
	gd->hlimit = hlimit;

	gd->linedata = xcalloc(gd->sy, sizeof *gd->linedata);

	return (gd);
}

/* Destroy grid. */
void
grid_destroy(struct grid *gd)
{
	struct grid_line	*gl;
	u_int			 yy;

	for (yy = 0; yy < gd->hsize + gd->sy; yy++) {
		gl = &gd->linedata[yy];
		if (gl->celldata != NULL)
			xfree(gl->celldata);
		if (gl->utf8data != NULL)
			xfree(gl->utf8data);
	}

	xfree(gd->linedata);

	xfree(gd);
}

/* Compare grids. */
int
grid_compare(struct grid *ga, struct grid *gb)
{
	struct grid_line	*gla, *glb;
	struct grid_cell	*gca, *gcb;
	struct grid_utf8	*gua, *gub;
	u_int			 xx, yy;

	if (ga->sx != gb->sx || ga->sy != ga->sy)
		return (1);

	for (yy = 0; yy < ga->sy; yy++) {
		gla = &ga->linedata[yy];
		glb = &gb->linedata[yy];
		if (gla->cellsize != glb->cellsize)
			return (1);
		for (xx = 0; xx < ga->sx; xx++) {
			gca = &gla->celldata[xx];
			gcb = &glb->celldata[xx];
			if (memcmp(gca, gcb, sizeof (struct grid_cell)) != 0)
				return (1);
			if (!(gca->flags & GRID_FLAG_UTF8))
				continue;
			gua = &gla->utf8data[xx];
			gub = &glb->utf8data[xx];
			if (memcmp(gua, gub, sizeof (struct grid_utf8)) != 0)
				return (1);
		}
	}

	return (0);
}

/*
 * Collect lines from the history if at the limit. Free the top (oldest) 10%
 * and shift up.
 */
void
grid_collect_history(struct grid *gd)
{
	u_int	yy;

	GRID_DEBUG(gd, "");

	if (gd->hsize < gd->hlimit)
		return;

	yy = gd->hlimit / 10;
	if (yy < 1)
		yy = 1;

	grid_move_lines(gd, 0, yy, gd->hsize + gd->sy - yy);
	gd->hsize -= yy;
}

/*
 * Scroll the entire visible screen, moving one line into the history. Just
 * allocate a new line at the bottom and move the history size indicator.
 */
void
grid_scroll_history(struct grid *gd)
{
	u_int	yy;

	GRID_DEBUG(gd, "");

	yy = gd->hsize + gd->sy;
	gd->linedata = xrealloc(gd->linedata, yy + 1, sizeof *gd->linedata);
	memset(&gd->linedata[yy], 0, sizeof gd->linedata[yy]);

	gd->hsize++;
}

/* Scroll a region up, moving the top line into the history. */
void
grid_scroll_history_region(struct grid *gd, u_int upper, u_int lower)
{
	struct grid_line	*gl_history, *gl_upper, *gl_lower;
	u_int			 yy;

	GRID_DEBUG(gd, "upper=%u, lower=%u", upper, lower);

	/* Create a space for a new line. */
	yy = gd->hsize + gd->sy;
	gd->linedata = xrealloc(gd->linedata, yy + 1, sizeof *gd->linedata);

	/* Move the entire screen down to free a space for this line. */
	gl_history = &gd->linedata[gd->hsize];
	memmove(gl_history + 1, gl_history, gd->sy * sizeof *gl_history);

	/* Adjust the region and find its start and end. */
	upper++;
	gl_upper = &gd->linedata[upper];
	lower++;
	gl_lower = &gd->linedata[lower];

	/* Move the line into the history. */
	memcpy(gl_history, gl_upper, sizeof *gl_history);

	/* Then move the region up and clear the bottom line. */
	memmove(gl_upper, gl_upper + 1, (lower - upper) * sizeof *gl_upper);
	memset(gl_lower, 0, sizeof *gl_lower);

	/* Move the history offset down over the line. */
	gd->hsize++;
}

/* Expand line to fit to cell. */
void
grid_expand_line(struct grid *gd, u_int py, u_int sx)
{
	struct grid_line	*gl;
	u_int			 xx;

	gl = &gd->linedata[py];
	if (sx <= gl->cellsize)
		return;

	gl->celldata = xrealloc(gl->celldata, sx, sizeof *gl->celldata);
	for (xx = gl->cellsize; xx < sx; xx++)
		grid_put_cell(gd, xx, py, &grid_default_cell);
	gl->cellsize = sx;
}

/* Expand line to fit to cell for UTF-8. */
void
grid_expand_line_utf8(struct grid *gd, u_int py, u_int sx)
{
	struct grid_line	*gl;

	gl = &gd->linedata[py];
	if (sx <= gl->utf8size)
		return;

	gl->utf8data = xrealloc(gl->utf8data, sx, sizeof *gl->utf8data);
	gl->utf8size = sx;
}

/* Get cell for reading. */
const struct grid_cell *
grid_peek_cell(struct grid *gd, u_int px, u_int py)
{
	if (grid_check_y(gd, py) != 0)
		return (&grid_default_cell);

	if (px >= gd->linedata[py].cellsize)
		return (&grid_default_cell);
	return (&gd->linedata[py].celldata[px]);
}

/* Get cell at relative position (for writing). */
struct grid_cell *
grid_get_cell(struct grid *gd, u_int px, u_int py)
{
	if (grid_check_y(gd, py) != 0)
		return (NULL);

	grid_expand_line(gd, py, px + 1);
	return (&gd->linedata[py].celldata[px]);
}

/* Set cell at relative position. */
void
grid_set_cell(
    struct grid *gd, u_int px, u_int py, const struct grid_cell *gc)
{
	if (grid_check_y(gd, py) != 0)
		return;

	grid_expand_line(gd, py, px + 1);
	grid_put_cell(gd, px, py, gc);
}

/* Get UTF-8 for reading. */
const struct grid_utf8 *
grid_peek_utf8(struct grid *gd, u_int px, u_int py)
{
	if (grid_check_y(gd, py) != 0)
		return (NULL);

	if (px >= gd->linedata[py].utf8size)
		return (NULL);
	return (&gd->linedata[py].utf8data[px]);
}

/* Get utf8 at relative position (for writing). */
struct grid_utf8 *
grid_get_utf8(struct grid *gd, u_int px, u_int py)
{
	if (grid_check_y(gd, py) != 0)
		return (NULL);

	grid_expand_line_utf8(gd, py, px + 1);
	return (&gd->linedata[py].utf8data[px]);
}

/* Set utf8 at relative position. */
void
grid_set_utf8(
    struct grid *gd, u_int px, u_int py, const struct grid_utf8 *gc)
{
	if (grid_check_y(gd, py) != 0)
		return;

	grid_expand_line_utf8(gd, py, px + 1);
	grid_put_utf8(gd, px, py, gc);
}

/* Clear area. */
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

	if (grid_check_y(gd, py) != 0)
		return;
	if (grid_check_y(gd, py + ny - 1) != 0)
		return;

	for (yy = py; yy < py + ny; yy++) {
		if (px >= gd->linedata[yy].cellsize)
			continue;
		if (px + nx >= gd->linedata[yy].cellsize) {
			gd->linedata[yy].cellsize = px;
			continue;
		}
		for (xx = px; xx < px + nx; xx++) {
			if (xx >= gd->linedata[yy].cellsize)
				break;
			grid_put_cell(gd, xx, yy, &grid_default_cell);
		}
	}
}

/* Clear lines. This just frees and truncates the lines. */
void
grid_clear_lines(struct grid *gd, u_int py, u_int ny)
{
	struct grid_line	*gl;
	u_int			 yy;

	GRID_DEBUG(gd, "py=%u, ny=%u", py, ny);

	if (ny == 0)
		return;

	if (grid_check_y(gd, py) != 0)
		return;
	if (grid_check_y(gd, py + ny - 1) != 0)
		return;

	for (yy = py; yy < py + ny; yy++) {
		gl = &gd->linedata[yy];
		if (gl->celldata != NULL)
			xfree(gl->celldata);
		if (gl->utf8data != NULL)
			xfree(gl->utf8data);
		memset(gl, 0, sizeof *gl);
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

	memmove(
	    &gd->linedata[dy], &gd->linedata[py], ny * (sizeof *gd->linedata));

	/* Wipe any lines that have been moved (without freeing them). */
	for (yy = py; yy < py + ny; yy++) {
		if (yy >= dy && yy < dy + ny)
			continue;
		memset(&gd->linedata[yy], 0, sizeof gd->linedata[yy]);
	}
}

/* Move a group of cells. */
void
grid_move_cells(struct grid *gd, u_int dx, u_int px, u_int py, u_int nx)
{
	struct grid_line	*gl;
	u_int			 xx;

	GRID_DEBUG(gd, "dx=%u, px=%u, py=%u, nx=%u", dx, px, py, nx);

	if (nx == 0 || px == dx)
		return;

	if (grid_check_y(gd, py) != 0)
		return;
	gl = &gd->linedata[py];

	grid_expand_line(gd, py, px + nx);
	grid_expand_line(gd, py, dx + nx);
	memmove(
	    &gl->celldata[dx], &gl->celldata[px], nx * sizeof *gl->celldata);

	if (gl->utf8data != NULL) {
		grid_expand_line_utf8(gd, py, px + nx);
		grid_expand_line_utf8(gd, py, dx + nx);
		memmove(&gl->utf8data[dx],
		    &gl->utf8data[px], nx * sizeof *gl->utf8data);
	}

	/* Wipe any cells that have been moved. */
	for (xx = px; xx < px + nx; xx++) {
		if (xx >= dx && xx < dx + nx)
			continue;
		grid_put_cell(gd, xx, py, &grid_default_cell);
	}
}

/* Convert cells into a string. */
char *
grid_string_cells(struct grid *gd, u_int px, u_int py, u_int nx)
{
	const struct grid_cell	*gc;
	const struct grid_utf8	*gu;
	char			*buf;
	size_t			 len, off, size;
	u_int			 xx;

	GRID_DEBUG(gd, "px=%u, py=%u, nx=%u", px, py, nx);

	len = 128;
	buf = xmalloc(len);
	off = 0;

	for (xx = px; xx < px + nx; xx++) {
		gc = grid_peek_cell(gd, xx, py);
		if (gc->flags & GRID_FLAG_PADDING)
			continue;

		if (gc->flags & GRID_FLAG_UTF8) {
			gu = grid_peek_utf8(gd, xx, py);

			size = grid_utf8_size(gu);
			while (len < off + size + 1) {
				buf = xrealloc(buf, 2, len);
				len *= 2;
			}

			off += grid_utf8_copy(gu, buf + off, len - off);
		} else {
			while (len < off + 2) {
				buf = xrealloc(buf, 2, len);
				len *= 2;
			}

			buf[off++] = gc->data;
		}
	}

	while (off > 0 && buf[off - 1] == ' ')
		off--;
	buf[off] = '\0';
	return (buf);
}

/*
 * Duplicate a set of lines between two grids. If there aren't enough lines in
 * either source or destination, the number of lines is limited to the number
 * available.
 */
void
grid_duplicate_lines(
    struct grid *dst, u_int dy, struct grid *src, u_int sy, u_int ny)
{
	struct grid_line	*dstl, *srcl;
	u_int			 yy;

	GRID_DEBUG(src, "dy=%u, sy=%u, ny=%u", dy, sy, ny);

	if (dy + ny > dst->hsize + dst->sy)
		ny = dst->hsize + dst->sy - dy;
	if (sy + ny > src->hsize + src->sy)
		ny = src->hsize + src->sy - sy;
	grid_clear_lines(dst, dy, ny);

	for (yy = 0; yy < ny; yy++) {
		srcl = &src->linedata[sy];
		dstl = &dst->linedata[dy];

		memcpy(dstl, srcl, sizeof *dstl);
		if (srcl->cellsize != 0) {
			dstl->celldata = xcalloc(
			    srcl->cellsize, sizeof *dstl->celldata);
			memcpy(dstl->celldata, srcl->celldata,
			    srcl->cellsize * sizeof *dstl->celldata);
		}
		if (srcl->utf8size != 0) {
			dstl->utf8data = xcalloc(
			    srcl->utf8size, sizeof *dstl->utf8data);
			memcpy(dstl->utf8data, srcl->utf8data,
			    srcl->utf8size * sizeof *dstl->utf8data);
		}

		sy++;
		dy++;
	}
}
