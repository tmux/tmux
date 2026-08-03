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

#include <limits.h>
#include <png.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static struct images	images = RB_INITIALIZER(&images);
static u_int		image_next_id;

#define IMAGE_BACKEND_GRAPHICAL 0x1
#define IMAGE_BACKEND_SCROLLS   0x2

struct image_backend {
	const char	*name;
	int		 flags;
	void		(*draw_rectangle)(struct tty *,
		    const struct image_rectangle *, const struct tty_style_ctx *);
	void		(*free)(struct tty *, int);
	void		(*geometry_changed)(struct tty *);
};

static const struct image_backend image_backend_ascii = {
	"ascii", IMAGE_BACKEND_SCROLLS, NULL, NULL, NULL
};
static const struct image_backend image_backend_kitty = {
	"kitty", IMAGE_BACKEND_GRAPHICAL|IMAGE_BACKEND_SCROLLS,
	kitty_draw_rectangle, kitty_free_output, kitty_geometry_changed
};
static const struct image_backend image_backend_sixel = {
	"sixel", IMAGE_BACKEND_GRAPHICAL, sixel_draw_rectangle, NULL, NULL
};

static const struct image_backend *
image_tty_find_backend(struct tty *tty)
{
	if (tty->term != NULL && tty->term->flags & TERM_KITTY)
		return (&image_backend_kitty);
	if (tty->term != NULL && tty->term->flags & TERM_SIXEL &&
	    tty->xpixel != 0 && tty->ypixel != 0)
		return (&image_backend_sixel);
	return (&image_backend_ascii);
}

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

int
image_tty_is_graphical(struct tty *tty)
{
	image_tty_update(tty);
	return (!!(tty->image_backend->flags & IMAGE_BACKEND_GRAPHICAL));
}

int
image_tty_scrolls(struct tty *tty)
{
	image_tty_update(tty);
	return (!!(tty->image_backend->flags & IMAGE_BACKEND_SCROLLS));
}

void
image_tty_geometry_changed(struct tty *tty)
{
	image_tty_update(tty);
	if (tty->image_backend->geometry_changed != NULL)
		tty->image_backend->geometry_changed(tty);
}

void
image_tty_free(struct tty *tty, int send)
{
	if (tty->image_backend != NULL && tty->image_backend->free != NULL)
		tty->image_backend->free(tty, send);
	tty->image_backend = NULL;
	tty->image_data = NULL;
}

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

static void
image_sample(struct image *im, uint64_t sample_x, uint64_t sample_y,
    uint64_t sample_columns, uint64_t sample_rows, struct image_sample *sample)
{
	const u_char	*pixel;
	uint64_t	 red = 0, green = 0, blue = 0, alpha = 0;
	uint64_t	 brightness = 0, count = 0;
	u_int		 x, y, x0, x1, y0, y1;

	x0 = sample_x * im->width / sample_columns;
	x1 = (sample_x + 1) * im->width / sample_columns;
	y0 = sample_y * im->height / sample_rows;
	y1 = (sample_y + 1) * im->height / sample_rows;
	if (x1 <= x0)
		x1 = x0 + 1;
	if (y1 <= y0)
		y1 = y0 + 1;
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
			count++;
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
	if ((uint64_t)sx * sy > SIZE_MAX / sizeof *im->cells)
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
	free(im->cells);
	free(im);
}

const struct image_cell *
image_get_cell(struct image *im, u_int x, u_int y)
{
	if (im == NULL || x >= im->sx || y >= im->sy)
		return (NULL);
	if (im->cells == NULL)
		image_make_cells(im);
	return (&im->cells[(size_t)y * im->sx + x]);
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

/* Convert a cell-aligned image rectangle into source pixel coordinates. */
void
image_get_pixel_rectangle(const struct image *im, u_int x, u_int y,
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

	*px = (uint64_t)x * im->width / im->sx;
	*py = (uint64_t)y * im->height / im->sy;
	x1 = (uint64_t)(x + width) * im->width / im->sx;
	y1 = (uint64_t)(y + height) * im->height / im->sy;
	if (*px >= im->width)
		*px = im->width - 1;
	if (*py >= im->height)
		*py = im->height - 1;
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

void
image_size_in_cells(u_int width, u_int height, u_int xpixel, u_int ypixel,
    u_int *sx, u_int *sy)
{
	if (xpixel == 0)
		xpixel = 8;
	if (ypixel == 0)
		ypixel = 16;
	*sx = ((uint64_t)width + xpixel - 1) / xpixel;
	*sy = ((uint64_t)height + ypixel - 1) / ypixel;
}

u_char *
image_base64_decode(const char *data, size_t len, size_t limit, size_t *size)
{
	char	*copy;
	u_char	*out;
	size_t	 needed;
	int	 result;

	if (len > SIZE_MAX - 3)
		return (NULL);
	needed = (len + 3) / 4 * 3;
	if (needed > limit || needed > INT_MAX)
		return (NULL);

	copy = xmalloc(len + 1);
	memcpy(copy, data, len);
	copy[len] = '\0';
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

void
image_clear(struct screen_write_ctx *ctx, u_int id)
{
	struct screen		*s = ctx->s;
	struct grid		*gd = s->grid;
	struct grid_cell	 gc;
	u_int			 x, y;

	for (y = 0; y < gd->hsize + gd->sy; y++) {
		for (x = 0; x < gd->sx; x++) {
			grid_get_cell(gd, x, y, &gc);
			if ((gc.flags & GRID_FLAG_IMAGE) &&
			    (id == 0 || gc.image_id == id)) {
				if (y >= gd->hsize)
					image_damage_area(ctx, x, y - gd->hsize,
					    1, 1);
				grid_set_cell(gd, x, y, &grid_default_cell);
			}
		}
	}
}

static int
image_check_area(struct screen *s, u_int px, u_int py, u_int nx, u_int ny)
{
	struct grid_cell	 gc;
	u_int			 x, y, ex, ey;
	u_int			 sx = screen_size_x(s), sy = screen_size_y(s);

	if (px >= sx || py >= sy || nx == 0 || ny == 0)
		return (0);
	if (nx > sx - px)
		ex = sx;
	else
		ex = px + nx;
	if (ny > sy - py)
		ey = sy;
	else
		ey = py + ny;
	for (y = py; y < ey; y++) {
		for (x = px; x < ex; x++) {
			grid_view_get_cell(s->grid, x, y, &gc);
			if (gc.flags & GRID_FLAG_IMAGE)
				return (1);
		}
	}
	return (0);
}

void
image_damage_area(struct screen_write_ctx *ctx, u_int px, u_int py, u_int nx,
    u_int ny)
{
	if (ctx->wp != NULL && image_check_area(ctx->s, px, py, nx, ny))
		ctx->wp->flags |= PANE_REDRAW;
}

void
image_damage_all(struct screen_write_ctx *ctx)
{
	image_damage_area(ctx, 0, 0, screen_size_x(ctx->s),
	    screen_size_y(ctx->s));
}

void
image_damage_scroll(struct screen_write_ctx *ctx, __unused u_int lines)
{
	image_damage_all(ctx);
}

/* Draw the graphical image marker runs in one visible scene span. */
void
image_draw_line(struct tty *tty, struct screen *s, u_int px, u_int py,
    u_int nx, u_int atx, u_int aty, const struct tty_style_ctx *style_ctx)
{
	const struct image_backend	*backend;
	struct image_rectangle		 rectangle;
	struct grid_cell		 gc, next;
	struct image			*im;
	u_int				 i, run;

	image_tty_update(tty);
	backend = tty->image_backend;
	if (~backend->flags & IMAGE_BACKEND_GRAPHICAL)
		return;

	for (i = 0; i < nx; i += run) {
		grid_view_get_cell(s->grid, px + i, py, &gc);
		if (~gc.flags & GRID_FLAG_IMAGE) {
			run = 1;
			continue;
		}
		im = image_find(gc.image_id);
		if (im == NULL) {
			run = 1;
			continue;
		}
		for (run = 1; i + run < nx; run++) {
			grid_view_get_cell(s->grid, px + i + run, py, &next);
			if (~next.flags & GRID_FLAG_IMAGE ||
			    next.image_id != gc.image_id ||
			    next.image_y != gc.image_y ||
			    next.image_x != gc.image_x + run)
				break;
		}
		rectangle.image = im;
		memcpy(&rectangle.cell, &gc, sizeof rectangle.cell);
		rectangle.source_x = gc.image_x;
		rectangle.source_y = gc.image_y;
		rectangle.width = run;
		rectangle.height = 1;
		rectangle.destination_x = atx + i;
		rectangle.destination_y = aty;
		backend->draw_rectangle(tty, &rectangle, style_ctx);
	}
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
	image_damage_area(ctx, cx, cy, sx, sy);
	screen_write_cursormove(ctx, 0, cy + sy, 0);
}
