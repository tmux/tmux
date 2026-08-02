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
#include "image-diacritics.h"

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
