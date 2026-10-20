/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#ifdef HAVE_LIBPNG
#include <png.h>
#endif
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LIBZ
#include <zlib.h>
#endif

#include "tmux.h"

/* Decode a Base64 payload. */
u_char *
image_base64_decode(const char *data, size_t size, size_t limit, size_t *osize)
{
	char	*copy;
	u_char	*o;
	size_t	 needed, padded, padding;
	int	 n;

	if (size > SIZE_MAX - 3)
		return (NULL);
	padding = (4 - size % 4) % 4;
	if (padding == 3)
		return (NULL);
	padded = size + padding;
	needed = padded / 4 * 3;
	if (needed > limit || needed > INT_MAX)
		return (NULL);

	copy = xmalloc(padded + 1);
	memcpy(copy, data, size);
	memset(copy + size, '=', padding);
	copy[padded] = '\0';

	if (needed == 0)
		o = xmalloc(1);
	else
		o = xmalloc(needed);
	n = b64_pton(copy, o, needed);
	free(copy);
	if (n < 0) {
		free(o);
		return (NULL);
	}
	*osize = n;
	return (o);
}

/* Decompress a payload. */
#ifdef HAVE_LIBZ
static u_char *
image_zlib_decode(const u_char *data, size_t size, size_t expected,
    size_t limit)
{
	u_char	*r;
	uLongf	 rsize;

	if (expected == 0 ||
	    expected > limit ||
	    size > limit ||
	    expected > ULONG_MAX ||
	    size > ULONG_MAX)
		return (NULL);
	rsize = expected;

	r = xmalloc(expected);
	if (uncompress(r, &rsize, data, size) != Z_OK || rsize != expected) {
		free(r);
		return (NULL);
	}
	return (r);
}
#else
static u_char *
image_zlib_decode(__unused const u_char *data, __unused size_t size,
    __unused size_t expected, __unused size_t limit)
{
	return (NULL);
}
#endif

/* Decode PNG data into RGBA pixels. */
#ifdef HAVE_LIBPNG
u_char *
image_png_decode(const u_char *data, size_t size, size_t limit, int z,
    size_t expected, u_int *width, u_int *height)
{
	png_image	 pi;
	u_char		*r = NULL, *p = NULL;

	if (size == 0 || size > limit)
		return (NULL);
	if (z) {
		r = image_zlib_decode(data, size, expected, limit);
		if (r == NULL)
			return (NULL);
		data = r;
		size = expected;
	}

	memset(&pi, 0, sizeof pi);
	pi.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_memory(&pi, data, size))
		goto out;
	pi.format = PNG_FORMAT_RGBA;

	if (pi.width == 0 ||
	    pi.height == 0 ||
	    (uint64_t)pi.width * pi.height > limit / 4)
		goto out;
	p = xmalloc(PNG_IMAGE_SIZE(pi));

	if (!png_image_finish_read(&pi, NULL, p, 0, NULL)) {
		free(p);
		p = NULL;
		goto out;
	}

	*width = pi.width;
	*height = pi.height;

out:
	png_image_free(&pi);
	free(r);
	return (p);
}
#else
u_char *
image_png_decode(__unused const u_char *data, __unused size_t size,
    __unused size_t limit, __unused int z, __unused size_t expected,
    __unused u_int *width, __unused u_int *height)
{
	return (NULL);
}
#endif

/* Decode raw RGB or RGBA data. */
u_char *
image_raw_decode(const u_char *data, size_t size, size_t limit, u_int format,
    u_int width, u_int height, int z)
{
	u_char	*r = NULL, *o;
	size_t	 expected, i, j;
	u_int	 bytes;

	if (format != 24 && format != 32)
		return (NULL);

	if (width == 0 ||
	    height == 0 ||
	    size > limit ||
	    (uint64_t)width * height > limit / 4)
		return (NULL);
	bytes = (format == 24 ? 3 : 4);
	expected = (size_t)width * height * bytes;

	if (z) {
		r = image_zlib_decode(data, size, expected, limit);
		if (r == NULL)
			return (NULL);
		data = r;
	} else {
		if (size != expected)
			return (NULL);
	}

	if (bytes == 4) {
		if (r != NULL)
			return (r);
		o = xmalloc(expected);
		memcpy(o, data, expected);
		return (o);
	}

	o = xmalloc((size_t)width * height * 4);
	for (i = j = 0; i < expected; i += 3, j += 4) {
		o[j + 0] = data[i + 0];
		o[j + 1] = data[i + 1];
		o[j + 2] = data[i + 2];
		o[j + 3] = 255;
	}
	free(r);
	return (o);
}
