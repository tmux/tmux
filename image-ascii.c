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

#include "tmux.h"

void
image_make_ascii(struct image *im)
{
	static const char	ramp[] = " .:-=+*#%@";
	const u_char		*pixel;
	uint64_t		 total, count;
	u_int			 x, y, px, py, x0, x1, y0, y1, level;

	im->ascii = xcalloc((size_t)im->sx * im->sy, 1);
	for (y = 0; y < im->sy; y++) {
		y0 = ((uint64_t)y * im->height) / im->sy;
		y1 = ((uint64_t)(y + 1) * im->height) / im->sy;
		if (y1 <= y0)
			y1 = y0 + 1;
		for (x = 0; x < im->sx; x++) {
			x0 = ((uint64_t)x * im->width) / im->sx;
			x1 = ((uint64_t)(x + 1) * im->width) / im->sx;
			if (x1 <= x0)
				x1 = x0 + 1;

			total = count = 0;
			for (py = y0; py < y1 && py < im->height; py++) {
				for (px = x0; px < x1 && px < im->width; px++) {
					pixel = im->pixels + py * im->stride + px * 4;
					total += ((2126ULL * pixel[0] +
					    7152ULL * pixel[1] +
					    722ULL * pixel[2]) / 10000) *
					    pixel[3] / 255;
					count++;
				}
			}
			if (count == 0)
				continue;
			level = (total / count) * (sizeof ramp - 2) / 255;
			im->ascii[y * im->sx + x] = ramp[level];
		}
	}
}

const char *
image_get_ascii(struct image *im, u_int x, u_int y)
{
	if (im == NULL || x >= im->sx || y >= im->sy)
		return (" ");
	return (&im->ascii[y * im->sx + x]);
}
