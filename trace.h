/* $XTermId: trace.h,v 1.81 2017/11/07 00:12:24 tom Exp $ */

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
 * Common/useful definitions for XTERM application
 */
#ifndef	included_trace_h
#define	included_trace_h
/* *INDENT-OFF* */

#include <xterm.h>

#if OPT_TRACE

#include <stdarg.h>

extern	void	Trace ( const char *, ... ) GCC_PRINTFLIKE(1,2);
extern	void	TraceVA ( const char *fmt, va_list ap );

#undef  TRACE
#define TRACE(p) Trace p

#undef  TRACE_VA
#define TRACE_VA(p) TraceVA p

extern	void	TraceClose (void);

#undef  TRACE_CLOSE
#define TRACE_CLOSE TraceClose

#if OPT_TRACE > 1
#define TRACE2(p) Trace p
#endif

extern	const char * visibleChars(const Char * /* buf */, unsigned /* len */);
extern	const char * visibleIChars(const IChar * /* buf */, unsigned /* len */);
extern	const char * visibleUChar(unsigned);
extern	const char * visibleDblChrset(unsigned /* chrset */);
extern	const char * visibleEventType (int);
extern	const char * visibleNotifyDetail(int /* code */);
extern	const char * visibleNotifyMode (int /* code */);
extern	const char * visibleScsCode(int /* chrset */);
extern	const char * visibleSelectionTarget(Display * /* d */, Atom /* a */);
extern	const char * visibleTekparse (int);
extern	const char * visibleVTparse (int);
extern	const char * visibleXError (int /* code */);

extern	const char * TraceAtomName(Display * /* d */, Atom /* a */);

extern	void	TraceArgv(const char * /* tag */, char ** /* argv */);
#undef  TRACE_ARGV
#define	TRACE_ARGV(tag,argv) TraceArgv(tag,argv)

extern	const	char *trace_who;
#undef  TRACE_CHILD
#define TRACE_CHILD int tracing_child = (trace_who = "child") != 0; (void) tracing_child;

extern	void	TraceFocus(Widget, XEvent *);
#undef  TRACE_FOCUS
#define	TRACE_FOCUS(w,e) TraceFocus((Widget)w, (XEvent *)e)

extern	void	TraceSizeHints(XSizeHints *);
#undef  TRACE_HINTS
#define	TRACE_HINTS(hints) TraceSizeHints(hints)

extern	void	TraceIds(const char * /* fname */, int  /* lnum */);
#undef  TRACE_IDS
#define	TRACE_IDS TraceIds(__FILE__, __LINE__)

extern	void	TraceTime(const char * /* fname */, int  /* lnum */);
#undef  TRACE_TIME
#define	TRACE_TIME TraceTime(__FILE__, __LINE__)

extern	void	TraceOptions(OptionHelp * /* options */, XrmOptionDescRec * /* resources */, Cardinal  /* count */);
#undef  TRACE_OPTS
#define	TRACE_OPTS(opts,ress,lens) TraceOptions(opts,ress,lens)

extern	void	TraceTranslations(const char *, Widget);
#undef  TRACE_TRANS
#define	TRACE_TRANS(name,w) TraceTranslations(name,w)

extern	void	TraceWindowAttributes(XWindowAttributes *);
#undef  TRACE_WIN_ATTRS
#define	TRACE_WIN_ATTRS(a) TraceWindowAttributes(a)

extern	void	TraceWMSizeHints(XtermWidget);
#undef  TRACE_WM_HINTS
#define	TRACE_WM_HINTS(w) TraceWMSizeHints(w)

extern	void	TraceXtermResources(void);
#undef  TRACE_XRES
#define	TRACE_XRES() TraceXtermResources()

extern	XtGeometryResult TraceResizeRequest(const char * /* fn */, int  /* ln */, Widget  /* w */, unsigned  /* reqwide */, unsigned  /* reqhigh */, Dimension * /* gotwide */, Dimension * /* gothigh */);
#undef  REQ_RESIZE
#define REQ_RESIZE(w, reqwide, reqhigh, gotwide, gothigh) \
	TraceResizeRequest(__FILE__, __LINE__, w, \
			   (reqwide), (reqhigh), \
			   (gotwide), (gothigh))

extern const char * ModifierName(unsigned /* modifier */);
#define FMT_MODIFIER_NAMES "%s%s%s%s%s%s%s%s"
#define ARG_MODIFIER_NAMES(state) \
	   ModifierName(state & ShiftMask), \
	   ModifierName(state & LockMask), \
	   ModifierName(state & ControlMask), \
	   ModifierName(state & Mod1Mask), \
	   ModifierName(state & Mod2Mask), \
	   ModifierName(state & Mod3Mask), \
	   ModifierName(state & Mod4Mask), \
	   ModifierName(state & Mod5Mask)

#else

#define REQ_RESIZE(w, reqwide, reqhigh, gotwide, gothigh) \
	XtMakeResizeRequest((Widget) (w), \
			    (Dimension) (reqwide), (Dimension) (reqhigh), \
			    (gotwide), (gothigh))

#endif

extern void TraceScreen(XtermWidget /* xw */, int /* whichBuf */);

/*
 * The whole wnew->screen struct is zeroed in VTInitialize.  Use these macros
 * where applicable for copying the pieces from the request widget into the
 * new widget.  We do not have to use them for wnew->misc, but the associated
 * traces are very useful for debugging.
 */
#if OPT_TRACE
#define init_Bres(name) \
	TRACE(("init " #name " = %s\n", \
		BtoS(wnew->name = request->name)))
#define init_Dres(name) \
	TRACE(("init " #name " = %f\n", \
		wnew->name = request->name))
#define init_Dres2(name,i) \
	TRACE(("init " #name "[%d] = %f\n", i, \
		wnew->name[i] = request->name[i]))
#define init_Ires(name) \
	TRACE(("init " #name " = %d\n", \
		wnew->name = request->name))
#define init_Mres(name) \
	TRACE(("init " #name " = %s\n", \
		MtoS(wnew->name = request->name)))
#define init_Sres(name) \
	TRACE(("init " #name " = \"%s\"\n", \
		(wnew->name = x_strtrim(request->name)) != NULL \
			? wnew->name : "<null>"))
#define init_Sres2(name,i) \
	TRACE(("init " #name "[%d] = \"%s\"\n", i, \
		(wnew->name(i) = x_strtrim(request->name(i))) != NULL \
			? wnew->name(i) : "<null>"))
#define init_Tres(offset) \
	TRACE(("init screen.Tcolors[" #offset "] = %#lx\n", \
		fill_Tres(wnew, request, offset)))
#else
#define init_Bres(name)    wnew->name = request->name
#define init_Dres(name)    wnew->name = request->name
#define init_Dres2(name,i) wnew->name[i] = request->name[i]
#define init_Ires(name)    wnew->name = request->name
#define init_Mres(name)    wnew->name = request->name
#define init_Sres(name)    wnew->name = x_strtrim(request->name)
#define init_Sres2(name,i) wnew->name(i) = x_strtrim(request->name(i))
#define init_Tres(offset)  fill_Tres(wnew, request, offset)
#endif

/* *INDENT-ON* */

#endif /* included_trace_h */
