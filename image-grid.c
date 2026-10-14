/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <string.h>

#include "tmux.h"

#define IMAGE_Z_BELOW_BACKGROUND (INT32_MIN / 2)

/* One contiguous row of a placement in the grid. */
struct image_span {
	u_int			 x;		/* Owning grid column. */
	u_int			 sx;		/* Number of image cells. */
	u_int			 source_x;	/* Origin in image cells. */
	u_int			 source_y;
	struct image_line	*line;
	struct image_placement	*placement;
	TAILQ_ENTRY(image_span)	 line_entry;
	TAILQ_ENTRY(image_span)	 placement_entry;
};
TAILQ_HEAD(image_spans, image_span);

/* Image spans attached to one grid line. */
struct image_line {
	struct image_spans	 spans;
};

enum image_input {
	IMAGE_INPUT_ALL = -1,
	IMAGE_INPUT_OVERWRITTEN = -2,
	IMAGE_INPUT_SIXEL,
	IMAGE_INPUT_KITTY
};

/* One logical image placement, shared by all of its row spans. */
struct image_placement {
	struct image_store	*store;
	struct image		*image;
	enum image_input	 input;
	int			 placeholder;	/* Uses Unicode placeholders. */
	u_int			 app_image_id;
	u_int			 app_placement_id;
	int32_t			 z;
	uint64_t		 serial;	/* Layer order. */
	u_int			 x;
	u_int			 recover_from;
	struct image_spans	 spans;
	TAILQ_ENTRY(image_placement) entry;
};
TAILQ_HEAD(image_placements, image_placement);

/* One image row to extend after a pane grows. */
struct image_recover_row {
	struct image_line	*line;
	u_int			 source_y;
};

/* Placements belonging to one grid. */
struct image_store {
	uint64_t		 next_serial;
	struct image_placements	 placements;
};

/* Spans saved while overlapping grid cells are moved. */
struct image_move {
	struct image_placement	*placement;
	u_int			 x;		/* Destination grid column. */
	u_int			 sx;		/* Number of image cells. */
	u_int			 source_x;	/* Origin in image cells. */
	u_int			 source_y;
};

/* Pair source and destination placements during one grid copy. */
struct image_placement_map {
	struct image_placement	*source;
	struct image_placement	*destination;
};

/* Placement mappings kept only for the duration of a grid copy. */
struct image_copy_ctx {
	struct grid			*destination;
	struct image_placement_map	*maps;
	u_int				 count;	/* Placement mapping count. */
};

/* A backend to draw an image. */
struct image_backend {
	const char	*name;
	int		(*get_flags)(struct tty *);
	int		(*select)(struct tty *);
	const struct grid_cell *(*map_cell)(struct tty *, struct grid *, u_int,
			    u_int, const struct grid_cell *, struct grid_cell *,
			    int *);
	void		(*draw_rect)(struct tty *, const struct image_rect *);
	void		(*start_draw)(struct tty *, u_int, u_int, u_int, u_int);
	void		(*flush)(struct tty *);
	void		(*finish_draw)(struct tty *);
	void		(*free)(struct tty *, int);
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
	NULL,
	sixel_free_pending
};

/* Kitty backend. */
static struct image_backend image_backend_kitty = {
	"kitty",
	kitty_get_flags,
	kitty_select,
	NULL,
	kitty_draw_rect,
	kitty_redraw_start,
	NULL,
	kitty_redraw_finish,
	kitty_free_output_state
};

/* Backends in order of preference. */
static struct image_backend *image_backends[] = {
	&image_backend_kitty,
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
		tty->image_backend->free(tty, !!(tty->flags & TTY_OPENED));
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
			tty->image_backend->free(tty, !!(tty->flags & TTY_OPENED));
	}
}

/* Free image backend state for a terminal. */
void
image_tty_free(struct tty *tty, int send)
{
	if (tty->image_backend != NULL) {
		if (tty->image_backend->free != NULL)
			tty->image_backend->free(tty, send);
		tty->image_backend = NULL;
		tty->image_data = NULL;
	}
}

/* Finish drawing image placements for this terminal. */
void
image_redraw_finish(struct tty *tty)
{
	if (tty->image_backend->finish_draw != NULL)
		tty->image_backend->finish_draw(tty);
}

/* Return the ordering band for a placement. */
static int
image_placement_band(const struct image_placement *placement)
{
	if (placement->input == IMAGE_INPUT_SIXEL)
		return (1);
	if (placement->z < 0)
		return (0);
	return (2);
}

/* Compare two placements in logical drawing order. */
static int
image_placement_cmp(const struct image_placement *a,
    const struct image_placement *b)
{
	int	aband, bband;

	aband = image_placement_band(a);
	bband = image_placement_band(b);
	if (aband != bband)
		return (aband < bband ? -1 : 1);
	if (a->input == IMAGE_INPUT_KITTY && a->z != b->z)
		return (a->z < b->z ? -1 : 1);
	if (a->input == IMAGE_INPUT_KITTY &&
	    image_get_order_id(a->image) != image_get_order_id(b->image)) {
		return (image_get_order_id(a->image) <
		    image_get_order_id(b->image) ? -1 : 1);
	}
	if (a->serial != b->serial)
		return (a->serial < b->serial ? -1 : 1);
	return (0);
}

/* Allocate the image store for a grid when first needed. */
static struct image_store *
image_store_get(struct grid *gd)
{
	struct image_store	*store = gd->images;

	if (store == NULL) {
		store = xcalloc(1, sizeof *store);
		TAILQ_INIT(&store->placements);
		gd->images = store;
	}
	return (store);
}

/* Allocate the image span list for a grid line when first needed. */
static struct image_line *
image_line_get(struct grid_line *gl)
{
	struct image_line	*line = gl->images;

	if (line == NULL) {
		line = xcalloc(1, sizeof *line);
		TAILQ_INIT(&line->spans);
		gl->images = line;
	}
	return (line);
}

/* Create a logical image placement. */
static struct image_placement *
image_placement_create(struct grid *gd, struct image *im,
    enum image_input input, u_int app_image_id, u_int app_placement_id,
    int32_t z)
{
	struct image_store	*store = image_store_get(gd);
	struct image_placement	*placement;

	placement = xcalloc(1, sizeof *placement);
	placement->store = store;
	placement->image = im;
	placement->input = input;
	placement->app_image_id = app_image_id;
	placement->app_placement_id = app_placement_id;
	placement->z = z;
	placement->serial = ++store->next_serial;
	if (placement->serial == 0)
		placement->serial = ++store->next_serial;
	TAILQ_INIT(&placement->spans);
	TAILQ_INSERT_TAIL(&store->placements, placement, entry);
	image_ref(im);
	return (placement);
}

/* Free a placement which no longer has any spans. */
static void
image_placement_free(struct image_placement *placement)
{
	if (!TAILQ_EMPTY(&placement->spans))
		fatalx("freeing image placement with spans");
	TAILQ_REMOVE(&placement->store->placements, placement, entry);
	image_free(placement->image);
	free(placement);
}

/* Insert a span into both its line and placement lists. */
static struct image_span *
image_span_add(struct image_line *line, struct image_placement *placement,
    u_int x, u_int width, u_int source_x, u_int source_y)
{
	struct image_span	*span, *at;

	if (width == 0)
		return (NULL);
	span = xcalloc(1, sizeof *span);
	span->x = x;
	span->sx = width;
	span->source_x = source_x;
	span->source_y = source_y;
	span->line = line;
	span->placement = placement;
	TAILQ_FOREACH(at, &line->spans, line_entry) {
		if (image_placement_cmp(placement, at->placement) < 0 ||
		    (placement == at->placement && x < at->x)) {
			TAILQ_INSERT_BEFORE(at, span, line_entry);
			goto inserted;
		}
	}
	TAILQ_INSERT_TAIL(&line->spans, span, line_entry);
inserted:
	TAILQ_INSERT_TAIL(&placement->spans, span, placement_entry);
	return (span);
}

/* Unlink and free one span without pruning its placement. */
static void
image_span_free(struct image_span *span)
{
	TAILQ_REMOVE(&span->line->spans, span, line_entry);
	TAILQ_REMOVE(&span->placement->spans, span, placement_entry);
	free(span);
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


/* Remove a cell range from one placement or the selected input type. */
static void
image_line_remove(struct image_line *line, u_int x, u_int width,
    enum image_input input,
    struct image_placement *only)
{
	struct image_span	*span, *next;
	u_int			 end;

	if (line == NULL || width == 0)
		return;
	end = x + width;
	if (end < x)
		end = UINT_MAX;
	TAILQ_FOREACH_SAFE(span, &line->spans, line_entry, next) {
		if (only != NULL && span->placement != only)
			continue;
		if (input == IMAGE_INPUT_OVERWRITTEN) {
			/* Keep ordinary Kitty placements on text writes. */
			if (span->placement->input != IMAGE_INPUT_SIXEL &&
			    !span->placement->placeholder) {
				continue;
			}
		} else if (input != IMAGE_INPUT_ALL) {
			if (span->placement->input != input)
				continue;
		}
		image_span_remove(span, x, end);
	}
}
/* Remove placement records which have no remaining spans. */
static void
image_store_prune(struct image_store *store)
{
	struct image_placement	*placement, *next;

	if (store == NULL)
		return;
	TAILQ_FOREACH_SAFE(placement, &store->placements, entry, next) {
		if (TAILQ_EMPTY(&placement->spans))
			image_placement_free(placement);
	}
}

/* Remove SIXEL and placeholder spans overwritten by text. */
void
image_grid_remove_overwritten_spans(struct grid *gd, u_int x, u_int y,
    u_int width, u_int height)
{
	u_int	row;

	if (gd->images == NULL || width == 0 || height == 0)
		return;
	if (y >= gd->hsize + gd->sy)
		return;
	if (height > gd->hsize + gd->sy - y)
		height = gd->hsize + gd->sy - y;
	for (row = y; row < y + height; row++) {
		image_line_remove(gd->linedata[row].images, x, width,
		    IMAGE_INPUT_OVERWRITTEN, NULL);
	}
	image_store_prune(gd->images);
}

/* Free all image spans belonging to a grid line. */
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

/* Free the empty image store when a grid is destroyed. */
void
image_grid_free(struct grid *gd)
{
	if (gd->images == NULL)
		return;
	image_store_prune(gd->images);
	if (!TAILQ_EMPTY(&gd->images->placements))
		fatalx("freeing grid with image placements");
	free(gd->images);
	gd->images = NULL;
}

/* Move image spans with a range of grid cells. */
void
image_grid_move_cells(struct grid *gd, u_int dx, u_int px, u_int py,
    u_int nx)
{
	struct image_line	*line;
	struct image_span	*span;
	struct image_move	*moves = NULL;
	size_t			 count = 0, i;
	u_int			 start, end, span_end;

	if (gd->images == NULL || nx == 0 || px == dx ||
	    py >= gd->hsize + gd->sy) {
		return;
	}
	line = gd->linedata[py].images;
	if (line == NULL)
		return;

	/* Save source spans before removing the overlapping cell ranges. */
	end = px + nx;
	TAILQ_FOREACH(span, &line->spans, line_entry) {
		span_end = span->x + span->sx;
		start = (span->x > px ? span->x : px);
		if (start >= end || span_end <= px)
			continue;
		if (span_end > end)
			span_end = end;
		moves = xreallocarray(moves, count + 1, sizeof *moves);
		moves[count].placement = span->placement;
		moves[count].x = dx + start - px;
		moves[count].sx = span_end - start;
		moves[count].source_x = span->source_x + start - span->x;
		moves[count].source_y = span->source_y;
		span->placement->recover_from = 0;
		count++;
	}

	/* Replace both ranges and restore spans at their new columns. */
	image_line_remove(line, px, nx, IMAGE_INPUT_ALL, NULL);
	image_line_remove(line, dx, nx, IMAGE_INPUT_ALL, NULL);
	for (i = 0; i < count; i++) {
		image_span_add(line, moves[i].placement, moves[i].x,
		    moves[i].sx, moves[i].source_x, moves[i].source_y);
	}
	free(moves);
	image_store_prune(gd->images);
}

/* Find or create the destination placement for a copied source placement. */
static struct image_placement *
image_copy_placement(struct image_copy_ctx *ctx,
    struct image_placement *source)
{
	struct image_placement	*placement;
	u_int			 i;

	for (i = 0; i < ctx->count; i++) {
		if (ctx->maps[i].source == source)
			return (ctx->maps[i].destination);
	}
	placement = image_placement_create(ctx->destination, source->image,
	    source->input, source->app_image_id, source->app_placement_id,
	    source->z);
	placement->placeholder = source->placeholder;
	placement->x = source->x;
	placement->recover_from = source->recover_from;
	ctx->maps = xreallocarray(ctx->maps, ctx->count + 1,
	    sizeof *ctx->maps);
	ctx->maps[ctx->count].source = source;
	ctx->maps[ctx->count].destination = placement;
	ctx->count++;
	return (placement);
}

/* Duplicate image spans alongside a group of grid lines. */
void
image_grid_duplicate_lines(struct grid *dst, u_int dy, struct grid *src,
    u_int sy, u_int ny)
{
	struct image_copy_ctx	 ctx = { .destination = dst };
	struct image_placement	*placement;
	struct image_line	*source_line, *destination_line;
	struct image_span	*span;
	u_int			 row;

	for (row = 0; row < ny; row++) {
		source_line = src->linedata[sy + row].images;
		if (source_line == NULL)
			continue;
		destination_line = image_line_get(&dst->linedata[dy + row]);
		TAILQ_FOREACH(span, &source_line->spans, line_entry) {
			placement = image_copy_placement(&ctx, span->placement);
			image_span_add(destination_line, placement, span->x,
			    span->sx, span->source_x, span->source_y);
		}
	}
	free(ctx.maps);
}

/* Copy clipped image spans between grid areas. */
void
image_grid_copy_area(struct grid *dst, u_int destination_x,
    u_int destination_y, struct grid *src, u_int source_x, u_int source_y,
    u_int sx, u_int sy)
{
	struct image_copy_ctx	 ctx = { .destination = dst };
	struct image_placement	*placement;
	struct image_line	*source_line, *destination_line;
	struct image_span	*span;
	u_int			 row, start, end, span_end;

	if (dst == src || sx == 0 || sy == 0)
		return;
	if (destination_y >= dst->hsize + dst->sy ||
	    source_y >= src->hsize + src->sy) {
		return;
	}
	if (sy > dst->hsize + dst->sy - destination_y)
		sy = dst->hsize + dst->sy - destination_y;
	if (sy > src->hsize + src->sy - source_y)
		sy = src->hsize + src->sy - source_y;
	end = source_x + sx;
	if (end < source_x)
		end = UINT_MAX;

	/* Copy clipped spans, sharing one destination placement per source. */
	for (row = 0; row < sy; row++) {
		source_line = src->linedata[source_y + row].images;
		if (source_line == NULL)
			continue;
		destination_line = image_line_get(
		    &dst->linedata[destination_y + row]);
		TAILQ_FOREACH(span, &source_line->spans, line_entry) {
			span_end = span->x + span->sx;
			start = (span->x > source_x ? span->x : source_x);
			if (start >= end || span_end <= source_x)
				continue;
			if (span_end > end)
				span_end = end;

			placement = image_copy_placement(&ctx, span->placement);
			placement->recover_from = 0;
			image_span_add(destination_line, placement,
			    destination_x + start - source_x, span_end - start,
			    span->source_x + start - span->x, span->source_y);
		}
	}
	free(ctx.maps);
}

/* Return whether a grid line contains any image spans. */
int
image_grid_line_has_images(struct grid_line *gl)
{
	return (gl->images != NULL && !TAILQ_EMPTY(&gl->images->spans));
}

/* Return whether a grid rectangle contains any image spans. */
int
image_grid_area_has_images(struct grid *gd, u_int x, u_int y, u_int width,
    u_int height)
{
	struct image_line	*line;
	struct image_span	*span;
	u_int			 row, end;

	if (gd->images == NULL || width == 0 || height == 0 ||
	    y >= gd->hsize + gd->sy) {
		return (0);
	}
	end = x + width;
	if (end < x)
		end = UINT_MAX;
	if (height > gd->hsize + gd->sy - y)
		height = gd->hsize + gd->sy - y;
	for (row = y; row < y + height; row++) {
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

/* Find the resolved placeholder at one grid cell. */
int
image_grid_get_placeholder(struct grid *gd, u_int x, u_int y,
    struct kitty_placeholder *placeholder)
{
	struct image_line	*line;
	struct image_span	*span, *found = NULL;

	if (y >= gd->hsize + gd->sy ||
	    (line = gd->linedata[y].images) == NULL) {
		return (0);
	}
	TAILQ_FOREACH(span, &line->spans, line_entry) {
		if (!span->placement->placeholder)
			continue;
		if (x < span->x)
			continue;
		if (x < span->x + span->sx)
			found = span;
	}
	if (found == NULL)
		return (0);
	placeholder->image = found->placement->image;
	placeholder->source_x = found->source_x + x - found->x;
	placeholder->source_y = found->source_y;
	placeholder->image_id = found->placement->app_image_id;
	placeholder->placement_id = found->placement->app_placement_id;
	placeholder->z = found->placement->z;
	return (1);
}

/* Add one Kitty Unicode-placeholder cell to an image placement. */
void
image_place_cell_kitty(struct screen_write_ctx *ctx, struct image *im,
    u_int x, u_int y, u_int source_x, u_int source_y, u_int image_id,
    u_int placement_id, int32_t z)
{
	struct grid		*gd = ctx->s->grid;
	struct image_store	*store = image_store_get(gd);
	struct image_placement	*placement = NULL, *candidate;
	struct image_line	*line;
	struct image_span	*span;

	/* Reuse a matching placeholder placement in this grid. */
	TAILQ_FOREACH_REVERSE(candidate, &store->placements,
	    image_placements, entry) {
		if (!candidate->placeholder)
			continue;
		if (candidate->input == IMAGE_INPUT_KITTY &&
		    candidate->image == im &&
		    candidate->app_image_id == image_id &&
		    candidate->app_placement_id == placement_id &&
		    candidate->z == z) {
			placement = candidate;
			break;
		}
	}
	if (placement == NULL) {
		placement = image_placement_create(gd, im, IMAGE_INPUT_KITTY,
		    image_id, placement_id, z);
		placement->placeholder = 1;
	}
	/* Extend an adjacent source run or add a new span for this cell. */
	line = image_line_get(&gd->linedata[gd->hsize + y]);
	TAILQ_FOREACH(span, &line->spans, line_entry) {
		if (span->placement == placement && span->x + span->sx == x &&
		    span->source_y == source_y &&
		    span->source_x + span->sx == source_x) {
			span->sx++;
			image_redraw_area(ctx, x, y, 1, 1);
			return;
		}
	}
	image_span_add(line, placement, x, 1, source_x, source_y);
	image_redraw_area(ctx, x, y, 1, 1);
}

/* Return the image for a drawing rectangle. */
struct image *
image_rect_get_image(const struct image_rect *rectangle)
{
	return (rectangle->image);
}

/* Return the source and destination coordinates of a drawing rectangle. */
void
image_rect_get_coords(const struct image_rect *rectangle,
    u_int *source_x, u_int *source_y, u_int *width, u_int *height,
    u_int *destination_x, u_int *destination_y)
{
	*source_x = rectangle->src_x;
	*source_y = rectangle->src_y;
	*width = rectangle->sx;
	*height = rectangle->sy;
	*destination_x = rectangle->dst_x;
	*destination_y = rectangle->dst_y;
}

/* Return the output z-index for a drawing rectangle. */
int64_t
image_rect_get_z(const struct image_rect *rectangle)
{
	return (rectangle->z);
}

/* Get the image covering a grid cell and its position in that image. */
int
image_grid_get_at(struct grid *gd, u_int x, u_int y, struct image **im,
    u_int *src_x, u_int *src_y)
{
	struct image_line	*line;
	struct image_span	*span, *found = NULL;
	struct image_placement	*placement;
	struct grid_cell	 gc;

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
	placement = found->placement;
	if (placement->input == IMAGE_INPUT_KITTY && placement->z < 0) {
		grid_get_cell(gd, x, y, &gc);
		if (!kitty_cell_is_placeholder(&gc)) {
			if (gc.data.size != 1 || gc.data.data[0] != ' ')
				return (-1);
		}
		if (placement->z < IMAGE_Z_BELOW_BACKGROUND &&
		    !COLOUR_DEFAULT(gc.bg))
			return (-1);
	}
	*im = placement->image;
	*src_x = found->source_x + x - found->x;
	*src_y = found->source_y;
	return (1);
}
/* Remove one placement and all of its spans. */
static void
image_remove_placement(struct image_placement *placement)
{
	struct image_span	*span, *next;

	TAILQ_FOREACH_SAFE(span, &placement->spans, placement_entry, next)
		image_span_free(span);
	image_placement_free(placement);
}

/* Clear image placements with an internal image ID from a screen. */
void
image_clear(struct screen_write_ctx *ctx, u_int id)
{
	struct image_store	*store = ctx->s->grid->images;
	struct image_placement	*placement, *next;

	if (store == NULL)
		return;
	TAILQ_FOREACH_SAFE(placement, &store->placements, entry, next) {
		if (id != 0) {
			if (image_get_source_id(placement->image) != id) {
				if (placement->placeholder)
					continue;
				if (image_get_id(placement->image) != id)
					continue;
			}
		}
		image_remove_placement(placement);
	}
	if (ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
}

/* Return whether a grid still references an image or its source. */
int
image_grid_has_image(struct grid *gd, u_int id)
{
	struct image_placement	*placement;

	if (gd->images == NULL)
		return (0);
	TAILQ_FOREACH(placement, &gd->images->placements, entry) {
		if (image_get_id(placement->image) == id)
			return (1);
		if (image_get_source_id(placement->image) == id)
			return (1);
	}
	return (0);
}

/* Return whether a placement intersects the selected screen cells. */
static int
image_placement_intersects(struct grid *gd, struct image_placement *placement,
    const struct kitty_parse_result *result)
{
	struct image_line	*line;
	struct image_span	*span;
	u_int			 x = result->x, y = result->y, row;
	char			 how = result->delete;
	int			 match_x = 0, match_y = 0;

	if (how >= 'A') {
		if (how <= 'Z')
			how += 'a' - 'A';
	}
	switch (how) {
	case 'p': case 'q': case 'c':
		match_y = 1;
		/* FALLTHROUGH */
	case 'x':
		match_x = 1;
		break;
	case 'y':
		match_y = 1;
		break;
	}
	for (row = 0; row < gd->sy; row++) {
		if (match_y) {
			if (y == 0)
				continue;
			if (row != y - 1)
				continue;
		}
		line = gd->linedata[gd->hsize + row].images;
		if (line == NULL)
			continue;
		TAILQ_FOREACH(span, &line->spans, line_entry) {
			if (span->placement != placement)
				continue;
			if (span->x >= gd->sx)
				continue;
			if (match_x) {
				if (x == 0)
					continue;
				if (x > gd->sx)
					continue;
				if (x - 1 < span->x)
					continue;
				if (x - 1 >= span->x + span->sx)
					continue;
			}
			return (1);
		}
	}
	return (0);
}

/* Clear ordinary Kitty placements selected by a graphics delete command. */
void
image_clear_kitty(struct screen_write_ctx *ctx,
    const struct kitty_parse_result *result)
{
	struct grid		*gd = ctx->s->grid;
	struct image_store	*store = gd->images;
	struct image_placement	*placement, *next;
	struct kitty_parse_result selection = *result;
	char			 how = result->delete;
	int			 matched;
	u_int			 placement_id = result->placement_id;

	if (store == NULL)
		return;
	if (how >= 'A') {
		if (how <= 'Z')
			how += 'a' - 'A';
	}
	if (how == 'c') {
		selection.x = ctx->s->cx + 1;
		selection.y = ctx->s->cy + 1;
	}
	TAILQ_FOREACH_SAFE(placement, &store->placements, entry, next) {
		if (placement->input != IMAGE_INPUT_KITTY)
			continue;
		if (placement->placeholder)
			continue;
		matched = 0;
		switch (how) {
		case 'i':
			if (placement->app_image_id != result->image_id)
				break;
			if (placement_id != 0) {
				if (placement->app_placement_id != placement_id)
					break;
			}
			matched = 1;
			break;
		case 'r':
			if (placement->app_image_id < result->x)
				break;
			if (placement->app_image_id > result->y)
				break;
			matched = 1;
			break;
		case 'z':
			if (placement->z == result->z)
				matched = 1;
			break;
		case 'q':
			if (placement->z != result->z)
				break;
			/* FALLTHROUGH */
		case 'a': case 'c': case 'p': case 'x': case 'y':
			matched = image_placement_intersects(gd, placement,
			    &selection);
			break;
		}
		if (matched)
			image_remove_placement(placement);
	}
	if (ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
}

/* Record window damage for image layers in a screen area. */
void
image_redraw_area(struct screen_write_ctx *ctx, u_int px, u_int py, u_int nx,
    u_int ny)
{
	struct window_pane	*wp = ctx->wp;

	if (wp == NULL)
		return;
	if (!image_grid_area_has_images(ctx->s->grid, px,
	    ctx->s->grid->hsize + py, nx, ny)) {
		return;
	}
	redraw_damage_window(wp->window, wp->xoff + px, wp->yoff + py, nx,
	    ny);
}

/* Redraw all image layers on a screen. */
void
image_redraw_all(struct screen_write_ctx *ctx)
{
	image_redraw_area(ctx, 0, 0, screen_size_x(ctx->s),
	    screen_size_y(ctx->s));
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

/* Draw a clipped part of one image span. */
static void
image_draw_span(const struct image_backend *backend, struct tty *tty,
    struct image_span *span, u_int start, u_int end,
    u_int px, u_int atx, u_int aty)
{
	struct image_placement	*placement = span->placement;
	struct image_rect	 rectangle;

	rectangle.image = placement->image;
	if (placement->input == IMAGE_INPUT_SIXEL)
		rectangle.z = 0;
	else if (placement->z >= 0)
		rectangle.z = (int64_t)placement->z + 1;
	else
		rectangle.z = placement->z;
	rectangle.src_x = span->source_x + start - span->x;
	rectangle.src_y = span->source_y;
	rectangle.sx = end - start;
	rectangle.sy = 1;
	rectangle.dst_x = atx + start - px;
	rectangle.dst_y = aty;
	backend->draw_rect(tty, &rectangle);
}

/* Return whether a cell contains a glyph or text decoration. */
static int
image_cell_has_text(struct grid *gd, u_int x, u_int y)
{
	struct grid_cell	gc;

	grid_view_get_cell(gd, x, y, &gc);
	if (kitty_cell_is_placeholder(&gc))
		return (0);
	if (gc.data.size != 1 || gc.data.data[0] != ' ')
		return (1);
	return (gc.attr != 0);
}

/* Draw a span's graphical image layers before or after its text. */
void
image_draw_line(struct tty *tty, struct screen *s, u_int px, u_int py,
    u_int nx, u_int atx, u_int aty, int before)
{
	const struct image_backend	*backend;
	struct image_line		*line;
	struct image_span		*span;
	struct image_placement		*placement;
	u_int				 start, end, span_end, draw_end;
	int				 blank_only;

	image_tty_update(tty);
	backend = tty->image_backend;
	if (~backend->get_flags(tty) & IMAGE_BACKEND_GRAPHICAL)
		return;
	if (py >= s->grid->sy)
		return;
	line = s->grid->linedata[s->grid->hsize + py].images;
	if (line == NULL)
		return;
	end = px + nx;
	TAILQ_FOREACH(span, &line->spans, line_entry) {
		placement = span->placement;
		blank_only = 0;

		/* Emulate Kitty's negative layers with SIXEL. */
		if (placement->input == IMAGE_INPUT_KITTY &&
		    placement->z < 0) {
			if (backend == &image_backend_sixel &&
			    placement->z >= IMAGE_Z_BELOW_BACKGROUND) {
				if (before)
					continue;
				blank_only = 1;
			} else if (!before)
				continue;
		} else if (before)
			continue;
		span_end = span->x + span->sx;
		start = (span->x > px ? span->x : px);
		if (start >= end || span_end <= px)
			continue;
		if (span_end > end)
			span_end = end;
		if (!blank_only) {
			image_draw_span(backend, tty, span, start, span_end,
			    px, atx, aty);
			continue;
		}
		/* Draw below-text SIXEL layers only on cells without text. */
		while (start < span_end) {
			while (start < span_end &&
			    image_cell_has_text(s->grid, start, py))
				start++;
			draw_end = start;
			while (draw_end < span_end &&
			    !image_cell_has_text(s->grid, draw_end, py))
				draw_end++;
			if (start < draw_end) {
				image_draw_span(backend, tty, span, start,
				    draw_end, px, atx, aty);
			}
			start = draw_end;
		}
	}
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
	u_int		 npx, npy, nsx, nsy, opx, opy, osx, osy, xx, yy;
	u_int		 ncw, nch, ocw, och, ncx, ncy, ocx, ocy;
	const u_char	*np, *op, *npixels, *opixels;
	size_t		 nstride, ostride;

	image_get_canvas_size(new, &ncw, &nch);
	image_get_canvas_size(old, &ocw, &och);
	image_get_size_in_cells(new, &ncx, &ncy);
	image_get_size_in_cells(old, &ocx, &ocy);
	if (ncw / ncx != ocw / ocx || nch / ncy != och / ocy)
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

/* Discard cells of older SIXEL placements fully covered by a new image. */
static void
image_line_cover(struct image_line *line, struct image_placement *placement,
    u_int x, u_int source_x, u_int source_y)
{
	struct image_span	*span, *next;
	struct image_placement	*old;

	TAILQ_FOREACH_SAFE(span, &line->spans, line_entry, next) {
		old = span->placement;
		if (old == placement || old->input != IMAGE_INPUT_SIXEL)
			continue;
		if (x < span->x || x >= span->x + span->sx)
			continue;
		if (!image_cell_covers(placement->image, source_x, source_y,
		    old->image, span->source_x + x - span->x, span->source_y)) {
			continue;
		}
		image_line_remove(line, x, 1, IMAGE_INPUT_ALL, old);
	}
}

/* Add spans for one row of a placement between two source columns. */
static void
image_extend_row(struct image_line *line, struct image_placement *placement,
    u_int cx, u_int source_y, u_int old_end, u_int new_end)
{
	struct image	*im = placement->image;
	u_int		 x, run;

	for (x = old_end; x < new_end; x += run) {
		if (!image_cell_has_visible_pixels(im, x, source_y)) {
			run = 1;
			continue;
		}
		for (run = 1; x + run < new_end; run++) {
			if (!image_cell_has_visible_pixels(im, x + run, source_y))
				break;
		}
		image_span_add(line, placement, cx + x, run, x, source_y);
	}
}

/* Extend image rows when grid is resized. */
void
image_grid_resize_width(struct grid *gd, u_int new_sx)
{
	struct image_placement		*plmt;
	struct image_span		*span;
	struct image_recover_row	*rows;
	u_int				 image_sx, image_sy, from, to;
	u_int				 nrows, i;

	if (gd->images == NULL)
		return;
	TAILQ_FOREACH(plmt, &gd->images->placements, entry) {
		from = plmt->recover_from;
		if (from == 0 || plmt->x >= new_sx)
			continue;
		image_get_size_in_cells(plmt->image, &image_sx, &image_sy);

		to = new_sx - plmt->x;
		if (to > image_sx)
			to = image_sx;
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
		plmt->recover_from = (to == image_sx ? 0 : to);
	}
}

/* Place an image at the cursor using the supplied input semantics. */
static void
image_write(struct screen_write_ctx *ctx, struct image *im, u_int bg,
    enum image_input input, u_int app_image_id, u_int app_placement_id,
    int32_t z)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct image_placement	*placement;
	struct image_line	*line;
	u_int			 cx = s->cx, cy = s->cy;
	u_int			 x, y, i, run, sx, sy, lines, origin_y = 0;
	u_int			 hist_origin_y, region_height, remaining, chunk;
	u_int			 cursor_x = 0, cursor_rows, image_sx;

	image_get_size_in_cells(im, &sx, &sy);
	image_sx = sx;
	if (sx > screen_size_x(s) - cx)
		sx = screen_size_x(s) - cx;
	if (sx == 0)
		return;

	/* Kitty advances to the last image row; SIXEL advances past it. */
	cursor_rows = sy;
	if (input == IMAGE_INPUT_KITTY)
		cursor_rows--;

	/* Scroll images that move the cursor, or clip stationary placements. */
	if (image_has_no_cursor(im)) {
		if (sy > screen_size_y(s) - cy)
			sy = screen_size_y(s) - cy;
	} else if (screen_size_y(s) - cy <= cursor_rows) {
		lines = cursor_rows - (screen_size_y(s) - cy) + 1;

		/* screen_write_scrollup() clamps lines per call, so loop it. */
		region_height = s->rlower - s->rupper + 1;
		if (region_height == 0)
			region_height = 1;
		remaining = lines;
		while (remaining > 0) {
			chunk = (remaining < region_height) ? remaining :
			    region_height;
			screen_write_scrollup(ctx, chunk, bg);
			remaining -= chunk;
		}

		if (lines > cy) {
			origin_y = lines - cy;
			screen_write_cursormove(ctx, -1, 0, 0);
		} else
			screen_write_cursormove(ctx, -1, cy - lines, 0);
		cy = s->cy;
		sy -= origin_y;
		cursor_rows -= origin_y;
	}

	placement = image_placement_create(gd, im, input, app_image_id,
	    app_placement_id, z);
	if (sx < image_sx) {
		placement->x = cx;
		placement->recover_from = sx;
	}

	/* Attach scrolled-off image rows to the available history lines. */
	hist_origin_y = origin_y;
	if (hist_origin_y > gd->hsize)
		hist_origin_y = gd->hsize;
	for (y = 0; y < hist_origin_y; y++) {
		line = image_line_get(
		    &gd->linedata[gd->hsize - hist_origin_y + y]);
		image_extend_row(line, placement, cx,
		    origin_y - hist_origin_y + y, 0, sx);
	}

	/* Attach visible runs, discarding SIXEL cells they completely cover. */
	for (y = 0; y < sy; y++) {
		line = image_line_get(&gd->linedata[gd->hsize + cy + y]);
		for (x = 0; x < sx; x += run) {
			if (!image_cell_has_visible_pixels(im, x, origin_y + y)) {
				run = 1;
				continue;
			}
			for (run = 1; x + run < sx; run++) {
				if (!image_cell_has_visible_pixels(im, x + run,
				    origin_y + y)) {
					break;
				}
			}
			if (input == IMAGE_INPUT_SIXEL) {
				for (i = 0; i < run; i++) {
					image_line_cover(line, placement,
					    cx + x + i, x + i, origin_y + y);
				}
			}
			image_span_add(line, placement, cx + x, run, x,
			    origin_y + y);
		}
	}

	/* Schedule a redraw and apply the input protocol's cursor movement. */
	image_store_prune(gd->images);
	image_redraw_area(ctx, cx, cy, sx, sy);
	if (!image_has_no_cursor(im)) {
		if (input == IMAGE_INPUT_KITTY)
			cursor_x = cx + sx;
		screen_write_cursormove(ctx, cursor_x, cy + cursor_rows, 0);
	}
}

/* Place an image received through SIXEL. */
void
image_write_sixel(struct screen_write_ctx *ctx, struct image *im, u_int bg)
{
	image_write(ctx, im, bg, IMAGE_INPUT_SIXEL, 0, 0, 0);
}

/* Place an image received through the Kitty graphics protocol. */
void
image_write_kitty(struct screen_write_ctx *ctx, struct image *im, u_int bg,
    u_int image_id, u_int placement_id, int32_t z)
{
	image_write(ctx, im, bg, IMAGE_INPUT_KITTY, image_id, placement_id, z);
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

	plmt = image_placement_create(gd, im, IMAGE_INPUT_SIXEL, 0, 0, 0);
	for (row = 0; row < sy; row++) {
		line = image_line_get(&gd->linedata[y + row]);
		image_span_add(line, plmt, x, sx, src_x, src_y + row);
	}
}

/* Remove every image span in a grid rectangle. */
void
image_grid_remove(struct grid *gd, u_int x, u_int y, u_int sx, u_int sy)
{
	u_int row, rows = gd->hsize + gd->sy;

	if (gd->images == NULL || sx == 0 || sy == 0 || y >= rows)
		return;
	if (sy > rows - y)
		sy = rows - y;
	for (row = y; row < y + sy; row++)
		image_line_remove(gd->linedata[row].images, x, sx,
		    IMAGE_INPUT_ALL, NULL);
	image_store_prune(gd->images);
}

/* Redraw images under changed cells and remove those overwritten by input. */
void
image_damage_screen(struct screen_write_ctx *ctx, u_int x, u_int y,
    u_int sx, u_int sy)
{
	struct screen *s = ctx->s;

	image_redraw_area(ctx, x, y, sx, sy);
	if (ctx->flags & SCREEN_WRITE_INPUT)
		image_grid_remove_overwritten_spans(s->grid, x,
		    s->grid->hsize + y, sx, sy);
}
