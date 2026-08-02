/* $OpenBSD$ */

/*
 * Copyright (c) 2019 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static struct images	images = RB_INITIALIZER(&images);
static u_int		image_next_id;

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

struct image *
image_find(u_int id)
{
	struct image	find;

	find.id = id;
	return (RB_FIND(images, &images, &find));
}

struct image *
image_create(u_int width, u_int height, u_int sx, u_int sy, u_char *pixels)
{
	struct image	*im;

	if (width == 0 || height == 0 || sx == 0 || sy == 0 || pixels == NULL)
		return (NULL);
	if ((uint64_t)width * height * 4 > SIZE_MAX)
		return (NULL);
	if ((uint64_t)sx * sy > SIZE_MAX)
		return (NULL);

	im = xcalloc(1, sizeof *im);
	do {
		if (++image_next_id == 0)
			image_next_id++;
		im->id = image_next_id;
	} while (image_find(im->id) != NULL);

	im->references = 1;
	im->width = width;
	im->height = height;
	im->sx = sx;
	im->sy = sy;
	im->stride = (size_t)width * 4;
	im->size = im->stride * height;
	im->pixels = pixels;
	image_make_ascii(im);

	RB_INSERT(images, &images, im);
	log_debug("%s: image %u is %ux%u pixels, %ux%u cells", __func__,
	    im->id, width, height, sx, sy);
	return (im);
}

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
	free(im->pixels);
	free(im->ascii);
	free(im);
}

void
image_set_cell(struct grid_cell *gc, struct image *im, u_int x, u_int y)
{
	memcpy(gc, &grid_default_cell, sizeof *gc);
	gc->flags |= GRID_FLAG_IMAGE;
	gc->image_id = im->id;
	gc->image_x = x;
	gc->image_y = y;
}

void
image_clear(struct screen *s, u_int id)
{
	struct grid		*gd = s->grid;
	struct grid_cell	 gc;
	u_int			 x, y;

	for (y = 0; y < gd->hsize + gd->sy; y++) {
		for (x = 0; x < gd->sx; x++) {
			grid_get_cell(gd, x, y, &gc);
			if ((gc.flags & GRID_FLAG_IMAGE) &&
			    (id == 0 || gc.image_id == id))
				grid_set_cell(gd, x, y, &grid_default_cell);
		}
	}
}

int
image_check_area(struct screen *s, u_int px, u_int py, u_int nx, u_int ny)
{
	struct grid_cell	 gc;
	u_int			 x, y, ex, ey;

	ex = px + nx;
	if (ex > screen_size_x(s))
		ex = screen_size_x(s);
	ey = py + ny;
	if (ey > screen_size_y(s))
		ey = screen_size_y(s);
	for (y = py; y < ey; y++) {
		for (x = px; x < ex; x++) {
			grid_view_get_cell(s->grid, x, y, &gc);
			if (gc.flags & GRID_FLAG_IMAGE)
				return (1);
		}
	}
	return (0);
}

int
image_check_line(struct screen *s, u_int py, u_int ny)
{
	return (image_check_area(s, 0, py, screen_size_x(s), ny));
}

int
image_free_all(struct screen *s)
{
	return (image_check_area(s, 0, 0, screen_size_x(s),
	    screen_size_y(s)));
}

int
image_scroll_up(struct screen *s, __unused u_int lines)
{
	return (image_free_all(s));
}

/*
 * Put image marker cells at the cursor. The pixel object is immutable; only
 * the marker rectangle is clipped to the available pane cells.
 */
void
image_write(struct screen_write_ctx *ctx, struct image *im, u_int bg)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_cell		 gc;
	u_int			 cx = s->cx, cy = s->cy;
	u_int			 x, y, sx, sy, lines;

	sx = im->sx;
	if (sx > screen_size_x(s) - cx)
		sx = screen_size_x(s) - cx;
	sy = im->sy;
	if (sy > screen_size_y(s) - 1)
		sy = screen_size_y(s) - 1;
	if (sx == 0 || sy == 0)
		return;

	if (screen_size_y(s) - cy <= sy) {
		lines = sy - (screen_size_y(s) - cy) + 1;
		screen_write_scrollup(ctx, lines, bg);
		if (lines > cy)
			screen_write_cursormove(ctx, -1, 0, 0);
		else
			screen_write_cursormove(ctx, -1, cy - lines, 0);
		cy = s->cy;
	}

	for (y = 0; y < sy; y++) {
		for (x = 0; x < sx; x++) {
			image_set_cell(&gc, im, x, y);
			gc.bg = bg;
			grid_view_set_cell(gd, cx + x, cy + y, &gc);
		}
	}
	if (ctx->wp != NULL)
		ctx->wp->flags |= PANE_REDRAW;
	screen_write_cursormove(ctx, 0, cy + sy, 0);
}
