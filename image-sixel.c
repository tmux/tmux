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
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* Maximum SIXEL width and height. */
#define SIXEL_WIDTH_LIMIT 10000
#define SIXEL_HEIGHT_LIMIT 10000

/* Number of colours in a generated SIXEL palette. */
#define SIXEL_PALETTE_SIZE 256

/* Number of histogram levels for each RGB component. */
#define SIXEL_HGRAM_LEVELS 32

/* Number of bins in the RGB histogram. */
#define SIXEL_HGRAM_SIZE \
	(SIXEL_HGRAM_LEVELS * SIXEL_HGRAM_LEVELS * SIXEL_HGRAM_LEVELS)

/* Maximum number of bytes in the SIXEL cache. */
#define SIXEL_CACHE_SIZE_LIMIT IMAGE_SIZE_LIMIT

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

	/* Terminal cell pixel size used for decoding. */
	u_int			 xpixel;
	u_int			 ypixel;

	/* SIXEL palette. */
	u_int			*colours;
	u_int			 ncolours;
	u_int			 used_colours;

	/* SIXEL raster attributes. */
	u_int			 ra_x;
	u_int			 ra_y;

	/* DCS background parameter. */
	u_int			 p2;

	/* Current parser position and colour register. */
	u_int			 dx;
	u_int			 dy;
	u_int			 dc;

	/* Lines of palette indexes and the bytes allocated for them. */
	struct sixel_line	*lines;
	size_t			 allocated;
};

/* One colour's encoded columns in the current six-row band. */
struct sixel_plane {
	/* Position of the next encoded colour plane. */
	u_int	 next_x;
	u_int	 next_y;

	/* State used while encoding SIXEL patterns. */
	u_int	 count;
	char	 pattern;
	char	 next_pattern;

	/* Output buffer and its allocation and used lengths. */
	size_t	 stream_len;
	size_t	 stream_used;
	char	*stream;
};

/* Adjacent placement rows buffered for one SIXEL output sequence. */
struct sixel_pending {
	struct image	*image;

	/* Source in image in cells. */
	u_int		 src_x;
	u_int		 src_y;

	/* Size in cells. */
	u_int		 w;
	u_int		 h;

	/* Destination on terminal in cells. */
	u_int		 dst_x;
	u_int		 dst_y;
};

/* Item in the cache. */
struct sixel_item {
	u_int			 id;
	struct sixel_image	*si;
	size_t			 size;

	u_int			 cw;
	u_int			 ch;

	RB_ENTRY(sixel_item)	tree_entry;
	TAILQ_ENTRY(sixel_item)	entry;
};

RB_HEAD(sixel_tree, sixel_item);
TAILQ_HEAD(sixel_items, sixel_item);
static struct sixel_tree sixel_tree = RB_INITIALIZER(&sixel_tree);
static struct sixel_items sixel_cache = TAILQ_HEAD_INITIALIZER(sixel_cache);
static size_t sixel_cache_size;

/* RGB histogram entry. */
struct sixel_hgram {
	u_int		count;
	uint64_t	r;
	uint64_t	g;
	uint64_t	b;
};

/* Bounds and population of one palette region. */
struct sixel_box {
	u_int	rmin;
	u_int	rmax;
	u_int	gmin;
	u_int	gmax;
	u_int	bmin;
	u_int	bmax;
	u_int	count;
};

/* One generated palette colour. */
struct sixel_rgb {
	u_char	r;
	u_char	g;
	u_char	b;
};

/* Pixels and dimensions of the image being encoded. */
struct sixel_source {
	struct image	*image;

	/* Destination canvas size in pixels. */
	u_int		 dst_w;
	u_int		 dst_h;

	/* Output position and size in the destination canvas in pixels. */
	u_int		 dst_x;
	u_int		 dst_y;
	u_int		 out_w;
	u_int		 out_h;
};

static void sixel_free(struct sixel_image *);

/* SIXEL cache item comparison. */
static int
sixel_cache_cmp(struct sixel_item *a, struct sixel_item *b)
{
	if (a->id < b->id)
		return (-1);
	if (a->id > b->id)
		return (1);
	if (a->cw < b->cw)
		return (-1);
	if (a->cw > b->cw)
		return (1);
	if (a->ch < b->ch)
		return (-1);
	if (a->ch > b->ch)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(sixel_tree, sixel_item, tree_entry, sixel_cache_cmp);

/* Grow a SIXEL image to contain a line. */
static int
sixel_parse_expand_lines(struct sixel_image *si, u_int y)
{
	u_int	height = si->y;

	if (y > height)
		height = y;
	if (height > SIXEL_HEIGHT_LIMIT)
		return (1);
	if (si->x * height > IMAGE_SIZE_LIMIT / 4)
		return (1);
	if (y <= si->y)
		return (0);
	si->lines = xrecallocarray(si->lines, si->y, y, sizeof *si->lines);
	si->y = y;
	return (0);
}

/* Grow a SIXEL line to contain a pixel. */
static int
sixel_parse_expand_line(struct sixel_image *si, struct sixel_line *sl, u_int x)
{
	u_int	width = si->x;
	size_t	added;

	if (x > width)
		width = x;
	if (width > SIXEL_WIDTH_LIMIT)
		return (1);
	if (width * si->y > IMAGE_SIZE_LIMIT / 4)
		return (1);
	if (x <= sl->x)
		return (0);
	added = (width - sl->x) * sizeof *sl->data;
	if (added > IMAGE_SIZE_LIMIT / 2 - si->allocated)
		return (1);
	si->x = width;
	sl->data = xrecallocarray(sl->data, sl->x, si->x, sizeof *sl->data);
	sl->x = si->x;
	si->allocated += added;
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
	if (sixel_parse_expand_lines(si, y) != 0) {
		log_debug("%s: size limit reached", __func__);
		return (NULL);
	}

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
			log_debug("%s: size limit reached", __func__);
			return (NULL);
		}
		si->dx++;
	}
	return (last);
}

/* Parse SIXEL data into an indexed image. */
struct sixel_image *
sixel_parse(const char *buf, size_t len, u_int p2, u_int xpixel, u_int ypixel)
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
			if (ch < 0x3f || ch > 0x7e)
				break;
			if (sixel_parse_write(si, ch - 0x3f) != 0) {
				log_debug("%s: size limit reached", __func__);
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
static void
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
sixel_colour_to_rgb(u_int c, u_char *r, u_char *g, u_char *b)
{
	u_int	type = (c >> 25);
	double	h, l, s, p, q;

	if (type == 2) {
		*r = (((c >> 16) & 0xff) * 255 + 50) / 100;
		*g = (((c >> 8) & 0xff) * 255 + 50) / 100;
		*b = ((c & 0xff) * 255 + 50) / 100;
		return;
	}
	if (type != 1) {
		*r = *g = *b = 0;
		return;
	}

	h = ((c >> 16) & 0x1ff) / 360.;
	l = ((c >> 8) & 0xff) / 100.;
	s = (c & 0xff) / 100.;
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
static struct image *
sixel_to_image(struct sixel_image *si)
{
	struct image		*im;
	struct image_geom	 ig = { 0 };
	u_char			*pixels, *pixel, r, g, b;
	u_int			 x, y, c, sx, sy;

	if ((uint64_t)si->x * si->y > IMAGE_SIZE_LIMIT / 4)
		return (NULL);

	sixel_size_in_cells(si, &sx, &sy);
	if ((uint64_t)sx * si->xpixel > UINT_MAX ||
	    (uint64_t)sy * si->ypixel > UINT_MAX)
		return (NULL);

	pixels = xcalloc(si->x * si->y, 4);
	for (y = 0; y < si->y; y++) {
		for (x = 0; x < si->x; x++) {
			pixel = pixels + (y * si->x + x) * 4;

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
	ig.src_w = ig.scaled_width = si->x;
	ig.src_h = ig.scaled_height = si->y;
	ig.canvas_width = sx * si->xpixel;
	ig.canvas_height = sy * si->ypixel;
	ig.sx = sx;
	ig.sy = sy;
	im = image_create(IMAGE_TYPE_SIXEL, &ig, pixels);
	if (im == NULL)
		free(pixels);
	return (im);
}

/* Write a SIXEL image. */
void
sixel_write(struct screen_write_ctx *ctx, struct sixel_image *si, u_int bg)
{
	struct image *im;

	im = sixel_to_image(si);
	sixel_free(si);
	if (im == NULL)
		return;
	image_write(ctx, im, bg, 0, 0);
	image_free(im);
}

/* Append data to a growing SIXEL output buffer. */
static void
sixel_print_add(char **buf, size_t *len, size_t *used, const char *s,
    size_t slen)
{
	while (*used + slen + 1 > *len) {
		*buf = xreallocarray(*buf, 2, *len);
		(*len) *= 2;
	}
	memcpy(*buf + *used, s, slen);
	(*used) += slen;
}

/* Append a SIXEL character repetition to an output buffer. */
static void
sixel_print_repeat(char **buf, size_t *len, size_t *used, u_int count, char ch)
{
	char	tmp[16];
	size_t	tmplen;

	if (count == 1)
		sixel_print_add(buf, len, used, &ch, 1);
	else if (count == 2) {
		sixel_print_add(buf, len, used, &ch, 1);
		sixel_print_add(buf, len, used, &ch, 1);
	} else if (count == 3) {
		sixel_print_add(buf, len, used, &ch, 1);
		sixel_print_add(buf, len, used, &ch, 1);
		sixel_print_add(buf, len, used, &ch, 1);
	} else if (count != 0) {
		tmplen = xsnprintf(tmp, sizeof tmp, "!%u%c", count, ch);
		sixel_print_add(buf, len, used, tmp, tmplen);
	}
}

/* Build compressed SIXEL output planes for a sixel row. */
static void
sixel_print_compress_colors(struct sixel_image *si, struct sixel_plane *planes,
    u_int y, u_int *active, u_int *nactive)
{
	u_int			 i, x, c, dx, pixels[6];
	struct sixel_plane	*plane = NULL;
	struct sixel_line	*sl;

	for (x = 0; x < si->x; x++) {
		/* Collect this column's six-bit pattern for each colour. */
		for (i = 0; i < 6; i++) {
			pixels[i] = 0;
			if (y + i < si->y) {
				sl = &si->lines[y + i];
				if (x < sl->x && sl->data[x] != 0) {
					pixels[i] = sl->data[x];
					c = sl->data[x] - 1;
					planes[c].next_pattern |= 1 << i;
				}
			}
		}

		/* Compress patterns and gaps in active colour planes. */
		for (i = 0; i < 6; i++) {
			if (pixels[i] == 0)
				continue;

			c = pixels[i] - 1;
			plane = &planes[c];
			if (plane->next_x == x + 1)
				continue;

			if (plane->next_y < y + 1) {
				plane->next_y = y + 1;
				active[(*nactive)++] = c;
			}

			dx = x - plane->next_x;
			if (plane->pattern != plane->next_pattern || dx != 0) {
				sixel_print_repeat(&plane->stream,
				    &plane->stream_len, &plane->stream_used,
				    plane->count, plane->pattern + 0x3f);
				sixel_print_repeat(&plane->stream,
				    &plane->stream_len, &plane->stream_used,
				    dx, '?');
				plane->pattern = plane->next_pattern;
				plane->count = 0;
			}
			plane->count++;
			plane->next_pattern = 0;
			plane->next_x = x + 1;
		}
	}
}

/* Encode an indexed SIXEL image for terminal output. */
static char *
sixel_print(struct sixel_image *si, size_t *size)
{
	char			*buf, tmp[64];
	size_t			 len, used = 0, tmplen;
	u_int			 used_colours, i, c, y;
	u_int			*active, nactive;
	struct sixel_plane	*planes, *plane;

	used_colours = si->used_colours;
	if (used_colours == 0)
		return (NULL);

	len = 8192;
	buf = xmalloc(len);

	sixel_print_add(&buf, &len, &used, "\033P9;1q", 6);

	tmplen = xsnprintf(tmp, sizeof tmp, "\"1;1;%u;%u", si->ra_x,
	    si->ra_y);
	sixel_print_add(&buf, &len, &used, tmp, tmplen);

	/* Track colour planes containing pixels in the current six-row band. */
	planes = xcalloc(used_colours, sizeof *planes);
	active = xcalloc(used_colours, sizeof *active);

	/* Emit the palette before the encoded colour planes. */
	for (i = 0; i < si->ncolours; i++) {
		c = si->colours[i];
		tmplen = xsnprintf(tmp, sizeof tmp, "#%u;%u;%u;%u;%u",
		    i, c >> 25, (c >> 16) & 0x1ff, (c >> 8) & 0xff, c & 0xff);
		sixel_print_add(&buf, &len, &used, tmp, tmplen);
	}

	for (i = 0; i < used_colours; i++) {
		plane = &planes[i];
		plane->stream_len = 8;
		plane->stream = xmalloc(plane->stream_len);
	}

	/* Encode each six-row band using only its active colours. */
	for (y = 0; y < si->y; y += 6) {
		nactive = 0;
		sixel_print_compress_colors(si, planes, y, active, &nactive);

		for (i = 0; i < nactive; i++) {
			c = active[i];
			plane = &planes[c];
			tmplen = xsnprintf(tmp, sizeof tmp, "#%u", c);
			sixel_print_add(&buf, &len, &used, tmp, tmplen);
			sixel_print_add(&buf, &len, &used, plane->stream,
			    plane->stream_used);
			sixel_print_repeat(&buf, &len, &used, plane->count,
			    plane->pattern + 0x3f);
			sixel_print_add(&buf, &len, &used, "$", 1);
			plane->stream_used = plane->next_x = plane->count = 0;
		}

		if (buf[used - 1] == '$')
			used--;
		sixel_print_add(&buf, &len, &used, "-", 1);
	}
	if (buf[used - 1] == '-')
		used--;

	sixel_print_add(&buf, &len, &used, "\033\\", 2);

	buf[used] = '\0';
	if (size != NULL)
		*size = used;

	for (i = 0; i < used_colours; i++)
		free(planes[i].stream);
	free(active);
	free(planes);

	return (buf);
}

/* Update the occupied bounds and pixel count of a colour region. */
static void
sixel_box_update(struct sixel_box *box, struct sixel_hgram *hg)
{
	struct sixel_hgram	*entry;
	u_int			 r, g, b, rmax = 0, gmax = 0, bmax = 0;
	u_int			 rmin = SIXEL_HGRAM_LEVELS;
	u_int			 gmin = SIXEL_HGRAM_LEVELS;
	u_int			 bmin = SIXEL_HGRAM_LEVELS;
	u_int			 count = 0;

	for (r = box->rmin; r <= box->rmax; r++) {
		for (g = box->gmin; g <= box->gmax; g++) {
			for (b = box->bmin; b <= box->bmax; b++) {
				entry = &hg[(r << 10)|(g << 5)|b];
				if (entry->count != 0) {
					if (r < rmin)
						rmin = r;
					if (r > rmax)
						rmax = r;
					if (g < gmin)
						gmin = g;
					if (g > gmax)
						gmax = g;
					if (b < bmin)
						bmin = b;
					if (b > bmax)
						bmax = b;
					count += entry->count;
				}
			}
		}
	}
	box->count = count;
	if (count != 0) {
		box->rmin = rmin;
		box->rmax = rmax;
		box->gmin = gmin;
		box->gmax = gmax;
		box->bmin = bmin;
		box->bmax = bmax;
	}
}

/* Split a histogram box at its weighted median. */
static int
sixel_box_split(struct sixel_box *box, struct sixel_box *new,
    struct sixel_hgram *hg)
{
	u_int	 levels[SIXEL_HGRAM_LEVELS] = { 0 };
	u_int	 r, g, b, index, channel, first, last, level;
	u_int	 rrange, grange, brange, count = 0;

	/* Split along the channel with the widest occupied range. */
	rrange = box->rmax - box->rmin;
	grange = box->gmax - box->gmin;
	brange = box->bmax - box->bmin;
	if (rrange == 0 && grange == 0 && brange == 0)
		return (0);
	if (grange >= rrange && grange >= brange)
		channel = 1;
	else if (rrange >= brange)
		channel = 0;
	else
		channel = 2;
	for (r = box->rmin; r <= box->rmax; r++) {
		for (g = box->gmin; g <= box->gmax; g++) {
			for (b = box->bmin; b <= box->bmax; b++) {
				index = (r << 10)|(g << 5)|b;
				if (channel == 0)
					levels[r] += hg[index].count;
				else if (channel == 1)
					levels[g] += hg[index].count;
				else
					levels[b] += hg[index].count;
			}
		}
	}
	if (channel == 0) {
		first = box->rmin;
		last = box->rmax;
	} else if (channel == 1) {
		first = box->gmin;
		last = box->gmax;
	} else {
		first = box->bmin;
		last = box->bmax;
	}

	/* Find the weighted median without emptying either resulting box. */
	for (level = first; level < last; level++) {
		count += levels[level];
		if (count >= box->count / 2)
			break;
	}

	/* Keep the maximum occupied level in the new box. */
	if (level == last)
		level--;
	memcpy(new, box, sizeof *new);
	if (channel == 0) {
		box->rmax = level;
		new->rmin = level + 1;
	} else if (channel == 1) {
		box->gmax = level;
		new->gmin = level + 1;
	} else {
		box->bmax = level;
		new->bmin = level + 1;
	}
	sixel_box_update(box, hg);
	sixel_box_update(new, hg);
	return (box->count != 0 && new->count != 0);
}

/* Build an adaptive palette from an RGB histogram. */
static u_int
sixel_make_palette(struct sixel_hgram *hg, struct sixel_rgb *palette)
{
	struct sixel_box	 boxes[SIXEL_PALETTE_SIZE], new;
	struct sixel_box	*box;
	uint64_t		 best_score, score, rsum, gsum, bsum, count;
	u_int			 i, nbox = 1, best, r, g, b, index;
	u_int			 rrange, grange, brange;

	/* Initialize the first box. */
	memset(&boxes[0], 0, sizeof boxes[0]);
	boxes[0].rmax = boxes[0].gmax = boxes[0].bmax = SIXEL_HGRAM_LEVELS - 1;
	sixel_box_update(&boxes[0], hg);
	if (boxes[0].count == 0)
		return (0);

	/* Repeatedly split the most populated and varied colour region. */
	while (nbox < SIXEL_PALETTE_SIZE) {
		best = nbox;
		best_score = 0;
		for (i = 0; i < nbox; i++) {
			box = &boxes[i];
			rrange = box->rmax - box->rmin;
			grange = box->gmax - box->gmin;
			brange = box->bmax - box->bmin;
			score = (uint64_t)box->count * (rrange * rrange +
			    grange * grange + brange * brange);
			if (score > best_score) {
				best = i;
				best_score = score;
			}
		}
		if (best == nbox || !sixel_box_split(&boxes[best], &new, hg))
			break;
		memcpy(&boxes[nbox++], &new, sizeof new);
	}

	/* Average each region's colours into one palette entry. */
	for (i = 0; i < nbox; i++) {
		box = &boxes[i];

		rsum = gsum = bsum = count = 0;
		for (r = box->rmin; r <= box->rmax; r++) {
			for (g = box->gmin; g <= box->gmax; g++) {
				for (b = box->bmin; b <= box->bmax; b++) {
					index = (r << 10)|(g << 5)|b;
					rsum += hg[index].r;
					gsum += hg[index].g;
					bsum += hg[index].b;
					count += hg[index].count;
				}
			}
		}
		palette[i].r = (rsum + count / 2) / count;
		palette[i].g = (gsum + count / 2) / count;
		palette[i].b = (bsum + count / 2) / count;
	}

	return (nbox);
}

/* Find the closest adaptive palette entry for an RGB colour. */
static u_int
sixel_nearest_colour(struct sixel_rgb *palette, u_int ncolours,
    uint16_t *cache, u_int r, u_int g, u_int b)
{
	uint64_t	 distance, best_distance = UINT64_MAX;
	int		 dr, dg, db;
	u_int		 i, best = 0, index;

	index = ((r >> 3) << 10)|((g >> 3) << 5)|(b >> 3);
	if (cache[index] != UINT16_MAX)
		return (cache[index]);
	for (i = 0; i < ncolours; i++) {
		dr = (int)r - palette[i].r;
		dg = (int)g - palette[i].g;
		db = (int)b - palette[i].b;
		distance = 3ULL * dr * dr + 6ULL * dg * dg + db * db;
		if (distance < best_distance) {
			best = i;
			best_distance = distance;
		}
	}
	cache[index] = best;
	return (best);
}

/* Clamp an RGB component to the valid range. */
static u_int
sixel_clamp_colour(int colour)
{
	if (colour < 0)
		return (0);
	if (colour > 255)
		return (255);
	return (colour);
}

/* Return a source pixel mapped to an output SIXEL pixel. */
static const u_char *
sixel_from_image_pixel(struct sixel_source *src, u_int x, u_int y)
{
	u_int	px = src->dst_x + x, py = src->dst_y + y;

	return (image_get_pixel(src->image, px, py, src->dst_w, src->dst_h));
}

/* Make a palette from the visible pixels in the output area. */
static u_int
sixel_make_image_palette(struct sixel_source *src, struct sixel_rgb *palette)
{
	struct sixel_hgram	*hg, *entry;
	const u_char		*pixel;
	u_int			 x, y, index, ncolours;

	hg = xcalloc(SIXEL_HGRAM_SIZE, sizeof *hg);
	for (y = 0; y < src->out_h; y++) {
		for (x = 0; x < src->out_w; x++) {
			pixel = sixel_from_image_pixel(src, x, y);
			if (pixel[3] == 0)
				continue;

			index = ((pixel[0] >> 3) << 10)| ((pixel[1] >> 3) << 5)|
			    (pixel[2] >> 3);
			entry = &hg[index];
			entry->count++;
			entry->r += pixel[0];
			entry->g += pixel[1];
			entry->b += pixel[2];
		}
	}
	ncolours = sixel_make_palette(hg, palette);
	free(hg);
	return (ncolours);
}

/* Choose an indexed colour for a pixel and diffuse it. */
static u_int
sixel_dither_pixel(struct sixel_rgb *palette, u_int ncolours, uint16_t *cache,
    const u_char *pixel, int *current, int *next)
{
	int	 alphaerr, rerr, gerr, berr;
	u_int	 alpha, r, g, b, c;

	/* Dither alpha to SIXEL's binary transparency. */
	alpha = sixel_clamp_colour((int)pixel[3] + current[3] / 16);
	alphaerr = alpha;
	if (alpha >= 128)
		alphaerr -= 255;
	current[7] += alphaerr * 7;
	next[-1] += alphaerr * 3;
	next[3] += alphaerr * 5;
	next[7] += alphaerr;
	if (alpha < 128)
		return (0);

	/* Get the RGB colours. */
	r = sixel_clamp_colour((int)pixel[0] + current[0] / 16);
	g = sixel_clamp_colour((int)pixel[1] + current[1] / 16);
	b = sixel_clamp_colour((int)pixel[2] + current[2] / 16);
	c = sixel_nearest_colour(palette, ncolours, cache, r, g, b);

	/* Find the colour error. */
	rerr = (int)r - palette[c].r;
	gerr = (int)g - palette[c].g;
	berr = (int)b - palette[c].b;

	/* Diffuse error with the 7:3:5:1 kernel. */
	current[4] += rerr * 7;
	current[5] += gerr * 7;
	current[6] += berr * 7;
	next[-4] += rerr * 3;
	next[-3] += gerr * 3;
	next[-2] += berr * 3;
	next[0] += rerr * 5;
	next[1] += gerr * 5;
	next[2] += berr * 5;
	next[4] += rerr;
	next[5] += gerr;
	next[6] += berr;
	return (c + 1);
}

/* Make an indexed SIXEL image from part of an image. */
static struct sixel_image *
sixel_make_image(struct image *im, u_int cx, u_int cy, u_int csx, u_int csy,
    u_int cw, u_int ch)
{
	struct sixel_image	*si;
	struct sixel_rgb	 palette[SIXEL_PALETTE_SIZE];
	struct sixel_source	 src;
	struct image_geom	*ig = image_get_geometry(im);
	const u_char		*pixel;
	uint16_t		*cache;
	int			*current, *next, *tmp;
	u_int			 x, y, sx, sy, error_index;
	u_int			 r, g, b, c, i, nc;
	uint64_t		 dst_w, dst_h, content_width, content_height;
	uint64_t		 left, right, top, bottom;

	/* Work out the requested cell crop in destination pixel coordinates. */
	src.image = im;
	dst_w = (uint64_t)ig->sx * cw;
	dst_h = (uint64_t)ig->sy * ch;
	if (dst_w > UINT_MAX || dst_h > UINT_MAX)
		return (NULL);
	content_width = ceil((ig->canvas_xoff + ig->scaled_width) * dst_w /
	    ig->canvas_width);
	content_height = ceil((ig->canvas_yoff + ig->scaled_height) * dst_h /
	    ig->canvas_height);
	if (content_width > dst_w)
		content_width = dst_w;
	if (content_height > dst_h)
		content_height = dst_h;

	/* Clip the requested cell rectangle to output pixel bounds. */
	left = (uint64_t)cx * cw;
	top = (uint64_t)cy * ch;
	right = ((uint64_t)cx + csx) * cw;
	bottom = ((uint64_t)cy + csy) * ch;
	if (right > content_width)
		right = content_width;
	if (bottom > content_height)
		bottom = content_height;
	if (right <= left || bottom <= top)
		return (NULL);

	/* The clipped output bounds determine the SIXEL image dimensions. */
	sx = right - left;
	sy = bottom - top;
	if (sx == 0 ||
	    sy == 0 ||
	    sx > SIXEL_WIDTH_LIMIT ||
	    sy > SIXEL_HEIGHT_LIMIT) {
		return (NULL);
	}
	if (sx * sy > IMAGE_SIZE_LIMIT / 4)
		return (NULL);

	/* Map output pixels using their position in the full canvas. */
	src.dst_w = dst_w;
	src.dst_h = dst_h;
	src.dst_x = left;
	src.dst_y = top;
	src.out_w = sx;
	src.out_h = sy;

	/* Build an adaptive palette from the visible nontransparent pixels. */
	nc = sixel_make_image_palette(&src, palette);
	if (nc == 0)
		return (NULL);

	/* Create the indexed image with a SIXEL RGB palette. */
	si = xcalloc(1, sizeof *si);
	si->ra_x = sx;
	si->ra_y = sy;
	si->ncolours = si->used_colours = nc;
	si->colours = xcalloc(si->ncolours, sizeof *si->colours);
	for (i = 0; i < si->ncolours; i++) {
		r = (palette[i].r * 100 + 127) / 255;
		g = (palette[i].g * 100 + 127) / 255;
		b = (palette[i].b * 100 + 127) / 255;
		si->colours[i] = (2U << 25)|(r << 16)|(g << 8)|b;
	}

	/* Dither colour and alpha into the indexed image. */
	cache = xmalloc(SIXEL_HGRAM_SIZE * sizeof *cache);
	memset(cache, 0xff, SIXEL_HGRAM_SIZE * sizeof *cache);
	current = xcalloc(((size_t)sx + 2) * 4, sizeof *current);
	next = xcalloc(((size_t)sx + 2) * 4, sizeof *next);
	for (y = 0; y < sy; y++) {
		for (x = 0; x < sx; x++) {
			pixel = sixel_from_image_pixel(&src, x, y);
			error_index = (x + 1) * 4;
			c = sixel_dither_pixel(palette, nc, cache, pixel,
			    current + error_index, next + error_index);
			if (c != 0 && sixel_set_pixel(si, x, y, c) != 0)
				goto fail;
		}

		/* Advance to the next output row's accumulated error. */
		tmp = current;
		current = next;
		next = tmp;
		memset(next, 0, ((size_t)sx + 2) * 4 * sizeof *next);
	}

	free(current);
	free(next);
	free(cache);
	return (si);

fail:
	free(current);
	free(next);
	free(cache);
	sixel_free(si);
	return (NULL);
}

/* Return the number of bytes allocated for an indexed SIXEL image. */
static size_t
sixel_image_size(struct sixel_image *si)
{
	return (sizeof *si + si->y * sizeof *si->lines + si->allocated +
	    si->ncolours * sizeof *si->colours);
}

/* Remove an image from the global SIXEL output cache. */
static void
sixel_remove_cache(struct sixel_item *cache)
{
	RB_REMOVE(sixel_tree, &sixel_tree, cache);
	TAILQ_REMOVE(&sixel_cache, cache, entry);
	sixel_cache_size -= cache->size;
	sixel_free(cache->si);
	free(cache);
}

/* Drop SIXEL cache entries whose source images have gone away. */
static void
sixel_collect_images(void)
{
	struct sixel_item	*cache, *next;

	TAILQ_FOREACH_SAFE(cache, &sixel_cache, entry, next) {
		if (image_find(cache->id) == NULL)
			sixel_remove_cache(cache);
	}
}

/* Get a cached image at the given cell size. */
static struct sixel_image *
sixel_get_image(struct image *im, u_int cw, u_int ch)
{
	struct sixel_image	*si;
	struct image_geom	*ig = image_get_geometry(im);
	size_t			 size;
	u_int			 id = image_get_id(im);
	struct sixel_item	*cache, find = { .id = id, .cw = cw, .ch = ch };

	cache = RB_FIND(sixel_tree, &sixel_tree, &find);
	if (cache != NULL) {
		TAILQ_REMOVE(&sixel_cache, cache, entry);
		TAILQ_INSERT_TAIL(&sixel_cache, cache, entry);
		return (cache->si);
	}

	si = sixel_make_image(im, 0, 0, ig->sx, ig->sy, cw, ch);
	if (si == NULL)
		return (NULL);

	size = (sizeof *cache) + sixel_image_size(si);
	if (size > SIXEL_CACHE_SIZE_LIMIT) {
		sixel_free(si);
		return (NULL);
	}
	if (sixel_cache_size > SIXEL_CACHE_SIZE_LIMIT - size)
		sixel_collect_images();
	while (sixel_cache_size > SIXEL_CACHE_SIZE_LIMIT - size)
		sixel_remove_cache(TAILQ_FIRST(&sixel_cache));

	cache = xcalloc(1, sizeof *cache);
	cache->id = id;
	cache->cw = cw;
	cache->ch = ch;
	cache->size = size;
	cache->si = si;
	RB_INSERT(sixel_tree, &sixel_tree, cache);
	TAILQ_INSERT_TAIL(&sixel_cache, cache, entry);

	sixel_cache_size += size;
	return (si);
}

/* Copy a rectangle from a SIXEL image. */
static struct sixel_image *
sixel_crop(struct sixel_image *si, u_int cx, u_int cy, u_int csx, u_int csy,
    u_int cw, u_int ch)
{
	struct sixel_image	*new;
	struct sixel_line	*src, *dst;
	uint64_t		 left, right, top, bottom;
	u_int			 x, y, w, h, i;

	left = (uint64_t)cx * cw;
	top = (uint64_t)cy * ch;
	right = ((uint64_t)cx + csx) * cw;
	bottom = ((uint64_t)cy + csy) * ch;
	if (right > si->ra_x)
		right = si->ra_x;
	if (bottom > si->ra_y)
		bottom = si->ra_y;
	if (right <= left || bottom <= top)
		return (NULL);
	x = left;
	y = top;
	w = right - left;
	h = bottom - top;

	new = xcalloc(1, sizeof *new);
	new->x = new->ra_x = w;
	new->y = new->ra_y = h;
	new->lines = xcalloc(h, sizeof *new->lines);
	for (i = 0; i < h; i++) {
		if (y + i >= si->y)
			break;
		src = &si->lines[y + i];
		if (x >= src->x)
			continue;
		dst = &new->lines[i];
		dst->x = src->x - x;
		if (dst->x > w)
			dst->x = w;
		dst->data = xmalloc(dst->x * sizeof *dst->data);
		memcpy(dst->data, src->data + x, dst->x * sizeof *dst->data);
		new->allocated += dst->x * sizeof *dst->data;
	}
	new->used_colours = si->used_colours;
	new->ncolours = si->ncolours;
	new->colours = xmalloc(new->ncolours * sizeof *new->colours);
	memcpy(new->colours, si->colours, new->ncolours * sizeof *new->colours);
	return (new);
}

/* Encode part of an image as SIXEL. */
static char *
sixel_from_image(struct image *im, u_int x, u_int y, u_int sx, u_int sy,
    u_int cw, u_int ch, size_t *size)
{
	struct sixel_image	*si, *cached;
	char			*data;

	cached = sixel_get_image(im, cw, ch);
	if (cached != NULL)
		si = sixel_crop(cached, x, y, sx, sy, cw, ch);
	else
		si = sixel_make_image(im, x, y, sx, sy, cw, ch);
	if (si == NULL)
		return (NULL);
	data = sixel_print(si, size);
	sixel_free(si);
	return (data);
}

/* Return whether the terminal supports SIXEL. */
int
sixel_select(struct tty *tty)
{
	if (tty->term != NULL &&
	    (tty->term->flags & TERM_SIXEL) &&
	    tty->xpixel != 0 &&
	    tty->ypixel != 0)
		return (1);
	return (0);
}

/* Get SIXEL backend flags. */
int
sixel_get_flags(struct tty *tty)
{
	int	flags = IMAGE_BACKEND_GRAPHICAL;

	if (tty_term_flag(tty->term, TTYC_ISCRL))
		flags |= IMAGE_BACKEND_SCROLLS;
	return (flags);
}

/* Get pending SIXEL rows for a terminal. */
static struct sixel_pending *
sixel_get_pending(struct tty *tty)
{
	struct sixel_pending	*sp = tty->image_data;

	if (sp == NULL) {
		sp = xcalloc(1, sizeof *sp);
		tty->image_data = sp;
	}
	return (sp);
}

/* Free pending SIXEL rows for a terminal. */
void
sixel_free_pending(struct tty *tty)
{
	struct sixel_pending	*sp = tty->image_data;

	if (sp == NULL)
		return;
	if (sp->image != NULL)
		image_free(sp->image);
	free(sp);
	tty->image_data = NULL;
}

/* Write the rows held in the pending run as a single SIXEL. */
void
sixel_flush_pending(struct tty *tty)
{
	struct sixel_pending	*sp = tty->image_data;
	struct image		*im;
	char			*data;
	size_t			 size;

	if (sp == NULL || sp->image == NULL)
		return;
	im = sp->image;
	sp->image = NULL;

	data = sixel_from_image(im, sp->src_x, sp->src_y, sp->w, sp->h,
	    tty->xpixel, tty->ypixel, &size);
	if (data != NULL) {
		tty_region_off(tty);
		tty_margin_off(tty);
		tty_cursor(tty, sp->dst_x, sp->dst_y);

		tty->flags |= TTY_NOBLOCK;
		tty_putn(tty, data, size, 0);
		tty->cx = tty->cy = UINT_MAX;

		free(data);
	}
	image_free(im);
}

/* Queue adjacent placement rows for one SIXEL output sequence. */
void
sixel_draw_rect(struct tty *tty, struct image_rect *rect)
{
	struct sixel_pending	*sp = sixel_get_pending(tty);

	if (sp->image == rect->image &&
	    sp->src_x == rect->src_x &&
	    sp->w == rect->sx &&
	    sp->dst_x == rect->dst_x &&
	    sp->src_y + sp->h == rect->src_y &&
	    sp->dst_y + sp->h == rect->dst_y) {
		sp->h += rect->sy;
		return;
	}

	image_ref(rect->image);
	sixel_flush_pending(tty);

	sp->image = rect->image;
	sp->src_x = rect->src_x;
	sp->src_y = rect->src_y;
	sp->w = rect->sx;
	sp->h = rect->sy;
	sp->dst_x = rect->dst_x;
	sp->dst_y = rect->dst_y;
}

/* Remove old SIXEL pixels before drawing an image area. */
void
sixel_start_draw(struct tty *tty, u_int x, u_int y, u_int sx, u_int sy)
{
	u_int	yy;

	for (yy = y; yy < y + sy; yy++) {
		tty_cursor(tty, x, yy);
		if (tty_term_has(tty->term, TTYC_ECH))
			tty_putcode_i(tty, TTYC_ECH, sx);
		else
			tty_repeat_space(tty, sx);
	}
}
