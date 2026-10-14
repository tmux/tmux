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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* An image. */
struct image {
	u_int			 id;
	enum image_type		 type;
	u_int			 references;
	struct image_geom	 geom;

	struct image		*parent;
	u_int			 order_id;

	/*
	 * RGBA pixels use four bytes each. Stride is the number of bytes
	 * between the start of each row. Size is the number of bytes
	 * containing all of this image's pixels - it may include pixels that
	 * are part of the parent image but not part of this image.
	 */
	size_t			 stride;
	size_t			 size;
	u_char			*pixels;

	RB_ENTRY(image)		 entry;
};
RB_HEAD(images, image);
static struct images images = RB_INITIALIZER(&images);

/* Next image ID. */
static u_int image_next_id;

/* Compare images by IDs. */
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

/* Find an image by its ID. */
struct image *
image_find(u_int id)
{
	struct image	find;

	find.id = id;
	return (RB_FIND(images, &images, &find));
}

/* Get the ID of an image. */
u_int
image_get_id(struct image *im)
{
	return (im->id);
}

/* Get the type of an image. */
enum image_type
image_get_type(struct image *im)
{
	return (im->type);
}

/* Allocate an image and assign a unique ID. */
static struct image *
image_alloc(enum image_type type, struct image_geom *ig, size_t stride,
    u_char *pixels)
{
	struct image	*im;
	u_int		 id;

	do {
		if (++image_next_id == 0)
			image_next_id++;
		id = image_next_id;
	} while (image_find(id) != NULL);

	im = xcalloc(1, sizeof *im);
	im->id = id;
	im->type = type;
	im->references = 1;
	im->order_id = id;
	im->geom = *ig;
	im->stride = stride;
	im->size = (ig->src_h - 1) * stride + ig->src_w * 4;
	im->pixels = pixels;
	RB_INSERT(images, &images, im);
	return (im);
}

/* Create an image with the provided pixels. */
struct image *
image_create(enum image_type type, struct image_geom *ig, u_char *pixels)
{
	size_t	stride = ig->src_w * 4;

	return (image_create_with_parent(type, NULL, ig, stride, pixels));
}

/* Create an image based on a parent image. */
struct image *
image_create_with_parent(enum image_type type, struct image *parent,
    struct image_geom *ig, size_t stride, u_char *pixels)
{
	struct image	*im;

	if (ig->src_w == 0 || ig->src_h == 0)
		return (NULL);
	if (ig->canvas_width == 0 || ig->canvas_height == 0)
		return (NULL);
	if (ig->sx == 0 || ig->sy == 0 || pixels == NULL)
		return (NULL);
	if ((uint64_t)ig->src_w * ig->src_h > IMAGE_SIZE_LIMIT / 4)
		return (NULL);
	if (ig->sx > USHRT_MAX || ig->sy > USHRT_MAX)
		return (NULL);
	if (ig->sx * ig->sy > IMAGE_SIZE_LIMIT || stride < ig->src_w * 4)
		return (NULL);
	if (ig->scaled_width <= 0 || ig->scaled_height <= 0)
		return (NULL);

	im = image_alloc(type, ig, stride, pixels);
	im->parent = parent;
	if (parent != NULL) {
		im->order_id = parent->order_id;
		image_ref(parent);
	}
	return (im);
}

/* Add a reference to an image. */
void
image_ref(struct image *im)
{
	if (im->references == UINT_MAX)
		fatalx("invalid image reference %u", im->id);
	im->references++;
}

/* Free an image. */
void
image_free(struct image *im)
{
	if (im->references == 0)
		fatalx("invalid image reference %u", im->id);
	if (--im->references == 0) {
		RB_REMOVE(images, &images, im);
		if (im->parent == NULL)
			free(im->pixels);
		else
			image_free(im->parent);
		free(im);
	}
}

/* Get the image geometry. */
struct image_geom *
image_get_geometry(struct image *im)
{
	return (&im->geom);
}

/* Get the image pixels. */
u_char *
image_get_pixels(struct image *im, size_t *stride, size_t *size)
{
	if (stride != NULL)
		*stride = im->stride;
	if (size != NULL)
		*size = im->size;
	return (im->pixels + im->geom.src_y * im->stride + im->geom.src_x * 4);
}

/* Convert an output X position to a source image position in pixels. */
static double
image_map_x(struct image_geom *ig, u_int x, u_int w)
{
	double	p;

	p = (double)x * ig->canvas_width;
	p -= (double)ig->canvas_xoff * w;
	p *= ig->src_w;
	return (p / (w * ig->scaled_width));
}

/* Convert an output Y position to a source image position in pixels. */
static double
image_map_y(struct image_geom *ig, u_int y, u_int h)
{
	double	p;

	p = ((double)y * ig->canvas_height) - ((double)ig->canvas_yoff * h);
	return ((p * ig->src_h) / (h * ig->scaled_height));
}

/* Find source image bounds for part of an output. */
int
image_get_bounds(struct image_geom *ig, u_int x, u_int y, u_int w, u_int h,
    u_int sx, u_int sy, struct image_bounds *ib)
{
	if (x >= sx || y >= sy || w == 0 || h == 0)
		return (0);
	if (w > sx - x)
		w = sx - x;
	if (h > sy - y)
		h = sy - y;

	ib->left = image_map_x(ig, x, sx);
	ib->top = image_map_y(ig, y, sy);
	ib->right = image_map_x(ig, x + w, sx);
	ib->bottom = image_map_y(ig, y + h, sy);
	if (ib->left >= ig->src_w ||
	    ib->top >= ig->src_h ||
	    ib->right <= 0 ||
	    ib->bottom <= 0)
		return (0);
	ib->area = (ib->right - ib->left) * (ib->bottom - ib->top);

	if (ib->left < 0)
		ib->left = 0;
	if (ib->top < 0)
		ib->top = 0;
	if (ib->right > ig->src_w)
		ib->right = ig->src_w;
	if (ib->bottom > ig->src_h)
		ib->bottom = ig->src_h;
	return (1);
}

/* Map a pixel in the full output canvas to the original source pixels. */
u_char *
image_get_pixel(struct image *im, u_int x, u_int y, u_int w, u_int h)
{
	static u_char		 empty[4];
	struct image_geom	*ig = &im->geom;
	double			 px, py;
	u_int			 sx, sy;

	px = image_map_x(ig, x, w);
	py = image_map_y(ig, y, h);
	if (px < 0 || py < 0 || px >= ig->src_w || py >= ig->src_h)
		return (empty);
	sx = px;
	sy = py;
	return (image_get_pixels(im, NULL, NULL) + sy * im->stride + sx * 4);
}

/* Calculate the cell size. */
void
image_size_in_cells(u_int w, u_int h, u_int xpixel, u_int ypixel, u_int *sx,
    u_int *sy)
{
	if (xpixel == 0)
		xpixel = 8;
	if (ypixel == 0)
		ypixel = 16;
	*sx = w / xpixel + (w % xpixel != 0);
	*sy = h / ypixel + (h % ypixel != 0);
}

/* Find the source pixels covered by a rectangle of placement cells. */
void
image_get_pixel_area(struct image *im, u_int x, u_int y, u_int w, u_int h,
    u_int *px, u_int *py, u_int *pw, u_int *ph)
{
	struct image_geom	*ig;
	struct image_bounds	 ib;

	*px = *py = *pw = *ph = 0;

	if (im != NULL) {
		ig = &im->geom;
		if (image_get_bounds(ig, x, y, w, h, ig->sx, ig->sy, &ib)) {
			*px = floor(ib.left);
			*py = floor(ib.top);
			*pw = ceil(ib.right) - *px;
			*ph = ceil(ib.bottom) - *py;
		}
	}
}
