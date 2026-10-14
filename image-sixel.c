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

	/* Visible pixel size inside a padded output canvas. */
	u_int			 raster_x;
	u_int			 raster_y;

	/* Terminal cell pixel dimensions used for scaling. */
	u_int			 xpixel;
	u_int			 ypixel;

	/* SIXEL palette. */
	u_int			*colours;
	u_int			 ncolours;
	u_int			 used_colours;

	/* SIXEL raster attributes. */
	u_int			 set_ra;
	u_int			 ra_x;
	u_int			 ra_y;

	/* DCS parameters. */
	u_int			 p1;
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

/* An indexed image cached for one terminal cell size. */
struct sixel_image_cache {
	/* Image and terminal geometry associated with this entry. */
	u_int			 server_id;
	u_int			 cell_w;
	u_int			 cell_h;
	/* Memory and age used for cache eviction. */
	size_t			 size;
	uint64_t		 age;
	/* Cached decoded/scaled image and next entry. */
	struct sixel_image	*si;
	struct sixel_image_cache	*next;
};

/* Cached images and rows waiting to be sent to a terminal. */
struct sixel_output {
	/* Per-terminal cached images and aggregate cache state. */
	struct sixel_image_cache	*images;
	size_t			 size;
	uint64_t		 age;
	/* Rows waiting to be written as one SIXEL. */
	struct sixel_pending	 pending;
};

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
	/* Source pixels and number of bytes between rows. */
	const u_char	*pixels;
	size_t		 stride;

	/* Source image and canvas sizes in pixels and size in cells. */
	u_int		 width;
	u_int		 height;
	u_int		 canvas_width;
	u_int		 canvas_height;
	u_int		 sx;
	u_int		 sy;

	/* Destination canvas size in pixels. */
	u_int		 dst_w;
	u_int		 dst_h;

	/* Output position and size in the destination canvas in pixels. */
	u_int		 dst_x;
	u_int		 dst_y;
	u_int		 out_w;
	u_int		 out_h;
};

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

	si->set_ra = 1;
	si->ra_x = x;
	si->ra_y = y;

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
	if (si->used_colours <= c)
		si->used_colours = c + 1;
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
	im = image_create(si->x, si->y, sx * si->xpixel, sy * si->ypixel, sx,
	    sy, pixels);
	if (im == NULL)
		free(pixels);
	if (im != NULL)
		image_set_sixel(im, si);
	return (im);
}

/* Scale or crop an indexed SIXEL image. */
static struct sixel_image *
sixel_scale(struct sixel_image *si, u_int cell_w, u_int cell_h, u_int ox,
    u_int oy, u_int sx, u_int sy, int colours)
{
	struct sixel_image	*new;
	u_int			 cx, cy, raster_sx, raster_sy;
	u_int			 pox, poy, psx, psy, tsx, tsy, px, py;
	uint64_t		 source_left, source_right;
	uint64_t		 source_top, source_bottom;
	uint64_t		 target_left, target_right;
	uint64_t		 target_top, target_bottom;
	u_int			 x, y, i;

	/* Clip image cells before mapping them to terminal cells. */
	sixel_size_in_cells(si, &cx, &cy);
	if (ox >= cx || oy >= cy)
		return (NULL);
	if (ox + sx >= cx)
		sx = cx - ox;
	if (oy + sy >= cy)
		sy = cy - oy;

	if (cell_w == 0)
		cell_w = si->xpixel;
	if (cell_h == 0)
		cell_h = si->ypixel;
	raster_sx = (si->raster_x != 0 ? si->raster_x : si->x);
	raster_sy = (si->raster_y != 0 ? si->raster_y : si->y);
	if (raster_sx > si->x)
		raster_sx = si->x;
	if (raster_sy > si->y)
		raster_sy = si->y;

	/* Map whole cells at pixel boundaries and clip the final cell. */
	source_left = (uint64_t)ox * si->xpixel;
	source_right = (uint64_t)(ox + sx) * si->xpixel;
	source_top = (uint64_t)oy * si->ypixel;
	source_bottom = (uint64_t)(oy + sy) * si->ypixel;
	if (source_right > raster_sx)
		source_right = raster_sx;
	if (source_bottom > raster_sy)
		source_bottom = raster_sy;
	if (source_right <= source_left || source_bottom <= source_top)
		return (NULL);
	pox = source_left;
	poy = source_top;
	psx = source_right - source_left;
	psy = source_bottom - source_top;

	/* Preserve the pixel extent of a partial final cell. */
	target_right = ((uint64_t)raster_sx * cell_w + si->xpixel - 1) /
	    si->xpixel;
	target_bottom = ((uint64_t)raster_sy * cell_h + si->ypixel - 1) /
	    si->ypixel;
	if (target_right > UINT_MAX || target_bottom > UINT_MAX)
		return (NULL);
	target_left = (uint64_t)ox * cell_w;
	target_top = (uint64_t)oy * cell_h;
	if (target_left >= target_right || target_top >= target_bottom)
		return (NULL);
	if ((uint64_t)(ox + sx) * cell_w < target_right)
		target_right = (uint64_t)(ox + sx) * cell_w;
	if ((uint64_t)(oy + sy) * cell_h < target_bottom)
		target_bottom = (uint64_t)(oy + sy) * cell_h;
	tsx = target_right - target_left;
	tsy = target_bottom - target_top;
	if (tsx == 0 || tsy == 0)
		return (NULL);

	if ((uint64_t)tsx * tsy > IMAGE_SIZE_LIMIT / 4)
		return (NULL);

	/* Resample the crop and preserve its palette and raster attributes. */
	new = xcalloc(1, sizeof *si);
	new->xpixel = cell_w;
	new->ypixel = cell_h;
	new->p1 = si->p1;
	new->p2 = si->p2;

	new->set_ra = si->set_ra;
	/* The raster attributes describe the scaled output rectangle. */
	new->ra_x = tsx;
	new->ra_y = tsy;

	new->used_colours = si->used_colours;
	for (y = 0; y < tsy; y++) {
		py = poy + ((double)y * psy / tsy);
		for (x = 0; x < tsx; x++) {
			px = pox + ((double)x * psx / tsx);
			if (sixel_set_pixel(new, x, y, sixel_get_pixel(si, px, py)) != 0) {
				sixel_free(new);
				return (NULL);
			}
		}
	}
	/* Keep transparent edges in the scaled raster canvas as well. */
	if (sixel_parse_expand_lines(new, tsy) != 0) {
		sixel_free(new);
		return (NULL);
	}
	new->x = tsx;

	if (colours && si->ncolours != 0) {
		new->colours = xmalloc(si->ncolours * sizeof *new->colours);
		for (i = 0; i < si->ncolours; i++)
			new->colours[i] = si->colours[i];
		new->ncolours = si->ncolours;
	}
	return (new);
}

/* Fit an indexed SIXEL image into a terminal cell canvas. */
static struct sixel_image *
sixel_fit(struct sixel_image *si, u_int cell_w, u_int cell_h, u_int cells_x,
    u_int cells_y)
{
	struct sixel_image	*new;
	uint64_t		 canvas_width, canvas_height;
	u_int			 width, height, x, y, px, py, i;

	if (si->xpixel == 0)
		si->xpixel = 8;
	if (si->ypixel == 0)
		si->ypixel = 16;
	if (cell_w == 0)
		cell_w = si->xpixel;
	if (cell_h == 0)
		cell_h = si->ypixel;

	canvas_width = (uint64_t)cells_x * cell_w;
	canvas_height = (uint64_t)cells_y * cell_h;
	if (si->x == 0 || si->y == 0 || canvas_width == 0 ||
	    canvas_height == 0 || canvas_width > SIXEL_WIDTH_LIMIT ||
	    canvas_height > SIXEL_HEIGHT_LIMIT) {
		return (NULL);
	}

	/* Keep the same cell footprint as the RGBA output path. */
	width = ((uint64_t)si->x * cell_w + si->xpixel - 1) / si->xpixel;
	height = ((uint64_t)si->y * cell_h + si->ypixel - 1) / si->ypixel;
	if (width == 0)
		width = 1;
	if (height == 0)
		height = 1;
	if (width > canvas_width)
		width = canvas_width;
	if (height > canvas_height)
		height = canvas_height;

	if (canvas_width * canvas_height > IMAGE_SIZE_LIMIT / 4)
		return (NULL);

	/* Resample the raster and pad the unused cell canvas. */
	new = xcalloc(1, sizeof *new);
	new->xpixel = cell_w;
	new->ypixel = cell_h;
	new->p1 = si->p1;
	new->p2 = si->p2;
	new->set_ra = 1;
	new->ra_x = width;
	new->ra_y = height;
	new->raster_x = width;
	new->raster_y = height;
	new->used_colours = si->used_colours;

	for (y = 0; y < height; y++) {
		py = (uint64_t)y * si->y / height;
		for (x = 0; x < width; x++) {
			px = (uint64_t)x * si->x / width;
			if (sixel_set_pixel(new, x, y, sixel_get_pixel(si, px, py)) != 0) {
				sixel_free(new);
				return (NULL);
			}
		}
	}
	/* Keep the unused part of the cell canvas as blank padding. */
	if (sixel_parse_expand_lines(new, canvas_height) != 0) {
		sixel_free(new);
		return (NULL);
	}
	new->x = canvas_width;

	if (si->ncolours != 0) {
		new->colours = xmalloc(si->ncolours * sizeof *new->colours);
		for (i = 0; i < si->ncolours; i++)
			new->colours[i] = si->colours[i];
		new->ncolours = si->ncolours;
	}
	return (new);
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

	tmplen = xsnprintf(tmp, sizeof tmp, "\033P%u;%uq", si->p1, si->p2);
	sixel_print_add(&buf, &len, &used, tmp, tmplen);

	if (si->set_ra) {
		tmplen = xsnprintf(tmp, sizeof tmp, "\"1;1;%u;%u", si->ra_x,
		    si->ra_y);
		sixel_print_add(&buf, &len, &used, tmp, tmplen);
	}

	/* Track colour planes containing pixels in the current six-row band. */
	planes = xcalloc(used_colours, sizeof *planes);
	active = xcalloc(used_colours, sizeof *active);

	/* Emit the palette before the encoded colour planes. */
	for (i = 0; i < used_colours; i++) {
		c = 0;
		if (i < si->ncolours)
			c = si->colours[i];
		if (c >> 25 == 0)
			c = 2U << 25;
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
sixel_from_image_pixel(const struct sixel_source *src, u_int x, u_int y)
{
	u_int	 px, py;

	px = ((uint64_t)src->dst_x + x) * src->canvas_width / src->dst_w;
	py = ((uint64_t)src->dst_y + y) * src->canvas_height / src->dst_h;
	return (src->pixels + py * src->stride + px * 4);
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
	const u_char		*pixel;
	uint16_t		*cache;
	int			*current, *next, *tmp;
	u_int			 x, y, sx, sy, error_index;
	u_int			 r, g, b, c, i, nc;
	uint64_t		 dst_w, dst_h, content_width, content_height;
	uint64_t		 left, right, top, bottom;

	/* Work out the requested cell crop in destination pixel coordinates. */
	src.pixels = image_get_pixels(im, &src.stride, NULL);
	image_get_size(im, &src.width, &src.height);
	image_get_canvas_size(im, &src.canvas_width, &src.canvas_height);
	image_get_size_in_cells(im, &src.sx, &src.sy);
	dst_w = (uint64_t)src.sx * cw;
	dst_h = (uint64_t)src.sy * ch;
	if (dst_w > UINT_MAX || dst_h > UINT_MAX)
		return (NULL);
	content_width = ((uint64_t)src.width * dst_w +
	    src.canvas_width - 1) / src.canvas_width;
	content_height = ((uint64_t)src.height * dst_h +
	    src.canvas_height - 1) / src.canvas_height;

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
	si->xpixel = cw;
	si->ypixel = ch;
	si->p1 = 9;
	si->p2 = 1;
	si->set_ra = 1;
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

/* Return the SIXEL output cache for a terminal. */
static struct sixel_output *
sixel_get_output(struct tty *tty)
{
	struct sixel_output	*so = tty->image_data;

	if (so == NULL) {
		so = xcalloc(1, sizeof *so);
		tty->image_data = so;
	}
	return (so);
}

/* Return the memory used by an indexed SIXEL image. */
static size_t
sixel_image_size(struct sixel_image *si)
{
	uint64_t	 size;

	if ((uint64_t)si->x * si->y > SIZE_MAX / sizeof(uint16_t))
		return (0);
	size = (uint64_t)si->x * si->y * sizeof(uint16_t);
	if ((uint64_t)si->ncolours * sizeof *si->colours > SIZE_MAX - size)
		return (0);
	size += (uint64_t)si->ncolours * sizeof *si->colours;
	if (size > SIZE_MAX)
		return (0);
	return (size);
}

/* Remove an image from the SIXEL output cache. */
static void
sixel_remove_cache(struct sixel_output *so, struct sixel_image_cache **pp)
{
	struct sixel_image_cache	*cache = *pp;

	*pp = cache->next;
	so->size -= cache->size;
	sixel_free(cache->si);
	free(cache);
}

/* Drop SIXEL cache entries whose source images have gone away. */
static void
sixel_collect_images(struct sixel_output *so)
{
	struct sixel_image_cache	**pp, *cache;

	for (pp = &so->images; (cache = *pp) != NULL; ) {
		if (image_find(cache->server_id) == NULL)
			sixel_remove_cache(so, pp);
		else
			pp = &cache->next;
	}
}

/* Free SIXEL output state for a terminal. */
void
sixel_free_output(struct tty *tty, __unused int send)
{
	struct sixel_output		*so = tty->image_data;
	struct sixel_image_cache	*cache, *next;

	if (so == NULL)
		return;
	/* Discard pending rows measured at the old terminal geometry. */
	so->pending.image = NULL;
	for (cache = so->images; cache != NULL; cache = next) {
		next = cache->next;
		sixel_free(cache->si);
		free(cache);
	}
	if (so->pending.image != NULL)
		image_free(so->pending.image);
	free(so);
	tty->image_data = NULL;
}

/* Render an image at a terminal's current pixel geometry. */
static struct sixel_image *
sixel_render_image(struct image *im, u_int cell_w, u_int cell_h)
{
	struct sixel_image	*original, *si;
	u_int			 sx, sy;

	image_get_size_in_cells(im, &sx, &sy);
	/* Preserve the original SIXEL palette and pixels when possible. */
	original = image_get_sixel(im);
	if (original != NULL)
		si = sixel_fit(original, cell_w, cell_h, sx, sy);
	else
		si = sixel_make_image(im, 0, 0, sx, sy, cell_w, cell_h);
	return (si);
}

/* Return a rendered image from the SIXEL output cache. */
static struct sixel_image *
sixel_get_image(struct tty *tty, struct image *im)
{
	struct sixel_output		*so = sixel_get_output(tty);
	struct sixel_image_cache	**pp, *cache, **oldest;
	struct sixel_image		*si;
	size_t				 size;

	sixel_collect_images(so);
	for (cache = so->images; cache != NULL; cache = cache->next) {
		if (cache->server_id != image_get_id(im) ||
		    cache->cell_w != tty->xpixel ||
		    cache->cell_h != tty->ypixel) {
			continue;
		}
		cache->age = ++so->age;
		return (cache->si);
	}

	si = sixel_render_image(im, tty->xpixel, tty->ypixel);
	if (si == NULL)
		return (NULL);
	size = sixel_image_size(si);
	if (size == 0 || size > IMAGE_SIZE_LIMIT) {
		/* Use the rendered image without caching it. */
		return (si);
	}
	while (so->size > IMAGE_SIZE_LIMIT - size) {
		oldest = NULL;
		for (pp = &so->images; (cache = *pp) != NULL;
		    pp = &cache->next) {
			if (oldest == NULL || cache->age < (*oldest)->age)
				oldest = pp;
		}
		if (oldest == NULL)
			break;
		sixel_remove_cache(so, oldest);
	}
	cache = xcalloc(1, sizeof *cache);
	cache->server_id = image_get_id(im);
	cache->cell_w = tty->xpixel;
	cache->cell_h = tty->ypixel;
	cache->size = size;
	cache->age = ++so->age;
	cache->si = si;
	cache->next = so->images;
	so->images = cache;
	so->size += size;
	return (si);
}

/* Return if a SIXEL image is held by the output cache. */
static int
sixel_image_is_cached(struct tty *tty, struct sixel_image *si)
{
	struct sixel_output		*so = tty->image_data;
	struct sixel_image_cache	*cache;

	if (so == NULL)
		return (0);
	for (cache = so->images; cache != NULL; cache = cache->next) {
		if (cache->si == si)
			return (1);
	}
	return (0);
}

/* Write the rows held in the pending run as a single SIXEL. */
void
sixel_flush_output(struct tty *tty)
{
	struct sixel_output	*so = tty->image_data;
	struct sixel_pending	*sp;
	struct sixel_image	*si, *crop;
	struct image		*im;
	char			*data;
	size_t			 size;

	if (so == NULL || so->pending.image == NULL)
		return;
	sp = &so->pending;

	im = sp->image;
	si = sixel_get_image(tty, im);
	sp->image = NULL;
	image_free(im);
	if (si == NULL)
		return;
	crop = sixel_scale(si, tty->xpixel, tty->ypixel, sp->src_x,
	    sp->src_y, sp->w, sp->h, 1);
	if (!sixel_image_is_cached(tty, si))
		sixel_free(si);
	if (crop == NULL)
		return;
	data = sixel_print(crop, &size);
	sixel_free(crop);
	if (data == NULL)
		return;
	tty_region_off(tty);
	tty_margin_off(tty);
	tty_cursor(tty, sp->dst_x, sp->dst_y);
	tty->flags |= TTY_NOBLOCK;
	tty_putn(tty, data, size, 0);
	/* SIXEL moves the cursor, but does not change terminal attributes. */
	tty->cx = tty->cy = UINT_MAX;
	free(data);
}

/* Queue adjacent placement rows for one SIXEL output sequence. */
void
sixel_draw_rect(struct tty *tty, const struct image_rect *rectangle)
{
	struct sixel_output	*so = sixel_get_output(tty);
	struct sixel_pending	*sp = &so->pending;
	struct image		*im = image_rect_get_image(rectangle);
	u_int			 source_x, source_y, width, height;
	u_int			 destination_x, destination_y;

	image_rect_get_coords(rectangle, &source_x, &source_y, &width,
	    &height, &destination_x, &destination_y);

	if (sp->image == im &&
	    sp->src_x == source_x &&
	    sp->w == width &&
	    sp->dst_x == destination_x &&
	    sp->src_y + sp->h == source_y &&
	    sp->dst_y + sp->h == destination_y) {
		sp->h += height;
		return;
	}

	/* Flush a completed run before queuing a different rectangle. */
	image_ref(im);
	sixel_flush_output(tty);
	sp->image = im;
	sp->src_x = source_x;
	sp->src_y = source_y;
	sp->w = width;
	sp->h = height;
	sp->dst_x = destination_x;
	sp->dst_y = destination_y;
}

/* Remove old SIXEL pixels before replaying a dirty image area. */
void
sixel_redraw_start(struct tty *tty, u_int x, u_int y, u_int sx, u_int sy)
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

/* Write a SIXEL image and release the parsed data. */
void
sixel_write(struct screen_write_ctx *ctx, struct sixel_image *si, u_int bg)
{
	struct image *im;

	im = sixel_to_image(si);
	if (im == NULL) {
		sixel_free(si);
		return;
	}
	image_write_sixel(ctx, im, bg);
	image_free(im);
}
