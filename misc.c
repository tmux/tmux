/* $XTermId: misc.c,v 1.785 2017/12/26 11:42:24 tom Exp $ */

/*
 * Copyright 1999-2016,2017 by Thomas E. Dickey
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

#include <version.h>
#include <main.h>
#include <xterm.h>
#include <xterm_io.h>

#include <sys/stat.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/wait.h>

#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Xlocale.h>

#include <X11/Xmu/Error.h>
#include <X11/Xmu/SysUtil.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/Xmu/Xmu.h>
#if HAVE_X11_SUNKEYSYM_H
#include <X11/Sunkeysym.h>
#endif

#ifdef HAVE_LIBXPM
#include <X11/xpm.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#endif

#include <xutf8.h>

#include <data.h>
#include <error.h>
#include <menu.h>
#include <fontutils.h>
#include <xstrings.h>
#include <xtermcap.h>
#include <VTparse.h>
#include <graphics.h>
#include <graphics_regis.h>
#include <graphics_sixel.h>

#include <assert.h>

#if (XtSpecificationRelease < 6)
#ifndef X_GETTIMEOFDAY
#define X_GETTIMEOFDAY(t) gettimeofday(t,(struct timezone *)0)
#endif
#endif

#ifdef VMS
#define XTERM_VMS_LOGFILE "SYS$SCRATCH:XTERM_LOG.TXT"
#ifdef ALLOWLOGFILEEXEC
#undef ALLOWLOGFILEEXEC
#endif
#endif /* VMS */

#if OPT_TEK4014
#define OUR_EVENT(event,Type) \
		(event.type == Type && \
		  (event.xcrossing.window == XtWindow(XtParent(xw)) || \
		    (tekWidget && \
		     event.xcrossing.window == XtWindow(XtParent(tekWidget)))))
#else
#define OUR_EVENT(event,Type) \
		(event.type == Type && \
		   (event.xcrossing.window == XtWindow(XtParent(xw))))
#endif

#define VB_DELAY    screen->visualBellDelay
#define EVENT_DELAY TScreenOf(term)->nextEventDelay

static Boolean xtermAllocColor(XtermWidget, XColor *, const char *);
static Cursor make_hidden_cursor(XtermWidget);

static char emptyString[] = "";

#if OPT_EXEC_XTERM
/* Like readlink(2), but returns a malloc()ed buffer, or NULL on
   error; adapted from libc docs */
static char *
Readlink(const char *filename)
{
    char *buf = NULL;
    size_t size = 100;

    for (;;) {
	int n;
	char *tmp = TypeRealloc(char, size, buf);
	if (tmp == NULL) {
	    free(buf);
	    return NULL;
	}
	buf = tmp;
	memset(buf, 0, size);

	n = (int) readlink(filename, buf, size);
	if (n < 0) {
	    free(buf);
	    return NULL;
	}

	if ((unsigned) n < size) {
	    return buf;
	}

	size *= 2;
    }
}
#endif /* OPT_EXEC_XTERM */

static void
Sleep(int msec)
{
    static struct timeval select_timeout;

    select_timeout.tv_sec = 0;
    select_timeout.tv_usec = msec * 1000;
    select(0, 0, 0, 0, &select_timeout);
}

static void
selectwindow(XtermWidget xw, int flag)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("selectwindow(%d) flag=%d\n", screen->select, flag));

#if OPT_TEK4014
    if (TEK4014_ACTIVE(xw)) {
	if (!Ttoggled)
	    TCursorToggle(tekWidget, TOGGLE);
	screen->select |= flag;
	if (!Ttoggled)
	    TCursorToggle(tekWidget, TOGGLE);
    } else
#endif
    {
#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
	TInput *input = lookupTInput(xw, (Widget) xw);
	if (input && input->xic)
	    XSetICFocus(input->xic);
#endif

	if (screen->cursor_state && CursorMoved(screen))
	    HideCursor();
	screen->select |= flag;
	if (screen->cursor_state)
	    ShowCursor();
    }
    GetScrollLock(screen);
}

static void
unselectwindow(XtermWidget xw, int flag)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("unselectwindow(%d) flag=%d\n", screen->select, flag));

    if (screen->hide_pointer && screen->pointer_mode < pFocused) {
	screen->hide_pointer = False;
	xtermDisplayCursor(xw);
    }

    if (!screen->always_highlight) {
#if OPT_TEK4014
	if (TEK4014_ACTIVE(xw)) {
	    if (!Ttoggled)
		TCursorToggle(tekWidget, TOGGLE);
	    screen->select &= ~flag;
	    if (!Ttoggled)
		TCursorToggle(tekWidget, TOGGLE);
	} else
#endif
	{
#if OPT_I18N_SUPPORT && OPT_INPUT_METHOD
	    TInput *input = lookupTInput(xw, (Widget) xw);
	    if (input && input->xic)
		XUnsetICFocus(input->xic);
#endif

	    screen->select &= ~flag;
	    if (screen->cursor_state && CursorMoved(screen))
		HideCursor();
	    if (screen->cursor_state)
		ShowCursor();
	}
    }
}

static void
DoSpecialEnterNotify(XtermWidget xw, XEnterWindowEvent *ev)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("DoSpecialEnterNotify(%d)\n", screen->select));
    TRACE_FOCUS(xw, ev);
    if (((ev->detail) != NotifyInferior) &&
	ev->focus &&
	!(screen->select & FOCUS))
	selectwindow(xw, INWINDOW);
}

static void
DoSpecialLeaveNotify(XtermWidget xw, XEnterWindowEvent *ev)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("DoSpecialLeaveNotify(%d)\n", screen->select));
    TRACE_FOCUS(xw, ev);
    if (((ev->detail) != NotifyInferior) &&
	ev->focus &&
	!(screen->select & FOCUS))
	unselectwindow(xw, INWINDOW);
}

#ifndef XUrgencyHint
#define XUrgencyHint (1L << 8)	/* X11R5 does not define */
#endif

static void
setXUrgency(XtermWidget xw, Bool enable)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->bellIsUrgent) {
	XWMHints *h = XGetWMHints(screen->display, VShellWindow(xw));
	if (h != 0) {
	    if (enable && !(screen->select & FOCUS)) {
		h->flags |= XUrgencyHint;
	    } else {
		h->flags &= ~XUrgencyHint;
	    }
	    XSetWMHints(screen->display, VShellWindow(xw), h);
	}
    }
}

void
do_xevents(void)
{
    TScreen *screen = TScreenOf(term);

    if (xtermAppPending()
	||
#if defined(VMS) || defined(__VMS)
	screen->display->qlen > 0
#else
	GetBytesAvailable(ConnectionNumber(screen->display)) > 0
#endif
	)
	xevents();
}

void
xtermDisplayCursor(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->Vshow) {
	if (screen->hide_pointer) {
	    TRACE(("Display hidden_cursor\n"));
	    XDefineCursor(screen->display, VWindow(screen), screen->hidden_cursor);
	} else {
	    TRACE(("Display pointer_cursor\n"));
	    recolor_cursor(screen,
			   screen->pointer_cursor,
			   T_COLOR(screen, MOUSE_FG),
			   T_COLOR(screen, MOUSE_BG));
	    XDefineCursor(screen->display, VWindow(screen), screen->pointer_cursor);
	}
    }
}

void
xtermShowPointer(XtermWidget xw, Bool enable)
{
    static int tried = -1;
    TScreen *screen = TScreenOf(xw);

#if OPT_TEK4014
    if (TEK4014_SHOWN(xw))
	enable = True;
#endif

    /*
     * Whether we actually hide the pointer depends on the pointer-mode and
     * the mouse-mode:
     */
    if (!enable) {
	switch (screen->pointer_mode) {
	case pNever:
	    enable = True;
	    break;
	case pNoMouse:
	    if (screen->send_mouse_pos != MOUSE_OFF)
		enable = True;
	    break;
	case pAlways:
	case pFocused:
	    break;
	}
    }

    if (enable) {
	if (screen->hide_pointer) {
	    screen->hide_pointer = False;
	    xtermDisplayCursor(xw);
	    switch (screen->send_mouse_pos) {
	    case ANY_EVENT_MOUSE:
		break;
	    default:
		MotionOff(screen, xw);
		break;
	    }
	}
    } else if (!(screen->hide_pointer) && (tried <= 0)) {
	if (screen->hidden_cursor == 0) {
	    screen->hidden_cursor = make_hidden_cursor(xw);
	}
	if (screen->hidden_cursor == 0) {
	    tried = 1;
	} else {
	    tried = 0;
	    screen->hide_pointer = True;
	    xtermDisplayCursor(xw);
	    MotionOn(screen, xw);
	}
    }
}

#if OPT_TRACE
static void
TraceExposeEvent(XEvent *arg)
{
    XExposeEvent *event = (XExposeEvent *) arg;

    TRACE(("pending Expose %ld %d: %d,%d %dx%d %#lx\n",
	   event->serial,
	   event->count,
	   event->y,
	   event->x,
	   event->height,
	   event->width,
	   event->window));
}

#else
#define TraceExposeEvent(event)	/* nothing */
#endif

/* true if p contains q */
#define ExposeContains(p,q) \
	    ((p)->y <= (q)->y \
	  && (p)->x <= (q)->x \
	  && ((p)->y + (p)->height) >= ((q)->y + (q)->height) \
	  && ((p)->x + (p)->width) >= ((q)->x + (q)->width))

static XtInputMask
mergeExposeEvents(XEvent *target)
{
    XEvent next_event;
    XExposeEvent *p;

    TRACE(("pending Expose...?\n"));
    TraceExposeEvent(target);
    XtAppNextEvent(app_con, target);
    p = (XExposeEvent *) target;

    while (XtAppPending(app_con)
	   && XtAppPeekEvent(app_con, &next_event)
	   && next_event.type == Expose) {
	Boolean merge_this = False;
	XExposeEvent *q;

	TraceExposeEvent(&next_event);
	q = (XExposeEvent *) (&next_event);
	XtAppNextEvent(app_con, &next_event);

	/*
	 * If either window is contained within the other, merge the events.
	 * The traces show that there are also cases where a full repaint of
	 * a window is broken into 3 or more rectangles, which do not arrive
	 * in the same instant.  We could merge those if xterm were modified
	 * to skim several events ahead.
	 */
	if (p->window == q->window) {
	    if (ExposeContains(p, q)) {
		TRACE(("pending Expose...merged forward\n"));
		merge_this = True;
		next_event = *target;
	    } else if (ExposeContains(q, p)) {
		TRACE(("pending Expose...merged backward\n"));
		merge_this = True;
	    }
	}
	if (!merge_this) {
	    XtDispatchEvent(target);
	}
	*target = next_event;
    }
    XtDispatchEvent(target);
    return XtAppPending(app_con);
}

#if OPT_TRACE
static void
TraceConfigureEvent(XEvent *arg)
{
    XConfigureEvent *event = (XConfigureEvent *) arg;

    TRACE(("pending Configure %ld %d,%d %dx%d %#lx\n",
	   event->serial,
	   event->y,
	   event->x,
	   event->height,
	   event->width,
	   event->window));
}

#else
#define TraceConfigureEvent(event)	/* nothing */
#endif

/*
 * On entry, we have peeked at the event queue and see a configure-notify
 * event.  Remove that from the queue so we can look further.
 *
 * Then, as long as there is a configure-notify event in the queue, remove
 * that.  If the adjacent events are for different windows, process the older
 * event and update the event used for comparing windows.  If they are for the
 * same window, only the newer event is of interest.
 *
 * Finally, process the (remaining) configure-notify event.
 */
static XtInputMask
mergeConfigureEvents(XEvent *target)
{
    XEvent next_event;
    XConfigureEvent *p;

    XtAppNextEvent(app_con, target);
    p = (XConfigureEvent *) target;

    TRACE(("pending Configure...?%s\n", XtAppPending(app_con) ? "yes" : "no"));
    TraceConfigureEvent(target);

    if (XtAppPending(app_con)
	&& XtAppPeekEvent(app_con, &next_event)
	&& next_event.type == ConfigureNotify) {
	Boolean merge_this = False;
	XConfigureEvent *q;

	TraceConfigureEvent(&next_event);
	XtAppNextEvent(app_con, &next_event);
	q = (XConfigureEvent *) (&next_event);

	if (p->window == q->window) {
	    TRACE(("pending Configure...merged\n"));
	    merge_this = True;
	}
	if (!merge_this) {
	    TRACE(("pending Configure...skipped\n"));
	    XtDispatchEvent(target);
	}
	*target = next_event;
    }
    XtDispatchEvent(target);
    return XtAppPending(app_con);
}

/*
 * Filter redundant Expose- and ConfigureNotify-events.  This is limited to
 * adjacent events because there could be other event-loop processing.  Absent
 * that limitation, it might be possible to scan ahead to find when the screen
 * would be completely updated, skipping unnecessary re-repainting before that
 * point.
 *
 * Note: all cases should allow doing XtAppNextEvent if result is true.
 */
XtInputMask
xtermAppPending(void)
{
    XtInputMask result = XtAppPending(app_con);
    XEvent this_event;
    Boolean found = False;

    while (result && XtAppPeekEvent(app_con, &this_event)) {
	found = True;
	if (this_event.type == Expose) {
	    result = mergeExposeEvents(&this_event);
	    TRACE(("got merged expose events\n"));
	} else if (this_event.type == ConfigureNotify) {
	    result = mergeConfigureEvents(&this_event);
	    TRACE(("got merged configure notify events\n"));
	} else {
	    TRACE(("pending %s\n", visibleEventType(this_event.type)));
	    break;
	}
    }

    /*
     * With NetBSD, closing a shell results in closing the X input event
     * stream, which interferes with the "-hold" option.  Wait a short time in
     * this case, to avoid max'ing the CPU.
     */
    if (hold_screen && caught_intr && !found) {
	Sleep(EVENT_DELAY);
    }
    return result;
}

void
xevents(void)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);
    XEvent event;
    XtInputMask input_mask;

    if (need_cleanup)
	NormalExit();

    if (screen->scroll_amt)
	FlushScroll(xw);
    /*
     * process timeouts, relying on the fact that XtAppProcessEvent
     * will process the timeout and return without blockng on the
     * XEvent queue.  Other sources i.e., the pty are handled elsewhere
     * with select().
     */
    while ((input_mask = xtermAppPending()) != 0) {
	if (input_mask & XtIMTimer)
	    XtAppProcessEvent(app_con, (XtInputMask) XtIMTimer);
#if OPT_SESSION_MGT
	/*
	 * Session management events are alternative input events. Deal with
	 * them in the same way.
	 */
	else if (input_mask & XtIMAlternateInput)
	    XtAppProcessEvent(app_con, (XtInputMask) XtIMAlternateInput);
#endif
	else
	    break;
    }

    /*
     * If there's no XEvents, don't wait around...
     */
    if ((input_mask & XtIMXEvent) != XtIMXEvent)
	return;
    do {
	/*
	 * This check makes xterm hang when in mouse hilite tracking mode.
	 * We simply ignore all events except for those not passed down to
	 * this function, e.g., those handled in in_put().
	 */
	if (screen->waitingForTrackInfo) {
	    Sleep(EVENT_DELAY);
	    return;
	}
	XtAppNextEvent(app_con, &event);
	/*
	 * Hack to get around problems with the toolkit throwing away
	 * eventing during the exclusive grab of the menu popup.  By
	 * looking at the event ourselves we make sure that we can
	 * do the right thing.
	 */
	if (OUR_EVENT(event, EnterNotify)) {
	    DoSpecialEnterNotify(xw, &event.xcrossing);
	} else if (OUR_EVENT(event, LeaveNotify)) {
	    DoSpecialLeaveNotify(xw, &event.xcrossing);
	} else if ((screen->send_mouse_pos == ANY_EVENT_MOUSE
#if OPT_DEC_LOCATOR
		    || screen->send_mouse_pos == DEC_LOCATOR
#endif /* OPT_DEC_LOCATOR */
		   )
		   && event.xany.type == MotionNotify
		   && event.xcrossing.window == XtWindow(xw)) {
	    SendMousePosition(xw, &event);
	    xtermShowPointer(xw, True);
	    continue;
	}

	/*
	 * If the event is interesting (and not a keyboard event), turn the
	 * mouse pointer back on.
	 */
	if (screen->hide_pointer) {
	    if (screen->pointer_mode >= pFocused) {
		switch (event.xany.type) {
		case MotionNotify:
		    xtermShowPointer(xw, True);
		    break;
		}
	    } else {
		switch (event.xany.type) {
		case KeyPress:
		case KeyRelease:
		case ButtonPress:
		case ButtonRelease:
		    /* also these... */
		case Expose:
		case GraphicsExpose:
		case NoExpose:
		case PropertyNotify:
		case ClientMessage:
		    break;
		default:
		    xtermShowPointer(xw, True);
		    break;
		}
	    }
	}

	if (!event.xany.send_event ||
	    screen->allowSendEvents ||
	    ((event.xany.type != KeyPress) &&
	     (event.xany.type != KeyRelease) &&
	     (event.xany.type != ButtonPress) &&
	     (event.xany.type != ButtonRelease))) {

	    XtDispatchEvent(&event);
	}
    } while (xtermAppPending() & XtIMXEvent);
}

static Cursor
make_hidden_cursor(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    Cursor c;
    Display *dpy = screen->display;
    XFontStruct *fn;

    static XColor dummy;

    /*
     * Prefer nil2 (which is normally available) to "fixed" (which is supposed
     * to be "always" available), since it's a smaller glyph in case the
     * server insists on drawing _something_.
     */
    TRACE(("Ask for nil2 font\n"));
    if ((fn = XLoadQueryFont(dpy, "nil2")) == 0) {
	TRACE(("...Ask for fixed font\n"));
	fn = XLoadQueryFont(dpy, DEFFONT);
    }

    if (fn != 0) {
	/* a space character seems to work as a cursor (dots are not needed) */
	c = XCreateGlyphCursor(dpy, fn->fid, fn->fid, 'X', ' ', &dummy, &dummy);
	XFreeFont(dpy, fn);
    } else {
	c = 0;
    }
    TRACE(("XCreateGlyphCursor ->%#lx\n", c));
    return (c);
}

/*
 * Xlib uses Xcursor to customize cursor coloring, which interferes with
 * xterm's pointerColor resource.  Work around this by providing our own
 * default theme.  Testing seems to show that we only have to provide this
 * until the window is initialized.
 */
void
init_colored_cursor(Display *dpy)
{
#ifdef HAVE_LIB_XCURSOR
    static const char theme[] = "index.theme";
    static const char pattern[] = "xtermXXXXXX";
    char *env = getenv("XCURSOR_THEME");

    xterm_cursor_theme = 0;
    /*
     * The environment variable overrides a (possible) resource Xcursor.theme
     */
    if (IsEmpty(env)) {
	env = XGetDefault(dpy, "Xcursor", "theme");
    }
    /*
     * If neither found, provide our own default theme.
     */
    if (IsEmpty(env)) {
	const char *tmp_dir;
	char *filename;
	size_t needed;

	if ((tmp_dir = getenv("TMPDIR")) == 0) {
	    tmp_dir = P_tmpdir;
	}
	needed = strlen(tmp_dir) + 4 + strlen(theme) + strlen(pattern);
	if ((filename = malloc(needed)) != 0) {
	    sprintf(filename, "%s/%s", tmp_dir, pattern);

#ifdef HAVE_MKDTEMP
	    xterm_cursor_theme = mkdtemp(filename);
#else
	    if (mktemp(filename) != 0
		&& mkdir(filename, 0700) == 0) {
		xterm_cursor_theme = filename;
	    }
#endif
	    /*
	     * Actually, Xcursor does what _we_ want just by steering its
	     * search path away from home.  We are setting up the complete
	     * theme just in case the library ever acquires a maintainer.
	     */
	    if (xterm_cursor_theme != 0) {
		char *leaf = xterm_cursor_theme + strlen(xterm_cursor_theme);
		FILE *fp;

		strcat(leaf, "/");
		strcat(leaf, theme);
		if ((fp = fopen(xterm_cursor_theme, "w")) != 0) {
		    fprintf(fp, "[Icon Theme]\n");
		    fclose(fp);
		    *leaf = '\0';
		    xtermSetenv("XCURSOR_PATH", xterm_cursor_theme);
		    *leaf = '/';
		}
		atexit(cleanup_colored_cursor);
	    }
	}
    }
#else
    (void) dpy;
#endif /* HAVE_LIB_XCURSOR */
}

/*
 * Once done, discard the file and directory holding it.
 */
void
cleanup_colored_cursor(void)
{
#ifdef HAVE_LIB_XCURSOR
    if (xterm_cursor_theme != 0) {
	char *my_path = getenv("XCURSOR_PATH");
	struct stat sb;
	if (!IsEmpty(my_path)
	    && stat(my_path, &sb) == 0
	    && (sb.st_mode & S_IFMT) == S_IFDIR) {
	    unlink(xterm_cursor_theme);
	    rmdir(my_path);
	    free(xterm_cursor_theme);
	    xterm_cursor_theme = 0;
	}
    }
#endif /* HAVE_LIB_XCURSOR */
}

Cursor
make_colored_cursor(unsigned cursorindex,	/* index into font */
		    unsigned long fg,	/* pixel value */
		    unsigned long bg)	/* pixel value */
{
    TScreen *screen = TScreenOf(term);
    Cursor c;
    Display *dpy = screen->display;

    c = XCreateFontCursor(dpy, cursorindex);
    if (c != None) {
	recolor_cursor(screen, c, fg, bg);
    }
    return (c);
}

/* ARGSUSED */
void
HandleKeyPressed(Widget w GCC_UNUSED,
		 XEvent *event,
		 String *params GCC_UNUSED,
		 Cardinal *nparams GCC_UNUSED)
{
    TRACE(("Handle insert-seven-bit for %p\n", (void *) w));
    Input(term, &event->xkey, False);
}

/* ARGSUSED */
void
HandleEightBitKeyPressed(Widget w GCC_UNUSED,
			 XEvent *event,
			 String *params GCC_UNUSED,
			 Cardinal *nparams GCC_UNUSED)
{
    TRACE(("Handle insert-eight-bit for %p\n", (void *) w));
    Input(term, &event->xkey, True);
}

/* ARGSUSED */
void
HandleStringEvent(Widget w GCC_UNUSED,
		  XEvent *event GCC_UNUSED,
		  String *params,
		  Cardinal *nparams)
{

    if (*nparams != 1)
	return;

    if ((*params)[0] == '0' && (*params)[1] == 'x' && (*params)[2] != '\0') {
	const char *abcdef = "ABCDEF";
	const char *xxxxxx;
	Char c;
	UString p;
	unsigned value = 0;

	for (p = (UString) (*params + 2); (c = CharOf(x_toupper(*p))) !=
	     '\0'; p++) {
	    value *= 16;
	    if (c >= '0' && c <= '9')
		value += (unsigned) (c - '0');
	    else if ((xxxxxx = (strchr) (abcdef, c)) != 0)
		value += (unsigned) (xxxxxx - abcdef) + 10;
	    else
		break;
	}
	if (c == '\0') {
	    Char hexval[2];
	    hexval[0] = (Char) value;
	    hexval[1] = 0;
	    StringInput(term, hexval, (size_t) 1);
	}
    } else {
	StringInput(term, (const Char *) *params, strlen(*params));
    }
}

#if OPT_EXEC_XTERM

#ifndef PROCFS_ROOT
#define PROCFS_ROOT "/proc"
#endif

/*
 * Determine the current working directory of the child so that we can
 * spawn a new terminal in the same directory.
 *
 * If we cannot get the CWD of the child, just use our own.
 */
char *
ProcGetCWD(pid_t pid)
{
    char *child_cwd = NULL;

    if (pid) {
	char child_cwd_link[sizeof(PROCFS_ROOT) + 80];
	sprintf(child_cwd_link, PROCFS_ROOT "/%lu/cwd", (unsigned long) pid);
	child_cwd = Readlink(child_cwd_link);
    }
    return child_cwd;
}

/* ARGSUSED */
void
HandleSpawnTerminal(Widget w GCC_UNUSED,
		    XEvent *event GCC_UNUSED,
		    String *params,
		    Cardinal *nparams)
{
    TScreen *screen = TScreenOf(term);
    char *child_cwd = NULL;
    char *child_exe;
    pid_t pid;

    /*
     * Try to find the actual program which is running in the child process.
     * This works for Linux.  If we cannot find the program, fall back to the
     * xterm program (which is usually adequate).  Give up if we are given only
     * a relative path to xterm, since that would not always match $PATH.
     */
    child_exe = Readlink(PROCFS_ROOT "/self/exe");
    if (!child_exe) {
	if (strncmp(ProgramName, "./", (size_t) 2)
	    && strncmp(ProgramName, "../", (size_t) 3)) {
	    child_exe = xtermFindShell(ProgramName, True);
	} else {
	    xtermWarning("Cannot exec-xterm given \"%s\"\n", ProgramName);
	}
	if (child_exe == 0)
	    return;
    }

    child_cwd = ProcGetCWD(screen->pid);

    /* The reaper will take care of cleaning up the child */
    pid = fork();
    if (pid == -1) {
	xtermWarning("Could not fork: %s\n", SysErrorMsg(errno));
    } else if (!pid) {
	/* We are the child */
	if (child_cwd) {
	    IGNORE_RC(chdir(child_cwd));	/* We don't care if this fails */
	}

	if (setuid(screen->uid) == -1
	    || setgid(screen->gid) == -1) {
	    xtermWarning("Cannot reset uid/gid\n");
	} else {
	    unsigned myargc = *nparams + 1;
	    char **myargv = TypeMallocN(char *, myargc + 1);

	    if (myargv != 0) {
		unsigned n = 0;

		myargv[n++] = child_exe;

		while (n < myargc) {
		    myargv[n++] = (char *) *params++;
		}

		myargv[n] = 0;
		execv(child_exe, myargv);
	    }

	    /* If we get here, we've failed */
	    xtermWarning("exec of '%s': %s\n", child_exe, SysErrorMsg(errno));
	}
	_exit(0);
    }

    /* We are the parent; clean up */
    if (child_cwd)
	free(child_cwd);
    free(child_exe);
}
#endif /* OPT_EXEC_XTERM */

/*
 * Rather than sending characters to the host, put them directly into our
 * input queue.  That lets a user have access to any of the control sequences
 * for a key binding.  This is the equivalent of local function key support.
 *
 * NOTE:  This code does not support the hexadecimal kludge used in
 * HandleStringEvent because it prevents us from sending an arbitrary string
 * (but it appears in a lot of examples - so we are stuck with it).  The
 * standard string converter does recognize "\" for newline ("\n") and for
 * octal constants (e.g., "\007" for BEL).  So we assume the user can make do
 * without a specialized converter.  (Don't try to use \000, though).
 */
/* ARGSUSED */
void
HandleInterpret(Widget w GCC_UNUSED,
		XEvent *event GCC_UNUSED,
		String *params,
		Cardinal *param_count)
{
    if (*param_count == 1) {
	const char *value = params[0];
	int need = (int) strlen(value);
	int used = (int) (VTbuffer->next - VTbuffer->buffer);
	int have = (int) (VTbuffer->last - VTbuffer->buffer);

	if (have - used + need < BUF_SIZE) {

	    fillPtyData(term, VTbuffer, value, (int) strlen(value));

	    TRACE(("Interpret %s\n", value));
	    VTbuffer->update++;
	}
    }
}

/*ARGSUSED*/
void
HandleEnterWindow(Widget w GCC_UNUSED,
		  XtPointer eventdata GCC_UNUSED,
		  XEvent *event GCC_UNUSED,
		  Boolean *cont GCC_UNUSED)
{
    /* NOP since we handled it above */
    TRACE(("HandleEnterWindow ignored\n"));
    TRACE_FOCUS(w, event);
}

/*ARGSUSED*/
void
HandleLeaveWindow(Widget w GCC_UNUSED,
		  XtPointer eventdata GCC_UNUSED,
		  XEvent *event GCC_UNUSED,
		  Boolean *cont GCC_UNUSED)
{
    /* NOP since we handled it above */
    TRACE(("HandleLeaveWindow ignored\n"));
    TRACE_FOCUS(w, event);
}

/*ARGSUSED*/
void
HandleFocusChange(Widget w GCC_UNUSED,
		  XtPointer eventdata GCC_UNUSED,
		  XEvent *ev,
		  Boolean *cont GCC_UNUSED)
{
    XFocusChangeEvent *event = (XFocusChangeEvent *) ev;
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    TRACE(("HandleFocusChange type=%s, mode=%s, detail=%s\n",
	   visibleEventType(event->type),
	   visibleNotifyMode(event->mode),
	   visibleNotifyDetail(event->detail)));
    TRACE_FOCUS(xw, event);

    if (screen->quiet_grab
	&& (event->mode == NotifyGrab || event->mode == NotifyUngrab)) {
	/* EMPTY */ ;
    } else if (event->type == FocusIn) {
	if (event->detail != NotifyPointer) {
	    setXUrgency(xw, False);
	}

	/*
	 * NotifyNonlinear only happens (on FocusIn) if the pointer was not in
	 * one of our windows.  Use this to reset a case where one xterm is
	 * partly obscuring another, and X gets (us) confused about whether the
	 * pointer was in the window.  In particular, this can happen if the
	 * user is resizing the obscuring window, causing some events to not be
	 * delivered to the obscured window.
	 */
	if (event->detail == NotifyNonlinear
	    && (screen->select & INWINDOW) != 0) {
	    unselectwindow(xw, INWINDOW);
	}
	selectwindow(xw,
		     ((event->detail == NotifyPointer)
		      ? INWINDOW
		      : FOCUS));
	SendFocusButton(xw, event);
    } else {
#if OPT_FOCUS_EVENT
	if (event->type == FocusOut) {
	    SendFocusButton(xw, event);
	}
#endif
	/*
	 * XGrabKeyboard() will generate NotifyGrab event that we want to
	 * ignore.
	 */
	if (event->mode != NotifyGrab) {
	    unselectwindow(xw,
			   ((event->detail == NotifyPointer)
			    ? INWINDOW
			    : FOCUS));
	}
	if (screen->grabbedKbd && (event->mode == NotifyUngrab)) {
	    Bell(xw, XkbBI_Info, 100);
	    ReverseVideo(xw);
	    screen->grabbedKbd = False;
	    update_securekbd();
	}
    }
}

static long lastBellTime;	/* in milliseconds */

#if defined(HAVE_XKB_BELL_EXT)
static Atom
AtomBell(XtermWidget xw, int which)
{
#define DATA(name) { XkbBI_##name, XkbBN_##name }
    static struct {
	int value;
	const char *name;
    } table[] = {
	DATA(Info),
	    DATA(MarginBell),
	    DATA(MinorError),
	    DATA(TerminalBell)
    };
    Cardinal n;
    Atom result = None;

    for (n = 0; n < XtNumber(table); ++n) {
	if (table[n].value == which) {
	    result = XInternAtom(XtDisplay(xw), table[n].name, False);
	    break;
	}
    }
    return result;
}
#endif

void
xtermBell(XtermWidget xw, int which, int percent)
{
    TScreen *screen = TScreenOf(xw);
#if defined(HAVE_XKB_BELL_EXT)
    Atom tony = AtomBell(xw, which);
#endif

    switch (which) {
    case XkbBI_Info:
    case XkbBI_MinorError:
    case XkbBI_MajorError:
    case XkbBI_TerminalBell:
	switch (screen->warningVolume) {
	case bvOff:
	    percent = -100;
	    break;
	case bvLow:
	    break;
	case bvHigh:
	    percent = 100;
	    break;
	}
	break;
    case XkbBI_MarginBell:
	switch (screen->marginVolume) {
	case bvOff:
	    percent = -100;
	    break;
	case bvLow:
	    break;
	case bvHigh:
	    percent = 100;
	    break;
	}
	break;
    default:
	break;
    }

#if defined(HAVE_XKB_BELL_EXT)
    if (tony != None) {
	XkbBell(screen->display, VShellWindow(xw), percent, tony);
    } else
#endif
	XBell(screen->display, percent);
}

void
Bell(XtermWidget xw, int which, int percent)
{
    TScreen *screen = TScreenOf(xw);
    struct timeval curtime;

    TRACE(("BELL %d %d%%\n", which, percent));
    if (!XtIsRealized((Widget) xw)) {
	return;
    }

    setXUrgency(xw, True);

    /* has enough time gone by that we are allowed to ring
       the bell again? */
    if (screen->bellSuppressTime) {
	long now_msecs;

	if (screen->bellInProgress) {
	    do_xevents();
	    if (screen->bellInProgress) {	/* even after new events? */
		return;
	    }
	}
	X_GETTIMEOFDAY(&curtime);
	now_msecs = 1000 * curtime.tv_sec + curtime.tv_usec / 1000;
	if (lastBellTime != 0 && now_msecs - lastBellTime >= 0 &&
	    now_msecs - lastBellTime < screen->bellSuppressTime) {
	    return;
	}
	lastBellTime = now_msecs;
    }

    if (screen->visualbell) {
	VisualBell();
    } else {
	xtermBell(xw, which, percent);
    }

    if (screen->poponbell)
	XRaiseWindow(screen->display, VShellWindow(xw));

    if (screen->bellSuppressTime) {
	/* now we change a property and wait for the notify event to come
	   back.  If the server is suspending operations while the bell
	   is being emitted (problematic for audio bell), this lets us
	   know when the previous bell has finished */
	Widget w = CURRENT_EMU();
	XChangeProperty(XtDisplay(w), XtWindow(w),
			XA_NOTICE, XA_NOTICE, 8, PropModeAppend, NULL, 0);
	screen->bellInProgress = True;
    }
}

static void
flashWindow(TScreen *screen, Window window, GC visualGC, unsigned width, unsigned height)
{
    int y = 0;
    int x = 0;

    if (screen->flash_line) {
	y = CursorY(screen, screen->cur_row);
	height = (unsigned) FontHeight(screen);
    }
    XFillRectangle(screen->display, window, visualGC, x, y, width, height);
    XFlush(screen->display);
    Sleep(VB_DELAY);
    XFillRectangle(screen->display, window, visualGC, x, y, width, height);
}

void
VisualBell(void)
{
    TScreen *screen = TScreenOf(term);

    if (VB_DELAY > 0) {
	Pixel xorPixel = (T_COLOR(screen, TEXT_FG) ^
			  T_COLOR(screen, TEXT_BG));
	XGCValues gcval;
	GC visualGC;

	gcval.function = GXxor;
	gcval.foreground = xorPixel;
	visualGC = XtGetGC((Widget) term, GCFunction + GCForeground, &gcval);
#if OPT_TEK4014
	if (TEK4014_ACTIVE(term)) {
	    TekScreen *tekscr = TekScreenOf(tekWidget);
	    flashWindow(screen, TWindow(tekscr), visualGC,
			TFullWidth(tekscr),
			TFullHeight(tekscr));
	} else
#endif
	{
	    flashWindow(screen, VWindow(screen), visualGC,
			FullWidth(screen),
			FullHeight(screen));
	}
	XtReleaseGC((Widget) term, visualGC);
    }
}

/* ARGSUSED */
void
HandleBellPropertyChange(Widget w GCC_UNUSED,
			 XtPointer data GCC_UNUSED,
			 XEvent *ev,
			 Boolean *more GCC_UNUSED)
{
    TScreen *screen = TScreenOf(term);

    if (ev->xproperty.atom == XA_NOTICE) {
	screen->bellInProgress = False;
    }
}

void
xtermWarning(const char *fmt,...)
{
    int save_err = errno;
    va_list ap;

    fflush(stdout);

#if OPT_TRACE
    va_start(ap, fmt);
    Trace("xtermWarning: ");
    TraceVA(fmt, ap);
    va_end(ap);
#endif

    fprintf(stderr, "%s: ", ProgramName);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    (void) fflush(stderr);

    va_end(ap);
    errno = save_err;
}

void
xtermPerror(const char *fmt,...)
{
    int save_err = errno;
    char *msg = strerror(errno);
    va_list ap;

    fflush(stdout);

#if OPT_TRACE
    va_start(ap, fmt);
    Trace("xtermPerror: ");
    TraceVA(fmt, ap);
    va_end(ap);
#endif

    fprintf(stderr, "%s: ", ProgramName);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, ": %s\n", msg);
    (void) fflush(stderr);

    va_end(ap);
    errno = save_err;
}

Window
WMFrameWindow(XtermWidget xw)
{
    Window win_root, win_current, *children;
    Window win_parent = 0;
    unsigned int nchildren;

    win_current = XtWindow(xw);

    /* find the parent which is child of root */
    do {
	if (win_parent)
	    win_current = win_parent;
	XQueryTree(TScreenOf(xw)->display,
		   win_current,
		   &win_root,
		   &win_parent,
		   &children,
		   &nchildren);
	XFree(children);
    } while (win_root != win_parent);

    return win_current;
}

#if OPT_DABBREV
/*
 * The following code implements `dynamic abbreviation' expansion a la
 * Emacs.  It looks in the preceding visible screen and its scrollback
 * to find expansions of a typed word.  It compares consecutive
 * expansions and ignores one of them if they are identical.
 * (Tomasz J. Cholewo, t.cholewo@ieee.org)
 */

#define IS_WORD_CONSTITUENT(x) ((x) != ' ' && (x) != '\0')

static int
dabbrev_prev_char(TScreen *screen, CELL *cell, LineData **ld)
{
    int result = -1;
    int firstLine = -(screen->savedlines);

    *ld = getLineData(screen, cell->row);
    while (cell->row >= firstLine) {
	if (--(cell->col) >= 0) {
	    result = (int) (*ld)->charData[cell->col];
	    break;
	}
	if (--(cell->row) < firstLine)
	    break;		/* ...there is no previous line */
	*ld = getLineData(screen, cell->row);
	cell->col = MaxCols(screen);
	if (!LineTstWrapped(*ld)) {
	    result = ' ';	/* treat lines as separate */
	    break;
	}
    }
    return result;
}

static char *
dabbrev_prev_word(XtermWidget xw, CELL *cell, LineData **ld)
{
    TScreen *screen = TScreenOf(xw);
    char *abword;
    int c;
    char *ab_end = (xw->work.dabbrev_data + MAX_DABBREV - 1);
    char *result = 0;

    abword = ab_end;
    *abword = '\0';		/* end of string marker */

    while ((c = dabbrev_prev_char(screen, cell, ld)) >= 0 &&
	   IS_WORD_CONSTITUENT(c)) {
	if (abword > xw->work.dabbrev_data)	/* store only the last chars */
	    *(--abword) = (char) c;
    }

    if (c >= 0) {
	result = abword;
    } else if (abword != ab_end) {
	result = abword;
    }

    if (result != 0) {
	while ((c = dabbrev_prev_char(screen, cell, ld)) >= 0 &&
	       !IS_WORD_CONSTITUENT(c)) {
	    ;			/* skip preceding spaces */
	}
	(cell->col)++;		/* can be | > screen->max_col| */
    }
    return result;
}

static int
dabbrev_expand(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    int pty = screen->respond;	/* file descriptor of pty */

    static CELL cell;
    static char *dabbrev_hint = 0, *lastexpansion = 0;
    static unsigned int expansions;

    char *expansion;
    size_t hint_len;
    int result = 0;
    LineData *ld;

    if (!screen->dabbrev_working) {	/* initialize */
	expansions = 0;
	cell.col = screen->cur_col;
	cell.row = screen->cur_row;

	if (dabbrev_hint != 0)
	    free(dabbrev_hint);

	if ((dabbrev_hint = dabbrev_prev_word(xw, &cell, &ld)) != 0) {

	    if (lastexpansion != 0)
		free(lastexpansion);

	    if ((lastexpansion = strdup(dabbrev_hint)) != 0) {

		/* make own copy */
		if ((dabbrev_hint = strdup(dabbrev_hint)) != 0) {
		    screen->dabbrev_working = True;
		    /* we are in the middle of dabbrev process */
		}
	    } else {
		return result;
	    }
	} else {
	    return result;
	}
	if (!screen->dabbrev_working) {
	    if (lastexpansion != 0) {
		free(lastexpansion);
		lastexpansion = 0;
	    }
	    return result;
	}
    }

    if (dabbrev_hint == 0)
	return result;

    hint_len = strlen(dabbrev_hint);
    for (;;) {
	if ((expansion = dabbrev_prev_word(xw, &cell, &ld)) == 0) {
	    if (expansions >= 2) {
		expansions = 0;
		cell.col = screen->cur_col;
		cell.row = screen->cur_row;
		continue;
	    }
	    break;
	}
	if (!strncmp(dabbrev_hint, expansion, hint_len) &&	/* empty hint matches everything */
	    strlen(expansion) > hint_len &&	/* trivial expansion disallowed */
	    strcmp(expansion, lastexpansion))	/* different from previous */
	    break;
    }

    if (expansion != 0) {
	Char *copybuffer;
	size_t del_cnt = strlen(lastexpansion) - hint_len;
	size_t buf_cnt = del_cnt + strlen(expansion) - hint_len;

	if ((copybuffer = TypeMallocN(Char, buf_cnt)) != 0) {
	    /* delete previous expansion */
	    memset(copybuffer, screen->dabbrev_erase_char, del_cnt);
	    memmove(copybuffer + del_cnt,
		    expansion + hint_len,
		    strlen(expansion) - hint_len);
	    v_write(pty, copybuffer, (unsigned) buf_cnt);
	    /* v_write() just reset our flag */
	    screen->dabbrev_working = True;
	    free(copybuffer);

	    free(lastexpansion);

	    if ((lastexpansion = strdup(expansion)) != 0) {
		result = 1;
		expansions++;
	    }
	}
    }

    return result;
}

/*ARGSUSED*/
void
HandleDabbrevExpand(Widget w,
		    XEvent *event GCC_UNUSED,
		    String *params GCC_UNUSED,
		    Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    TRACE(("Handle dabbrev-expand for %p\n", (void *) w));
    if ((xw = getXtermWidget(w)) != 0) {
	if (!dabbrev_expand(xw))
	    Bell(xw, XkbBI_TerminalBell, 0);
    }
}
#endif /* OPT_DABBREV */

#if OPT_MAXIMIZE
/*ARGSUSED*/
void
HandleDeIconify(Widget w,
		XEvent *event GCC_UNUSED,
		String *params GCC_UNUSED,
		Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	TScreen *screen = TScreenOf(xw);
	XMapWindow(screen->display, VShellWindow(xw));
    }
}

/*ARGSUSED*/
void
HandleIconify(Widget w,
	      XEvent *event GCC_UNUSED,
	      String *params GCC_UNUSED,
	      Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	TScreen *screen = TScreenOf(xw);
	XIconifyWindow(screen->display,
		       VShellWindow(xw),
		       DefaultScreen(screen->display));
    }
}

int
QueryMaximize(XtermWidget xw, unsigned *width, unsigned *height)
{
    TScreen *screen = TScreenOf(xw);
    XSizeHints hints;
    long supp = 0;
    Window root_win;
    int root_x = -1;		/* saved co-ordinates */
    int root_y = -1;
    unsigned root_border;
    unsigned root_depth;
    int code;

    if (XGetGeometry(screen->display,
		     RootWindowOfScreen(XtScreen(xw)),
		     &root_win,
		     &root_x,
		     &root_y,
		     width,
		     height,
		     &root_border,
		     &root_depth)) {
	TRACE(("QueryMaximize: XGetGeometry position %d,%d size %d,%d border %d\n",
	       root_x,
	       root_y,
	       *width,
	       *height,
	       root_border));

	*width -= (root_border * 2);
	*height -= (root_border * 2);

	hints.flags = PMaxSize;
	if (XGetWMNormalHints(screen->display,
			      VShellWindow(xw),
			      &hints,
			      &supp)
	    && (hints.flags & PMaxSize) != 0) {

	    TRACE(("QueryMaximize: WM hints max_w %#x max_h %#x\n",
		   hints.max_width,
		   hints.max_height));

	    if ((unsigned) hints.max_width < *width)
		*width = (unsigned) hints.max_width;
	    if ((unsigned) hints.max_height < *height)
		*height = (unsigned) hints.max_height;
	}
	code = 1;
    } else {
	*width = 0;
	*height = 0;
	code = 0;
    }
    return code;
}

void
RequestMaximize(XtermWidget xw, int maximize)
{
    TScreen *screen = TScreenOf(xw);
    XWindowAttributes wm_attrs, vshell_attrs;
    unsigned root_width, root_height;
    Boolean success = False;

    TRACE(("RequestMaximize %d:%s\n",
	   maximize,
	   (maximize
	    ? "maximize"
	    : "restore")));

    /*
     * Before any maximize, ensure that we can capture the current screensize
     * as well as the estimated root-window size.
     */
    if (maximize
	&& QueryMaximize(xw, &root_width, &root_height)
	&& xtermGetWinAttrs(screen->display,
			    WMFrameWindow(xw),
			    &wm_attrs)
	&& xtermGetWinAttrs(screen->display,
			    VShellWindow(xw),
			    &vshell_attrs)) {

	if (screen->restore_data != True
	    || screen->restore_width != root_width
	    || screen->restore_height != root_height) {
	    screen->restore_data = True;
	    screen->restore_x = wm_attrs.x + wm_attrs.border_width;
	    screen->restore_y = wm_attrs.y + wm_attrs.border_width;
	    screen->restore_width = (unsigned) vshell_attrs.width;
	    screen->restore_height = (unsigned) vshell_attrs.height;
	    TRACE(("RequestMaximize: save window position %d,%d size %d,%d\n",
		   screen->restore_x,
		   screen->restore_y,
		   screen->restore_width,
		   screen->restore_height));
	}

	/* subtract wm decoration dimensions */
	root_width -= (unsigned) ((wm_attrs.width - vshell_attrs.width)
				  + (wm_attrs.border_width * 2));
	root_height -= (unsigned) ((wm_attrs.height - vshell_attrs.height)
				   + (wm_attrs.border_width * 2));
	success = True;
    } else if (screen->restore_data) {
	success = True;
	maximize = 0;
    }

    if (success) {
	switch (maximize) {
	case 3:
	    FullScreen(xw, 3);	/* depends on EWMH */
	    break;
	case 2:
	    FullScreen(xw, 2);	/* depends on EWMH */
	    break;
	case 1:
	    FullScreen(xw, 0);	/* overrides any EWMH hint */
	    XMoveResizeWindow(screen->display, VShellWindow(xw),
			      0 + wm_attrs.border_width,	/* x */
			      0 + wm_attrs.border_width,	/* y */
			      root_width,
			      root_height);
	    break;

	default:
	    FullScreen(xw, 0);	/* reset any EWMH hint */
	    if (screen->restore_data) {
		screen->restore_data = False;

		TRACE(("HandleRestoreSize: position %d,%d size %d,%d\n",
		       screen->restore_x,
		       screen->restore_y,
		       screen->restore_width,
		       screen->restore_height));

		XMoveResizeWindow(screen->display,
				  VShellWindow(xw),
				  screen->restore_x,
				  screen->restore_y,
				  screen->restore_width,
				  screen->restore_height);
	    }
	    break;
	}
    }
}

/*ARGSUSED*/
void
HandleMaximize(Widget w,
	       XEvent *event GCC_UNUSED,
	       String *params GCC_UNUSED,
	       Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	RequestMaximize(xw, 1);
    }
}

/*ARGSUSED*/
void
HandleRestoreSize(Widget w,
		  XEvent *event GCC_UNUSED,
		  String *params GCC_UNUSED,
		  Cardinal *nparams GCC_UNUSED)
{
    XtermWidget xw;

    if ((xw = getXtermWidget(w)) != 0) {
	RequestMaximize(xw, 0);
    }
}
#endif /* OPT_MAXIMIZE */

void
Redraw(void)
{
    TScreen *screen = TScreenOf(term);
    XExposeEvent event;

    TRACE(("Redraw\n"));

    event.type = Expose;
    event.display = screen->display;
    event.x = 0;
    event.y = 0;
    event.count = 0;

    if (VWindow(screen)) {
	event.window = VWindow(screen);
	event.width = term->core.width;
	event.height = term->core.height;
	(*term->core.widget_class->core_class.expose) ((Widget) term,
						       (XEvent *) &event,
						       NULL);
	if (ScrollbarWidth(screen)) {
	    (screen->scrollWidget->core.widget_class->core_class.expose)
		(screen->scrollWidget, (XEvent *) &event, NULL);
	}
    }
#if OPT_TEK4014
    if (TEK4014_SHOWN(term)) {
	TekScreen *tekscr = TekScreenOf(tekWidget);
	event.window = TWindow(tekscr);
	event.width = tekWidget->core.width;
	event.height = tekWidget->core.height;
	TekExpose((Widget) tekWidget, (XEvent *) &event, NULL);
    }
#endif
}

#ifdef VMS
#define TIMESTAMP_FMT "%s%d-%02d-%02d-%02d-%02d-%02d"
#else
#define TIMESTAMP_FMT "%s%d-%02d-%02d.%02d:%02d:%02d"
#endif

void
timestamp_filename(char *dst, const char *src)
{
    time_t tstamp;
    struct tm *tstruct;

    tstamp = time((time_t *) 0);
    tstruct = localtime(&tstamp);
    sprintf(dst, TIMESTAMP_FMT,
	    src,
	    (int) tstruct->tm_year + 1900,
	    tstruct->tm_mon + 1,
	    tstruct->tm_mday,
	    tstruct->tm_hour,
	    tstruct->tm_min,
	    tstruct->tm_sec);
}

int
open_userfile(uid_t uid, gid_t gid, char *path, Bool append)
{
    int fd;
    struct stat sb;

#ifdef VMS
    if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
	int the_error = errno;
	xtermWarning("cannot open %s: %d:%s\n",
		     path,
		     the_error,
		     SysErrorMsg(the_error));
	return -1;
    }
    chown(path, uid, gid);
#else
    if ((access(path, F_OK) != 0 && (errno != ENOENT))
	|| (creat_as(uid, gid, append, path, 0644) <= 0)
	|| ((fd = open(path, O_WRONLY | O_APPEND)) < 0)) {
	int the_error = errno;
	xtermWarning("cannot open %s: %d:%s\n",
		     path,
		     the_error,
		     SysErrorMsg(the_error));
	return -1;
    }
#endif

    /*
     * Doublecheck that the user really owns the file that we've opened before
     * we do any damage, and that it is not world-writable.
     */
    if (fstat(fd, &sb) < 0
	|| sb.st_uid != uid
	|| (sb.st_mode & 022) != 0) {
	xtermWarning("you do not own %s\n", path);
	close(fd);
	return -1;
    }
    return fd;
}

#ifndef VMS
/*
 * Create a file only if we could with the permissions of the real user id.
 * We could emulate this with careful use of access() and following
 * symbolic links, but that is messy and has race conditions.
 * Forking is messy, too, but we can't count on setreuid() or saved set-uids
 * being available.
 *
 * Note: When called for user logging, we have ensured that the real and
 * effective user ids are the same, so this remains as a convenience function
 * for the debug logs.
 *
 * Returns
 *	 1 if we can proceed to open the file in relative safety,
 *	-1 on error, e.g., cannot fork
 *	 0 otherwise.
 */
int
creat_as(uid_t uid, gid_t gid, Bool append, char *pathname, unsigned mode)
{
    int fd;
    pid_t pid;
    int retval = 0;
    int childstat = 0;
#ifndef HAVE_WAITPID
    int waited;
    void (*chldfunc) (int);

    chldfunc = signal(SIGCHLD, SIG_DFL);
#endif /* HAVE_WAITPID */

    TRACE(("creat_as(uid=%d/%d, gid=%d/%d, append=%d, pathname=%s, mode=%#o)\n",
	   (int) uid, (int) geteuid(),
	   (int) gid, (int) getegid(),
	   append,
	   pathname,
	   mode));

    if (uid == geteuid() && gid == getegid()) {
	fd = open(pathname,
		  O_WRONLY | O_CREAT | (append ? O_APPEND : O_EXCL),
		  mode);
	if (fd >= 0)
	    close(fd);
	return (fd >= 0);
    }

    pid = fork();
    switch (pid) {
    case 0:			/* child */
	if (setgid(gid) == -1
	    || setuid(uid) == -1) {
	    /* we cannot report an error here via stderr, just quit */
	    retval = 1;
	} else {
	    fd = open(pathname,
		      O_WRONLY | O_CREAT | (append ? O_APPEND : O_EXCL),
		      mode);
	    if (fd >= 0) {
		close(fd);
		retval = 0;
	    } else {
		retval = 1;
	    }
	}
	_exit(retval);
	/* NOTREACHED */
    case -1:			/* error */
	return retval;
    default:			/* parent */
#ifdef HAVE_WAITPID
	while (waitpid(pid, &childstat, 0) < 0) {
#ifdef EINTR
	    if (errno == EINTR)
		continue;
#endif /* EINTR */
#ifdef ERESTARTSYS
	    if (errno == ERESTARTSYS)
		continue;
#endif /* ERESTARTSYS */
	    break;
	}
#else /* HAVE_WAITPID */
	waited = wait(&childstat);
	signal(SIGCHLD, chldfunc);
	/*
	   Since we had the signal handler uninstalled for a while,
	   we might have missed the termination of our screen child.
	   If we can check for this possibility without hanging, do so.
	 */
	do
	    if (waited == TScreenOf(term)->pid)
		NormalExit();
	while ((waited = nonblocking_wait()) > 0) ;
#endif /* HAVE_WAITPID */
#ifndef WIFEXITED
#define WIFEXITED(status) ((status & 0xff) != 0)
#endif
	if (WIFEXITED(childstat))
	    retval = 1;
	return retval;
    }
}
#endif /* !VMS */

int
xtermResetIds(TScreen *screen)
{
    int result = 0;
    if (setgid(screen->gid) == -1) {
	xtermWarning("unable to reset group-id\n");
	result = -1;
    }
    if (setuid(screen->uid) == -1) {
	xtermWarning("unable to reset user-id\n");
	result = -1;
    }
    return result;
}

#ifdef ALLOWLOGGING

/*
 * Logging is a security hole, since it allows a setuid program to write
 * arbitrary data to an arbitrary file.  So it is disabled by default.
 */

#ifdef ALLOWLOGFILEEXEC
static void
logpipe(int sig GCC_UNUSED)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    DEBUG_MSG("handle:logpipe\n");
#ifdef SYSV
    (void) signal(SIGPIPE, SIG_IGN);
#endif /* SYSV */
    if (screen->logging)
	CloseLog(xw);
}
#endif /* ALLOWLOGFILEEXEC */

void
StartLog(XtermWidget xw)
{
    static char *log_default;
    TScreen *screen = TScreenOf(xw);

    if (screen->logging || (screen->inhibit & I_LOG))
	return;
#ifdef VMS			/* file name is fixed in VMS variant */
    screen->logfd = open(XTERM_VMS_LOGFILE,
			 O_CREAT | O_TRUNC | O_APPEND | O_RDWR,
			 0640);
    if (screen->logfd < 0)
	return;			/* open failed */
#else /*VMS */
    if (screen->logfile == NULL || *screen->logfile == 0) {
	if (screen->logfile)
	    free(screen->logfile);
	if (log_default == NULL) {
#if defined(HAVE_GETHOSTNAME) && defined(HAVE_STRFTIME)
	    const char form[] = "Xterm.log.%s%s.%d";
	    char where[255 + 1];	/* Internet standard limit (RFC 1035):
					   ``To simplify implementations, the
					   total length of a domain name (i.e.,
					   label octets and label length
					   octets) is restricted to 255 octets
					   or less.'' */
	    char when[LEN_TIMESTAMP];
	    char formatted[sizeof(form) + sizeof(where) + sizeof(when) + 9];
	    time_t now;
	    struct tm *ltm;

	    now = time((time_t *) 0);
	    ltm = (struct tm *) localtime(&now);
	    if ((gethostname(where, sizeof(where)) == 0) &&
		(strftime(when, sizeof(when), FMT_TIMESTAMP, ltm) > 0)) {
		(void) sprintf(formatted, form, where, when, (int) getpid());
	    } else {
		return;
	    }
	    if ((log_default = x_strdup(formatted)) == NULL) {
		return;
	    }
#else
	    static const char log_def_name[] = "XtermLog.XXXXXX";
	    if ((log_default = x_strdup(log_def_name)) == NULL) {
		return;
	    }
	    mktemp(log_default);
#endif
	}
	if ((screen->logfile = x_strdup(log_default)) == 0)
	    return;
    }
    if (*screen->logfile == '|') {	/* exec command */
#ifdef ALLOWLOGFILEEXEC
	/*
	 * Warning, enabling this "feature" allows arbitrary programs
	 * to be run.  If ALLOWLOGFILECHANGES is enabled, this can be
	 * done through escape sequences....  You have been warned.
	 */
	int pid;
	int p[2];
	static char *shell;
	struct passwd pw;

	if ((shell = x_getenv("SHELL")) == NULL) {

	    if (x_getpwuid(screen->uid, &pw)) {
		char *name = x_getlogin(screen->uid, &pw);
		if (*(pw.pw_shell)) {
		    shell = pw.pw_shell;
		}
		free(name);
	    }
	}

	if (shell == 0) {
	    static char dummy[] = "/bin/sh";
	    shell = dummy;
	}

	if (access(shell, X_OK) != 0) {
	    xtermPerror("Can't execute `%s'\n", shell);
	    return;
	}

	if (pipe(p) < 0) {
	    xtermPerror("Can't make a pipe connection\n");
	    return;
	} else if ((pid = fork()) < 0) {
	    xtermPerror("Can't fork...\n");
	    return;
	}
	if (pid == 0) {		/* child */
	    /*
	     * Close our output (we won't be talking back to the
	     * parent), and redirect our child's output to the
	     * original stderr.
	     */
	    close(p[1]);
	    dup2(p[0], 0);
	    close(p[0]);
	    dup2(fileno(stderr), 1);
	    dup2(fileno(stderr), 2);

	    close(fileno(stderr));
	    close(ConnectionNumber(screen->display));
	    close(screen->respond);

	    signal(SIGHUP, SIG_DFL);
	    signal(SIGCHLD, SIG_DFL);

	    /* (this is redundant) */
	    if (xtermResetIds(screen) < 0)
		exit(ERROR_SETUID);

	    if (access(shell, X_OK) == 0) {
		execl(shell, shell, "-c", &screen->logfile[1], (void *) 0);
		xtermWarning("Can't exec `%s'\n", &screen->logfile[1]);
	    } else {
		xtermWarning("Can't execute `%s'\n", shell);
	    }
	    exit(ERROR_LOGEXEC);
	}
	close(p[0]);
	screen->logfd = p[1];
	signal(SIGPIPE, logpipe);
#else
	Bell(xw, XkbBI_Info, 0);
	Bell(xw, XkbBI_Info, 0);
	return;
#endif
    } else {
	if ((screen->logfd = open_userfile(screen->uid,
					   screen->gid,
					   screen->logfile,
					   (log_default != 0))) < 0)
	    return;
    }
#endif /*VMS */
    screen->logstart = VTbuffer->next;
    screen->logging = True;
    update_logging();
}

void
CloseLog(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (!screen->logging || (screen->inhibit & I_LOG))
	return;
    FlushLog(xw);
    close(screen->logfd);
    screen->logging = False;
    update_logging();
}

void
FlushLog(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->logging && !(screen->inhibit & I_LOG)) {
	Char *cp;
	int i;

#ifdef VMS			/* avoid logging output loops which otherwise occur sometimes
				   when there is no output and cp/screen->logstart are 1 apart */
	if (!tt_new_output)
	    return;
	tt_new_output = False;
#endif /* VMS */
	cp = VTbuffer->next;
	if (screen->logstart != 0
	    && (i = (int) (cp - screen->logstart)) > 0) {
	    IGNORE_RC(write(screen->logfd, screen->logstart, (size_t) i));
	}
	screen->logstart = VTbuffer->next;
    }
}

#endif /* ALLOWLOGGING */

/***====================================================================***/

static unsigned
maskToShift(unsigned long mask)
{
    unsigned result = 0;
    if (mask != 0) {
	while ((mask & 1) == 0) {
	    mask >>= 1;
	    ++result;
	}
    }
    return result;
}

int
getVisualInfo(XtermWidget xw)
{
#define MYFMT "getVisualInfo \
depth %d, \
type %d (%s), \
size %d \
rgb masks (%04lx/%04lx/%04lx)\n"
#define MYARG \
       vi->depth,\
       vi->class,\
       ((vi->class & 1) ? "dynamic" : "static"),\
       vi->colormap_size,\
       vi->red_mask,\
       vi->green_mask,\
       vi->blue_mask

    TScreen *screen = TScreenOf(xw);
    Display *dpy = screen->display;
    XVisualInfo myTemplate;

    if (xw->visInfo == 0 && xw->numVisuals == 0) {
	myTemplate.visualid = XVisualIDFromVisual(DefaultVisual(dpy,
								XDefaultScreen(dpy)));
	xw->visInfo = XGetVisualInfo(dpy, (long) VisualIDMask,
				     &myTemplate, &xw->numVisuals);

	if ((xw->visInfo != 0) && (xw->numVisuals > 0)) {
	    XVisualInfo *vi = xw->visInfo;
	    xw->rgb_shifts[0] = maskToShift(vi->red_mask);
	    xw->rgb_shifts[1] = maskToShift(vi->green_mask);
	    xw->rgb_shifts[2] = maskToShift(vi->blue_mask);

	    xw->has_rgb = ((vi->red_mask != 0) &&
			   (vi->green_mask != 0) &&
			   (vi->blue_mask != 0) &&
			   ((vi->red_mask & vi->green_mask) == 0) &&
			   ((vi->green_mask & vi->blue_mask) == 0) &&
			   ((vi->blue_mask & vi->red_mask) == 0));

	    if (resource.reportColors) {
		printf(MYFMT, MYARG);
	    }
	    TRACE((MYFMT, MYARG));
	    TRACE(("...shifts %u/%u/%u\n",
		   xw->rgb_shifts[0],
		   xw->rgb_shifts[1],
		   xw->rgb_shifts[2]));
	}
    }
    return (xw->visInfo != 0) && (xw->numVisuals > 0);
#undef MYFMT
#undef MYARG
}

#if OPT_ISO_COLORS
static void
ReportAnsiColorRequest(XtermWidget xw, int colornum, int final)
{
    if (AllowColorOps(xw, ecGetAnsiColor)) {
	XColor color;
	Colormap cmap = xw->core.colormap;
	char buffer[80];

	TRACE(("ReportAnsiColorRequest %d\n", colornum));
	color.pixel = GET_COLOR_RES(xw, TScreenOf(xw)->Acolors[colornum]);
	XQueryColor(TScreenOf(xw)->display, cmap, &color);
	sprintf(buffer, "4;%d;rgb:%04x/%04x/%04x",
		colornum,
		color.red,
		color.green,
		color.blue);
	unparseputc1(xw, ANSI_OSC);
	unparseputs(xw, buffer);
	unparseputc1(xw, final);
	unparse_end(xw);
    }
}

static void
getColormapInfo(XtermWidget xw, unsigned *typep, unsigned *sizep)
{
    if (getVisualInfo(xw)) {
	*typep = (unsigned) xw->visInfo->class;
	*sizep = (unsigned) xw->visInfo->colormap_size;
    } else {
	*typep = 0;
	*sizep = 0;
    }
}

#define MAX_COLORTABLE 4096

/*
 * Make only one call to XQueryColors(), since it can be slow.
 */
static Boolean
loadColorTable(XtermWidget xw, unsigned length)
{
    Colormap cmap = xw->core.colormap;
    TScreen *screen = TScreenOf(xw);
    Boolean result = (screen->cmap_data != 0);

    if (!result
	&& length != 0
	&& length < MAX_COLORTABLE) {
	screen->cmap_data = TypeMallocN(XColor, (size_t) length);

	if (screen->cmap_data != 0) {
	    unsigned i;

	    screen->cmap_size = length;

	    for (i = 0; i < screen->cmap_size; i++) {
		screen->cmap_data[i].pixel = (unsigned long) i;
	    }
	    result = (Boolean) (XQueryColors(screen->display,
					     cmap,
					     screen->cmap_data,
					     (int) screen->cmap_size) != 0);
	}
    }
    return result;
}

/*
 * Find closest color for "def" in "cmap".
 * Set "def" to the resulting color.
 *
 * Based on Monish Shah's "find_closest_color()" for Vim 6.0,
 * modified with ideas from David Tong's "noflash" library.
 * The code from Vim in turn was derived from FindClosestColor() in Tcl/Tk.
 *
 * Return False if not able to find or allocate a color.
 */
static Boolean
allocateClosestRGB(XtermWidget xw, Colormap cmap, XColor *def)
{
    TScreen *screen = TScreenOf(xw);
    Boolean result = False;
    unsigned cmap_type;
    unsigned cmap_size;

    getColormapInfo(xw, &cmap_type, &cmap_size);

    if ((cmap_type & 1) != 0) {

	if (loadColorTable(xw, cmap_size)) {
	    char *tried = TypeCallocN(char, (size_t) cmap_size);

	    if (tried != 0) {
		unsigned attempts;

		/*
		 * Try (possibly each entry in the color map) to find the best
		 * approximation to the requested color.
		 */
		for (attempts = 0; attempts < cmap_size; attempts++) {
		    Boolean first = True;
		    double bestRGB = 0.0;
		    unsigned bestInx = 0;
		    unsigned i;

		    for (i = 0; i < cmap_size; i++) {
			if (!tried[bestInx]) {
			    double diff, thisRGB = 0.0;

			    /*
			     * Look for the best match based on luminance.
			     * Measure this by the least-squares difference of
			     * the weighted R/G/B components from the color map
			     * versus the requested color.  Use the Y (luma)
			     * component of the YIQ color space model for
			     * weights that correspond to the luminance.
			     */
#define AddColorWeight(weight, color) \
			    diff = weight * (int) ((def->color) - screen->cmap_data[i].color); \
			    thisRGB += diff * diff

			    AddColorWeight(0.30, red);
			    AddColorWeight(0.61, green);
			    AddColorWeight(0.11, blue);

			    if (first || (thisRGB < bestRGB)) {
				first = False;
				bestInx = i;
				bestRGB = thisRGB;
			    }
			}
		    }
		    if (XAllocColor(screen->display, cmap,
				    &screen->cmap_data[bestInx]) != 0) {
			*def = screen->cmap_data[bestInx];
			TRACE(("...closest %x/%x/%x\n", def->red,
			       def->green, def->blue));
			result = True;
			break;
		    }
		    /*
		     * It failed - either the color map entry was readonly, or
		     * another client has allocated the entry.  Mark the entry
		     * so we will ignore it
		     */
		    tried[bestInx] = True;
		}
		free(tried);
	    }
	}
    }
    return result;
}

#ifndef ULONG_MAX
#define ULONG_MAX (unsigned long)(~(0L))
#endif

#define CheckColor(result, value) \
	    result = 0; \
	    if (value.red) \
		result |= 1; \
	    if (value.green) \
		result |= 2; \
	    if (value.blue) \
		result |= 4

#define SelectColor(state, value, result) \
	switch (state) { \
	default: \
	case 1: \
	    result = value.red; \
	    break; \
	case 2: \
	    result = value.green; \
	    break; \
	case 4: \
	    result = value.blue; \
	    break; \
	}

/*
 * Check if the color map consists of values in exactly one of the red, green
 * or blue columns.  If it is not, we do not know how to use it for the exact
 * match.
 */
static int
simpleColors(XColor *colortable, unsigned length)
{
    unsigned n;
    int state = 0;
    int check;

    for (n = 0; n < length; ++n) {
	if (state > 0) {
	    CheckColor(check, colortable[n]);
	    if (check > 0 && check != state) {
		state = 0;
		break;
	    }
	} else {
	    CheckColor(state, colortable[n]);
	}
    }
    switch (state) {
    case 1:
    case 2:
    case 4:
	break;
    default:
	state = 0;
	break;
    }
    return state;
}

/*
 * Shift the mask left or right to put its most significant bit at the 16-bit
 * mark.
 */
static unsigned
normalizeMask(unsigned mask)
{
    while (mask < 0x8000) {
	mask <<= 1;
    }
    while (mask >= 0x10000) {
	mask >>= 1;
    }
    return mask;
}

static unsigned
searchColors(XColor *colortable, unsigned mask, unsigned length, unsigned
	     color, int state)
{
    unsigned result = 0;
    unsigned n;
    unsigned long best = ULONG_MAX;
    unsigned value;

    mask = normalizeMask(mask);
    for (n = 0; n < length; ++n) {
	unsigned long diff;

	SelectColor(state, colortable[n], value);
	diff = ((color & mask) - (value & mask));
	diff *= diff;
	if (diff < best) {
#if 0
	    TRACE(("...%d:looking for %x, found %x/%x/%x (%lx)\n",
		   n, color,
		   colortable[n].red,
		   colortable[n].green,
		   colortable[n].blue,
		   diff));
#endif
	    result = n;
	    best = diff;
	}
    }
    SelectColor(state, colortable[result], value);
    return value;
}

/*
 * This is a workaround for a longstanding defect in the X libraries.
 *
 * According to
 * http://www.unix.com/man-page/all/3x/XAllocColoA/
 *
 *     XAllocColor() acts differently on static and dynamic visuals.  On Pseu-
 *     doColor, DirectColor, and GrayScale  visuals,  XAllocColor()  fails  if
 *     there  are  no  unallocated  colorcells and no allocated read-only cell
 *     exactly matches the requested RGB values.  On  StaticColor,  TrueColor,
 *     and  StaticGray  visuals,  XAllocColor() returns the closest RGB values
 *     available in the colormap.  The colorcell_in_out structure returns  the
 *     actual RGB values allocated.
 *
 * That is, XAllocColor() should suffice unless the color map is full.  In that
 * case, allocateClosestRGB() is useful for the dynamic display classes such as
 * PseudoColor.  It is not useful for TrueColor, since XQueryColors() does not
 * return regular RGB triples (unless a different scheme was used for
 * specifying the pixel values); only the blue value is filled in.  However, it
 * is filled in with the colors that the server supports.
 *
 * Also (the reason for this function), XAllocColor() does not really work as
 * described.  For some TrueColor configurations it merely returns a close
 * approximation, but not the closest.
 */
static Boolean
allocateExactRGB(XtermWidget xw, Colormap cmap, XColor *def)
{
    XColor save = *def;
    TScreen *screen = TScreenOf(xw);
    Boolean result = (Boolean) (XAllocColor(screen->display, cmap, def) != 0);

    /*
     * If this is a statically allocated display with too many items to store
     * in our array, i.e., TrueColor, see if we can improve on the result by
     * using the color values actually supported by the server.
     */
    if (result) {
	unsigned cmap_type;
	unsigned cmap_size;

	getColormapInfo(xw, &cmap_type, &cmap_size);

	if (cmap_type == TrueColor) {
	    XColor temp = *def;
	    int state;

	    if (loadColorTable(xw, cmap_size)
		&& (state = simpleColors(screen->cmap_data, cmap_size)) > 0) {
#define SearchColors(which) \
	temp.which = (unsigned short) searchColors(screen->cmap_data, \
						   (unsigned) xw->visInfo->which##_mask,\
						   cmap_size, \
						   save.which, \
						   state)
		SearchColors(red);
		SearchColors(green);
		SearchColors(blue);
		if (XAllocColor(screen->display, cmap, &temp) != 0) {
#if OPT_TRACE
		    if (temp.red != save.red
			|| temp.green != save.green
			|| temp.blue != save.blue) {
			TRACE(("...improved %x/%x/%x ->%x/%x/%x\n",
			       save.red, save.green, save.blue,
			       temp.red, temp.green, temp.blue));
		    } else {
			TRACE(("...no improvement for %x/%x/%x\n",
			       save.red, save.green, save.blue));
		    }
#endif
		    *def = temp;
		}
	    }
	}
    }

    return result;
}

/*
 * Allocate a color for the "ANSI" colors.  That actually includes colors up
 * to 256.
 *
 * Returns
 *	-1 on error
 *	0 on no change
 *	1 if a new color was allocated.
 */
static int
AllocateAnsiColor(XtermWidget xw,
		  ColorRes * res,
		  const char *spec)
{
    int result;
    XColor def;

    if (xtermAllocColor(xw, &def, spec)) {
	if (
#if OPT_COLOR_RES
	       res->mode == True &&
#endif
	       EQL_COLOR_RES(res, def.pixel)) {
	    result = 0;
	} else {
	    result = 1;
	    SET_COLOR_RES(res, def.pixel);
	    res->red = def.red;
	    res->green = def.green;
	    res->blue = def.blue;
	    TRACE(("AllocateAnsiColor[%d] %s (rgb:%04x/%04x/%04x, pixel 0x%06lx)\n",
		   (int) (res - TScreenOf(xw)->Acolors), spec,
		   def.red,
		   def.green,
		   def.blue,
		   def.pixel));
#if OPT_COLOR_RES
	    if (!res->mode)
		result = 0;
	    res->mode = True;
#endif
	}
    } else {
	TRACE(("AllocateAnsiColor %s (failed)\n", spec));
	result = -1;
    }
    return (result);
}

#if OPT_COLOR_RES
Pixel
xtermGetColorRes(XtermWidget xw, ColorRes * res)
{
    Pixel result = 0;

    if (res->mode) {
	result = res->value;
    } else {
	TRACE(("xtermGetColorRes for Acolors[%d]\n",
	       (int) (res - TScreenOf(xw)->Acolors)));

	if (res >= TScreenOf(xw)->Acolors) {
	    assert(res - TScreenOf(xw)->Acolors < MAXCOLORS);

	    if (AllocateAnsiColor(xw, res, res->resource) < 0) {
		res->value = TScreenOf(xw)->Tcolors[TEXT_FG].value;
		res->mode = -True;
		xtermWarning("Cannot allocate color \"%s\"\n",
			     NonNull(res->resource));
	    }
	    result = res->value;
	} else {
	    result = 0;
	}
    }
    return result;
}
#endif

static int
ChangeOneAnsiColor(XtermWidget xw, int color, const char *name)
{
    int code;

    if (color < 0 || color >= MAXCOLORS) {
	code = -1;
    } else {
	ColorRes *res = &(TScreenOf(xw)->Acolors[color]);

	TRACE(("ChangeAnsiColor for Acolors[%d]\n", color));
	code = AllocateAnsiColor(xw, res, name);
    }
    return code;
}

/*
 * Set or query entries in the Acolors[] array by parsing pairs of color/name
 * values from the given buffer.
 *
 * The color can be any legal index into Acolors[], which consists of the
 * 16/88/256 "ANSI" colors, followed by special color values for the various
 * colorXX resources.  The indices for the special color values are not
 * simple to work with, so an alternative is to use the calls which pass in
 * 'first' set to the beginning of those indices.
 *
 * If the name is "?", report to the host the current value for the color.
 */
static Bool
ChangeAnsiColorRequest(XtermWidget xw,
		       char *buf,
		       int first,
		       int final)
{
    int repaint = False;
    int code;
    int last = (MAXCOLORS - first);

    TRACE(("ChangeAnsiColorRequest string='%s'\n", buf));

    while (buf && *buf) {
	int color;
	char *name = strchr(buf, ';');

	if (name == NULL)
	    break;
	*name = '\0';
	name++;
	color = atoi(buf);
	if (color < 0 || color >= last)
	    break;		/* quit on any error */
	buf = strchr(name, ';');
	if (buf) {
	    *buf = '\0';
	    buf++;
	}
	if (!strcmp(name, "?")) {
	    ReportAnsiColorRequest(xw, color + first, final);
	} else {
	    code = ChangeOneAnsiColor(xw, color + first, name);
	    if (code < 0) {
		/* stop on any error */
		break;
	    } else if (code > 0) {
		repaint = True;
	    }
	    /* FIXME:  free old color somehow?  We aren't for the other color
	     * change style (dynamic colors).
	     */
	}
    }

    return (repaint);
}

static Bool
ResetOneAnsiColor(XtermWidget xw, int color, int start)
{
    Bool repaint = False;
    int last = MAXCOLORS - start;

    if (color >= 0 && color < last) {
	ColorRes *res = &(TScreenOf(xw)->Acolors[color + start]);

	if (res->mode) {
	    /* a color has been allocated for this slot - test further... */
	    if (ChangeOneAnsiColor(xw, color + start, res->resource) > 0) {
		repaint = True;
	    }
	}
    }
    return repaint;
}

int
ResetAnsiColorRequest(XtermWidget xw, char *buf, int start)
{
    int repaint = 0;
    int color;

    TRACE(("ResetAnsiColorRequest(%s)\n", buf));
    if (*buf != '\0') {
	/* reset specific colors */
	while (!IsEmpty(buf)) {
	    char *next;

	    color = (int) (strtol) (buf, &next, 10);
	    if (!PartS2L(buf, next) || (color < 0))
		break;		/* no number at all */
	    if (next != 0) {
		if (strchr(";", *next) == 0)
		    break;	/* unexpected delimiter */
		++next;
	    }

	    if (ResetOneAnsiColor(xw, color, start)) {
		++repaint;
	    }
	    buf = next;
	}
    } else {
	TRACE(("...resetting all %d colors\n", MAXCOLORS));
	for (color = 0; color < MAXCOLORS; ++color) {
	    if (ResetOneAnsiColor(xw, color, start)) {
		++repaint;
	    }
	}
    }
    TRACE(("...ResetAnsiColorRequest ->%d\n", repaint));
    return repaint;
}
#else
#define allocateClosestRGB(xw, cmap, def) 0
#define allocateExactRGB(xw, cmap, def) XAllocColor(TScreenOf(xw)->display, cmap, def)
#endif /* OPT_ISO_COLORS */

Boolean
allocateBestRGB(XtermWidget xw, XColor *def)
{
    Colormap cmap = xw->core.colormap;

    return allocateExactRGB(xw, cmap, def) || allocateClosestRGB(xw, cmap, def);
}

static Boolean
xtermAllocColor(XtermWidget xw, XColor *def, const char *spec)
{
    Boolean result = False;
    TScreen *screen = TScreenOf(xw);
    Colormap cmap = xw->core.colormap;

    if (XParseColor(screen->display, cmap, spec, def)) {
	XColor save_def = *def;
	if (resource.reportColors) {
	    printf("color  %04x/%04x/%04x = \"%s\"\n",
		   def->red, def->green, def->blue,
		   spec);
	}
	if (allocateBestRGB(xw, def)) {
	    if (resource.reportColors) {
		if (def->red != save_def.red ||
		    def->green != save_def.green ||
		    def->blue != save_def.blue) {
		    printf("color  %04x/%04x/%04x ~ \"%s\"\n",
			   def->red, def->green, def->blue,
			   spec);
		}
	    }
	    TRACE(("xtermAllocColor -> %x/%x/%x\n",
		   def->red, def->green, def->blue));
	    result = True;
	}
    }
    return result;
}

/*
 * This provides an approximation (the closest color from xterm's palette)
 * rather than the "exact" color (whatever the display could provide, actually)
 * because of the context in which it is used.
 */
#define ColorDiff(given,cache) ((long) ((cache) >> 8) - (long) (given))
int
xtermClosestColor(XtermWidget xw, int find_red, int find_green, int find_blue)
{
    int result = -1;
#if OPT_COLOR_RES && OPT_ISO_COLORS
    int n;
    int best_index = -1;
    unsigned long best_value = 0;
    unsigned long this_value;
    long diff_red, diff_green, diff_blue;

    TRACE(("xtermClosestColor(%x/%x/%x)\n", find_red, find_green, find_blue));

    for (n = NUM_ANSI_COLORS - 1; n >= 0; --n) {
	ColorRes *res = &(TScreenOf(xw)->Acolors[n]);

	/* ensure that we have a value for each of the colors */
	if (!res->mode) {
	    (void) AllocateAnsiColor(xw, res, res->resource);
	}

	/* find the closest match */
	if (res->mode == True) {
	    TRACE2(("...lookup %lx -> %x/%x/%x\n",
		    res->value, res->red, res->green, res->blue));
	    diff_red = ColorDiff(find_red, res->red);
	    diff_green = ColorDiff(find_green, res->green);
	    diff_blue = ColorDiff(find_blue, res->blue);
	    this_value = (unsigned long) ((diff_red * diff_red)
					  + (diff_green * diff_green)
					  + (diff_blue * diff_blue));
	    if (best_index < 0 || this_value < best_value) {
		best_index = n;
		best_value = this_value;
	    }
	}
    }
    TRACE(("...best match at %d with diff %lx\n", best_index, best_value));
    result = best_index;
#else
    (void) xw;
    (void) find_red;
    (void) find_green;
    (void) find_blue;
#endif
    return result;
}

#if OPT_DIRECT_COLOR
int
getDirectColor(XtermWidget xw, int red, int green, int blue)
{
#define nRGB(name,shift) \
	((unsigned long)(name << xw->rgb_shifts[shift]) \
		         & xw->visInfo->name ##_mask)
    MyPixel result = (MyPixel) (nRGB(red, 0) | nRGB(green, 1) | nRGB(blue, 2));
    return (int) result;
}

static void
formatDirectColor(char *target, XtermWidget xw, unsigned value)
{
#define fRGB(name, shift) \
	(value & xw->visInfo->name ## _mask) >> xw->rgb_shifts[shift]
    sprintf(target, "%lu:%lu:%lu", fRGB(red, 0), fRGB(green, 1), fRGB(blue, 2));
}
#endif /* OPT_DIRECT_COLOR */

#define fg2SGR(n) \
		(n) >= 8 ? 9 : 3, \
		(n) >= 8 ? (n) - 8 : (n)
#define bg2SGR(n) \
		(n) >= 8 ? 10 : 4, \
		(n) >= 8 ? (n) - 8 : (n)

#define EndOf(s) (s) + strlen(s)

char *
xtermFormatSGR(XtermWidget xw, char *target, unsigned attr, int fg, int bg)
{
    TScreen *screen = TScreenOf(xw);
    char *msg = target;

    strcpy(target, "0");
    if (attr & BOLD)
	strcat(msg, ";1");
    if (attr & UNDERLINE)
	strcat(msg, ";4");
    if (attr & BLINK)
	strcat(msg, ";5");
    if (attr & INVERSE)
	strcat(msg, ";7");
    if (attr & INVISIBLE)
	strcat(msg, ";8");
#if OPT_WIDE_ATTRS
    if (attr & ATR_FAINT)
	strcat(msg, ";2");
    if (attr & ATR_ITALIC)
	strcat(msg, ";3");
    if (attr & ATR_STRIKEOUT)
	strcat(msg, ";9");
    if (attr & ATR_DBL_UNDER)
	strcat(msg, ";21");
#endif
#if OPT_256_COLORS || OPT_88_COLORS
    if_OPT_ISO_COLORS(screen, {
	if (attr & FG_COLOR) {
	    if_OPT_DIRECT_COLOR2(screen, hasDirectFG(attr), {
		strcat(msg, ";38:2::");
		formatDirectColor(EndOf(msg), xw, (unsigned) fg);
	    } else
	    )if (fg >= 16) {
		sprintf(EndOf(msg), ";38:5:%d", fg);
	    } else {
		sprintf(EndOf(msg), ";%d%d", fg2SGR(fg));
	    }
	}
	if (attr & BG_COLOR) {
	    if_OPT_DIRECT_COLOR2(screen, hasDirectBG(attr), {
		strcat(msg, ";48:2::");
		formatDirectColor(EndOf(msg), xw, (unsigned) bg);
	    } else
	    )if (bg >= 16) {
		sprintf(EndOf(msg), ";48:5:%d", bg);
	    } else {
		sprintf(EndOf(msg), ";%d%d", bg2SGR(bg));
	    }
	}
    });
#elif OPT_ISO_COLORS
    if_OPT_ISO_COLORS(screen, {
	if (attr & FG_COLOR) {
	    sprintf(EndOf(msg), ";%d%d", fg2SGR(fg));
	}
	if (attr & BG_COLOR) {
	    sprintf(EndOf(msg), ";%d%d", bg2SGR(bg));
	}
    });
#endif
    return target;
}

#if OPT_PASTE64
static void
ManipulateSelectionData(XtermWidget xw, TScreen *screen, char *buf, int final)
{
#define PDATA(a,b) { a, #b }
    static struct {
	char given;
	String result;
    } table[] = {
	PDATA('s', SELECT),
	    PDATA('p', PRIMARY),
	    PDATA('c', CLIPBOARD),
	    PDATA('0', CUT_BUFFER0),
	    PDATA('1', CUT_BUFFER1),
	    PDATA('2', CUT_BUFFER2),
	    PDATA('3', CUT_BUFFER3),
	    PDATA('4', CUT_BUFFER4),
	    PDATA('5', CUT_BUFFER5),
	    PDATA('6', CUT_BUFFER6),
	    PDATA('7', CUT_BUFFER7),
    };

    const char *base = buf;
    Cardinal j, n = 0;

    TRACE(("Manipulate selection data\n"));

    while (*buf != ';' && *buf != '\0') {
	++buf;
    }

    if (*buf == ';') {
	char *used;

	*buf++ = '\0';

	if (*base == '\0')
	    base = "s0";

	if ((used = x_strdup(base)) != 0) {
	    String *select_args;

	    if ((select_args = TypeCallocN(String, 2 + strlen(base))) != 0) {
		while (*base != '\0') {
		    for (j = 0; j < XtNumber(table); ++j) {
			if (*base == table[j].given) {
			    used[n] = *base;
			    select_args[n++] = table[j].result;
			    TRACE(("atom[%d] %s\n", n, table[j].result));
			    break;
			}
		    }
		    ++base;
		}
		used[n] = 0;

		if (!strcmp(buf, "?")) {
		    if (AllowWindowOps(xw, ewGetSelection)) {
			TRACE(("Getting selection\n"));
			unparseputc1(xw, ANSI_OSC);
			unparseputs(xw, "52");
			unparseputc(xw, ';');

			unparseputs(xw, used);
			unparseputc(xw, ';');

			/* Tell xtermGetSelection data is base64 encoded */
			screen->base64_paste = n;
			screen->base64_final = final;

			screen->selection_time =
			    XtLastTimestampProcessed(TScreenOf(xw)->display);

			/* terminator will be written in this call */
			xtermGetSelection((Widget) xw,
					  screen->selection_time,
					  select_args, n,
					  NULL);
			/*
			 * select_args is used via SelectionReceived, cannot
			 * free it here.
			 */
		    } else {
			free(select_args);
		    }
		} else {
		    if (AllowWindowOps(xw, ewSetSelection)) {
			TRACE(("Setting selection with %s\n", buf));
			screen->selection_time =
			    XtLastTimestampProcessed(TScreenOf(xw)->display);
			ClearSelectionBuffer(screen);
			while (*buf != '\0')
			    AppendToSelectionBuffer(screen, CharOf(*buf++));
			CompleteSelection(xw, select_args, n);
		    }
		    free(select_args);
		}
	    }
	    free(used);
	}
    }
}
#endif /* OPT_PASTE64 */

/***====================================================================***/

#define IsSetUtf8Title(xw) (IsTitleMode(xw, tmSetUtf8) || (xw->screen.utf8_title))

static Bool
xtermIsPrintable(XtermWidget xw, Char **bufp, Char *last)
{
    TScreen *screen = TScreenOf(xw);
    Bool result = False;
    Char *cp = *bufp;
    Char *next = cp;

    (void) screen;
    (void) last;

#if OPT_WIDE_CHARS
    if (xtermEnvUTF8() && IsSetUtf8Title(xw)) {
	PtyData data;

	if (decodeUtf8(screen, fakePtyData(&data, cp, last))) {
	    if (data.utf_data != UCS_REPL
		&& (data.utf_data >= 128 ||
		    ansi_table[data.utf_data] == CASE_PRINT)) {
		next += (data.utf_size - 1);
		result = True;
	    } else {
		result = False;
	    }
	} else {
	    result = False;
	}
    } else
#endif
#if OPT_C1_PRINT
	if (screen->c1_printable
	    && (*cp >= 128 && *cp < 160)) {
	result = True;
    } else
#endif
    if (ansi_table[*cp] == CASE_PRINT) {
	result = True;
    }
    *bufp = next;
    return result;
}

/***====================================================================***/

/*
 * Enum corresponding to the actual OSC codes rather than the internal
 * array indices.  Compare with TermColors.
 */
typedef enum {
    OSC_TEXT_FG = 10
    ,OSC_TEXT_BG
    ,OSC_TEXT_CURSOR
    ,OSC_MOUSE_FG
    ,OSC_MOUSE_BG
#if OPT_TEK4014
    ,OSC_TEK_FG = 15
    ,OSC_TEK_BG
#endif
#if OPT_HIGHLIGHT_COLOR
    ,OSC_HIGHLIGHT_BG = 17
#endif
#if OPT_TEK4014
    ,OSC_TEK_CURSOR = 18
#endif
#if OPT_HIGHLIGHT_COLOR
    ,OSC_HIGHLIGHT_FG = 19
#endif
    ,OSC_NCOLORS
} OscTextColors;

/*
 * Map codes to OSC controls that can reset colors.
 */
#define OSC_RESET 100
#define OSC_Reset(code) (code) + OSC_RESET

static Bool
GetOldColors(XtermWidget xw)
{
    if (xw->work.oldColors == NULL) {
	int i;

	xw->work.oldColors = TypeXtMalloc(ScrnColors);
	if (xw->work.oldColors == NULL) {
	    xtermWarning("allocation failure in GetOldColors\n");
	    return (False);
	}
	xw->work.oldColors->which = 0;
	for (i = 0; i < NCOLORS; i++) {
	    xw->work.oldColors->colors[i] = 0;
	    xw->work.oldColors->names[i] = NULL;
	}
	GetColors(xw, xw->work.oldColors);
    }
    return (True);
}

static int
oppositeColor(int n)
{
    switch (n) {
    case TEXT_FG:
	n = TEXT_BG;
	break;
    case TEXT_BG:
	n = TEXT_FG;
	break;
    case MOUSE_FG:
	n = MOUSE_BG;
	break;
    case MOUSE_BG:
	n = MOUSE_FG;
	break;
#if OPT_TEK4014
    case TEK_FG:
	n = TEK_BG;
	break;
    case TEK_BG:
	n = TEK_FG;
	break;
#endif
#if OPT_HIGHLIGHT_COLOR
    case HIGHLIGHT_FG:
	n = HIGHLIGHT_BG;
	break;
    case HIGHLIGHT_BG:
	n = HIGHLIGHT_FG;
	break;
#endif
    default:
	break;
    }
    return n;
}

static void
ReportColorRequest(XtermWidget xw, int ndx, int final)
{
    if (AllowColorOps(xw, ecGetColor)) {
	XColor color;
	Colormap cmap = xw->core.colormap;
	char buffer[80];

	/*
	 * ChangeColorsRequest() has "always" chosen the opposite color when
	 * reverse-video is set.  Report this as the original color index, but
	 * reporting the opposite color which would be used.
	 */
	int i = (xw->misc.re_verse) ? oppositeColor(ndx) : ndx;

	GetOldColors(xw);
	color.pixel = xw->work.oldColors->colors[ndx];
	XQueryColor(TScreenOf(xw)->display, cmap, &color);
	sprintf(buffer, "%d;rgb:%04x/%04x/%04x", i + 10,
		color.red,
		color.green,
		color.blue);
	TRACE(("ReportColorRequest #%d: 0x%06lx as %s\n",
	       ndx, xw->work.oldColors->colors[ndx], buffer));
	unparseputc1(xw, ANSI_OSC);
	unparseputs(xw, buffer);
	unparseputc1(xw, final);
	unparse_end(xw);
    }
}

static Bool
UpdateOldColors(XtermWidget xw GCC_UNUSED, ScrnColors * pNew)
{
    int i;

    /* if we were going to free old colors, this would be the place to
     * do it.   I've decided not to (for now), because it seems likely
     * that we'd have a small set of colors we use over and over, and that
     * we could save some overhead this way.   The only case in which this
     * (clearly) fails is if someone is trying a boatload of colors, in
     * which case they can restart xterm
     */
    for (i = 0; i < NCOLORS; i++) {
	if (COLOR_DEFINED(pNew, i)) {
	    if (xw->work.oldColors->names[i] != NULL) {
		XtFree(xw->work.oldColors->names[i]);
		xw->work.oldColors->names[i] = NULL;
	    }
	    if (pNew->names[i]) {
		xw->work.oldColors->names[i] = pNew->names[i];
	    }
	    xw->work.oldColors->colors[i] = pNew->colors[i];
	}
    }
    return (True);
}

/*
 * OSC codes are constant, but the indices for the color arrays depend on how
 * xterm is compiled.
 */
static int
OscToColorIndex(OscTextColors mode)
{
    int result = 0;

#define CASE(name) case OSC_##name: result = name; break
    switch (mode) {
	CASE(TEXT_FG);
	CASE(TEXT_BG);
	CASE(TEXT_CURSOR);
	CASE(MOUSE_FG);
	CASE(MOUSE_BG);
#if OPT_TEK4014
	CASE(TEK_FG);
	CASE(TEK_BG);
#endif
#if OPT_HIGHLIGHT_COLOR
	CASE(HIGHLIGHT_BG);
	CASE(HIGHLIGHT_FG);
#endif
#if OPT_TEK4014
	CASE(TEK_CURSOR);
#endif
    case OSC_NCOLORS:
	break;
    }
    return result;
}

static Bool
ChangeColorsRequest(XtermWidget xw,
		    int start,
		    char *names,
		    int final)
{
    Bool result = False;
    ScrnColors newColors;

    TRACE(("ChangeColorsRequest start=%d, names='%s'\n", start, names));

    if (GetOldColors(xw)) {
	int i;

	newColors.which = 0;
	for (i = 0; i < NCOLORS; i++) {
	    newColors.names[i] = NULL;
	}
	for (i = start; i < OSC_NCOLORS; i++) {
	    int ndx = OscToColorIndex((OscTextColors) i);
	    if (xw->misc.re_verse)
		ndx = oppositeColor(ndx);

	    if (IsEmpty(names)) {
		newColors.names[ndx] = NULL;
	    } else {
		char *thisName = ((names[0] == ';') ? NULL : names);

		names = strchr(names, ';');
		if (names != NULL) {
		    *names++ = '\0';
		}
		if (thisName != 0) {
		    if (!strcmp(thisName, "?")) {
			ReportColorRequest(xw, ndx, final);
		    } else if (!xw->work.oldColors->names[ndx]
			       || strcmp(thisName, xw->work.oldColors->names[ndx])) {
			AllocateTermColor(xw, &newColors, ndx, thisName, False);
		    }
		}
	    }
	}

	if (newColors.which != 0) {
	    ChangeColors(xw, &newColors);
	    UpdateOldColors(xw, &newColors);
	}
	result = True;
    }
    return result;
}

static Bool
ResetColorsRequest(XtermWidget xw,
		   int code)
{
    Bool result = False;

    (void) xw;
    (void) code;

    TRACE(("ResetColorsRequest code=%d\n", code));

#if OPT_COLOR_RES
    if (GetOldColors(xw)) {
	ScrnColors newColors;
	const char *thisName;
	int ndx = OscToColorIndex((OscTextColors) (code - OSC_RESET));

	if (xw->misc.re_verse)
	    ndx = oppositeColor(ndx);

	thisName = xw->screen.Tcolors[ndx].resource;

	newColors.which = 0;
	newColors.names[ndx] = NULL;

	if (thisName != 0
	    && xw->work.oldColors->names[ndx] != 0
	    && strcmp(thisName, xw->work.oldColors->names[ndx])) {
	    AllocateTermColor(xw, &newColors, ndx, thisName, False);

	    if (newColors.which != 0) {
		ChangeColors(xw, &newColors);
		UpdateOldColors(xw, &newColors);
	    }
	}
	result = True;
    }
#endif
    return result;
}

#if OPT_SHIFT_FONTS
/*
 * Initially, 'source' points to '#' or '?'.
 *
 * Look for an optional sign and optional number.  If those are found, lookup
 * the corresponding menu font entry.
 */
static int
ParseShiftedFont(XtermWidget xw, String source, String *target)
{
    TScreen *screen = TScreenOf(xw);
    int num = screen->menu_font_number;
    int rel = 0;

    if (*++source == '+') {
	rel = 1;
	source++;
    } else if (*source == '-') {
	rel = -1;
	source++;
    }

    if (isdigit(CharOf(*source))) {
	int val = atoi(source);
	if (rel > 0)
	    rel = val;
	else if (rel < 0)
	    rel = -val;
	else
	    num = val;
    }

    if (rel != 0) {
	num = lookupRelativeFontSize(xw,
				     screen->menu_font_number, rel);

    }
    TRACE(("ParseShiftedFont(%s) ->%d (%s)\n", *target, num, source));
    *target = source;
    return num;
}

static void
QueryFontRequest(XtermWidget xw, String buf, int final)
{
    if (AllowFontOps(xw, efGetFont)) {
	TScreen *screen = TScreenOf(xw);
	Bool success = True;
	int num;
	String base = buf + 1;
	const char *name = 0;

	num = ParseShiftedFont(xw, buf, &buf);
	if (num < 0
	    || num > fontMenu_lastBuiltin) {
	    Bell(xw, XkbBI_MinorError, 0);
	    success = False;
	} else {
#if OPT_RENDERFONT
	    if (UsingRenderFont(xw)) {
		name = getFaceName(xw, False);
	    } else
#endif
	    if ((name = screen->MenuFontName(num)) == 0) {
		success = False;
	    }
	}

	unparseputc1(xw, ANSI_OSC);
	unparseputs(xw, "50");

	if (success) {
	    unparseputc(xw, ';');
	    if (buf >= base) {
		/* identify the font-entry, unless it is the current one */
		if (*buf != '\0') {
		    char temp[10];

		    unparseputc(xw, '#');
		    sprintf(temp, "%d", num);
		    unparseputs(xw, temp);
		    if (*name != '\0')
			unparseputc(xw, ' ');
		}
	    }
	    unparseputs(xw, name);
	}

	unparseputc1(xw, final);
	unparse_end(xw);
    }
}

static void
ChangeFontRequest(XtermWidget xw, String buf)
{
    if (AllowFontOps(xw, efSetFont)) {
	TScreen *screen = TScreenOf(xw);
	Bool success = True;
	int num;
	VTFontNames fonts;
	char *name;

	/*
	 * If the font specification is a "#", followed by an optional sign and
	 * optional number, lookup the corresponding menu font entry.
	 *
	 * Further, if the "#", etc., is followed by a font name, use that
	 * to load the font entry.
	 */
	if (*buf == '#') {
	    num = ParseShiftedFont(xw, buf, &buf);

	    if (num < 0
		|| num > fontMenu_lastBuiltin) {
		Bell(xw, XkbBI_MinorError, 0);
		success = False;
	    } else {
		/*
		 * Skip past the optional number, and any whitespace to look
		 * for a font specification within the control.
		 */
		while (isdigit(CharOf(*buf))) {
		    ++buf;
		}
		while (isspace(CharOf(*buf))) {
		    ++buf;
		}
#if OPT_RENDERFONT
		if (UsingRenderFont(xw)) {
		    /* EMPTY */
		    /* there is only one font entry to load */
		    ;
		} else
#endif
		{
		    /*
		     * Normally there is no font specified in the control.
		     * But if there is, simply overwrite the font entry.
		     */
		    if (*buf == '\0') {
			if ((buf = screen->MenuFontName(num)) == 0) {
			    success = False;
			}
		    }
		}
	    }
	} else {
	    num = screen->menu_font_number;
	}
	name = x_strtrim(buf);
	if (screen->EscapeFontName()) {
	    FREE_STRING(screen->EscapeFontName());
	    screen->EscapeFontName() = 0;
	}
	if (success && !IsEmpty(name)) {
#if OPT_RENDERFONT
	    if (UsingRenderFont(xw)) {
		setFaceName(xw, name);
		xtermUpdateFontInfo(xw, True);
	    } else
#endif
	    {
		memset(&fonts, 0, sizeof(fonts));
		fonts.f_n = name;
		SetVTFont(xw, num, True, &fonts);
		if (num == screen->menu_font_number &&
		    num != fontMenu_fontescape) {
		    screen->EscapeFontName() = x_strdup(name);
		}
	    }
	} else {
	    Bell(xw, XkbBI_MinorError, 0);
	}
	update_font_escape();
	free(name);
    }
}
#endif /* OPT_SHIFT_FONTS */

/***====================================================================***/

void
do_osc(XtermWidget xw, Char *oscbuf, size_t len, int final)
{
    TScreen *screen = TScreenOf(xw);
    int mode;
    Char *cp;
    int state = 0;
    char *buf = 0;
    char temp[2];
#if OPT_ISO_COLORS
    int ansi_colors = 0;
#endif
    Bool need_data = True;
    Bool optional_data = False;

    TRACE(("do_osc %s\n", oscbuf));

    (void) screen;

    /*
     * Lines should be of the form <OSC> number ; string <ST>, however
     * older xterms can accept <BEL> as a final character.  We will respond
     * with the same final character as the application sends to make this
     * work better with shell scripts, which may have trouble reading an
     * <ESC><backslash>, which is the 7-bit equivalent to <ST>.
     */
    mode = 0;
    for (cp = oscbuf; *cp != '\0'; cp++) {
	switch (state) {
	case 0:
	    if (isdigit(*cp)) {
		mode = 10 * mode + (*cp - '0');
		if (mode > 65535) {
		    TRACE(("do_osc found unknown mode %d\n", mode));
		    return;
		}
		break;
	    }
	    /* FALLTHRU */
	case 1:
	    if (*cp != ';') {
		TRACE(("do_osc did not find semicolon offset %d\n",
		       (int) (cp - oscbuf)));
		return;
	    }
	    state = 2;
	    break;
	case 2:
	    buf = (char *) cp;
	    state = 3;
	    /* FALLTHRU */
	default:
	    if (!xtermIsPrintable(xw, &cp, oscbuf + len)) {
		switch (mode) {
		case 0:
		case 1:
		case 2:
		    break;
		default:
		    TRACE(("do_osc found nonprinting char %02X offset %d\n",
			   CharOf(*cp),
			   (int) (cp - oscbuf)));
		    return;
		}
	    }
	}
    }

    /*
     * Check if the palette changed and there are no more immediate changes
     * that could be deferred to the next repaint.
     */
    if (xw->work.palette_changed) {
	switch (mode) {
	case 3:		/* change X property */
	case 30:		/* Konsole (unused) */
	case 31:		/* Konsole (unused) */
	case 50:		/* font operations */
	case 51:		/* Emacs (unused) */
#if OPT_PASTE64
	case 52:		/* selection data */
#endif
	    TRACE(("forced repaint after palette changed\n"));
	    xw->work.palette_changed = False;
	    xtermRepaint(xw);
	    break;
	}
    }

    /*
     * Most OSC controls other than resets require data.  Handle the others as
     * a special case.
     */
    switch (mode) {
    case 50:
#if OPT_ISO_COLORS
    case OSC_Reset(4):
    case OSC_Reset(5):
	need_data = False;
	optional_data = True;
	break;
    case OSC_Reset(OSC_TEXT_FG):
    case OSC_Reset(OSC_TEXT_BG):
    case OSC_Reset(OSC_TEXT_CURSOR):
    case OSC_Reset(OSC_MOUSE_FG):
    case OSC_Reset(OSC_MOUSE_BG):
#if OPT_HIGHLIGHT_COLOR
    case OSC_Reset(OSC_HIGHLIGHT_BG):
    case OSC_Reset(OSC_HIGHLIGHT_FG):
#endif
#if OPT_TEK4014
    case OSC_Reset(OSC_TEK_FG):
    case OSC_Reset(OSC_TEK_BG):
    case OSC_Reset(OSC_TEK_CURSOR):
#endif
	need_data = False;
	break;
#endif
    default:
	break;
    }

    /*
     * Check if we have data when we want, and not when we do not want it.
     * Either way, that is a malformed control sequence, and will be ignored.
     */
    if (IsEmpty(buf)) {
	if (need_data) {
	    TRACE(("do_osc found no data\n"));
	    return;
	}
	temp[0] = '\0';
	buf = temp;
    } else if (!need_data && !optional_data) {
	TRACE(("do_osc found unwanted data\n"));
	return;
    }

    switch (mode) {
    case 0:			/* new icon name and title */
	ChangeIconName(xw, buf);
	ChangeTitle(xw, buf);
	break;

    case 1:			/* new icon name only */
	ChangeIconName(xw, buf);
	break;

    case 2:			/* new title only */
	ChangeTitle(xw, buf);
	break;

    case 3:			/* change X property */
	if (AllowWindowOps(xw, ewSetXprop))
	    ChangeXprop(buf);
	break;
#if OPT_ISO_COLORS
    case 5:
	ansi_colors = NUM_ANSI_COLORS;
	/* FALLTHRU */
    case 4:
	if (ChangeAnsiColorRequest(xw, buf, ansi_colors, final))
	    xw->work.palette_changed = True;
	break;
    case 6:
	/* FALLTHRU */
    case OSC_Reset(6):
	TRACE(("parse colorXXMode:%s\n", buf));
	while (*buf != '\0') {
	    long which = 0;
	    long value = 0;
	    char *next;
	    if (*buf == ';') {
		++buf;
	    } else {
		which = strtol(buf, &next, 10);
		if (!PartS2L(buf, next) || (which < 0))
		    break;
		buf = next;
		if (*buf == ';')
		    ++buf;
	    }
	    if (*buf == ';') {
		++buf;
	    } else {
		value = strtol(buf, &next, 10);
		if (!PartS2L(buf, next) || (value < 0))
		    break;
		buf = next;
		if (*buf == ';')
		    ++buf;
	    }
	    TRACE(("updating colorXXMode which=%ld, value=%ld\n", which, value));
	    switch (which) {
	    case 0:
		screen->colorBDMode = (value != 0);
		break;
	    case 1:
		screen->colorULMode = (value != 0);
		break;
	    case 2:
		screen->colorBLMode = (value != 0);
		break;
	    case 3:
		screen->colorRVMode = (value != 0);
		break;
#if OPT_WIDE_ATTRS
	    case 4:
		screen->colorITMode = (value != 0);
		break;
#endif
	    default:
		TRACE(("...unknown colorXXMode\n"));
		break;
	    }
	}
	break;
    case OSC_Reset(5):
	ansi_colors = NUM_ANSI_COLORS;
	/* FALLTHRU */
    case OSC_Reset(4):
	if (ResetAnsiColorRequest(xw, buf, ansi_colors))
	    xw->work.palette_changed = True;
	break;
#endif
    case OSC_TEXT_FG:
    case OSC_TEXT_BG:
    case OSC_TEXT_CURSOR:
    case OSC_MOUSE_FG:
    case OSC_MOUSE_BG:
#if OPT_HIGHLIGHT_COLOR
    case OSC_HIGHLIGHT_BG:
    case OSC_HIGHLIGHT_FG:
#endif
#if OPT_TEK4014
    case OSC_TEK_FG:
    case OSC_TEK_BG:
    case OSC_TEK_CURSOR:
#endif
	if (xw->misc.dynamicColors) {
	    ChangeColorsRequest(xw, mode, buf, final);
	}
	break;
    case OSC_Reset(OSC_TEXT_FG):
    case OSC_Reset(OSC_TEXT_BG):
    case OSC_Reset(OSC_TEXT_CURSOR):
    case OSC_Reset(OSC_MOUSE_FG):
    case OSC_Reset(OSC_MOUSE_BG):
#if OPT_HIGHLIGHT_COLOR
    case OSC_Reset(OSC_HIGHLIGHT_BG):
    case OSC_Reset(OSC_HIGHLIGHT_FG):
#endif
#if OPT_TEK4014
    case OSC_Reset(OSC_TEK_FG):
    case OSC_Reset(OSC_TEK_BG):
    case OSC_Reset(OSC_TEK_CURSOR):
#endif
	if (xw->misc.dynamicColors) {
	    ResetColorsRequest(xw, mode);
	}
	break;

    case 30:
    case 31:
	/* reserved for Konsole (Stephan Binner <Stephan.Binner@gmx.de>) */
	break;

#ifdef ALLOWLOGGING
    case 46:			/* new log file */
#ifdef ALLOWLOGFILECHANGES
	/*
	 * Warning, enabling this feature allows people to overwrite
	 * arbitrary files accessible to the person running xterm.
	 */
	if (strcmp(buf, "?")) {
	    char *bp;
	    if ((bp = x_strdup(buf)) != NULL) {
		if (screen->logfile)
		    free(screen->logfile);
		screen->logfile = bp;
		break;
	    }
	}
#endif
	Bell(xw, XkbBI_Info, 0);
	Bell(xw, XkbBI_Info, 0);
	break;
#endif /* ALLOWLOGGING */

    case 50:
#if OPT_SHIFT_FONTS
	if (*buf == '?') {
	    QueryFontRequest(xw, buf, final);
	} else if (xw->misc.shift_fonts) {
	    ChangeFontRequest(xw, buf);
	}
#endif /* OPT_SHIFT_FONTS */
	break;
    case 51:
	/* reserved for Emacs shell (Rob Mayoff <mayoff@dqd.com>) */
	break;

#if OPT_PASTE64
    case 52:
	ManipulateSelectionData(xw, screen, buf, final);
	break;
#endif
	/*
	 * One could write code to send back the display and host names,
	 * but that could potentially open a fairly nasty security hole.
	 */
    default:
	TRACE(("do_osc - unrecognized code\n"));
	break;
    }
    unparse_end(xw);
}

/*
 * Parse one nibble of a hex byte from the OSC string.  We have removed the
 * string-terminator (replacing it with a null), so the only other delimiter
 * that is expected is semicolon.  Ignore other characters (Ray Neuman says
 * "real" terminals accept commas in the string definitions).
 */
static int
udk_value(const char **cp)
{
    int result = -1;

    for (;;) {
	int c;

	if ((c = **cp) != '\0')
	    *cp = *cp + 1;
	if (c == ';' || c == '\0')
	    break;
	if ((result = x_hex2int(c)) >= 0)
	    break;
    }

    return result;
}

void
reset_decudk(XtermWidget xw)
{
    int n;
    for (n = 0; n < MAX_UDK; n++) {
	if (xw->work.user_keys[n].str != 0) {
	    free(xw->work.user_keys[n].str);
	    xw->work.user_keys[n].str = 0;
	    xw->work.user_keys[n].len = 0;
	}
    }
}

/*
 * Parse the data for DECUDK (user-defined keys).
 */
static void
parse_decudk(XtermWidget xw, const char *cp)
{
    while (*cp) {
	const char *base = cp;
	char *str = TextAlloc(strlen(cp) + 2);
	unsigned key = 0;
	int len = 0;

	if (str == NULL)
	    break;

	while (isdigit(CharOf(*cp)))
	    key = (key * 10) + (unsigned) (*cp++ - '0');

	if (*cp == '/') {
	    int lo, hi;

	    cp++;
	    while ((hi = udk_value(&cp)) >= 0
		   && (lo = udk_value(&cp)) >= 0) {
		str[len++] = (char) ((hi << 4) | lo);
	    }
	}
	if (len > 0 && key < MAX_UDK) {
	    str[len] = '\0';
	    if (xw->work.user_keys[key].str != 0)
		free(xw->work.user_keys[key].str);
	    xw->work.user_keys[key].str = str;
	    xw->work.user_keys[key].len = len;
	} else {
	    free(str);
	}
	if (*cp == ';')
	    cp++;
	if (cp == base)		/* badly-formed sequence - bail out */
	    break;
    }
}

/*
 * Parse numeric parameters.  Normally we use a state machine to simplify
 * interspersing with control characters, but have the string already.
 */
static void
parse_ansi_params(ANSI *params, const char **string)
{
    const char *cp = *string;
    ParmType nparam = 0;
    int last_empty = 1;

    memset(params, 0, sizeof(*params));
    while (*cp != '\0') {
	Char ch = CharOf(*cp++);

	if (isdigit(ch)) {
	    last_empty = 0;
	    if (nparam < NPARAM) {
		params->a_param[nparam] =
		    (ParmType) ((params->a_param[nparam] * 10)
				+ (ch - '0'));
	    }
	} else if (ch == ';') {
	    last_empty = 1;
	    nparam++;
	} else if (ch < 32) {
	    /* EMPTY */ ;
	} else {
	    /* should be 0x30 to 0x7e */
	    params->a_final = ch;
	    break;
	}
    }

    *string = cp;
    if (!last_empty)
	nparam++;
    if (nparam > NPARAM)
	params->a_nparam = NPARAM;
    else
	params->a_nparam = nparam;
}

#if OPT_TRACE
#define SOFT_WIDE 10
#define SOFT_HIGH 20

static void
parse_decdld(ANSI *params, const char *string)
{
    char DscsName[8];
    int len;
    int Pfn = params->a_param[0];
    int Pcn = params->a_param[1];
    int Pe = params->a_param[2];
    int Pcmw = params->a_param[3];
    int Pw = params->a_param[4];
    int Pt = params->a_param[5];
    int Pcmh = params->a_param[6];
    int Pcss = params->a_param[7];

    int start_char = Pcn + 0x20;
    int char_wide = ((Pcmw == 0)
		     ? (Pcss ? 6 : 10)
		     : (Pcmw > 4
			? Pcmw
			: (Pcmw + 3)));
    int char_high = ((Pcmh == 0)
		     ? ((Pcmw >= 2 && Pcmw <= 4)
			? 10
			: 20)
		     : Pcmh);
    Char ch;
    Char bits[SOFT_HIGH][SOFT_WIDE];
    Bool first = True;
    Bool prior = False;
    int row = 0, col = 0;

    TRACE(("Parsing DECDLD\n"));
    TRACE(("  font number   %d\n", Pfn));
    TRACE(("  starting char %d\n", Pcn));
    TRACE(("  erase control %d\n", Pe));
    TRACE(("  char-width    %d\n", Pcmw));
    TRACE(("  font-width    %d\n", Pw));
    TRACE(("  text/full     %d\n", Pt));
    TRACE(("  char-height   %d\n", Pcmh));
    TRACE(("  charset-size  %d\n", Pcss));

    if (Pfn > 1
	|| Pcn > 95
	|| Pe > 2
	|| Pcmw > 10
	|| Pcmw == 1
	|| Pt > 2
	|| Pcmh > 20
	|| Pcss > 1
	|| char_wide > SOFT_WIDE
	|| char_high > SOFT_HIGH) {
	TRACE(("DECDLD illegal parameter\n"));
	return;
    }

    len = 0;
    while (*string != '\0') {
	ch = CharOf(*string++);
	if (ch >= ANSI_SPA && ch <= 0x2f) {
	    if (len < 2)
		DscsName[len++] = (char) ch;
	} else if (ch >= 0x30 && ch <= 0x7e) {
	    DscsName[len++] = (char) ch;
	    break;
	}
    }
    DscsName[len] = 0;
    TRACE(("  Dscs name     '%s'\n", DscsName));

    TRACE(("  character matrix %dx%d\n", char_high, char_wide));
    while (*string != '\0') {
	if (first) {
	    TRACE(("Char %d:\n", start_char));
	    if (prior) {
		for (row = 0; row < char_high; ++row) {
		    TRACE(("%.*s\n", char_wide, bits[row]));
		}
	    }
	    prior = False;
	    first = False;
	    for (row = 0; row < char_high; ++row) {
		for (col = 0; col < char_wide; ++col) {
		    bits[row][col] = '.';
		}
	    }
	    row = col = 0;
	}
	ch = CharOf(*string++);
	if (ch >= 0x3f && ch <= 0x7e) {
	    int n;

	    ch = CharOf(ch - 0x3f);
	    for (n = 0; n < 6; ++n) {
		bits[row + n][col] = CharOf((ch & (1 << n)) ? '*' : '.');
	    }
	    col += 1;
	    prior = True;
	} else if (ch == '/') {
	    row += 6;
	    col = 0;
	} else if (ch == ';') {
	    first = True;
	    ++start_char;
	}
    }
}
#else
#define parse_decdld(p,q)	/* nothing */
#endif

void
do_dcs(XtermWidget xw, Char *dcsbuf, size_t dcslen)
{
    TScreen *screen = TScreenOf(xw);
    char reply[BUFSIZ];
    const char *cp = (const char *) dcsbuf;
    Bool okay;
    ANSI params;

    TRACE(("do_dcs(%s:%lu)\n", (char *) dcsbuf, (unsigned long) dcslen));

    if (dcslen != strlen(cp))
	/* shouldn't have nulls in the string */
	return;

    switch (*cp) {		/* intermediate character, or parameter */
    case '$':			/* DECRQSS */
	okay = True;

	cp++;
	if (*cp++ == 'q') {
	    if (!strcmp(cp, "\"q")) {	/* DECSCA */
		TRACE(("DECRQSS -> DECSCA\n"));
		sprintf(reply, "%d%s",
			(screen->protected_mode == DEC_PROTECT)
			&& (xw->flags & PROTECTED) ? 1 : 0,
			cp);
	    } else if (!strcmp(cp, "\"p")) {	/* DECSCL */
		if (screen->vtXX_level < 2) {
		    /* actually none of DECRQSS is valid for vt100's */
		    break;
		}
		TRACE(("DECRQSS -> DECSCL\n"));
		sprintf(reply, "%d%s%s",
			(screen->vtXX_level ?
			 screen->vtXX_level : 1) + 60,
			(screen->vtXX_level >= 2)
			? (screen->control_eight_bits
			   ? ";0" : ";1")
			: "",
			cp);
	    } else if (!strcmp(cp, "r")) {	/* DECSTBM */
		TRACE(("DECRQSS -> DECSTBM\n"));
		sprintf(reply, "%d;%dr",
			screen->top_marg + 1,
			screen->bot_marg + 1);
	    } else if (!strcmp(cp, "s")) {	/* DECSLRM */
		if (screen->vtXX_level >= 4) {	/* VT420 */
		    TRACE(("DECRQSS -> DECSLRM\n"));
		    sprintf(reply, "%d;%ds",
			    screen->lft_marg + 1,
			    screen->rgt_marg + 1);
		} else {
		    okay = False;
		}
	    } else if (!strcmp(cp, "m")) {	/* SGR */
		TRACE(("DECRQSS -> SGR\n"));
		xtermFormatSGR(xw, reply, xw->flags, xw->cur_foreground, xw->cur_background);
		strcat(reply, "m");
	    } else if (!strcmp(cp, " q")) {	/* DECSCUSR */
		int code = STEADY_BLOCK;
		if (isCursorUnderline(screen))
		    code = STEADY_UNDERLINE;
		else if (isCursorBar(screen))
		    code = STEADY_BAR;
#if OPT_BLINK_CURS
		if (screen->cursor_blink_esc != 0)
		    code -= 1;
#endif
		TRACE(("reply DECSCUSR\n"));
		sprintf(reply, "%d%s", code, cp);
	    } else {
		okay = False;
	    }

	    unparseputc1(xw, ANSI_DCS);
	    unparseputc(xw, okay ? '1' : '0');
	    unparseputc(xw, '$');
	    unparseputc(xw, 'r');
	    cp = reply;
	    unparseputs(xw, cp);
	    unparseputc1(xw, ANSI_ST);
	} else {
	    unparseputc(xw, ANSI_CAN);
	}
	break;
#if OPT_TCAP_QUERY
    case '+':
	cp++;
	switch (*cp) {
	case 'p':
	    if (AllowTcapOps(xw, etSetTcap)) {
		set_termcap(xw, cp + 1);
	    }
	    break;
	case 'q':
	    if (AllowTcapOps(xw, etGetTcap)) {
		Bool fkey;
		unsigned state;
		int code;
		const char *tmp;
		const char *parsed = ++cp;

		code = xtermcapKeycode(xw, &parsed, &state, &fkey);

		unparseputc1(xw, ANSI_DCS);

		unparseputc(xw, code >= 0 ? '1' : '0');

		unparseputc(xw, '+');
		unparseputc(xw, 'r');

		while (*cp != 0 && (code >= -1)) {
		    if (cp == parsed)
			break;	/* no data found, error */

		    for (tmp = cp; tmp != parsed; ++tmp)
			unparseputc(xw, *tmp);

		    if (code >= 0) {
			unparseputc(xw, '=');
			screen->tc_query_code = code;
			screen->tc_query_fkey = fkey;
#if OPT_ISO_COLORS
			/* XK_COLORS is a fake code for the "Co" entry (maximum
			 * number of colors) */
			if (code == XK_COLORS) {
			    unparseputn(xw, NUM_ANSI_COLORS);
			} else
#endif
			if (code == XK_TCAPNAME) {
			    unparseputs(xw, resource.term_name);
			} else {
			    XKeyEvent event;
			    event.state = state;
			    Input(xw, &event, False);
			}
			screen->tc_query_code = -1;
		    } else {
			break;	/* no match found, error */
		    }

		    cp = parsed;
		    if (*parsed == ';') {
			unparseputc(xw, *parsed++);
			cp = parsed;
			code = xtermcapKeycode(xw, &parsed, &state, &fkey);
		    }
		}
		unparseputc1(xw, ANSI_ST);
	    }
	    break;
	}
	break;
#endif
    default:
	if (screen->terminal_id == 125 ||
	    screen->vtXX_level >= 2) {	/* VT220 */
	    parse_ansi_params(&params, &cp);
	    switch (params.a_final) {
	    case 'p':
#if OPT_REGIS_GRAPHICS
		if (screen->terminal_id == 125 ||
		    screen->terminal_id == 240 ||
		    screen->terminal_id == 241 ||
		    screen->terminal_id == 330 ||
		    screen->terminal_id == 340) {
		    parse_regis(xw, &params, cp);
		}
#else
		TRACE(("ignoring ReGIS graphic (compilation flag not enabled)\n"));
#endif
		break;
	    case 'q':
#if OPT_SIXEL_GRAPHICS
		if (screen->terminal_id == 125 ||
		    screen->terminal_id == 240 ||
		    screen->terminal_id == 241 ||
		    screen->terminal_id == 330 ||
		    screen->terminal_id == 340 ||
		    screen->terminal_id == 382) {
		    (void) parse_sixel(xw, &params, cp);
		}
#else
		TRACE(("ignoring sixel graphic (compilation flag not enabled)\n"));
#endif
		break;
	    case '|':		/* DECUDK */
		if (screen->vtXX_level >= 2) {	/* VT220 */
		    if (params.a_param[0] == 0)
			reset_decudk(xw);
		    parse_decudk(xw, cp);
		}
		break;
	    case L_CURL:	/* DECDLD */
		if (screen->vtXX_level >= 2) {	/* VT220 */
		    parse_decdld(&params, cp);
		}
		break;
	    }
	}
	break;
    }
    unparse_end(xw);
}

#if OPT_DEC_RECTOPS
enum {
    mdUnknown = 0,
    mdMaybeSet = 1,
    mdMaybeReset = 2,
    mdAlwaysSet = 3,
    mdAlwaysReset = 4
};

#define MdBool(bool)      ((bool) ? mdMaybeSet : mdMaybeReset)
#define MdFlag(mode,flag) MdBool((mode) & (flag))

/*
 * Reply is the same format as the query, with pair of mode/value:
 * 0 - not recognized
 * 1 - set
 * 2 - reset
 * 3 - permanently set
 * 4 - permanently reset
 * Only one mode can be reported at a time.
 */
void
do_ansi_rqm(XtermWidget xw, int nparams, int *params)
{
    ANSI reply;
    int count = 0;

    TRACE(("do_ansi_rqm %d:%d\n", nparams, params[0]));
    memset(&reply, 0, sizeof(reply));

    if (nparams >= 1) {
	int result = mdUnknown;

	/* DECRQM can only ask about one mode at a time */
	switch (params[0]) {
	case 1:		/* GATM */
	    result = mdAlwaysReset;
	    break;
	case 2:
	    result = MdFlag(xw->keyboard.flags, MODE_KAM);
	    break;
	case 3:		/* CRM */
	    result = mdMaybeReset;
	    break;
	case 4:
	    result = MdFlag(xw->flags, INSERT);
	    break;
	case 5:		/* SRTM */
	case 7:		/* VEM */
	case 10:		/* HEM */
	case 11:		/* PUM */
	    result = mdAlwaysReset;
	    break;
	case 12:
	    result = MdFlag(xw->keyboard.flags, MODE_SRM);
	    break;
	case 13:		/* FEAM */
	case 14:		/* FETM */
	case 15:		/* MATM */
	case 16:		/* TTM */
	case 17:		/* SATM */
	case 18:		/* TSM */
	case 19:		/* EBM */
	    result = mdAlwaysReset;
	    break;
	case 20:
	    result = MdFlag(xw->flags, LINEFEED);
	    break;
	}
	reply.a_param[count++] = (ParmType) params[0];
	reply.a_param[count++] = (ParmType) result;
    }
    reply.a_type = ANSI_CSI;
    reply.a_nparam = (ParmType) count;
    reply.a_inters = '$';
    reply.a_final = 'y';
    unparseseq(xw, &reply);
}

void
do_dec_rqm(XtermWidget xw, int nparams, int *params)
{
    ANSI reply;
    int count = 0;

    TRACE(("do_dec_rqm %d:%d\n", nparams, params[0]));
    memset(&reply, 0, sizeof(reply));

    if (nparams >= 1) {
	TScreen *screen = TScreenOf(xw);
	int result = mdUnknown;

	/* DECRQM can only ask about one mode at a time */
	switch ((DECSET_codes) params[0]) {
	case srm_DECCKM:
	    result = MdFlag(xw->keyboard.flags, MODE_DECCKM);
	    break;
	case srm_DECANM:	/* ANSI/VT52 mode      */
#if OPT_VT52_MODE
	    result = MdBool(screen->vtXX_level >= 1);
#else
	    result = mdMaybeSet;
#endif
	    break;
	case srm_DECCOLM:
	    result = MdFlag(xw->flags, IN132COLUMNS);
	    break;
	case srm_DECSCLM:	/* (slow scroll)        */
	    result = MdFlag(xw->flags, SMOOTHSCROLL);
	    break;
	case srm_DECSCNM:
	    result = MdFlag(xw->flags, REVERSE_VIDEO);
	    break;
	case srm_DECOM:
	    result = MdFlag(xw->flags, ORIGIN);
	    break;
	case srm_DECAWM:
	    result = MdFlag(xw->flags, WRAPAROUND);
	    break;
	case srm_DECARM:
	    result = mdAlwaysReset;
	    break;
	case srm_X10_MOUSE:	/* X10 mouse                    */
	    result = MdBool(screen->send_mouse_pos == X10_MOUSE);
	    break;
#if OPT_TOOLBAR
	case srm_RXVT_TOOLBAR:
	    result = MdBool(resource.toolBar);
	    break;
#endif
#if OPT_BLINK_CURS
	case srm_ATT610_BLINK:	/* AT&T 610: Start/stop blinking cursor */
	    result = MdBool(screen->cursor_blink_esc);
	    break;
	case srm_CURSOR_BLINK_OPS:
	    switch (screen->cursor_blink) {
	    case cbTrue:
		result = mdMaybeSet;
		break;
	    case cbFalse:
		result = mdMaybeReset;
		break;
	    case cbAlways:
		result = mdAlwaysSet;
		break;
	    case cbLAST:
		/* FALLTHRU */
	    case cbNever:
		result = mdAlwaysReset;
		break;
	    }
	    break;
	case srm_XOR_CURSOR_BLINKS:
	    result = (screen->cursor_blink_xor
		      ? mdAlwaysSet
		      : mdAlwaysReset);
	    break;
#endif
	case srm_DECPFF:	/* print form feed */
	    result = MdBool(PrinterOf(screen).printer_formfeed);
	    break;
	case srm_DECPEX:	/* print extent */
	    result = MdBool(PrinterOf(screen).printer_extent);
	    break;
	case srm_DECTCEM:	/* Show/hide cursor (VT200) */
	    result = MdBool(screen->cursor_set);
	    break;
	case srm_RXVT_SCROLLBAR:
	    result = MdBool(screen->fullVwin.sb_info.width != OFF);
	    break;
#if OPT_SHIFT_FONTS
	case srm_RXVT_FONTSIZE:
	    result = MdBool(xw->misc.shift_fonts);
	    break;
#endif
#if OPT_TEK4014
	case srm_DECTEK:
	    result = MdBool(TEK4014_ACTIVE(xw));
	    break;
#endif
	case srm_132COLS:
	    result = MdBool(screen->c132);
	    break;
	case srm_CURSES_HACK:
	    result = MdBool(screen->curses);
	    break;
	case srm_DECNRCM:	/* national charset (VT220) */
	    if (screen->vtXX_level >= 2) {
		result = MdFlag(xw->flags, NATIONAL);
	    } else {
		result = 0;
	    }
	    break;
	case srm_MARGIN_BELL:	/* margin bell                  */
	    result = MdBool(screen->marginbell);
	    break;
	case srm_REVERSEWRAP:	/* reverse wraparound   */
	    result = MdFlag(xw->flags, REVERSEWRAP);
	    break;
#ifdef ALLOWLOGGING
	case srm_ALLOWLOGGING:	/* logging              */
#ifdef ALLOWLOGFILEONOFF
	    result = MdBool(screen->logging);
#endif /* ALLOWLOGFILEONOFF */
	    break;
#endif
	case srm_OPT_ALTBUF_CURSOR:	/* alternate buffer & cursor */
	    /* FALLTHRU */
	case srm_OPT_ALTBUF:
	    /* FALLTHRU */
	case srm_ALTBUF:
	    result = MdBool(screen->whichBuf);
	    break;
	case srm_DECNKM:
	    result = MdFlag(xw->keyboard.flags, MODE_DECKPAM);
	    break;
	case srm_DECBKM:
	    result = MdFlag(xw->keyboard.flags, MODE_DECBKM);
	    break;
	case srm_DECLRMM:
	    if (screen->vtXX_level >= 4) {	/* VT420 */
		result = MdFlag(xw->flags, LEFT_RIGHT);
	    } else {
		result = 0;
	    }
	    break;
#if OPT_SIXEL_GRAPHICS
	case srm_DECSDM:
	    result = MdFlag(xw->keyboard.flags, MODE_DECSDM);
	    break;
#endif
	case srm_DECNCSM:
	    if (screen->vtXX_level >= 5) {	/* VT510 */
		result = MdFlag(xw->flags, NOCLEAR_COLM);
	    } else {
		result = 0;
	    }
	    break;
	case srm_VT200_MOUSE:	/* xterm bogus sequence         */
	    result = MdBool(screen->send_mouse_pos == VT200_MOUSE);
	    break;
	case srm_VT200_HIGHLIGHT_MOUSE:	/* xterm sequence w/hilite tracking */
	    result = MdBool(screen->send_mouse_pos == VT200_HIGHLIGHT_MOUSE);
	    break;
	case srm_BTN_EVENT_MOUSE:
	    result = MdBool(screen->send_mouse_pos == BTN_EVENT_MOUSE);
	    break;
	case srm_ANY_EVENT_MOUSE:
	    result = MdBool(screen->send_mouse_pos == ANY_EVENT_MOUSE);
	    break;
#if OPT_FOCUS_EVENT
	case srm_FOCUS_EVENT_MOUSE:
	    result = MdBool(screen->send_focus_pos);
	    break;
#endif
	case srm_EXT_MODE_MOUSE:
	    /* FALLTHRU */
	case srm_SGR_EXT_MODE_MOUSE:
	    /* FALLTHRU */
	case srm_URXVT_EXT_MODE_MOUSE:
	    result = MdBool(screen->extend_coords == params[0]);
	    break;
	case srm_ALTERNATE_SCROLL:
	    result = MdBool(screen->alternateScroll);
	    break;
	case srm_RXVT_SCROLL_TTY_OUTPUT:
	    result = MdBool(screen->scrollttyoutput);
	    break;
	case srm_RXVT_SCROLL_TTY_KEYPRESS:
	    result = MdBool(screen->scrollkey);
	    break;
	case srm_EIGHT_BIT_META:
	    result = MdBool(screen->eight_bit_meta);
	    break;
#if OPT_NUM_LOCK
	case srm_REAL_NUMLOCK:
	    result = MdBool(xw->misc.real_NumLock);
	    break;
	case srm_META_SENDS_ESC:
	    result = MdBool(screen->meta_sends_esc);
	    break;
#endif
	case srm_DELETE_IS_DEL:
	    result = MdBool(screen->delete_is_del);
	    break;
#if OPT_NUM_LOCK
	case srm_ALT_SENDS_ESC:
	    result = MdBool(screen->alt_sends_esc);
	    break;
#endif
	case srm_KEEP_SELECTION:
	    result = MdBool(screen->keepSelection);
	    break;
	case srm_SELECT_TO_CLIPBOARD:
	    result = MdBool(screen->selectToClipboard);
	    break;
	case srm_BELL_IS_URGENT:
	    result = MdBool(screen->bellIsUrgent);
	    break;
	case srm_POP_ON_BELL:
	    result = MdBool(screen->poponbell);
	    break;
	case srm_KEEP_CLIPBOARD:
	    result = MdBool(screen->keepClipboard);
	    break;
	case srm_ALLOW_ALTBUF:
	    result = MdBool(xw->misc.titeInhibit);
	    break;
	case srm_SAVE_CURSOR:
	    result = MdBool(screen->sc[screen->whichBuf].saved);
	    break;
#if OPT_TCAP_FKEYS
	case srm_TCAP_FKEYS:
	    result = MdBool(xw->keyboard.type == keyboardIsTermcap);
	    break;
#endif
#if OPT_SUN_FUNC_KEYS
	case srm_SUN_FKEYS:
	    result = MdBool(xw->keyboard.type == keyboardIsSun);
	    break;
#endif
#if OPT_HP_FUNC_KEYS
	case srm_HP_FKEYS:
	    result = MdBool(xw->keyboard.type == keyboardIsHP);
	    break;
#endif
#if OPT_SCO_FUNC_KEYS
	case srm_SCO_FKEYS:
	    result = MdBool(xw->keyboard.type == keyboardIsSCO);
	    break;
#endif
	case srm_LEGACY_FKEYS:
	    result = MdBool(xw->keyboard.type == keyboardIsLegacy);
	    break;
#if OPT_SUNPC_KBD
	case srm_VT220_FKEYS:
	    result = MdBool(xw->keyboard.type == keyboardIsVT220);
	    break;
#endif
#if OPT_READLINE
	case srm_BUTTON1_MOVE_POINT:
	    result = MdBool(SCREEN_FLAG(screen, click1_moves));
	    break;
	case srm_BUTTON2_MOVE_POINT:
	    result = MdBool(SCREEN_FLAG(screen, paste_moves));
	    break;
	case srm_DBUTTON3_DELETE:
	    result = MdBool(SCREEN_FLAG(screen, dclick3_deletes));
	    break;
	case srm_PASTE_IN_BRACKET:
	    result = MdBool(SCREEN_FLAG(screen, paste_brackets));
	    break;
	case srm_PASTE_QUOTE:
	    result = MdBool(SCREEN_FLAG(screen, paste_quotes));
	    break;
	case srm_PASTE_LITERAL_NL:
	    result = MdBool(SCREEN_FLAG(screen, paste_literal_nl));
	    break;
#endif /* OPT_READLINE */
#if OPT_SIXEL_GRAPHICS
	case srm_PRIVATE_COLOR_REGISTERS:
	    result = MdBool(screen->privatecolorregisters);
	    break;
#endif
#if OPT_SIXEL_GRAPHICS
	case srm_SIXEL_SCROLLS_RIGHT:
	    result = MdBool(screen->sixel_scrolls_right);
	    break;
#endif
	default:
	    TRACE(("DATA_ERROR: requested report for unknown private mode %d\n",
		   params[0]));
	}
	reply.a_param[count++] = (ParmType) params[0];
	reply.a_param[count++] = (ParmType) result;
    }
    reply.a_type = ANSI_CSI;
    reply.a_pintro = '?';
    reply.a_nparam = (ParmType) count;
    reply.a_inters = '$';
    reply.a_final = 'y';
    unparseseq(xw, &reply);
}
#endif /* OPT_DEC_RECTOPS */

char *
udk_lookup(XtermWidget xw, int keycode, int *len)
{
    if (keycode >= 0 && keycode < MAX_UDK) {
	*len = xw->work.user_keys[keycode].len;
	return xw->work.user_keys[keycode].str;
    }
    return 0;
}

#ifdef HAVE_LIBXPM

#ifndef PIXMAP_ROOTDIR
#define PIXMAP_ROOTDIR "/usr/share/pixmaps/"
#endif

typedef struct {
    const char *name;
    const char *const *data;
} XPM_DATA;

static char *
x_find_icon(char **work, int *state, const char *suffix)
{
    const char *filename = resource.icon_hint;
    const char *prefix = PIXMAP_ROOTDIR;
    const char *larger = "_48x48";
    char *result = 0;

    if (*state >= 0) {
	if ((*state & 1) == 0)
	    suffix = "";
	if ((*state & 2) == 0)
	    larger = "";
	if ((*state & 4) == 0) {
	    prefix = "";
	} else if (!strncmp(filename, "/", (size_t) 1) ||
		   !strncmp(filename, "./", (size_t) 2) ||
		   !strncmp(filename, "../", (size_t) 3)) {
	    *state = -1;
	} else if (*state >= 8) {
	    *state = -1;
	}
    }

    if (*state >= 0) {
	size_t length;

	if (*work) {
	    free(*work);
	    *work = 0;
	}
	length = 3 + strlen(prefix) + strlen(filename) + strlen(larger) +
	    strlen(suffix);
	if ((result = malloc(length)) != 0) {
	    sprintf(result, "%s%s%s%s", prefix, filename, larger, suffix);
	    *work = result;
	}
	*state += 1;
	TRACE(("x_find_icon %d:%s\n", *state, result));
    }
    return result;
}

#if OPT_BUILTIN_XPMS
static const XPM_DATA *
BuiltInXPM(const XPM_DATA * table, Cardinal length)
{
    const char *find = resource.icon_hint;
    const XPM_DATA *result = 0;
    if (!IsEmpty(find)) {
	Cardinal n;
	for (n = 0; n < length; ++n) {
	    if (!x_strcasecmp(find, table[n].name)) {
		result = table + n;
		break;
	    }
	}

	/*
	 * As a fallback, check if the icon name matches without the lengths,
	 * which are all _HHxWW format.
	 */
	if (result == 0) {
	    const char *base = table[0].name;
	    const char *last = strchr(base, '_');
	    if (last != 0
		&& !x_strncasecmp(find, base, (unsigned) (last - base))) {
		result = table + length - 1;
	    }
	}
    }
    return result;
}
#endif /* OPT_BUILTIN_XPMS */

typedef enum {
    eHintDefault = 0		/* use the largest builtin-icon */
    ,eHintNone
    ,eHintSearch
} ICON_HINT;

static ICON_HINT
which_icon_hint(void)
{
    ICON_HINT result = eHintDefault;
    if (!IsEmpty(resource.icon_hint)) {
	if (!x_strcasecmp(resource.icon_hint, "none")) {
	    result = eHintNone;
	} else {
	    result = eHintSearch;
	}
    }
    return result;
}
#endif /* HAVE_LIBXPM */

int
getVisualDepth(XtermWidget xw)
{
    int result = 0;

    if (getVisualInfo(xw)) {
	result = xw->visInfo->depth;
    }
    return result;
}

/*
 * WM_ICON_SIZE should be honored if possible.
 */
void
xtermLoadIcon(XtermWidget xw)
{
#ifdef HAVE_LIBXPM
    Display *dpy = XtDisplay(xw);
    Pixmap myIcon = 0;
    Pixmap myMask = 0;
    char *workname = 0;
    ICON_HINT hint = which_icon_hint();
#include <builtin_icons.h>

    TRACE(("xtermLoadIcon %p:%s\n", (void *) xw, NonNull(resource.icon_hint)));

    if (hint == eHintSearch) {
	int state = 0;
	while (x_find_icon(&workname, &state, ".xpm") != 0) {
	    Pixmap resIcon = 0;
	    Pixmap shapemask = 0;
	    XpmAttributes attributes;

	    attributes.depth = (unsigned) getVisualDepth(xw);
	    attributes.valuemask = XpmDepth;

	    if (XpmReadFileToPixmap(dpy,
				    DefaultRootWindow(dpy),
				    workname,
				    &resIcon,
				    &shapemask,
				    &attributes) == XpmSuccess) {
		myIcon = resIcon;
		myMask = shapemask;
		TRACE(("...success\n"));
		break;
	    }
	}
    }

    /*
     * If no external file was found, look for the name in the built-in table.
     * If that fails, just use the biggest mini-icon.
     */
    if (myIcon == 0 && hint != eHintNone) {
	char **data;
#if OPT_BUILTIN_XPMS
	const XPM_DATA *myData = 0;
	myData = BuiltInXPM(mini_xterm_xpms, XtNumber(mini_xterm_xpms));
	if (myData == 0)
	    myData = BuiltInXPM(filled_xterm_xpms, XtNumber(filled_xterm_xpms));
	if (myData == 0)
	    myData = BuiltInXPM(xterm_color_xpms, XtNumber(xterm_color_xpms));
	if (myData == 0)
	    myData = BuiltInXPM(xterm_xpms, XtNumber(xterm_xpms));
	if (myData == 0)
	    myData = &mini_xterm_xpms[XtNumber(mini_xterm_xpms) - 1];
	data = (char **) myData->data;
#else
	data = (char **) &mini_xterm_48x48_xpm;
#endif
	if (XpmCreatePixmapFromData(dpy,
				    DefaultRootWindow(dpy),
				    data,
				    &myIcon, &myMask, 0) != 0) {
	    myIcon = 0;
	    myMask = 0;
	}
    }

    if (myIcon != 0) {
	XWMHints *hints = XGetWMHints(dpy, VShellWindow(xw));
	if (!hints)
	    hints = XAllocWMHints();

	if (hints) {
	    hints->flags |= IconPixmapHint;
	    hints->icon_pixmap = myIcon;
	    if (myMask) {
		hints->flags |= IconMaskHint;
		hints->icon_mask = myMask;
	    }

	    XSetWMHints(dpy, VShellWindow(xw), hints);
	    XFree(hints);
	    TRACE(("...loaded icon\n"));
	}
    }

    if (workname != 0)
	free(workname);

#else
    (void) xw;
#endif
}

void
ChangeGroup(XtermWidget xw, const char *attribute, char *value)
{
#if OPT_WIDE_CHARS
    static Char *converted;	/* NO_LEAKS */
#endif

    Arg args[1];
    Boolean changed = True;
    Widget w = CURRENT_EMU();
    Widget top = SHELL_OF(w);

    char *my_attr;
    char *name;
    size_t limit;
    Char *c1;
    Char *cp;

    if (!AllowTitleOps(xw))
	return;

    if (value == 0)
	value = emptyString;
    if (IsTitleMode(xw, tmSetBase16)) {
	const char *temp;
	char *test;

	value = x_decode_hex(value, &temp);
	if (*temp != '\0') {
	    free(value);
	    return;
	}
	for (test = value; *test != '\0'; ++test) {
	    if (CharOf(*test) < 32) {
		*test = '\0';
		break;
	    }
	}
    }

    c1 = (Char *) value;
    name = value;
    limit = strlen(name);
    my_attr = x_strdup(attribute);

    TRACE(("ChangeGroup(attribute=%s, value=%s)\n", my_attr, name));

    /*
     * Ignore titles that are too long to be plausible requests.
     */
    if (limit > 0 && limit < 1024) {

	/*
	 * After all decoding, overwrite nonprintable characters with '?'.
	 */
	for (cp = c1; *cp != 0; ++cp) {
	    Char *c2 = cp;
	    if (!xtermIsPrintable(xw, &cp, c1 + limit)) {
		memset(c2, '?', (size_t) (cp + 1 - c2));
	    }
	}

#if OPT_WIDE_CHARS
	/*
	 * If we're running in UTF-8 mode, and have not been told that the
	 * title string is in UTF-8, it is likely that non-ASCII text in the
	 * string will be rejected because it is not printable in the current
	 * locale.  So we convert it to UTF-8, allowing the X library to
	 * convert it back.
	 */
	if (xtermEnvUTF8() && !IsSetUtf8Title(xw)) {
	    int n;

	    for (n = 0; name[n] != '\0'; ++n) {
		if (CharOf(name[n]) > 127) {
		    if (converted != 0)
			free(converted);
		    if ((converted = TypeMallocN(Char, 1 + (6 * limit))) != 0) {
			Char *temp = converted;
			while (*name != 0) {
			    temp = convertToUTF8(temp, CharOf(*name));
			    ++name;
			}
			*temp = 0;
			name = (char *) converted;
			TRACE(("...converted{%s}\n", name));
		    }
		    break;
		}
	    }
	}
#endif

#if OPT_SAME_NAME
	/* If the attribute isn't going to change, then don't bother... */

	if (resource.sameName) {
	    char *buf = 0;
	    XtSetArg(args[0], my_attr, &buf);
	    XtGetValues(top, args, 1);
	    TRACE(("...comparing{%s}\n", buf));
	    if (buf != 0 && strcmp(name, buf) == 0)
		changed = False;
	}
#endif /* OPT_SAME_NAME */

	if (changed) {
	    TRACE(("...updating %s\n", my_attr));
	    TRACE(("...value is %s\n", name));
	    XtSetArg(args[0], my_attr, name);
	    XtSetValues(top, args, 1);

#if OPT_WIDE_CHARS
	    if (xtermEnvUTF8()) {
		Display *dpy = XtDisplay(xw);
		Atom my_atom;

		const char *propname = (!strcmp(my_attr, XtNtitle)
					? "_NET_WM_NAME"
					: "_NET_WM_ICON_NAME");
		if ((my_atom = XInternAtom(dpy, propname, False)) != None) {
		    if (IsSetUtf8Title(xw)) {
			TRACE(("...updating %s\n", propname));
			TRACE(("...value is %s\n", value));
			XChangeProperty(dpy, VShellWindow(xw), my_atom,
					XA_UTF8_STRING(dpy), 8,
					PropModeReplace,
					(Char *) value,
					(int) strlen(value));
		    } else {
			TRACE(("...deleting %s\n", propname));
			XDeleteProperty(dpy, VShellWindow(xw), my_atom);
		    }
		}
	    }
#endif
	}
    }
    if (IsTitleMode(xw, tmSetBase16) && (value != emptyString)) {
	free(value);
    }
    free(my_attr);

    return;
}

void
ChangeIconName(XtermWidget xw, char *name)
{
    if (name == 0) {
	name = emptyString;
    }
    if (!showZIconBeep(xw, name))
	ChangeGroup(xw, XtNiconName, name);
}

void
ChangeTitle(XtermWidget xw, char *name)
{
    ChangeGroup(xw, XtNtitle, name);
}

#define Strlen(s) strlen((const char *)(s))

void
ChangeXprop(char *buf)
{
    Display *dpy = XtDisplay(toplevel);
    Window w = XtWindow(toplevel);
    XTextProperty text_prop;
    Atom aprop;
    Char *pchEndPropName = (Char *) strchr(buf, '=');

    if (pchEndPropName)
	*pchEndPropName = '\0';
    aprop = XInternAtom(dpy, buf, False);
    if (pchEndPropName == NULL) {
	/* no "=value" given, so delete the property */
	XDeleteProperty(dpy, w, aprop);
    } else {
	text_prop.value = pchEndPropName + 1;
	text_prop.encoding = XA_STRING;
	text_prop.format = 8;
	text_prop.nitems = Strlen(text_prop.value);
	XSetTextProperty(dpy, w, &text_prop, aprop);
    }
}

/***====================================================================***/

/*
 * This is part of ReverseVideo().  It reverses the data stored for the old
 * "dynamic" colors that might have been retrieved using OSC 10-18.
 */
void
ReverseOldColors(XtermWidget xw)
{
    ScrnColors *pOld = xw->work.oldColors;
    Pixel tmpPix;
    char *tmpName;

    if (pOld) {
	/* change text cursor, if necesary */
	if (pOld->colors[TEXT_CURSOR] == pOld->colors[TEXT_FG]) {
	    pOld->colors[TEXT_CURSOR] = pOld->colors[TEXT_BG];
	    if (pOld->names[TEXT_CURSOR]) {
		XtFree(xw->work.oldColors->names[TEXT_CURSOR]);
		pOld->names[TEXT_CURSOR] = NULL;
	    }
	    if (pOld->names[TEXT_BG]) {
		if ((tmpName = x_strdup(pOld->names[TEXT_BG])) != 0) {
		    pOld->names[TEXT_CURSOR] = tmpName;
		}
	    }
	}

	EXCHANGE(pOld->colors[TEXT_FG], pOld->colors[TEXT_BG], tmpPix);
	EXCHANGE(pOld->names[TEXT_FG], pOld->names[TEXT_BG], tmpName);

	EXCHANGE(pOld->colors[MOUSE_FG], pOld->colors[MOUSE_BG], tmpPix);
	EXCHANGE(pOld->names[MOUSE_FG], pOld->names[MOUSE_BG], tmpName);

#if OPT_TEK4014
	EXCHANGE(pOld->colors[TEK_FG], pOld->colors[TEK_BG], tmpPix);
	EXCHANGE(pOld->names[TEK_FG], pOld->names[TEK_BG], tmpName);
#endif
    }
    return;
}

Bool
AllocateTermColor(XtermWidget xw,
		  ScrnColors * pNew,
		  int ndx,
		  const char *name,
		  Bool always)
{
    Bool result = False;

    if (always || AllowColorOps(xw, ecSetColor)) {
	XColor def;
	char *newName;

	result = True;
	if (!x_strcasecmp(name, XtDefaultForeground)) {
	    def.pixel = xw->old_foreground;
	} else if (!x_strcasecmp(name, XtDefaultBackground)) {
	    def.pixel = xw->old_background;
	} else if (!xtermAllocColor(xw, &def, name)) {
	    result = False;
	}

	if (result
	    && (newName = x_strdup(name)) != 0) {
	    if (COLOR_DEFINED(pNew, ndx)) {
		free(pNew->names[ndx]);
	    }
	    SET_COLOR_VALUE(pNew, ndx, def.pixel);
	    SET_COLOR_NAME(pNew, ndx, newName);
	    TRACE(("AllocateTermColor #%d: %s (pixel 0x%06lx)\n",
		   ndx, newName, def.pixel));
	} else {
	    TRACE(("AllocateTermColor #%d: %s (failed)\n", ndx, name));
	    result = False;
	}
    }
    return result;
}
/***====================================================================***/

/* ARGSUSED */
void
Panic(const char *s GCC_UNUSED, int a GCC_UNUSED)
{
    if_DEBUG({
	xtermWarning(s, a);
    });
}

const char *
SysErrorMsg(int code)
{
    static const char unknown[] = "unknown error";
    char *s = strerror(code);
    return s ? s : unknown;
}

const char *
SysReasonMsg(int code)
{
    /* *INDENT-OFF* */
    static const struct {
	int code;
	const char *name;
    } table[] = {
	{ ERROR_FIONBIO,	"main:  ioctl() failed on FIONBIO" },
	{ ERROR_F_GETFL,	"main: ioctl() failed on F_GETFL" },
	{ ERROR_F_SETFL,	"main: ioctl() failed on F_SETFL", },
	{ ERROR_OPDEVTTY,	"spawn: open() failed on /dev/tty", },
	{ ERROR_TIOCGETP,	"spawn: ioctl() failed on TIOCGETP", },
	{ ERROR_PTSNAME,	"spawn: ptsname() failed", },
	{ ERROR_OPPTSNAME,	"spawn: open() failed on ptsname", },
	{ ERROR_PTEM,		"spawn: ioctl() failed on I_PUSH/\"ptem\"" },
	{ ERROR_CONSEM,		"spawn: ioctl() failed on I_PUSH/\"consem\"" },
	{ ERROR_LDTERM,		"spawn: ioctl() failed on I_PUSH/\"ldterm\"" },
	{ ERROR_TTCOMPAT,	"spawn: ioctl() failed on I_PUSH/\"ttcompat\"" },
	{ ERROR_TIOCSETP,	"spawn: ioctl() failed on TIOCSETP" },
	{ ERROR_TIOCSETC,	"spawn: ioctl() failed on TIOCSETC" },
	{ ERROR_TIOCSETD,	"spawn: ioctl() failed on TIOCSETD" },
	{ ERROR_TIOCSLTC,	"spawn: ioctl() failed on TIOCSLTC" },
	{ ERROR_TIOCLSET,	"spawn: ioctl() failed on TIOCLSET" },
	{ ERROR_INIGROUPS,	"spawn: initgroups() failed" },
	{ ERROR_FORK,		"spawn: fork() failed" },
	{ ERROR_EXEC,		"spawn: exec() failed" },
	{ ERROR_PTYS,		"get_pty: not enough ptys" },
	{ ERROR_PTY_EXEC,	"waiting for initial map" },
	{ ERROR_SETUID,		"spawn: setuid() failed" },
	{ ERROR_INIT,		"spawn: can't initialize window" },
	{ ERROR_TIOCKSET,	"spawn: ioctl() failed on TIOCKSET" },
	{ ERROR_TIOCKSETC,	"spawn: ioctl() failed on TIOCKSETC" },
	{ ERROR_LUMALLOC,	"luit: command-line malloc failed" },
	{ ERROR_SELECT,		"in_put: select() failed" },
	{ ERROR_VINIT,		"VTInit: can't initialize window" },
	{ ERROR_KMMALLOC1,	"HandleKeymapChange: malloc failed" },
	{ ERROR_TSELECT,	"Tinput: select() failed" },
	{ ERROR_TINIT,		"TekInit: can't initialize window" },
	{ ERROR_BMALLOC2,	"SaltTextAway: malloc() failed" },
	{ ERROR_LOGEXEC,	"StartLog: exec() failed" },
	{ ERROR_XERROR,		"xerror: XError event" },
	{ ERROR_XIOERROR,	"xioerror: X I/O error" },
	{ ERROR_SCALLOC,	"Alloc: calloc() failed on base" },
	{ ERROR_SCALLOC2,	"Alloc: calloc() failed on rows" },
	{ ERROR_SAVE_PTR,	"ScrnPointers: malloc/realloc() failed" },
    };
    /* *INDENT-ON* */

    Cardinal n;
    const char *result = "?";

    for (n = 0; n < XtNumber(table); ++n) {
	if (code == table[n].code) {
	    result = table[n].name;
	    break;
	}
    }
    return result;
}

void
SysError(int code)
{
    int oerrno = errno;

    fprintf(stderr, "%s: Error %d, errno %d: ", ProgramName, code, oerrno);
    fprintf(stderr, "%s\n", SysErrorMsg(oerrno));
    fprintf(stderr, "Reason: %s\n", SysReasonMsg(code));

    Cleanup(code);
}

void
NormalExit(void)
{
    static Bool cleaning;

    /*
     * Process "-hold" and session cleanup only for a normal exit.
     */
    if (cleaning) {
	hold_screen = 0;
	return;
    }

    cleaning = True;
    need_cleanup = False;

    if (hold_screen) {
	hold_screen = 2;
	while (hold_screen) {
	    xevents();
	    Sleep(EVENT_DELAY);
	}
    }
#if OPT_SESSION_MGT
    if (resource.sessionMgt) {
	XtVaSetValues(toplevel,
		      XtNjoinSession, False,
		      (void *) 0);
    }
#endif
    Cleanup(0);
}

/*
 * cleanup by sending SIGHUP to client processes
 */
void
Cleanup(int code)
{
    TScreen *screen = TScreenOf(term);

    TRACE(("Cleanup %d\n", code));

    if (screen->pid > 1) {
	(void) kill_process_group(screen->pid, SIGHUP);
    }
    Exit(code);
}

#ifndef S_IXOTH
#define S_IXOTH 1
#endif

Boolean
validProgram(const char *pathname)
{
    Boolean result = False;
    struct stat sb;

    if (!IsEmpty(pathname)
	&& *pathname == '/'
	&& strstr(pathname, "/..") == 0
	&& stat(pathname, &sb) == 0
	&& (sb.st_mode & S_IFMT) == S_IFREG
	&& (sb.st_mode & S_IXOTH) != 0) {
	result = True;
    }
    return result;
}

#ifndef VMS
#ifndef PATH_MAX
#define PATH_MAX 512		/* ... is not defined consistently in Xos.h */
#endif
char *
xtermFindShell(char *leaf, Bool warning)
{
    char *s0;
    char *s;
    char *d;
    char *tmp;
    char *result = leaf;
    Bool allocated = False;

    TRACE(("xtermFindShell(%s)\n", leaf));

    if (!strncmp("./", result, (size_t) 2)
	|| !strncmp("../", result, (size_t) 3)) {
	size_t need = PATH_MAX;
	size_t used = strlen(result) + 2;
	char *buffer = malloc(used + need);
	if (buffer != 0) {
	    if (getcwd(buffer, need) != 0) {
		sprintf(buffer + strlen(buffer), "/%s", result);
		result = buffer;
		allocated = True;
	    } else {
		free(buffer);
	    }
	}
    } else if (*result != '\0' && strchr("+/-", *result) == 0) {
	/* find it in $PATH */
	if ((s = s0 = x_getenv("PATH")) != 0) {
	    if ((tmp = TypeMallocN(char, strlen(leaf) + strlen(s) + 2)) != 0) {
		Bool found = False;
		while (*s != '\0') {
		    strcpy(tmp, s);
		    for (d = tmp;; ++d) {
			if (*d == ':' || *d == '\0') {
			    int skip = (*d != '\0');
			    *d = '/';
			    strcpy(d + 1, leaf);
			    if (skip)
				++d;
			    s += (d - tmp);
			    if (validProgram(tmp)) {
				result = x_strdup(tmp);
				found = True;
				allocated = True;
			    }
			    break;
			}
		    }
		    if (found)
			break;
		}
		free(tmp);
	    }
	    free(s0);
	}
    }
    TRACE(("...xtermFindShell(%s)\n", result));
    if (!validProgram(result)) {
	if (warning)
	    xtermWarning("No absolute path found for shell: %s\n", result);
	if (allocated)
	    free(result);
	result = 0;
    }
    /* be consistent, so that caller can always free the result */
    if (result != 0 && !allocated)
	result = x_strdup(result);
    return result;
}
#endif /* VMS */

#define ENV_HUNK(n)	(unsigned) ((((n) + 1) | 31) + 1)

/*
 * If we do not have unsetenv(), make consistent updates for environ[].
 * This could happen on some older machines due to the uneven standardization
 * process for the two functions.
 *
 * That is, putenv() makes a copy of environ, and some implementations do not
 * update the environ pointer, so the fallback when unsetenv() is missing would
 * not work as intended.  Likewise, the reverse could be true, i.e., unsetenv
 * could copy environ.
 */
#if defined(HAVE_PUTENV) && !defined(HAVE_UNSETENV)
#undef HAVE_PUTENV
#elif !defined(HAVE_PUTENV) && defined(HAVE_UNSETENV)
#undef HAVE_UNSETENV
#endif

/*
 * copy the environment before Setenv'ing.
 */
void
xtermCopyEnv(char **oldenv)
{
#ifdef HAVE_PUTENV
    (void) oldenv;
#else
    unsigned size;
    char **newenv;

    for (size = 0; oldenv[size] != NULL; size++) {
	;
    }

    newenv = TypeCallocN(char *, ENV_HUNK(size));
    memmove(newenv, oldenv, size * sizeof(char *));
    environ = newenv;
#endif
}

#if !defined(HAVE_PUTENV) || !defined(HAVE_UNSETENV)
static int
findEnv(const char *var, int *lengthp)
{
    char *test;
    int envindex = 0;
    size_t len = strlen(var);
    int found = -1;

    TRACE(("findEnv(%s=..)\n", var));

    while ((test = environ[envindex]) != NULL) {
	if (strncmp(test, var, len) == 0 && test[len] == '=') {
	    found = envindex;
	    break;
	}
	envindex++;
    }
    *lengthp = envindex;
    return found;
}
#endif

/*
 * sets the value of var to be arg in the Unix 4.2 BSD environment env.
 * Var should end with '=' (bindings are of the form "var=value").
 * This procedure assumes the memory for the first level of environ
 * was allocated using calloc, with enough extra room at the end so not
 * to have to do a realloc().
 */
void
xtermSetenv(const char *var, const char *value)
{
    if (value != 0) {
#ifdef HAVE_PUTENV
	char *both = malloc(2 + strlen(var) + strlen(value));
	TRACE(("xtermSetenv(%s=%s)\n", var, value));
	if (both) {
	    sprintf(both, "%s=%s", var, value);
	    putenv(both);
	}
#else
	size_t len = strlen(var);
	int envindex;
	int found = findEnv(var, &envindex);

	TRACE(("xtermSetenv(%s=%s)\n", var, value));

	if (found < 0) {
	    unsigned need = ENV_HUNK(envindex + 1);
	    unsigned have = ENV_HUNK(envindex);

	    if (need > have) {
		char **newenv;
		newenv = TypeMallocN(char *, need);
		if (newenv == 0) {
		    xtermWarning("Cannot increase environment\n");
		    return;
		}
		memmove(newenv, environ, have * sizeof(*newenv));
		free(environ);
		environ = newenv;
	    }

	    found = envindex;
	    environ[found + 1] = NULL;
	    environ = environ;
	}

	environ[found] = TextAlloc(1 + len + strlen(value));
	if (environ[found] == 0) {
	    xtermWarning("Cannot allocate environment %s\n", var);
	    return;
	}
	sprintf(environ[found], "%s=%s", var, value);
#endif
    }
}

void
xtermUnsetenv(const char *var)
{
    TRACE(("xtermUnsetenv(%s)\n", var));
#ifdef HAVE_UNSETENV
    unsetenv(var);
#else
    {
	int ignore;
	int item = findEnv(var, &ignore);
	if (item >= 0) {
	    while ((environ[item] = environ[item + 1]) != 0) {
		++item;
	    }
	}
    }
#endif
}

/*ARGSUSED*/
int
xerror(Display *d, XErrorEvent *ev)
{
    xtermWarning("warning, error event received:\n");
    (void) XmuPrintDefaultErrorMessage(d, ev, stderr);
    Exit(ERROR_XERROR);
    return 0;			/* appease the compiler */
}

void
ice_error(IceConn iceConn)
{
    (void) iceConn;

    xtermWarning("ICE IO error handler doing an exit(), pid = %ld, errno = %d\n",
		 (long) getpid(), errno);

    Exit(ERROR_ICEERROR);
}

/*ARGSUSED*/
int
xioerror(Display *dpy)
{
    int the_error = errno;

    xtermWarning("fatal IO error %d (%s) or KillClient on X server \"%s\"\r\n",
		 the_error, SysErrorMsg(the_error),
		 DisplayString(dpy));

    Exit(ERROR_XIOERROR);
    return 0;			/* appease the compiler */
}

void
xt_error(String message)
{
    xtermWarning("Xt error: %s\n", message);

    /*
     * Check for the obvious - Xt does a poor job of reporting this.
     */
    if (x_getenv("DISPLAY") == 0) {
	xtermWarning("DISPLAY is not set\n");
    }
    exit(1);
}

int
XStrCmp(char *s1, char *s2)
{
    if (s1 && s2)
	return (strcmp(s1, s2));
    if (s1 && *s1)
	return (1);
    if (s2 && *s2)
	return (-1);
    return (0);
}

#if OPT_TEK4014
static void
withdraw_window(Display *dpy, Window w, int scr)
{
    TRACE(("withdraw_window %#lx\n", (long) w));
    (void) XmuUpdateMapHints(dpy, w, NULL);
    XWithdrawWindow(dpy, w, scr);
    return;
}
#endif

void
set_vt_visibility(Bool on)
{
    XtermWidget xw = term;
    TScreen *screen = TScreenOf(xw);

    TRACE(("set_vt_visibility(%d)\n", on));
    if (on) {
	if (!screen->Vshow && xw) {
	    VTInit(xw);
	    XtMapWidget(XtParent(xw));
#if OPT_TOOLBAR
	    /* we need both of these during initialization */
	    XtMapWidget(SHELL_OF(xw));
	    ShowToolbar(resource.toolBar);
#endif
	    screen->Vshow = True;
	}
    }
#if OPT_TEK4014
    else {
	if (screen->Vshow && xw) {
	    withdraw_window(XtDisplay(xw),
			    VShellWindow(xw),
			    XScreenNumberOfScreen(XtScreen(xw)));
	    screen->Vshow = False;
	}
    }
    set_vthide_sensitivity();
    set_tekhide_sensitivity();
    update_vttekmode();
    update_tekshow();
    update_vtshow();
#endif
    return;
}

#if OPT_TEK4014
void
set_tek_visibility(Bool on)
{
    TRACE(("set_tek_visibility(%d)\n", on));

    if (on) {
	if (!TEK4014_SHOWN(term)) {
	    if (tekWidget == 0) {
		TekInit();	/* will exit on failure */
	    }
	    if (tekWidget != 0) {
		Widget tekParent = SHELL_OF(tekWidget);
		XtRealizeWidget(tekParent);
		XtMapWidget(XtParent(tekWidget));
#if OPT_TOOLBAR
		/* we need both of these during initialization */
		XtMapWidget(tekParent);
		XtMapWidget(tekWidget);
#endif
		XtOverrideTranslations(tekParent,
				       XtParseTranslationTable
				       ("<Message>WM_PROTOCOLS: DeleteWindow()"));
		(void) XSetWMProtocols(XtDisplay(tekParent),
				       XtWindow(tekParent),
				       &wm_delete_window, 1);
		TEK4014_SHOWN(term) = True;
	    }
	}
    } else {
	if (TEK4014_SHOWN(term) && tekWidget) {
	    withdraw_window(XtDisplay(tekWidget),
			    TShellWindow,
			    XScreenNumberOfScreen(XtScreen(tekWidget)));
	    TEK4014_SHOWN(term) = False;
	}
    }
    set_tekhide_sensitivity();
    set_vthide_sensitivity();
    update_vtshow();
    update_tekshow();
    update_vttekmode();
    return;
}

void
end_tek_mode(void)
{
    XtermWidget xw = term;

    if (TEK4014_ACTIVE(xw)) {
	FlushLog(xw);
	TEK4014_ACTIVE(xw) = False;
	xtermSetWinSize(xw);
	longjmp(Tekend, 1);
    }
    return;
}

void
end_vt_mode(void)
{
    XtermWidget xw = term;

    if (!TEK4014_ACTIVE(xw)) {
	FlushLog(xw);
	TEK4014_ACTIVE(xw) = True;
	TekSetWinSize(tekWidget);
	longjmp(VTend, 1);
    }
    return;
}

void
switch_modes(Bool tovt)		/* if true, then become vt mode */
{
    if (tovt) {
	if (tekRefreshList)
	    TekRefresh(tekWidget);
	end_tek_mode();		/* WARNING: this does a longjmp... */
    } else {
	end_vt_mode();		/* WARNING: this does a longjmp... */
    }
}

void
hide_vt_window(void)
{
    set_vt_visibility(False);
    if (!TEK4014_ACTIVE(term))
	switch_modes(False);	/* switch to tek mode */
}

void
hide_tek_window(void)
{
    set_tek_visibility(False);
    tekRefreshList = (TekLink *) 0;
    if (TEK4014_ACTIVE(term))
	switch_modes(True);	/* does longjmp to vt mode */
}
#endif /* OPT_TEK4014 */

static const char *
skip_punct(const char *s)
{
    while (*s == '-' || *s == '/' || *s == '+' || *s == '#' || *s == '%') {
	++s;
    }
    return s;
}

static int
cmp_options(const void *a, const void *b)
{
    const char *s1 = skip_punct(((const OptionHelp *) a)->opt);
    const char *s2 = skip_punct(((const OptionHelp *) b)->opt);
    return strcmp(s1, s2);
}

static int
cmp_resources(const void *a, const void *b)
{
    return strcmp(((const XrmOptionDescRec *) a)->option,
		  ((const XrmOptionDescRec *) b)->option);
}

XrmOptionDescRec *
sortedOptDescs(XrmOptionDescRec * descs, Cardinal res_count)
{
    static XrmOptionDescRec *res_array = 0;

#ifdef NO_LEAKS
    if (descs == 0) {
	if (res_array != 0) {
	    free(res_array);
	    res_array = 0;
	}
    } else
#endif
    if (res_array == 0) {
	Cardinal j;

	/* make a sorted index to 'resources' */
	res_array = TypeCallocN(XrmOptionDescRec, res_count);
	if (res_array != 0) {
	    for (j = 0; j < res_count; j++)
		res_array[j] = descs[j];
	    qsort(res_array, (size_t) res_count, sizeof(*res_array), cmp_resources);
	}
    }
    return res_array;
}

/*
 * The first time this is called, construct sorted index to the main program's
 * list of options, taking into account the on/off options which will be
 * compressed into one token.  It's a lot simpler to do it this way than
 * maintain the list in sorted form with lots of ifdef's.
 */
OptionHelp *
sortedOpts(OptionHelp * options, XrmOptionDescRec * descs, Cardinal numDescs)
{
    static OptionHelp *opt_array = 0;

#ifdef NO_LEAKS
    if (descs == 0 && opt_array != 0) {
	sortedOptDescs(descs, numDescs);
	free(opt_array);
	opt_array = 0;
	return 0;
    } else if (options == 0 || descs == 0) {
	return 0;
    }
#endif

    if (opt_array == 0) {
	size_t opt_count, j;
#if OPT_TRACE
	Cardinal k;
	XrmOptionDescRec *res_array = sortedOptDescs(descs, numDescs);
	int code;
	const char *mesg;
#else
	(void) descs;
	(void) numDescs;
#endif

	/* count 'options' and make a sorted index to it */
	for (opt_count = 0; options[opt_count].opt != 0; ++opt_count) {
	    ;
	}
	opt_array = TypeCallocN(OptionHelp, opt_count + 1);
	for (j = 0; j < opt_count; j++)
	    opt_array[j] = options[j];
	qsort(opt_array, opt_count, sizeof(OptionHelp), cmp_options);

	/* supply the "turn on/off" strings if needed */
#if OPT_TRACE
	for (j = 0; j < opt_count; j++) {
	    if (!strncmp(opt_array[j].opt, "-/+", (size_t) 3)) {
		char temp[80];
		const char *name = opt_array[j].opt + 3;
		for (k = 0; k < numDescs; ++k) {
		    const char *value = res_array[k].value;
		    if (res_array[k].option[0] == '-') {
			code = -1;
		    } else if (res_array[k].option[0] == '+') {
			code = 1;
		    } else {
			code = 0;
		    }
		    sprintf(temp, "%.*s",
			    (int) sizeof(temp) - 2,
			    opt_array[j].desc);
		    if (x_strindex(temp, "inhibit") != 0)
			code = -code;
		    if (code != 0
			&& res_array[k].value != 0
			&& !strcmp(name, res_array[k].option + 1)) {
			if (((code < 0) && !strcmp(value, "on"))
			    || ((code > 0) && !strcmp(value, "off"))
			    || ((code > 0) && !strcmp(value, "0"))) {
			    mesg = "turn on/off";
			} else {
			    mesg = "turn off/on";
			}
			if (strncmp(mesg, opt_array[j].desc, strlen(mesg))) {
			    if (strncmp(opt_array[j].desc, "turn ", (size_t) 5)) {
				char *s = TextAlloc(strlen(mesg)
						    + 1
						    + strlen(opt_array[j].desc));
				if (s != 0) {
				    sprintf(s, "%s %s", mesg, opt_array[j].desc);
				    opt_array[j].desc = s;
				}
			    } else {
				TRACE(("OOPS "));
			    }
			}
			TRACE(("%s: %s %s: %s (%s)\n",
			       mesg,
			       res_array[k].option,
			       res_array[k].value,
			       opt_array[j].opt,
			       opt_array[j].desc));
			break;
		    }
		}
	    }
	}
#endif
    }
    return opt_array;
}

/*
 * Report the character-type locale that xterm was started in.
 */
String
xtermEnvLocale(void)
{
    static String result;

    if (result == 0) {
	if ((result = x_nonempty(setlocale(LC_CTYPE, 0))) == 0) {
	    result = x_strdup("C");
	} else {
	    result = x_strdup(result);
	}
	TRACE(("xtermEnvLocale ->%s\n", result));
    }
    return result;
}

char *
xtermEnvEncoding(void)
{
    static char *result;

    if (result == 0) {
#ifdef HAVE_LANGINFO_CODESET
	result = nl_langinfo(CODESET);
#else
	char *locale = xtermEnvLocale();
	if (!strcmp(locale, "C") || !strcmp(locale, "POSIX")) {
	    result = "ASCII";
	} else {
	    result = "ISO-8859-1";
	}
#endif
	TRACE(("xtermEnvEncoding ->%s\n", result));
    }
    return result;
}

#if OPT_WIDE_CHARS
/*
 * Tell whether xterm was started in a locale that uses UTF-8 encoding for
 * characters.  That environment is inherited by subprocesses and used in
 * various library calls.
 */
Bool
xtermEnvUTF8(void)
{
    static Bool init = False;
    static Bool result = False;

    if (!init) {
	init = True;
#ifdef HAVE_LANGINFO_CODESET
	result = (strcmp(xtermEnvEncoding(), "UTF-8") == 0);
#else
	{
	    char *locale = x_strdup(xtermEnvLocale());
	    int n;
	    for (n = 0; locale[n] != 0; ++n) {
		locale[n] = x_toupper(locale[n]);
	    }
	    if (strstr(locale, "UTF-8") != 0)
		result = True;
	    else if (strstr(locale, "UTF8") != 0)
		result = True;
	    free(locale);
	}
#endif
	TRACE(("xtermEnvUTF8 ->%s\n", BtoS(result)));
    }
    return result;
}
#endif /* OPT_WIDE_CHARS */

/*
 * Check if the current widget, or any parent, is the VT100 "xterm" widget.
 */
XtermWidget
getXtermWidget(Widget w)
{
    XtermWidget xw;

    if (w == 0) {
	xw = (XtermWidget) CURRENT_EMU();
	if (!IsXtermWidget(xw)) {
	    xw = 0;
	}
    } else if (IsXtermWidget(w)) {
	xw = (XtermWidget) w;
    } else {
	xw = getXtermWidget(XtParent(w));
    }
    TRACE2(("getXtermWidget %p -> %p\n", w, xw));
    return xw;
}

#if OPT_SESSION_MGT
static void
die_callback(Widget w GCC_UNUSED,
	     XtPointer client_data GCC_UNUSED,
	     XtPointer call_data GCC_UNUSED)
{
    NormalExit();
}

static void
save_callback(Widget w GCC_UNUSED,
	      XtPointer client_data GCC_UNUSED,
	      XtPointer call_data)
{
    XtCheckpointToken token = (XtCheckpointToken) call_data;
    /* we have nothing to save */
    token->save_success = True;
}

static void
icewatch(IceConn iceConn,
	 IcePointer clientData GCC_UNUSED,
	 Bool opening,
	 IcePointer * watchData GCC_UNUSED)
{
    if (opening) {
	ice_fd = IceConnectionNumber(iceConn);
	TRACE(("got IceConnectionNumber %d\n", ice_fd));
    } else {
	ice_fd = -1;
	TRACE(("reset IceConnectionNumber\n"));
    }
}

void
xtermOpenSession(void)
{
    if (resource.sessionMgt) {
	TRACE(("Enabling session-management callbacks\n"));
	XtAddCallback(toplevel, XtNdieCallback, die_callback, NULL);
	XtAddCallback(toplevel, XtNsaveCallback, save_callback, NULL);
    }
}

void
xtermCloseSession(void)
{
    IceRemoveConnectionWatch(icewatch, NULL);
}
#endif /* OPT_SESSION_MGT */

Widget
xtermOpenApplication(XtAppContext * app_context_return,
		     String my_class,
		     XrmOptionDescRec * options,
		     Cardinal num_options,
		     int *argc_in_out,
		     String *argv_in_out,
		     String *fallback_resources,
		     WidgetClass widget_class,
		     ArgList args,
		     Cardinal num_args)
{
    Widget result;

    XtSetErrorHandler(xt_error);
#if OPT_SESSION_MGT
    result = XtOpenApplication(app_context_return,
			       my_class,
			       options,
			       num_options,
			       argc_in_out,
			       argv_in_out,
			       fallback_resources,
			       widget_class,
			       args,
			       num_args);
    IceAddConnectionWatch(icewatch, NULL);
#else
    (void) widget_class;
    (void) args;
    (void) num_args;
    result = XtAppInitialize(app_context_return,
			     my_class,
			     options,
			     num_options,
			     argc_in_out,
			     argv_in_out,
			     fallback_resources,
			     NULL, 0);
#endif /* OPT_SESSION_MGT */
    init_colored_cursor(XtDisplay(result));

    XtSetErrorHandler((XtErrorHandler) 0);

    return result;
}

static int x11_errors;

static int
catch_x11_error(Display *display, XErrorEvent *error_event)
{
    (void) display;
    (void) error_event;
    ++x11_errors;
    return 0;
}

Boolean
xtermGetWinAttrs(Display *dpy, Window win, XWindowAttributes * attrs)
{
    Boolean result = False;
    Status code;

    memset(attrs, 0, sizeof(*attrs));
    if (win != None) {
	XErrorHandler save = XSetErrorHandler(catch_x11_error);
	x11_errors = 0;
	code = XGetWindowAttributes(dpy, win, attrs);
	XSetErrorHandler(save);
	result = (Boolean) ((code != 0) && !x11_errors);
	if (result) {
	    TRACE_WIN_ATTRS(attrs);
	} else {
	    xtermWarning("invalid window-id %ld\n", (long) win);
	}
    }
    return result;
}

Boolean
xtermGetWinProp(Display *display,
		Window win,
		Atom property,
		long long_offset,
		long long_length,
		Atom req_type,
		Atom *actual_type_return,
		int *actual_format_return,
		unsigned long *nitems_return,
		unsigned long *bytes_after_return,
		unsigned char **prop_return)
{
    Boolean result = True;

    if (win != None) {
	XErrorHandler save = XSetErrorHandler(catch_x11_error);
	x11_errors = 0;
	if (XGetWindowProperty(display,
			       win,
			       property,
			       long_offset,
			       long_length,
			       False,
			       req_type,
			       actual_type_return,
			       actual_format_return,
			       nitems_return,
			       bytes_after_return,
			       prop_return) == Success
	    && x11_errors == 0) {
	    result = True;
	}
	XSetErrorHandler(save);
    }
    return result;
}

void
xtermEmbedWindow(Window winToEmbedInto)
{
    Display *dpy = XtDisplay(toplevel);
    XWindowAttributes attrs;

    TRACE(("checking winToEmbedInto %#lx\n", winToEmbedInto));
    if (xtermGetWinAttrs(dpy, winToEmbedInto, &attrs)) {
	XtermWidget xw = term;
	TScreen *screen = TScreenOf(xw);

	XtRealizeWidget(toplevel);

	TRACE(("...reparenting toplevel %#lx into %#lx\n",
	       XtWindow(toplevel),
	       winToEmbedInto));
	XReparentWindow(dpy,
			XtWindow(toplevel),
			winToEmbedInto, 0, 0);

	screen->embed_high = (Dimension) attrs.height;
	screen->embed_wide = (Dimension) attrs.width;
    }
}

void
free_string(String value)
{
    free((void *) value);
}

/* Set tty's idea of window size, using the given file descriptor 'fd'. */
void
update_winsize(int fd, int rows, int cols, int height, int width)
{
#ifdef TTYSIZE_STRUCT
    TTYSIZE_STRUCT ts;
    int code;

    setup_winsize(ts, rows, cols, height, width);
    TRACE_RC(code, SET_TTYSIZE(fd, ts));
    trace_winsize(ts, "from SET_TTYSIZE");
    (void) code;
#endif

    (void) rows;
    (void) cols;
    (void) height;
    (void) width;
}

/*
 * Update stty settings to match the values returned by dtterm window
 * manipulation 18 and 19.
 */
void
xtermSetWinSize(XtermWidget xw)
{
#if OPT_TEK4014
    if (!TEK4014_ACTIVE(xw))
#endif
	if (XtIsRealized((Widget) xw)) {
	    TScreen *screen = TScreenOf(xw);

	    TRACE(("xtermSetWinSize\n"));
	    update_winsize(screen->respond,
			   MaxRows(screen),
			   MaxCols(screen),
			   Height(screen),
			   Width(screen));
	}
}
