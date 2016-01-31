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
	0, 0, { .fg = 8 }, { .bg = 8 }, { { ' ' }, 0, 1, 1 }
};
const struct grid_cell_entry grid_default_entry = {
	0, { .data = { 0, 8, 8, ' ' } }
};

int	grid_check_y(struct grid *, u_int);

void	grid_reflow_copy(struct grid_line *, u_int, struct grid_line *l,
	    u_int, u_int);
void	grid_reflow_join(struct grid *, u_int *, struct grid_line *, u_int);
void	grid_reflow_split(struct grid *, u_int *, struct grid_line *, u_int,
	    u_int);
void	grid_reflow_move(struct grid *, u_int *, struct grid_line *);
size_t	grid_string_cells_fg(const struct grid_cell *, int *);
size_t	grid_string_cells_bg(const struct grid_cell *, int *);
void	grid_string_cells_code(const struct grid_cell *,
	    const struct grid_cell *, char *, size_t, int);

/* Copy default into a cell. */
static void
grid_clear_cell(struct grid *gd, u_int px, u_int py)
{
	gd->linedata[py].celldata[px] = grid_default_entry;
}

/* Check grid y position. */
int
grid_check_y(struct grid *gd, u_int py)
{
	if ((py) >= (gd)->hsize + (gd)->sy) {
		log_debug("y out of range: %u", py);
		return (-1);
	}
	return (0);
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
		free(gl->celldata);
		free(gl->extddata);
	}

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
			if (memcmp(&gca, &gcb, sizeof (struct grid_cell)) != 0)
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

	yy = gd->hsize + gd->sy;
	gd->linedata = xreallocarray(gd->linedata, yy + 1,
	    sizeof *gd->linedata);
	memset(&gd->linedata[yy], 0, sizeof gd->linedata[yy]);

	gd->hsize++;
}

/* Clear the history. */
void
grid_clear_history(struct grid *gd)
{
	grid_clear_lines(gd, 0, gd->hsize);
	grid_move_lines(gd, 0, gd->hsize, gd->sy);

	gd->hsize = 0;
	gd->linedata = xreallocarray(gd->linedata, gd->sy,
	    sizeof *gd->linedata);
}

/* Scroll a region up, moving the top line into the history. */
void
grid_scroll_history_region(struct grid *gd, u_int upper, u_int lower)
{
	struct grid_line	*gl_history, *gl_upper, *gl_lower;
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

	gl->celldata = xreallocarray(gl->celldata, sx, sizeof *gl->celldata);
	for (xx = gl->cellsize; xx < sx; xx++)
		grid_clear_cell(gd, xx, py);
	gl->cellsize = sx;
}

/* Peek at grid line. */
const struct grid_line *
grid_peek_line(struct grid *gd, u_int py)
{
	if (grid_check_y(gd, py) != 0)
		return (NULL);
	return (&gd->linedata[py]);
}

/* Get cell for reading. */
void
grid_get_cell(struct grid *gd, u_int px, u_int py, struct grid_cell *gc)
{
	struct grid_line	*gl;
	struct grid_cell_entry	*gce;

	if (grid_check_y(gd, py) != 0 || px >= gd->linedata[py].cellsize) {
		memcpy(gc, &grid_default_cell, sizeof *gc);
		return;
	}

	gl = &gd->linedata[py];
	gce = &gl->celldata[px];

	if (gce->flags & GRID_FLAG_EXTENDED) {
		if (gce->offset >= gl->extdsize)
			memcpy(gc, &grid_default_cell, sizeof *gc);
		else
			memcpy(gc, &gl->extddata[gce->offset], sizeof *gc);
		return;
	}

	gc->flags = gce->flags & ~GRID_FLAG_EXTENDED;
	gc->attr = gce->data.attr;
	gc->fg = gce->data.fg;
	gc->bg = gce->data.bg;
	utf8_set(&gc->data, gce->data.data);
}

/* Set cell at relative position. */
void
grid_set_cell(struct grid *gd, u_int px, u_int py, const struct grid_cell *gc)
{
	struct grid_line	*gl;
	struct grid_cell_entry	*gce;
	struct grid_cell 	*gcp;
	int			 extended;

	if (grid_check_y(gd, py) != 0)
		return;

	grid_expand_line(gd, py, px + 1);

	gl = &gd->linedata[py];
	gce = &gl->celldata[px];

	extended = (gce->flags & GRID_FLAG_EXTENDED);
	if (!extended && (gc->data.size != 1 || gc->data.width != 1))
		extended = 1;
	if (!extended && (gc->flags & (GRID_FLAG_FGRGB|GRID_FLAG_BGRGB)))
		extended = 1;
	if (extended) {
		if (~gce->flags & GRID_FLAG_EXTENDED) {
			gl->extddata = xreallocarray(gl->extddata,
			    gl->extdsize + 1, sizeof *gl->extddata);
			gce->offset = gl->extdsize++;
			gce->flags = gc->flags | GRID_FLAG_EXTENDED;
		}

		if (gce->offset >= gl->extdsize)
			fatalx("offset too big");
		gcp = &gl->extddata[gce->offset];
		memcpy(gcp, gc, sizeof *gcp);
		return;
	}

	gce->flags = gc->flags & ~GRID_FLAG_EXTENDED;
	gce->data.attr = gc->attr;
	gce->data.fg = gc->fg;
	gce->data.bg = gc->bg;
	gce->data.data = gc->data.data[0];
}

/* Clear area. */
void
grid_clear(struct grid *gd, u_int px, u_int py, u_int nx, u_int ny)
{
	u_int	xx, yy;

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
			grid_clear_cell(gd, xx, yy);
		}
	}
}

/* Clear lines. This just frees and truncates the lines. */
void
grid_clear_lines(struct grid *gd, u_int py, u_int ny)
{
	struct grid_line	*gl;
	u_int			 yy;

	if (ny == 0)
		return;

	if (grid_check_y(gd, py) != 0)
		return;
	if (grid_check_y(gd, py + ny - 1) != 0)
		return;

	for (yy = py; yy < py + ny; yy++) {
		gl = &gd->linedata[yy];
		free(gl->celldata);
		free(gl->extddata);
		memset(gl, 0, sizeof *gl);
	}
}

/* Move a group of lines. */
void
grid_move_lines(struct grid *gd, u_int dy, u_int py, u_int ny)
{
	u_int	yy;

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

	memmove(&gd->linedata[dy], &gd->linedata[py],
	    ny * (sizeof *gd->linedata));

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

	if (nx == 0 || px == dx)
		return;

	if (grid_check_y(gd, py) != 0)
		return;
	gl = &gd->linedata[py];

	grid_expand_line(gd, py, px + nx);
	grid_expand_line(gd, py, dx + nx);
	memmove(&gl->celldata[dx], &gl->celldata[px],
	    nx * sizeof *gl->celldata);

	/* Wipe any cells that have been moved. */
	for (xx = px; xx < px + nx; xx++) {
		if (xx >= dx && xx < dx + nx)
			continue;
		grid_clear_cell(gd, xx, py);
	}
}

/* Get ANSI foreground sequence. */
size_t
grid_string_cells_fg(const struct grid_cell *gc, int *values)
{
	size_t	n;

	n = 0;
	if (gc->flags & GRID_FLAG_FG256) {
		values[n++] = 38;
		values[n++] = 5;
		values[n++] = gc->fg;
	} else if (gc->flags & GRID_FLAG_FGRGB) {
		values[n++] = 38;
		values[n++] = 2;
		values[n++] = gc->fg_rgb.r;
		values[n++] = gc->fg_rgb.g;
		values[n++] = gc->fg_rgb.b;
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
size_t
grid_string_cells_bg(const struct grid_cell *gc, int *values)
{
	size_t	n;

	n = 0;
	if (gc->flags & GRID_FLAG_BG256) {
		values[n++] = 48;
		values[n++] = 5;
		values[n++] = gc->bg;
	} else if (gc->flags & GRID_FLAG_BGRGB) {
		values[n++] = 48;
		values[n++] = 2;
		values[n++] = gc->bg_rgb.r;
		values[n++] = gc->bg_rgb.g;
		values[n++] = gc->bg_rgb.b;
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
 * given a current state. The output buffer must be able to hold at least 57
 * bytes.
 */
void
grid_string_cells_code(const struct grid_cell *lastgc,
    const struct grid_cell *gc, char *buf, size_t len, int escape_c0)
{
	int	oldc[64], newc[64], s[128];
	size_t	noldc, nnewc, n, i;
	u_int	attr = gc->attr;
	u_int	lastattr = lastgc->attr;
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
		{ GRID_ATTR_HIDDEN, 8 }
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

	/* If the foreground colour changed, append its parameters. */
	nnewc = grid_string_cells_fg(gc, newc);
	noldc = grid_string_cells_fg(lastgc, oldc);
	if (nnewc != noldc || memcmp(newc, oldc, nnewc * sizeof newc[0]) != 0) {
		for (i = 0; i < nnewc; i++)
			s[n++] = newc[i];
	}

	/* If the background colour changed, append its parameters. */
	nnewc = grid_string_cells_bg(gc, newc);
	noldc = grid_string_cells_bg(lastgc, oldc);
	if (nnewc != noldc || memcmp(newc, oldc, nnewc * sizeof newc[0]) != 0) {
		for (i = 0; i < nnewc; i++)
			s[n++] = newc[i];
	}

	/* If there are any parameters, append an SGR code. */
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

	/* Append shift in/shift out if needed. */
	if ((attr & GRID_ATTR_CHARSET) && !(lastattr & GRID_ATTR_CHARSET)) {
		if (escape_c0)
			strlcat(buf, "\\016", len);  /* SO */
		else
			strlcat(buf, "\016", len);  /* SO */
	}
	if (!(attr & GRID_ATTR_CHARSET) && (lastattr & GRID_ATTR_CHARSET)) {
		if (escape_c0)
			strlcat(buf, "\\017", len);  /* SI */
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
 * Duplicate a set of lines between two grids. If there aren't enough lines in
 * either source or destination, the number of lines is limited to the number
 * available.
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
	grid_clear_lines(dst, dy, ny);

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

/* Copy a section of a line. */
void
grid_reflow_copy(struct grid_line *dst_gl, u_int to, struct grid_line *src_gl,
    u_int from, u_int to_copy)
{
	struct grid_cell_entry	*gce;
	u_int			 i, was;

	memcpy(&dst_gl->celldata[to], &src_gl->celldata[from],
	    to_copy * sizeof *dst_gl->celldata);

	for (i = to; i < to + to_copy; i++) {
		gce = &dst_gl->celldata[i];
		if (~gce->flags & GRID_FLAG_EXTENDED)
			continue;
		was = gce->offset;

		dst_gl->extddata = xreallocarray(dst_gl->extddata,
		    dst_gl->extdsize + 1, sizeof *dst_gl->extddata);
		gce->offset = dst_gl->extdsize++;
		memcpy(&dst_gl->extddata[gce->offset], &src_gl->extddata[was],
		    sizeof *dst_gl->extddata);
	}
}

/* Join line data. */
void
grid_reflow_join(struct grid *dst, u_int *py, struct grid_line *src_gl,
    u_int new_x)
{
	struct grid_line	*dst_gl = &dst->linedata[(*py) - 1];
	u_int			 left, to_copy, ox, nx;

	/* How much is left on the old line? */
	left = new_x - dst_gl->cellsize;

	/* Work out how much to append. */
	to_copy = src_gl->cellsize;
	if (to_copy > left)
		to_copy = left;
	ox = dst_gl->cellsize;
	nx = ox + to_copy;

	/* Resize the destination line. */
	dst_gl->celldata = xreallocarray(dst_gl->celldata, nx,
	    sizeof *dst_gl->celldata);
	dst_gl->cellsize = nx;

	/* Append as much as possible. */
	grid_reflow_copy(dst_gl, ox, src_gl, 0, to_copy);

	/* If there is any left in the source, split it. */
	if (src_gl->cellsize > to_copy) {
		dst_gl->flags |= GRID_LINE_WRAPPED;

		src_gl->cellsize -= to_copy;
		grid_reflow_split(dst, py, src_gl, new_x, to_copy);
	}
}

/* Split line data. */
void
grid_reflow_split(struct grid *dst, u_int *py, struct grid_line *src_gl,
    u_int new_x, u_int offset)
{
	struct grid_line	*dst_gl = NULL;
	u_int			 to_copy;

	/* Loop and copy sections of the source line. */
	while (src_gl->cellsize > 0) {
		/* Create new line. */
		if (*py >= dst->hsize + dst->sy)
			grid_scroll_history(dst);
		dst_gl = &dst->linedata[*py];
		(*py)++;

		/* How much should we copy? */
		to_copy = new_x;
		if (to_copy > src_gl->cellsize)
			to_copy = src_gl->cellsize;

		/* Expand destination line. */
		dst_gl->celldata = xreallocarray(NULL, to_copy,
		    sizeof *dst_gl->celldata);
		dst_gl->cellsize = to_copy;
		dst_gl->flags |= GRID_LINE_WRAPPED;

		/* Copy the data. */
		grid_reflow_copy(dst_gl, 0, src_gl, offset, to_copy);

		/* Move offset and reduce old line size. */
		offset += to_copy;
		src_gl->cellsize -= to_copy;
	}

	/* Last line is not wrapped. */
	if (dst_gl != NULL)
		dst_gl->flags &= ~GRID_LINE_WRAPPED;
}

/* Move line data. */
void
grid_reflow_move(struct grid *dst, u_int *py, struct grid_line *src_gl)
{
	struct grid_line	*dst_gl;

	/* Create new line. */
	if (*py >= dst->hsize + dst->sy)
		grid_scroll_history(dst);
	dst_gl = &dst->linedata[*py];
	(*py)++;

	/* Copy the old line. */
	memcpy(dst_gl, src_gl, sizeof *dst_gl);
	dst_gl->flags &= ~GRID_LINE_WRAPPED;

	/* Clear old line. */
	src_gl->celldata = NULL;
	src_gl->extddata = NULL;
}

/*
 * Reflow lines from src grid into dst grid of width new_x. Returns number of
 * lines fewer in the visible area. The source grid is destroyed.
 */
u_int
grid_reflow(struct grid *dst, struct grid *src, u_int new_x)
{
	u_int			 py, sy, line;
	int			 previous_wrapped;
	struct grid_line	*src_gl;

	py = 0;
	sy = src->sy;

	previous_wrapped = 0;
	for (line = 0; line < sy + src->hsize; line++) {
		src_gl = src->linedata + line;
		if (!previous_wrapped) {
			/* Wasn't wrapped. If smaller, move to destination. */
			if (src_gl->cellsize <= new_x)
				grid_reflow_move(dst, &py, src_gl);
			else
				grid_reflow_split(dst, &py, src_gl, new_x, 0);
		} else {
			/* Previous was wrapped. Try to join. */
			grid_reflow_join(dst, &py, src_gl, new_x);
		}
		previous_wrapped = (src_gl->flags & GRID_LINE_WRAPPED);
	}

	grid_destroy(src);

	if (py > sy)
		return (0);
	return (sy - py);
}
