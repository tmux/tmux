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
#include <png.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* An average of part of an image cell. RGB is premultiplied. */
struct image_sample {
	u_char			 red;
	u_char			 green;
	u_char			 blue;
	u_char			 alpha;
	u_char			 brightness;
};

/* Half blocks, quadrants and sextants all divide evenly into a 2 by 6 grid. */
#define IMAGE_SAMPLE_COLUMNS 2
#define IMAGE_SAMPLE_ROWS 6
#define IMAGE_FLAG_NO_CURSOR 0x1
#define IMAGE_Z_BELOW_BACKGROUND (INT32_MIN / 2)
struct image_cell {
	struct image_sample	 whole;
	struct image_sample	 samples[IMAGE_SAMPLE_ROWS][IMAGE_SAMPLE_COLUMNS];
};

/* Immutable image data and cell geometry. */
struct image {
	u_int			 id;
	u_int			 references;
	u_int			 flags;
	u_int			 parent_id;
	u_int			 source_id;
	u_int			 width;
	u_int			 height;
	u_int			 canvas_width;
	u_int			 canvas_height;
	u_int			 sx;
	u_int			 sy;
	size_t			 stride;
	size_t			 size;
	u_char			*pixels;
	/* Original indexed SIXEL data, if this image arrived as SIXEL. */
	struct sixel_image	*sixel;
	struct image_cell	*cells;

	RB_ENTRY(image)		 entry;
};
RB_HEAD(images, image);

/* A cell-aligned part of an image to draw at a terminal position. */
struct image_rect {
	struct image		*image;
	struct grid_cell	 cell;
	int32_t			 z;
	u_int			 source_x;
	u_int			 source_y;
	u_int			 sx;
	u_int			 sy;
	u_int			 destination_x;
	u_int			 destination_y;
};

/* One contiguous row of a placement in the grid. */
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

/* Image spans attached to one grid line. */
struct image_line {
	struct image_spans	 spans;
};

#define IMAGE_INPUT_SIXEL 0
#define IMAGE_INPUT_KITTY 1

/* One logical image placement, shared by all of its row spans. */
struct image_placement {
	struct image_store	*store;
	struct image		*image;
	u_int			 input;
	u_int			 app_image_id;
	u_int			 app_placement_id;
	int32_t			 z;
	uint64_t		 serial;
	struct image_spans	 spans;
	TAILQ_ENTRY(image_placement) entry;
};
TAILQ_HEAD(image_placements, image_placement);

/* Placements belonging to one grid. */
struct image_store {
	struct grid		*grid;
	uint64_t		 next_serial;
	struct image_placements	 placements;
};

static struct images	images = RB_INITIALIZER(&images);
static u_int		image_next_id;

struct image_backend {
	const char	*name;
	int		 flags;
	void		(*draw_rect)(struct tty *,
		    const struct image_rect *, const struct tty_style_ctx *);
	void		(*free)(struct tty *, int);
	void		(*geometry_changed)(struct tty *);
};

static const struct image_backend image_backend_fallback = {
	"fallback", IMAGE_BACKEND_SCROLLS, NULL, NULL, NULL
};
static const struct image_backend image_backend_kitty = {
	"kitty", IMAGE_BACKEND_GRAPHICAL|IMAGE_BACKEND_SCROLLS,
	kitty_draw_rect, kitty_free_output, kitty_geometry_changed
};
static const struct image_backend image_backend_sixel = {
	"sixel", IMAGE_BACKEND_GRAPHICAL,
	sixel_draw_rect,
	sixel_free_output, sixel_geometry_changed
};

/* Find the image backend supported by a terminal. */
static const struct image_backend *
image_tty_find_backend(struct tty *tty)
{
	if (tty->term != NULL && tty->term->flags & TERM_KITTY)
		return (&image_backend_kitty);
	if (tty->term != NULL && tty->term->flags & TERM_SIXEL &&
	    tty->xpixel != 0 && tty->ypixel != 0)
		return (&image_backend_sixel);
	return (&image_backend_fallback);
}

/* Update a terminal's image backend after its capabilities change. */
void
image_tty_update(struct tty *tty)
{
	const struct image_backend	*backend;

	backend = image_tty_find_backend(tty);
	if (tty->image_backend == backend)
		return;

	if (tty->image_backend != NULL && tty->image_backend->free != NULL)
		tty->image_backend->free(tty, !!(tty->flags & TTY_OPENED));
	tty->image_data = NULL;
	tty->image_backend = backend;
	log_debug("%s: %s image backend is %s", __func__,
	    tty->client->name, backend->name);
}

/* Remove Kitty placements which will be replaced by a redraw. */
void
image_redraw_start(struct tty *tty, u_int x, u_int y, u_int width,
    u_int height)
{
	image_tty_update(tty);
	if (tty->image_backend == &image_backend_kitty)
		kitty_redraw_start(tty, x, y, width, height);
	else if (tty->image_backend == &image_backend_sixel)
		sixel_redraw_start(tty, x, y, width, height);
}

/* Return the flags for a terminal's image backend. */
int
image_backend_flags(struct tty *tty)
{
	image_tty_update(tty);
	return (tty->image_backend->flags);
}

/* Discard image backend state after a terminal geometry change. */
void
image_tty_geometry_changed(struct tty *tty)
{
	image_tty_update(tty);
	if (tty->image_backend->geometry_changed != NULL)
		tty->image_backend->geometry_changed(tty);
}

/* Free image backend state for a terminal. */
void
image_tty_free(struct tty *tty, int send)
{
	if (tty->image_backend != NULL && tty->image_backend->free != NULL)
		tty->image_backend->free(tty, send);
	tty->image_backend = NULL;
	tty->image_data = NULL;
}

/* Compare images by server ID. */
static int
image_cmp(struct image *a, struct image *b)
{
	if (a->id < b->id)
		return (-1);
	if (a->id > b->id)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(images, image, entry, image_cmp);

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
	    a->app_image_id != b->app_image_id)
		return (a->app_image_id < b->app_image_id ? -1 : 1);
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
		store->grid = gd;
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
image_placement_create(struct grid *gd, struct image *im, u_int input,
    u_int app_image_id, u_int app_placement_id, int32_t z)
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
	image_ref(im->id);
	return (placement);
}

/* Free a placement which no longer has any spans. */
static void
image_placement_free(struct image_placement *placement)
{
	if (!TAILQ_EMPTY(&placement->spans))
		fatalx("freeing image placement with spans");
	TAILQ_REMOVE(&placement->store->placements, placement, entry);
	image_free(placement->image->id);
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

/* Remove a range from selected spans on a line. */
static void
image_line_remove(struct image_line *line, u_int x, u_int width, int input)
{
	struct image_span	*span, *next;
	u_int			 end, span_end, right;

	if (line == NULL || width == 0)
		return;
	end = x + width;
	if (end < x)
		end = UINT_MAX;
	TAILQ_FOREACH_SAFE(span, &line->spans, line_entry, next) {
		if (input != -1 && span->placement->input != (u_int)input)
			continue;
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

/* Remove temporal image data overwritten by text. */
void
image_grid_damage(struct grid *gd, u_int x, u_int y, u_int width,
    u_int height)
{
	u_int	row;

	if (gd->images == NULL || width == 0 || height == 0)
		return;
	if (y >= gd->hsize + gd->sy)
		return;
	if (height > gd->hsize + gd->sy - y)
		height = gd->hsize + gd->sy - y;
	for (row = y; row < y + height; row++)
		image_line_remove(gd->linedata[row].images, x, width,
		    IMAGE_INPUT_SIXEL);
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
	struct image_move {
		struct image_placement *placement;
		u_int x, sx, source_x, source_y;
	} *moves = NULL;
	size_t			 count = 0;
	u_int			 start, end, span_end;

	if (gd->images == NULL || nx == 0 || px == dx ||
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
		moves = xreallocarray(moves, count + 1, sizeof *moves);
		moves[count].placement = span->placement;
		moves[count].x = dx + start - px;
		moves[count].sx = span_end - start;
		moves[count].source_x = span->source_x + start - span->x;
		moves[count].source_y = span->source_y;
		count++;
	}
	image_line_remove(line, px, nx, -1);
	image_line_remove(line, dx, nx, -1);
	for (size_t i = 0; i < count; i++)
		image_span_add(line, moves[i].placement, moves[i].x,
		    moves[i].sx, moves[i].source_x, moves[i].source_y);
	free(moves);
	image_store_prune(gd->images);
}

/* Duplicate image spans alongside a group of grid lines. */
void
image_grid_duplicate_lines(struct grid *dst, u_int dy, struct grid *src,
    u_int sy, u_int ny)
{
	struct image_map {
		struct image_placement *source;
		struct image_placement *destination;
	} *maps = NULL;
	struct image_placement	*placement;
	struct image_line	*source_line, *destination_line;
	struct image_span	*span;
	size_t			 count = 0, i;
	u_int			 row;

	for (row = 0; row < ny; row++) {
		source_line = src->linedata[sy + row].images;
		if (source_line == NULL)
			continue;
		destination_line = image_line_get(&dst->linedata[dy + row]);
		TAILQ_FOREACH(span, &source_line->spans, line_entry) {
			placement = NULL;
			for (i = 0; i < count; i++) {
				if (maps[i].source == span->placement) {
					placement = maps[i].destination;
					break;
				}
			}
			if (placement == NULL) {
				placement = image_placement_create(dst,
				    span->placement->image, span->placement->input,
				    span->placement->app_image_id,
				    span->placement->app_placement_id,
				    span->placement->z);
				maps = xreallocarray(maps, count + 1, sizeof *maps);
				maps[count].source = span->placement;
				maps[count].destination = placement;
				count++;
			}
			image_span_add(destination_line, placement, span->x,
			    span->sx, span->source_x, span->source_y);
		}
	}
	free(maps);
}

/* Copy clipped image spans between grid areas. */
void
image_grid_copy_area(struct grid *dst, u_int destination_x,
    u_int destination_y, struct grid *src, u_int source_x, u_int source_y,
    u_int sx, u_int sy)
{
	struct image_map {
		struct image_placement *source;
		struct image_placement *destination;
	} *maps = NULL;
	struct image_placement	*placement;
	struct image_line	*source_line, *destination_line;
	struct image_span	*span;
	size_t			 count = 0, i;
	u_int			 row, start, end, span_end;

	if (dst == src || sx == 0 || sy == 0)
		return;
	if (destination_y >= dst->hsize + dst->sy ||
	    source_y >= src->hsize + src->sy)
		return;
	if (sy > dst->hsize + dst->sy - destination_y)
		sy = dst->hsize + dst->sy - destination_y;
	if (sy > src->hsize + src->sy - source_y)
		sy = src->hsize + src->sy - source_y;
	end = source_x + sx;
	if (end < source_x)
		end = UINT_MAX;

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

			placement = NULL;
			for (i = 0; i < count; i++) {
				if (maps[i].source == span->placement) {
					placement = maps[i].destination;
					break;
				}
			}
			if (placement == NULL) {
				placement = image_placement_create(dst,
				    span->placement->image, span->placement->input,
				    span->placement->app_image_id,
				    span->placement->app_placement_id,
				    span->placement->z);
				maps = xreallocarray(maps, count + 1,
				    sizeof *maps);
				maps[count].source = span->placement;
				maps[count].destination = placement;
				count++;
			}
			image_span_add(destination_line, placement,
			    destination_x + start - source_x, span_end - start,
			    span->source_x + start - span->x, span->source_y);
		}
	}
	free(maps);
}

/* Return whether a grid line contains any image spans. */
int
image_grid_line_has_images(const struct grid_line *gl)
{
	return (gl->images != NULL && !TAILQ_EMPTY(&gl->images->spans));
}

/* Return whether a grid rectangle contains any image spans. */
int
image_grid_check_area(struct grid *gd, u_int x, u_int y, u_int width,
    u_int height)
{
	struct image_line	*line;
	struct image_span	*span;
	u_int			 row, end;

	if (gd->images == NULL || width == 0 || height == 0 ||
	    y >= gd->hsize + gd->sy)
		return (0);
	end = x + width;
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

/* Find source coordinates for an image span at one grid cell. */
int
image_grid_get_source(struct grid *gd, u_int x, u_int y, struct image *im,
    u_int *source_x, u_int *source_y)
{
	struct image_line	*line;
	struct image_span	*span, *found = NULL;

	if (y >= gd->hsize + gd->sy ||
	    (line = gd->linedata[y].images) == NULL)
		return (0);
	TAILQ_FOREACH(span, &line->spans, line_entry) {
		if (span->placement->image == im && x >= span->x &&
		    x < span->x + span->sx)
			found = span;
	}
	if (found == NULL)
		return (0);
	*source_x = found->source_x + x - found->x;
	*source_y = found->source_y;
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

	TAILQ_FOREACH_REVERSE(candidate, &store->placements,
	    image_placements, entry) {
		if (candidate->input == IMAGE_INPUT_KITTY &&
		    candidate->image == im &&
		    candidate->app_image_id == image_id &&
		    candidate->app_placement_id == placement_id &&
		    candidate->z == z) {
			placement = candidate;
			break;
		}
	}
	if (placement == NULL)
		placement = image_placement_create(gd, im, IMAGE_INPUT_KITTY,
		    image_id, placement_id, z);
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

/* Average a rectangle of image pixels into one fallback sample. */
static void
image_sample(struct image *im, uint64_t sample_x, uint64_t sample_y,
    uint64_t sample_columns, uint64_t sample_rows, struct image_sample *sample)
{
	const u_char	*pixel;
	uint64_t	 red = 0, green = 0, blue = 0, alpha = 0;
	uint64_t	 brightness = 0, count = 0;
	u_int		 x, y, x0, x1, y0, y1;

	x0 = sample_x * im->canvas_width / sample_columns;
	x1 = ((sample_x + 1) * im->canvas_width + sample_columns - 1) /
	    sample_columns;
	y0 = sample_y * im->canvas_height / sample_rows;
	y1 = ((sample_y + 1) * im->canvas_height + sample_rows - 1) /
	    sample_rows;
	if (x1 <= x0)
		x1 = x0 + 1;
	if (y1 <= y0)
		y1 = y0 + 1;
	count = (uint64_t)(x1 - x0) * (y1 - y0);
	if (x0 >= im->width || y0 >= im->height)
		return;
	if (x1 > im->width)
		x1 = im->width;
	if (y1 > im->height)
		y1 = im->height;

	for (y = y0; y < y1; y++) {
		for (x = x0; x < x1; x++) {
			pixel = im->pixels + y * im->stride + x * 4;
			red += pixel[0] * pixel[3] / 255;
			green += pixel[1] * pixel[3] / 255;
			blue += pixel[2] * pixel[3] / 255;
			alpha += pixel[3];
			brightness += ((2126ULL * pixel[0] +
			    7152ULL * pixel[1] + 722ULL * pixel[2]) / 10000) *
			    pixel[3] / 255;
		}
	}
	if (count == 0)
		return;
	sample->red = red / count;
	sample->green = green / count;
	sample->blue = blue / count;
	sample->alpha = alpha / count;
	sample->brightness = brightness / count;
}

/* Build fallback samples for every image cell. */
static void
image_make_cells(struct image *im)
{
	struct image_cell	*cell;
	uint64_t		 columns, rows;
	u_int			 x, y, sample_x, sample_y;

	columns = (uint64_t)im->sx * IMAGE_SAMPLE_COLUMNS;
	rows = (uint64_t)im->sy * IMAGE_SAMPLE_ROWS;
	im->cells = xcalloc((size_t)im->sx * im->sy, sizeof *im->cells);
	for (y = 0; y < im->sy; y++) {
		for (x = 0; x < im->sx; x++) {
			cell = &im->cells[(size_t)y * im->sx + x];
			image_sample(im, x, y, im->sx, im->sy, &cell->whole);
			for (sample_y = 0; sample_y < IMAGE_SAMPLE_ROWS;
			    sample_y++) {
				for (sample_x = 0;
				    sample_x < IMAGE_SAMPLE_COLUMNS; sample_x++) {
					image_sample(im,
					    (uint64_t)x * IMAGE_SAMPLE_COLUMNS + sample_x,
					    (uint64_t)y * IMAGE_SAMPLE_ROWS + sample_y,
					    columns, rows,
					    &cell->samples[sample_y][sample_x]);
				}
			}
		}
	}
}

/* Find an image by server ID. */
struct image *
image_find(u_int id)
{
	struct image	find;

	find.id = id;
	return (RB_FIND(images, &images, &find));
}

/* Return an image's server ID. */
u_int
image_get_id(const struct image *im)
{
	return (im->id);
}

/* Return an image's pixel dimensions. */
void
image_get_size(const struct image *im, u_int *width, u_int *height)
{
	if (width != NULL)
		*width = im->width;
	if (height != NULL)
		*height = im->height;
}

/* Return an image canvas's pixel dimensions. */
void
image_get_canvas_size(const struct image *im, u_int *width,
    u_int *height)
{
	if (width != NULL)
		*width = im->canvas_width;
	if (height != NULL)
		*height = im->canvas_height;
}

/* Return an image's cell dimensions. */
void
image_get_size_in_cells(const struct image *im, u_int *sx, u_int *sy)
{
	if (sx != NULL)
		*sx = im->sx;
	if (sy != NULL)
		*sy = im->sy;
}

/* Return an image's RGBA pixels and layout. */
const u_char *
image_get_pixels(const struct image *im, size_t *stride, size_t *size)
{
	if (stride != NULL)
		*stride = im->stride;
	if (size != NULL)
		*size = im->size;
	return (im->pixels);
}

/* Suppress cursor movement when writing an image. */
void
image_set_no_cursor(struct image *im)
{
	im->flags |= IMAGE_FLAG_NO_CURSOR;
}

/* Return an image's original SIXEL data. */
struct sixel_image *
image_get_sixel(const struct image *im)
{
	return (im->sixel);
}

/* Associate original SIXEL data with an image. */
void
image_set_sixel(struct image *im, struct sixel_image *si)
{
	im->sixel = si;
}

/* Return the image for a drawing rectangle. */
struct image *
image_rect_get_image(const struct image_rect *rectangle)
{
	return (rectangle->image);
}

/* Return the source grid cell for a drawing rectangle. */
const struct grid_cell *
image_rect_get_cell(const struct image_rect *rectangle)
{
	return (&rectangle->cell);
}

/* Return the source and destination coordinates of a drawing rectangle. */
void
image_rect_get_coords(const struct image_rect *rectangle,
    u_int *source_x, u_int *source_y, u_int *width, u_int *height,
    u_int *destination_x, u_int *destination_y)
{
	*source_x = rectangle->source_x;
	*source_y = rectangle->source_y;
	*width = rectangle->sx;
	*height = rectangle->sy;
	*destination_x = rectangle->destination_x;
	*destination_y = rectangle->destination_y;
}

/* Return the output z-index for a drawing rectangle. */
int32_t
image_rect_get_z(const struct image_rect *rectangle)
{
	return (rectangle->z);
}

/* Create and register an immutable image. */
static struct image *
image_create1(u_int width, u_int height, u_int canvas_width,
    u_int canvas_height, u_int sx, u_int sy, size_t stride, u_char *pixels)
{
	struct image	*im;

	im = xcalloc(1, sizeof *im);
	do {
		if (++image_next_id == 0)
			image_next_id++;
		im->id = image_next_id;
	} while (image_find(im->id) != NULL);

	im->references = 1;
	im->source_id = im->id;
	im->width = width;
	im->height = height;
	im->canvas_width = canvas_width;
	im->canvas_height = canvas_height;
	im->sx = sx;
	im->sy = sy;
	im->stride = stride;
	im->size = (size_t)width * height * 4;
	im->pixels = pixels;

	RB_INSERT(images, &images, im);
	log_debug("%s: image %u is %ux%u pixels on %ux%u canvas, "
	    "%ux%u cells", __func__, im->id, width, height, canvas_width,
	    canvas_height, sx, sy);
	return (im);
}

/* Create an image from decoded pixel data. */
struct image *
image_create(u_int width, u_int height, u_int canvas_width,
    u_int canvas_height, u_int sx, u_int sy, u_char *pixels)
{
	struct image	*im;

	if (width == 0 || height == 0 || canvas_width < width ||
	    canvas_height < height || sx == 0 || sy == 0 || pixels == NULL)
		return (NULL);
	if ((uint64_t)width * height * 4 > SIZE_MAX)
		return (NULL);
	if ((uint64_t)sx * sy > SIZE_MAX / sizeof *im->cells ||
	    sx > USHRT_MAX || sy > USHRT_MAX)
		return (NULL);
	im = image_create1(width, height, canvas_width, canvas_height, sx, sy,
	    (size_t)width * 4, pixels);
	return (im);
}

/* Create an immutable rectangular view without copying its source pixels. */
/* Create a cell-aligned view of an existing image. */
struct image *
image_create_view(struct image *source, u_int x, u_int y, u_int width,
    u_int height, u_int canvas_width, u_int canvas_height, u_int sx, u_int sy)
{
	struct image	*im;

	if (source == NULL || x >= source->width || y >= source->height ||
	    width == 0 || width > source->width - x || height == 0 ||
	    height > source->height - y || canvas_width < width ||
	    canvas_height < height || sx == 0 || sy == 0)
		return (NULL);
	if ((uint64_t)sx * sy > SIZE_MAX / sizeof *im->cells ||
	    sx > USHRT_MAX || sy > USHRT_MAX)
		return (NULL);

	im = image_create1(width, height, canvas_width, canvas_height, sx, sy,
	    source->stride, source->pixels + (size_t)y * source->stride +
	    (size_t)x * 4);
	if (im == NULL)
		return (NULL);
	im->parent_id = source->id;
	im->source_id = source->source_id;
	image_ref(source->id);
	return (im);
}

/* Add a reference to an image. */
void
image_ref(u_int id)
{
	struct image	*im = image_find(id);

	if (im == NULL)
		fatalx("reference to missing image %u", id);
	if (im->references == UINT_MAX)
		fatalx("too many references to image %u", id);
	im->references++;
}

/* Drop a reference to an image. */
void
image_free(u_int id)
{
	struct image	*im = image_find(id);

	if (im == NULL)
		fatalx("free of missing image %u", id);
	if (--im->references != 0)
		return;

	log_debug("%s: freeing image %u", __func__, id);
	RB_REMOVE(images, &images, im);
	if (im->parent_id == 0)
		free(im->pixels);
	else
		image_free(im->parent_id);
	if (im->sixel != NULL)
		sixel_free(im->sixel);
	free(im->cells);
	free(im);
}

/* Return a precomputed fallback cell sample. */
static const struct image_cell *
image_get_cell(struct image *im, u_int x, u_int y)
{
	if (im == NULL || x >= im->sx || y >= im->sy)
		return (NULL);
	if (im->cells == NULL)
		image_make_cells(im);
	return (&im->cells[(size_t)y * im->sx + x]);
}

/* Fill a grid cell with a fallback image glyph. */
void
image_get_fallback_cell(__unused struct tty *tty, struct image *im, u_int x,
    u_int y, const struct grid_cell *gc, struct grid_cell *out,
    __unused const struct tty_style_ctx *style_ctx)
{
	static const char	 ramp[] = " .:-=+*#%@";
	const struct image_cell	*cell;
	u_int			 level = 0;

	memcpy(out, gc, sizeof *out);
	cell = image_get_cell(im, x, y);
	if (cell != NULL)
		level = cell->whole.brightness * (sizeof ramp - 2) / 255;
	utf8_set(&out->data, ramp[level]);
}

/* Return one for a fallback cell, minus one to continue along an image line. */
int
image_get_fallback_at(struct tty *tty, struct screen *s, u_int x, u_int y,
    const struct grid_cell *gc, struct grid_cell *out,
    const struct tty_style_ctx *style_ctx)
{
	struct image_line	*line;
	struct image_span	*span, *found = NULL;
	struct image_placement	*placement;

	if (image_backend_flags(tty) & IMAGE_BACKEND_GRAPHICAL ||
	    y >= s->grid->sy)
		return (0);
	line = s->grid->linedata[s->grid->hsize + y].images;
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
		if (gc->data.size != 1 || gc->data.data[0] != ' ')
			return (-1);
		if (placement->z < IMAGE_Z_BELOW_BACKGROUND &&
		    !COLOUR_DEFAULT(gc->bg))
			return (-1);
	}
	image_get_fallback_cell(tty, placement->image,
	    found->source_x + x - found->x, found->source_y, gc, out,
	    style_ctx);
	return (1);
}

/* Convert an image cell rectangle to pixel coordinates. */
void
image_get_pixel_rect(const struct image *im, u_int x, u_int y,
    u_int width, u_int height, u_int *px, u_int *py, u_int *pwidth,
    u_int *pheight)
{
	u_int	x1, y1;

	*px = *py = *pwidth = *pheight = 0;
	if (im == NULL || x >= im->sx || y >= im->sy || width == 0 ||
	    height == 0)
		return;
	if (width > im->sx - x)
		width = im->sx - x;
	if (height > im->sy - y)
		height = im->sy - y;

	*px = (uint64_t)x * im->canvas_width / im->sx;
	*py = (uint64_t)y * im->canvas_height / im->sy;
	x1 = ((uint64_t)(x + width) * im->canvas_width + im->sx - 1) /
	    im->sx;
	y1 = ((uint64_t)(y + height) * im->canvas_height + im->sy - 1) /
	    im->sy;
	if (*px >= im->width || *py >= im->height) {
		*px = *py = 0;
		return;
	}
	if (x1 <= *px)
		x1 = *px + 1;
	if (y1 <= *py)
		y1 = *py + 1;
	if (x1 > im->width)
		x1 = im->width;
	if (y1 > im->height)
		y1 = im->height;
	*pwidth = x1 - *px;
	*pheight = y1 - *py;
}

/* Calculate the cell dimensions required for pixel dimensions. */
void
image_size_in_cells(u_int width, u_int height, u_int xpixel, u_int ypixel,
    u_int *sx, u_int *sy)
{
	if (xpixel == 0)
		xpixel = 8;
	if (ypixel == 0)
		ypixel = 16;
	*sx = width / xpixel + (width % xpixel != 0);
	*sy = height / ypixel + (height % ypixel != 0);
}

/* Decode a base64 payload with a size limit. */
u_char *
image_base64_decode(const char *data, size_t len, size_t limit, size_t *size)
{
	char	*copy;
	u_char	*out;
	size_t	 needed, padded, padding;
	int	 result;

	if (len > SIZE_MAX - 3)
		return (NULL);
	padding = (4 - len % 4) % 4;
	if (padding == 3)
		return (NULL);
	padded = len + padding;
	needed = padded / 4 * 3;
	if (needed > limit || needed > INT_MAX)
		return (NULL);

	copy = xmalloc(padded + 1);
	memcpy(copy, data, len);
	memset(copy + len, '=', padding);
	copy[padded] = '\0';
	out = xmalloc(needed == 0 ? 1 : needed);
	result = b64_pton(copy, out, needed);
	free(copy);
	if (result < 0) {
		free(out);
		return (NULL);
	}
	*size = result;
	return (out);
}

/* Decode a PNG payload into RGBA pixels. */
u_char *
image_png_decode(const u_char *data, size_t size, size_t limit, u_int *width,
    u_int *height)
{
	png_image	 pi;
	u_char		*pixels;

	if (size == 0 || size > limit)
		return (NULL);
	memset(&pi, 0, sizeof pi);
	pi.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_memory(&pi, data, size))
		return (NULL);
	pi.format = PNG_FORMAT_RGBA;
	if (pi.width == 0 || pi.height == 0 ||
	    (uint64_t)pi.width * pi.height * 4 > limit) {
		png_image_free(&pi);
		return (NULL);
	}
	pixels = xmalloc(PNG_IMAGE_SIZE(pi));
	if (!png_image_finish_read(&pi, NULL, pixels, 0, NULL)) {
		free(pixels);
		png_image_free(&pi);
		return (NULL);
	}
	*width = pi.width;
	*height = pi.height;
	png_image_free(&pi);
	return (pixels);
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
		if (id != 0 && placement->image->id != id &&
		    placement->image->source_id != id)
			continue;
		image_remove_placement(placement);
	}
	if (ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
}

/* Clear Kitty placements selected by application identity or z-index. */
void
image_clear_kitty(struct screen_write_ctx *ctx, char how, u_int image_id,
    u_int placement_id, int32_t z)
{
	struct image_store	*store = ctx->s->grid->images;
	struct image_placement	*placement, *next;
	int			 matched;

	if (store == NULL)
		return;
	TAILQ_FOREACH_SAFE(placement, &store->placements, entry, next) {
		if (placement->input != IMAGE_INPUT_KITTY)
			continue;
		matched = 0;
		switch (how) {
		case 'a': case 'A':
			matched = 1;
			break;
		case 'i':
			matched = (placement->app_image_id == image_id &&
			    (placement_id == 0 ||
			    placement->app_placement_id == placement_id));
			break;
		case 'I':
			matched = (placement->app_image_id == image_id);
			break;
		case 'z': case 'Z':
			matched = (placement->z == z);
			break;
		}
		if (matched)
			image_remove_placement(placement);
	}
	if (ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
}

/* Redraw image layers in a screen area. */
void
image_redraw_area(struct screen_write_ctx *ctx, u_int px, u_int py, u_int nx,
    u_int ny)
{
	if (ctx->wp != NULL && image_grid_check_area(ctx->s->grid, px,
	    ctx->s->grid->hsize + py, nx, ny))
		ctx->wp->flags |= PANE_REDRAW;
}

/* Redraw all image layers on a screen. */
void
image_redraw_all(struct screen_write_ctx *ctx)
{
	image_redraw_area(ctx, 0, 0, screen_size_x(ctx->s),
	    screen_size_y(ctx->s));
}

/* Redraw images after a scrolling operation. */
void
image_redraw_scroll(struct screen_write_ctx *ctx, __unused u_int lines)
{
	image_redraw_all(ctx);
}

/* Draw a clipped part of one image span. */
static void
image_draw_span(const struct image_backend *backend, struct tty *tty,
    struct screen *s, struct image_span *span, u_int start, u_int end,
    u_int px, u_int py, u_int atx, u_int aty,
    const struct tty_style_ctx *style_ctx)
{
	struct image_placement	*placement = span->placement;
	struct image_rect	 rectangle;

	rectangle.image = placement->image;
	grid_view_get_cell(s->grid, start, py, &rectangle.cell);
	if (placement->input == IMAGE_INPUT_SIXEL)
		rectangle.z = 0;
	else if (placement->z >= 0 && placement->z < INT32_MAX)
		rectangle.z = placement->z + 1;
	else
		rectangle.z = placement->z;
	rectangle.source_x = span->source_x + start - span->x;
	rectangle.source_y = span->source_y;
	rectangle.sx = end - start;
	rectangle.sy = 1;
	rectangle.destination_x = atx + start - px;
	rectangle.destination_y = aty;
	backend->draw_rect(tty, &rectangle, style_ctx);
}

/* Return whether a cell contains a glyph or text decoration. */
static int
image_cell_has_text(struct grid *gd, u_int x, u_int y)
{
	struct grid_cell	gc;

	grid_view_get_cell(gd, x, y, &gc);
	if (gc.data.size != 1 || gc.data.data[0] != ' ')
		return (1);
	return (gc.attr != 0);
}

/* Draw a span's graphical image layers before or after its text. */
void
image_draw_line(struct tty *tty, struct screen *s, u_int px, u_int py,
    u_int nx, u_int atx, u_int aty, int before,
    const struct tty_style_ctx *style_ctx)
{
	const struct image_backend	*backend;
	struct image_line		*line;
	struct image_span		*span;
	struct image_placement		*placement;
	u_int				 start, end, span_end, draw_end;
	int				 blank_only;

	image_tty_update(tty);
	backend = tty->image_backend;
	if (~backend->flags & IMAGE_BACKEND_GRAPHICAL)
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
			image_draw_span(backend, tty, s, span, start, span_end,
			    px, py, atx, aty, style_ctx);
			continue;
		}
		while (start < span_end) {
			while (start < span_end &&
			    image_cell_has_text(s->grid, start, py))
				start++;
			draw_end = start;
			while (draw_end < span_end &&
			    !image_cell_has_text(s->grid, draw_end, py))
				draw_end++;
			if (start < draw_end)
				image_draw_span(backend, tty, s, span, start,
				    draw_end, px, py, atx, aty, style_ctx);
			start = draw_end;
		}
	}
}

/* Return whether an image cell contains at least one nontransparent pixel. */
static int
image_cell_has_alpha(struct image *im, u_int x, u_int y)
{
	u_int		 px, py, sx, sy, xx, yy;
	const u_char	*pixels;

	image_get_pixel_rect(im, x, y, 1, 1, &px, &py, &sx, &sy);
	if (sx == 0 || sy == 0)
		return (0);
	pixels = im->pixels;
	for (yy = py; yy < py + sy; yy++) {
		for (xx = px; xx < px + sx; xx++) {
			if (pixels[(size_t)yy * im->stride + (size_t)xx * 4 + 3] != 0)
				return (1);
		}
	}
	return (0);
}

/* Place an image at the cursor using the supplied input semantics. */
static void
image_write(struct screen_write_ctx *ctx, struct image *im, u_int bg,
    u_int input, u_int app_image_id, u_int app_placement_id, int32_t z)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct image_placement	*placement;
	struct image_line	*line;
	u_int			 cx = s->cx, cy = s->cy;
	u_int			 x, y, run, sx, sy, lines, origin_y = 0;

	sx = im->sx;
	if (sx > screen_size_x(s) - cx)
		sx = screen_size_x(s) - cx;
	sy = im->sy;
	if (sx == 0)
		return;

	if (im->flags & IMAGE_FLAG_NO_CURSOR) {
		if (sy > screen_size_y(s) - cy)
			sy = screen_size_y(s) - cy;
	} else if (screen_size_y(s) - cy <= sy) {
		lines = sy - (screen_size_y(s) - cy) + 1;
		screen_write_scrollup(ctx, lines, bg);
		if (lines > cy) {
			origin_y = lines - cy;
			screen_write_cursormove(ctx, -1, 0, 0);
		} else
			screen_write_cursormove(ctx, -1, cy - lines, 0);
		cy = s->cy;
		sy -= origin_y;
	}

	placement = image_placement_create(gd, im, input, app_image_id,
	    app_placement_id, z);
	for (y = 0; y < sy; y++) {
		line = image_line_get(&gd->linedata[gd->hsize + cy + y]);
		for (x = 0; x < sx; x += run) {
			if (!image_cell_has_alpha(im, x, origin_y + y)) {
				run = 1;
				continue;
			}
			for (run = 1; x + run < sx; run++) {
				if (!image_cell_has_alpha(im, x + run,
				    origin_y + y))
					break;
			}
			image_span_add(line, placement, cx + x, run, x,
			    origin_y + y);
		}
	}
	image_store_prune(gd->images);
	image_redraw_area(ctx, cx, cy, sx, sy);
	if (!(im->flags & IMAGE_FLAG_NO_CURSOR))
		screen_write_cursormove(ctx, 0, cy + sy, 0);
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
