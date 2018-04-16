/* $XTermId: scrollbar.c,v 1.202 2017/12/26 01:58:48 tom Exp $ */

/*
 * Copyright 2000-2016,2017 by Thomas E. Dickey
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

#include <xterm.h>

#include <X11/Xatom.h>

#if defined(HAVE_LIB_XAW)
#include <X11/Xaw/Scrollbar.h>
#elif defined(HAVE_LIB_XAW3D)
#include <X11/Xaw3d/Scrollbar.h>
#elif defined(HAVE_LIB_XAW3DXFT)
#include <X11/Xaw3dxft/Scrollbar.h>
#elif defined(HAVE_LIB_NEXTAW)
#include <X11/neXtaw/Scrollbar.h>
#elif defined(HAVE_LIB_XAWPLUS)
#include <X11/XawPlus/Scrollbar.h>
#endif

#if defined(HAVE_XKBQUERYEXTENSION)
#include <X11/extensions/XKB.h>
#include <X11/XKBlib.h>
#endif

#include <data.h>
#include <error.h>
#include <menu.h>
#include <xstrings.h>

/*
 * The scrollbar's border overlaps the border of the vt100 window.  If there
 * is no border for the vt100, there can be no border for the scrollbar.
 */
#define SCROLLBAR_BORDER(xw) (TScreenOf(xw)->scrollBarBorder)
#if OPT_TOOLBAR
#define ScrollBarBorder(xw) (BorderWidth(xw) ? SCROLLBAR_BORDER(xw) : 0)
#else
#define ScrollBarBorder(xw) SCROLLBAR_BORDER(xw)
#endif

/* Event handlers */

static void ScrollTextTo PROTO_XT_CALLBACK_ARGS;
static void ScrollTextUpDownBy PROTO_XT_CALLBACK_ARGS;

/* Resize the text window for a terminal screen, modifying the
 * appropriate WM_SIZE_HINTS and taking advantage of bit gravity.
 */
void
DoResizeScreen(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    int border = 2 * screen->border;
    int min_wide = border + screen->fullVwin.sb_info.width;
    int min_high = border;
    XtGeometryResult geomreqresult;
    Dimension reqWidth, reqHeight, repWidth, repHeight;
#ifndef NO_ACTIVE_ICON
    VTwin *saveWin = WhichVWin(screen);

    /* all units here want to be in the normal font units */
    WhichVWin(screen) = &screen->fullVwin;
#endif /* NO_ACTIVE_ICON */

    /*
     * I'm going to try to explain, as I understand it, why we
     * have to do XGetWMNormalHints and XSetWMNormalHints here,
     * although I can't guarantee that I've got it right.
     *
     * In a correctly written toolkit program, the Shell widget
     * parses the user supplied geometry argument.  However,
     * because of the way xterm does things, the VT100 widget does
     * the parsing of the geometry option, not the Shell widget.
     * The result of this is that the Shell widget doesn't set the
     * correct window manager hints, and doesn't know that the
     * user has specified a geometry.
     *
     * The XtVaSetValues call below tells the Shell widget to
     * change its hints.  However, since it's confused about the
     * hints to begin with, it doesn't get them all right when it
     * does the SetValues -- it undoes some of what the VT100
     * widget did when it originally set the hints.
     *
     * To fix this, we do the following:
     *
     * 1. Get the sizehints directly from the window, going around
     *    the (confused) shell widget.
     * 2. Call XtVaSetValues to let the shell widget know which
     *    hints have changed.  Note that this may not even be
     *    necessary, since we're going to right ahead after that
     *    and set the hints ourselves, but it's good to put it
     *    here anyway, so that when we finally do fix the code so
     *    that the Shell does the right thing with hints, we
     *    already have the XtVaSetValues in place.
     * 3. We set the sizehints directly, this fixing up whatever
     *    damage was done by the Shell widget during the
     *    XtVaSetValues.
     *
     * Gross, huh?
     *
     * The correct fix is to redo VTRealize, VTInitialize and
     * VTSetValues so that font processing happens early enough to
     * give back responsibility for the size hints to the Shell.
     *
     * Someday, we hope to have time to do this.  Someday, we hope
     * to have time to completely rewrite xterm.
     */

    TRACE(("DoResizeScreen\n"));

#if 1				/* ndef nothack */
    /*
     * NOTE: the hints and the XtVaSetValues() must match.
     */
    TRACE(("%s@%d -- ", __FILE__, __LINE__));
    TRACE_WM_HINTS(xw);
    getXtermSizeHints(xw);

    xtermSizeHints(xw, ScrollbarWidth(screen));

    /* These are obsolete, but old clients may use them */
    xw->hints.width = MaxCols(screen) * FontWidth(screen) + xw->hints.min_width;
    xw->hints.height = MaxRows(screen) * FontHeight(screen) + xw->hints.min_height;
#if OPT_MAXIMIZE
    /* assure single-increment resize for fullscreen */
    if (xw->work.ewmh[0].mode) {
	xw->hints.width_inc = 1;
	xw->hints.height_inc = 1;
    }
#endif /* OPT_MAXIMIZE */
#endif

    XSetWMNormalHints(screen->display, VShellWindow(xw), &xw->hints);

    reqWidth = (Dimension) (MaxCols(screen) * FontWidth(screen) + min_wide);
    reqHeight = (Dimension) (MaxRows(screen) * FontHeight(screen) + min_high);

#if OPT_MAXIMIZE
    /* compensate for fullscreen mode */
    if (xw->work.ewmh[0].mode) {
	Screen *xscreen = DefaultScreenOfDisplay(xw->screen.display);
	reqWidth = (Dimension) WidthOfScreen(xscreen);
	reqHeight = (Dimension) HeightOfScreen(xscreen);
	ScreenResize(xw, reqWidth, reqHeight, &xw->flags);
    }
#endif /* OPT_MAXIMIZE */

    TRACE(("...requesting screensize chars %dx%d, pixels %dx%d\n",
	   MaxRows(screen),
	   MaxCols(screen),
	   reqHeight, reqWidth));

    geomreqresult = REQ_RESIZE((Widget) xw, reqWidth, reqHeight,
			       &repWidth, &repHeight);

    if (geomreqresult == XtGeometryAlmost) {
	TRACE(("...almost, retry screensize %dx%d\n", repHeight, repWidth));
	geomreqresult = REQ_RESIZE((Widget) xw, repWidth,
				   repHeight, NULL, NULL);
    }

    if (geomreqresult != XtGeometryYes) {
	/* The resize wasn't successful, so we might need to adjust
	   our idea of how large the screen is. */
	TRACE(("...still no (%d) - resize the core-class\n", geomreqresult));
	xw->core.widget_class->core_class.resize((Widget) xw);
    }
#if 1				/* ndef nothack */
    /*
     * XtMakeResizeRequest() has the undesirable side-effect of clearing
     * the window manager's hints, even on a failed request.  This would
     * presumably be fixed if the shell did its own work.
     */
    if (xw->hints.flags
	&& repHeight
	&& repWidth) {
	xw->hints.height = repHeight;
	xw->hints.width = repWidth;
	TRACE_HINTS(&xw->hints);
	XSetWMNormalHints(screen->display, VShellWindow(xw), &xw->hints);
    }
#endif
    XSync(screen->display, False);	/* synchronize */
    if (xtermAppPending())
	xevents();

#ifndef NO_ACTIVE_ICON
    WhichVWin(screen) = saveWin;
#endif /* NO_ACTIVE_ICON */
}

static Widget
CreateScrollBar(XtermWidget xw, int x, int y, int height)
{
    Widget result;
    Arg args[6];

    XtSetArg(args[0], XtNx, x);
    XtSetArg(args[1], XtNy, y);
    XtSetArg(args[2], XtNheight, height);
    XtSetArg(args[3], XtNreverseVideo, xw->misc.re_verse);
    XtSetArg(args[4], XtNorientation, XtorientVertical);
    XtSetArg(args[5], XtNborderWidth, ScrollBarBorder(xw));

    result = XtCreateWidget("scrollbar", scrollbarWidgetClass,
			    (Widget) xw, args, XtNumber(args));
    XtAddCallback(result, XtNscrollProc, ScrollTextUpDownBy, 0);
    XtAddCallback(result, XtNjumpProc, ScrollTextTo, 0);
    return (result);
}

void
ScrollBarReverseVideo(Widget scrollWidget)
{
    XtermWidget xw = getXtermWidget(scrollWidget);

    if (xw != 0) {
	SbInfo *sb = &(TScreenOf(xw)->fullVwin.sb_info);
	Arg args[4];
	Cardinal nargs = XtNumber(args);

	/*
	 * Remember the scrollbar's original colors.
	 */
	if (sb->rv_cached == False) {
	    XtSetArg(args[0], XtNbackground, &(sb->bg));
	    XtSetArg(args[1], XtNforeground, &(sb->fg));
	    XtSetArg(args[2], XtNborderColor, &(sb->bdr));
	    XtSetArg(args[3], XtNborderPixmap, &(sb->bdpix));
	    XtGetValues(scrollWidget, args, nargs);
	    sb->rv_cached = True;
	    sb->rv_active = 0;
	}

	sb->rv_active = !(sb->rv_active);
	if (sb->rv_active) {
	    XtSetArg(args[0], XtNbackground, sb->fg);
	    XtSetArg(args[1], XtNforeground, sb->bg);
	} else {
	    XtSetArg(args[0], XtNbackground, sb->bg);
	    XtSetArg(args[1], XtNforeground, sb->fg);
	}
	nargs = 2;		/* don't set border_pixmap */
	if (sb->bdpix == XtUnspecifiedPixmap) {
	    /* if not pixmap then pixel */
	    if (sb->rv_active) {
		/* keep border visible */
		XtSetArg(args[2], XtNborderColor, args[1].value);
	    } else {
		XtSetArg(args[2], XtNborderColor, sb->bdr);
	    }
	    nargs = 3;
	}
	XtSetValues(scrollWidget, args, nargs);
    }
}

void
ScrollBarDrawThumb(Widget scrollWidget)
{
    XtermWidget xw = getXtermWidget(scrollWidget);

    if (xw != 0) {
	TScreen *screen = TScreenOf(xw);
	int thumbTop, thumbHeight, totalHeight;

	thumbTop = ROW2INX(screen, screen->savedlines);
	thumbHeight = MaxRows(screen);
	totalHeight = thumbHeight + screen->savedlines;

	XawScrollbarSetThumb(scrollWidget,
			     ((float) thumbTop) / (float) totalHeight,
			     ((float) thumbHeight) / (float) totalHeight);
    }
}

void
ResizeScrollBar(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->scrollWidget != 0) {
	int height = screen->fullVwin.height + screen->border * 2;
	int width = screen->scrollWidget->core.width;
	int ypos = -ScrollBarBorder(xw);
#ifdef SCROLLBAR_RIGHT
	int xpos = ((xw->misc.useRight)
		    ? (screen->fullVwin.fullwidth -
		       screen->scrollWidget->core.width -
		       BorderWidth(screen->scrollWidget))
		    : -ScrollBarBorder(xw));
#else
	int xpos = -ScrollBarBorder(xw);
#endif

	TRACE(("ResizeScrollBar at %d,%d %dx%d\n", ypos, xpos, height, width));

	XtConfigureWidget(
			     screen->scrollWidget,
			     (Position) xpos,
			     (Position) ypos,
			     (Dimension) width,
			     (Dimension) height,
			     BorderWidth(screen->scrollWidget));
	ScrollBarDrawThumb(screen->scrollWidget);
    }
}

void
WindowScroll(XtermWidget xw, int top, Bool always GCC_UNUSED)
{
    TScreen *screen = TScreenOf(xw);

#if OPT_SCROLL_LOCK
    if (screen->allowScrollLock && (screen->scroll_lock && !always)) {
	if (screen->scroll_dirty) {
	    screen->scroll_dirty = False;
	    ScrnRefresh(xw, 0, 0, MaxRows(screen), MaxCols(screen), False);
	}
    } else
#endif
    {
	int i;

	if (top < -screen->savedlines) {
	    top = -screen->savedlines;
	} else if (top > 0) {
	    top = 0;
	}

	if ((i = screen->topline - top) != 0) {
	    int lines;
	    int scrolltop, scrollheight, refreshtop;

	    if (screen->cursor_state)
		HideCursor();
	    lines = i > 0 ? i : -i;
	    if (lines > MaxRows(screen))
		lines = MaxRows(screen);
	    scrollheight = screen->max_row - lines + 1;
	    if (i > 0)
		refreshtop = scrolltop = 0;
	    else {
		scrolltop = lines;
		refreshtop = scrollheight;
	    }
	    scrolling_copy_area(xw, scrolltop, scrollheight, -i);
	    screen->topline = top;

	    ScrollSelection(screen, i, True);

#if OPT_DOUBLE_BUFFER
	    XFillRectangle(screen->display,
			   VDrawable(screen),
			   ReverseGC(xw, screen),
			   OriginX(screen),
			   OriginY(screen) + refreshtop * FontHeight(screen),
			   (unsigned) Width(screen),
			   (unsigned) (lines * FontHeight(screen)));
#else
	    XClearArea(screen->display,
		       VWindow(screen),
		       OriginX(screen),
		       OriginY(screen) + refreshtop * FontHeight(screen),
		       (unsigned) Width(screen),
		       (unsigned) (lines * FontHeight(screen)),
		       False);
#endif
	    ScrnRefresh(xw, refreshtop, 0, lines, MaxCols(screen), False);

#if OPT_BLINK_CURS || OPT_BLINK_TEXT
	    RestartBlinking(screen);
#endif
	}
    }
    ScrollBarDrawThumb(screen->scrollWidget);
}

#ifdef SCROLLBAR_RIGHT
/*
 * Adjust the scrollbar position if we're asked to turn on scrollbars for the
 * first time (or after resizing) after the xterm is already running.  That
 * makes the window grow after we've initially configured the scrollbar's
 * position.  (There must be a better way).
 */
void
updateRightScrollbar(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (xw->misc.useRight
	&& screen->fullVwin.fullwidth < xw->core.width)
	XtVaSetValues(screen->scrollWidget,
		      XtNx, screen->fullVwin.fullwidth - BorderWidth(screen->scrollWidget),
		      (XtPointer) 0);
}
#endif

void
ScrollBarOn(XtermWidget xw, Bool init)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->fullVwin.sb_info.width || IsIcon(screen))
	return;

    TRACE(("ScrollBarOn(init %s)\n", BtoS(init)));
    if (init) {			/* then create it only */
	if (screen->scrollWidget == 0) {
	    /* make it a dummy size and resize later */
	    screen->scrollWidget = CreateScrollBar(xw,
						   -ScrollBarBorder(xw),
						   -ScrollBarBorder(xw),
						   5);
	    if (screen->scrollWidget == NULL) {
		Bell(xw, XkbBI_MinorError, 0);
	    }
	}
    } else if (!screen->scrollWidget || !XtIsRealized((Widget) xw)) {
	Bell(xw, XkbBI_MinorError, 0);
	Bell(xw, XkbBI_MinorError, 0);
    } else {

	ResizeScrollBar(xw);
	xtermAddInput(screen->scrollWidget);
	XtRealizeWidget(screen->scrollWidget);
	TRACE_TRANS("scrollbar", screen->scrollWidget);

	screen->fullVwin.sb_info.rv_cached = False;

	screen->fullVwin.sb_info.width = (screen->scrollWidget->core.width
					  + BorderWidth(screen->scrollWidget));

	TRACE(("setting scrollbar width %d = %d + %d\n",
	       screen->fullVwin.sb_info.width,
	       screen->scrollWidget->core.width,
	       BorderWidth(screen->scrollWidget)));

	ScrollBarDrawThumb(screen->scrollWidget);
	DoResizeScreen(xw);

#ifdef SCROLLBAR_RIGHT
	updateRightScrollbar(xw);
#endif

	XtMapWidget(screen->scrollWidget);
	update_scrollbar();
	if (screen->visbuf) {
	    xtermClear(xw);
	    Redraw();
	}
    }
}

void
ScrollBarOff(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (!screen->fullVwin.sb_info.width || IsIcon(screen))
	return;

    TRACE(("ScrollBarOff\n"));
    if (XtIsRealized((Widget) xw)) {
	XtUnmapWidget(screen->scrollWidget);
	screen->fullVwin.sb_info.width = 0;
	DoResizeScreen(xw);
	update_scrollbar();
	if (screen->visbuf) {
	    xtermClear(xw);
	    Redraw();
	}
    } else {
	Bell(xw, XkbBI_MinorError, 0);
    }
}

/*
 * Toggle the visibility of the scrollbars.
 */
void
ToggleScrollBar(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (IsIcon(screen)) {
	Bell(xw, XkbBI_MinorError, 0);
    } else {
	TRACE(("ToggleScrollBar{{\n"));
	if (screen->fullVwin.sb_info.width) {
	    ScrollBarOff(xw);
	} else {
	    ScrollBarOn(xw, False);
	}
	update_scrollbar();
	TRACE(("...ToggleScrollBar}}\n"));
    }
}

/*ARGSUSED*/
static void
ScrollTextTo(
		Widget scrollbarWidget,
		XtPointer client_data GCC_UNUSED,
		XtPointer call_data)
{
    XtermWidget xw = getXtermWidget(scrollbarWidget);

    if (xw != 0) {
	float *topPercent = (float *) call_data;
	TScreen *screen = TScreenOf(xw);
	int thumbTop;		/* relative to first saved line */
	int newTopLine;

	/*
	 * screen->savedlines : Number of offscreen text lines,
	 * MaxRows(screen)    : Number of onscreen  text lines,
	 */
	thumbTop = (int) (*topPercent
			  * (float) (screen->savedlines + MaxRows(screen)));
	newTopLine = thumbTop - screen->savedlines;
	WindowScroll(xw, newTopLine, True);
    }
}

/*ARGSUSED*/
static void
ScrollTextUpDownBy(
		      Widget scrollbarWidget,
		      XtPointer client_data GCC_UNUSED,
		      XtPointer call_data)
{
    XtermWidget xw = getXtermWidget(scrollbarWidget);

    if (xw != 0) {
	long pixels = (long) call_data;

	TScreen *screen = TScreenOf(xw);
	int rowOnScreen, newTopLine;

	rowOnScreen = (int) (pixels / FontHeight(screen));
	if (rowOnScreen == 0) {
	    if (pixels < 0)
		rowOnScreen = -1;
	    else if (pixels > 0)
		rowOnScreen = 1;
	}
	newTopLine = ROW2INX(screen, rowOnScreen);
	WindowScroll(xw, newTopLine, True);
    }
}

/*
 * assume that b is alphabetic and allow plural
 */
static int
CompareWidths(const char *a, const char *b, int *modifier)
{
    int result;
    char ca, cb;

    *modifier = 0;
    if (!a || !b)
	return 0;

    for (;;) {
	ca = x_toupper(*a);
	cb = x_toupper(*b);
	if (ca != cb || ca == '\0')
	    break;		/* if not eq else both nul */
	a++, b++;
    }
    if (cb != '\0')
	return 0;

    if (ca == 'S')
	ca = *++a;

    switch (ca) {
    case '+':
    case '-':
	*modifier = (ca == '-' ? -1 : 1) * atoi(a + 1);
	result = 1;
	break;

    case '\0':
	result = 1;
	break;

    default:
	result = 0;
	break;
    }
    return result;
}

static long
params_to_pixels(TScreen *screen, String *params, Cardinal n)
{
    int mult = 1;
    const char *s;
    int modifier;

    switch (n > 2 ? 2 : n) {
    case 2:
	s = params[1];
	if (CompareWidths(s, "PAGE", &modifier)) {
	    mult = (MaxRows(screen) + modifier) * FontHeight(screen);
	} else if (CompareWidths(s, "HALFPAGE", &modifier)) {
	    mult = ((MaxRows(screen) + modifier) * FontHeight(screen)) / 2;
	} else if (CompareWidths(s, "PIXEL", &modifier)) {
	    mult = 1;
	} else {
	    /* else assume that it is Line */
	    mult = FontHeight(screen);
	}
	mult *= atoi(params[0]);
	TRACE(("params_to_pixels(%s,%s) = %d\n", params[0], params[1], mult));
	break;
    case 1:
	mult = atoi(params[0]) * FontHeight(screen);	/* lines */
	TRACE(("params_to_pixels(%s) = %d\n", params[0], mult));
	break;
    default:
	mult = screen->scrolllines * FontHeight(screen);
	TRACE(("params_to_pixels() = %d\n", mult));
	break;
    }
    return mult;
}

static long
AmountToScroll(Widget w, String *params, Cardinal nparams)
{
    long result = 0;
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	TScreen *screen = TScreenOf(xw);
	if (nparams <= 2
	    || screen->send_mouse_pos == MOUSE_OFF) {
	    result = params_to_pixels(screen, params, nparams);
	}
    }
    return result;
}

static void
AlternateScroll(Widget w, long amount)
{
    XtermWidget xw;
    TScreen *screen;

    if ((xw = getXtermWidget(w)) != 0 &&
	(screen = TScreenOf(xw)) != 0 &&
	screen->alternateScroll && screen->whichBuf) {
	ANSI reply;

	amount /= FontHeight(screen);
	memset(&reply, 0, sizeof(reply));
	reply.a_type = ((xw->keyboard.flags & MODE_DECCKM)
			? ANSI_SS3
			: ANSI_CSI);
	if (amount > 0) {
	    reply.a_final = 'B';
	} else {
	    amount = -amount;
	    reply.a_final = 'A';
	}
	while (amount-- > 0) {
	    unparseseq(xw, &reply);
	}
    } else {
	ScrollTextUpDownBy(w, (XtPointer) 0, (XtPointer) amount);
    }
}

/*ARGSUSED*/
void
HandleScrollTo(
		  Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *nparams)
{
    XtermWidget xw;
    TScreen *screen;

    if ((xw = getXtermWidget(w)) != 0 &&
	(screen = TScreenOf(xw)) != 0 &&
	*nparams > 0) {
	long amount;
	int value;
	int to_top = (screen->topline - screen->savedlines);
	if (!x_strcasecmp(params[0], "begin")) {
	    amount = to_top * FontHeight(screen);
	} else if (!x_strcasecmp(params[0], "end")) {
	    amount = -to_top * FontHeight(screen);
	} else if ((value = atoi(params[0])) >= 0) {
	    amount = (value + to_top) * FontHeight(screen);
	} else {
	    amount = 0;
	}
	AlternateScroll(w, amount);
    }
}

/*ARGSUSED*/
void
HandleScrollForward(
		       Widget xw,
		       XEvent *event GCC_UNUSED,
		       String *params,
		       Cardinal *nparams)
{
    long amount;

    if ((amount = AmountToScroll(xw, params, *nparams)) != 0) {
	AlternateScroll(xw, amount);
    }
}

/*ARGSUSED*/
void
HandleScrollBack(
		    Widget xw,
		    XEvent *event GCC_UNUSED,
		    String *params,
		    Cardinal *nparams)
{
    long amount;

    if ((amount = -AmountToScroll(xw, params, *nparams)) != 0) {
	AlternateScroll(xw, amount);
    }
}

#if OPT_SCROLL_LOCK
#define SCROLL_LOCK_LED 3

#ifdef HAVE_XKBQUERYEXTENSION
/*
 * Check for Xkb on client and server.
 */
static int
have_xkb(Display *dpy)
{
    static int initialized = -1;

    if (initialized < 0) {
	int xkbmajor = XkbMajorVersion;
	int xkbminor = XkbMinorVersion;
	int xkbopcode, xkbevent, xkberror;

	initialized = 0;
	if (XkbLibraryVersion(&xkbmajor, &xkbminor)
	    && XkbQueryExtension(dpy,
				 &xkbopcode,
				 &xkbevent,
				 &xkberror,
				 &xkbmajor,
				 &xkbminor)) {
	    TRACE(("we have Xkb\n"));
	    initialized = 1;
#if OPT_TRACE
	    {
		XkbDescPtr xkb;
		unsigned int mask;

		xkb = XkbGetKeyboard(dpy, XkbAllComponentsMask, XkbUseCoreKbd);
		if (xkb != NULL) {
		    int n;

		    TRACE(("XkbGetKeyboard ok\n"));
		    for (n = 0; n < XkbNumVirtualMods; ++n) {
			if (xkb->names->vmods[n] != 0) {
			    char *modStr = XGetAtomName(xkb->dpy,
							xkb->names->vmods[n]);
			    if (modStr != 0) {
				XkbVirtualModsToReal(xkb,
						     (unsigned) (1 << n),
						     &mask);
				TRACE(("  name[%d] %s (%#x)\n", n, modStr, mask));
			    }
			}
		    }
		    XkbFreeKeyboard(xkb, 0, True);
		}
	    }
#endif
	}
    }
    return initialized;
}

static Boolean
getXkbLED(Display *dpy, const char *name, Boolean *result)
{
    Atom my_atom;
    Boolean success = False;
    Bool state;

    if (have_xkb(dpy)) {
	my_atom = XInternAtom(dpy, name, False);
	if ((my_atom != None) &&
	    XkbGetNamedIndicator(dpy, my_atom, NULL, &state, NULL, NULL)) {
	    *result = (Boolean) state;
	    success = True;
	}
    }

    return success;
}

/*
 * Use Xkb if we have it (still unreliable, but slightly better than hardcoded).
 */
static Boolean
showXkbLED(Display *dpy, const char *name, Bool enable)
{
    Atom my_atom;
    Boolean result = False;

    if (have_xkb(dpy)) {
	my_atom = XInternAtom(dpy, name, False);
	if ((my_atom != None) &&
	    XkbGetNamedIndicator(dpy, my_atom, NULL, NULL, NULL, NULL) &&
	    XkbSetNamedIndicator(dpy, my_atom, True, enable, False, NULL)) {
	    result = True;
	}
    }

    return result;
}
#endif

/*
 * xlsatoms agrees with this list.  However Num/Caps lock are generally
 * unusable due to special treatment in X.  They are used here for
 * completeness.
 */
static const char *led_table[] =
{
    "Num Lock",
    "Caps Lock",
    "Scroll Lock"
};

static Boolean
xtermGetLED(TScreen *screen, Cardinal led_number)
{
    Display *dpy = screen->display;
    Boolean result = False;

#ifdef HAVE_XKBQUERYEXTENSION
    if (!getXkbLED(dpy, led_table[led_number - 1], &result))
#endif
    {
	XKeyboardState state;
	unsigned long my_bit = (unsigned long) (1 << (led_number - 1));

	XGetKeyboardControl(dpy, &state);

	result = (Boolean) ((state.led_mask & my_bit) != 0);
    }

    TRACE(("xtermGetLED %d:%s\n", led_number, BtoS(result)));
    return result;
}

/*
 * Display the given LED, preferably independent of keyboard state.
 */
void
xtermShowLED(TScreen *screen, Cardinal led_number, Bool enable)
{
    TRACE(("xtermShowLED %d:%s\n", led_number, BtoS(enable)));
    if ((led_number >= 1) && (led_number <= XtNumber(led_table))) {
	Display *dpy = screen->display;

#ifdef HAVE_XKBQUERYEXTENSION
	if (!showXkbLED(dpy, led_table[led_number - 1], enable))
#endif
	{
	    XKeyboardState state;
	    XKeyboardControl values;
	    unsigned long use_mask;
	    unsigned long my_bit = (unsigned long) (1 << (led_number - 1));

	    XGetKeyboardControl(dpy, &state);
	    use_mask = state.led_mask;
	    if (enable) {
		use_mask |= my_bit;
	    } else {
		use_mask &= ~my_bit;
	    }

	    if (state.led_mask != use_mask) {
		values.led = (int) led_number;
		values.led_mode = enable;
		XChangeKeyboardControl(dpy, KBLed | KBLedMode, &values);
	    }
	}
    }
}

void
xtermClearLEDs(TScreen *screen)
{
    Display *dpy = screen->display;
    XKeyboardControl values;

    TRACE(("xtermClearLEDs\n"));
#ifdef HAVE_XKBQUERYEXTENSION
    ShowScrollLock(screen, False);
#endif
    memset(&values, 0, sizeof(values));
    XChangeKeyboardControl(dpy, KBLedMode, &values);
}

void
ShowScrollLock(TScreen *screen, Bool enable)
{
    xtermShowLED(screen, SCROLL_LOCK_LED, enable);
}

void
GetScrollLock(TScreen *screen)
{
    if (screen->allowScrollLock)
	screen->scroll_lock = xtermGetLED(screen, SCROLL_LOCK_LED);
}

void
SetScrollLock(TScreen *screen, Bool enable)
{
    if (screen->allowScrollLock) {
	if (screen->scroll_lock != enable) {
	    TRACE(("SetScrollLock %s\n", BtoS(enable)));
	    screen->scroll_lock = (Boolean) enable;
	    ShowScrollLock(screen, enable);
	}
    }
}

/* ARGSUSED */
void
HandleScrollLock(Widget w,
		 XEvent *event GCC_UNUSED,
		 String *params,
		 Cardinal *param_count)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	TScreen *screen = TScreenOf(xw);

	if (screen->allowScrollLock) {

	    switch (decodeToggle(xw, params, *param_count)) {
	    case toggleOff:
		SetScrollLock(screen, False);
		break;
	    case toggleOn:
		SetScrollLock(screen, True);
		break;
	    case toggleAll:
		SetScrollLock(screen, !screen->scroll_lock);
		break;
	    }
	}
    }
}
#endif
