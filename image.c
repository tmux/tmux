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

/* An image. */
struct image {
	u_int			 id;
	u_int			 references;
	u_int			 flags;
#define IMAGE_FLAG_OWN_PIXELS 0x1

	struct image		*parent;
	u_int			 source_id;
	u_int			 order_id;

	/* Size in cells. */
	u_int			 sx;
	u_int			 sy;

	/* Size in pixels. */
	u_int			 width;
	u_int			 height;

	/* Canvas size in pixels. */
	u_int			 canvas_width;
	u_int			 canvas_height;

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

/* Return the ID of an image. */
u_int
image_get_id(struct image *im)
{
	return (im->id);
}

/* Allocate an image and assign a unique ID. */
static struct image *
image_alloc(u_int w, u_int h, u_int cw, u_int ch, u_int sx, u_int sy,
    size_t stride, u_char *pixels)
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
	im->references = 1;
	im->source_id = im->order_id = id;
	im->width = w;
	im->height = h;
	im->canvas_width = cw;
	im->canvas_height = ch;
	im->sx = sx;
	im->sy = sy;
	im->stride = stride;
	im->size = (h - 1) * stride + w * 4;
	im->pixels = pixels;
	RB_INSERT(images, &images, im);
	return (im);
}

/* Create an image with the provided pixels. */
struct image *
image_create(u_int w, u_int h, u_int cw, u_int ch, u_int sx, u_int sy,
    u_char *pixels)
{
	struct image	*im;

	if (w == 0 || h == 0 || cw < w || ch < h)
		return (NULL);
	if (sx == 0 || sy == 0 || pixels == NULL)
		return (NULL);
	if ((uint64_t)w * h > IMAGE_SIZE_LIMIT / 4)
		return (NULL);
	if (sx > USHRT_MAX || sy > USHRT_MAX)
		return (NULL);
	if (sx * sy > IMAGE_SIZE_LIMIT)
		return (NULL);

	im = image_alloc(w, h, cw, ch, sx, sy, w * 4, pixels);
	im->flags |= IMAGE_FLAG_OWN_PIXELS;
	return (im);
}

/* Resample the crop into transparent, padded RGBA pixels. */
static u_char *
image_view_resample(struct image *src, struct image_view *view, u_int w,
    u_int h)
{
	u_char		*pixels, *row, *src_row;
	u_int		 src_x, src_y;
	uint64_t	 x, y, off;

	pixels = xcalloc(w * h, 4);
	for (y = 0; y < view->scaled_height; y++) {
		src_y = view->y + y * view->height / view->scaled_height;
		src_row = src->pixels + src_y * src->stride + view->x * 4;

		off = ((y + view->canvas_yoff) * w + view->canvas_xoff) * 4;
		row = pixels + off;

		if (view->scaled_width == view->width)
			memcpy(row, src_row, view->width * 4);
		else {
			for (x = 0; x < view->scaled_width; x++) {
				src_x = x * view->width / view->scaled_width;
				memcpy(row + x * 4, src_row + src_x * 4, 4);
			}
		}
	}
	return (pixels);
}

/* Crop and resample an image. The view takes a reference to the image. */
struct image *
image_create_view(struct image *src, struct image_view *view)
{
	struct image	*im;
	u_char		*pixels;
	u_int		 w, h;

	if (src == NULL ||
	    view == NULL ||
	    view->x >= src->width ||
	    view->y >= src->height ||
	    view->width == 0 ||
	    view->height == 0 ||
	    view->scaled_width == 0 ||
	    view->scaled_height == 0 ||
	    view->sx == 0 ||
	    view->sy == 0 ||
	    view->width > src->width - view->x ||
	    view->height > src->height - view->y ||
	    view->sx > USHRT_MAX ||
	    view->sy > USHRT_MAX ||
	    view->sx * view->sy > IMAGE_SIZE_LIMIT ||
	    view->canvas_xoff > view->canvas_width ||
	    view->scaled_width > view->canvas_width - view->canvas_xoff ||
	    view->canvas_yoff > view->canvas_height ||
	    view->scaled_height > view->canvas_height - view->canvas_yoff)
		return (NULL);

	if (view->scaled_width == view->width &&
	    view->scaled_height == view->height &&
	    view->canvas_xoff == 0 &&
	    view->canvas_yoff == 0) {
		pixels = src->pixels + view->y * src->stride + view->x * 4;
		im = image_alloc(view->width, view->height, view->canvas_width,
		    view->canvas_height, view->sx, view->sy, src->stride,
		    pixels);
	} else {
		w = view->canvas_xoff + view->scaled_width;
		h = view->canvas_yoff + view->scaled_height;
		if ((uint64_t)w * h > IMAGE_SIZE_LIMIT / 4)
			return (NULL);
		pixels = image_view_resample(src, view, w, h);
		im = image_alloc(w, h, view->canvas_width, view->canvas_height,
		    view->sx, view->sy, w * 4, pixels);
		im->flags |= IMAGE_FLAG_OWN_PIXELS;
	}
	im->parent = src;
	im->source_id = src->source_id;
	im->order_id = src->order_id;
	image_ref(src);
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
		if (im->flags & IMAGE_FLAG_OWN_PIXELS)
			free(im->pixels);
		if (im->parent != NULL)
			image_free(im->parent);
		free(im);
	}
}

/* Return image size in pixels. */
void
image_get_size(struct image *im, u_int *w, u_int *h)
{
	*w = im->width;
	*h = im->height;
}

/* Return canvas size in pixels. */
void
image_get_canvas_size(struct image *im, u_int *w, u_int *h)
{
	*w = im->canvas_width;
	*h = im->canvas_height;
}

/* Return image size in cells. */
void
image_get_size_in_cells(struct image *im, u_int *sx, u_int *sy)
{
	*sx = im->sx;
	*sy = im->sy;
}

/* Return image pixels. */
const u_char *
image_get_pixels(struct image *im, size_t *stride, size_t *size)
{
	if (stride != NULL)
		*stride = im->stride;
	if (size != NULL)
		*size = im->size;
	return (im->pixels);
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
