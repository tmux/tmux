/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2016 Avi Halachmi <avihpit@yahoo.com>
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
#include <math.h>

#include "tmux.h"

static int
colour_dist_sq(int R, int G, int B, int r, int g, int b)
{
	return ((R - r) * (R - r) + (G - g) * (G - g) + (B - b) * (B - b));
}

static int
colour_to_6cube(int v)
{
	if (v < 48)
		return (0);
	if (v < 114)
		return (1);
	return ((v - 35) / 40);
}

/*
 * Convert an RGB triplet to the xterm(1) 256 colour palette.
 *
 * xterm provides a 6x6x6 colour cube (16 - 231) and 24 greys (232 - 255). We
 * map our RGB colour to the closest in the cube, also work out the closest
 * grey, and use the nearest of the two.
 *
 * Note that the xterm has much lower resolution for darker colours (they are
 * not evenly spread out), so our 6 levels are not evenly spread: 0x0, 0x5f
 * (95), 0x87 (135), 0xaf (175), 0xd7 (215) and 0xff (255). Greys are more
 * evenly spread (8, 18, 28 ... 238).
 */
int
colour_find_rgb(u_char r, u_char g, u_char b)
{
	static const int	q2c[6] = { 0x00, 0x5f, 0x87, 0xaf, 0xd7, 0xff };
	int			qr, qg, qb, cr, cg, cb, d, idx;
	int			grey_avg, grey_idx, grey;

	/* Map RGB to 6x6x6 cube. */
	qr = colour_to_6cube(r); cr = q2c[qr];
	qg = colour_to_6cube(g); cg = q2c[qg];
	qb = colour_to_6cube(b); cb = q2c[qb];

	/* If we have hit the colour exactly, return early. */
	if (cr == r && cg == g && cb == b)
		return ((16 + (36 * qr) + (6 * qg) + qb) | COLOUR_FLAG_256);

	/* Work out the closest grey (average of RGB). */
	grey_avg = (r + g + b) / 3;
	if (grey_avg > 238)
		grey_idx = 23;
	else
		grey_idx = (grey_avg - 3) / 10;
	grey = 8 + (10 * grey_idx);

	/* Is grey or 6x6x6 colour closest? */
	d = colour_dist_sq(cr, cg, cb, r, g, b);
	if (colour_dist_sq(grey, grey, grey, r, g, b) < d)
		idx = 232 + grey_idx;
	else
		idx = 16 + (36 * qr) + (6 * qg) + qb;
	return (idx | COLOUR_FLAG_256);
}

/* Join RGB into a colour. */
int
colour_join_rgb(u_char r, u_char g, u_char b)
{
	return ((((int)((r) & 0xff)) << 16) |
	    (((int)((g) & 0xff)) << 8) |
	    (((int)((b) & 0xff))) | COLOUR_FLAG_RGB);
}

/* Split colour into RGB. */
void
colour_split_rgb(int c, u_char *r, u_char *g, u_char *b)
{
	*r = (c >> 16) & 0xff;
	*g = (c >> 8) & 0xff;
	*b = c & 0xff;
}

/* Force colour to RGB if not already. */
int
colour_force_rgb(int c)
{
	if (c & COLOUR_FLAG_RGB)
		return (c);
	if (c & COLOUR_FLAG_256)
		return (colour_256toRGB(c));
	if (c >= 0 && c <= 7)
		return (colour_256toRGB(c));
	if (c >= 90 && c <= 97)
		return (colour_256toRGB(8 + c - 90));
	return (-1);
}

/* Convert colour to a string. */
const char *
colour_tostring(int c)
{
	static char	s[32];
	u_char		r, g, b;

	if (c == -1)
		return ("none");

	if (c & COLOUR_FLAG_RGB) {
		colour_split_rgb(c, &r, &g, &b);
		xsnprintf(s, sizeof s, "#%02x%02x%02x", r, g, b);
		return (s);
	}

	if (c & COLOUR_FLAG_256) {
		xsnprintf(s, sizeof s, "colour%u", c & 0xff);
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
	case 9:
		return ("terminal");
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
	return ("invalid");
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
		return (colour_join_rgb(r, g, b));
	}

	if (strncasecmp(s, "colour", (sizeof "colour") - 1) == 0) {
		n = strtonum(s + (sizeof "colour") - 1, 0, 255, &errstr);
		if (errstr != NULL)
			return (-1);
		return (n | COLOUR_FLAG_256);
	}
	if (strncasecmp(s, "color", (sizeof "color") - 1) == 0) {
		n = strtonum(s + (sizeof "color") - 1, 0, 255, &errstr);
		if (errstr != NULL)
			return (-1);
		return (n | COLOUR_FLAG_256);
	}

	if (strcasecmp(s, "default") == 0)
		return (8);
	if (strcasecmp(s, "terminal") == 0)
		return (9);

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
	return (colour_byname(s));
}

/* Convert 256 colour to RGB colour. */
int
colour_256toRGB(int c)
{
	static const int table[256] = {
		0x000000, 0x800000, 0x008000, 0x808000,
		0x000080, 0x800080, 0x008080, 0xc0c0c0,
		0x808080, 0xff0000, 0x00ff00, 0xffff00,
		0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
		0x000000, 0x00005f, 0x000087, 0x0000af,
		0x0000d7, 0x0000ff, 0x005f00, 0x005f5f,
		0x005f87, 0x005faf, 0x005fd7, 0x005fff,
		0x008700, 0x00875f, 0x008787, 0x0087af,
		0x0087d7, 0x0087ff, 0x00af00, 0x00af5f,
		0x00af87, 0x00afaf, 0x00afd7, 0x00afff,
		0x00d700, 0x00d75f, 0x00d787, 0x00d7af,
		0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f,
		0x00ff87, 0x00ffaf, 0x00ffd7, 0x00ffff,
		0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af,
		0x5f00d7, 0x5f00ff, 0x5f5f00, 0x5f5f5f,
		0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff,
		0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af,
		0x5f87d7, 0x5f87ff, 0x5faf00, 0x5faf5f,
		0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff,
		0x5fd700, 0x5fd75f, 0x5fd787, 0x5fd7af,
		0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f,
		0x5fff87, 0x5fffaf, 0x5fffd7, 0x5fffff,
		0x870000, 0x87005f, 0x870087, 0x8700af,
		0x8700d7, 0x8700ff, 0x875f00, 0x875f5f,
		0x875f87, 0x875faf, 0x875fd7, 0x875fff,
		0x878700, 0x87875f, 0x878787, 0x8787af,
		0x8787d7, 0x8787ff, 0x87af00, 0x87af5f,
		0x87af87, 0x87afaf, 0x87afd7, 0x87afff,
		0x87d700, 0x87d75f, 0x87d787, 0x87d7af,
		0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f,
		0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff,
		0xaf0000, 0xaf005f, 0xaf0087, 0xaf00af,
		0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f,
		0xaf5f87, 0xaf5faf, 0xaf5fd7, 0xaf5fff,
		0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af,
		0xaf87d7, 0xaf87ff, 0xafaf00, 0xafaf5f,
		0xafaf87, 0xafafaf, 0xafafd7, 0xafafff,
		0xafd700, 0xafd75f, 0xafd787, 0xafd7af,
		0xafd7d7, 0xafd7ff, 0xafff00, 0xafff5f,
		0xafff87, 0xafffaf, 0xafffd7, 0xafffff,
		0xd70000, 0xd7005f, 0xd70087, 0xd700af,
		0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f,
		0xd75f87, 0xd75faf, 0xd75fd7, 0xd75fff,
		0xd78700, 0xd7875f, 0xd78787, 0xd787af,
		0xd787d7, 0xd787ff, 0xd7af00, 0xd7af5f,
		0xd7af87, 0xd7afaf, 0xd7afd7, 0xd7afff,
		0xd7d700, 0xd7d75f, 0xd7d787, 0xd7d7af,
		0xd7d7d7, 0xd7d7ff, 0xd7ff00, 0xd7ff5f,
		0xd7ff87, 0xd7ffaf, 0xd7ffd7, 0xd7ffff,
		0xff0000, 0xff005f, 0xff0087, 0xff00af,
		0xff00d7, 0xff00ff, 0xff5f00, 0xff5f5f,
		0xff5f87, 0xff5faf, 0xff5fd7, 0xff5fff,
		0xff8700, 0xff875f, 0xff8787, 0xff87af,
		0xff87d7, 0xff87ff, 0xffaf00, 0xffaf5f,
		0xffaf87, 0xffafaf, 0xffafd7, 0xffafff,
		0xffd700, 0xffd75f, 0xffd787, 0xffd7af,
		0xffd7d7, 0xffd7ff, 0xffff00, 0xffff5f,
		0xffff87, 0xffffaf, 0xffffd7, 0xffffff,
		0x080808, 0x121212, 0x1c1c1c, 0x262626,
		0x303030, 0x3a3a3a, 0x444444, 0x4e4e4e,
		0x585858, 0x626262, 0x6c6c6c, 0x767676,
		0x808080, 0x8a8a8a, 0x949494, 0x9e9e9e,
		0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6,
		0xd0d0d0, 0xdadada, 0xe4e4e4, 0xeeeeee
	};

	return (table[c & 0xff] | COLOUR_FLAG_RGB);
}

/* Convert 256 colour to 16 colour. */
int
colour_256to16(int c)
{
	static const char table[256] = {
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

	return (table[c & 0xff]);
}

/* Get colour by X11 colour name. */
int
colour_byname(const char *name)
{
	static const struct {
		const char	*name;
		int		 c;
	} colours[] = {
		{ "AliceBlue", 0xf0f8ff },
		{ "AntiqueWhite", 0xfaebd7 },
		{ "AntiqueWhite1", 0xffefdb },
		{ "AntiqueWhite2", 0xeedfcc },
		{ "AntiqueWhite3", 0xcdc0b0 },
		{ "AntiqueWhite4", 0x8b8378 },
		{ "BlanchedAlmond", 0xffebcd },
		{ "BlueViolet", 0x8a2be2 },
		{ "CadetBlue", 0x5f9ea0 },
		{ "CadetBlue1", 0x98f5ff },
		{ "CadetBlue2", 0x8ee5ee },
		{ "CadetBlue3", 0x7ac5cd },
		{ "CadetBlue4", 0x53868b },
		{ "CornflowerBlue", 0x6495ed },
		{ "DarkBlue", 0x00008b },
		{ "DarkCyan", 0x008b8b },
		{ "DarkGoldenrod", 0xb8860b },
		{ "DarkGoldenrod1", 0xffb90f },
		{ "DarkGoldenrod2", 0xeead0e },
		{ "DarkGoldenrod3", 0xcd950c },
		{ "DarkGoldenrod4", 0x8b6508 },
		{ "DarkGray", 0xa9a9a9 },
		{ "DarkGreen", 0x006400 },
		{ "DarkGrey", 0xa9a9a9 },
		{ "DarkKhaki", 0xbdb76b },
		{ "DarkMagenta", 0x8b008b },
		{ "DarkOliveGreen", 0x556b2f },
		{ "DarkOliveGreen1", 0xcaff70 },
		{ "DarkOliveGreen2", 0xbcee68 },
		{ "DarkOliveGreen3", 0xa2cd5a },
		{ "DarkOliveGreen4", 0x6e8b3d },
		{ "DarkOrange", 0xff8c00 },
		{ "DarkOrange1", 0xff7f00 },
		{ "DarkOrange2", 0xee7600 },
		{ "DarkOrange3", 0xcd6600 },
		{ "DarkOrange4", 0x8b4500 },
		{ "DarkOrchid", 0x9932cc },
		{ "DarkOrchid1", 0xbf3eff },
		{ "DarkOrchid2", 0xb23aee },
		{ "DarkOrchid3", 0x9a32cd },
		{ "DarkOrchid4", 0x68228b },
		{ "DarkRed", 0x8b0000 },
		{ "DarkSalmon", 0xe9967a },
		{ "DarkSeaGreen", 0x8fbc8f },
		{ "DarkSeaGreen1", 0xc1ffc1 },
		{ "DarkSeaGreen2", 0xb4eeb4 },
		{ "DarkSeaGreen3", 0x9bcd9b },
		{ "DarkSeaGreen4", 0x698b69 },
		{ "DarkSlateBlue", 0x483d8b },
		{ "DarkSlateGray", 0x2f4f4f },
		{ "DarkSlateGray1", 0x97ffff },
		{ "DarkSlateGray2", 0x8deeee },
		{ "DarkSlateGray3", 0x79cdcd },
		{ "DarkSlateGray4", 0x528b8b },
		{ "DarkSlateGrey", 0x2f4f4f },
		{ "DarkTurquoise", 0x00ced1 },
		{ "DarkViolet", 0x9400d3 },
		{ "DeepPink", 0xff1493 },
		{ "DeepPink1", 0xff1493 },
		{ "DeepPink2", 0xee1289 },
		{ "DeepPink3", 0xcd1076 },
		{ "DeepPink4", 0x8b0a50 },
		{ "DeepSkyBlue", 0x00bfff },
		{ "DeepSkyBlue1", 0x00bfff },
		{ "DeepSkyBlue2", 0x00b2ee },
		{ "DeepSkyBlue3", 0x009acd },
		{ "DeepSkyBlue4", 0x00688b },
		{ "DimGray", 0x696969 },
		{ "DimGrey", 0x696969 },
		{ "DodgerBlue", 0x1e90ff },
		{ "DodgerBlue1", 0x1e90ff },
		{ "DodgerBlue2", 0x1c86ee },
		{ "DodgerBlue3", 0x1874cd },
		{ "DodgerBlue4", 0x104e8b },
		{ "FloralWhite", 0xfffaf0 },
		{ "ForestGreen", 0x228b22 },
		{ "GhostWhite", 0xf8f8ff },
		{ "GreenYellow", 0xadff2f },
		{ "HotPink", 0xff69b4 },
		{ "HotPink1", 0xff6eb4 },
		{ "HotPink2", 0xee6aa7 },
		{ "HotPink3", 0xcd6090 },
		{ "HotPink4", 0x8b3a62 },
		{ "IndianRed", 0xcd5c5c },
		{ "IndianRed1", 0xff6a6a },
		{ "IndianRed2", 0xee6363 },
		{ "IndianRed3", 0xcd5555 },
		{ "IndianRed4", 0x8b3a3a },
		{ "LavenderBlush", 0xfff0f5 },
		{ "LavenderBlush1", 0xfff0f5 },
		{ "LavenderBlush2", 0xeee0e5 },
		{ "LavenderBlush3", 0xcdc1c5 },
		{ "LavenderBlush4", 0x8b8386 },
		{ "LawnGreen", 0x7cfc00 },
		{ "LemonChiffon", 0xfffacd },
		{ "LemonChiffon1", 0xfffacd },
		{ "LemonChiffon2", 0xeee9bf },
		{ "LemonChiffon3", 0xcdc9a5 },
		{ "LemonChiffon4", 0x8b8970 },
		{ "LightBlue", 0xadd8e6 },
		{ "LightBlue1", 0xbfefff },
		{ "LightBlue2", 0xb2dfee },
		{ "LightBlue3", 0x9ac0cd },
		{ "LightBlue4", 0x68838b },
		{ "LightCoral", 0xf08080 },
		{ "LightCyan", 0xe0ffff },
		{ "LightCyan1", 0xe0ffff },
		{ "LightCyan2", 0xd1eeee },
		{ "LightCyan3", 0xb4cdcd },
		{ "LightCyan4", 0x7a8b8b },
		{ "LightGoldenrod", 0xeedd82 },
		{ "LightGoldenrod1", 0xffec8b },
		{ "LightGoldenrod2", 0xeedc82 },
		{ "LightGoldenrod3", 0xcdbe70 },
		{ "LightGoldenrod4", 0x8b814c },
		{ "LightGoldenrodYellow", 0xfafad2 },
		{ "LightGray", 0xd3d3d3 },
		{ "LightGreen", 0x90ee90 },
		{ "LightGrey", 0xd3d3d3 },
		{ "LightPink", 0xffb6c1 },
		{ "LightPink1", 0xffaeb9 },
		{ "LightPink2", 0xeea2ad },
		{ "LightPink3", 0xcd8c95 },
		{ "LightPink4", 0x8b5f65 },
		{ "LightSalmon", 0xffa07a },
		{ "LightSalmon1", 0xffa07a },
		{ "LightSalmon2", 0xee9572 },
		{ "LightSalmon3", 0xcd8162 },
		{ "LightSalmon4", 0x8b5742 },
		{ "LightSeaGreen", 0x20b2aa },
		{ "LightSkyBlue", 0x87cefa },
		{ "LightSkyBlue1", 0xb0e2ff },
		{ "LightSkyBlue2", 0xa4d3ee },
		{ "LightSkyBlue3", 0x8db6cd },
		{ "LightSkyBlue4", 0x607b8b },
		{ "LightSlateBlue", 0x8470ff },
		{ "LightSlateGray", 0x778899 },
		{ "LightSlateGrey", 0x778899 },
		{ "LightSteelBlue", 0xb0c4de },
		{ "LightSteelBlue1", 0xcae1ff },
		{ "LightSteelBlue2", 0xbcd2ee },
		{ "LightSteelBlue3", 0xa2b5cd },
		{ "LightSteelBlue4", 0x6e7b8b },
		{ "LightYellow", 0xffffe0 },
		{ "LightYellow1", 0xffffe0 },
		{ "LightYellow2", 0xeeeed1 },
		{ "LightYellow3", 0xcdcdb4 },
		{ "LightYellow4", 0x8b8b7a },
		{ "LimeGreen", 0x32cd32 },
		{ "MediumAquamarine", 0x66cdaa },
		{ "MediumBlue", 0x0000cd },
		{ "MediumOrchid", 0xba55d3 },
		{ "MediumOrchid1", 0xe066ff },
		{ "MediumOrchid2", 0xd15fee },
		{ "MediumOrchid3", 0xb452cd },
		{ "MediumOrchid4", 0x7a378b },
		{ "MediumPurple", 0x9370db },
		{ "MediumPurple1", 0xab82ff },
		{ "MediumPurple2", 0x9f79ee },
		{ "MediumPurple3", 0x8968cd },
		{ "MediumPurple4", 0x5d478b },
		{ "MediumSeaGreen", 0x3cb371 },
		{ "MediumSlateBlue", 0x7b68ee },
		{ "MediumSpringGreen", 0x00fa9a },
		{ "MediumTurquoise", 0x48d1cc },
		{ "MediumVioletRed", 0xc71585 },
		{ "MidnightBlue", 0x191970 },
		{ "MintCream", 0xf5fffa },
		{ "MistyRose", 0xffe4e1 },
		{ "MistyRose1", 0xffe4e1 },
		{ "MistyRose2", 0xeed5d2 },
		{ "MistyRose3", 0xcdb7b5 },
		{ "MistyRose4", 0x8b7d7b },
		{ "NavajoWhite", 0xffdead },
		{ "NavajoWhite1", 0xffdead },
		{ "NavajoWhite2", 0xeecfa1 },
		{ "NavajoWhite3", 0xcdb38b },
		{ "NavajoWhite4", 0x8b795e },
		{ "NavyBlue", 0x000080 },
		{ "OldLace", 0xfdf5e6 },
		{ "OliveDrab", 0x6b8e23 },
		{ "OliveDrab1", 0xc0ff3e },
		{ "OliveDrab2", 0xb3ee3a },
		{ "OliveDrab3", 0x9acd32 },
		{ "OliveDrab4", 0x698b22 },
		{ "OrangeRed", 0xff4500 },
		{ "OrangeRed1", 0xff4500 },
		{ "OrangeRed2", 0xee4000 },
		{ "OrangeRed3", 0xcd3700 },
		{ "OrangeRed4", 0x8b2500 },
		{ "PaleGoldenrod", 0xeee8aa },
		{ "PaleGreen", 0x98fb98 },
		{ "PaleGreen1", 0x9aff9a },
		{ "PaleGreen2", 0x90ee90 },
		{ "PaleGreen3", 0x7ccd7c },
		{ "PaleGreen4", 0x548b54 },
		{ "PaleTurquoise", 0xafeeee },
		{ "PaleTurquoise1", 0xbbffff },
		{ "PaleTurquoise2", 0xaeeeee },
		{ "PaleTurquoise3", 0x96cdcd },
		{ "PaleTurquoise4", 0x668b8b },
		{ "PaleVioletRed", 0xdb7093 },
		{ "PaleVioletRed1", 0xff82ab },
		{ "PaleVioletRed2", 0xee799f },
		{ "PaleVioletRed3", 0xcd6889 },
		{ "PaleVioletRed4", 0x8b475d },
		{ "PapayaWhip", 0xffefd5 },
		{ "PeachPuff", 0xffdab9 },
		{ "PeachPuff1", 0xffdab9 },
		{ "PeachPuff2", 0xeecbad },
		{ "PeachPuff3", 0xcdaf95 },
		{ "PeachPuff4", 0x8b7765 },
		{ "PowderBlue", 0xb0e0e6 },
		{ "RebeccaPurple", 0x663399 },
		{ "RosyBrown", 0xbc8f8f },
		{ "RosyBrown1", 0xffc1c1 },
		{ "RosyBrown2", 0xeeb4b4 },
		{ "RosyBrown3", 0xcd9b9b },
		{ "RosyBrown4", 0x8b6969 },
		{ "RoyalBlue", 0x4169e1 },
		{ "RoyalBlue1", 0x4876ff },
		{ "RoyalBlue2", 0x436eee },
		{ "RoyalBlue3", 0x3a5fcd },
		{ "RoyalBlue4", 0x27408b },
		{ "SaddleBrown", 0x8b4513 },
		{ "SandyBrown", 0xf4a460 },
		{ "SeaGreen", 0x2e8b57 },
		{ "SeaGreen1", 0x54ff9f },
		{ "SeaGreen2", 0x4eee94 },
		{ "SeaGreen3", 0x43cd80 },
		{ "SeaGreen4", 0x2e8b57 },
		{ "SkyBlue", 0x87ceeb },
		{ "SkyBlue1", 0x87ceff },
		{ "SkyBlue2", 0x7ec0ee },
		{ "SkyBlue3", 0x6ca6cd },
		{ "SkyBlue4", 0x4a708b },
		{ "SlateBlue", 0x6a5acd },
		{ "SlateBlue1", 0x836fff },
		{ "SlateBlue2", 0x7a67ee },
		{ "SlateBlue3", 0x6959cd },
		{ "SlateBlue4", 0x473c8b },
		{ "SlateGray", 0x708090 },
		{ "SlateGray1", 0xc6e2ff },
		{ "SlateGray2", 0xb9d3ee },
		{ "SlateGray3", 0x9fb6cd },
		{ "SlateGray4", 0x6c7b8b },
		{ "SlateGrey", 0x708090 },
		{ "SpringGreen", 0x00ff7f },
		{ "SpringGreen1", 0x00ff7f },
		{ "SpringGreen2", 0x00ee76 },
		{ "SpringGreen3", 0x00cd66 },
		{ "SpringGreen4", 0x008b45 },
		{ "SteelBlue", 0x4682b4 },
		{ "SteelBlue1", 0x63b8ff },
		{ "SteelBlue2", 0x5cacee },
		{ "SteelBlue3", 0x4f94cd },
		{ "SteelBlue4", 0x36648b },
		{ "VioletRed", 0xd02090 },
		{ "VioletRed1", 0xff3e96 },
		{ "VioletRed2", 0xee3a8c },
		{ "VioletRed3", 0xcd3278 },
		{ "VioletRed4", 0x8b2252 },
		{ "WebGray", 0x808080 },
		{ "WebGreen", 0x008000 },
		{ "WebGrey", 0x808080 },
		{ "WebMaroon", 0x800000 },
		{ "WebPurple", 0x800080 },
		{ "WhiteSmoke", 0xf5f5f5 },
		{ "X11Gray", 0xbebebe },
		{ "X11Green", 0x00ff00 },
		{ "X11Grey", 0xbebebe },
		{ "X11Maroon", 0xb03060 },
		{ "X11Purple", 0xa020f0 },
		{ "YellowGreen", 0x9acd32 },
		{ "alice blue", 0xf0f8ff },
		{ "antique white", 0xfaebd7 },
		{ "aqua", 0x00ffff },
		{ "aquamarine", 0x7fffd4 },
		{ "aquamarine1", 0x7fffd4 },
		{ "aquamarine2", 0x76eec6 },
		{ "aquamarine3", 0x66cdaa },
		{ "aquamarine4", 0x458b74 },
		{ "azure", 0xf0ffff },
		{ "azure1", 0xf0ffff },
		{ "azure2", 0xe0eeee },
		{ "azure3", 0xc1cdcd },
		{ "azure4", 0x838b8b },
		{ "beige", 0xf5f5dc },
		{ "bisque", 0xffe4c4 },
		{ "bisque1", 0xffe4c4 },
		{ "bisque2", 0xeed5b7 },
		{ "bisque3", 0xcdb79e },
		{ "bisque4", 0x8b7d6b },
		{ "black", 0x000000 },
		{ "blanched almond", 0xffebcd },
		{ "blue violet", 0x8a2be2 },
		{ "blue", 0x0000ff },
		{ "blue1", 0x0000ff },
		{ "blue2", 0x0000ee },
		{ "blue3", 0x0000cd },
		{ "blue4", 0x00008b },
		{ "brown", 0xa52a2a },
		{ "brown1", 0xff4040 },
		{ "brown2", 0xee3b3b },
		{ "brown3", 0xcd3333 },
		{ "brown4", 0x8b2323 },
		{ "burlywood", 0xdeb887 },
		{ "burlywood1", 0xffd39b },
		{ "burlywood2", 0xeec591 },
		{ "burlywood3", 0xcdaa7d },
		{ "burlywood4", 0x8b7355 },
		{ "cadet blue", 0x5f9ea0 },
		{ "chartreuse", 0x7fff00 },
		{ "chartreuse1", 0x7fff00 },
		{ "chartreuse2", 0x76ee00 },
		{ "chartreuse3", 0x66cd00 },
		{ "chartreuse4", 0x458b00 },
		{ "chocolate", 0xd2691e },
		{ "chocolate1", 0xff7f24 },
		{ "chocolate2", 0xee7621 },
		{ "chocolate3", 0xcd661d },
		{ "chocolate4", 0x8b4513 },
		{ "coral", 0xff7f50 },
		{ "coral1", 0xff7256 },
		{ "coral2", 0xee6a50 },
		{ "coral3", 0xcd5b45 },
		{ "coral4", 0x8b3e2f },
		{ "cornflower blue", 0x6495ed },
		{ "cornsilk", 0xfff8dc },
		{ "cornsilk1", 0xfff8dc },
		{ "cornsilk2", 0xeee8cd },
		{ "cornsilk3", 0xcdc8b1 },
		{ "cornsilk4", 0x8b8878 },
		{ "crimson", 0xdc143c },
		{ "cyan", 0x00ffff },
		{ "cyan1", 0x00ffff },
		{ "cyan2", 0x00eeee },
		{ "cyan3", 0x00cdcd },
		{ "cyan4", 0x008b8b },
		{ "dark blue", 0x00008b },
		{ "dark cyan", 0x008b8b },
		{ "dark goldenrod", 0xb8860b },
		{ "dark gray", 0xa9a9a9 },
		{ "dark green", 0x006400 },
		{ "dark grey", 0xa9a9a9 },
		{ "dark khaki", 0xbdb76b },
		{ "dark magenta", 0x8b008b },
		{ "dark olive green", 0x556b2f },
		{ "dark orange", 0xff8c00 },
		{ "dark orchid", 0x9932cc },
		{ "dark red", 0x8b0000 },
		{ "dark salmon", 0xe9967a },
		{ "dark sea green", 0x8fbc8f },
		{ "dark slate blue", 0x483d8b },
		{ "dark slate gray", 0x2f4f4f },
		{ "dark slate grey", 0x2f4f4f },
		{ "dark turquoise", 0x00ced1 },
		{ "dark violet", 0x9400d3 },
		{ "deep pink", 0xff1493 },
		{ "deep sky blue", 0x00bfff },
		{ "dim gray", 0x696969 },
		{ "dim grey", 0x696969 },
		{ "dodger blue", 0x1e90ff },
		{ "firebrick", 0xb22222 },
		{ "firebrick1", 0xff3030 },
		{ "firebrick2", 0xee2c2c },
		{ "firebrick3", 0xcd2626 },
		{ "firebrick4", 0x8b1a1a },
		{ "floral white", 0xfffaf0 },
		{ "forest green", 0x228b22 },
		{ "fuchsia", 0xff00ff },
		{ "gainsboro", 0xdcdcdc },
		{ "ghost white", 0xf8f8ff },
		{ "gold", 0xffd700 },
		{ "gold1", 0xffd700 },
		{ "gold2", 0xeec900 },
		{ "gold3", 0xcdad00 },
		{ "gold4", 0x8b7500 },
		{ "goldenrod", 0xdaa520 },
		{ "goldenrod1", 0xffc125 },
		{ "goldenrod2", 0xeeb422 },
		{ "goldenrod3", 0xcd9b1d },
		{ "goldenrod4", 0x8b6914 },
		{ "green yellow", 0xadff2f },
		{ "green", 0x00ff00 },
		{ "green1", 0x00ff00 },
		{ "green2", 0x00ee00 },
		{ "green3", 0x00cd00 },
		{ "green4", 0x008b00 },
		{ "honeydew", 0xf0fff0 },
		{ "honeydew1", 0xf0fff0 },
		{ "honeydew2", 0xe0eee0 },
		{ "honeydew3", 0xc1cdc1 },
		{ "honeydew4", 0x838b83 },
		{ "hot pink", 0xff69b4 },
		{ "indian red", 0xcd5c5c },
		{ "indigo", 0x4b0082 },
		{ "ivory", 0xfffff0 },
		{ "ivory1", 0xfffff0 },
		{ "ivory2", 0xeeeee0 },
		{ "ivory3", 0xcdcdc1 },
		{ "ivory4", 0x8b8b83 },
		{ "khaki", 0xf0e68c },
		{ "khaki1", 0xfff68f },
		{ "khaki2", 0xeee685 },
		{ "khaki3", 0xcdc673 },
		{ "khaki4", 0x8b864e },
		{ "lavender blush", 0xfff0f5 },
		{ "lavender", 0xe6e6fa },
		{ "lawn green", 0x7cfc00 },
		{ "lemon chiffon", 0xfffacd },
		{ "light blue", 0xadd8e6 },
		{ "light coral", 0xf08080 },
		{ "light cyan", 0xe0ffff },
		{ "light goldenrod yellow", 0xfafad2 },
		{ "light goldenrod", 0xeedd82 },
		{ "light gray", 0xd3d3d3 },
		{ "light green", 0x90ee90 },
		{ "light grey", 0xd3d3d3 },
		{ "light pink", 0xffb6c1 },
		{ "light salmon", 0xffa07a },
		{ "light sea green", 0x20b2aa },
		{ "light sky blue", 0x87cefa },
		{ "light slate blue", 0x8470ff },
		{ "light slate gray", 0x778899 },
		{ "light slate grey", 0x778899 },
		{ "light steel blue", 0xb0c4de },
		{ "light yellow", 0xffffe0 },
		{ "lime green", 0x32cd32 },
		{ "lime", 0x00ff00 },
		{ "linen", 0xfaf0e6 },
		{ "magenta", 0xff00ff },
		{ "magenta1", 0xff00ff },
		{ "magenta2", 0xee00ee },
		{ "magenta3", 0xcd00cd },
		{ "magenta4", 0x8b008b },
		{ "maroon", 0xb03060 },
		{ "maroon1", 0xff34b3 },
		{ "maroon2", 0xee30a7 },
		{ "maroon3", 0xcd2990 },
		{ "maroon4", 0x8b1c62 },
		{ "medium aquamarine", 0x66cdaa },
		{ "medium blue", 0x0000cd },
		{ "medium orchid", 0xba55d3 },
		{ "medium purple", 0x9370db },
		{ "medium sea green", 0x3cb371 },
		{ "medium slate blue", 0x7b68ee },
		{ "medium spring green", 0x00fa9a },
		{ "medium turquoise", 0x48d1cc },
		{ "medium violet red", 0xc71585 },
		{ "midnight blue", 0x191970 },
		{ "mint cream", 0xf5fffa },
		{ "misty rose", 0xffe4e1 },
		{ "moccasin", 0xffe4b5 },
		{ "navajo white", 0xffdead },
		{ "navy blue", 0x000080 },
		{ "navy", 0x000080 },
		{ "old lace", 0xfdf5e6 },
		{ "olive drab", 0x6b8e23 },
		{ "olive", 0x808000 },
		{ "orange red", 0xff4500 },
		{ "orange", 0xffa500 },
		{ "orange1", 0xffa500 },
		{ "orange2", 0xee9a00 },
		{ "orange3", 0xcd8500 },
		{ "orange4", 0x8b5a00 },
		{ "orchid", 0xda70d6 },
		{ "orchid1", 0xff83fa },
		{ "orchid2", 0xee7ae9 },
		{ "orchid3", 0xcd69c9 },
		{ "orchid4", 0x8b4789 },
		{ "pale goldenrod", 0xeee8aa },
		{ "pale green", 0x98fb98 },
		{ "pale turquoise", 0xafeeee },
		{ "pale violet red", 0xdb7093 },
		{ "papaya whip", 0xffefd5 },
		{ "peach puff", 0xffdab9 },
		{ "peru", 0xcd853f },
		{ "pink", 0xffc0cb },
		{ "pink1", 0xffb5c5 },
		{ "pink2", 0xeea9b8 },
		{ "pink3", 0xcd919e },
		{ "pink4", 0x8b636c },
		{ "plum", 0xdda0dd },
		{ "plum1", 0xffbbff },
		{ "plum2", 0xeeaeee },
		{ "plum3", 0xcd96cd },
		{ "plum4", 0x8b668b },
		{ "powder blue", 0xb0e0e6 },
		{ "purple", 0xa020f0 },
		{ "purple1", 0x9b30ff },
		{ "purple2", 0x912cee },
		{ "purple3", 0x7d26cd },
		{ "purple4", 0x551a8b },
		{ "rebecca purple", 0x663399 },
		{ "red", 0xff0000 },
		{ "red1", 0xff0000 },
		{ "red2", 0xee0000 },
		{ "red3", 0xcd0000 },
		{ "red4", 0x8b0000 },
		{ "rosy brown", 0xbc8f8f },
		{ "royal blue", 0x4169e1 },
		{ "saddle brown", 0x8b4513 },
		{ "salmon", 0xfa8072 },
		{ "salmon1", 0xff8c69 },
		{ "salmon2", 0xee8262 },
		{ "salmon3", 0xcd7054 },
		{ "salmon4", 0x8b4c39 },
		{ "sandy brown", 0xf4a460 },
		{ "sea green", 0x2e8b57 },
		{ "seashell", 0xfff5ee },
		{ "seashell1", 0xfff5ee },
		{ "seashell2", 0xeee5de },
		{ "seashell3", 0xcdc5bf },
		{ "seashell4", 0x8b8682 },
		{ "sienna", 0xa0522d },
		{ "sienna1", 0xff8247 },
		{ "sienna2", 0xee7942 },
		{ "sienna3", 0xcd6839 },
		{ "sienna4", 0x8b4726 },
		{ "silver", 0xc0c0c0 },
		{ "sky blue", 0x87ceeb },
		{ "slate blue", 0x6a5acd },
		{ "slate gray", 0x708090 },
		{ "slate grey", 0x708090 },
		{ "snow", 0xfffafa },
		{ "snow1", 0xfffafa },
		{ "snow2", 0xeee9e9 },
		{ "snow3", 0xcdc9c9 },
		{ "snow4", 0x8b8989 },
		{ "spring green", 0x00ff7f },
		{ "steel blue", 0x4682b4 },
		{ "tan", 0xd2b48c },
		{ "tan1", 0xffa54f },
		{ "tan2", 0xee9a49 },
		{ "tan3", 0xcd853f },
		{ "tan4", 0x8b5a2b },
		{ "teal", 0x008080 },
		{ "thistle", 0xd8bfd8 },
		{ "thistle1", 0xffe1ff },
		{ "thistle2", 0xeed2ee },
		{ "thistle3", 0xcdb5cd },
		{ "thistle4", 0x8b7b8b },
		{ "tomato", 0xff6347 },
		{ "tomato1", 0xff6347 },
		{ "tomato2", 0xee5c42 },
		{ "tomato3", 0xcd4f39 },
		{ "tomato4", 0x8b3626 },
		{ "turquoise", 0x40e0d0 },
		{ "turquoise1", 0x00f5ff },
		{ "turquoise2", 0x00e5ee },
		{ "turquoise3", 0x00c5cd },
		{ "turquoise4", 0x00868b },
		{ "violet red", 0xd02090 },
		{ "violet", 0xee82ee },
		{ "web gray", 0x808080 },
		{ "web green", 0x008000 },
		{ "web grey", 0x808080 },
		{ "web maroon", 0x800000 },
		{ "web purple", 0x800080 },
		{ "wheat", 0xf5deb3 },
		{ "wheat1", 0xffe7ba },
		{ "wheat2", 0xeed8ae },
		{ "wheat3", 0xcdba96 },
		{ "wheat4", 0x8b7e66 },
		{ "white smoke", 0xf5f5f5 },
		{ "white", 0xffffff },
		{ "x11 gray", 0xbebebe },
		{ "x11 green", 0x00ff00 },
		{ "x11 grey", 0xbebebe },
		{ "x11 maroon", 0xb03060 },
		{ "x11 purple", 0xa020f0 },
		{ "yellow green", 0x9acd32 },
		{ "yellow", 0xffff00 },
		{ "yellow1", 0xffff00 },
		{ "yellow2", 0xeeee00 },
		{ "yellow3", 0xcdcd00 },
		{ "yellow4", 0x8b8b00 }
	};
	u_int	i;
	int	c;

	if (strncmp(name, "grey", 4) == 0 || strncmp(name, "gray", 4) == 0) {
		if (!isdigit((u_char)name[4]))
			return (0xbebebe|COLOUR_FLAG_RGB);
		c = round(2.55 * atoi(name + 4));
		if (c < 0 || c > 255)
			return (-1);
		return (colour_join_rgb(c, c, c));
	}
	for (i = 0; i < nitems(colours); i++) {
		if (strcasecmp(colours[i].name, name) == 0)
			return (colours[i].c|COLOUR_FLAG_RGB);
	}
	return (-1);
}

/* Initialize palette. */
void
colour_palette_init(struct colour_palette *p)
{
	p->fg = 8;
	p->bg = 8;
	p->palette = NULL;
	p->default_palette = NULL;
}

/* Clear palette. */
void
colour_palette_clear(struct colour_palette *p)
{
	if (p != NULL) {
		p->fg = 8;
		p->bg = 8;
 		free(p->palette);
		p->palette = NULL;
	}
}

/* Free a palette. */
void
colour_palette_free(struct colour_palette *p)
{
	if (p != NULL) {
		free(p->palette);
		p->palette = NULL;
		free(p->default_palette);
		p->default_palette = NULL;
	}
}

/* Get a colour from a palette. */
int
colour_palette_get(struct colour_palette *p, int c)
{
	if (p == NULL)
		return (-1);

	if (c >= 90 && c <= 97)
		c = 8 + c - 90;
	else if (c & COLOUR_FLAG_256)
		c &= ~COLOUR_FLAG_256;
	else if (c >= 8)
		return (-1);

	if (p->palette != NULL && p->palette[c] != -1)
		return (p->palette[c]);
	if (p->default_palette != NULL && p->default_palette[c] != -1)
		return (p->default_palette[c]);
	return (-1);
}

/* Set a colour in a palette. */
int
colour_palette_set(struct colour_palette *p, int n, int c)
{
	u_int	i;

	if (p == NULL || n > 255)
		return (0);

	if (c == -1 && p->palette == NULL)
		return (0);

	if (c != -1 && p->palette == NULL) {
		if (p->palette == NULL)
			p->palette = xcalloc(256, sizeof *p->palette);
		for (i = 0; i < 256; i++)
			p->palette[i] = -1;
	}
	p->palette[n] = c;
	return (1);
}

/* Build palette defaults from an option. */
void
colour_palette_from_option(struct colour_palette *p, struct options *oo)
{
	struct options_entry		*o;
	struct options_array_item	*a;
	u_int				 i, n;
	int				 c;

	if (p == NULL)
		return;

	o = options_get(oo, "pane-colours");
	if ((a = options_array_first(o)) == NULL) {
		if (p->default_palette != NULL) {
			free(p->default_palette);
			p->default_palette = NULL;
		}
		return;
	}
	if (p->default_palette == NULL)
		p->default_palette = xcalloc(256, sizeof *p->default_palette);
	for (i = 0; i < 256; i++)
		p->default_palette[i] = -1;
	while (a != NULL) {
		n = options_array_item_index(a);
		if (n < 256) {
			c = options_array_item_value(a)->number;
			p->default_palette[n] = c;
		}
		a = options_array_next(a);
	}

}
