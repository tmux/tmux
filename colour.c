/* $Id$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Colour to string conversion functions. Bit 8 of the colour means it is one
 * of the 256 colour palette.
 */

/* An RGB colour. */
struct colour_rgb {
	u_char	r;
	u_char	g;
	u_char	b;
};

/* 256 colour RGB table, generated on first use. */
struct colour_rgb *colour_rgb_256;

void	colour_rgb_generate256(void);
u_int	colour_rgb_distance(struct colour_rgb *, struct colour_rgb *);
int	colour_rgb_find(struct colour_rgb *);

/* Generate 256 colour RGB table. */
void
colour_rgb_generate256(void)
{
	struct colour_rgb	*rgb;
	u_int			 i, r, g, b;

	/*
	 * Allocate the table. The first 16 colours are often changed by users
	 * and terminals so don't include them.
	 */
	colour_rgb_256 = xcalloc(240, sizeof *colour_rgb_256);

	/* Add the colours first. */
	r = g = b = 0;
	for (i = 240; i > 24; i--) {
		rgb = &colour_rgb_256[240 - i];

		if (r != 0)
			rgb->r = (r * 40) + 55;
		if (g != 0)
			rgb->g = (g * 40) + 55;
		if (b != 0)
			rgb->b = (b * 40) + 55;

		b++;
		if (b > 5) {
			b = 0;
			g++;
		}
		if (g > 5) {
			g = 0;
			r++;
		}
	}

	/* Then add the greys. */
	for (i = 24; i > 0; i--) {
		rgb = &colour_rgb_256[240 - i];

		rgb->r = 8 + (24 - i) * 10;
		rgb->g = 8 + (24 - i) * 10;
		rgb->b = 8 + (24 - i) * 10;
	}
}

/* Get colour RGB distance. */
u_int
colour_rgb_distance(struct colour_rgb *rgb1, struct colour_rgb *rgb2)
{
	int	r, g, b;

	r = rgb1->r - rgb2->r;
	g = rgb1->g - rgb2->g;
	b = rgb1->b - rgb2->b;
	return (r * r + g * g + b * b);
}

/* Work out the nearest colour from the 256 colour set. */
int
colour_rgb_find(struct colour_rgb *rgb)
{
	u_int	distance, lowest, colour, i;

	if (colour_rgb_256 == NULL)
		colour_rgb_generate256();

	colour = 16;
	lowest = UINT_MAX;
	for (i = 0; i < 240; i++) {
		distance = colour_rgb_distance(&colour_rgb_256[i], rgb);
		if (distance < lowest) {
			lowest = distance;
			colour = 16 + i;
		}
	}
	return (colour);
}

/* Set grid cell foreground colour. */
void
colour_set_fg(struct grid_cell *gc, int c)
{
	if (c & 0x100)
		gc->flags |= GRID_FLAG_FG256;
	gc->fg = c;
}

/* Set grid cell background colour. */
void
colour_set_bg(struct grid_cell *gc, int c)
{
	if (c & 0x100)
		gc->flags |= GRID_FLAG_BG256;
	gc->bg = c;
}

/* Convert colour to a string. */
const char *
colour_tostring(int c)
{
	static char	s[32];

	if (c & 0x100) {
		xsnprintf(s, sizeof s, "colour%u", c & ~0x100);
		return (s);
	}

	switch (c) {
	case 0:
		return ("black");
	case 1:
		return ("red");
	case 2:
		return ("green");
	case 3:
		return ("yellow");
	case 4:
		return ("blue");
	case 5:
		return ("magenta");
	case 6:
		return ("cyan");
	case 7:
		return ("white");
	case 8:
		return ("default");
	case 90:
		return ("brightblack");
	case 91:
		return ("brightred");
	case 92:
		return ("brightgreen");
	case 93:
		return ("brightyellow");
	case 94:
		return ("brightblue");
	case 95:
		return ("brightmagenta");
	case 96:
		return ("brightcyan");
	case 97:
		return ("brightwhite");
	}
	return (NULL);
}

/* Convert colour from string. */
int
colour_fromstring(const char *s)
{
	const char		*errstr;
	const char		*cp;
	struct colour_rgb	 rgb;
	int			 n;

	if (*s == '#' && strlen(s) == 7) {
		for (cp = s + 1; isxdigit((u_char) *cp); cp++)
			;
		if (*cp != '\0')
			return (-1);
		n = sscanf(s + 1, "%2hhx%2hhx%2hhx", &rgb.r, &rgb.g, &rgb.b);
		if (n != 3)
			return (-1);
		return (colour_rgb_find(&rgb) | 0x100);
	}

	if (strncasecmp(s, "colour", (sizeof "colour") - 1) == 0) {
		n = strtonum(s + (sizeof "colour") - 1, 0, 255, &errstr);
		if (errstr != NULL)
			return (-1);
		return (n | 0x100);
	}

	if (strcasecmp(s, "black") == 0 || (s[0] == '0' && s[1] == '\0'))
		return (0);
	if (strcasecmp(s, "red") == 0 || (s[0] == '1' && s[1] == '\0'))
		return (1);
	if (strcasecmp(s, "green") == 0 || (s[0] == '2' && s[1] == '\0'))
		return (2);
	if (strcasecmp(s, "yellow") == 0 || (s[0] == '3' && s[1] == '\0'))
		return (3);
	if (strcasecmp(s, "blue") == 0 || (s[0] == '4' && s[1] == '\0'))
		return (4);
	if (strcasecmp(s, "magenta") == 0 || (s[0] == '5' && s[1] == '\0'))
		return (5);
	if (strcasecmp(s, "cyan") == 0 || (s[0] == '6' && s[1] == '\0'))
		return (6);
	if (strcasecmp(s, "white") == 0 || (s[0] == '7' && s[1] == '\0'))
		return (7);
	if (strcasecmp(s, "default") == 0 || (s[0] == '8' && s[1] == '\0'))
		return (8);
	if (strcasecmp(s, "brightblack") == 0 ||
	    (s[0] == '9' && s[1] == '0' && s[1] == '\0'))
		return (90);
	if (strcasecmp(s, "brightred") == 0 ||
	    (s[0] == '9' && s[1] == '1' && s[1] == '\0'))
		return (91);
	if (strcasecmp(s, "brightgreen") == 0 ||
	    (s[0] == '9' && s[1] == '2' && s[1] == '\0'))
		return (92);
	if (strcasecmp(s, "brightyellow") == 0 ||
	    (s[0] == '9' && s[1] == '3' && s[1] == '\0'))
		return (93);
	if (strcasecmp(s, "brightblue") == 0 ||
	    (s[0] == '9' && s[1] == '4' && s[1] == '\0'))
		return (94);
	if (strcasecmp(s, "brightmagenta") == 0 ||
	    (s[0] == '9' && s[1] == '5' && s[1] == '\0'))
		return (95);
	if (strcasecmp(s, "brightcyan") == 0 ||
	    (s[0] == '9' && s[1] == '6' && s[1] == '\0'))
		return (96);
	if (strcasecmp(s, "brightwhite") == 0 ||
	    (s[0] == '9' && s[1] == '7' && s[1] == '\0'))
		return (97);
	return (-1);
}

/* Convert 256 colour palette to 16. */
u_char
colour_256to16(u_char c)
{
	static const u_char table[256] = {
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		 0,  4,  4,  4, 12, 12,  2,  6,  4,  4, 12, 12,  2,  2,  6,  4,
		12, 12,  2,  2,  2,  6, 12, 12, 10, 10, 10, 10, 14, 12, 10, 10,
		10, 10, 10, 14,  1,  5,  4,  4, 12, 12,  3,  8,  4,  4, 12, 12,
		 2,  2,  6,  4, 12, 12,  2,  2,  2,  6, 12, 12, 10, 10, 10, 10,
		14, 12, 10, 10, 10, 10, 10, 14,  1,  1,  5,  4, 12, 12,  1,  1,
		 5,  4, 12, 12,  3,  3,  8,  4, 12, 12,  2,  2,  2,  6, 12, 12,
		10, 10, 10, 10, 14, 12, 10, 10, 10, 10, 10, 14,  1,  1,  1,  5,
		12, 12,  1,  1,  1,  5, 12, 12,  1,  1,  1,  5, 12, 12,  3,  3,
		 3,  7, 12, 12, 10, 10, 10, 10, 14, 12, 10, 10, 10, 10, 10, 14,
		 9,  9,  9,  9, 13, 12,  9,  9,  9,  9, 13, 12,  9,  9,  9,  9,
		13, 12,  9,  9,  9,  9, 13, 12, 11, 11, 11, 11,  7, 12, 10, 10,
		10, 10, 10, 14,  9,  9,  9,  9,  9, 13,  9,  9,  9,  9,  9, 13,
		 9,  9,  9,  9,  9, 13,  9,  9,  9,  9,  9, 13,  9,  9,  9,  9,
		 9, 13, 11, 11, 11, 11, 11, 15,  0,  0,  0,  0,  0,  0,  8,  8,
		 8,  8,  8,  8,  7,  7,  7,  7,  7,  7, 15, 15, 15, 15, 15, 15
	};

	return (table[c]);
}

/* Convert 256 colour palette to 88. */
u_char
colour_256to88(u_char c)
{
	static const u_char table[256] = {
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
		16, 17, 17, 18, 18, 19, 20, 21, 21, 22, 22, 23, 20, 21, 21, 22,
		22, 23, 24, 25, 25, 26, 26, 27, 24, 25, 25, 26, 26, 27, 28, 29,
		29, 30, 30, 31, 32, 33, 33, 34, 34, 35, 36, 37, 37, 38, 38, 39,
		36, 37, 37, 38, 38, 39, 40, 41, 41, 42, 42, 43, 40, 41, 41, 42,
		42, 43, 44, 45, 45, 46, 46, 47, 32, 33, 33, 34, 34, 35, 36, 37,
		37, 38, 38, 39, 36, 37, 37, 38, 38, 39, 40, 41, 41, 42, 42, 43,
		40, 41, 41, 42, 42, 43, 44, 45, 45, 46, 46, 47, 48, 49, 49, 50,
		50, 51, 52, 53, 53, 54, 54, 55, 52, 53, 53, 54, 54, 55, 56, 57,
		57, 58, 58, 59, 56, 57, 57, 58, 58, 59, 60, 61, 61, 62, 62, 63,
		48, 49, 49, 50, 50, 51, 52, 53, 53, 54, 54, 55, 52, 53, 53, 54,
		54, 55, 56, 57, 57, 58, 58, 59, 56, 57, 57, 58, 58, 59, 60, 61,
		61, 62, 62, 63, 64, 65, 65, 66, 66, 67, 68, 69, 69, 70, 70, 71,
		68, 69, 69, 70, 70, 71, 72, 73, 73, 74, 74, 75, 72, 73, 73, 74,
		74, 75, 76, 77, 77, 78, 78, 79,  0,  0, 80, 80, 80, 81, 81, 81,
		82, 82, 82, 83, 83, 83, 84, 84, 84, 85, 85, 85, 86, 86, 86, 87
	};

	return (table[c]);
}
