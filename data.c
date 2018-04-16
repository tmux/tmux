/* $XTermId: data.c,v 1.98 2017/12/18 23:38:05 tom Exp $ */

/*
 * Copyright 2002-2016,2017 by Thomas E. Dickey
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

#include <data.h>

Widget toplevel;		/* top-most widget in xterm */

#if OPT_TEK4014
Char *Tpushb;
Char *Tpushback;
TekLink *tekRefreshList;
TekWidget tekWidget;
Widget tekshellwidget;
int T_lastx = -1;
int T_lasty = -1;
int Ttoggled = 0;
jmp_buf Tekend;
#endif

char *ProgramName;

Arg ourTopLevelShellArgs[] =
{
    {XtNallowShellResize, (XtArgVal) True},
    {XtNinput, (XtArgVal) True},
};
Cardinal number_ourTopLevelShellArgs = 2;

Atom wm_delete_window;		/* for ICCCM delete window */

Boolean guard_keyboard_type = False;
XTERM_RESOURCE resource;

PtyData *VTbuffer;

jmp_buf VTend;

#ifdef DEBUG
int debug = 0;			/* true causes error messages to be displayed */
#endif /* DEBUG */

XtAppContext app_con;
XtermWidget term;		/* master data structure for client */

int hold_screen;
SIG_ATOMIC_T need_cleanup = False;
SIG_ATOMIC_T caught_intr = False;

int am_slave = -1;		/* set to file-descriptor if we're a slave process */
int max_plus1;
PtySelect Select_mask;
PtySelect X_mask;
PtySelect pty_mask;
char *ptydev;
char *ttydev;

#if HANDLE_STRUCT_NOTIFY
int mapstate = -1;
#endif /* HANDLE_STRUCT_NOTIFY */

#ifdef HAVE_LIB_XCURSOR
char *xterm_cursor_theme;
#endif

#if OPT_SESSION_MGT
int ice_fd = -1;
#endif

#ifdef USE_IGNORE_RC
int ignore_unused;
#endif

#if OPT_DIRECT_COLOR
CellColor initCColor =
{0, 0};
#else
CellColor initCColor = 0;
#endif
