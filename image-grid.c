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

/* Next image placement index. */
static uint64_t next_image_placement_index;

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
	uint64_t			 index;
	int				 z;
	struct image_spans		 spans;

	u_int				 x;
	u_int				 recover_from;

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

/* One image row to extend after a pane grows. */
struct image_recover_row {
	struct image_line	*line;
	u_int			 source_y;
};

/* A backend to draw an image. */
struct image_backend {
	const char	*name;
	int		(*get_flags)(struct tty *);
	int		(*select)(struct tty *);
	const struct grid_cell *(*map_cell)(struct tty *, struct grid *, u_int,
			    u_int, const struct grid_cell *, struct grid_cell *,
			    int *);
	void		(*draw_rect)(struct tty *, struct image_rect *);
	void		(*start_draw)(struct tty *, u_int, u_int, u_int, u_int);
	void		(*flush)(struct tty *);
	void		(*free)(struct tty *);
};

/* Backend definitions. */
static struct image_backend image_backend_fallback = {
	"fallback",
	fallback_get_flags,
	fallback_select,
	fallback_map_cell,
	NULL,
	NULL,
	NULL,
	NULL
};
static struct image_backend image_backend_sixel = {
	"sixel",
	sixel_get_flags,
	sixel_select,
	NULL,
	sixel_draw_rect,
	sixel_start_draw,
	sixel_flush_pending,
	sixel_free_pending
};

/* Backends in order of preference. */
static struct image_backend *image_backends[] = {
	&image_backend_sixel,
	&image_backend_fallback
};

/* Update the image backend. */
int
image_tty_update(struct tty *tty)
{
	struct image_backend	*backend;
	u_int			 i;

	for (i = 0; i < nitems(image_backends); i++) {
		backend = image_backends[i];
		if (backend->select(tty))
			break;
	}
	if (tty->image_backend == backend)
		return (0);

	if (tty->image_backend != NULL && tty->image_backend->free != NULL)
		tty->image_backend->free(tty);
	tty->image_data = NULL;
	tty->image_backend = backend;
	return (1);
}

/* Get the flags for a terminal's image backend. */
int
image_backend_get_flags(struct tty *tty)
{
	image_tty_update(tty);
	return (tty->image_backend->get_flags(tty));
}

/* Map an image grid cell to text for the selected backend. */
const struct grid_cell *
image_map_cell(struct tty *tty, struct grid *gd, u_int x, u_int y,
    const struct grid_cell *gc, struct grid_cell *out, int *empty)
{
	image_tty_update(tty);
	if (tty->image_backend->map_cell == NULL)
		return (gc);
	return (tty->image_backend->map_cell(tty, gd, x, y, gc, out, empty));
}

/* Draw a clipped part of one image span. */
static void
image_draw_span(struct image_backend *backend, struct tty *tty,
    struct image_span *span, u_int start, u_int end, u_int px, u_int atx,
    u_int aty)
{
	struct image_rect	 rect;

	rect.image = span->placement->image;
	rect.src_x = span->source_x + start - span->x;
	rect.src_y = span->source_y;
	rect.sx = end - start;
	rect.sy = 1;
	rect.dst_x = atx + start - px;
	rect.dst_y = aty;
	backend->draw_rect(tty, &rect);
}

/* Draw images in the visible part of one screen line. */
void
image_draw_line(struct tty *tty, struct screen *s, u_int px, u_int py,
    u_int nx, u_int atx, u_int aty)
{
	struct image_backend	*backend;
	struct image_line	*line;
	struct image_span	*span;
	u_int			 start, end, span_end;

	image_tty_update(tty);
	backend = tty->image_backend;
	if (backend->draw_rect == NULL)
		return;
	if (py >= s->grid->sy)
		return;
	line = s->grid->linedata[s->grid->hsize + py].images;
	if (line == NULL)
		return;
	end = px + nx;
	TAILQ_FOREACH(span, &line->spans, line_entry) {
		span_end = span->x + span->sx;
		start = span->x;
		if (start < px)
			start = px;
		if (start >= end || span_end <= px)
			continue;
		if (span_end > end)
			span_end = end;
		image_draw_span(backend, tty, span, start, span_end, px, atx,
		    aty);
	}
}

/* Clear old image pixels before redrawing a terminal area. */
void
image_redraw_start(struct tty *tty, u_int x, u_int y, u_int w, u_int h)
{
	image_tty_update(tty);
	if (tty->image_backend->start_draw != NULL)
		tty->image_backend->start_draw(tty, x, y, w, h);
}

/* Write pending image output to the terminal. */
void
image_draw_flush(struct tty *tty)
{
	image_tty_update(tty);
	if (tty->image_backend->flush != NULL)
		tty->image_backend->flush(tty);
}

/* Discard image backend state after a terminal size change. */
void
image_tty_size_changed(struct tty *tty)
{
	if (!image_tty_update(tty)) {
		if (tty->image_backend->free != NULL)
			tty->image_backend->free(tty);
	}
}

/* Free image backend state for a terminal. */
void
image_tty_free(struct tty *tty)
{
	if (tty->image_backend != NULL) {
		if (tty->image_backend->free != NULL)
			tty->image_backend->free(tty);
		tty->image_backend = NULL;
		tty->image_data = NULL;
	}
}

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
image_placement_create(struct grid *gd, struct image *im, int z)
{
	struct image_store	*store = image_store_get(gd);
	struct image_placement	*plmt;

	plmt = xcalloc(1, sizeof *plmt);
	plmt->store = store;
	plmt->image = im;
	plmt->index = ++next_image_placement_index;
	plmt->z = z;
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
	struct image_geom	*ig;
	struct image_placement	*plmt;
	struct image_line	*line;
	u_int			 rows = gd->hsize + gd->sy, row;

	if (sx == 0 || sy == 0 || x >= gd->sx || y >= rows)
		return;
	ig = image_get_geometry(im);
	if (src_x >= ig->sx || src_y >= ig->sy)
		return;

	if (sx > gd->sx - x)
		sx = gd->sx - x;
	if (sx > ig->sx - src_x)
		sx = ig->sx - src_x;
	if (sy > rows - y)
		sy = rows - y;
	if (sy > ig->sy - src_y)
		sy = ig->sy - src_y;

	plmt = image_placement_create(gd, im, 0);
	for (row = 0; row < sy; row++) {
		line = image_line_get(&gd->linedata[y + row]);
		image_span_add(line, plmt, x, sx, src_x, src_y + row);
	}
}

/* Remove the covered part of one span. */
static void
image_span_remove(struct image_span *span, u_int x, u_int end)
{
	u_int	span_end = span->x + span->sx;

	if (span_end <= x || span->x >= end)
		return;
	if (span->x < x && span_end > end) {
		span->sx = x - span->x;
		image_span_add(span->line, span->placement, end, span_end - end,
		    span->source_x + end - span->x, span->source_y);
		return;
	}
	if (span->x < x) {
		span->sx = x - span->x;
		return;
	}
	if (span_end > end) {
		span->source_x += end - span->x;
		span->sx = span_end - end;
		span->x = end;
		return;
	}
	image_span_free(span);
}

/* Remove all or only overwritten image spans in a range of cells. */
static void
image_line_remove(struct image_line *line, u_int x, u_int w, int overwrite)
{
	struct image_span	*span, *next;
	enum image_type		 type;
	u_int			 end;

	if (line == NULL || w == 0)
		return;
	end = x + w;
	if (end < x)
		end = UINT_MAX;
	TAILQ_FOREACH_SAFE(span, &line->spans, line_entry, next) {
		type = image_get_type(span->placement->image);
		if (overwrite && type == IMAGE_TYPE_KITTY)
			continue;
		image_span_remove(span, x, end);
	}
}

/* Remove SIXEL spans overwritten by text writes or erases. */
void
image_grid_overwrite(struct grid *gd, u_int x, u_int y, u_int sx, u_int sy)
{
	u_int	rows = gd->hsize + gd->sy;
	u_int	row;

	if (gd->images == NULL || sx == 0 || sy == 0 || y >= rows)
		return;
	if (sy > rows - y)
		sy = rows - y;
	for (row = y; row < y + sy; row++)
		image_line_remove(gd->linedata[row].images, x, sx, 1);
	image_store_prune(gd->images);
}

/* Check whether a placement has any spans in the visible grid. */
static int
image_placement_visible(struct grid *gd, struct image_placement *plmt)
{
	struct image_line	*line;
	struct image_span	*span;
	u_int			 y;

	for (y = 0; y < gd->sy; y++) {
		line = gd->linedata[gd->hsize + y].images;
		if (line == NULL)
			continue;
		TAILQ_FOREACH(span, &line->spans, line_entry) {
			if (span->placement == plmt && span->x < gd->sx)
				return (1);
		}
	}
	return (0);
}

/* Check whether a grid retains any placements of an image. */
int
image_grid_has_image(struct grid *gd, u_int id)
{
	struct image_placement	*plmt;

	if (gd->images == NULL)
		return (0);
	TAILQ_FOREACH(plmt, &gd->images->placements, entry) {
		if (image_get_id(plmt->image) == id)
			return (1);
	}
	return (0);
}

/* Remove placements of one image or all images of a type from a grid. */
u_int
image_grid_clear(struct grid *gd, enum image_type type, u_int id, int visible)
{
	struct image_placement	*plmt, *plmt1;
	struct image_span	*span, *span1;
	u_int			 removed = 0;

	if (gd->images == NULL)
		return (0);
	TAILQ_FOREACH_SAFE(plmt, &gd->images->placements, entry, plmt1) {
		if (image_get_type(plmt->image) != type)
			continue;
		if (id != 0 && image_get_id(plmt->image) != id)
			continue;
		if (visible && !image_placement_visible(gd, plmt))
			continue;
		TAILQ_FOREACH_SAFE(span, &plmt->spans, placement_entry, span1)
			image_span_free(span);
		image_placement_free(plmt);
		removed++;
	}
	return (removed);
}

/* Remove placements of an image from a screen. */
void
image_clear(struct screen_write_ctx *ctx, u_int id)
{
	struct image	*im = image_find(id);
	enum image_type	 type;

	if (im == NULL)
		return;
	type = image_get_type(im);
	image_grid_clear(ctx->s->grid, type, id, 0);
	if (ctx->s->saved_grid != NULL)
		image_grid_clear(ctx->s->saved_grid, type, id, 0);
	if (ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
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
		span->placement->recover_from = 0;
		n++;
	}
	image_line_remove(line, px, nx, 0);
	image_line_remove(line, dx, nx, 0);
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
	plmt = image_placement_create(dst, src->image, src->z);
	plmt->x = src->x;
	plmt->recover_from = src->recover_from;

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

/* Copy clipped image spans between grid areas. */
void
image_grid_copy_area(struct grid *dst, u_int dx, u_int dy, struct grid *src,
    u_int px, u_int py, u_int sx, u_int sy)
{
	struct image_copy_map	*maps = NULL;
	struct image_placement	*plmt;
	struct image_line	*src_line, *dst_line;
	struct grid_line	*gl;
	struct image_span	*span;
	u_int			 nmaps = 0, row, start, end, span_end;

	if (dst == src || sx == 0 || sy == 0)
		return;
	if (dy >= dst->hsize + dst->sy || py >= src->hsize + src->sy)
		return;
	if (sy > dst->hsize + dst->sy - dy)
		sy = dst->hsize + dst->sy - dy;
	if (sy > src->hsize + src->sy - py)
		sy = src->hsize + src->sy - py;
	end = px + sx;
	if (end < px)
		end = UINT_MAX;

	for (row = 0; row < sy; row++) {
		src_line = src->linedata[py + row].images;
		if (src_line == NULL)
			continue;
		dst_line = NULL;
		TAILQ_FOREACH(span, &src_line->spans, line_entry) {
			span_end = span->x + span->sx;
			start = (span->x > px ? span->x : px);
			if (start >= end || span_end <= px)
				continue;
			if (span_end > end)
				span_end = end;
			if (dst_line == NULL) {
				gl = &dst->linedata[dy + row];
				dst_line = image_line_get(gl);
			}
			plmt = image_copy_placement(dst, span->placement, &maps,
			    &nmaps);
			plmt->recover_from = 0;
			image_span_add(dst_line, plmt, dx + start - px,
			    span_end - start, span->source_x + start - span->x,
			    span->source_y);
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
	struct screen		*s = ctx->s;
	struct window_pane	*wp = ctx->wp;
	u_int			 sx = screen_size_x(s);
	u_int			 sy = s->rlower - s->rupper + 1;

	if (wp == NULL)
		return;
	if (!image_grid_area_has_images(s->grid, 0,
	    s->grid->hsize + s->rupper, sx, sy))
		return;
	redraw_damage_scroll(wp->window, wp->xoff,
	    wp->yoff + s->rupper, sx, sy);
}

/* Redraw images under changed cells. */
void
image_damage_screen(struct screen_write_ctx *ctx, u_int x, u_int y, u_int sx,
    u_int sy)
{
	struct screen	*s = ctx->s;

	image_redraw_area(ctx, x, y, sx, sy);
	if (ctx->flags & SCREEN_WRITE_INPUT)
		image_grid_overwrite(s->grid, x, s->grid->hsize + y, sx, sy);
}

/* Check whether an image cell has any visible pixels. */
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

/* Return whether a new cell's pixels completely cover an old cell. */
static int
image_cell_covers(struct image *new, u_int nx, u_int ny, struct image *old,
    u_int ox, u_int oy)
{
	struct image_geom	*ng = image_get_geometry(new);
	struct image_geom	*og = image_get_geometry(old);
	u_int			 npx, npy, nsx, nsy, opx, opy, osx, osy, xx, yy;
	const u_char		*np, *op, *npixels, *opixels;
	size_t			 nstride, ostride;

	if (ng->canvas_width / ng->sx != og->canvas_width / og->sx ||
	    ng->canvas_height / ng->sy != og->canvas_height / og->sy)
		return (0);
	image_get_pixel_area(new, nx, ny, 1, 1, &npx, &npy, &nsx, &nsy);
	image_get_pixel_area(old, ox, oy, 1, 1, &opx, &opy, &osx, &osy);

	npixels = image_get_pixels(new, &nstride, NULL);
	opixels = image_get_pixels(old, &ostride, NULL);
	for (yy = 0; yy < osy; yy++) {
		op = opixels + (opy + yy) * ostride + opx * 4;
		if (yy < nsy) {
			np = npixels + (npy + yy) * nstride + npx * 4;
		}
		for (xx = 0; xx < osx; xx++) {
			if (op[xx * 4 + 3] == 0)
				continue;
			if (yy >= nsy || xx >= nsx || np[xx * 4 + 3] != 255)
				return (0);
		}
	}
	return (1);
}

/* Discard older SIXEL cells fully covered by a new SIXEL image. */
static void
image_line_cover(struct image_line *line, struct image_placement *plmt,
    u_int x, u_int src_x, u_int src_y)
{
	struct image_span	*span, *next;
	struct image_placement	*old;

	TAILQ_FOREACH_SAFE(span, &line->spans, line_entry, next) {
		old = span->placement;
		if (old == plmt)
			continue;
		if (image_get_type(old->image) != IMAGE_TYPE_SIXEL)
			continue;
		if (x < span->x || x >= span->x + span->sx)
			continue;
		if (!image_cell_covers(plmt->image, src_x, src_y,
		    old->image, span->source_x + x - span->x,
		    span->source_y))
			continue;
		image_span_remove(span, x, x + 1);
	}
}

/* Attach the visible runs from one image row. */
static void
image_add_row(struct image_line *line, struct image_placement *plmt,
    u_int cx, u_int src_y, u_int sx)
{
	struct image	*im = plmt->image;
	u_int		 x, i, run;

	for (x = 0; x < sx; x += run) {
		if (!image_cell_has_visible_pixels(im, x, src_y)) {
			run = 1;
			continue;
		}
		for (run = 1; x + run < sx; run++) {
			if (!image_cell_has_visible_pixels(im, x + run, src_y))
				break;
		}

		if (image_get_type(im) != IMAGE_TYPE_SIXEL) {
			image_span_add(line, plmt, cx + x, run, x, src_y);
			continue;
		}
		for (i = 0; i < run; i++)
			image_line_cover(line, plmt, cx + x + i, x + i, src_y);
		image_span_add(line, plmt, cx + x, run, x, src_y);
	}
}

/* Add newly visible columns from one image row. */
static void
image_extend_row(struct image_line *line, struct image_placement *plmt,
    u_int cx, u_int src_y, u_int from, u_int to)
{
	struct image	*im = plmt->image;
	u_int		 x, run;

	for (x = from; x < to; x += run) {
		if (!image_cell_has_visible_pixels(im, x, src_y)) {
			run = 1;
			continue;
		}
		for (run = 1; x + run < to; run++) {
			if (!image_cell_has_visible_pixels(im, x + run, src_y))
				break;
		}
		image_span_add(line, plmt, cx + x, run, x, src_y);
	}
}

/* Extend image rows when grid is resized. */
void
image_grid_resize_width(struct grid *gd, u_int new_sx)
{
	struct image_geom		*ig;
	struct image_placement		*plmt;
	struct image_span		*span;
	struct image_recover_row	*rows;
	u_int				 from, to;
	u_int				 nrows, i;

	if (gd->images == NULL)
		return;
	TAILQ_FOREACH(plmt, &gd->images->placements, entry) {
		from = plmt->recover_from;
		if (from == 0 || plmt->x >= new_sx)
			continue;
		ig = image_get_geometry(plmt->image);

		to = new_sx - plmt->x;
		if (to > ig->sx)
			to = ig->sx;
		if (to <= from)
			continue;

		rows = NULL;
		nrows = 0;
		TAILQ_FOREACH(span, &plmt->spans, placement_entry) {
			for (i = 0; i < nrows; i++) {
				if (rows[i].line == span->line)
					break;
			}
			if (i != nrows)
				continue;
			rows = xreallocarray(rows, nrows + 1, sizeof *rows);
			rows[nrows].line = span->line;
			rows[nrows].source_y = span->source_y;
			nrows++;
		}
		for (i = 0; i < nrows; i++) {
			image_extend_row(rows[i].line, plmt, plmt->x,
			    rows[i].source_y, from, to);
		}
		free(rows);
		plmt->recover_from = (to == ig->sx ? 0 : to);
	}
}

/* Place an image at the cursor. */
void
image_write(struct screen_write_ctx *ctx, struct image *im, u_int bg,
    int no_cursor)
{
	struct image_geom	*ig = image_get_geometry(im);
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct image_placement	*plmt;
	struct grid_line	*gl;
	struct image_line	*line;
	u_int			 cx = s->cx, cy = s->cy, y, lines;
	u_int			 sx = ig->sx, sy = ig->sy;
	u_int			 cursor_x = 0, cursor_rows;
	u_int			 origin_y = 0, h_origin_y, region_height;
	u_int			 remaining, n;

	if (sx > screen_size_x(s) - cx)
		sx = screen_size_x(s) - cx;
	if (sx == 0)
		return;

	/* Kitty moves to the last image row; SIXEL advances past it. */
	cursor_rows = sy;
	if (image_get_type(im) == IMAGE_TYPE_KITTY)
		cursor_rows--;

	/* Scroll images that move the cursor. */
	if (no_cursor) {
		if (sy > screen_size_y(s) - cy)
			sy = screen_size_y(s) - cy;
	} else if (screen_size_y(s) - cy <= cursor_rows) {
		lines = cursor_rows - (screen_size_y(s) - cy) + 1;
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
		cursor_rows -= origin_y;
	}
	plmt = image_placement_create(gd, im, 0);
	if (sx < ig->sx) {
		plmt->x = cx;
		plmt->recover_from = sx;
	}

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

	/* Schedule a redraw and move the cursor. */
	image_store_prune(gd->images);
	image_redraw_area(ctx, cx, cy, sx, sy);
	if (!no_cursor) {
		if (image_get_type(im) == IMAGE_TYPE_KITTY)
			cursor_x = cx + sx;
		screen_write_cursormove(ctx, cursor_x, cy + cursor_rows, 0);
	}
}

/* Find the cell within an image at a grid position. */
int
image_grid_get_at(struct grid *gd, u_int x, u_int y, struct image **im,
    u_int *src_x, u_int *src_y)
{
	struct image_line	*line;
	struct image_span	*span, *found = NULL;

	if (y >= gd->hsize + gd->sy)
		return (0);
	line = gd->linedata[y].images;
	if (line == NULL)
		return (0);
	TAILQ_FOREACH(span, &line->spans, line_entry) {
		if (x >= span->x && x < span->x + span->sx)
			found = span;
	}
	if (found == NULL)
		return (-1);
	*im = found->placement->image;
	*src_x = found->source_x + x - found->x;
	*src_y = found->source_y;
	return (1);
}
