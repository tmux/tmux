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

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* Character-cell fallback for clients without a graphical image protocol. */

enum image_glyph_detail {
	IMAGE_GLYPH_ASCII,
	IMAGE_GLYPH_SHADE5,
	IMAGE_GLYPH_SHADE8,
	IMAGE_GLYPH_HALF,
	IMAGE_GLYPH_QUADRANT,
	IMAGE_GLYPH_SEXTANT
};

enum image_glyph_palette {
	IMAGE_GLYPH_PALETTE_8,
	IMAGE_GLYPH_PALETTE_16,
	IMAGE_GLYPH_PALETTE_256,
	IMAGE_GLYPH_PALETTE_RGB
};

struct image_rgb {
	u_char	r;
	u_char	g;
	u_char	b;
};

struct image_fallback_data {
	u_char	*shade5;
	u_char	*shade8;
};

static const struct image_rgb image_ansi_colours[16] = {
	{ 0x00, 0x00, 0x00 }, { 0x80, 0x00, 0x00 },
	{ 0x00, 0x80, 0x00 }, { 0x80, 0x80, 0x00 },
	{ 0x00, 0x00, 0x80 }, { 0x80, 0x00, 0x80 },
	{ 0x00, 0x80, 0x80 }, { 0xc0, 0xc0, 0xc0 },
	{ 0x80, 0x80, 0x80 }, { 0xff, 0x00, 0x00 },
	{ 0x00, 0xff, 0x00 }, { 0xff, 0xff, 0x00 },
	{ 0x00, 0x00, 0xff }, { 0xff, 0x00, 0xff },
	{ 0x00, 0xff, 0xff }, { 0xff, 0xff, 0xff }
};

static u_int
image_glyph_distance(struct image_rgb a, struct image_rgb b)
{
	int	r = a.r - b.r, g = a.g - b.g, bl = a.b - b.b;

	return (r * r + g * g + bl * bl);
}

static int
image_glyph_low_saturation(struct image_rgb colour)
{
	u_int	minimum, maximum;

	minimum = maximum = colour.r;
	if (colour.g < minimum)
		minimum = colour.g;
	if (colour.b < minimum)
		minimum = colour.b;
	if (colour.g > maximum)
		maximum = colour.g;
	if (colour.b > maximum)
		maximum = colour.b;
	return (maximum == 0 || (maximum - minimum) * 4 <= maximum);
}

static u_int
image_glyph_nearest_ansi(struct image_rgb colour, u_int colours)
{
	u_int	i, distance, best_distance = UINT_MAX, best = 0;
	int	neutral = image_glyph_low_saturation(colour);

	for (i = 0; i < colours; i++) {
		if (neutral && i != 0 && i != 7 &&
		    (colours != 16 || (i != 8 && i != 15)))
			continue;
		distance = image_glyph_distance(colour, image_ansi_colours[i]);
		if (distance < best_distance) {
			best_distance = distance;
			best = i;
		}
	}
	return (best);
}

static struct image_rgb
image_glyph_quantize(struct image_rgb colour, enum image_glyph_palette palette,
    int *output)
{
	struct image_rgb	 result;
	int		 value;
	u_int		 index;

	if (palette == IMAGE_GLYPH_PALETTE_RGB) {
		*output = colour_join_rgb(colour.r, colour.g, colour.b);
		return (colour);
	}
	if (palette == IMAGE_GLYPH_PALETTE_256) {
		*output = colour_find_rgb(colour.r, colour.g, colour.b);
		value = colour_256toRGB(*output);
		colour_split_rgb(value, &result.r, &result.g, &result.b);
		return (result);
	}
	index = image_glyph_nearest_ansi(colour,
	    palette == IMAGE_GLYPH_PALETTE_8 ? 8 : 16);
	result = image_ansi_colours[index];
	if (index < 8)
		*output = index;
	else
		*output = 90 + index - 8;
	return (result);
}

static void
image_glyph_fit_colours(const struct image_rgb *samples, u_int count,
    struct image_rgb centres[2])
{
	u_int	sum[2][3], counts[2], group, i, j, iteration, distance;
	u_int	maximum = 0, first = 0, second = 0;

	for (i = 0; i < count; i++) {
		for (j = i + 1; j < count; j++) {
			distance = image_glyph_distance(samples[i], samples[j]);
			if (distance > maximum) {
				maximum = distance;
				first = i;
				second = j;
			}
		}
	}
	centres[0] = samples[first];
	centres[1] = samples[second];
	for (iteration = 0; iteration < 4; iteration++) {
		memset(sum, 0, sizeof sum);
		memset(counts, 0, sizeof counts);
		for (i = 0; i < count; i++) {
			group = image_glyph_distance(samples[i], centres[1]) <
			    image_glyph_distance(samples[i], centres[0]);
			sum[group][0] += samples[i].r;
			sum[group][1] += samples[i].g;
			sum[group][2] += samples[i].b;
			counts[group]++;
		}
		for (i = 0; i < 2; i++) {
			if (counts[i] == 0)
				continue;
			centres[i].r = (sum[i][0] + counts[i] / 2) / counts[i];
			centres[i].g = (sum[i][1] + counts[i] / 2) / counts[i];
			centres[i].b = (sum[i][2] + counts[i] / 2) / counts[i];
		}
	}
}

static int
image_glyph_set_acs(struct tty *tty, struct utf8_data *data, u_char key)
{
	struct utf8_data	*ud;
	const char	*s = tty_acs_get(tty, key);

	if (s == NULL)
		return (0);
	ud = utf8_fromcstr(s);
	if (ud[0].size == 0) {
		free(ud);
		return (0);
	}
	memcpy(data, &ud[0], sizeof *data);
	free(ud);
	return (1);
}

static u_char
image_glyph_block_key(enum image_glyph_detail detail, u_int mask)
{
	static const u_char half[4] = {
		0, TTY_ACS_IMAGE_HALF_UPPER, TTY_ACS_IMAGE_HALF_LOWER,
		TTY_ACS_IMAGE_BLOCK
	};
	static const u_char quadrant[16] = {
		0, TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT,
		TTY_ACS_IMAGE_QUADRANT_UPPER_RIGHT, TTY_ACS_IMAGE_HALF_UPPER,
		TTY_ACS_IMAGE_QUADRANT_LOWER_LEFT, TTY_ACS_IMAGE_HALF_LEFT,
		TTY_ACS_IMAGE_QUADRANT_UPPER_RIGHT_LOWER_LEFT,
		TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT_UPPER_RIGHT_LOWER_LEFT,
		TTY_ACS_IMAGE_QUADRANT_LOWER_RIGHT,
		TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT_LOWER_RIGHT,
		TTY_ACS_IMAGE_HALF_RIGHT,
		TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT_UPPER_RIGHT_LOWER_RIGHT,
		TTY_ACS_IMAGE_HALF_LOWER,
		TTY_ACS_IMAGE_QUADRANT_UPPER_LEFT_LOWER_LEFT_LOWER_RIGHT,
		TTY_ACS_IMAGE_QUADRANT_UPPER_RIGHT_LOWER_LEFT_LOWER_RIGHT,
		TTY_ACS_IMAGE_BLOCK
	};

	if (detail == IMAGE_GLYPH_HALF)
		return (half[mask]);
	if (detail == IMAGE_GLYPH_QUADRANT)
		return (quadrant[mask]);
	if (mask == 0)
		return (0);
	if (mask == 21)
		return (TTY_ACS_IMAGE_HALF_LEFT);
	if (mask == 42)
		return (TTY_ACS_IMAGE_HALF_RIGHT);
	if (mask == 63)
		return (TTY_ACS_IMAGE_BLOCK);
	return (tty_acs_image_sextant(mask));
}

static u_char *
image_glyph_make_shades(struct image *im, u_int levels)
{
	struct image_fallback_data *data;
	int			*values, value, represented, error;
	size_t			 cells, index;
	u_int			 x, y, scan, level, minimum = 255, maximum = 0;
	u_int			 sx, sy;
	int			 reverse;
	u_char			*result;

	data = image_get_fallback_data(im);
	if (data == NULL) {
		data = xcalloc(1, sizeof *data);
		image_set_fallback_data(im, data);
	}
	result = (levels == 5 ? data->shade5 : data->shade8);
	if (result != NULL)
		return (result);
	image_get_cell_dimensions(im, &sx, &sy);
	cells = (size_t)sx * sy;
	values = xcalloc(cells, sizeof *values);
	result = xcalloc(cells, 1);
	for (y = 0; y < sy; y++) {
		for (x = 0; x < sx; x++) {
			index = (size_t)y * sx + x;
			value = image_get_brightness(im, x, y);
			if ((u_int)value < minimum)
				minimum = value;
			if ((u_int)value > maximum)
				maximum = value;
			values[index] = value;
		}
	}
	if (maximum > minimum) {
		for (index = 0; index < cells; index++)
			values[index] = (values[index] - minimum) * 255 /
			    (maximum - minimum);
	}
	for (y = 0; y < sy; y++) {
		reverse = (y & 1);
		for (scan = 0; scan < sx; scan++) {
			x = (reverse ? sx - scan - 1 : scan);
			index = (size_t)y * sx + x;
			value = values[index];
			if (value < 0)
				value = 0;
			if (value > 255)
				value = 255;
			level = (value * (levels - 1) + 127) / 255;
			result[index] = level;
			represented = level * 255 / (levels - 1);
			error = value - represented;
			if (!reverse && x + 1 < sx)
				values[index + 1] += error * 7 / 16;
			if (reverse && x != 0)
				values[index - 1] += error * 7 / 16;
			if (y + 1 == sy)
				continue;
			if (!reverse && x != 0)
				values[index + sx - 1] += error * 3 / 16;
			if (reverse && x + 1 < sx)
				values[index + sx + 1] += error * 3 / 16;
			values[index + sx] += error * 5 / 16;
			if (!reverse && x + 1 < sx)
				values[index + sx + 1] += error / 16;
			if (reverse && x != 0)
				values[index + sx - 1] += error / 16;
		}
	}
	free(values);
	if (levels == 5)
		data->shade5 = result;
	else
		data->shade8 = result;
	return (result);
}

static enum image_glyph_palette
image_glyph_get_palette(struct tty *tty)
{
	int colours;

	if (tty->term->flags & TERM_RGBCOLOURS)
		return (IMAGE_GLYPH_PALETTE_RGB);
	if (tty->term->flags & TERM_256COLOURS)
		return (IMAGE_GLYPH_PALETTE_256);
	colours = tty_term_number(tty->term, TTYC_COLORS);
	if (colours >= 256)
		return (IMAGE_GLYPH_PALETTE_256);
	if (colours >= 16)
		return (IMAGE_GLYPH_PALETTE_16);
	return (IMAGE_GLYPH_PALETTE_8);
}

static enum image_glyph_detail
image_glyph_get_detail(struct tty *tty, enum image_glyph_palette palette)
{
	if (tty_acs_needed(tty))
		return (IMAGE_GLYPH_ASCII);
	if (tty->term->flags & TERM_IMAGE_SEXTANTS)
		return (IMAGE_GLYPH_SEXTANT);
	if (tty->term->flags & TERM_IMAGE_QUADRANTS)
		return (IMAGE_GLYPH_QUADRANT);
	if (palette != IMAGE_GLYPH_PALETTE_8)
		return (IMAGE_GLYPH_HALF);
	if (tty_term_has(tty->term, TTYC_NOBR))
		return (IMAGE_GLYPH_SHADE5);
	return (IMAGE_GLYPH_SHADE8);
}

static void
image_glyph_block(struct tty *tty, struct image *im, u_int x, u_int y,
    enum image_glyph_detail detail, enum image_glyph_palette palette,
    struct grid_cell *out)
{
	struct image_rgb	 samples[6], centres[2], quantized[2];
	u_int			 columns, rows, sx, sy, i, n, mask;
	int			 colours[2];
	u_char			 key;

	columns = (detail == IMAGE_GLYPH_HALF ? 1 : 2);
	rows = (detail == IMAGE_GLYPH_HALF ? 2 :
	    detail == IMAGE_GLYPH_QUADRANT ? 2 : 3);
	i = 0;
	for (sy = 0; sy < rows; sy++) {
		for (sx = 0; sx < columns; sx++) {
			image_get_cell_average(im, x, y, sx, sy, columns, rows,
			    &samples[i].r, &samples[i].g, &samples[i].b);
			i++;
		}
	}
	n = columns * rows;
	image_glyph_fit_colours(samples, n, centres);
	quantized[0] = image_glyph_quantize(centres[0], palette, &colours[0]);
	quantized[1] = image_glyph_quantize(centres[1], palette, &colours[1]);
	mask = 0;
	for (i = 0; i < n; i++) {
		if (image_glyph_distance(samples[i], quantized[1]) <
		    image_glyph_distance(samples[i], quantized[0]))
			mask |= (1U << i);
	}
	key = image_glyph_block_key(detail, mask);
	if (key == 0)
		utf8_set(&out->data, ' ');
	else if (!image_glyph_set_acs(tty, &out->data, key))
		utf8_set(&out->data, ' ');
	out->fg = colours[1];
	out->bg = colours[0];
}

void
image_get_fallback_cell(struct tty *tty, struct image *im, u_int x, u_int y,
    const struct grid_cell *gc, struct grid_cell *out,
    __unused const struct tty_style_ctx *style_ctx)
{
	static const char	 ascii[] = " .:-=+*#%@";
	static const u_char	 shades[5] = { 0, TTY_ACS_IMAGE_SHADE_LIGHT,
		TTY_ACS_IMAGE_SHADE_MEDIUM, TTY_ACS_IMAGE_SHADE_DARK,
		TTY_ACS_IMAGE_BLOCK };
	static const u_char	 bold_shades[8] = { 0,
		TTY_ACS_IMAGE_SHADE_LIGHT, TTY_ACS_IMAGE_SHADE_LIGHT,
		TTY_ACS_IMAGE_SHADE_MEDIUM, TTY_ACS_IMAGE_SHADE_MEDIUM,
		TTY_ACS_IMAGE_SHADE_DARK, TTY_ACS_IMAGE_BLOCK,
		TTY_ACS_IMAGE_BLOCK };
	enum image_glyph_palette	 palette;
	enum image_glyph_detail	 detail;
	u_char			*levels, key;
	u_int			 level = 0, sx;

	memcpy(out, gc, sizeof *out);
	out->flags &= ~GRID_FLAG_IMAGE;
	palette = image_glyph_get_palette(tty);
	detail = image_glyph_get_detail(tty, palette);
	if (detail == IMAGE_GLYPH_ASCII) {
		level = image_get_brightness(im, x, y) * (sizeof ascii - 2) / 255;
		utf8_set(&out->data, ascii[level]);
		return;
	}
	if (detail == IMAGE_GLYPH_SHADE5 || detail == IMAGE_GLYPH_SHADE8) {
		levels = image_glyph_make_shades(im,
		    detail == IMAGE_GLYPH_SHADE5 ? 5 : 8);
		image_get_cell_dimensions(im, &sx, NULL);
		level = levels[(size_t)y * sx + x];
		key = (detail == IMAGE_GLYPH_SHADE5 ? shades[level] :
		    bold_shades[level]);
		if (key == 0)
			utf8_set(&out->data, ' ');
		else if (!image_glyph_set_acs(tty, &out->data, key))
			utf8_set(&out->data, ' ');
		out->fg = 7;
		out->bg = 0;
		out->attr &= ~GRID_ATTR_BRIGHT;
		if (detail == IMAGE_GLYPH_SHADE8 &&
		    (level == 2 || level == 4 || level == 7))
			out->attr |= GRID_ATTR_BRIGHT;
		return;
	}
	image_glyph_block(tty, im, x, y, detail, palette, out);
}

void
image_free_fallback(struct image *im)
{
	struct image_fallback_data *data;

	data = image_get_fallback_data(im);
	if (data == NULL)
		return;
	free(data->shade5);
	free(data->shade8);
	free(data);
}
