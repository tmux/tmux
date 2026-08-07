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
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

#define SIXEL_WIDTH_LIMIT 10000
#define SIXEL_HEIGHT_LIMIT 10000
#define SIXEL_PALETTE_SIZE 256
#define SIXEL_HISTOGRAM_LEVELS 32
#define SIXEL_HISTOGRAM_SIZE (SIXEL_HISTOGRAM_LEVELS * \
	SIXEL_HISTOGRAM_LEVELS * SIXEL_HISTOGRAM_LEVELS)

struct sixel_line {
	u_int		 x;
	uint16_t	*data;
};

struct sixel_image {
	u_int			 x;
	u_int			 y;
	u_int			 xpixel;
	u_int			 ypixel;

	u_int			 set_ra;
	u_int			 ra_x;
	u_int			 ra_y;

	u_int			*colours;
	u_int			 ncolours;
	u_int			 used_colours;
	u_int			 p2;

	u_int			 dx;
	u_int			 dy;
	u_int			 dc;

	struct sixel_line	*lines;
};

struct sixel_chunk {
	u_int	 next_x;
	u_int	 next_y;

	u_int	 count;
	char	 pattern;
	char	 next_pattern;

	size_t	 len;
	size_t	 used;
	char	*data;
};

struct sixel_image_cache {
	u_int			 server_id;
	u_int			 xpixel;
	u_int			 ypixel;
	size_t			 size;
	uint64_t		 age;
	struct sixel_image	*si;
	struct sixel_image_cache	*next;
};

struct sixel_output {
	struct sixel_image_cache	*images;
	size_t			 size;
	uint64_t		 age;
};

struct sixel_histogram {
	u_int		 count;
	uint64_t	 red;
	uint64_t	 green;
	uint64_t	 blue;
};

struct sixel_box {
	u_int		 red_min;
	u_int		 red_max;
	u_int		 green_min;
	u_int		 green_max;
	u_int		 blue_min;
	u_int		 blue_max;
	u_int		 count;
};

struct sixel_rgb {
	u_char		 red;
	u_char		 green;
	u_char		 blue;
};

struct sixel_source {
	const u_char	*pixels;
	size_t		 stride;
	u_int		 width;
	u_int		 height;
	u_int		 canvas_width;
	u_int		 canvas_height;
	u_int		 sx;
	u_int		 sy;
};

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

	x = strtoul(endptr + 1, &endptr, 10);
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	if (x > SIXEL_WIDTH_LIMIT) {
		log_debug("%s: image is too wide", __func__);
		return (NULL);
	}
	y = strtoul(endptr + 1, &endptr, 10);
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

	si->set_ra = 1;
	si->ra_x = x;
	si->ra_y = y;

	return (last);
}

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

	c = strtoul(cp, &endptr, 10);
	if (c > SIXEL_COLOUR_REGISTERS) {
		log_debug("%s: too many colours", __func__);
		return (NULL);
	}
	if (si->used_colours <= c)
		si->used_colours = c + 1;
	si->dc = c + 1;
	if (endptr == last || *endptr != ';')
		return (last);

	type = strtoul(endptr + 1, &endptr, 10);
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	c1 = strtoul(endptr + 1, &endptr, 10);
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	c2 = strtoul(endptr + 1, &endptr, 10);
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	c3 = strtoul(endptr + 1, &endptr, 10);
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

	si = xcalloc (1, sizeof *si);
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

void
sixel_size_in_cells(struct sixel_image *si, u_int *x, u_int *y)
{
	if (si->xpixel == 0)
		si->xpixel = 8;
	if (si->ypixel == 0)
		si->ypixel = 16;
	image_size_in_cells(si->x, si->y, si->xpixel, si->ypixel, x, y);
}

#ifdef ENABLE_IMAGES
static double
sixel_hue(double p, double q, double t)
{
	if (t < 0)
		t += 1;
	if (t > 1)
		t -= 1;
	if (t < 1.0 / 6)
		return (p + (q - p) * 6 * t);
	if (t < 1.0 / 2)
		return (q);
	if (t < 2.0 / 3)
		return (p + (q - p) * (2.0 / 3 - t) * 6);
	return (p);
}

static void
sixel_colour_to_rgb(u_int colour, u_char *r, u_char *g, u_char *b)
{
	u_int	type = colour >> 25;
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

	h = ((colour >> 16) & 0x1ff) / 360.0;
	l = ((colour >> 8) & 0xff) / 100.0;
	s = (colour & 0xff) / 100.0;
	if (s == 0) {
		*r = *g = *b = l * 255 + 0.5;
		return;
	}
	q = l < 0.5 ? l * (1 + s) : l + s - l * s;
	p = 2 * l - q;
	/* SIXEL HLS has blue at 0, red at 120 and green at 240 degrees. */
	*r = sixel_hue(p, q, h) * 255 + 0.5;
	*g = sixel_hue(p, q, h - 1.0 / 3) * 255 + 0.5;
	*b = sixel_hue(p, q, h + 1.0 / 3) * 255 + 0.5;
}

/* Convert decoded SIXEL data into the immutable image. */
struct image *
sixel_to_image(struct sixel_image *si)
{
	u_char	*pixels, *pixel, r, g, b;
	u_int	 x, y, c, sx, sy;
	struct image	*im;

	if ((uint64_t)si->x * si->y * 4 > SIZE_MAX)
		return (NULL);
	pixels = xcalloc(si->x * si->y, 4);
	for (y = 0; y < si->y; y++) {
		for (x = 0; x < si->x; x++) {
			c = sixel_get_pixel(si, x, y);
			pixel = pixels + ((size_t)y * si->x + x) * 4;
			if (c == 0) {
				pixel[3] = si->p2 == 1 ? 0 : 255;
				continue;
			}
			c--;
			if (c < si->ncolours)
				sixel_colour_to_rgb(si->colours[c], &r, &g, &b);
			else
				r = g = b = 0;
			pixel[0] = r;
			pixel[1] = g;
			pixel[2] = b;
			pixel[3] = 255;
		}
	}
	sixel_size_in_cells(si, &sx, &sy);
	if ((uint64_t)sx * si->xpixel > UINT_MAX ||
	    (uint64_t)sy * si->ypixel > UINT_MAX) {
		free(pixels);
		return (NULL);
	}
	im = image_create(si->x, si->y, sx * si->xpixel, sy * si->ypixel,
	    sx, sy, pixels);
	if (im == NULL)
		free(pixels);
	else
		image_set_sixel(im, si);
	return (im);
}
#endif

struct sixel_image *
sixel_scale(struct sixel_image *si, u_int xpixel, u_int ypixel, u_int ox,
    u_int oy, u_int sx, u_int sy, int colours)
{
	struct sixel_image	*new;
	u_int			 cx, cy, pox, poy, psx, psy, tsx, tsy, px, py;
	u_int			 x, y, i;

	/*
	 * We want to get the section of the image at ox,oy in image cells and
	 * map it onto the same size in terminal cells.
	 */

	sixel_size_in_cells(si, &cx, &cy);
	if (ox >= cx)
		return (NULL);
	if (oy >= cy)
		return (NULL);
	if (ox + sx >= cx)
		sx = cx - ox;
	if (oy + sy >= cy)
		sy = cy - oy;

	if (xpixel == 0)
		xpixel = si->xpixel;
	if (ypixel == 0)
		ypixel = si->ypixel;

	pox = ox * si->xpixel;
	poy = oy * si->ypixel;
	psx = sx * si->xpixel;
	psy = sy * si->ypixel;

	tsx = sx * xpixel;
	tsy = sy * ypixel;

	new = xcalloc (1, sizeof *si);
	new->xpixel = xpixel;
	new->ypixel = ypixel;
	new->p2 = si->p2;

	new->set_ra = si->set_ra;
	/* subtract offset */
	new->ra_x = si->ra_x > pox ? si->ra_x - pox : 0;
	new->ra_y = si->ra_y > poy ? si->ra_y - poy : 0;
	/* clamp to size */
	new->ra_x = new->ra_x < psx ? new->ra_x : psx;
	new->ra_y = new->ra_y < psy ? new->ra_y : psy;
	/* resize */
	new->ra_x = new->ra_x * xpixel / si->xpixel;
	new->ra_y = new->ra_y * ypixel / si->ypixel;

	new->used_colours = si->used_colours;
	for (y = 0; y < tsy; y++) {
		py = poy + ((double)y * psy / tsy);
		for (x = 0; x < tsx; x++) {
			px = pox + ((double)x * psx / tsx);
			sixel_set_pixel(new, x, y, sixel_get_pixel(si, px, py));
		}
	}

	if (colours && si->ncolours != 0) {
		new->colours = xmalloc(si->ncolours * sizeof *new->colours);
		for (i = 0; i < si->ncolours; i++)
			new->colours[i] = si->colours[i];
		new->ncolours = si->ncolours;
	}
	return (new);
}

static void
sixel_print_add(char **buf, size_t *len, size_t *used, const char *s,
    size_t slen)
{
	while (*used + slen >= *len + 1) {
		*buf = xreallocarray(*buf, 2, *len);
		(*len) *= 2;
	}
	memcpy(*buf + *used, s, slen);
	(*used) += slen;
}

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

static void
sixel_print_compress_colors(struct sixel_image *si, struct sixel_chunk *chunks,
    u_int y, u_int *active, u_int *nactive)
{
	u_int			 i, x, c, dx, colors[6];
	struct sixel_chunk	*chunk = NULL;
	struct sixel_line	*sl;

	for (x = 0; x < si->x; x++) {
		for (i = 0; i < 6; i++) {
			colors[i] = 0;
			if (y + i < si->y) {
				sl = &si->lines[y + i];
				if (x < sl->x && sl->data[x] != 0) {
					colors[i] = sl->data[x];
					c = sl->data[x] - 1;
					chunks[c].next_pattern |= 1 << i;
				}
			}
		}

		for (i = 0; i < 6; i++) {
			if (colors[i] == 0)
				continue;

			c = colors[i] - 1;
			chunk = &chunks[c];
			if (chunk->next_x == x + 1)
				continue;

			if (chunk->next_y < y + 1) {
				chunk->next_y = y + 1;
				active[(*nactive)++] = c;
			}

			dx = x - chunk->next_x;
			if (chunk->pattern != chunk->next_pattern || dx != 0) {
				sixel_print_repeat(&chunk->data, &chunk->len,
				    &chunk->used, chunk->count,
				    chunk->pattern + 0x3f);
				sixel_print_repeat(&chunk->data, &chunk->len,
				    &chunk->used, dx, '?');
				chunk->pattern = chunk->next_pattern;
				chunk->count = 0;
			}
			chunk->count++;
			chunk->next_pattern = 0;
			chunk->next_x = x + 1;
		}
	}
}

char *
sixel_print(struct sixel_image *si, struct sixel_image *map, size_t *size)
{
	char			*buf, tmp[64];
	size_t			 len, used = 0, tmplen;
	u_int			*colours, ncolours, used_colours, i, c, y;
	u_int			*active, nactive;
	struct sixel_chunk	*chunks, *chunk;

	if (map != NULL) {
		colours = map->colours;
		ncolours = map->ncolours;
	} else {
		colours = si->colours;
		ncolours = si->ncolours;
	}

	used_colours = si->used_colours;
	if (used_colours == 0)
		return (NULL);

	len = 8192;
	buf = xmalloc(len);

	tmplen = xsnprintf(tmp, sizeof tmp, "\033P9;%uq", si->p2);
	sixel_print_add(&buf, &len, &used, tmp, tmplen);

	if (si->set_ra) {
		tmplen = xsnprintf(tmp, sizeof tmp, "\"1;1;%u;%u", si->ra_x,
		    si->ra_y);
		sixel_print_add(&buf, &len, &used, tmp, tmplen);
	}

	chunks = xcalloc(used_colours, sizeof *chunks);
	active = xcalloc(used_colours, sizeof *active);

	for (i = 0; i < ncolours; i++) {
		c = colours[i];
		tmplen = xsnprintf(tmp, sizeof tmp, "#%u;%u;%u;%u;%u",
		    i, c >> 25, (c >> 16) & 0x1ff, (c >> 8) & 0xff, c & 0xff);
		sixel_print_add(&buf, &len, &used, tmp, tmplen);
	}

	for (i = 0; i < used_colours; i++) {
		chunk = &chunks[i];
		chunk->len = 8;
		chunk->data = xmalloc(chunk->len);
	}

	for (y = 0; y < si->y; y += 6) {
		nactive = 0;
		sixel_print_compress_colors(si, chunks, y, active, &nactive);

		for (i = 0; i < nactive; i++) {
			c = active[i];
			chunk = &chunks[c];
			tmplen = xsnprintf(tmp, sizeof tmp, "#%u", c);
			sixel_print_add(&buf, &len, &used, tmp, tmplen);
			sixel_print_add(&buf, &len, &used, chunk->data,
			    chunk->used);
			sixel_print_repeat(&buf, &len, &used, chunk->count,
			    chunk->pattern + 0x3f);
			sixel_print_add(&buf, &len, &used, "$", 1);
			chunk->used = chunk->next_x = chunk->count = 0;
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
		free(chunks[i].data);
	free(active);
	free(chunks);

	return (buf);
}

/* Split a 5-bit RGB histogram into an adaptive palette using median cut. */
static void
sixel_box_update(struct sixel_box *box, struct sixel_histogram *histogram)
{
	struct sixel_histogram	*entry;
	u_int			 red, green, blue, index;
	u_int			 red_min = SIXEL_HISTOGRAM_LEVELS;
	u_int			 green_min = SIXEL_HISTOGRAM_LEVELS;
	u_int			 blue_min = SIXEL_HISTOGRAM_LEVELS;
	u_int			 red_max = 0, green_max = 0, blue_max = 0;
	u_int			 count = 0;

	for (red = box->red_min; red <= box->red_max; red++) {
		for (green = box->green_min; green <= box->green_max; green++) {
			for (blue = box->blue_min; blue <= box->blue_max; blue++) {
				index = (red << 10)|(green << 5)|blue;
				entry = &histogram[index];
				if (entry->count == 0)
					continue;
				if (red < red_min)
					red_min = red;
				if (red > red_max)
					red_max = red;
				if (green < green_min)
					green_min = green;
				if (green > green_max)
					green_max = green;
				if (blue < blue_min)
					blue_min = blue;
				if (blue > blue_max)
					blue_max = blue;
				count += entry->count;
			}
		}
	}
	box->count = count;
	if (count == 0)
		return;
	box->red_min = red_min;
	box->red_max = red_max;
	box->green_min = green_min;
	box->green_max = green_max;
	box->blue_min = blue_min;
	box->blue_max = blue_max;
}

static int
sixel_box_split(struct sixel_box *box, struct sixel_box *new,
    struct sixel_histogram *histogram)
{
	u_int	 levels[SIXEL_HISTOGRAM_LEVELS] = { 0 };
	u_int	 red, green, blue, index, channel, first, last, level;
	u_int	 red_range, green_range, blue_range, count = 0;

	red_range = box->red_max - box->red_min;
	green_range = box->green_max - box->green_min;
	blue_range = box->blue_max - box->blue_min;
	if (red_range == 0 && green_range == 0 && blue_range == 0)
		return (0);
	if (green_range >= red_range && green_range >= blue_range)
		channel = 1;
	else if (red_range >= blue_range)
		channel = 0;
	else
		channel = 2;

	for (red = box->red_min; red <= box->red_max; red++) {
		for (green = box->green_min; green <= box->green_max; green++) {
			for (blue = box->blue_min; blue <= box->blue_max; blue++) {
				index = (red << 10)|(green << 5)|blue;
				if (channel == 0)
					levels[red] += histogram[index].count;
				else if (channel == 1)
					levels[green] += histogram[index].count;
				else
					levels[blue] += histogram[index].count;
			}
		}
	}
	if (channel == 0) {
		first = box->red_min;
		last = box->red_max;
	} else if (channel == 1) {
		first = box->green_min;
		last = box->green_max;
	} else {
		first = box->blue_min;
		last = box->blue_max;
	}
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
		box->red_max = level;
		new->red_min = level + 1;
	} else if (channel == 1) {
		box->green_max = level;
		new->green_min = level + 1;
	} else {
		box->blue_max = level;
		new->blue_min = level + 1;
	}
	sixel_box_update(box, histogram);
	sixel_box_update(new, histogram);
	return (box->count != 0 && new->count != 0);
}

static u_int
sixel_make_palette(struct sixel_histogram *histogram,
    struct sixel_rgb *palette)
{
	struct sixel_box	 boxes[SIXEL_PALETTE_SIZE], new;
	struct sixel_box	*box;
	uint64_t	 best_score, score, red, green, blue, count;
	u_int		 i, nboxes = 1, best, r, g, b, index;
	u_int		 red_range, green_range, blue_range;

	memset(&boxes[0], 0, sizeof boxes[0]);
	boxes[0].red_max = boxes[0].green_max = boxes[0].blue_max =
	    SIXEL_HISTOGRAM_LEVELS - 1;
	sixel_box_update(&boxes[0], histogram);
	if (boxes[0].count == 0)
		return (0);

	while (nboxes < SIXEL_PALETTE_SIZE) {
		best = nboxes;
		best_score = 0;
		for (i = 0; i < nboxes; i++) {
			box = &boxes[i];
			red_range = box->red_max - box->red_min;
			green_range = box->green_max - box->green_min;
			blue_range = box->blue_max - box->blue_min;
			score = (uint64_t)box->count *
			    (red_range * red_range + green_range * green_range +
			    blue_range * blue_range);
			if (score > best_score) {
				best = i;
				best_score = score;
			}
		}
		if (best == nboxes ||
		    !sixel_box_split(&boxes[best], &new, histogram))
			break;
		memcpy(&boxes[nboxes++], &new, sizeof new);
	}

	for (i = 0; i < nboxes; i++) {
		box = &boxes[i];
		red = green = blue = count = 0;
		for (r = box->red_min; r <= box->red_max; r++) {
			for (g = box->green_min; g <= box->green_max; g++) {
				for (b = box->blue_min; b <= box->blue_max; b++) {
					index = (r << 10)|(g << 5)|b;
					red += histogram[index].red;
					green += histogram[index].green;
					blue += histogram[index].blue;
					count += histogram[index].count;
				}
			}
		}
		palette[i].red = (red + count / 2) / count;
		palette[i].green = (green + count / 2) / count;
		palette[i].blue = (blue + count / 2) / count;
	}
	return (nboxes);
}

static u_int
sixel_nearest_colour(struct sixel_rgb *palette, u_int ncolours,
    uint16_t *cache, u_int red, u_int green, u_int blue)
{
	uint64_t	 distance, best_distance = UINT64_MAX;
	int		 dr, dg, db;
	u_int		 i, best = 0, index;

	index = ((red >> 3) << 10)|((green >> 3) << 5)|(blue >> 3);
	if (cache[index] != UINT16_MAX)
		return (cache[index]);
	for (i = 0; i < ncolours; i++) {
		dr = (int)red - palette[i].red;
		dg = (int)green - palette[i].green;
		db = (int)blue - palette[i].blue;
		distance = 3ULL * dr * dr + 6ULL * dg * dg + db * db;
		if (distance < best_distance) {
			best = i;
			best_distance = distance;
		}
	}
	cache[index] = best;
	return (best);
}

static u_int
sixel_clamp_colour(int colour)
{
	if (colour < 0)
		return (0);
	if (colour > 255)
		return (255);
	return (colour);
}

static const u_char *
sixel_from_image_pixel(const struct sixel_source *source, u_int sourcex0,
    u_int sourcey0,
    u_int sourcewidth, u_int sourceheight, u_int sx, u_int sy, u_int x,
    u_int y)
{
	u_int	 sourcex, sourcey;

	sourcex = sourcex0 + (uint64_t)x * sourcewidth / sx;
	sourcey = sourcey0 + (uint64_t)y * sourceheight / sy;
	if (sourcex >= source->width)
		sourcex = source->width - 1;
	if (sourcey >= source->height)
		sourcey = source->height - 1;
	return (source->pixels + sourcey * source->stride + sourcex * 4);
}

static struct sixel_image *
sixel_from_image(struct image *im, u_int ox, u_int oy, u_int cells_x,
    u_int cells_y, u_int xpixel, u_int ypixel)
{
	struct sixel_image		*si;
	struct sixel_source		 source;
	struct sixel_histogram		*histogram, *entry;
	struct sixel_rgb		 palette[SIXEL_PALETTE_SIZE];
	const u_char			*pixel;
	uint16_t			*cache;
	int				*current, *next, *tmp;
	int				 red_error, green_error, blue_error, alpha_error;
	u_int				 x, y, sx, sy, index, error_index;
	u_int			 sourcex0, sourcey0, sourcewidth, sourceheight;
	u_int			 red, green, blue, alpha, colour, i, ncolours;
	uint64_t		 destination_width, destination_height;
	uint64_t		 content_width, content_height, x0, x1, y0, y1;

	source.pixels = image_get_pixels(im, &source.stride, NULL);
	image_get_dimensions(im, &source.width, &source.height);
	image_get_canvas_dimensions(im, &source.canvas_width,
	    &source.canvas_height);
	image_get_cell_dimensions(im, &source.sx, &source.sy);
	destination_width = (uint64_t)source.sx * xpixel;
	destination_height = (uint64_t)source.sy * ypixel;
	if (destination_width > UINT_MAX || destination_height > UINT_MAX)
		return (NULL);
	content_width = ((uint64_t)source.width * destination_width +
	    source.canvas_width - 1) / source.canvas_width;
	content_height = ((uint64_t)source.height * destination_height +
	    source.canvas_height - 1) / source.canvas_height;
	x0 = (uint64_t)ox * xpixel;
	y0 = (uint64_t)oy * ypixel;
	x1 = ((uint64_t)ox + cells_x) * xpixel;
	y1 = ((uint64_t)oy + cells_y) * ypixel;
	if (x1 > content_width)
		x1 = content_width;
	if (y1 > content_height)
		y1 = content_height;
	if (x1 <= x0 || y1 <= y0)
		return (NULL);
	sx = x1 - x0;
	sy = y1 - y0;
	if (sx == 0 || sy == 0 || sx > SIXEL_WIDTH_LIMIT ||
	    sy > SIXEL_HEIGHT_LIMIT)
		return (NULL);
	image_get_pixel_rectangle(im, ox, oy, cells_x, cells_y, &sourcex0,
	    &sourcey0, &sourcewidth, &sourceheight);
	if (sourcewidth == 0 || sourceheight == 0)
		return (NULL);

	histogram = xcalloc(SIXEL_HISTOGRAM_SIZE, sizeof *histogram);
	for (y = 0; y < sy; y++) {
		for (x = 0; x < sx; x++) {
			pixel = sixel_from_image_pixel(&source, sourcex0, sourcey0,
			    sourcewidth, sourceheight, sx, sy, x, y);
			if (pixel[3] == 0)
				continue;
			index = ((pixel[0] >> 3) << 10)|
			    ((pixel[1] >> 3) << 5)|(pixel[2] >> 3);
			entry = &histogram[index];
			entry->count++;
			entry->red += pixel[0];
			entry->green += pixel[1];
			entry->blue += pixel[2];
		}
	}
	ncolours = sixel_make_palette(histogram, palette);
	free(histogram);
	if (ncolours == 0)
		return (NULL);

	si = xcalloc(1, sizeof *si);
	si->xpixel = xpixel;
	si->ypixel = ypixel;
	si->p2 = 1;
	si->set_ra = 1;
	si->ra_x = sx;
	si->ra_y = sy;
	si->ncolours = si->used_colours = ncolours;
	si->colours = xcalloc(si->ncolours, sizeof *si->colours);
	for (i = 0; i < si->ncolours; i++) {
		red = (palette[i].red * 100 + 127) / 255;
		green = (palette[i].green * 100 + 127) / 255;
		blue = (palette[i].blue * 100 + 127) / 255;
		si->colours[i] = (2U << 25)|(red << 16)|(green << 8)|blue;
	}

	cache = xmalloc(SIXEL_HISTOGRAM_SIZE * sizeof *cache);
	memset(cache, 0xff, SIXEL_HISTOGRAM_SIZE * sizeof *cache);
	current = xcalloc(((size_t)sx + 2) * 4, sizeof *current);
	next = xcalloc(((size_t)sx + 2) * 4, sizeof *next);
	for (y = 0; y < sy; y++) {
		for (x = 0; x < sx; x++) {
			pixel = sixel_from_image_pixel(&source, sourcex0, sourcey0,
			    sourcewidth, sourceheight, sx, sy, x, y);
			error_index = (x + 1) * 4;
			/* SIXEL pixels are binary, so dither alpha separately. */
			alpha = sixel_clamp_colour((int)pixel[3] +
			    current[error_index + 3] / 16);
			alpha_error = (int)alpha;
			if (alpha >= 128) {
				alpha_error -= 255;
				red = sixel_clamp_colour((int)pixel[0] +
				    current[error_index] / 16);
				green = sixel_clamp_colour((int)pixel[1] +
				    current[error_index + 1] / 16);
				blue = sixel_clamp_colour((int)pixel[2] +
				    current[error_index + 2] / 16);
				colour = sixel_nearest_colour(palette, ncolours, cache,
				    red, green, blue);
				if (sixel_set_pixel(si, x, y, colour + 1) != 0)
					goto fail;

				red_error = (int)red - palette[colour].red;
				green_error = (int)green - palette[colour].green;
				blue_error = (int)blue - palette[colour].blue;
				current[error_index + 4] += red_error * 7;
				current[error_index + 5] += green_error * 7;
				current[error_index + 6] += blue_error * 7;
				next[error_index - 4] += red_error * 3;
				next[error_index - 3] += green_error * 3;
				next[error_index - 2] += blue_error * 3;
				next[error_index] += red_error * 5;
				next[error_index + 1] += green_error * 5;
				next[error_index + 2] += blue_error * 5;
				next[error_index + 4] += red_error;
				next[error_index + 5] += green_error;
				next[error_index + 6] += blue_error;
			}
			current[error_index + 7] += alpha_error * 7;
			next[error_index - 1] += alpha_error * 3;
			next[error_index + 3] += alpha_error * 5;
			next[error_index + 7] += alpha_error;
		}
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

static void
sixel_remove_cache(struct sixel_output *so, struct sixel_image_cache **pp)
{
	struct sixel_image_cache	*cache = *pp;

	*pp = cache->next;
	so->size -= cache->size;
	sixel_free(cache->si);
	free(cache);
}

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

void
sixel_free_output(struct tty *tty, __unused int send)
{
	struct sixel_output		*so = tty->image_data;
	struct sixel_image_cache	*cache, *next;

	if (so == NULL)
		return;
	for (cache = so->images; cache != NULL; cache = next) {
		next = cache->next;
		sixel_free(cache->si);
		free(cache);
	}
	free(so);
	tty->image_data = NULL;
}

void
sixel_geometry_changed(struct tty *tty)
{
	sixel_free_output(tty, !!(tty->flags & TTY_OPENED));
}

static struct sixel_image *
sixel_render_image(struct image *im, u_int xpixel, u_int ypixel)
{
	struct sixel_image	*original;
	u_int			 sx, sy;

	image_get_cell_dimensions(im, &sx, &sy);
	/* Preserve SIXEL's original palette and indexed pixels when possible. */
	original = image_get_sixel(im);
	if (original != NULL)
		return (sixel_scale(original, xpixel, ypixel, 0, 0, sx, sy, 1));
	return (sixel_from_image(im, 0, 0, sx, sy, xpixel, ypixel));
}

static struct sixel_image *
sixel_get_image(struct tty *tty, struct image *im)
{
	struct sixel_output		*so = sixel_get_output(tty);
	struct sixel_image_cache	**pp, *cache, **oldest;
	struct sixel_image		*si;
	size_t			 size;

	sixel_collect_images(so);
	for (cache = so->images; cache != NULL; cache = cache->next) {
		if (cache->server_id != image_get_id(im) ||
		    cache->xpixel != tty->xpixel ||
		    cache->ypixel != tty->ypixel)
			continue;
		cache->age = ++so->age;
		return (cache->si);
	}

	si = sixel_render_image(im, tty->xpixel, tty->ypixel);
	if (si == NULL)
		return (NULL);
	size = sixel_image_size(si);
	if (size == 0 || size > IMAGE_SIZE_LIMIT) {
		/* The renderer still has a usable image, but it is not cacheable. */
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
	cache->xpixel = tty->xpixel;
	cache->ypixel = tty->ypixel;
	cache->size = size;
	cache->age = ++so->age;
	cache->si = si;
	cache->next = so->images;
	so->images = cache;
	so->size += size;
	return (si);
}

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

void
sixel_draw_rectangle(struct tty *tty, const struct image_rectangle *rectangle,
    const struct tty_style_ctx *style_ctx)
{
	struct sixel_image	*si, *crop;
	struct grid_cell	 draw_gc;
	char			*data;
	size_t			 size;
	u_int			 source_x, source_y, width, height, y;
	u_int			 destination_x, destination_y;

	si = sixel_get_image(tty, image_rectangle_get_image(rectangle));
	if (si == NULL)
		return;
	image_rectangle_get_coordinates(rectangle, &source_x, &source_y, &width,
	    &height, &destination_x, &destination_y);
	crop = sixel_scale(si, tty->xpixel, tty->ypixel,
	    source_x, source_y, width, height, 1);
	if (!sixel_image_is_cached(tty, si))
		sixel_free(si);
	if (crop == NULL)
		return;
	data = sixel_print(crop, NULL, &size);
	sixel_free(crop);
	if (data == NULL)
		return;
	memcpy(&draw_gc, image_rectangle_get_cell(rectangle), sizeof draw_gc);
	draw_gc.flags &= ~(GRID_FLAG_IMAGE|GRID_FLAG_SELECTED);
	utf8_set(&draw_gc.data, ' ');
	for (y = 0; y < height; y++) {
		tty_cursor(tty, destination_x, destination_y + y);
		tty_attributes(tty, &draw_gc, style_ctx);
		tty_repeat_space(tty, width);
		tty_reset(tty);
	}
	tty_region_off(tty);
	tty_margin_off(tty);
	tty_cursor(tty, destination_x, destination_y);
	tty->flags |= TTY_NOBLOCK;
	tty_putn(tty, data, size, 0);
	tty_invalidate(tty);
	free(data);
}

struct screen *
sixel_to_screen(struct sixel_image *si)
{
	struct screen		*s;
	struct screen_write_ctx	 ctx;
	struct grid_cell	 gc;
	u_int			 x, y, sx, sy;

	sixel_size_in_cells(si, &sx, &sy);

	s = xmalloc(sizeof *s);
	screen_init(s, sx, sy, 0);

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.attr |= (GRID_ATTR_CHARSET|GRID_ATTR_DIM);
	utf8_set(&gc.data, '~');

	screen_write_start(&ctx, s);
	if (sx == 1 || sy == 1) {
		for (y = 0; y < sy; y++) {
			for (x = 0; x < sx; x++)
				grid_view_set_cell(s->grid, x, y, &gc);
		}
	} else {
		screen_write_box(&ctx, sx, sy, BOX_LINES_DEFAULT, NULL, NULL);
		for (y = 1; y < sy - 1; y++) {
			for (x = 1; x < sx - 1; x++)
				grid_view_set_cell(s->grid, x, y, &gc);
		}
	}
	screen_write_stop(&ctx);
	return (s);
}
