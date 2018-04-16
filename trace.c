/* $XTermId: trace.c,v 1.172 2017/11/07 00:12:24 tom Exp $ */

/*
 * Copyright 1997-2016,2017 by Thomas E. Dickey
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
 * debugging support via TRACE macro.
 */

#include <xterm.h>		/* for definition of GCC_UNUSED */
#include <version.h>

#if OPT_TRACE

#include <data.h>
#include <trace.h>

#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <assert.h>

#include <X11/Xatom.h>
#include <X11/Xmu/Atoms.h>

#ifdef HAVE_X11_TRANSLATEI_H
#include <X11/ConvertI.h>
#include <X11/TranslateI.h>
#else
#ifdef __cplusplus
extern "C" {
#endif

    extern String _XtPrintXlations(Widget w,
				   XtTranslations xlations,
				   Widget accelWidget,
				   _XtBoolean includeRHS);
#ifdef __cplusplus
}
#endif
#endif
const char *trace_who = "parent";

static FILE *trace_fp;

static FILE *
TraceOpen(void)
{
    static const char *trace_out;

    if (trace_fp != 0
	&& trace_who != trace_out) {
	fclose(trace_fp);
	trace_fp = 0;
    }
    trace_out = trace_who;

    if (!trace_fp) {
	mode_t oldmask = umask(077);
	char name[BUFSIZ];
#if 0				/* usually I do not want unique names */
	int unique;
	for (unique = 0;; ++unique) {
	    if (unique)
		sprintf(name, "Trace-%s.out-%d", trace_who, unique);
	    else
		sprintf(name, "Trace-%s.out", trace_who);
	    if ((trace_fp = fopen(name, "r")) == 0) {
		break;
	    }
	    fclose(trace_fp);
	}
#else
	sprintf(name, "Trace-%s.out", trace_who);
#endif
	trace_fp = fopen(name, "w");
	/*
	 * Try to put the trace-file in user's home-directory if the current
	 * directory is not writable.
	 */
	if (trace_fp == 0) {
	    char *home = getenv("HOME");
	    if (home != 0) {
		sprintf(name, "%.*s/Trace-%.8s.out",
			(BUFSIZ - 21), home,
			trace_who);
		trace_fp = fopen(name, "w");
	    }
	}
	if (trace_fp != 0) {
	    fprintf(trace_fp, "%s\n", xtermVersion());
	    TraceIds(NULL, 0);
	}
	if (!trace_fp) {
	    xtermWarning("Cannot open \"%s\"\n", name);
	    exit(EXIT_FAILURE);
	}
	(void) umask(oldmask);
    }
    return trace_fp;
}

void
Trace(const char *fmt,...)
{
    va_list ap;
    FILE *fp = TraceOpen();

    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    (void) fflush(fp);
    va_end(ap);
}

void
TraceVA(const char *fmt, va_list ap)
{
    FILE *fp = TraceOpen();

    vfprintf(fp, fmt, ap);
    (void) fflush(fp);
}

void
TraceClose(void)
{
    if (trace_fp != 0) {
	(void) fclose(trace_fp);
	(void) fflush(stdout);
	(void) fflush(stderr);
	(void) visibleChars(NULL, 0);
	(void) visibleIChars(NULL, 0);
	trace_fp = 0;
    }
}

void
TraceIds(const char *fname, int lnum)
{
    Trace("process %d ", (int) getpid());
#ifdef HAVE_UNISTD_H
    Trace("real (%u/%u) effective (%u/%u)",
	  (unsigned) getuid(), (unsigned) getgid(),
	  (unsigned) geteuid(), (unsigned) getegid());
#endif
    if (fname != 0) {
	Trace(" (%s@%d)\n", fname, lnum);
    } else {
	time_t now = time((time_t *) 0);
	Trace("-- %s", ctime(&now));
    }
}

void
TraceTime(const char *fname, int lnum)
{
    time_t now;
    if (fname != 0) {
	Trace("datetime (%s@%d) ", fname, lnum);
    }
    now = time((time_t *) 0);
    Trace("-- %s", ctime(&now));
}

static void
formatAscii(char *dst, unsigned value)
{
    switch (value) {
    case '\\':
	sprintf(dst, "\\\\");
	break;
    case '\b':
	sprintf(dst, "\\b");
	break;
    case '\n':
	sprintf(dst, "\\n");
	break;
    case '\r':
	sprintf(dst, "\\r");
	break;
    case '\t':
	sprintf(dst, "\\t");
	break;
    default:
	if (E2A(value) < 32 || (E2A(value) >= 127 && E2A(value) < 160))
	    sprintf(dst, "\\%03o", value & 0xff);
	else
	    sprintf(dst, "%c", CharOf(value));
	break;
    }
}

#if OPT_DEC_CHRSET

const char *
visibleDblChrset(unsigned chrset)
{
    const char *result = "?";
    switch (chrset) {
    case CSET_SWL:
	result = "CSET_SWL";
	break;
    case CSET_DHL_TOP:
	result = "CSET_DHL_TOP";
	break;
    case CSET_DHL_BOT:
	result = "CSET_DHL_BOT";
	break;
    case CSET_DWL:
	result = "CSET_DWL";
	break;
    }
    return result;
}
#endif

const char *
visibleScsCode(int chrset)
{
#define MAP(to,from) case from: result = to; break
    const char *result = "<ERR>";
    switch ((DECNRCM_codes) chrset) {
	MAP("B", nrc_ASCII);
	MAP("A", nrc_British);
	MAP("A", nrc_British_Latin_1);
	MAP("&4", nrc_Cyrillic);
	MAP("0", nrc_DEC_Spec_Graphic);
	MAP("1", nrc_DEC_Alt_Chars);
	MAP("2", nrc_DEC_Alt_Graphics);
	MAP("<", nrc_DEC_Supp);
	MAP("%5", nrc_DEC_Supp_Graphic);
	MAP(">", nrc_DEC_Technical);
	MAP("4", nrc_Dutch);
	MAP("5", nrc_Finnish);
	MAP("C", nrc_Finnish2);
	MAP("R", nrc_French);
	MAP("f", nrc_French2);
	MAP("Q", nrc_French_Canadian);
	MAP("9", nrc_French_Canadian2);
	MAP("K", nrc_German);
	MAP("\"?", nrc_Greek);
	MAP("F", nrc_Greek_Supp);
	MAP("\"4", nrc_Hebrew);
	MAP("%=", nrc_Hebrew2);
	MAP("H", nrc_Hebrew_Supp);
	MAP("Y", nrc_Italian);
	MAP("M", nrc_Latin_5_Supp);
	MAP("L", nrc_Latin_Cyrillic);
	MAP("`", nrc_Norwegian_Danish);
	MAP("E", nrc_Norwegian_Danish2);
	MAP("6", nrc_Norwegian_Danish3);
	MAP("%6", nrc_Portugese);
	MAP("&5", nrc_Russian);
	MAP("%3", nrc_SCS_NRCS);
	MAP("Z", nrc_Spanish);
	MAP("7", nrc_Swedish);
	MAP("H", nrc_Swedish2);
	MAP("=", nrc_Swiss);
	MAP("%0", nrc_Turkish);
	MAP("%2", nrc_Turkish2);
	MAP("<UNK>", nrc_Unknown);
    }
#undef MAP
    return result;
}

const char *
visibleChars(const Char *buf, unsigned len)
{
    static char *result;
    static unsigned used;

    if (buf != 0) {
	unsigned limit = ((len + 1) * 8) + 1;

	if (limit > used) {
	    used = limit;
	    result = XtRealloc(result, used);
	}
	if (result != 0) {
	    char *dst = result;
	    *dst = '\0';
	    while (len--) {
		unsigned value = *buf++;
		formatAscii(dst, value);
		dst += strlen(dst);
	    }
	}
    } else if (result != 0) {
	free(result);
	result = 0;
	used = 0;
    }
    return NonNull(result);
}

const char *
visibleIChars(const IChar *buf, unsigned len)
{
    static char *result;
    static unsigned used;

    if (buf != 0) {
	unsigned limit = ((len + 1) * 8) + 1;

	if (limit > used) {
	    used = limit;
	    result = XtRealloc(result, used);
	}
	if (result != 0) {
	    char *dst = result;
	    *dst = '\0';
	    while (len--) {
		unsigned value = *buf++;
#if OPT_WIDE_CHARS
		if (value > 255)
		    sprintf(dst, "\\u+%04X", value);
		else
#endif
		    formatAscii(dst, value);
		dst += strlen(dst);
	    }
	}
    } else if (result != 0) {
	free(result);
	result = 0;
	used = 0;
    }
    return NonNull(result);
}

const char *
visibleUChar(unsigned chr)
{
    IChar buf[1];
    buf[0] = chr;
    return visibleIChars(buf, 1);
}

const char *
visibleEventType(int type)
{
    const char *result = "?";
    switch (type) {
	CASETYPE(KeyPress);
	CASETYPE(KeyRelease);
	CASETYPE(ButtonPress);
	CASETYPE(ButtonRelease);
	CASETYPE(MotionNotify);
	CASETYPE(EnterNotify);
	CASETYPE(LeaveNotify);
	CASETYPE(FocusIn);
	CASETYPE(FocusOut);
	CASETYPE(KeymapNotify);
	CASETYPE(Expose);
	CASETYPE(GraphicsExpose);
	CASETYPE(NoExpose);
	CASETYPE(VisibilityNotify);
	CASETYPE(CreateNotify);
	CASETYPE(DestroyNotify);
	CASETYPE(UnmapNotify);
	CASETYPE(MapNotify);
	CASETYPE(MapRequest);
	CASETYPE(ReparentNotify);
	CASETYPE(ConfigureNotify);
	CASETYPE(ConfigureRequest);
	CASETYPE(GravityNotify);
	CASETYPE(ResizeRequest);
	CASETYPE(CirculateNotify);
	CASETYPE(CirculateRequest);
	CASETYPE(PropertyNotify);
	CASETYPE(SelectionClear);
	CASETYPE(SelectionRequest);
	CASETYPE(SelectionNotify);
	CASETYPE(ColormapNotify);
	CASETYPE(ClientMessage);
	CASETYPE(MappingNotify);
    }
    return result;
}

const char *
visibleNotifyMode(int code)
{
    const char *result = "?";
    switch (code) {
	CASETYPE(NotifyNormal);
	CASETYPE(NotifyGrab);
	CASETYPE(NotifyUngrab);
	CASETYPE(NotifyWhileGrabbed);
    }
    return result;
}

const char *
visibleNotifyDetail(int code)
{
    const char *result = "?";
    switch (code) {
	CASETYPE(NotifyAncestor);
	CASETYPE(NotifyVirtual);
	CASETYPE(NotifyInferior);
	CASETYPE(NotifyNonlinear);
	CASETYPE(NotifyNonlinearVirtual);
	CASETYPE(NotifyPointer);
	CASETYPE(NotifyPointerRoot);
	CASETYPE(NotifyDetailNone);
    }
    return result;
}

const char *
visibleSelectionTarget(Display *d, Atom a)
{
    const char *result = "?";

    if (a == XA_STRING) {
	result = "XA_STRING";
    } else if (a == XA_TEXT(d)) {
	result = "XA_TEXT()";
    } else if (a == XA_COMPOUND_TEXT(d)) {
	result = "XA_COMPOUND_TEXT()";
    } else if (a == XA_UTF8_STRING(d)) {
	result = "XA_UTF8_STRING()";
    } else if (a == XA_TARGETS(d)) {
	result = "XA_TARGETS()";
    }

    return result;
}

const char *
visibleTekparse(int code)
{
    static const struct {
	int code;
	const char *name;
    } table[] = {
#include "Tekparse.cin"
    };
    const char *result = "?";
    Cardinal n;
    for (n = 0; n < XtNumber(table); ++n) {
	if (table[n].code == code) {
	    result = table[n].name;
	    break;
	}
    }
    return result;
}

const char *
visibleVTparse(int code)
{
    static const struct {
	int code;
	const char *name;
    } table[] = {
#include "VTparse.cin"
    };
    const char *result = "?";
    Cardinal n;
    for (n = 0; n < XtNumber(table); ++n) {
	if (table[n].code == code) {
	    result = table[n].name;
	    break;
	}
    }
    return result;
}

const char *
visibleXError(int code)
{
    static char temp[80];
    const char *result = "?";
    switch (code) {
	CASETYPE(Success);
	CASETYPE(BadRequest);
	CASETYPE(BadValue);
	CASETYPE(BadWindow);
	CASETYPE(BadPixmap);
	CASETYPE(BadAtom);
	CASETYPE(BadCursor);
	CASETYPE(BadFont);
	CASETYPE(BadMatch);
	CASETYPE(BadDrawable);
	CASETYPE(BadAccess);
	CASETYPE(BadAlloc);
	CASETYPE(BadColor);
	CASETYPE(BadGC);
	CASETYPE(BadIDChoice);
	CASETYPE(BadName);
	CASETYPE(BadLength);
	CASETYPE(BadImplementation);
    default:
	sprintf(temp, "%d", code);
	result = temp;
	break;
    }
    return result;
}

#if OPT_TRACE_FLAGS
#define isScrnFlag(flag) ((flag) == LINEWRAPPED)

static char *
ScrnText(LineData *ld)
{
    return visibleIChars(ld->charData, ld->lineSize);
}

#define SHOW_BAD_LINE(name, ld) \
	Trace("OOPS " #name " bad row\n")

#define SHOW_SCRN_FLAG(name,code) \
	Trace(#name " %s:%s\n", \
	      code ? "*" : "", \
	      ScrnText(ld))

void
LineClrFlag(LineData *ld, int flag)
{
    if (ld == 0) {
	SHOW_BAD_LINE(LineClrFlag, ld);
	assert(0);
    } else if (isScrnFlag(flag)) {
	SHOW_SCRN_FLAG(LineClrFlag, 0);
    }

    LineFlags(ld) &= ~flag;
}

void
LineSetFlag(LineData *ld, int flag)
{
    if (ld == 0) {
	SHOW_BAD_LINE(LineSetFlag, ld);
	assert(0);
    } else if (isScrnFlag(flag)) {
	SHOW_SCRN_FLAG(LineSetFlag, 1);
    }

    LineFlags(ld) |= flag;
}

int
LineTstFlag(LineData ld, int flag)
{
    int code = 0;
    if (ld == 0) {
	SHOW_BAD_LINE(LineTstFlag, ld);
    } else {
	code = LineFlags(ld);

	if (isScrnFlag(flag)) {
	    SHOW_SCRN_FLAG(LineTstFlag, code);
	}
    }
    return code;
}
#endif /* OPT_TRACE_FLAGS */

const char *
TraceAtomName(Display *dpy, Atom atom)
{
    static char *result;
    free(result);
    result = XGetAtomName(dpy, atom);
    return result;
}

/*
 * Trace the normal or alternate screen, showing color values up to 16, e.g.,
 * for debugging with vttest.
 */
void
TraceScreen(XtermWidget xw, int whichBuf)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->editBuf_index[whichBuf]) {
	int row;

	TRACE(("TraceScreen %d:\n", whichBuf));
	for (row = 0; row <= screen->max_row; ++row) {
	    LineData *ld = getLineData(screen, row);

	    TRACE((" %3d:", row));
	    if (ld != 0) {
		int col;

		for (col = 0; col < ld->lineSize; ++col) {
		    int ch = (int) ld->charData[col];
		    if (ch < ' ')
			ch = ' ';
		    if (ch >= 127)
			ch = '#';
		    TRACE(("%c", ch));
		}
		TRACE((":\n"));

		TRACE(("  xx:"));
		for (col = 0; col < ld->lineSize; ++col) {
		    unsigned attrs = ld->attribs[col];
		    char ch;
		    if (attrs & PROTECTED) {
			ch = '*';
		    } else if (attrs & BLINK) {
			ch = 'B';
		    } else if (attrs & CHARDRAWN) {
			ch = '+';
		    } else {
			ch = ' ';
		    }
		    TRACE(("%c", ch));
		}
		TRACE((":\n"));

#if 0
		TRACE(("  fg:"));
		for (col = 0; col < ld->lineSize; ++col) {
		    unsigned fg = extract_fg(xw, ld->color[col], ld->attribs[col]);
		    if (fg > 15)
			fg = 15;
		    TRACE(("%1x", fg));
		}
		TRACE((":\n"));

		TRACE(("  bg:"));
		for (col = 0; col < ld->lineSize; ++col) {
		    unsigned bg = extract_bg(xw, ld->color[col], ld->attribs[col]);
		    if (bg > 15)
			bg = 15;
		    TRACE(("%1x", bg));
		}
		TRACE((":\n"));
#endif
	    } else {
		TRACE(("null lineData\n"));
	    }
	}
    } else {
	TRACE(("TraceScreen %d is nil\n", whichBuf));
    }
}

void
TraceFocus(Widget w, XEvent *ev)
{
    TRACE(("trace_focus event type %d:%s\n",
	   ev->type, visibleEventType(ev->type)));
    switch (ev->type) {
    case FocusIn:
    case FocusOut:
	{
	    XFocusChangeEvent *event = (XFocusChangeEvent *) ev;
	    TRACE(("\tdetail: %s\n", visibleNotifyDetail(event->detail)));
	    TRACE(("\tmode:   %s\n", visibleNotifyMode(event->mode)));
	    TRACE(("\twindow: %#lx\n", event->window));
	}
	break;
    case EnterNotify:
    case LeaveNotify:
	{
	    XCrossingEvent *event = (XCrossingEvent *) ev;
	    TRACE(("\tdetail:    %s\n", visibleNotifyDetail(event->detail)));
	    TRACE(("\tmode:      %s\n", visibleNotifyMode(event->mode)));
	    TRACE(("\twindow:    %#lx\n", event->window));
	    TRACE(("\tfocus:     %d\n", event->focus));
	    TRACE(("\troot:      %#lx\n", event->root));
	    TRACE(("\tsubwindow: %#lx\n", event->subwindow));
	}
	break;
    }
    while (w != 0) {
	TRACE(("w %p -> %#lx\n", (void *) w, XtWindow(w)));
	w = XtParent(w);
    }
}

void
TraceSizeHints(XSizeHints * hints)
{
    TRACE(("size hints:\n"));
    if (hints->flags & (USPosition | PPosition))
	TRACE(("   position   %d,%d%s%s\n", hints->y, hints->x,
	       (hints->flags & USPosition) ? " user" : "",
	       (hints->flags & PPosition) ? " prog" : ""));
    if (hints->flags & (USSize | PSize))
	TRACE(("   size       %d,%d%s%s\n", hints->height, hints->width,
	       (hints->flags & USSize) ? " user" : "",
	       (hints->flags & PSize) ? " prog" : ""));
    if (hints->flags & PMinSize)
	TRACE(("   min        %d,%d\n", hints->min_height, hints->min_width));
    if (hints->flags & PMaxSize)
	TRACE(("   max        %d,%d\n", hints->max_height, hints->max_width));
    if (hints->flags & PResizeInc)
	TRACE(("   inc        %d,%d\n", hints->height_inc, hints->width_inc));
    else
	TRACE(("   inc        NONE!\n"));
    if (hints->flags & PAspect)
	TRACE(("   min aspect %d/%d\n", hints->min_aspect.y, hints->min_aspect.y));
    if (hints->flags & PAspect)
	TRACE(("   max aspect %d/%d\n", hints->max_aspect.y, hints->max_aspect.y));
    if (hints->flags & PBaseSize)
	TRACE(("   base       %d,%d\n", hints->base_height, hints->base_width));
    if (hints->flags & PWinGravity)
	TRACE(("   gravity    %d\n", hints->win_gravity));
}

static void
TraceEventMask(const char *tag, long mask)
{
#define DATA(name) { name##Mask, #name }
    /* *INDENT-OFF* */
    static struct {
	long mask;
	const char *name;
    } table[] = {
	DATA(KeyPress),
	DATA(KeyRelease),
	DATA(ButtonPress),
	DATA(ButtonRelease),
	DATA(EnterWindow),
	DATA(LeaveWindow),
	DATA(PointerMotion),
	DATA(PointerMotionHint),
	DATA(Button1Motion),
	DATA(Button2Motion),
	DATA(Button3Motion),
	DATA(Button4Motion),
	DATA(Button5Motion),
	DATA(ButtonMotion),
	DATA(KeymapState),
	DATA(Exposure),
	DATA(VisibilityChange),
	DATA(StructureNotify),
	DATA(ResizeRedirect),
	DATA(SubstructureNotify),
	DATA(SubstructureRedirect),
	DATA(FocusChange),
	DATA(PropertyChange),
	DATA(ColormapChange),
	DATA(OwnerGrabButton),
    };
#undef DATA
    Cardinal n;
    /* *INDENT-ON* */

    for (n = 0; n < XtNumber(table); ++n) {
	if (table[n].mask & mask) {
	    TRACE(("%s %s\n", tag, table[n].name));
	}
    }
}

void
TraceWindowAttributes(XWindowAttributes * attrs)
{
    TRACE(("window attributes:\n"));
    TRACE(("   position     %d,%d\n", attrs->y, attrs->x));
    TRACE(("   size         %dx%d\n", attrs->height, attrs->width));
    TRACE(("   border       %d\n", attrs->border_width));
    TRACE(("   depth        %d\n", attrs->depth));
    TRACE(("   bit_gravity  %d\n", attrs->bit_gravity));
    TRACE(("   win_gravity  %d\n", attrs->win_gravity));
    TRACE(("   root         %#lx\n", (long) attrs->root));
    TRACE(("   class        %s\n", ((attrs->class == InputOutput)
				    ? "InputOutput"
				    : ((attrs->class == InputOnly)
				       ? "InputOnly"
				       : "unknown"))));
    TRACE(("   map_state    %s\n", ((attrs->map_state == IsUnmapped)
				    ? "IsUnmapped"
				    : ((attrs->map_state == IsUnviewable)
				       ? "IsUnviewable"
				       : ((attrs->map_state == IsViewable)
					  ? "IsViewable"
					  : "unknown")))));
    TRACE(("   all_events\n"));
    TraceEventMask("        ", attrs->all_event_masks);
    TRACE(("   your_events\n"));
    TraceEventMask("        ", attrs->your_event_mask);
    TRACE(("   no_propagate\n"));
    TraceEventMask("        ", attrs->do_not_propagate_mask);
}

void
TraceWMSizeHints(XtermWidget xw)
{
    XSizeHints sizehints = xw->hints;

    getXtermSizeHints(xw);
    TraceSizeHints(&xw->hints);
    xw->hints = sizehints;
}

/*
 * Some calls to XGetAtom() will fail, and we don't want to stop.  So we use
 * our own error-handler.
 */
/* ARGSUSED */
static int
no_error(Display *dpy GCC_UNUSED, XErrorEvent *event GCC_UNUSED)
{
    return 1;
}

const char *
ModifierName(unsigned modifier)
{
    const char *s = "";
    if (modifier & ShiftMask)
	s = " Shift";
    else if (modifier & LockMask)
	s = " Lock";
    else if (modifier & ControlMask)
	s = " Control";
    else if (modifier & Mod1Mask)
	s = " Mod1";
    else if (modifier & Mod2Mask)
	s = " Mod2";
    else if (modifier & Mod3Mask)
	s = " Mod3";
    else if (modifier & Mod4Mask)
	s = " Mod4";
    else if (modifier & Mod5Mask)
	s = " Mod5";
    return s;
}

void
TraceTranslations(const char *name, Widget w)
{
    String result;
    XErrorHandler save = XSetErrorHandler(no_error);
    XtTranslations xlations;
    Widget xcelerat;

    TRACE(("TraceTranslations for %s (widget %#lx) {{\n", name, (long) w));
    if (w) {
	XtVaGetValues(w,
		      XtNtranslations, &xlations,
		      XtNaccelerators, &xcelerat,
		      (XtPointer) 0);
	TRACE(("... xlations %#08lx\n", (long) xlations));
	TRACE(("... xcelerat %#08lx\n", (long) xcelerat));
	result = _XtPrintXlations(w, xlations, xcelerat, True);
	TRACE(("%s\n", NonNull(result)));
	if (result)
	    XFree((char *) result);
    } else {
	TRACE(("none (widget is null)\n"));
    }
    TRACE(("}}\n"));
    XSetErrorHandler(save);
}

XtGeometryResult
TraceResizeRequest(const char *fn, int ln, Widget w,
		   unsigned reqwide,
		   unsigned reqhigh,
		   Dimension *gotwide,
		   Dimension *gothigh)
{
    XtGeometryResult rc;

    TRACE(("%s@%d ResizeRequest %ux%u\n", fn, ln, reqhigh, reqwide));
    rc = XtMakeResizeRequest((Widget) w,
			     (Dimension) reqwide,
			     (Dimension) reqhigh,
			     gotwide, gothigh);
    TRACE(("... ResizeRequest -> "));
    if (gothigh && gotwide)
	TRACE(("%dx%d ", *gothigh, *gotwide));
    TRACE(("(%d)\n", rc));
    return rc;
}

#define XRES_S(name) Trace(#name " = %s\n", NonNull(resp->name))
#define XRES_B(name) Trace(#name " = %s\n", MtoS(resp->name))
#define XRES_I(name) Trace(#name " = %d\n", resp->name)

void
TraceXtermResources(void)
{
    XTERM_RESOURCE *resp = &resource;

    Trace("XTERM_RESOURCE settings:\n");
    XRES_S(icon_geometry);
    XRES_S(title);
    XRES_S(icon_hint);
    XRES_S(icon_name);
    XRES_S(term_name);
    XRES_S(tty_modes);
    XRES_I(minBufSize);
    XRES_I(maxBufSize);
    XRES_B(hold_screen);
    XRES_B(utmpInhibit);
    XRES_B(utmpDisplayId);
    XRES_B(messages);
    XRES_S(menuLocale);
    XRES_S(omitTranslation);
    XRES_S(keyboardType);
#if OPT_PRINT_ON_EXIT
    XRES_I(printModeNow);
    XRES_I(printModeOnXError);
    XRES_I(printOptsNow);
    XRES_I(printOptsOnXError);
    XRES_S(printFileNow);
    XRES_S(printFileOnXError);
#endif
#if OPT_SUNPC_KBD
    XRES_B(sunKeyboard);
#endif
#if OPT_HP_FUNC_KEYS
    XRES_B(hpFunctionKeys);
#endif
#if OPT_SCO_FUNC_KEYS
    XRES_B(scoFunctionKeys);
#endif
#if OPT_SUN_FUNC_KEYS
    XRES_B(sunFunctionKeys);
#endif
#if OPT_INITIAL_ERASE
    XRES_B(ptyInitialErase);
    XRES_B(backarrow_is_erase);
#endif
    XRES_B(useInsertMode);
#if OPT_ZICONBEEP
    XRES_I(zIconBeep);
    XRES_S(zIconFormat);
#endif
#if OPT_PTY_HANDSHAKE
    XRES_B(wait_for_map);
    XRES_B(ptyHandshake);
    XRES_B(ptySttySize);
#endif
#if OPT_REPORT_CCLASS
    XRES_B(reportCClass);
#endif
#if OPT_REPORT_COLORS
    XRES_B(reportColors);
#endif
#if OPT_REPORT_FONTS
    XRES_B(reportFonts);
#endif
#if OPT_SAME_NAME
    XRES_B(sameName);
#endif
#if OPT_SESSION_MGT
    XRES_B(sessionMgt);
#endif
#if OPT_TOOLBAR
    XRES_B(toolBar);
#endif
#if OPT_MAXIMIZE
    XRES_B(maximized);
    XRES_S(fullscreen_s);
#endif
}

void
TraceArgv(const char *tag, char **argv)
{
    TRACE(("%s:\n", tag));
    if (argv != 0) {
	int n = 0;

	while (*argv != 0) {
	    TRACE(("  %d:%s\n", n++, *argv++));
	}
    }
}

static char *
parse_option(char *dst, String src, int first)
{
    char *s;

    if (!strncmp(src, "-/+", (size_t) 3)) {
	dst[0] = (char) first;
	strcpy(dst + 1, src + 3);
    } else {
	strcpy(dst, src);
    }
    for (s = dst; *s != '\0'; s++) {
	if (*s == '#' || *s == '%' || *s == 'S') {
	    s[1] = '\0';
	} else if (*s == ' ') {
	    *s = '\0';
	    break;
	}
    }
    return dst;
}

static Bool
same_option(OptionHelp * opt, XrmOptionDescRec * res)
{
    char temp[BUFSIZ];
    return !strcmp(parse_option(temp, opt->opt, res->option[0]), res->option);
}

static Bool
standard_option(String opt)
{
    static const char *table[] =
    {
	"+rv",
	"+synchronous",
	"-background",
	"-bd",
	"-bg",
	"-bordercolor",
	"-borderwidth",
	"-bw",
	"-display",
	"-fg",
	"-fn",
	"-font",
	"-foreground",
	"-geometry",
	"-iconic",
	"-name",
	"-reverse",
	"-rv",
	"-selectionTimeout",
	"-synchronous",
	"-title",
	"-xnllanguage",
	"-xrm",
	"-xtsessionID",
    };
    Cardinal n;
    char temp[BUFSIZ];

    opt = parse_option(temp, opt, '-');
    for (n = 0; n < XtNumber(table); n++) {
	if (!strcmp(opt, table[n]))
	    return True;
    }
    return False;
}

/*
 * Analyse the options/help messages for inconsistencies.
 */
void
TraceOptions(OptionHelp * options, XrmOptionDescRec * resources, Cardinal res_count)
{
    OptionHelp *opt_array = sortedOpts(options, resources, res_count);
    size_t j, k;
    XrmOptionDescRec *res_array = sortedOptDescs(resources, res_count);
    Bool first, found;

    TRACE(("Checking options-tables for inconsistencies:\n"));

#if 0
    TRACE(("Options listed in help-message:\n"));
    for (j = 0; options[j].opt != 0; j++)
	TRACE(("%5d %-28s %s\n", j, opt_array[j].opt, opt_array[j].desc));
    TRACE(("Options listed in resource-table:\n"));
    for (j = 0; j < res_count; j++)
	TRACE(("%5d %-28s %s\n", j, res_array[j].option, res_array[j].specifier));
#endif

    /* list all options[] not found in resources[] */
    for (j = 0, first = True; options[j].opt != 0; j++) {
	found = False;
	for (k = 0; k < res_count; k++) {
	    if (same_option(&opt_array[j], &res_array[k])) {
		found = True;
		break;
	    }
	}
	if (!found) {
	    if (first) {
		TRACE(("Options listed in help, not found in resource list:\n"));
		first = False;
	    }
	    TRACE(("  %-28s%s\n", opt_array[j].opt,
		   standard_option(opt_array[j].opt) ? " (standard)" : ""));
	}
    }

    /* list all resources[] not found in options[] */
    for (j = 0, first = True; j < res_count; j++) {
	found = False;
	for (k = 0; options[k].opt != 0; k++) {
	    if (same_option(&opt_array[k], &res_array[j])) {
		found = True;
		break;
	    }
	}
	if (!found) {
	    if (first) {
		TRACE(("Resource list items not found in options-help:\n"));
		first = False;
	    }
	    TRACE(("  %s\n", res_array[j].option));
	}
    }

    TRACE(("Resource list items that will be ignored by XtOpenApplication:\n"));
    for (j = 0; j < res_count; j++) {
	switch (res_array[j].argKind) {
	case XrmoptionSkipArg:
	    TRACE(("  %-28s {param}\n", res_array[j].option));
	    break;
	case XrmoptionSkipNArgs:
	    TRACE(("  %-28s {%ld params}\n", res_array[j].option, (long)
		   res_array[j].value));
	    break;
	case XrmoptionSkipLine:
	    TRACE(("  %-28s {remainder of line}\n", res_array[j].option));
	    break;
	case XrmoptionIsArg:
	case XrmoptionNoArg:
	case XrmoptionResArg:
	case XrmoptionSepArg:
	case XrmoptionStickyArg:
	default:
	    break;
	}
    }
}
#else
extern void empty_trace(void);
void
empty_trace(void)
{
}
#endif
