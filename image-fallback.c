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

#include <string.h>

#include "tmux.h"

/* UTF-8 characters used for image fallback. */
static const struct utf8_data fallback_shades[5] = {
	{ " ", 1, 1, 1 },
	{ "\342\226\221", 3, 3, 1 },
	{ "\342\226\222", 3, 3, 1 },
	{ "\342\226\223", 3, 3, 1 },
	{ "\342\226\210", 3, 3, 1 }
};
static const struct utf8_data fallback_half_lower = {
	"\342\226\204", 3, 3, 1
};

/* Fallback output is available on every terminal. */
int
fallback_select(__unused struct tty *tty)
{
	return (1);
}

/* Text images move with terminal scrolling. */
int
fallback_get_flags(__unused struct tty *tty)
{
	return (IMAGE_BACKEND_SCROLLS);
}

/* Get the average colour and brightness of an area. */
static int
fallback_sample(struct image *im, uint64_t px, uint64_t py, uint64_t pw,
    uint64_t ph, u_char *brightness)
{
	const u_char	*pixels, *p;
	uint64_t	 red = 0, green = 0, blue = 0, sum = 0, count;
	u_int		 w, h, cw, ch, x0, x1, y0, y1, x, y;
	size_t		 stride;

	image_get_size(im, &w, &h);
	image_get_canvas_size(im, &cw, &ch);
	pixels = image_get_pixels(im, &stride, NULL);

	x0 = px * cw / pw;
	x1 = ((px + 1) * cw + pw - 1) / pw;
	y0 = py * ch / ph;
	y1 = ((py + 1) * ch + ph - 1) / ph;
	if (x1 <= x0)
		x1 = x0 + 1;
	if (y1 <= y0)
		y1 = y0 + 1;
	if (x0 >= w || y0 >= h) {
		if (brightness != NULL)
			*brightness = 0;
		return (colour_join_rgb(0, 0, 0));
	}
	count = (uint64_t)(x1 - x0) * (y1 - y0);

	if (x1 > w)
		x1 = w;
	if (y1 > h)
		y1 = h;
	for (y = y0; y < y1; y++) {
		for (x = x0; x < x1; x++) {
			p = pixels + y * stride + x * 4;

			red += p[0] * p[3] / 255;
			green += p[1] * p[3] / 255;
			blue += p[2] * p[3] / 255;

			sum += ((2126ULL * p[0] + 7152ULL * p[1] +
			    722ULL * p[2]) / 10000) * p[3] / 255;
		}
	}
	if (brightness != NULL)
		*brightness = sum / count;
	return (colour_join_rgb(red / count, green / count, blue / count));
}

/* Set a cell to a character representing part of an image. */
static void
fallback_get_cell(struct tty *tty, struct image *im, u_int x, u_int y,
    const struct grid_cell *gc, struct grid_cell *out)
{
	static const char	 a[] = " .:-=+*#%@";
	static const u_char	 bold_shades[8] = { 0, 1, 1, 2, 2, 3, 4, 4 };
	u_int			 ch, level, nlevels, sx, sy, colours;
	u_char			 brightness;

	if (out != gc)
		memcpy(out, gc, sizeof *out);
	out->flags &= GRID_FLAG_SELECTED;
	out->attr = 0;
	out->link = 0;
	image_get_size_in_cells(im, &sx, &sy);
	if (x >= sx || y >= sy) {
		utf8_set(&out->data, ' ');
		return;
	}

	if (tty_acs_needed(tty)) {
		fallback_sample(im, x, y, sx, sy, &brightness);
		utf8_set(&out->data, a[brightness * (sizeof a - 2) / 255]);
		return;
	}

	if (tty->term->flags & (TERM_RGBCOLOURS|TERM_256COLOURS))
		colours = 256;
	else
		colours = tty_term_number(tty->term, TTYC_COLORS);
	if (colours >= 16) {
		out->bg = fallback_sample(im, x, y * 2, sx, sy * 2, NULL);
		out->fg = fallback_sample(im, x, y * 2 + 1, sx, sy * 2, NULL);
		utf8_copy(&out->data, &fallback_half_lower);
		return;
	}

	if (tty_term_has(tty->term, TTYC_NOBR))
		nlevels = 5;
	else
		nlevels = 8;
	fallback_sample(im, x, y, sx, sy, &brightness);
	level = (brightness * (nlevels - 1) + 127) / 255;

	if (nlevels == 8)
		ch = bold_shades[level];
	else
		ch = level;
	utf8_copy(&out->data, &fallback_shades[ch]);

	out->fg = 7;
	out->bg = 0;
	out->attr &= ~GRID_ATTR_BRIGHT;
	if (nlevels == 8 && (level == 2 || level == 4 || level == 7))
		out->attr |= GRID_ATTR_BRIGHT;
}

/* Get a text cell for an image grid cell, if there is one. */
const struct grid_cell *
fallback_map_cell(struct tty *tty, struct grid *gd, u_int x, u_int y,
    const struct grid_cell *gc, struct grid_cell *out, int *empty)
{
	struct image	*im;
	u_int		 src_x, src_y;
	int		 status;

	status = image_grid_get_at(gd, x, y, &im, &src_x, &src_y);
	if (status == 1) {
		fallback_get_cell(tty, im, src_x, src_y, gc, out);
		if (empty != NULL)
			*empty = 0;
		return (out);
	}
	if (status == -1 && empty != NULL)
		*empty = 1;
	return (gc);
}
