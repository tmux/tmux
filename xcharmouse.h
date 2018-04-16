/* $XTermId: xcharmouse.h,v 1.18 2012/09/26 00:39:14 tom Exp $ */

/************************************************************

Copyright 1997-2011,2012 by Thomas E. Dickey
Copyright 1998 by Jason Bacon <acadix@execpc.com>

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the above listed
copyright holder(s) not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

#ifndef included_xcharmouse_h
#define included_xcharmouse_h
/* *INDENT-OFF* */

/*
 * Macros for dpmodes (Thomas Dickey and others):
 * J. Bacon, acadix@execpc.com, June 1998
 * Steve Wall, September 1999
 * Ilya Zakharevich, August 2002
 * Ryan Johnson, August 2010
 * Egmont Koblinger, December 2011
 */

/* DECSET arguments for turning on mouse reporting modes */
#define SET_X10_MOUSE               9
#define SET_VT200_MOUSE             1000
#define SET_VT200_HIGHLIGHT_MOUSE   1001
#define SET_BTN_EVENT_MOUSE         1002
#define SET_ANY_EVENT_MOUSE         1003

#if OPT_FOCUS_EVENT
#define SET_FOCUS_EVENT_MOUSE       1004 /* can be combined with above */
#endif

/* Extend mouse tracking for terminals wider(taller) than 223 cols(rows) */
#define SET_EXT_MODE_MOUSE          1005 /* compatible with above */
#define SET_SGR_EXT_MODE_MOUSE      1006
#define SET_URXVT_EXT_MODE_MOUSE    1015

#define SET_ALTERNATE_SCROLL        1007 /* wheel mouse may send cursor-keys */

#define SET_BUTTON1_MOVE_POINT      2001 /* click1 emit Esc seq to move point*/
#define SET_BUTTON2_MOVE_POINT      2002 /* press2 emit Esc seq to move point*/
#define SET_DBUTTON3_DELETE         2003 /* Double click-3 deletes */
#define SET_PASTE_IN_BRACKET        2004 /* Surround paste by escapes */
#define SET_PASTE_QUOTE             2005 /* Quote each char during paste */
#define SET_PASTE_LITERAL_NL        2006 /* Paste "\n" as C-j */

#if OPT_DEC_LOCATOR

/* Bit fields for screen->locator_events */
#define	LOC_BTNS_DN		0x1
#define	LOC_BTNS_UP		0x2

/* Special values for screen->loc_filter_* */
#define	LOC_FILTER_POS		-1

#endif /* OPT_DEC_LOCATOR */

/* Values for screen->send_mouse_pos */
typedef enum {
    MOUSE_OFF
    ,X10_MOUSE
    ,VT200_MOUSE
    ,VT200_HIGHLIGHT_MOUSE
    ,BTN_EVENT_MOUSE
    ,ANY_EVENT_MOUSE
    ,DEC_LOCATOR
} XtermMouseModes;

/* *INDENT-ON* */

#endif /* included_xcharmouse_h */
