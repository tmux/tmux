/* $XTermId: charsets.c,v 1.71 2017/11/08 01:39:21 tom Exp $ */

/*
 * Copyright 1998-2013,2017 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 *
 */

#include <xterm.h>
#include <data.h>
#include <fontutils.h>

#include <X11/keysym.h>

/*
 * This module performs translation as needed to support the DEC VT220 national
 * replacement character sets.  We assume that xterm's font is based on the ISO
 * 8859-1 (Latin 1) character set, which is almost the same as the DEC
 * multinational character set.  Glyph positions 0-31 have to be the DEC
 * graphic characters, though.
 *
 * References:
 *	"VT220 Programmer Pocket Guide" EK-VT220-HR-002 (2nd ed., 1984), which
 *		contains character charts for the national character sets.
 *	"VT330/VT340 Programmer Reference Manual Volume 1: Text Programming"
 *		EK-VT3XX-TP-001 (1st ed, 1987), which contains a table (2-1)
 *		listing the glyphs which are mapped from the multinational
 *		character set to the national character set.
 *
 * The latter reference, though easier to read, has a few errors and omissions.
 */

#define map_NRCS_Dutch(code) \
	switch (code) { \
	    MAP(0x23, XK_sterling); \
	    MAP(0x40, XK_threequarters); \
	    UNI(0x5b, 0x0133); /* ij ligature */ \
	    MAP(0x5c, XK_onehalf); \
	    MAP(0x5d, XK_bar); \
	    MAP(0x7b, XK_diaeresis); \
	    UNI(0x7c, 0x0192); /* florin */ \
	    MAP(0x7d, XK_onequarter); \
	    MAP(0x7e, XK_acute); \
	}

#define map_NRCS_Finnish(code) \
	switch (code) { \
	    MAP(0x5b, XK_Adiaeresis); \
	    MAP(0x5c, XK_Odiaeresis); \
	    MAP(0x5d, XK_Aring); \
	    MAP(0x5e, XK_Udiaeresis); \
	    MAP(0x60, XK_eacute); \
	    MAP(0x7b, XK_adiaeresis); \
	    MAP(0x7c, XK_odiaeresis); \
	    MAP(0x7d, XK_aring); \
	    MAP(0x7e, XK_udiaeresis); \
	}

#define map_NRCS_French(code) \
	switch (code) { \
	    MAP(0x23, XK_sterling); \
	    MAP(0x40, XK_agrave); \
	    MAP(0x5b, XK_degree); \
	    MAP(0x5c, XK_ccedilla); \
	    MAP(0x5d, XK_section); \
	    MAP(0x7b, XK_eacute); \
	    MAP(0x7c, XK_ugrave); \
	    MAP(0x7d, XK_egrave); \
	    MAP(0x7e, XK_diaeresis); \
	}

#define map_NRCS_French_Canadian(code) \
	switch (code) { \
	    MAP(0x40, XK_agrave); \
	    MAP(0x5b, XK_acircumflex); \
	    MAP(0x5c, XK_ccedilla); \
	    MAP(0x5d, XK_ecircumflex); \
	    MAP(0x5e, XK_icircumflex); \
	    MAP(0x60, XK_ocircumflex); \
	    MAP(0x7b, XK_eacute); \
	    MAP(0x7c, XK_ugrave); \
	    MAP(0x7d, XK_egrave); \
	    MAP(0x7e, XK_ucircumflex); \
	}

#define map_NRCS_German(code) \
	switch (code) { \
	    MAP(0x40, XK_section); \
	    MAP(0x5b, XK_Adiaeresis); \
	    MAP(0x5c, XK_Odiaeresis); \
	    MAP(0x5d, XK_Udiaeresis); \
	    MAP(0x7b, XK_adiaeresis); \
	    MAP(0x7c, XK_odiaeresis); \
	    MAP(0x7d, XK_udiaeresis); \
	    MAP(0x7e, XK_ssharp); \
	}

#define map_NRCS_Italian(code) \
	switch (code) { \
	    MAP(0x23, XK_sterling); \
	    MAP(0x40, XK_section); \
	    MAP(0x5b, XK_degree); \
	    MAP(0x5c, XK_ccedilla); \
	    MAP(0x5d, XK_eacute); \
	    MAP(0x60, XK_ugrave); \
	    MAP(0x7b, XK_agrave); \
	    MAP(0x7c, XK_ograve); \
	    MAP(0x7d, XK_egrave); \
	    MAP(0x7e, XK_igrave); \
	}

#define map_NRCS_Norwegian_Danish(code) \
	switch (code) { \
	    MAP(0x40, XK_Adiaeresis); \
	    MAP(0x5b, XK_AE); \
	    MAP(0x5c, XK_Ooblique); \
	    MAP(0x5d, XK_Aring); \
	    MAP(0x5e, XK_Udiaeresis); \
	    MAP(0x60, XK_adiaeresis); \
	    MAP(0x7b, XK_ae); \
	    MAP(0x7c, XK_oslash); \
	    MAP(0x7d, XK_aring); \
	    MAP(0x7e, XK_udiaeresis); \
	}

#define map_NRCS_Portuguese(code) \
	switch (code) { \
	    MAP(0x5b, XK_Atilde); \
	    MAP(0x5c, XK_Ccedilla); \
	    MAP(0x5d, XK_Otilde); \
	    MAP(0x7b, XK_atilde); \
	    MAP(0x7c, XK_ccedilla); \
	    MAP(0x7d, XK_otilde); \
	}

#define map_NRCS_Spanish(code) \
	switch (code) { \
	    MAP(0x23, XK_sterling); \
	    MAP(0x40, XK_section); \
	    MAP(0x5b, XK_exclamdown); \
	    MAP(0x5c, XK_Ntilde); \
	    MAP(0x5d, XK_questiondown); \
	    MAP(0x7b, XK_degree); \
	    MAP(0x7c, XK_ntilde); \
	    MAP(0x7d, XK_ccedilla); \
	}

#define map_NRCS_Swedish(code) \
	switch (code) { \
	    MAP(0x40, XK_Eacute); \
	    MAP(0x5b, XK_Adiaeresis); \
	    MAP(0x5c, XK_Odiaeresis); \
	    MAP(0x5d, XK_Aring); \
	    MAP(0x5e, XK_Udiaeresis); \
	    MAP(0x60, XK_eacute); \
	    MAP(0x7b, XK_adiaeresis); \
	    MAP(0x7c, XK_odiaeresis); \
	    MAP(0x7d, XK_aring); \
	    MAP(0x7e, XK_udiaeresis); \
	}

#define map_NRCS_Swiss(code) \
	switch (code) { \
	    MAP(0x23, XK_ugrave); \
	    MAP(0x40, XK_agrave); \
	    MAP(0x5b, XK_eacute); \
	    MAP(0x5c, XK_ccedilla); \
	    MAP(0x5d, XK_ecircumflex); \
	    MAP(0x5e, XK_icircumflex); \
	    MAP(0x5f, XK_egrave); \
	    MAP(0x60, XK_ocircumflex); \
	    MAP(0x7b, XK_adiaeresis); \
	    MAP(0x7c, XK_odiaeresis); \
	    MAP(0x7d, XK_udiaeresis); \
	    MAP(0x7e, XK_ucircumflex); \
	}

/*
 * Unlike NRCS, which splices a few characters onto ASCII, the supplementary
 * character sets are complete, normally mapped to GR.  Most of these mappings
 * rely upon glyphs not found in ISO-8859-1.  We can display most of those
 * using Unicode, thereby supporting specialized applications that use SCS
 * with luit, subject to the limitation that select/paste will give meaningless
 * results in terms of the application which uses these mappings.
 *
 * Since the VT320, etc, use only 8-bit encodings, there is no plausible
 * argument to be made that these mappings "use" UTF-8, even though there is
 * a hidden step in the terminal emulator which relies upon UTF-8.
 */
#define map_SCS_DEC_Supp_Graphic(code,dft) \
	switch (code) { \
	    XXX(0x24, 0x2e2e); \
	    XXX(0x26, 0x2e2e); \
	    MAP(0x28, 0xa4); \
	    XXX(0x2c, 0x2e2e); \
	    XXX(0x2d, 0x2e2e); \
	    XXX(0x2e, 0x2e2e); \
	    XXX(0x2f, 0x2e2e); \
	    XXX(0x34, 0x2e2e); \
	    XXX(0x38, 0x2e2e); \
	    XXX(0x3e, 0x2e2e); \
	    XXX(0x50, 0x2e2e); \
	    UNI(0x57, 0x0152); \
	    MAP(0x5d, 0x0178); \
	    XXX(0x5e, 0x2e2e); \
	    XXX(0x70, 0x2e2e); \
	    UNI(0x77, 0x0153); \
	    MAP(0x7d, 0xff); \
	    XXX(0x7e, 0x2e2e); \
	    XXX(0x7f, 0x2e2e); \
	    default: dft; break; \
	}

	/* derived from http://www.vt100.net/charsets/technical.html */
#if OPT_WIDE_CHARS
#define map_SCS_DEC_Technical(code) \
	switch (code) { \
	    UNI(0x21, 0x23b7);	/* RADICAL SYMBOL BOTTOM Centred left to right, so that it joins up with 02/02 */ \
	    UNI(0x22, 0x250c);	/* BOX DRAWINGS LIGHT DOWN AND RIGHT */ \
	    UNI(0x23, 0x2500);	/* BOX DRAWINGS LIGHT HORIZONTAL */ \
	    UNI(0x24, 0x2320);	/* TOP HALF INTEGRAL with the proviso that the stem is vertical, to join with 02/06 */ \
	    UNI(0x25, 0x2321);	/* BOTTOM HALF INTEGRAL with the proviso above. */ \
	    UNI(0x26, 0x2502);	/* BOX DRAWINGS LIGHT VERTICAL */ \
	    UNI(0x27, 0x23a1);	/* LEFT SQUARE BRACKET UPPER CORNER Joins vertically to 02/06, 02/08. Doesn't join to its right. */ \
	    UNI(0x28, 0x23a3);	/* LEFT SQUARE BRACKET LOWER CORNER Joins vertically to 02/06, 02/07. Doesn't join to its right. */ \
	    UNI(0x29, 0x23a4);	/* RIGHT SQUARE BRACKET UPPER CORNER Joins vertically to 026, 02a. Doesn't join to its left. */ \
	    UNI(0x2a, 0x23a6);	/* RIGHT SQUARE BRACKET LOWER CORNER Joins vertically to 026, 029. Doesn't join to its left. */ \
	    UNI(0x2b, 0x23a7);	/* LEFT CURLY BRACKET UPPER HOOK Joins vertically to 026, 02c, 02/15. Doesn't join to its right. */ \
	    UNI(0x2c, 0x23a9);	/* LEFT CURLY BRACKET LOWER HOOK Joins vertically to 026, 02b, 02/15. Doesn't join to its right. */ \
	    UNI(0x2d, 0x23ab);	/* RIGHT CURLY BRACKET UPPER HOOK Joins vertically to 026, 02e, 03/00. Doesn't join to its left. */ \
	    UNI(0x2e, 0x23ad);	/* RIGHT CURLY BRACKET LOWER HOOK Joins vertically to 026, 02d, 03/00. Doesn't join to its left. */ \
	    UNI(0x2f, 0x23a8);	/* LEFT CURLY BRACKET MIDDLE PIECE Joins vertically to 026, 02b, 02c. */ \
	    UNI(0x30, 0x23ac);	/* RIGHT CURLY BRACKET MIDDLE PIECE Joins vertically to 02/06, 02d, 02e. */ \
	    XXX(0x31, 0x2426);	/* Top Left Sigma. Joins to right with 02/03, 03/05. Joins diagonally below right with 03/03, 03/07. */ \
	    XXX(0x32, 0x2426);	/* Bottom Left Sigma. Joins to right with 02/03, 03/06. Joins diagonally above right with 03/04, 03/07. */ \
	    XXX(0x33, 0x2426);	/* Top Diagonal Sigma. Line for joining 03/01 to 03/04 or 03/07. */ \
	    XXX(0x34, 0x2426);	/* Bottom Diagonal Sigma. Line for joining 03/02 to 03/03 or 03/07. */ \
	    XXX(0x35, 0x2426);	/* Top Right Sigma. Joins to left with 02/03, 03/01. */ \
	    XXX(0x36, 0x2426);	/* Bottom Right Sigma. Joins to left with 02/03, 03/02. */ \
	    XXX(0x37, 0x2426);	/* Middle Sigma. Joins diagonally with 03/01, 03/02, 03/03, 03/04. */ \
	    XXX(0x38, 0x2426);	/* undefined */ \
	    XXX(0x39, 0x2426);	/* undefined */ \
	    XXX(0x3a, 0x2426);	/* undefined */ \
	    XXX(0x3b, 0x2426);	/* undefined */ \
	    UNI(0x3c, 0x2264);	/* LESS-THAN OR EQUAL TO */ \
	    UNI(0x3d, 0x2260);	/* NOT EQUAL TO */ \
	    UNI(0x3e, 0x2265);	/* GREATER-THAN OR EQUAL TO */ \
	    UNI(0x3f, 0x222B);	/* INTEGRAL */ \
	    UNI(0x40, 0x2234);	/* THEREFORE */ \
	    UNI(0x41, 0x221d);	/* PROPORTIONAL TO */ \
	    UNI(0x42, 0x221e);	/* INFINITY */ \
	    UNI(0x43, 0x00f7);	/* DIVISION SIGN */ \
	    UNI(0x44, 0x0394);	/* GREEK CAPITAL DELTA */ \
	    UNI(0x45, 0x2207);	/* NABLA */ \
	    UNI(0x46, 0x03a6);	/* GREEK CAPITAL LETTER PHI */ \
	    UNI(0x47, 0x0393);	/* GREEK CAPITAL LETTER GAMMA */ \
	    UNI(0x48, 0x223c);	/* TILDE OPERATOR */ \
	    UNI(0x49, 0x2243);	/* ASYMPTOTICALLY EQUAL TO */ \
	    UNI(0x4a, 0x0398);	/* GREEK CAPITAL LETTER THETA */ \
	    UNI(0x4b, 0x00d7);	/* MULTIPLICATION SIGN */ \
	    UNI(0x4c, 0x039b);	/* GREEK CAPITAL LETTER LAMDA */ \
	    UNI(0x4d, 0x21d4);	/* LEFT RIGHT DOUBLE ARROW */ \
	    UNI(0x4e, 0x21d2);	/* RIGHTWARDS DOUBLE ARROW */ \
	    UNI(0x4f, 0x2261);	/* IDENTICAL TO */ \
	    UNI(0x50, 0x03a0);	/* GREEK CAPITAL LETTER PI */ \
	    UNI(0x51, 0x03a8);	/* GREEK CAPITAL LETTER PSI */ \
	    UNI(0x52, 0x2426);	/* undefined */ \
	    UNI(0x53, 0x03a3);	/* GREEK CAPITAL LETTER SIGMA */ \
	    XXX(0x54, 0x2426);	/* undefined */ \
	    XXX(0x55, 0x2426);	/* undefined */ \
	    UNI(0x56, 0x221a);	/* SQUARE ROOT */ \
	    UNI(0x57, 0x03a9);	/* GREEK CAPITAL LETTER OMEGA */ \
	    UNI(0x58, 0x039e);	/* GREEK CAPITAL LETTER XI */ \
	    UNI(0x59, 0x03a5);	/* GREEK CAPITAL LETTER UPSILON */ \
	    UNI(0x5a, 0x2282);	/* SUBSET OF */ \
	    UNI(0x5b, 0x2283);	/* SUPERSET OF */ \
	    UNI(0x5c, 0x2229);	/* INTERSECTION */ \
	    UNI(0x5d, 0x222a);	/* UNION */ \
	    UNI(0x5e, 0x2227);	/* LOGICAL AND */ \
	    UNI(0x5f, 0x2228);	/* LOGICAL OR */ \
	    UNI(0x60, 0x00ac);	/* NOT SIGN */ \
	    UNI(0x61, 0x03b1);	/* GREEK SMALL LETTER ALPHA */ \
	    UNI(0x62, 0x03b2);	/* GREEK SMALL LETTER BETA */ \
	    UNI(0x63, 0x03c7);	/* GREEK SMALL LETTER CHI */ \
	    UNI(0x64, 0x03b4);	/* GREEK SMALL LETTER DELTA */ \
	    UNI(0x65, 0x03b5);	/* GREEK SMALL LETTER EPSILON */ \
	    UNI(0x66, 0x03c6);	/* GREEK SMALL LETTER PHI */ \
	    UNI(0x67, 0x03b3);	/* GREEK SMALL LETTER GAMMA */ \
	    UNI(0x68, 0x03b7);	/* GREEK SMALL LETTER ETA */ \
	    UNI(0x69, 0x03b9);	/* GREEK SMALL LETTER IOTA */ \
	    UNI(0x6a, 0x03b8);	/* GREEK SMALL LETTER THETA */ \
	    UNI(0x6b, 0x03ba);	/* GREEK SMALL LETTER KAPPA */ \
	    UNI(0x6c, 0x03bb);	/* GREEK SMALL LETTER LAMDA */ \
	    XXX(0x6d, 0x2426);	/* undefined */ \
	    UNI(0x6e, 0x03bd);	/* GREEK SMALL LETTER NU */ \
	    UNI(0x6f, 0x2202);	/* PARTIAL DIFFERENTIAL */ \
	    UNI(0x70, 0x03c0);	/* GREEK SMALL LETTER PI */ \
	    UNI(0x71, 0x03c8);	/* GREEK SMALL LETTER PSI */ \
	    UNI(0x72, 0x03c1);	/* GREEK SMALL LETTER RHO */ \
	    UNI(0x73, 0x03c3);	/* GREEK SMALL LETTER SIGMA */ \
	    UNI(0x74, 0x03c4);	/* GREEK SMALL LETTER TAU */ \
	    XXX(0x75, 0x2426);	/* undefined */ \
	    UNI(0x76, 0x0192);	/* LATIN SMALL LETTER F WITH HOOK Probably chosen for its meaning of "function" */ \
	    UNI(0x77, 0x03c9);	/* GREEK SMALL LETTER OMEGA */ \
	    UNI(0x78, 0x03bE);	/* GREEK SMALL LETTER XI */ \
	    UNI(0x79, 0x03c5);	/* GREEK SMALL LETTER UPSILON */ \
	    UNI(0x7a, 0x03b6);	/* GREEK SMALL LETTER ZETA */ \
	    UNI(0x7b, 0x2190);	/* LEFTWARDS ARROW */ \
	    UNI(0x7c, 0x2191);	/* UPWARDS ARROW */ \
	    UNI(0x7d, 0x2192);	/* RIGHTWARDS ARROW */ \
	    UNI(0x7e, 0x2193);	/* DOWNWARDS ARROW */ \
	}
#else
#define map_SCS_DEC_Technical(code)	/* nothing */
#endif /* OPT_WIDE_CHARS */

/*
 * Translate an input keysym to the corresponding NRC keysym.
 */
unsigned
xtermCharSetIn(TScreen *screen, unsigned code, int charset)
{
#define MAP(to, from) case from: code = to; break

#if OPT_WIDE_CHARS
#define UNI(to, from) case from: if (screen->utf8_nrc_mode) code = to; break
#else
#define UNI(to, from) case from: break
#endif

#define XXX(to, from)		/* no defined mapping to 0..255 */

    TRACE(("CHARSET-IN GL=%s(G%d) GR=%s(G%d) SS%d\n\t%s\n",
	   visibleScsCode(screen->gsets[screen->curgl]), screen->curgl,
	   visibleScsCode(screen->gsets[screen->curgr]), screen->curgr,
	   screen->curss,
	   visibleUChar(code)));

    switch (charset) {
    case nrc_British:		/* United Kingdom set (or Latin 1)      */
	if (code == XK_sterling)
	    code = 0x23;
	code &= 0x7f;
	break;

#if OPT_XMC_GLITCH
    case nrc_Unknown:
#endif
    case nrc_DEC_Alt_Chars:
    case nrc_DEC_Alt_Graphics:
    case nrc_ASCII:
	break;

    case nrc_DEC_Spec_Graphic:
	break;

    case nrc_DEC_Supp:
	map_SCS_DEC_Supp_Graphic(code, code &= 0x7f);
	break;

    case nrc_DEC_Supp_Graphic:
	map_SCS_DEC_Supp_Graphic(code, code |= 0x80);
	break;

    case nrc_DEC_Technical:
	map_SCS_DEC_Technical(code);
	break;

    case nrc_Dutch:
	map_NRCS_Dutch(code);
	break;

    case nrc_Finnish:
    case nrc_Finnish2:
	map_NRCS_Finnish(code);
	break;

    case nrc_French:
    case nrc_French2:
	map_NRCS_French(code);
	break;

    case nrc_French_Canadian:
	map_NRCS_French_Canadian(code);
	break;

    case nrc_German:
	map_NRCS_German(code);
	break;

    case nrc_Hebrew:
    case nrc_Hebrew2:
	/* FIXME */
	break;

    case nrc_Italian:
	map_NRCS_Italian(code);
	break;

    case nrc_Norwegian_Danish:
    case nrc_Norwegian_Danish2:
    case nrc_Norwegian_Danish3:
	map_NRCS_Norwegian_Danish(code);
	break;

    case nrc_Portugese:
	map_NRCS_Portuguese(code);
	break;

    case nrc_SCS_NRCS:		/* vt5xx - probably Serbo/Croatian */
	/* FIXME */
	break;

    case nrc_Spanish:
	map_NRCS_Spanish(code);
	break;

    case nrc_Swedish2:
    case nrc_Swedish:
	map_NRCS_Swedish(code);
	break;

    case nrc_Swiss:
	map_NRCS_Swiss(code);
	break;

    case nrc_Turkish:
    case nrc_Turkish2:
	/* FIXME */
	break;

    default:			/* any character sets we don't recognize */
	break;
    }
    code &= 0x7f;		/* NRC in any case is 7-bit */
    TRACE(("->\t%s\n",
	   visibleUChar(code)));
    return code;
#undef MAP
#undef UNI
#undef XXX
}

/*
 * Translate a string to the display form.  This assumes the font has the
 * DEC graphic characters in cells 0-31, and otherwise is ISO-8859-1.
 */
int
xtermCharSetOut(XtermWidget xw, IChar *buf, IChar *ptr, int leftset)
{
    IChar *s;
    TScreen *screen = TScreenOf(xw);
    int count = 0;
    int rightset = screen->gsets[(int) (screen->curgr)];

#define MAP(from, to) case from: chr = to; break

#if OPT_WIDE_CHARS
#define UNI(from, to) case from: if (screen->utf8_nrc_mode) chr = to; break
#define XXX(from, to) UNI(from, to)
#else
#define UNI(old, new) chr = old; break
#define XXX(from, to)		/* nothing */
#endif

    TRACE(("CHARSET-OUT GL=%s(G%d) GR=%s(G%d) SS%d\n\t%s\n",
	   visibleScsCode(leftset), screen->curgl,
	   visibleScsCode(rightset), screen->curgr,
	   screen->curss,
	   visibleIChars(buf, (unsigned) (ptr - buf))));

    for (s = buf; s < ptr; ++s) {
	int eight = CharOf(E2A(*s));
	int seven = eight & 0x7f;
	int cs = (eight >= 128) ? rightset : leftset;
	int chr = eight;

	count++;
#if OPT_WIDE_CHARS
	/*
	 * This is only partly right - prevent inadvertant remapping of
	 * the replacement character and other non-8bit codes into bogus
	 * 8bit codes.
	 */
	if (screen->utf8_mode || screen->utf8_nrc_mode) {
	    if (*s > 255)
		continue;
	}
#endif
	if (*s < 32)
	    continue;

	switch (cs) {
	case nrc_British_Latin_1:
	    /* FALLTHRU */
	case nrc_British:	/* United Kingdom set (or Latin 1)      */
	    if ((xw->flags & NATIONAL)
		|| (screen->vtXX_level <= 1)) {
		if ((xw->flags & NATIONAL)) {
		    chr = seven;
		}
		if (chr == 0x23) {
		    chr = XTERM_POUND;
#if OPT_WIDE_CHARS
		    if (screen->utf8_nrc_mode) {
			chr = 0xa3;
		    }
#endif
		}
	    } else {
		chr = (seven | 0x80);
	    }
	    break;

#if OPT_XMC_GLITCH
	case nrc_Unknown:
#endif
	case nrc_DEC_Alt_Chars:
	case nrc_DEC_Alt_Graphics:
	case nrc_ASCII:
	    break;

	case nrc_DEC_Spec_Graphic:
	    if (seven > 0x5f && seven <= 0x7e) {
#if OPT_WIDE_CHARS
		if (screen->utf8_mode || screen->utf8_nrc_mode)
		    chr = (int) dec2ucs((unsigned) (seven - 0x5f));
		else
#endif
		    chr = seven - 0x5f;
	    } else {
		chr = seven;
	    }
	    break;

	case nrc_DEC_Supp:
	    map_SCS_DEC_Supp_Graphic(chr = seven, chr |= 0x80);
	    break;

	case nrc_DEC_Supp_Graphic:
	    map_SCS_DEC_Supp_Graphic(chr = seven, chr |= 0x80);
	    break;

	case nrc_DEC_Technical:
	    map_SCS_DEC_Technical(chr = seven);
	    break;

	case nrc_Dutch:
	    map_NRCS_Dutch(chr = seven);
	    break;

	case nrc_Finnish:
	case nrc_Finnish2:
	    map_NRCS_Finnish(chr = seven);
	    break;

	case nrc_French:
	case nrc_French2:
	    map_NRCS_French(chr = seven);
	    break;

	case nrc_French_Canadian:
	case nrc_French_Canadian2:
	    map_NRCS_French_Canadian(chr = seven);
	    break;

	case nrc_German:
	    map_NRCS_German(chr = seven);
	    break;

	case nrc_Hebrew:
	case nrc_Hebrew2:
	    /* FIXME */
	    break;

	case nrc_Italian:
	    map_NRCS_Italian(chr = seven);
	    break;

	case nrc_Norwegian_Danish:
	case nrc_Norwegian_Danish2:
	case nrc_Norwegian_Danish3:
	    map_NRCS_Norwegian_Danish(chr = seven);
	    break;

	case nrc_Portugese:
	    map_NRCS_Portuguese(chr = seven);
	    break;

	case nrc_SCS_NRCS:	/* vt5xx - probably Serbo/Croatian */
	    /* FIXME */
	    break;

	case nrc_Spanish:
	    map_NRCS_Spanish(chr = seven);
	    break;

	case nrc_Swedish2:
	case nrc_Swedish:
	    map_NRCS_Swedish(chr = seven);
	    break;

	case nrc_Swiss:
	    map_NRCS_Swiss(chr = seven);
	    break;

	case nrc_Turkish:
	case nrc_Turkish2:
	    /* FIXME */
	    break;

	default:		/* any character sets we don't recognize */
	    count--;
	    break;
	}
	/*
	 * The state machine already treated DEL as a nonprinting and
	 * nonspacing character.  If we have DEL now, simply render
	 * it as a blank.
	 */
	if (chr == ANSI_DEL)
	    chr = ' ';
	*s = (IChar) A2E(chr);
    }
    TRACE(("%d\t%s\n",
	   count,
	   visibleIChars(buf, (unsigned) (ptr - buf))));
    return count;
#undef MAP
#undef UNI
#undef XXX
}
