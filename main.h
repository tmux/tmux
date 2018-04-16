/* $XTermId: main.h,v 1.63 2016/12/22 23:43:46 tom Exp $ */

/*
 * Copyright 2000-2013,2016 by Thomas E. Dickey
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
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
#ifndef included_main_h
#define included_main_h

#include <xterm.h>

#ifndef DEFCLASS
#define DEFCLASS		"XTerm"
#endif

#ifndef DEFFONT
#define DEFFONT			"fixed"
#endif

#ifndef DEFWIDEFONT
#define DEFWIDEFONT		NULL	/* grab one which is 2x as wide */
#endif

#ifndef DEFWIDEBOLDFONT
#define DEFWIDEBOLDFONT		NULL
#endif

#ifndef DEFXIMFONT
#define DEFXIMFONT		"fixed"
#endif

#ifndef DEFBOLDFONT
#define DEFBOLDFONT		NULL	/* no bold font uses overstriking */
#endif

#ifndef DEFBORDER
#define DEFBORDER		2
#endif

#ifndef DEFFACENAME
#define DEFFACENAME		NULL
#endif

#ifndef DEFFACENAME_AUTO
#define DEFFACENAME_AUTO	"mono"
#endif

#ifndef DEFFACESIZE
#define DEFFACESIZE		"14.0"
#endif

#ifndef DEF_ALLOW_COLOR
#define DEF_ALLOW_COLOR		True
#endif

#ifndef DEF_ALLOW_FONT
#define DEF_ALLOW_FONT		True
#endif

#ifndef DEF_ALLOW_MOUSE
#define DEF_ALLOW_MOUSE		True
#endif

#ifndef DEF_ALLOW_TCAP
#define DEF_ALLOW_TCAP		True
#endif

#ifndef DEF_ALLOW_TITLE
#define DEF_ALLOW_TITLE		True
#endif

#ifndef DEF_ALLOW_WINDOW
#define DEF_ALLOW_WINDOW	False
#endif

#ifndef DEF_DISALLOWED_COLOR
#define DEF_DISALLOWED_COLOR	"SetColor,GetColor,GetAnsiColor"
#endif

#ifndef DEF_DISALLOWED_FONT
#define DEF_DISALLOWED_FONT	"SetFont,GetFont"
#endif

#ifndef DEF_DISALLOWED_MOUSE
#define DEF_DISALLOWED_MOUSE	"*"
#endif

#ifndef DEF_DISALLOWED_TCAP
#define DEF_DISALLOWED_TCAP	"SetTcap,GetTcap"
#endif

#ifndef DEF_DISALLOWED_WINDOW
#define DEF_DISALLOWED_WINDOW	"20,21,SetXprop,SetSelection"
#endif

#if OPT_BLINK_TEXT
#define DEFBLINKASBOLD		False
#else
#define DEFBLINKASBOLD		True
#endif

#ifndef DEFDELETE_DEL
#define DEFDELETE_DEL		Maybe
#endif

#ifndef DEF_BACKARO_ERASE
#define DEF_BACKARO_ERASE	False
#endif

#ifndef DEF_BACKARO_BS
#define DEF_BACKARO_BS		True
#endif

#ifndef DEF_ALT_SENDS_ESC
#define DEF_ALT_SENDS_ESC	False
#endif

#ifndef DEF_META_SENDS_ESC
#define DEF_META_SENDS_ESC	False
#endif

#ifndef DEF_8BIT_META
#define DEF_8BIT_META		"true"	/* eightBitMeta */
#endif

#ifndef DEF_COLOR4
#define DEF_COLOR4		"blue2"		/* see XTerm-col.ad */
#endif

#ifndef DEF_COLOR12
#define DEF_COLOR12		"rgb:5c/5c/ff"	/* see XTerm-col.ad */
#endif

#ifndef DEF_INITIAL_ERASE
#define DEF_INITIAL_ERASE	False
#endif

#ifndef DEF_MENU_LOCALE
#define DEF_MENU_LOCALE		"C"
#endif

#ifndef DEF_POINTER_MODE
#define DEF_POINTER_MODE	pNoMouse
#endif

#ifndef DEF_PTY_STTY_SIZE
#if defined(linux) || defined(__APPLE__)
#define DEF_PTY_STTY_SIZE	False
#else
#define DEF_PTY_STTY_SIZE	True
#endif
#endif

#ifndef DEF_TITLE_MODES
#define DEF_TITLE_MODES		0
#endif

#ifndef PROJECTROOT
#define PROJECTROOT		"/usr/X11R6"
#endif

/*
 * The configure script quotes PROJECTROOT's value.
 * imake does not quote PROJECTROOT's value.
 */
#ifdef HAVE_CONFIG_H
#define DEFLOCALEFILTER2(x)	x
#else
#define DEFLOCALEFILTER2(x)	#x
#endif

/*
 * If the configure script finds luit, we have the path directly.
 */
#ifdef LUIT_PATH
#define DEFLOCALEFILTER		LUIT_PATH
#else
#define DEFLOCALEFILTER1(x)	DEFLOCALEFILTER2(x)
#define DEFLOCALEFILTER		DEFLOCALEFILTER1(PROJECTROOT) "/bin/luit"
#endif

/*
 * See lib/Xt/Resources.c
 */
#define MAXRESOURCES            400

#endif /* included_main_h */
