/* $OpenBSD$ */

/*
 * Copyright (c) 2019 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#define SIXEL_WIDTH_LIMIT 10000
#define SIXEL_HEIGHT_LIMIT 10000

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
	struct sixel_line	*sl;
	u_int			 i;

	if (sixel_parse_expand_lines(si, si->dy + 6) != 0)
		return (1);
	sl = &si->lines[si->dy];

	for (i = 0; i < 6; i++) {
		if (sixel_parse_expand_line(si, sl, si->dx + 1) != 0)
			return (1);
		if (ch & (1 << i))
			sl->data[si->dx] = si->dc;
		sl++;
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
	u_int		 c, type, r, g, b;

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
	si->dc = c + 1;
	if (endptr == last || *endptr != ';')
		return (last);

	type = strtoul(endptr + 1, &endptr, 10);
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	r = strtoul(endptr + 1, &endptr, 10);
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	g = strtoul(endptr + 1, &endptr, 10);
	if (endptr == last || *endptr != ';') {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}
	b = strtoul(endptr + 1, &endptr, 10);
	if (endptr != last) {
		log_debug("%s: missing ;", __func__);
		return (NULL);
	}

	if (type != 1 && type != 2) {
		log_debug("%s: invalid type %d", __func__, type);
		return (NULL);
	}
	if (c + 1 > si->ncolours) {
		si->colours = xrecallocarray(si->colours, si->ncolours, c + 1,
		    sizeof *si->colours);
		si->ncolours = c + 1;
	}
	si->colours[c] = (type << 24) | (r << 16) | (g << 8) | b;
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
	free(si);
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
	if ((si->x % si->xpixel) == 0)
		*x = (si->x / si->xpixel);
	else
		*x = 1 + (si->x / si->xpixel);
	if ((si->y % si->ypixel) == 0)
		*y = (si->y / si->ypixel);
	else
		*y = 1 + (si->y / si->ypixel);
}

struct sixel_image *
sixel_scale(struct sixel_image *si, u_int xpixel, u_int ypixel, u_int ox,
    u_int oy, u_int sx, u_int sy, int colours)
{
	struct sixel_image	*new;
	u_int			 cx, cy, pox, poy, psx, psy, tsx, tsy, px, py;
	u_int			 x, y, i;

	/*
	 * We want to get the section of the image at ox,oy in image cells and
	 * map it onto the same size in terminal cells, remembering that we
	 * can only draw vertical sections of six pixels.
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
	tsy = ((sy * ypixel) / 6) * 6;

	new = xcalloc (1, sizeof *si);
	new->xpixel = xpixel;
	new->ypixel = ypixel;
	new->p2 = si->p2;

	new->set_ra = si->set_ra;
	/* clamp to slice end */
	new->ra_x = si->ra_x < psx ? si->ra_x : psx;
	new->ra_y = si->ra_y < psy ? si->ra_y : psy;
	/* subtract slice origin */
	new->ra_x = new->ra_x > pox ? new->ra_x - pox : 0;
	new->ra_y = new->ra_y > poy ? new->ra_y - poy : 0;
	/* resize */
	new->ra_x = new->ra_x * xpixel / si->xpixel;
	new->ra_y = new->ra_y * ypixel / si->ypixel;

	for (y = 0; y < tsy; y++) {
		py = poy + ((double)y * psy / tsy);
		for (x = 0; x < tsx; x++) {
			px = pox + ((double)x * psx / tsx);
			sixel_set_pixel(new, x, y, sixel_get_pixel(si, px, py));
		}
	}

	if (colours) {
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
	if (*used + slen >= *len + 1) {
		(*len) *= 2;
		*buf = xrealloc(*buf, *len);
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
	u_int			*colours, ncolours, i, c, y, *active, nactive;
	struct sixel_chunk	*chunks, *chunk;

	if (map != NULL) {
		colours = map->colours;
		ncolours = map->ncolours;
	} else {
		colours = si->colours;
		ncolours = si->ncolours;
	}

	if (ncolours == 0)
		return (NULL);

	len = 8192;
	buf = xmalloc(len);

	tmplen = xsnprintf(tmp, sizeof tmp, "\033P0;%uq", si->p2);
	sixel_print_add(&buf, &len, &used, tmp, tmplen);

	if (si->set_ra) {
		tmplen = xsnprintf(tmp, sizeof tmp, "\"1;1;%u;%u", si->ra_x,
		    si->ra_y);
		sixel_print_add(&buf, &len, &used, tmp, tmplen);
	}

	chunks = xcalloc(ncolours, sizeof *chunks);
	active = xcalloc(ncolours, sizeof *active);

	for (i = 0; i < ncolours; i++) {
		c = colours[i];
		tmplen = xsnprintf(tmp, sizeof tmp, "#%u;%u;%u;%u;%u",
		    i, c >> 24, (c >> 16) & 0xff, (c >> 8) & 0xff, c & 0xff);
		sixel_print_add(&buf, &len, &used, tmp, tmplen);

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

	for (i = 0; i < ncolours; i++)
		free(chunks[i].data);
	free(active);
	free(chunks);

	return (buf);
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
