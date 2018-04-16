/* $XTermId: xtermcap.h,v 1.20 2013/06/23 15:34:37 tom Exp $ */

/*
 * Copyright 2007-2011,2013 by Thomas E. Dickey
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
 */

/*
 * Common/useful definitions for XTERM termcap interface.
 */
#ifndef included_xtermcap_h
#define included_xtermcap_h
/* *INDENT-OFF* */

#include <xterm.h>

#include <ptyx.h>

#ifndef HAVE_TIGETSTR
#undef USE_TERMINFO
#endif

#ifndef USE_TERMINFO
#define USE_TERMINFO 0
#endif

#if !USE_TERMINFO
#undef HAVE_TIGETSTR
#ifndef USE_TERMCAP
#define USE_TERMCAP 1
#endif
#endif

#undef ERR			/* workaround for glibc 2.1.3 */

#ifdef HAVE_NCURSES_CURSES_H
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

#ifndef NCURSES_VERSION
#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#endif
#endif

#ifdef HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#elif defined(HAVE_TERM_H)
#include <term.h>		/* tgetent() */
#endif

/*
 * Get rid of conflicting symbols from term.h
 */
#undef bell

/***====================================================================***/

#ifdef __cplusplus
extern "C" {
#endif

#define	MOD_NONE	1
#define	MOD_SHIFT	1
#define	MOD_ALT		2
#define	MOD_CTRL	4
#define	MOD_META	8

#define MODIFIER_NAME(parm, name) \
	(((parm > MOD_NONE) && ((parm - MOD_NONE) & name)) ? " "#name : "")

/* xtermcap.c */
extern Bool get_termcap(XtermWidget /* xw */, char * /* name */);
extern void set_termcap(XtermWidget /* xw */, const char * /* name */);
extern void free_termcap(XtermWidget /* xw */);

extern char *get_tcap_buffer(XtermWidget /* xw */);
extern char *get_tcap_erase(XtermWidget /* xw */);

#if OPT_TCAP_FKEYS
extern int xtermcapString(XtermWidget /* xw */, int /* keycode */, unsigned /* mask */);
#endif

#if OPT_TCAP_QUERY
extern int xtermcapKeycode(XtermWidget /* xw */, const char ** /* params */, unsigned * /* state */, Bool * /* fkey */);
#endif

#ifdef __cplusplus
	}
#endif

/* *INDENT-ON* */
#endif	/* included_xtermcap_h */
