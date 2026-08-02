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
 * MERCHANTABILITY, FITNESS AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH
 * THIS SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <sys/types.h>

#include <limits.h>
#include <png.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "tmux.h"

/* Generated from Kitty's Unicode 6.0 rowcolumn-diacritics.txt. */
static const uint32_t kitty_diacritics[] = {
	0x0305, 0x030D, 0x030E, 0x0310, 0x0312, 0x033D, 0x033E, 0x033F,
	0x0346, 0x034A, 0x034B, 0x034C, 0x0350, 0x0351, 0x0352, 0x0357,
	0x035B, 0x0363, 0x0364, 0x0365, 0x0366, 0x0367, 0x0368, 0x0369,
	0x036A, 0x036B, 0x036C, 0x036D, 0x036E, 0x036F, 0x0483, 0x0484,
	0x0485, 0x0486, 0x0487, 0x0592, 0x0593, 0x0594, 0x0595, 0x0597,
	0x0598, 0x0599, 0x059C, 0x059D, 0x059E, 0x059F, 0x05A0, 0x05A1,
	0x05A8, 0x05A9, 0x05AB, 0x05AC, 0x05AF, 0x05C4, 0x0610, 0x0611,
	0x0612, 0x0613, 0x0614, 0x0615, 0x0616, 0x0617, 0x0657, 0x0658,
	0x0659, 0x065A, 0x065B, 0x065D, 0x065E, 0x06D6, 0x06D7, 0x06D8,
	0x06D9, 0x06DA, 0x06DB, 0x06DC, 0x06DF, 0x06E0, 0x06E1, 0x06E2,
	0x06E4, 0x06E7, 0x06E8, 0x06EB, 0x06EC, 0x0730, 0x0732, 0x0733,
	0x0735, 0x0736, 0x073A, 0x073D, 0x073F, 0x0740, 0x0741, 0x0743,
	0x0745, 0x0747, 0x0749, 0x074A, 0x07EB, 0x07EC, 0x07ED, 0x07EE,
	0x07EF, 0x07F0, 0x07F1, 0x07F3, 0x0816, 0x0817, 0x0818, 0x0819,
	0x081B, 0x081C, 0x081D, 0x081E, 0x081F, 0x0820, 0x0821, 0x0822,
	0x0823, 0x0825, 0x0826, 0x0827, 0x0829, 0x082A, 0x082B, 0x082C,
	0x082D, 0x0951, 0x0953, 0x0954, 0x0F82, 0x0F83, 0x0F86, 0x0F87,
	0x135D, 0x135E, 0x135F, 0x17DD, 0x193A, 0x1A17, 0x1A75, 0x1A76,
	0x1A77, 0x1A78, 0x1A79, 0x1A7A, 0x1A7B, 0x1A7C, 0x1B6B, 0x1B6D,
	0x1B6E, 0x1B6F, 0x1B70, 0x1B71, 0x1B72, 0x1B73, 0x1CD0, 0x1CD1,
	0x1CD2, 0x1CDA, 0x1CDB, 0x1CE0, 0x1DC0, 0x1DC1, 0x1DC3, 0x1DC4,
	0x1DC5, 0x1DC6, 0x1DC7, 0x1DC8, 0x1DC9, 0x1DCB, 0x1DCC, 0x1DD1,
	0x1DD2, 0x1DD3, 0x1DD4, 0x1DD5, 0x1DD6, 0x1DD7, 0x1DD8, 0x1DD9,
	0x1DDA, 0x1DDB, 0x1DDC, 0x1DDD, 0x1DDE, 0x1DDF, 0x1DE0, 0x1DE1,
	0x1DE2, 0x1DE3, 0x1DE4, 0x1DE5, 0x1DE6, 0x1DFE, 0x20D0, 0x20D1,
	0x20D4, 0x20D5, 0x20D6, 0x20D7, 0x20DB, 0x20DC, 0x20E1, 0x20E7,
	0x20E9, 0x20F0, 0x2CEF, 0x2CF0, 0x2CF1, 0x2DE0, 0x2DE1, 0x2DE2,
	0x2DE3, 0x2DE4, 0x2DE5, 0x2DE6, 0x2DE7, 0x2DE8, 0x2DE9, 0x2DEA,
	0x2DEB, 0x2DEC, 0x2DED, 0x2DEE, 0x2DEF, 0x2DF0, 0x2DF1, 0x2DF2,
	0x2DF3, 0x2DF4, 0x2DF5, 0x2DF6, 0x2DF7, 0x2DF8, 0x2DF9, 0x2DFA,
	0x2DFB, 0x2DFC, 0x2DFD, 0x2DFE, 0x2DFF, 0xA66F, 0xA67C, 0xA67D,
	0xA6F0, 0xA6F1, 0xA8E0, 0xA8E1, 0xA8E2, 0xA8E3, 0xA8E4, 0xA8E5,
	0xA8E6, 0xA8E7, 0xA8E8, 0xA8E9, 0xA8EA, 0xA8EB, 0xA8EC, 0xA8ED,
	0xA8EE, 0xA8EF, 0xA8F0, 0xA8F1, 0xAAB0, 0xAAB2, 0xAAB3, 0xAAB7,
	0xAAB8, 0xAABE, 0xAABF, 0xAAC1, 0xFE20, 0xFE21, 0xFE22, 0xFE23,
	0xFE24, 0xFE25, 0xFE26, 0x10A0F, 0x10A38, 0x1D185, 0x1D186, 0x1D187,
	0x1D188, 0x1D189, 0x1D1AA, 0x1D1AB, 0x1D1AC, 0x1D1AD, 0x1D242, 0x1D243,
	0x1D244
};

#define KITTY_IMAGE_LIMIT (64 * 1024 * 1024)

struct kitty_state {
	char	 action;
	char	 delete;
	u_int	 format;
	char	 medium;
	char	 compression;
	u_int	 width;
	u_int	 height;
	u_int	 columns;
	u_int	 rows;
	u_int	 image_id;
	u_int	 quiet;
	u_int	 data_size;
	int	 more;

	char	*encoded;
	size_t	 encodedlen;
};

struct kitty_source {
	u_int			 app_id;
	u_int			 server_id;
	struct kitty_source	*next;
};

struct kitty_context {
	struct kitty_state	*transfer;
	struct kitty_source	*sources;
};

struct tty_image_cache {
	u_int			 server_id;
	u_int			 kitty_id;
	u_int			 xpixel;
	u_int			 ypixel;
	struct tty_image_cache	*next;
};

static size_t
kitty_utf8(char *out, uint32_t value)
{
	if (value < 0x80) {
		out[0] = value;
		return (1);
	}
	if (value < 0x800) {
		out[0] = 0xc0 | (value >> 6);
		out[1] = 0x80 | (value & 0x3f);
		return (2);
	}
	if (value < 0x10000) {
		out[0] = 0xe0 | (value >> 12);
		out[1] = 0x80 | ((value >> 6) & 0x3f);
		out[2] = 0x80 | (value & 0x3f);
		return (3);
	}
	out[0] = 0xf0 | (value >> 18);
	out[1] = 0x80 | ((value >> 12) & 0x3f);
	out[2] = 0x80 | ((value >> 6) & 0x3f);
	out[3] = 0x80 | (value & 0x3f);
	return (4);
}

static void
kitty_delete(struct tty *tty, u_int id)
{
	char	s[64];

	xsnprintf(s, sizeof s, "\033_Ga=d,d=I,i=%u,q=2\033\\", id);
	tty_puts(tty, s);
}

void
kitty_images_free(struct tty *tty, int send)
{
	struct tty_image_cache	*cache, *next;

	for (cache = tty->images; cache != NULL; cache = next) {
		next = cache->next;
		if (send)
			kitty_delete(tty, cache->kitty_id);
		free(cache);
	}
	tty->images = NULL;
}

static void
kitty_images_collect(struct tty *tty)
{
	struct tty_image_cache	**pp, *cache;

	for (pp = &tty->images; (cache = *pp) != NULL; ) {
		if (image_find(cache->server_id) != NULL) {
			pp = &cache->next;
			continue;
		}
		kitty_delete(tty, cache->kitty_id);
		*pp = cache->next;
		free(cache);
	}
}

static void
kitty_place(struct tty *tty, struct image *im, u_int id)
{
	char		 control[192];
	u_int		 x, y, columns, rows, px, py, pwidth, pheight;
	u_int		 placement = 1;

	for (y = 0; y < im->sy; y += nitems(kitty_diacritics)) {
		rows = im->sy - y;
		if (rows > nitems(kitty_diacritics))
			rows = nitems(kitty_diacritics);
		py = (uint64_t)y * im->height / im->sy;
		pheight = (uint64_t)(y + rows) * im->height / im->sy - py;
		if (pheight == 0) {
			py = im->height - 1;
			pheight = 1;
		}
		for (x = 0; x < im->sx; x += nitems(kitty_diacritics)) {
			columns = im->sx - x;
			if (columns > nitems(kitty_diacritics))
				columns = nitems(kitty_diacritics);
			px = (uint64_t)x * im->width / im->sx;
			pwidth = (uint64_t)(x + columns) * im->width /
			    im->sx - px;
			if (pwidth == 0) {
				px = im->width - 1;
				pwidth = 1;
			}
			xsnprintf(control, sizeof control,
			    "\033_Ga=p,U=1,i=%u,p=%u,x=%u,y=%u,w=%u,h=%u,"
			    "c=%u,r=%u,q=2\033\\", id, placement++, px, py,
			    pwidth, pheight, columns, rows);
			tty_puts(tty, control);
		}
	}
}

static struct tty_image_cache *
kitty_upload(struct tty *tty, struct image *im)
{
	struct tty_image_cache	*cache;
	char			 control[128], encoded[4097];
	size_t			 offset, size;
	int			 encodedlen;
	u_int			 id;

	for (cache = tty->images; cache != NULL; cache = cache->next) {
		if (cache->server_id != im->id)
			continue;
		if (cache->xpixel == tty->xpixel &&
		    cache->ypixel == tty->ypixel)
			return (cache);
		kitty_delete(tty, cache->kitty_id);
		cache->server_id = 0;
		break;
	}
	if (cache == NULL) {
		cache = xcalloc(1, sizeof *cache);
		cache->next = tty->images;
		tty->images = cache;
	}
	do {
		id = ++tty->image_next_id & 0xffffff;
	} while (id == 0);
	cache->server_id = im->id;
	cache->kitty_id = id;
	cache->xpixel = tty->xpixel;
	cache->ypixel = tty->ypixel;

	for (offset = 0; offset < im->size; offset += size) {
		size = im->size - offset;
		if (size > 3072)
			size = 3072;
		encodedlen = b64_ntop(im->pixels + offset, size, encoded,
		    sizeof encoded);
		if (encodedlen < 0)
			return (NULL);
		if (offset == 0) {
			xsnprintf(control, sizeof control,
			    "\033_Ga=t,f=32,s=%u,v=%u,i=%u,q=2,m=%d;",
			    im->width, im->height, id,
			    offset + size < im->size);
		} else {
			xsnprintf(control, sizeof control, "\033_Gm=%d;",
			    offset + size < im->size);
		}
		tty_puts(tty, control);
		tty_putn(tty, encoded, encodedlen, 0);
		tty_puts(tty, "\033\\");
	}
	kitty_place(tty, im, id);
	return (cache);
}

static u_int
kitty_placement(struct image *im, u_int x, u_int y)
{
	u_int	across;

	across = (im->sx + nitems(kitty_diacritics) - 1) /
	    nitems(kitty_diacritics);
	return ((y / nitems(kitty_diacritics)) * across +
	    x / nitems(kitty_diacritics) + 1);
}

static void
kitty_placeholder(struct tty *tty, struct tty_image_cache *cache,
    struct image *im, u_int x, u_int y, u_int count)
{
	char		 buf[8192], sgr[96];
	size_t		 used = 0;
	u_int		 localx, localy, placement, i;

	localx = x % nitems(kitty_diacritics);
	localy = y % nitems(kitty_diacritics);
	placement = kitty_placement(im, x, y);
	xsnprintf(sgr, sizeof sgr,
	    "\033[38;2;%u;%u;%um\033[58;2;%u;%u;%um",
	    (cache->kitty_id >> 16) & 0xff, (cache->kitty_id >> 8) & 0xff,
	    cache->kitty_id & 0xff, (placement >> 16) & 0xff,
	    (placement >> 8) & 0xff, placement & 0xff);
	tty_puts(tty, sgr);
	for (i = 0; i < count; i++) {
		used += kitty_utf8(buf + used, 0x10eeee);
		if (i == 0) {
			used += kitty_utf8(buf + used,
			    kitty_diacritics[localy]);
			used += kitty_utf8(buf + used,
			    kitty_diacritics[localx]);
		}
	}
	tty_putn(tty, buf, used, count);
	tty_puts(tty, "\033[39;59m");
}

/*
 * Draw only the marker cells present in a scene-selected horizontal span.
 * Overlay and pane clipping have already been applied by screen-redraw.c.
 */
void
kitty_draw_line(struct tty *tty, struct screen *s, u_int px, u_int py,
    u_int nx, u_int atx, u_int aty, const struct tty_style_ctx *style_ctx)
{
	struct grid_cell	 gc, next, draw_gc;
	struct image		*im;
	struct tty_image_cache	*cache;
	u_int			 i, run, tile;

	kitty_images_collect(tty);
	for (i = 0; i < nx; i += run) {
		grid_view_get_cell(s->grid, px + i, py, &gc);
		if (~gc.flags & GRID_FLAG_IMAGE) {
			run = 1;
			continue;
		}
		im = image_find(gc.image_id);
		if (im == NULL) {
			run = 1;
			continue;
		}
		tile = gc.image_x / nitems(kitty_diacritics);
		for (run = 1; i + run < nx; run++) {
			grid_view_get_cell(s->grid, px + i + run, py, &next);
			if (~next.flags & GRID_FLAG_IMAGE ||
			    next.image_id != gc.image_id ||
			    next.image_y != gc.image_y ||
			    next.image_x != gc.image_x + run ||
			    next.image_x / nitems(kitty_diacritics) != tile)
				break;
		}
		cache = kitty_upload(tty, im);
		if (cache == NULL)
			continue;
		tty_cursor(tty, atx + i, aty);
		memcpy(&draw_gc, &gc, sizeof draw_gc);
		draw_gc.flags &= ~(GRID_FLAG_IMAGE|GRID_FLAG_SELECTED);
		utf8_set(&draw_gc.data, ' ');
		tty_attributes(tty, &draw_gc, style_ctx);
		kitty_placeholder(tty, cache, im, gc.image_x, gc.image_y,
		    run);
		tty_reset(tty);
	}
}

static int
kitty_number(const char *s, size_t len, u_int *value)
{
	char		 copy[32];
	const char	*errstr;
	long long	 ll;

	if (len == 0 || len >= sizeof copy)
		return (-1);
	memcpy(copy, s, len);
	copy[len] = '\0';
	ll = strtonum(copy, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return (-1);
	*value = ll;
	return (0);
}

static int
kitty_control(struct kitty_state *ks, const u_char *buf, size_t len)
{
	const u_char	*value, *end = buf + len, *comma;
	size_t		 valuelen;
	u_int		 number;
	char		 key;

	while (buf < end) {
		key = *buf++;
		if (buf == end || *buf++ != '=')
			return (-1);
		value = buf;
		comma = memchr(buf, ',', end - buf);
		if (comma == NULL) {
			valuelen = end - buf;
			buf = end;
		} else {
			valuelen = comma - buf;
			buf = comma + 1;
		}
		if (valuelen == 0)
			return (-1);

		switch (key) {
		case 'a':
			ks->action = value[0];
			break;
		case 'd':
			ks->delete = value[0];
			break;
		case 't':
			ks->medium = value[0];
			break;
		case 'o':
			ks->compression = value[0];
			break;
		case 'f':
		case 's':
		case 'v':
		case 'c':
		case 'r':
		case 'i':
		case 'q':
		case 'm':
		case 'S':
			if (kitty_number((const char *)value, valuelen,
			    &number) != 0)
				return (-1);
			switch (key) {
			case 'f': ks->format = number; break;
			case 's': ks->width = number; break;
			case 'v': ks->height = number; break;
			case 'c': ks->columns = number; break;
			case 'r': ks->rows = number; break;
			case 'i': ks->image_id = number; break;
			case 'q': ks->quiet = number; break;
			case 'm': ks->more = (number != 0); break;
			case 'S': ks->data_size = number; break;
			}
			break;
		}
	}
	return (0);
}

static void
kitty_state_free(struct kitty_state *ks)
{
	if (ks == NULL)
		return;
	free(ks->encoded);
	free(ks);
}

void
kitty_free_state(void *state)
{
	struct kitty_context	*kc = state;
	struct kitty_source	*source, *next;

	if (kc == NULL)
		return;
	kitty_state_free(kc->transfer);
	for (source = kc->sources; source != NULL; source = next) {
		next = source->next;
		image_free(source->server_id);
		free(source);
	}
	free(kc);
}

static struct kitty_source *
kitty_source_find(struct kitty_context *kc, u_int id)
{
	struct kitty_source	*source;

	for (source = kc->sources; source != NULL; source = source->next) {
		if (source->app_id == id)
			return (source);
	}
	return (NULL);
}

static void
kitty_source_set(struct kitty_context *kc, u_int id, struct image *im)
{
	struct kitty_source	*source;

	if (id == 0)
		return;
	source = kitty_source_find(kc, id);
	if (source == NULL) {
		source = xcalloc(1, sizeof *source);
		source->app_id = id;
		source->next = kc->sources;
		kc->sources = source;
	} else
		image_free(source->server_id);
	image_ref(im->id);
	source->server_id = im->id;
}

static struct image *
kitty_source_remove(struct kitty_context *kc, u_int id)
{
	struct kitty_source	**pp, *source;
	struct image		*im;

	for (pp = &kc->sources; (source = *pp) != NULL; pp = &source->next) {
		if (source->app_id != id)
			continue;
		im = image_find(source->server_id);
		if (im != NULL)
			image_ref(im->id);
		*pp = source->next;
		image_free(source->server_id);
		free(source);
		return (im);
	}
	return (NULL);
}

static struct image *
kitty_source_get(struct kitty_context *kc, u_int id)
{
	struct kitty_source	*source;
	struct image		*im;

	source = kitty_source_find(kc, id);
	if (source == NULL)
		return (NULL);
	im = image_find(source->server_id);
	if (im != NULL)
		image_ref(im->id);
	return (im);
}

static int
kitty_append(struct kitty_state *ks, const u_char *buf, size_t len)
{
	if (len > KITTY_IMAGE_LIMIT ||
	    ks->encodedlen > KITTY_IMAGE_LIMIT - len)
		return (-1);
	ks->encoded = xrealloc(ks->encoded, ks->encodedlen + len + 1);
	memcpy(ks->encoded + ks->encodedlen, buf, len);
	ks->encodedlen += len;
	ks->encoded[ks->encodedlen] = '\0';
	return (0);
}

static u_char *
kitty_base64(struct kitty_state *ks, size_t *size)
{
	u_char	*out;
	size_t	 needed;
	int	 result;

	if (ks->encodedlen > (SIZE_MAX - 3) / 3 * 4)
		return (NULL);
	needed = (ks->encodedlen + 3) / 4 * 3;
	out = xmalloc(needed == 0 ? 1 : needed);
	result = b64_pton(ks->encoded, out, needed);
	if (result < 0) {
		free(out);
		return (NULL);
	}
	*size = result;
	return (out);
}

static u_char *
kitty_png(const u_char *data, size_t size, u_int *width, u_int *height)
{
	png_image	 pi;
	u_char		*pixels;

	memset(&pi, 0, sizeof pi);
	pi.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_memory(&pi, data, size))
		return (NULL);
	pi.format = PNG_FORMAT_RGBA;
	if (pi.width == 0 || pi.height == 0 ||
	    PNG_IMAGE_SIZE(pi) > KITTY_IMAGE_LIMIT) {
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

static u_char *
kitty_raw(struct kitty_state *ks, u_char *data, size_t size)
{
	u_char		*raw, *pixels;
	size_t		 expected, i, j;
	uLongf		 rawlen;
	u_int		 bytes;

	bytes = (ks->format == 24 ? 3 : 4);
	if (ks->width == 0 || ks->height == 0 ||
	    (uint64_t)ks->width * ks->height * bytes > KITTY_IMAGE_LIMIT)
		return (NULL);
	expected = (size_t)ks->width * ks->height * bytes;
	raw = data;
	if (ks->compression == 'z') {
		rawlen = expected;
		raw = xmalloc(expected);
		if (uncompress(raw, &rawlen, data, size) != Z_OK ||
		    rawlen != expected) {
			free(raw);
			return (NULL);
		}
		size = rawlen;
	} else if (ks->compression != '\0')
		return (NULL);
	if (size != expected) {
		if (raw != data)
			free(raw);
		return (NULL);
	}
	if (bytes == 4)
		return (raw);

	pixels = xmalloc((size_t)ks->width * ks->height * 4);
	for (i = j = 0; i < expected; i += 3, j += 4) {
		pixels[j] = raw[i];
		pixels[j + 1] = raw[i + 1];
		pixels[j + 2] = raw[i + 2];
		pixels[j + 3] = 255;
	}
	if (raw != data)
		free(raw);
	return (pixels);
}

/*
 * Parse one Kitty graphics APC body (without the leading G). Only direct
 * static images are accepted. The returned image owns decoded RGBA pixels.
 */
struct image *
kitty_parse_image(void **state, const u_char *buf, size_t len, u_int xpixel,
    u_int ypixel, u_int *image_id, u_int *quiet, char *action, int *status)
{
	struct kitty_context	*kc = *state;
	struct kitty_state	*ks;
	const u_char		*semi;
	u_char			*decoded, *pixels;
	u_char			*uncompressed;
	size_t			 controllen, payloadlen, decodedlen;
	uLongf			 uncompressedlen;
	u_int			 sx, sy;
	struct image		*im;

	if (kc == NULL) {
		kc = xcalloc(1, sizeof *kc);
		*state = kc;
	}
	*image_id = 0;
	*quiet = 0;
	*action = '\0';
	*status = KITTY_PARSE_ERROR;
	ks = kc->transfer;
	semi = memchr(buf, ';', len);
	controllen = (semi == NULL ? len : (size_t)(semi - buf));
	payloadlen = (semi == NULL ? 0 : len - controllen - 1);
	if (ks == NULL) {
		ks = xcalloc(1, sizeof *ks);
		ks->action = 't';
		ks->delete = 'a';
		ks->format = 32;
		ks->medium = 'd';
	}
	*image_id = ks->image_id;
	*quiet = ks->quiet;
	*action = ks->action;
	if (kitty_control(ks, buf, controllen) != 0 ||
	    ks->medium != 'd' ||
	    (payloadlen != 0 &&
	    kitty_append(ks, semi + 1, payloadlen) != 0))
		goto fail;

	*image_id = ks->image_id;
	*quiet = ks->quiet;
	*action = ks->action;
	if (ks->more) {
		kc->transfer = ks;
		*status = KITTY_PARSE_MORE;
		return (NULL);
	}
	kc->transfer = NULL;
	if (ks->action == 'p') {
		im = kitty_source_get(kc, ks->image_id);
		*status = (im == NULL ? KITTY_PARSE_MISSING :
		    KITTY_PARSE_OK);
		kitty_state_free(ks);
		return (im);
	}
	if (ks->action == 'd') {
		if (ks->delete == 'a' || ks->delete == 'A')
			im = NULL;
		else if (ks->delete == 'i')
			im = kitty_source_get(kc, ks->image_id);
		else if (ks->delete == 'I')
			im = kitty_source_remove(kc, ks->image_id);
		else
			goto fail;
		if ((ks->delete == 'i' || ks->delete == 'I') &&
		    im == NULL)
			*status = KITTY_PARSE_MISSING;
		else
			*status = KITTY_PARSE_OK;
		kitty_state_free(ks);
		return (im);
	}
	if (ks->action != 'T' && ks->action != 't' && ks->action != 'q')
		goto fail;

	decoded = kitty_base64(ks, &decodedlen);
	if (decoded == NULL)
		goto fail;
	if (ks->format == 100) {
		if (ks->compression == 'z') {
			if (ks->data_size == 0 ||
			    ks->data_size > KITTY_IMAGE_LIMIT) {
				free(decoded);
				goto fail;
			}
			uncompressedlen = ks->data_size;
			uncompressed = xmalloc(uncompressedlen);
			if (uncompress(uncompressed, &uncompressedlen, decoded,
			    decodedlen) != Z_OK ||
			    uncompressedlen != ks->data_size) {
				free(decoded);
				free(uncompressed);
				goto fail;
			}
			free(decoded);
			decoded = uncompressed;
			decodedlen = uncompressedlen;
		} else if (ks->compression != '\0') {
				free(decoded);
				goto fail;
		}
		pixels = kitty_png(decoded, decodedlen, &ks->width, &ks->height);
		free(decoded);
	} else if (ks->format == 24 || ks->format == 32) {
		pixels = kitty_raw(ks, decoded, decodedlen);
		if (pixels != decoded)
			free(decoded);
	} else {
		free(decoded);
		goto fail;
	}
	if (pixels == NULL)
		goto fail;

	sx = ks->columns;
	sy = ks->rows;
	if (xpixel == 0)
		xpixel = 8;
	if (ypixel == 0)
		ypixel = 16;
	if (sx == 0)
		sx = (ks->width + xpixel - 1) / xpixel;
	if (sy == 0)
		sy = (ks->height + ypixel - 1) / ypixel;
	im = image_create(ks->width, ks->height, sx, sy, pixels);
	if (im == NULL)
		free(pixels);
	else
		*status = KITTY_PARSE_OK;
	if (im != NULL && ks->action != 'q')
		kitty_source_set(kc, ks->image_id, im);
	if (im != NULL && (ks->action == 'q' || ks->action == 't')) {
		image_free(im->id);
		im = NULL;
	}
	kitty_state_free(ks);
	return (im);

fail:
	kc->transfer = NULL;
	kitty_state_free(ks);
	return (NULL);
}
