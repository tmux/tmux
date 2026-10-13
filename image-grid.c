/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Michael Grant <mgrant@grant.org>
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

#include <limits.h>
#include <stdlib.h>

#include "tmux.h"

/* A run of cells on one grid line. */
struct image_span {
	u_int			 x;
	u_int			 sx;
	u_int			 source_x;
	u_int			 source_y;

	struct image_line	*line;
	struct image_placement	*placement;

	TAILQ_ENTRY(image_span)	 line_entry;
	TAILQ_ENTRY(image_span)	 placement_entry;
};
TAILQ_HEAD(image_spans, image_span);

/* Spans on one grid line. */
struct image_line {
	struct image_spans	 spans;
};

/* A placement of an image, may be across multiple lines */
struct image_placement {
	struct image_store		*store;
	struct image			*image;
	struct image_spans		 spans;

	TAILQ_ENTRY(image_placement)	 entry;
};
TAILQ_HEAD(image_placements, image_placement);

/* Placements owned by one grid. */
struct image_store {
	struct image_placements	 placements;
};

/* A shared placements copied when duplicating lines. */
struct image_copy_map {
	struct image_placement	*source;
	struct image_placement	*destination;
};

/* Part of a span waiting to be moved. */
struct image_move {
	struct image_placement	*placement;
	u_int			 x;
	u_int			 sx;
	u_int			 source_x;
	u_int			 source_y;
};

/* Get the placement store for a grid. */
static struct image_store *
image_store_get(struct grid *gd)
{
	if (gd->images == NULL) {
		gd->images = xcalloc(1, sizeof *gd->images);
		TAILQ_INIT(&gd->images->placements);
	}
	return (gd->images);
}

/* Get the spans for a grid line. */
static struct image_line *
image_line_get(struct grid_line *gl)
{
	if (gl->images == NULL) {
		gl->images = xcalloc(1, sizeof *gl->images);
		TAILQ_INIT(&gl->images->spans);
	}
	return (gl->images);
}

/* Create a placement. Takes a reference to the image. */
static struct image_placement *
image_placement_create(struct grid *gd, struct image *im)
{
	struct image_store	*store = image_store_get(gd);
	struct image_placement	*plmt;

	plmt = xcalloc(1, sizeof *plmt);
	plmt->store = store;
	plmt->image = im;
	TAILQ_INIT(&plmt->spans);
	TAILQ_INSERT_TAIL(&store->placements, plmt, entry);
	image_ref(im);
	return (plmt);
}

/* Free a placement after all its spans have gone. */
static void
image_placement_free(struct image_placement *plmt)
{
	TAILQ_REMOVE(&plmt->store->placements, plmt, entry);
	image_free(plmt->image);
	free(plmt);
}

/* Attach a span to both its grid line and its placement. */
static void
image_span_add(struct image_line *line, struct image_placement *plmt, u_int x,
    u_int sx, u_int src_x, u_int src_y)
{
	struct image_span	*span;

	if (sx == 0)
		return;

	span = xcalloc(1, sizeof *span);
	span->x = x;
	span->sx = sx;
	span->source_x = src_x;
	span->source_y = src_y;
	span->line = line;
	span->placement = plmt;
	TAILQ_INSERT_TAIL(&line->spans, span, line_entry);
	TAILQ_INSERT_TAIL(&plmt->spans, span, placement_entry);
}

/* Detach and free one span. */
static void
image_span_free(struct image_span *span)
{
	TAILQ_REMOVE(&span->line->spans, span, line_entry);
	TAILQ_REMOVE(&span->placement->spans, span, placement_entry);
	free(span);
}

/* Free placements with no remaining spans. */
static void
image_store_prune(struct image_store *store)
{
	struct image_placement	*plmt, *next;

	if (store == NULL)
		return;
	TAILQ_FOREACH_SAFE(plmt, &store->placements, entry, next) {
		if (TAILQ_EMPTY(&plmt->spans))
			image_placement_free(plmt);
	}
}

/* Add a source rectangle at absolute grid coordinates. */
void
image_grid_add(struct grid *gd, struct image *im, u_int x, u_int y,
    u_int src_x, u_int src_y, u_int sx, u_int sy)
{
	struct image_placement	*plmt;
	struct image_line	*line;
	u_int			 rows = gd->hsize + gd->sy, row;
	u_int			 image_sx, image_sy;

	if (sx == 0 || sy == 0 || x >= gd->sx || y >= rows)
		return;
	image_get_size_in_cells(im, &image_sx, &image_sy);
	if (src_x >= image_sx || src_y >= image_sy)
		return;

	if (sx > gd->sx - x)
		sx = gd->sx - x;
	if (sx > image_sx - src_x)
		sx = image_sx - src_x;
	if (sy > rows - y)
		sy = rows - y;
	if (sy > image_sy - src_y)
		sy = image_sy - src_y;

	plmt = image_placement_create(gd, im);
	for (row = 0; row < sy; row++) {
		line = image_line_get(&gd->linedata[y + row]);
		image_span_add(line, plmt, x, sx, src_x, src_y + row);
	}
}

/* Remove the covered part of a span. */
static void
image_line_remove(struct image_line *line, u_int x, u_int w)
{
	struct image_span	*span, *next;
	u_int			 end, span_end, right;

	if (line == NULL || w == 0)
		return;
	end = x + w;
	if (end < x)
		end = UINT_MAX;
	TAILQ_FOREACH_SAFE(span, &line->spans, line_entry, next) {
		span_end = span->x + span->sx;
		if (span_end <= x || span->x >= end)
			continue;
		if (span->x < x && span_end > end) {
			right = span_end - end;
			span->sx = x - span->x;
			image_span_add(line, span->placement, end, right,
			    span->source_x + end - span->x, span->source_y);
			continue;
		}
		if (span->x < x) {
			span->sx = x - span->x;
			continue;
		}
		if (span_end > end) {
			span->source_x += end - span->x;
			span->sx = span_end - end;
			span->x = end;
			continue;
		}
		image_span_free(span);
	}
}

/* Remove a rectangle of image spans from a grid. */
void
image_grid_remove(struct grid *gd, u_int x, u_int y, u_int sx, u_int sy)
{
	u_int	rows = gd->hsize + gd->sy;
	u_int	row;

	if (gd->images == NULL || sx == 0 || sy == 0 || y >= rows)
		return;
	if (sy > rows - y)
		sy = rows - y;
	for (row = y; row < y + sy; row++)
		image_line_remove(gd->linedata[row].images, x, sx);
	image_store_prune(gd->images);
}

/* Free all spans on a grid line. */
void
image_grid_free_line(struct grid *gd, struct grid_line *gl)
{
	struct image_line	*line = gl->images;
	struct image_span	*span, *next;

	if (line == NULL)
		return;

	TAILQ_FOREACH_SAFE(span, &line->spans, line_entry, next)
		image_span_free(span);
	free(line);

	gl->images = NULL;
	image_store_prune(gd->images);
}

/* Free a grid's empty placement store. */
void
image_grid_free(struct grid *gd)
{
	if (gd->images != NULL) {
		image_store_prune(gd->images);
		free(gd->images);
		gd->images = NULL;
	}
}

/* Move the image spans belonging to a range of text cells. */
void
image_grid_move_cells(struct grid *gd, u_int dx, u_int px, u_int py, u_int nx)
{
	struct image_line	*line;
	struct image_span	*span;
	struct image_move	*moves = NULL;
	size_t			 n = 0, i;
	u_int			 start, end, span_end;

	if (gd->images == NULL ||
	    nx == 0 ||
	    px == dx ||
	    py >= gd->hsize + gd->sy)
		return;
	line = gd->linedata[py].images;
	if (line == NULL)
		return;

	end = px + nx;
	TAILQ_FOREACH(span, &line->spans, line_entry) {
		span_end = span->x + span->sx;
		start = (span->x > px ? span->x : px);
		if (start >= end || span_end <= px)
			continue;
		if (span_end > end)
			span_end = end;

		moves = xreallocarray(moves, n + 1, sizeof *moves);
		moves[n].placement = span->placement;
		moves[n].x = dx + start - px;
		moves[n].sx = span_end - start;
		moves[n].source_x = span->source_x + start - span->x;
		moves[n].source_y = span->source_y;
		n++;
	}
	image_line_remove(line, px, nx);
	image_line_remove(line, dx, nx);
	for (i = 0; i < n; i++) {
		image_span_add(line, moves[i].placement, moves[i].x,
		    moves[i].sx, moves[i].source_x, moves[i].source_y);
	}
	free(moves);
	image_store_prune(gd->images);
}

/* Map one source placement to one destination placement during a copy. */
static struct image_placement *
image_copy_placement(struct grid *dst, struct image_placement *src,
    struct image_copy_map **maps, u_int *nmaps)
{
	struct image_placement	*plmt;
	struct image_copy_map	*map = *maps;
	u_int			 i, n = *nmaps;

	for (i = 0; i < n; i++) {
		if (map[i].source == src)
			return (map[i].destination);
	}
	plmt = image_placement_create(dst, src->image);

	map = xreallocarray(map, n + 1, sizeof *map);
	map[n].source = src;
	map[n].destination = plmt;

	*maps = map;
	*nmaps = n + 1;
	return (plmt);
}

/* Duplicate image spans and their placements between grid lines. */
void
image_grid_duplicate_lines(struct grid *dst, u_int dy, struct grid *src,
    u_int sy, u_int ny)
{
	struct image_copy_map	*maps = NULL;
	struct image_line	*src_line, *dst_line;
	struct image_span	*span;
	struct image_placement	*plmt;
	u_int			 nmaps = 0, row;

	for (row = 0; row < ny; row++) {
		src_line = src->linedata[sy + row].images;
		if (src_line == NULL)
			continue;
		dst_line = image_line_get(&dst->linedata[dy + row]);
		TAILQ_FOREACH(span, &src_line->spans, line_entry) {
			plmt = image_copy_placement(dst, span->placement, &maps,
			    &nmaps);
			image_span_add(dst_line, plmt, span->x, span->sx,
			    span->source_x, span->source_y);
		}
	}
	free(maps);
}

/* Check whether a grid line has any image spans. */
int
image_grid_line_has_images(struct grid_line *gl)
{
	return (gl->images != NULL && !TAILQ_EMPTY(&gl->images->spans));
}

/* Check whether a grid area has any image spans. */
int
image_grid_area_has_images(struct grid *gd, u_int x, u_int y, u_int sx,
    u_int sy)
{
	struct image_line	*line;
	struct image_span	*span;
	u_int			 row, end;

	if (gd->images == NULL)
		return (0);
	if (sx == 0 || sy == 0 || y >= gd->hsize + gd->sy)
		return (0);

	end = x + sx;
	if (end < x)
		end = UINT_MAX;
	if (sy > gd->hsize + gd->sy - y)
		sy = gd->hsize + gd->sy - y;

	for (row = y; row < y + sy; row++) {
		line = gd->linedata[row].images;
		if (line == NULL)
			continue;
		TAILQ_FOREACH(span, &line->spans, line_entry) {
			if (span->x < end && span->x + span->sx > x)
				return (1);
		}
	}
	return (0);
}

/* Record window damage when an area containing images changes. */
void
image_redraw_area(struct screen_write_ctx *ctx, u_int x, u_int y, u_int sx,
    u_int sy)
{
	struct window_pane	*wp = ctx->wp;
	struct grid		*gd = ctx->s->grid;

	if (wp == NULL)
		return;
	if (!image_grid_area_has_images(gd, x, gd->hsize + y, sx, sy))
		return;
	redraw_damage_window(wp->window, wp->xoff + x, wp->yoff + y, sx, sy);
}

/* Record window damage for all visible images. */
void
image_redraw_all(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	u_int		 sx = screen_size_x(s), sy = screen_size_y(s);

	image_redraw_area(ctx, 0, 0, sx, sy);
}

/* Record window damage for images in the scroll region. */
void
image_redraw_scroll(struct screen_write_ctx *ctx)
{
	struct screen	*s = ctx->s;
	u_int		 sx = screen_size_x(s), sy = s->rlower - s->rupper + 1;

	image_redraw_area(ctx, 0, s->rupper, sx, sy);
}

/* Redraw images under changed cells. */
void
image_damage_screen(struct screen_write_ctx *ctx, u_int x, u_int y, u_int sx,
    u_int sy)
{
	struct screen	*s = ctx->s;

	image_redraw_area(ctx, x, y, sx, sy);
	if (ctx->flags & SCREEN_WRITE_INPUT)
		image_grid_remove(s->grid, x, s->grid->hsize + y, sx, sy);
}

/* Return whether an image cell has any visible pixels. */
static int
image_cell_has_visible_pixels(struct image *im, u_int x, u_int y)
{
	u_int		 px, py, sx, sy, xx, yy;
	const u_char	*pixels;
	size_t		 stride;

	image_get_pixel_area(im, x, y, 1, 1, &px, &py, &sx, &sy);
	if (sx == 0 || sy == 0)
		return (0);
	pixels = image_get_pixels(im, &stride, NULL);
	for (yy = py; yy < py + sy; yy++) {
		for (xx = px; xx < px + sx; xx++) {
			if (pixels[yy * stride + xx * 4 + 3] != 0)
				return (1);
		}
	}
	return (0);
}

/* Attach the visible runs from one image row. */
static void
image_add_row(struct image_line *line, struct image_placement *plmt,
    u_int cx, u_int src_y, u_int sx)
{
	struct image	*im = plmt->image;
	u_int		 x, run;

	for (x = 0; x < sx; x += run) {
		if (!image_cell_has_visible_pixels(im, x, src_y)) {
			run = 1;
			continue;
		}
		for (run = 1; x + run < sx; run++) {
			if (!image_cell_has_visible_pixels(im, x + run, src_y))
				break;
		}
		image_span_add(line, plmt, cx + x, run, x, src_y);
	}
}

/* Place a SIXEL image at the cursor. */
void
image_write(struct screen_write_ctx *ctx, struct image *im, u_int bg)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct image_placement	*plmt;
	struct grid_line	*gl;
	struct image_line	*line;
	u_int			 cx = s->cx, cy = s->cy, y, sx, sy, lines;
	u_int			 origin_y = 0, h_origin_y, region_height;
	u_int			 remaining, n;

	image_get_size_in_cells(im, &sx, &sy);
	if (sx > screen_size_x(s) - cx)
		sx = screen_size_x(s) - cx;
	if (sx == 0)
		return;

	/* Scroll images that move the cursor. */
	if (screen_size_y(s) - cy <= sy) {
		lines = sy - (screen_size_y(s) - cy) + 1;
		region_height = s->rlower - s->rupper + 1;

		remaining = lines;
		while (remaining > 0) {
			if (remaining < region_height)
				n = remaining;
			else
				n = region_height;
			screen_write_scrollup(ctx, n, bg);
			remaining -= n;
		}

		if (lines > cy) {
			origin_y = lines - cy;
			screen_write_cursormove(ctx, -1, 0, 0);
		} else {
			origin_y = 0;
			screen_write_cursormove(ctx, -1, cy - lines, 0);
		}
		cy = s->cy;
		sy -= origin_y;
	}
	plmt = image_placement_create(gd, im);

	/* Attach scrolled image rows to the available history lines. */
	h_origin_y = origin_y;
	if (h_origin_y > gd->hsize)
		h_origin_y = gd->hsize;
	for (y = 0; y < h_origin_y; y++) {
		gl = &gd->linedata[gd->hsize - h_origin_y + y];
		line = image_line_get(gl);
		image_add_row(line, plmt, cx, origin_y - h_origin_y + y, sx);
	}

	/* Attach visible image rows. */
	for (y = 0; y < sy; y++) {
		line = image_line_get(&gd->linedata[gd->hsize + cy + y]);
		image_add_row(line, plmt, cx, origin_y + y, sx);
	}

	/* Schedule a redraw and advance the cursor past the image. */
	image_store_prune(gd->images);
	image_redraw_area(ctx, cx, cy, sx, sy);
	screen_write_cursormove(ctx, 0, cy + sy, 0);
}
