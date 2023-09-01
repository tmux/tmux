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

static void
image_free(struct image *im)
{
	struct screen	*s = im->s;

	TAILQ_REMOVE(&all_images, im, all_entry);
	all_images_count--;

	TAILQ_REMOVE(&s->images, im, entry);
	sixel_free(im->data);
	free(im->fallback);
	free(im);
}

int
image_free_all(struct screen *s)
{
	struct image	*im, *im1;
	int		 redraw = !TAILQ_EMPTY(&s->images);

	TAILQ_FOREACH_SAFE(im, &s->images, entry, im1)
		image_free(im);
	return (redraw);
}

/* Create text placeholder for an image. */
static void
image_fallback(char **ret, u_int sx, u_int sy)
{
	char	*buf, *label;
	u_int	 py, size, lsize;

	/* Allocate first line. */
	lsize = xasprintf(&label, "SIXEL IMAGE (%ux%u)\r\n", sx, sy) + 1;
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
image_store(struct screen *s, struct sixel_image *si)
{
	struct image	*im;

	im = xcalloc(1, sizeof *im);
	im->s = s;
	im->data = si;

	im->px = s->cx;
	im->py = s->cy;
	sixel_size_in_cells(si, &im->sx, &im->sy);

	image_fallback(&im->fallback, im->sx, im->sy);

	TAILQ_INSERT_TAIL(&s->images, im, entry);

	TAILQ_INSERT_TAIL(&all_images, im, all_entry);
	if (++all_images_count == 10/*XXX*/)
		image_free(TAILQ_FIRST(&all_images));

	return (im);
}

int
image_check_line(struct screen *s, u_int py, u_int ny)
{
	struct image	*im, *im1;
	int		 redraw = 0;

	TAILQ_FOREACH_SAFE(im, &s->images, entry, im1) {
		if (py + ny > im->py && py < im->py + im->sy) {
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
	int		 redraw = 0;

	TAILQ_FOREACH_SAFE(im, &s->images, entry, im1) {
		if (py + ny <= im->py || py >= im->py + im->sy)
			continue;
		if (px + nx <= im->px || px >= im->px + im->sx)
			continue;
		image_free(im);
		redraw = 1;
	}
	return (redraw);
}

int
image_scroll_up(struct screen *s, u_int lines)
{
	struct image		*im, *im1;
	int			 redraw = 0;
	u_int			 sx, sy;
	struct sixel_image	*new;

	TAILQ_FOREACH_SAFE(im, &s->images, entry, im1) {
		if (im->py >= lines) {
			im->py -= lines;
			redraw = 1;
			continue;
		}
		if (im->py + im->sy <= lines) {
			image_free(im);
			redraw = 1;
			continue;
		}
		sx = im->sx;
		sy = (im->py + im->sy) - lines;

		new = sixel_scale(im->data, 0, 0, 0, im->sy - sy, sx, sy, 1);
		sixel_free(im->data);
		im->data = new;

		im->py = 0;
		sixel_size_in_cells(im->data, &im->sx, &im->sy);

		free(im->fallback);
		image_fallback(&im->fallback, im->sx, im->sy);
		redraw = 1;
	}
	return (redraw);
}
