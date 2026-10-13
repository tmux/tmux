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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "tmux.h"

/* Maximum SIXEL width and height */
#define SIXEL_WIDTH_LIMIT 10000
#define SIXEL_HEIGHT_LIMIT 10000

/* Line of a SIXEL image. */
struct sixel_line {
	/* Number of pixels in this row. */
	u_int		 x;

	/* Palette index for each pixel in the row. */
	uint16_t	*data;
};

/* SIXEL image. */
struct sixel_image {
	/* Decoded image dimensions in pixels. */
	u_int			 x;
	u_int			 y;

	/* Terminal cell pixel dimensions used for scaling. */
	u_int			 xpixel;
	u_int			 ypixel;

	/* SIXEL palette. */
	u_int			*colours;
	u_int			 ncolours;

	/* DCS parameters. */
	u_int			 p1;
	u_int			 p2;

	/* Current parser position and colour register. */
	u_int			 dx;
	u_int			 dy;
	u_int			 dc;

	/* Lines of palette indexes. */
	struct sixel_line	*lines;
};

/* Grow a SIXEL image to contain a line. */
static int
sixel_parse_expand_lines(struct sixel_image *si, u_int y)
{
	if (y <= si->y)
		return (0);
	if (y > SIXEL_HEIGHT_LIMIT)
		return (1);
	si->lines = xrecallocarray(si->lines, si->y, y, sizeof *si->lines);
	si->y = y;
	return (0);
}

/* Grow a SIXEL line to contain a pixel. */
static int
sixel_parse_expand_line(struct sixel_image *si, struct sixel_line *sl, u_int x)
{
	if (x <= sl->x)
		return (0);
	if (x > SIXEL_WIDTH_LIMIT)
		return (1);
	if (x > si->x)
		si->x = x;
	sl->data = xrecallocarray(sl->data, sl->x, si->x, sizeof *sl->data);
	sl->x = si->x;
	return (0);
}

/* Return a SIXEL palette index at a pixel. */
static u_int
sixel_get_pixel(struct sixel_image *si, u_int x, u_int y)
{
	struct sixel_line	*sl;

	if (y >= si->y)
		return (0);
	sl = &si->lines[y];
	if (x >= sl->x)
		return (0);
	return (sl->data[x]);
}

/* Set a SIXEL palette index at a pixel. */
static int
sixel_set_pixel(struct sixel_image *si, u_int x, u_int y, u_int c)
{
	struct sixel_line	*sl;

	if (sixel_parse_expand_lines(si, y + 1) != 0)
		return (1);
	sl = &si->lines[y];
	if (sixel_parse_expand_line(si, sl, x + 1) != 0)
		return (1);
	sl->data[x] = c;
	return (0);
}

/* Write a SIXEL column. */
static int
sixel_parse_write(struct sixel_image *si, u_int ch)
{
	u_int			 i;

	for (i = 0; i < 6; i++) {
		if (ch & (1 << i)) {
			if (sixel_set_pixel(si, si->dx, si->dy + i, si->dc))
				return (1);
		}
	}
	return (0);
}

/* Parse a number. */
static int
sixel_parse_number(const char *cp, char **endptr, u_int *value)
{
	unsigned long long	number;

	if (*cp < '0' || *cp > '9')
		return (1);
	errno = 0;
	number = strtoull(cp, endptr, 10);
	if (errno == ERANGE || number > UINT_MAX)
		return (1);
	*value = number;
	return (0);
}

/* Parse SIXEL attributes. */
static const char *
sixel_parse_attributes(struct sixel_image *si, const char *cp, const char *end)
{
	const char	*last;
	char		*endptr;
	u_int		 x, y;

	last = cp;
	while (last != end) {
		if (*last != ';' && (*last < '0' || *last > '9'))
			break;
		last++;
	}
	strtoul(cp, &endptr, 10);
	if (endptr == last || *endptr != ';')
		return (last);
	strtoul(endptr + 1, &endptr, 10);
	if (endptr == last)
		return (last);
	if (*endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}

	if (sixel_parse_number(endptr + 1, &endptr, &x) != 0) {
		log_debug("%s: invalid number", __func__);
		return (NULL);
	}
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	if (x > SIXEL_WIDTH_LIMIT) {
		log_debug("%s: image is too wide", __func__);
		return (NULL);
	}
	if (sixel_parse_number(endptr + 1, &endptr, &y) != 0) {
		log_debug("%s: invalid number", __func__);
		return (NULL);
	}
	if (endptr != last) {
		log_debug("%s: extra ;", __func__);
		return (NULL);
	}
	if (y > SIXEL_HEIGHT_LIMIT) {
		log_debug("%s: image is too tall", __func__);
		return (NULL);
	}

	si->x = x;
	sixel_parse_expand_lines(si, y);

	return (last);
}

/* Parse a SIXEL colour register sequence. */
static const char *
sixel_parse_colour(struct sixel_image *si, const char *cp, const char *end)
{
	const char	*last;
	char		*endptr;
	u_int		 c, type, c1, c2, c3;

	last = cp;
	while (last != end) {
		if (*last != ';' && (*last < '0' || *last > '9'))
			break;
		last++;
	}

	if (sixel_parse_number(cp, &endptr, &c) != 0) {
		log_debug("%s: invalid number", __func__);
		return (NULL);
	}
	if (c > SIXEL_COLOUR_REGISTERS) {
		log_debug("%s: too many colours", __func__);
		return (NULL);
	}
	si->dc = c + 1;
	if (endptr == last || *endptr != ';')
		return (last);

	if (sixel_parse_number(endptr + 1, &endptr, &type) != 0) {
		log_debug("%s: invalid number", __func__);
		return (NULL);
	}
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	if (sixel_parse_number(endptr + 1, &endptr, &c1) != 0) {
		log_debug("%s: invalid number", __func__);
		return (NULL);
	}
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	if (sixel_parse_number(endptr + 1, &endptr, &c2) != 0) {
		log_debug("%s: invalid number", __func__);
		return (NULL);
	}
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	if (sixel_parse_number(endptr + 1, &endptr, &c3) != 0) {
		log_debug("%s: invalid number", __func__);
		return (NULL);
	}
	if (endptr != last) {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}

	if ((type != 1 && type != 2) ||
	    (type == 1 && (c1 > 360 || c2 > 100 || c3 > 100)) ||
	    (type == 2 && (c1 > 100 || c2 > 100 || c3 > 100))) {
		log_debug("%s: invalid color %u;%u;%u;%u", __func__, type,
		    c1, c2, c3);
		return (NULL);
	}

	if (c + 1 > si->ncolours) {
		si->colours = xrecallocarray(si->colours, si->ncolours, c + 1,
		    sizeof *si->colours);
		si->ncolours = c + 1;
	}
	si->colours[c] = (type << 25) | (c1 << 16) | (c2 << 8) | c3;
	return (last);
}

/* Parse a SIXEL repeat sequence. */
static const char *
sixel_parse_repeat(struct sixel_image *si, const char *cp, const char *end)
{
	const char	*last;
	char		 tmp[32], ch;
	u_int		 n = 0, i;
	const char	*errstr = NULL;

	last = cp;
	while (last != end) {
		if (*last < '0' || *last > '9')
			break;
		tmp[n++] = *last++;
		if (n == (sizeof tmp) - 1) {
			log_debug("%s: repeat not terminated", __func__);
			return (NULL);
		}
	}
	if (n == 0 || last == end) {
		log_debug("%s: repeat not terminated", __func__);
		return (NULL);
	}
	tmp[n] = '\0';

	n = strtonum(tmp, 1, SIXEL_WIDTH_LIMIT, &errstr);
	if (n == 0 || errstr != NULL) {
		log_debug("%s: repeat too wide", __func__);
		return (NULL);
	}

	ch = (*last++) - 0x3f;
	for (i = 0; i < n; i++) {
		if (sixel_parse_write(si, ch) != 0) {
			log_debug("%s: width limit reached", __func__);
			return (NULL);
		}
		si->dx++;
	}
	return (last);
}

/* Parse SIXEL data into an indexed image. */
struct sixel_image *
sixel_parse(const char *buf, size_t len, u_int p1, u_int p2, u_int xpixel,
    u_int ypixel)
{
	struct sixel_image	*si;
	const char		*cp = buf, *end = buf + len;
	char			 ch;

	if (len == 0 || len == 1 || *cp++ != 'q') {
		log_debug("%s: empty image", __func__);
		return (NULL);
	}

	si = xcalloc(1, sizeof *si);
	si->xpixel = xpixel;
	si->ypixel = ypixel;
	si->p1 = p1;
	si->p2 = p2;

	while (cp != end) {
		ch = *cp++;
		switch (ch) {
		case '"':
			cp = sixel_parse_attributes(si, cp, end);
			if (cp == NULL)
				goto bad;
			break;
		case '#':
			cp = sixel_parse_colour(si, cp, end);
			if (cp == NULL)
				goto bad;
			break;
		case '!':
			cp = sixel_parse_repeat(si, cp, end);
			if (cp == NULL)
				goto bad;
			break;
		case '-':
			si->dx = 0;
			si->dy += 6;
			break;
		case '$':
			si->dx = 0;
			break;
		default:
			if (ch < 0x20)
				break;
			if (ch < 0x3f || ch > 0x7e)
				goto bad;
			if (sixel_parse_write(si, ch - 0x3f) != 0) {
				log_debug("%s: width limit reached", __func__);
				goto bad;
			}
			si->dx++;
			break;
		}
	}

	if (si->x == 0 || si->y == 0)
		goto bad;
	return (si);

bad:
	sixel_free(si);
	return (NULL);
}

/* Free a SIXEL image. */
void
sixel_free(struct sixel_image *si)
{
	u_int	y;

	for (y = 0; y < si->y; y++)
		free(si->lines[y].data);
	free(si->lines);

	free(si->colours);
	free(si);
}

/* Get the size of a SIXEL image in cells. */
static void
sixel_size_in_cells(struct sixel_image *si, u_int *x, u_int *y)
{
	if (si->xpixel == 0)
		si->xpixel = 8;
	if (si->ypixel == 0)
		si->ypixel = 16;
	image_size_in_cells(si->x, si->y, si->xpixel, si->ypixel, x, y);
}

/* Log a SIXEL image. */
void
sixel_log(struct sixel_image *si)
{
	struct sixel_line	*sl;
	char			 s[SIXEL_WIDTH_LIMIT + 1];
	u_int			 i, x, y, cx, cy;

	sixel_size_in_cells(si, &cx, &cy);
	log_debug("%s: image %ux%u (%ux%u)", __func__, si->x, si->y, cx, cy);
	for (i = 0; i < si->ncolours; i++)
		log_debug("%s: colour %u is %07x", __func__, i, si->colours[i]);
	for (y = 0; y < si->y; y++) {
		sl = &si->lines[y];
		for (x = 0; x < si->x; x++) {
			if (x >= sl->x)
				s[x] = '_';
			else if (sl->data[x] != 0)
				s[x] = '0' + (sl->data[x] - 1) % 10;
			else
				s[x] = '.';
		}
		s[x] = '\0';
		log_debug("%s: %4u: %s", __func__, y, s);
	}
}

/* Convert one HLS component to RGB. */
static double
sixel_hue(double p, double q, double t)
{
	if (t < 0)
		t += 1;
	if (t > 1)
		t -= 1;
	if (t < 1. / 6)
		return (p + (q - p) * 6 * t);
	if (t < 1. / 2)
		return (q);
	if (t < 2. / 3)
		return (p + (q - p) * (2. / 3 - t) * 6);
	return (p);
}

/* Convert a SIXEL colour register to RGB. */
static void
sixel_colour_to_rgb(u_int colour, u_char *r, u_char *g, u_char *b)
{
	u_int	type = (colour >> 25);
	double	h, l, s, p, q;

	if (type == 2) {
		*r = (((colour >> 16) & 0xff) * 255 + 50) / 100;
		*g = (((colour >> 8) & 0xff) * 255 + 50) / 100;
		*b = ((colour & 0xff) * 255 + 50) / 100;
		return;
	}
	if (type != 1) {
		*r = *g = *b = 0;
		return;
	}

	h = ((colour >> 16) & 0x1ff) / 360.;
	l = ((colour >> 8) & 0xff) / 100.;
	s = (colour & 0xff) / 100.;
	if (s == 0) {
		*r = *g = *b = l * 255 + 0.5;
		return;
	}
	q = l < 0.5 ? l * (1 + s) : l + s - l * s;
	p = 2 * l - q;

	/* SIXEL HLS has blue at 0, red at 120 and green at 240 degrees. */
	*r = sixel_hue(p, q, h) * 255 + 0.5;
	*g = sixel_hue(p, q, h - 1. / 3) * 255 + 0.5;
	*b = sixel_hue(p, q, h + 1. / 3) * 255 + 0.5;
}

/* Convert a SIXEL image into the common RGBA format. */
struct image *
sixel_to_image(struct sixel_image *si)
{
	struct image	*im;
	u_char		*pixels, *pixel, r, g, b;
	u_int		 x, y, c, sx, sy;

	if ((uint64_t)si->x * si->y > IMAGE_SIZE_LIMIT / 4)
		return (NULL);

	sixel_size_in_cells(si, &sx, &sy);
	if ((uint64_t)sx * si->xpixel > UINT_MAX ||
	    (uint64_t)sy * si->ypixel > UINT_MAX)
		return (NULL);

	pixels = xcalloc((size_t)si->x * si->y, 4);
	for (y = 0; y < si->y; y++) {
		for (x = 0; x < si->x; x++) {
			pixel = pixels + ((size_t)y * si->x + x) * 4;

			c = sixel_get_pixel(si, x, y);
			if (c == 0) {
				pixel[3] = (si->p2 == 1 ? 0 : 255);
				continue;
			}
			if (--c < si->ncolours)
				sixel_colour_to_rgb(si->colours[c], &r, &g, &b);
			else
				r = g = b = 0;

			pixel[0] = r;
			pixel[1] = g;
			pixel[2] = b;
			pixel[3] = 255;
		}
	}
	im = image_create(si->x, si->y, sx * si->xpixel, sy * si->ypixel, sx,
	    sy, pixels);
	if (im == NULL)
		free(pixels);
	return (im);
}

/* Write a SIXEL image. */
void
sixel_write(__unused struct screen_write_ctx *ctx, struct sixel_image *si,
    __unused u_int bg)
{
	struct image *im;

	im = sixel_to_image(si);
	sixel_free(si);
	if (im == NULL)
		return;
	/* XXX image_write_sixel */
	image_free(im);
}
