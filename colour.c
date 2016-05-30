/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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


/**
 * Convert R/G/B triplet to xterm256 color value.
 *
 * Copyright (c) 2016 Avi Halachmi (:avih) <avihpit@yahoo.com>
 * Lisence: same as tmux, or any OSI-approved license
 *
 * Legends (with variable names etc):
 *   v - [0..255] channel value in rgb space
 *   q - [0..5] quantifier, representing an axis value in a 6x6x6 color cube
 *   c - color/channel
 *   g - gray (or green)
 *   color/gray index - [0..215]/[0..23] at the color/gray parts of xterm256
 *
 * This code incorrectly assumes sRGB is perceptually uniform - good enough.
 *
 * This code asumes that r/g/b are valid (0..255). u_char here makes them valid.
 * If that's not the case, crop them first, or else there's a potential out of
 * bounds access (q2v) or out of range return value.
 *
 * It's possible to get further ~x2 speedup with precalculated v2q (256 bytes)
 * and v2gi (768 bytes for r+g+b, or slightly slower 256 bytes for (r+g+b)/3),
 * with v2q contributing most of the speedup as it's used 3 times.
 * However, this would need the data cached already to have optimal impact and
 * would otherwise be slower. Also, it will take cache space which other code
 * could use. We're reasonably fast, so real time calculations is good enough.
 */

int
colour_find_rgb(u_char r, u_char g, u_char b)
{
	int color_dist, qr, qg, qb, cr, cg, cb, gray_dist, gray_index, gv;
#	define LC 1  /* 1: middle rounds down (Legacy Compatibility), else 0 */

#	define v2q(v) (v < 48 ? 0 : v < 114 ? 1 : (v - 35 - LC) / 40)
#	define color_index() (36 * qr + 6 * qg + qb)
	static const int q2v[6] = {0, 0x5f, 0x87, 0xaf, 0xd7, 0xff};
	qr = v2q(r);  qg = v2q(g);  qb = v2q(b);   /* 0..5 (6x6x6 cube) */
	cr = q2v[qr]; cg = q2v[qg]; cb = q2v[qb];  /* back to rgb values */
	if (cr == r && cg == g && cb == b)  /* usefulness depends on use case */
		return 16 + color_index();  /* rgb -> x256 -> rgb is the same */

#	define v2gi(v) (v > 238 ? 23 : (v - 3) / 10)
	gray_index = v2gi((r + g + b - LC) / 3);
	gv = 8 + 10 * gray_index;  /* back to rgb value (same gv for r/g/b) */
	/* not optimizing with early exit like above since it's yet x10 rarer */

#	define dist_sq(A,B,C, a,b,c) ((A-a)*(A-a) + (B-b)*(B-b) + (C-c)*(C-c))
	color_dist = dist_sq(cr, cg, cb, r, g, b);
	gray_dist  = dist_sq(gv, gv, gv, r, g, b);
	return gray_dist < color_dist ? 232 + gray_index : 16 + color_index();
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
		xsnprintf(s, sizeof s, "colour%d", c & ~0x100);
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
	const char	*errstr;
	const char	*cp;
	int		 n;
	u_char		 r, g, b;

	if (*s == '#' && strlen(s) == 7) {
		for (cp = s + 1; isxdigit((u_char) *cp); cp++)
			;
		if (*cp != '\0')
			return (-1);
		n = sscanf(s + 1, "%2hhx%2hhx%2hhx", &r, &g, &b);
		if (n != 3)
			return (-1);
		return (colour_find_rgb(r, g, b) | 0x100);
	}

	if (strncasecmp(s, "colour", (sizeof "colour") - 1) == 0) {
		n = strtonum(s + (sizeof "colour") - 1, 0, 255, &errstr);
		if (errstr != NULL)
			return (-1);
		return (n | 0x100);
	}

	if (strcasecmp(s, "black") == 0 || strcmp(s, "0") == 0)
		return (0);
	if (strcasecmp(s, "red") == 0 || strcmp(s, "1") == 0)
		return (1);
	if (strcasecmp(s, "green") == 0 || strcmp(s, "2") == 0)
		return (2);
	if (strcasecmp(s, "yellow") == 0 || strcmp(s, "3") == 0)
		return (3);
	if (strcasecmp(s, "blue") == 0 || strcmp(s, "4") == 0)
		return (4);
	if (strcasecmp(s, "magenta") == 0 || strcmp(s, "5") == 0)
		return (5);
	if (strcasecmp(s, "cyan") == 0 || strcmp(s, "6") == 0)
		return (6);
	if (strcasecmp(s, "white") == 0 || strcmp(s, "7") == 0)
		return (7);
	if (strcasecmp(s, "default") == 0 || strcmp(s, "8") == 0)
		return (8);
	if (strcasecmp(s, "brightblack") == 0 || strcmp(s, "90") == 0)
		return (90);
	if (strcasecmp(s, "brightred") == 0 || strcmp(s, "91") == 0)
		return (91);
	if (strcasecmp(s, "brightgreen") == 0 || strcmp(s, "92") == 0)
		return (92);
	if (strcasecmp(s, "brightyellow") == 0 || strcmp(s, "93") == 0)
		return (93);
	if (strcasecmp(s, "brightblue") == 0 || strcmp(s, "94") == 0)
		return (94);
	if (strcasecmp(s, "brightmagenta") == 0 || strcmp(s, "95") == 0)
		return (95);
	if (strcasecmp(s, "brightcyan") == 0 || strcmp(s, "96") == 0)
		return (96);
	if (strcasecmp(s, "brightwhite") == 0 || strcmp(s, "97") == 0)
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
