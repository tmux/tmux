/* $OpenBSD$ */

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

#include <stdlib.h>
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
const struct grid_cell grid_default_cell = {
	0, 0, 8, 8, { { ' ' }, 0, 1, 1 }
};
static const struct grid_cell_entry grid_default_entry = {
	0, { .data = { 0, 8, 8, ' ' } }
};

static void	grid_empty_line(struct grid *, u_int, u_int);

/* Store cell in entry. */
static void
grid_store_cell(struct grid_cell_entry *gce, const struct grid_cell *gc,
    u_char c)
{
	gce->flags = gc->flags;

	gce->data.fg = gc->fg & 0xff;
	if (gc->fg & COLOUR_FLAG_256)
		gce->flags |= GRID_FLAG_FG256;

	gce->data.bg = gc->bg & 0xff;
	if (gc->bg & COLOUR_FLAG_256)
		gce->flags |= GRID_FLAG_BG256;

	gce->data.attr = gc->attr;
	gce->data.data = c;
}

/* Check if a cell should be extended. */
static int
grid_need_extended_cell(const struct grid_cell_entry *gce,
    const struct grid_cell *gc)
{
	if (gce->flags & GRID_FLAG_EXTENDED)
		return (1);
	if (gc->attr > 0xff)
		return (1);
	if (gc->data.size != 1 || gc->data.width != 1)
		return (1);
	if ((gc->fg & COLOUR_FLAG_RGB) || (gc->bg & COLOUR_FLAG_RGB))
		return (1);
	return (0);
}

/* Free up unused extended cells. */
static void
grid_compact_line(struct grid_line *gl)
{
	int			 new_extdsize = 0;
	struct grid_cell	*new_extddata;
	struct grid_cell_entry	*gce;
	struct grid_cell	*gc;
	u_int			 px, idx;

	if (gl->extdsize == 0)
		return;

	for (px = 0; px < gl->cellsize; px++) {
		gce = &gl->celldata[px];
		if (gce->flags & GRID_FLAG_EXTENDED)
			new_extdsize++;
	}

	if (new_extdsize == 0) {
		free(gl->extddata);
		gl->extddata = NULL;
		gl->extdsize = 0;
		return;
	}
	new_extddata = xreallocarray(NULL, new_extdsize, sizeof *gl->extddata);

	idx = 0;
	for (px = 0; px < gl->cellsize; px++) {
		gce = &gl->celldata[px];
		if (gce->flags & GRID_FLAG_EXTENDED) {
			gc = &gl->extddata[gce->offset];
			memcpy(&new_extddata[idx], gc, sizeof *gc);
			gce->offset = idx++;
		}
	}

	free(gl->extddata);
	gl->extddata = new_extddata;
	gl->extdsize = new_extdsize;
}

/* Set cell as extended. */
static struct grid_cell *
grid_extended_cell(struct grid_line *gl, struct grid_cell_entry *gce,
    const struct grid_cell *gc)
{
	struct grid_cell	*gcp;

	gl->flags |= GRID_LINE_EXTENDED;

	if (~gce->flags & GRID_FLAG_EXTENDED) {
		gl->extddata = xreallocarray(gl->extddata, gl->extdsize + 1,
		    sizeof *gl->extddata);
		gce->offset = gl->extdsize++;
		gce->flags = gc->flags | GRID_FLAG_EXTENDED;
	}
	if (gce->offset >= gl->extdsize)
		fatalx("offset too big");

	gcp = &gl->extddata[gce->offset];
	memcpy(gcp, gc, sizeof *gcp);
	return (gcp);
}

/* Get line data. */
struct grid_line *
grid_get_line(struct grid *gd, u_int line)
{
	return (&gd->linedata[line]);
}

/* Adjust number of lines. */
void
grid_adjust_lines(struct grid *gd, u_int lines)
{
	gd->linedata = xreallocarray(gd->linedata, lines, sizeof *gd->linedata);
}

/* Copy default into a cell. */
static void
grid_clear_cell(struct grid *gd, u_int px, u_int py, u_int bg)
{
	struct grid_line	*gl = &gd->linedata[py];
	struct grid_cell_entry	*gce = &gl->celldata[px];
	struct grid_cell	*gc;

	memcpy(gce, &grid_default_entry, sizeof *gce);
	if (bg & COLOUR_FLAG_RGB) {
		gc = grid_extended_cell(gl, gce, &grid_default_cell);
		gc->bg = bg;
	} else {
		if (bg & COLOUR_FLAG_256)
			gce->flags |= GRID_FLAG_BG256;
		gce->data.bg = bg;
	}
}

/* Check grid y position. */
static int
grid_check_y(struct grid *gd, const char* from, u_int py)
{
	if (py >= gd->hsize + gd->sy) {
		log_debug("%s: y out of range: %u", from, py);
		return (-1);
	}
	return (0);
}

/* Compare grid cells. Return 1 if equal, 0 if not. */
int
grid_cells_equal(const struct grid_cell *gca, const struct grid_cell *gcb)
{
	if (gca->fg != gcb->fg || gca->bg != gcb->bg)
		return (0);
	if (gca->attr != gcb->attr || gca->flags != gcb->flags)
		return (0);
	if (gca->data.width != gcb->data.width)
		return (0);
	if (gca->data.size != gcb->data.size)
		return (0);
	return (memcmp(gca->data.data, gcb->data.data, gca->data.size) == 0);
}

/* Free one line. */
static void
grid_free_line(struct grid *gd, u_int py)
{
	free(gd->linedata[py].celldata);
	gd->linedata[py].celldata = NULL;
	free(gd->linedata[py].extddata);
	gd->linedata[py].extddata = NULL;
}

/* Free several lines. */
static void
grid_free_lines(struct grid *gd, u_int py, u_int ny)
{
	u_int	yy;

	for (yy = py; yy < py + ny; yy++)
		grid_free_line(gd, yy);
}

/* Create a new grid. */
struct grid *
grid_create(u_int sx, u_int sy, u_int hlimit)
{
	struct grid	*gd;

	gd = xmalloc(sizeof *gd);
	gd->sx = sx;
	gd->sy = sy;

	gd->flags = GRID_HISTORY;

	gd->hscrolled = 0;
	gd->hsize = 0;
	gd->hlimit = hlimit;

	if (gd->sy != 0)
		gd->linedata = xcalloc(gd->sy, sizeof *gd->linedata);
	else
		gd->linedata = NULL;

	return (gd);
}

/* Destroy grid. */
void
grid_destroy(struct grid *gd)
{
	grid_free_lines(gd, 0, gd->hsize + gd->sy);

	free(gd->linedata);

	free(gd);
}

/* Compare grids. */
int
grid_compare(struct grid *ga, struct grid *gb)
{
	struct grid_line	*gla, *glb;
	struct grid_cell	 gca, gcb;
	u_int			 xx, yy;

	if (ga->sx != gb->sx || ga->sy != gb->sy)
		return (1);

	for (yy = 0; yy < ga->sy; yy++) {
		gla = &ga->linedata[yy];
		glb = &gb->linedata[yy];
		if (gla->cellsize != glb->cellsize)
			return (1);
		for (xx = 0; xx < gla->cellsize; xx++) {
			grid_get_cell(ga, xx, yy, &gca);
			grid_get_cell(gb, xx, yy, &gcb);
			if (!grid_cells_equal(&gca, &gcb))
				return (1);
		}
	}

	return (0);
}

/* Trim lines from the history. */
static void
grid_trim_history(struct grid *gd, u_int ny)
{
	grid_free_lines(gd, 0, ny);
	memmove(&gd->linedata[0], &gd->linedata[ny],
	    (gd->hsize + gd->sy - ny) * (sizeof *gd->linedata));
}

/*
 * Collect lines from the history if at the limit. Free the top (oldest) 10%
 * and shift up.
 */
void
grid_collect_history(struct grid *gd)
{
	u_int	ny;

	if (gd->hsize == 0 || gd->hsize < gd->hlimit)
		return;

	ny = gd->hlimit / 10;
	if (ny < 1)
		ny = 1;
	if (ny > gd->hsize)
		ny = gd->hsize;

	/*
	 * Free the lines from 0 to ny then move the remaining lines over
	 * them.
	 */
	grid_trim_history(gd, ny);

	gd->hsize -= ny;
	if (gd->hscrolled > gd->hsize)
		gd->hscrolled = gd->hsize;
}

/*
 * Scroll the entire visible screen, moving one line into the history. Just
 * allocate a new line at the bottom and move the history size indicator.
 */
void
grid_scroll_history(struct grid *gd, u_int bg)
{
	u_int	yy;

	yy = gd->hsize + gd->sy;
	gd->linedata = xreallocarray(gd->linedata, yy + 1,
	    sizeof *gd->linedata);
	grid_empty_line(gd, yy, bg);

	gd->hscrolled++;
	grid_compact_line(&gd->linedata[gd->hsize]);
	gd->hsize++;
}

/* Clear the history. */
void
grid_clear_history(struct grid *gd)
{
	grid_trim_history(gd, gd->hsize);

	gd->hscrolled = 0;
	gd->hsize = 0;

	gd->linedata = xreallocarray(gd->linedata, gd->sy,
	    sizeof *gd->linedata);
}

/* Scroll a region up, moving the top line into the history. */
void
grid_scroll_history_region(struct grid *gd, u_int upper, u_int lower, u_int bg)
{
	struct grid_line	*gl_history, *gl_upper;
	u_int			 yy;

	/* Create a space for a new line. */
	yy = gd->hsize + gd->sy;
	gd->linedata = xreallocarray(gd->linedata, yy + 1,
	    sizeof *gd->linedata);

	/* Move the entire screen down to free a space for this line. */
	gl_history = &gd->linedata[gd->hsize];
	memmove(gl_history + 1, gl_history, gd->sy * sizeof *gl_history);

	/* Adjust the region and find its start and end. */
	upper++;
	gl_upper = &gd->linedata[upper];
	lower++;

	/* Move the line into the history. */
	memcpy(gl_history, gl_upper, sizeof *gl_history);

	/* Then move the region up and clear the bottom line. */
	memmove(gl_upper, gl_upper + 1, (lower - upper) * sizeof *gl_upper);
	grid_empty_line(gd, lower, bg);

	/* Move the history offset down over the line. */
	gd->hscrolled++;
	gd->hsize++;
}

/* Expand line to fit to cell. */
static void
grid_expand_line(struct grid *gd, u_int py, u_int sx, u_int bg)
{
	struct grid_line	*gl;
	u_int			 xx;

	gl = &gd->linedata[py];
	if (sx <= gl->cellsize)
		return;

	if (sx < gd->sx / 4)
		sx = gd->sx / 4;
	else if (sx < gd->sx / 2)
		sx = gd->sx / 2;
	else
		sx = gd->sx;

	gl->celldata = xreallocarray(gl->celldata, sx, sizeof *gl->celldata);
	for (xx = gl->cellsize; xx < sx; xx++)
		grid_clear_cell(gd, xx, py, bg);
	gl->cellsize = sx;
}

/* Empty a line and set background colour if needed. */
static void
grid_empty_line(struct grid *gd, u_int py, u_int bg)
{
	memset(&gd->linedata[py], 0, sizeof gd->linedata[py]);
	if (bg != 8)
		grid_expand_line(gd, py, gd->sx, bg);
}

/* Peek at grid line. */
const struct grid_line *
grid_peek_line(struct grid *gd, u_int py)
{
	if (grid_check_y(gd, __func__, py) != 0)
		return (NULL);
	return (&gd->linedata[py]);
}

/* Get cell from line. */
static void
grid_get_cell1(struct grid_line *gl, u_int px, struct grid_cell *gc)
{
	struct grid_cell_entry	*gce = &gl->celldata[px];

	if (gce->flags & GRID_FLAG_EXTENDED) {
		if (gce->offset >= gl->extdsize)
			memcpy(gc, &grid_default_cell, sizeof *gc);
		else
			memcpy(gc, &gl->extddata[gce->offset], sizeof *gc);
		return;
	}

	gc->flags = gce->flags & ~(GRID_FLAG_FG256|GRID_FLAG_BG256);
	gc->attr = gce->data.attr;
	gc->fg = gce->data.fg;
	if (gce->flags & GRID_FLAG_FG256)
		gc->fg |= COLOUR_FLAG_256;
	gc->bg = gce->data.bg;
	if (gce->flags & GRID_FLAG_BG256)
		gc->bg |= COLOUR_FLAG_256;
	utf8_set(&gc->data, gce->data.data);
}

/* Get cell for reading. */
void
grid_get_cell(struct grid *gd, u_int px, u_int py, struct grid_cell *gc)
{
	if (grid_check_y(gd, __func__, py) != 0 ||
	    px >= gd->linedata[py].cellsize) {
		memcpy(gc, &grid_default_cell, sizeof *gc);
		return;
	}
	return (grid_get_cell1(&gd->linedata[py], px, gc));
}

/* Set cell at relative position. */
void
grid_set_cell(struct grid *gd, u_int px, u_int py, const struct grid_cell *gc)
{
	struct grid_line	*gl;
	struct grid_cell_entry	*gce;

	if (grid_check_y(gd, __func__, py) != 0)
		return;

	grid_expand_line(gd, py, px + 1, 8);

	gl = &gd->linedata[py];
	if (px + 1 > gl->cellused)
		gl->cellused = px + 1;

	gce = &gl->celldata[px];
	if (grid_need_extended_cell(gce, gc))
		grid_extended_cell(gl, gce, gc);
	else
		grid_store_cell(gce, gc, gc->data.data[0]);
}

/* Set cells at relative position. */
void
grid_set_cells(struct grid *gd, u_int px, u_int py, const struct grid_cell *gc,
    const char *s, size_t slen)
{
	struct grid_line	*gl;
	struct grid_cell_entry	*gce;
	struct grid_cell	*gcp;
	u_int			 i;

	if (grid_check_y(gd, __func__, py) != 0)
		return;

	grid_expand_line(gd, py, px + slen, 8);

	gl = &gd->linedata[py];
	if (px + slen > gl->cellused)
		gl->cellused = px + slen;

	for (i = 0; i < slen; i++) {
		gce = &gl->celldata[px + i];
		if (grid_need_extended_cell(gce, gc)) {
			gcp = grid_extended_cell(gl, gce, gc);
			utf8_set(&gcp->data, s[i]);
		} else
			grid_store_cell(gce, gc, s[i]);
	}
}

/* Clear area. */
void
grid_clear(struct grid *gd, u_int px, u_int py, u_int nx, u_int ny, u_int bg)
{
	u_int	xx, yy;

	if (nx == 0 || ny == 0)
		return;

	if (px == 0 && nx == gd->sx) {
		grid_clear_lines(gd, py, ny, bg);
		return;
	}

	if (grid_check_y(gd, __func__, py) != 0)
		return;
	if (grid_check_y(gd, __func__, py + ny - 1) != 0)
		return;

	for (yy = py; yy < py + ny; yy++) {
		if (px + nx >= gd->sx && px < gd->linedata[yy].cellused)
			gd->linedata[yy].cellused = px;
		if (px > gd->linedata[yy].cellsize && bg == 8)
			continue;
		if (px + nx >= gd->linedata[yy].cellsize && bg == 8) {
			gd->linedata[yy].cellsize = px;
			continue;
		}
		grid_expand_line(gd, yy, px + nx, 8); /* default bg first */
		for (xx = px; xx < px + nx; xx++)
			grid_clear_cell(gd, xx, yy, bg);
	}
}

/* Clear lines. This just frees and truncates the lines. */
void
grid_clear_lines(struct grid *gd, u_int py, u_int ny, u_int bg)
{
	u_int	yy;

	if (ny == 0)
		return;

	if (grid_check_y(gd, __func__, py) != 0)
		return;
	if (grid_check_y(gd, __func__, py + ny - 1) != 0)
		return;

	for (yy = py; yy < py + ny; yy++) {
		grid_free_line(gd, yy);
		grid_empty_line(gd, yy, bg);
	}
}

/* Move a group of lines. */
void
grid_move_lines(struct grid *gd, u_int dy, u_int py, u_int ny, u_int bg)
{
	u_int	yy;

	if (ny == 0 || py == dy)
		return;

	if (grid_check_y(gd, __func__, py) != 0)
		return;
	if (grid_check_y(gd, __func__, py + ny - 1) != 0)
		return;
	if (grid_check_y(gd, __func__, dy) != 0)
		return;
	if (grid_check_y(gd, __func__, dy + ny - 1) != 0)
		return;

	/* Free any lines which are being replaced. */
	for (yy = dy; yy < dy + ny; yy++) {
		if (yy >= py && yy < py + ny)
			continue;
		grid_free_line(gd, yy);
	}

	memmove(&gd->linedata[dy], &gd->linedata[py],
	    ny * (sizeof *gd->linedata));

	/*
	 * Wipe any lines that have been moved (without freeing them - they are
	 * still present).
	 */
	for (yy = py; yy < py + ny; yy++) {
		if (yy < dy || yy >= dy + ny)
			grid_empty_line(gd, yy, bg);
	}
}

/* Move a group of cells. */
void
grid_move_cells(struct grid *gd, u_int dx, u_int px, u_int py, u_int nx,
    u_int bg)
{
	struct grid_line	*gl;
	u_int			 xx;

	if (nx == 0 || px == dx)
		return;

	if (grid_check_y(gd, __func__, py) != 0)
		return;
	gl = &gd->linedata[py];

	grid_expand_line(gd, py, px + nx, 8);
	grid_expand_line(gd, py, dx + nx, 8);
	memmove(&gl->celldata[dx], &gl->celldata[px],
	    nx * sizeof *gl->celldata);
	if (dx + nx > gl->cellused)
		gl->cellused = dx + nx;

	/* Wipe any cells that have been moved. */
	for (xx = px; xx < px + nx; xx++) {
		if (xx >= dx && xx < dx + nx)
			continue;
		grid_clear_cell(gd, xx, py, bg);
	}
}

/* Get ANSI foreground sequence. */
static size_t
grid_string_cells_fg(const struct grid_cell *gc, int *values)
{
	size_t	n;
	u_char	r, g, b;

	n = 0;
	if (gc->fg & COLOUR_FLAG_256) {
		values[n++] = 38;
		values[n++] = 5;
		values[n++] = gc->fg & 0xff;
	} else if (gc->fg & COLOUR_FLAG_RGB) {
		values[n++] = 38;
		values[n++] = 2;
		colour_split_rgb(gc->fg, &r, &g, &b);
		values[n++] = r;
		values[n++] = g;
		values[n++] = b;
	} else {
		switch (gc->fg) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			values[n++] = gc->fg + 30;
			break;
		case 8:
			values[n++] = 39;
			break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
			values[n++] = gc->fg;
			break;
		}
	}
	return (n);
}

/* Get ANSI background sequence. */
static size_t
grid_string_cells_bg(const struct grid_cell *gc, int *values)
{
	size_t	n;
	u_char	r, g, b;

	n = 0;
	if (gc->bg & COLOUR_FLAG_256) {
		values[n++] = 48;
		values[n++] = 5;
		values[n++] = gc->bg & 0xff;
	} else if (gc->bg & COLOUR_FLAG_RGB) {
		values[n++] = 48;
		values[n++] = 2;
		colour_split_rgb(gc->bg, &r, &g, &b);
		values[n++] = r;
		values[n++] = g;
		values[n++] = b;
	} else {
		switch (gc->bg) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			values[n++] = gc->bg + 40;
			break;
		case 8:
			values[n++] = 49;
			break;
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
			values[n++] = gc->bg - 10;
			break;
		}
	}
	return (n);
}

/*
 * Returns ANSI code to set particular attributes (colour, bold and so on)
 * given a current state.
 */
static void
grid_string_cells_code(const struct grid_cell *lastgc,
    const struct grid_cell *gc, char *buf, size_t len, int escape_c0)
{
	int	oldc[64], newc[64], s[128];
	size_t	noldc, nnewc, n, i;
	u_int	attr = gc->attr, lastattr = lastgc->attr;
	char	tmp[64];

	struct {
		u_int	mask;
		u_int	code;
	} attrs[] = {
		{ GRID_ATTR_BRIGHT, 1 },
		{ GRID_ATTR_DIM, 2 },
		{ GRID_ATTR_ITALICS, 3 },
		{ GRID_ATTR_UNDERSCORE, 4 },
		{ GRID_ATTR_BLINK, 5 },
		{ GRID_ATTR_REVERSE, 7 },
		{ GRID_ATTR_HIDDEN, 8 },
		{ GRID_ATTR_STRIKETHROUGH, 9 }
	};
	n = 0;

	/* If any attribute is removed, begin with 0. */
	for (i = 0; i < nitems(attrs); i++) {
		if (!(attr & attrs[i].mask) && (lastattr & attrs[i].mask)) {
			s[n++] = 0;
			lastattr &= GRID_ATTR_CHARSET;
			break;
		}
	}
	/* For each attribute that is newly set, add its code. */
	for (i = 0; i < nitems(attrs); i++) {
		if ((attr & attrs[i].mask) && !(lastattr & attrs[i].mask))
			s[n++] = attrs[i].code;
	}

	/* Write the attributes. */
	*buf = '\0';
	if (n > 0) {
		if (escape_c0)
			strlcat(buf, "\\033[", len);
		else
			strlcat(buf, "\033[", len);
		for (i = 0; i < n; i++) {
			if (i + 1 < n)
				xsnprintf(tmp, sizeof tmp, "%d;", s[i]);
			else
				xsnprintf(tmp, sizeof tmp, "%d", s[i]);
			strlcat(buf, tmp, len);
		}
		strlcat(buf, "m", len);
	}

	/* If the foreground colour changed, write its parameters. */
	nnewc = grid_string_cells_fg(gc, newc);
	noldc = grid_string_cells_fg(lastgc, oldc);
	if (nnewc != noldc ||
	    memcmp(newc, oldc, nnewc * sizeof newc[0]) != 0 ||
	    (n != 0 && s[0] == 0)) {
		if (escape_c0)
			strlcat(buf, "\\033[", len);
		else
			strlcat(buf, "\033[", len);
		for (i = 0; i < nnewc; i++) {
			if (i + 1 < nnewc)
				xsnprintf(tmp, sizeof tmp, "%d;", newc[i]);
			else
				xsnprintf(tmp, sizeof tmp, "%d", newc[i]);
			strlcat(buf, tmp, len);
		}
		strlcat(buf, "m", len);
	}

	/* If the background colour changed, append its parameters. */
	nnewc = grid_string_cells_bg(gc, newc);
	noldc = grid_string_cells_bg(lastgc, oldc);
	if (nnewc != noldc ||
	    memcmp(newc, oldc, nnewc * sizeof newc[0]) != 0 ||
	    (n != 0 && s[0] == 0)) {
		if (escape_c0)
			strlcat(buf, "\\033[", len);
		else
			strlcat(buf, "\033[", len);
		for (i = 0; i < nnewc; i++) {
			if (i + 1 < nnewc)
				xsnprintf(tmp, sizeof tmp, "%d;", newc[i]);
			else
				xsnprintf(tmp, sizeof tmp, "%d", newc[i]);
			strlcat(buf, tmp, len);
		}
		strlcat(buf, "m", len);
	}

	/* Append shift in/shift out if needed. */
	if ((attr & GRID_ATTR_CHARSET) && !(lastattr & GRID_ATTR_CHARSET)) {
		if (escape_c0)
			strlcat(buf, "\\016", len); /* SO */
		else
			strlcat(buf, "\016", len);  /* SO */
	}
	if (!(attr & GRID_ATTR_CHARSET) && (lastattr & GRID_ATTR_CHARSET)) {
		if (escape_c0)
			strlcat(buf, "\\017", len); /* SI */
		else
			strlcat(buf, "\017", len);  /* SI */
	}
}

/* Convert cells into a string. */
char *
grid_string_cells(struct grid *gd, u_int px, u_int py, u_int nx,
    struct grid_cell **lastgc, int with_codes, int escape_c0, int trim)
{
	struct grid_cell	 gc;
	static struct grid_cell	 lastgc1;
	const char		*data;
	char			*buf, code[128];
	size_t			 len, off, size, codelen;
	u_int			 xx;
	const struct grid_line	*gl;

	if (lastgc != NULL && *lastgc == NULL) {
		memcpy(&lastgc1, &grid_default_cell, sizeof lastgc1);
		*lastgc = &lastgc1;
	}

	len = 128;
	buf = xmalloc(len);
	off = 0;

	gl = grid_peek_line(gd, py);
	for (xx = px; xx < px + nx; xx++) {
		if (gl == NULL || xx >= gl->cellsize)
			break;
		grid_get_cell(gd, xx, py, &gc);
		if (gc.flags & GRID_FLAG_PADDING)
			continue;

		if (with_codes) {
			grid_string_cells_code(*lastgc, &gc, code, sizeof code,
			    escape_c0);
			codelen = strlen(code);
			memcpy(*lastgc, &gc, sizeof **lastgc);
		} else
			codelen = 0;

		data = gc.data.data;
		size = gc.data.size;
		if (escape_c0 && size == 1 && *data == '\\') {
			data = "\\\\";
			size = 2;
		}

		while (len < off + size + codelen + 1) {
			buf = xreallocarray(buf, 2, len);
			len *= 2;
		}

		if (codelen != 0) {
			memcpy(buf + off, code, codelen);
			off += codelen;
		}
		memcpy(buf + off, data, size);
		off += size;
	}

	if (trim) {
		while (off > 0 && buf[off - 1] == ' ')
			off--;
	}
	buf[off] = '\0';

	return (buf);
}

/*
 * Duplicate a set of lines between two grids. Both source and destination
 * should be big enough.
 */
void
grid_duplicate_lines(struct grid *dst, u_int dy, struct grid *src, u_int sy,
    u_int ny)
{
	struct grid_line	*dstl, *srcl;
	u_int			 yy;

	if (dy + ny > dst->hsize + dst->sy)
		ny = dst->hsize + dst->sy - dy;
	if (sy + ny > src->hsize + src->sy)
		ny = src->hsize + src->sy - sy;
	grid_free_lines(dst, dy, ny);

	for (yy = 0; yy < ny; yy++) {
		srcl = &src->linedata[sy];
		dstl = &dst->linedata[dy];

		memcpy(dstl, srcl, sizeof *dstl);
		if (srcl->cellsize != 0) {
			dstl->celldata = xreallocarray(NULL,
			    srcl->cellsize, sizeof *dstl->celldata);
			memcpy(dstl->celldata, srcl->celldata,
			    srcl->cellsize * sizeof *dstl->celldata);
		} else
			dstl->celldata = NULL;

		if (srcl->extdsize != 0) {
			dstl->extdsize = srcl->extdsize;
			dstl->extddata = xreallocarray(NULL, dstl->extdsize,
			    sizeof *dstl->extddata);
			memcpy(dstl->extddata, srcl->extddata, dstl->extdsize *
			    sizeof *dstl->extddata);
		}

		sy++;
		dy++;
	}
}

/* Mark line as dead. */
static void
grid_reflow_dead(struct grid_line *gl)
{
	memset(gl, 0, sizeof *gl);
	gl->flags = GRID_LINE_DEAD;
}

/* Add lines, return the first new one. */
static struct grid_line *
grid_reflow_add(struct grid *gd, u_int n)
{
	struct grid_line	*gl;
	u_int			 sy = gd->sy + n;

	gd->linedata = xreallocarray(gd->linedata, sy, sizeof *gd->linedata);
	gl = &gd->linedata[gd->sy];
	memset(gl, 0, n * (sizeof *gl));
	gd->sy = sy;
	return (gl);
}

/* Move a line across. */
static struct grid_line *
grid_reflow_move(struct grid *gd, struct grid_line *from)
{
	struct grid_line	*to;

	to = grid_reflow_add(gd, 1);
	memcpy(to, from, sizeof *to);
	grid_reflow_dead(from);
	return (to);
}

/* Join line below onto this one. */
static void
grid_reflow_join(struct grid *target, struct grid *gd, u_int sx, u_int yy,
    u_int width, u_int *cy, int already)
{
	struct grid_line	*gl, *from = NULL;
	struct grid_cell	 gc;
	u_int			 lines, left, i, to, line, want = 0;
	u_int			 at;
	int			 wrapped = 1;

	/*
	 * Add a new target line.
	 */
	if (!already) {
		to = target->sy;
		gl = grid_reflow_move(target, &gd->linedata[yy]);
	} else {
		to = target->sy - 1;
		gl = &target->linedata[to];
	}
	at = gl->cellused;

	/*
	 * Loop until no more to consume or the target line is full.
	 */
	lines = 0;
	for (;;) {
		/*
		 * If this is now the last line, there is nothing more to be
		 * done.
		 */
		if (yy + 1 + lines == gd->hsize + gd->sy)
			break;
		line = yy + 1 + lines;

		/* If the next line is empty, skip it. */
		if (~gd->linedata[line].flags & GRID_LINE_WRAPPED)
			wrapped = 0;
		if (gd->linedata[line].cellused == 0) {
			if (!wrapped)
				break;
			lines++;
			continue;
		}

		/*
		 * Is the destination line now full? Copy the first character
		 * separately because we need to leave "from" set to the last
		 * line if this line is full.
		 */
		grid_get_cell1(&gd->linedata[line], 0, &gc);
		if (width + gc.data.width > sx)
			break;
		width += gc.data.width;
		grid_set_cell(target, at, to, &gc);
		at++;

		/* Join as much more as possible onto the current line. */
		from = &gd->linedata[line];
		for (want = 1; want < from->cellused; want++) {
			grid_get_cell1(from, want, &gc);
			if (width + gc.data.width > sx)
				break;
			width += gc.data.width;

			grid_set_cell(target, at, to, &gc);
			at++;
		}
		lines++;

		/*
		 * If this line wasn't wrapped or we didn't consume the entire
		 * line, don't try to join any further lines.
		 */
		if (!wrapped || want != from->cellused || width == sx)
			break;
	}
	if (lines == 0)
		return;

	/*
	 * If we didn't consume the entire final line, then remove what we did
	 * consume. If we consumed the entire line and it wasn't wrapped,
	 * remove the wrap flag from this line.
	 */
	left = from->cellused - want;
	if (left != 0) {
		grid_move_cells(gd, 0, want, yy + lines, left, 8);
		from->cellsize = from->cellused = left;
		lines--;
	} else if (!wrapped)
		gl->flags &= ~GRID_LINE_WRAPPED;

	/* Remove the lines that were completely consumed. */
	for (i = yy + 1; i < yy + 1 + lines; i++) {
		free(gd->linedata[i].celldata);
		free(gd->linedata[i].extddata);
		grid_reflow_dead(&gd->linedata[i]);
	}

	/* Adjust cursor and scroll positions. */
	if (*cy > to + lines)
		*cy -= lines;
	else if (*cy > to)
		*cy = to;
	if (gd->hscrolled > to + lines)
		gd->hscrolled -= lines;
	else if (gd->hscrolled > to)
		gd->hscrolled = to;
}

/* Split this line into several new ones */
static void
grid_reflow_split(struct grid *target, struct grid *gd, u_int sx, u_int yy,
    u_int at, u_int *cy)
{
	struct grid_line	*gl = &gd->linedata[yy], *first;
	struct grid_cell	 gc;
	u_int			 line, lines, width, i, xx;
	u_int			 used = gl->cellused;
	int			 flags = gl->flags;

	/* How many lines do we need to insert? We know we need at least two. */
	if (~gl->flags & GRID_LINE_EXTENDED)
		lines = 1 + (gl->cellused - 1) / sx;
	else {
		lines = 2;
		width = 0;
		for (i = at; i < used; i++) {
			grid_get_cell1(gl, i, &gc);
			if (width + gc.data.width > sx) {
				lines++;
				width = 0;
			}
			width += gc.data.width;
		}
	}

	/* Insert new lines. */
	line = target->sy + 1;
	first = grid_reflow_add(target, lines);

	/* Copy sections from the original line. */
	width = 0;
	xx = 0;
	for (i = at; i < used; i++) {
		grid_get_cell1(gl, i, &gc);
		if (width + gc.data.width > sx) {
			target->linedata[line].flags |= GRID_LINE_WRAPPED;

			line++;
			width = 0;
			xx = 0;
		}
		width += gc.data.width;
		grid_set_cell(target, xx, line, &gc);
		xx++;
	}
	if (flags & GRID_LINE_WRAPPED)
		target->linedata[line].flags |= GRID_LINE_WRAPPED;

	/* Move the remainder of the original line. */
	gl->cellsize = gl->cellused = at;
	gl->flags |= GRID_LINE_WRAPPED;
	memcpy(first, gl, sizeof *first);
	grid_reflow_dead(gl);

	/* Adjust the cursor and scroll positions. */
	if (yy <= *cy)
		(*cy) += lines - 1;
	if (yy <= gd->hscrolled)
		gd->hscrolled += lines - 1;

	/*
	 * If the original line had the wrapped flag and there is still space
	 * in the last new line, try to join with the next lines.
	 */
	if (width < sx && (flags & GRID_LINE_WRAPPED))
		grid_reflow_join(target, gd, sx, yy, width, cy, 1);
}

/* Reflow lines on grid to new width. */
void
grid_reflow(struct grid *gd, u_int sx, u_int *cursor)
{
	struct grid		*target;
	struct grid_line	*gl;
	struct grid_cell	 gc;
	u_int			 yy, cy, width, i, at, first;
	struct timeval		 start, tv;

	gettimeofday(&start, NULL);

	log_debug("%s: %u lines, new width %u", __func__, gd->hsize + gd->sy,
	    sx);
	cy = gd->hsize + (*cursor);

	/*
	 * Create a destination grid. This is just used as a container for the
	 * line data and may not be fully valid.
	 */
	target = grid_create(gd->sx, 0, 0);

	/*
	 * Loop over each source line.
	 */
	for (yy = 0; yy < gd->hsize + gd->sy; yy++) {
		gl = &gd->linedata[yy];
		if (gl->flags & GRID_LINE_DEAD)
			continue;

		/*
		 * Work out the width of this line. first is the width of the
		 * first character, at is the point at which the available
		 * width is hit, and width is the full line width.
		 */
		first = at = width = 0;
		if (~gl->flags & GRID_LINE_EXTENDED) {
			first = 1;
			width = gl->cellused;
			if (width > sx)
				at = sx;
			else
				at = width;
		} else {
			for (i = 0; i < gl->cellused; i++) {
				grid_get_cell1(gl, i, &gc);
				if (i == 0)
					first = gc.data.width;
				if (at == 0 && width + gc.data.width > sx)
					at = i;
				width += gc.data.width;
			}
		}

		/*
		 * If the line is exactly right or the first character is wider
		 * than the targe width, just move it across unchanged.
		 */
		if (width == sx || first > sx) {
			grid_reflow_move(target, gl);
			continue;
		}

		/*
		 * If the line is too big, it needs to be split, whether or not
		 * it was previously wrapped.
		 */
		if (width > sx) {
			grid_reflow_split(target, gd, sx, yy, at, &cy);
			continue;
		}

		/*
		 * If the line was previously wrapped, join as much as possible
		 * of the next line.
		 */
		if (gl->flags & GRID_LINE_WRAPPED)
			grid_reflow_join(target, gd, sx, yy, width, &cy, 0);
		else
			grid_reflow_move(target, gl);
	}

	/*
	 * Replace the old grid with the new.
	 */
	if (target->sy < gd->sy)
		grid_reflow_add(target, gd->sy - target->sy);
	gd->hsize = target->sy - gd->sy;
	free(gd->linedata);
	gd->linedata = target->linedata;
	free(target);

	/*
	 * Update scrolled and cursor positions.
	 */
	if (gd->hscrolled > gd->hsize)
		gd->hscrolled = gd->hsize;
	if (cy < gd->hsize)
		*cursor = 0;
	else
		*cursor = cy - gd->hsize;

	gettimeofday(&tv, NULL);
	timersub(&tv, &start, &tv);
	log_debug("%s: now %u lines (in %llu.%06u seconds)", __func__,
	    gd->hsize + gd->sy, (unsigned long long)tv.tv_sec,
	    (u_int)tv.tv_usec);
}
