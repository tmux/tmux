/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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

static struct images	all_images = TAILQ_HEAD_INITIALIZER(all_images);
static u_int		all_images_count;
#define MAX_IMAGE_COUNT 20

static void printflike(3, 4)
image_log(struct image *im, const char* from, const char* fmt, ...)
{
	va_list	ap;
	char	s[128];

	if (log_get_level() == 0)
		return;

	if (fmt == NULL) {
		log_debug("%s: %p (%ux%u %u,%u)", from, im, im->sx, im->sy,
		    im->px, im->py);
		return;
	}

	va_start(ap, fmt);
	vsnprintf(s, sizeof s, fmt, ap);
	va_end(ap);

	log_debug("%s: %p (%ux%u %u,%u): %s", from, im, im->sx, im->sy,
	    im->px, im->py, s);
}

static void
image_free(struct image *im)
{
	struct screen	*s = im->s;

	image_log(im, __func__, NULL);

	TAILQ_REMOVE(&all_images, im, all_entry);
	all_images_count--;

	TAILQ_REMOVE(&s->images, im, entry);

	switch (im->type) {
#ifdef ENABLE_SIXEL_IMAGES
	case IMAGE_SIXEL:
		sixel_free(im->data.sixel);
		break;
#endif
#ifdef ENABLE_KITTY_IMAGES
	case IMAGE_KITTY:
		kitty_free(im->data.kitty);
		break;
#endif
	default:
		break;
	}

	free(im->fallback);
	free(im);
}

int
image_free_all(struct screen *s)
{
	struct image	*im, *im1;
	int		 redraw = !TAILQ_EMPTY(&s->images);

	if (redraw)
		log_debug ("%s", __func__);
	TAILQ_FOREACH_SAFE(im, &s->images, entry, im1)
		image_free(im);
	return (redraw);
}

/* Create text placeholder for an image. */
static void
image_fallback(char **ret, enum image_type type, u_int sx, u_int sy)
{
	char	*buf, *label;
	u_int	 py, size, lsize;
	const char *type_name;

	switch (type) {
#ifdef ENABLE_SIXEL_IMAGES
	case IMAGE_SIXEL:
		type_name = "SIXEL";
		break;
#endif
#ifdef ENABLE_KITTY_IMAGES
	case IMAGE_KITTY:
		type_name = "KITTY";
		break;
#endif
	default:
		type_name = "UNKNOWN";
		break;
	}

	/* Allocate first line. */
	lsize = xasprintf(&label, "%s IMAGE (%ux%u)\r\n", type_name, sx, sy) + 1;
	if (sx < lsize - 3)
		size = lsize - 1;
	else
		size = sx + 2;

	/* Remaining lines. Every placeholder line has \r\n at the end. */
	size += (sx + 2) * (sy - 1) + 1;
	*ret = buf = xmalloc(size);

	/* Render first line. */
	if (sx < lsize - 3) {
		memcpy(buf, label, lsize);
		buf += lsize - 1;
	} else {
		memcpy(buf, label, lsize - 3);
		buf += lsize - 3;
		memset(buf, '+', sx - lsize + 3);
		buf += sx - lsize + 3;
		snprintf(buf, 3, "\r\n");
		buf += 2;
	}

	/* Remaining lines. */
	for (py = 1; py < sy; py++) {
		memset(buf, '+', sx);
		buf += sx;
		snprintf(buf, 3, "\r\n");
		buf += 2;
	}

	free(label);
}

struct image*
image_store(struct screen *s, enum image_type type, void *data)
{
	struct image	*im;

	im = xcalloc(1, sizeof *im);

	im->type = type;
	im->s = s;

	im->px = s->cx;
	im->py = s->cy;

	switch (type) {
#ifdef ENABLE_SIXEL_IMAGES
	case IMAGE_SIXEL:
		im->data.sixel = data;
		sixel_size_in_cells(im->data.sixel, &im->sx, &im->sy);
		break;
#endif
#ifdef ENABLE_KITTY_IMAGES
	case IMAGE_KITTY:
		im->data.kitty = data;
		kitty_size_in_cells(im->data.kitty, &im->sx, &im->sy);
		break;
#endif
	default:
		break;
	}

	image_fallback(&im->fallback, type, im->sx, im->sy);

	image_log(im, __func__, NULL);
	TAILQ_INSERT_TAIL(&s->images, im, entry);

	TAILQ_INSERT_TAIL(&all_images, im, all_entry);
	if (++all_images_count == MAX_IMAGE_COUNT)
		image_free(TAILQ_FIRST(&all_images));

	return (im);
}

int
image_check_line(struct screen *s, u_int py, u_int ny)
{
	struct image	*im, *im1;
	int		 redraw = 0, in;

	TAILQ_FOREACH_SAFE(im, &s->images, entry, im1) {
		in = (py + ny > im->py && py < im->py + im->sy);
		image_log(im, __func__, "py=%u, ny=%u, in=%d", py, ny, in);
		if (in) {
			image_free(im);
			redraw = 1;
		}
	}
	return (redraw);
}

int
image_check_area(struct screen *s, u_int px, u_int py, u_int nx, u_int ny)
{
	struct image	*im, *im1;
	int		 redraw = 0, in;

	TAILQ_FOREACH_SAFE(im, &s->images, entry, im1) {
		in = (py < im->py + im->sy &&
		    py + ny > im->py &&
		    px < im->px + im->sx &&
		    px + nx > im->px);
		image_log(im, __func__, "py=%u, ny=%u, in=%d", py, ny, in);
		if (in) {
			image_free(im);
			redraw = 1;
		}
	}
	return (redraw);
}

int
image_scroll_up(struct screen *s, u_int lines)
{
	struct image		*im, *im1;
	int			 redraw = 0;
#ifdef ENABLE_SIXEL_IMAGES
	struct sixel_image	*new;
	u_int			 sx, sy;
#endif

	TAILQ_FOREACH_SAFE(im, &s->images, entry, im1) {
		if (im->py >= lines) {
			image_log(im, __func__, "1, lines=%u", lines);
			im->py -= lines;
			redraw = 1;
			continue;
		}
		if (im->py + im->sy <= lines) {
			image_log(im, __func__, "2, lines=%u", lines);
			image_free(im);
			redraw = 1;
			continue;
		}

		/* Image is partially scrolled off - need to crop it */
		switch (im->type) {
#ifdef ENABLE_SIXEL_IMAGES
		case IMAGE_SIXEL:
			sx = im->sx;
			sy = (im->py + im->sy) - lines;
			image_log(im, __func__, "sixel, lines=%u, sy=%u",
			    lines, sy);

			new = sixel_scale(im->data.sixel, 0, 0, 0, im->sy - sy,
			    sx, sy, 1);
			sixel_free(im->data.sixel);
			im->data.sixel = new;

			im->py = 0;
			sixel_size_in_cells(im->data.sixel, &im->sx, &im->sy);

			free(im->fallback);
			image_fallback(&im->fallback, im->type, im->sx, im->sy);
			redraw = 1;
			break;
#endif
#ifdef ENABLE_KITTY_IMAGES
		case IMAGE_KITTY:
			/*
			 * For kitty images, we can't rescale - the terminal
			 * owns the placement. Just adjust position and let
			 * the terminal handle clipping.
			 */
			im->py = 0;
			redraw = 1;
			break;
#endif
		default:
			break;
		}
	}
	return (redraw);
}
