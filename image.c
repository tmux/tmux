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

/* An image. */
struct image {
	u_int			 id;
	u_int			 references;
	u_int			 flags;
#define IMAGE_FLAG_OWN_PIXELS 0x1
#define IMAGE_FLAG_NO_CURSOR 0x2

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

/* Get the ID of an image. */
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
	if ((uint64_t)sx * sy > IMAGE_SIZE_LIMIT)
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
	    (uint64_t)view->sx * view->sy > IMAGE_SIZE_LIMIT ||
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

/* Get image size in pixels. */
void
image_get_size(struct image *im, u_int *w, u_int *h)
{
	*w = im->width;
	*h = im->height;
}

/* Get canvas size in pixels. */
void
image_get_canvas_size(struct image *im, u_int *w, u_int *h)
{
	*w = im->canvas_width;
	*h = im->canvas_height;
}

/* Get image size in cells. */
void
image_get_size_in_cells(struct image *im, u_int *sx, u_int *sy)
{
	*sx = im->sx;
	*sy = im->sy;
}

/* Get image pixels. */
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

/* Find area covered by a rectangle of cells in pixels. */
void
image_get_pixel_area(struct image *im, u_int x, u_int y, u_int w, u_int h,
    u_int *px, u_int *py, u_int *pw, u_int *ph)
{
	u_int	left, top, right, bottom;

	*px = *py = *pw = *ph = 0;

	if (im == NULL || x >= im->sx || y >= im->sy || w == 0 || h == 0)
		return;
	if (w > im->sx - x)
		w = im->sx - x;
	if (h > im->sy - y)
		h = im->sy - y;

	left = (uint64_t)x * im->canvas_width / im->sx;
	top = (uint64_t)y * im->canvas_height / im->sy;
	right = ((uint64_t)(x + w) * im->canvas_width + im->sx - 1) / im->sx;
	bottom = ((uint64_t)(y + h) * im->canvas_height + im->sy - 1) / im->sy;

	if (left >= im->width || top >= im->height)
		*px = *py = 0;
	else {
		if (right <= left)
			right = left + 1;
		if (bottom <= top)
			bottom = top + 1;
		if (right > im->width)
			right = im->width;
		if (bottom > im->height)
			bottom = im->height;
		*px = left;
		*py = top;
		*pw = right - left;
		*ph = bottom - top;
	}
}

/* Suppress cursor movement when writing an image. */
void
image_set_no_cursor(struct image *im)
{
	im->flags |= IMAGE_FLAG_NO_CURSOR;
}

/* Return whether writing the image should leave the cursor in place. */
int
image_has_no_cursor(struct image *im)
{
	return (!!(im->flags & IMAGE_FLAG_NO_CURSOR));
}

/* Return the original source image ID. */
u_int
image_get_source_id(struct image *im)
{
	return (im->source_id);
}

/* Return the image's original ordering ID. */
u_int
image_get_order_id(struct image *im)
{
	return (im->order_id);
}

/* Set the original ordering ID after replacing an image. */
void
image_set_order_id(struct image *im, u_int id)
{
	im->order_id = id;
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
	if (needed == 0)
		out = xmalloc(1);
	else
		out = xmalloc(needed);
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
