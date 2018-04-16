/* $XTermId: cachedGCs.c,v 1.67 2017/01/02 18:58:13 tom Exp $ */

/*
 * Copyright 2007-2016,2017 by Thomas E. Dickey
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

#include <data.h>
#include <xstrings.h>
#include <fontutils.h>

#include <X11/Xmu/Drawing.h>

/*
 * hide (or eliminate) calls to
 *	XCreateGC()
 *	XFreeGC()
 *	XGetGCValues()
 *	XSetBackground()
 *	XSetFont()
 *	XSetForeground()
 *	XtGetGC()
 *	XtReleaseGC()
 * by associating an integer with each GC, maintaining a cache which
 * reflects frequency of use rather than most recent usage.
 *
 * FIXME: XTermFonts should hold gc, font, fs.
 */
typedef struct {
    GC gc;
    unsigned used;
    unsigned cset;
    XTermFonts *font;
    Pixel tile;
    Pixel fg;
    Pixel bg;
} CgsCacheData;

#define DEPTH 8
#define ITEM()      (int) (me->data - me->list)
#define LIST(item)  me->list[item]
#define LINK(item)  me->data = (me->list + (item))
#define THIS(field) me->data->field
#define NEXT(field) me->next.field

#define HaveFont(font) (Boolean) ((font) != 0 && (font)->fs != 0)

#define GC_CSet GCFunction

typedef struct {
    CgsCacheData list[DEPTH];
    CgsCacheData *data;		/* points to current list[] entry */
    XtGCMask mask;		/* changes since the last getCgsGC() */
    CgsCacheData next;		/* updated values, apply in getCgsGC() */
} CgsCache;

#if OPT_TRACE
#define CASE(name) case gc##name: result = #name; break
static const char *
traceCgsEnum(CgsEnum value)
{
    const char *result = "?";
    switch (value) {
	CASE(Norm);
	CASE(Bold);
	CASE(NormReverse);
	CASE(BoldReverse);
#if OPT_BOX_CHARS
	CASE(Line);
	CASE(Dots);
#endif
#if OPT_DEC_CHRSET
	CASE(CNorm);
	CASE(CBold);
#endif
#if OPT_WIDE_CHARS
	CASE(Wide);
	CASE(WBold);
	CASE(WideReverse);
	CASE(WBoldReverse);
#endif
	CASE(VTcursNormal);
	CASE(VTcursFilled);
	CASE(VTcursReverse);
	CASE(VTcursOutline);
#if OPT_TEK4014
	CASE(TKcurs);
#endif
	CASE(MAX);
    }
    return result;
}

#undef CASE

static const char *
traceVTwin(XtermWidget xw, VTwin *value)
{
    const char *result = "?";
    if (value == 0)
	result = "null";
    else if (value == &(TScreenOf(xw)->fullVwin))
	result = "fullVwin";
#ifndef NO_ACTIVE_ICON
    else if (value == &(TScreenOf(xw)->iconVwin))
	result = "iconVwin";
#endif
    return result;
}

#if OPT_TRACE > 1
static String
traceCSet(unsigned cset)
{
    static char result[80];
    switch (cset) {
    case CSET_SWL:
	strcpy(result, "SWL");
	break;
    case CSET_DHL_TOP:
	strcpy(result, "DHL_TOP");
	break;
    case CSET_DHL_BOT:
	strcpy(result, "DHL_BOT");
	break;
    case CSET_DWL:
	strcpy(result, "DWL");
	break;
    default:
	sprintf(result, "%#x", cset);
	break;
    }
    return result;
}

static String
traceFont(XTermFonts * font)
{
    static char result[80];

    if (HaveFont(font)) {
	XFontStruct *fs = font->fs;
	sprintf(result, "%p(%dx%d %d %#lx)",
		fs,
		fs->max_bounds.width,
		fs->max_bounds.ascent + fs->max_bounds.descent,
		fs->max_bounds.descent,
		(unsigned long) (fs->fid));
    } else {
	strcpy(result, "null");
    }
    return result;
}

static String
tracePixel(XtermWidget xw, Pixel value)
{
#define CASE(name) { name, #name }
    static struct {
	TermColors code;
	String name;
    } t_colors[] = {
	CASE(TEXT_FG),
	    CASE(TEXT_BG),
	    CASE(TEXT_CURSOR),
	    CASE(MOUSE_FG),
	    CASE(MOUSE_BG),
#if OPT_TEK4014
	    CASE(TEK_FG),
	    CASE(TEK_BG),
#endif
#if OPT_HIGHLIGHT_COLOR
	    CASE(HIGHLIGHT_BG),
	    CASE(HIGHLIGHT_FG),
#endif
#if OPT_TEK4014
	    CASE(TEK_CURSOR),
#endif
    };
    TScreen *screen = TScreenOf(xw);
    String result = 0;
    int n;

    for (n = 0; n < NCOLORS; ++n) {
	if (value == T_COLOR(screen, t_colors[n].code)) {
	    result = t_colors[n].name;
	    break;
	}
    }

    if (result == 0) {
	for (n = 0; n < MAXCOLORS; ++n) {
#if OPT_COLOR_RES
	    if (screen->Acolors[n].mode > 0
		&& value == screen->Acolors[n].value) {
		result = screen->Acolors[n].resource;
		break;
	    }
#else
	    if (value == screen->Acolors[n]) {
		char temp[80];
		sprintf(temp, "Acolors[%d]", n);
		result = x_strdup(temp);
		break;
	    }
#endif
	}
    }

    if (result == 0) {
	char temp[80];
	sprintf(temp, "%#lx", value);
	result = x_strdup(temp);
    }

    return result;
}

#undef CASE

#endif /* OPT_TRACE > 1 */
#endif /* OPT_TRACE */

static CgsCache *
allocCache(void **cache_pointer)
{
    if (*cache_pointer == 0) {
	*cache_pointer = TypeCallocN(CgsCache, gcMAX);
	TRACE(("allocCache %p\n", *cache_pointer));
    }
    return *((CgsCache **) cache_pointer);
}

static int
dataIndex(CgsCache * me)
{
    return ITEM();
}

static void
relinkData(CgsCache * me, int item)
{
    LINK(item);
}

/*
 * Returns the appropriate cache pointer.
 */
static CgsCache *
myCache(XtermWidget xw, VTwin *cgsWin, CgsEnum cgsId)
{
    CgsCache *result = 0;

    if ((int) cgsId >= 0 && cgsId < gcMAX) {
#ifdef NO_ACTIVE_ICON
	(void) xw;
	(void) cgsWin;
#else
	if (cgsWin == &(TScreenOf(xw)->iconVwin))
	    result = allocCache(&(TScreenOf(xw)->icon_cgs_cache));
	else
#endif
	    result = allocCache(&(TScreenOf(xw)->main_cgs_cache));

	result += cgsId;
	if (result->data == 0) {
	    result->data = result->list;
	}
    }

    return result;
}

static Display *
myDisplay(XtermWidget xw)
{
    return TScreenOf(xw)->display;
}

static Drawable
myDrawable(XtermWidget xw, VTwin *cgsWin)
{
    Drawable drawable = 0;

    if (cgsWin != 0 && cgsWin->window != 0)
	drawable = cgsWin->window;
    if (drawable == 0)
	drawable = RootWindowOfScreen(XtScreen(xw));
    return drawable;
}

static GC
newCache(XtermWidget xw, VTwin *cgsWin, CgsEnum cgsId, CgsCache * me)
{
    XGCValues xgcv;
    XtGCMask mask;

    THIS(font) = NEXT(font);
    THIS(cset) = NEXT(cset);
    THIS(fg) = NEXT(fg);
    THIS(bg) = NEXT(bg);

    memset(&xgcv, 0, sizeof(xgcv));
    xgcv.font = NEXT(font)->fs->fid;
    mask = (GCForeground | GCBackground | GCFont);

    switch (cgsId) {
    case gcNorm:
    case gcBold:
    case gcNormReverse:
    case gcBoldReverse:
#if OPT_WIDE_CHARS
    case gcWide:
    case gcWBold:
    case gcWideReverse:
    case gcWBoldReverse:
#endif
	mask |= (GCGraphicsExposures | GCFunction);
	xgcv.graphics_exposures = True;		/* default */
	xgcv.function = GXcopy;
	break;
#if OPT_BOX_CHARS
    case gcLine:
	mask |= (GCGraphicsExposures | GCFunction);
	xgcv.graphics_exposures = True;		/* default */
	xgcv.function = GXcopy;
	break;
    case gcDots:
	xgcv.fill_style = FillTiled;
	xgcv.tile =
	    XmuCreateStippledPixmap(XtScreen((Widget) xw),
				    THIS(fg),
				    THIS(bg),
				    xw->core.depth);
	THIS(tile) = xgcv.tile;
	mask = (GCForeground | GCBackground);
	mask |= (GCGraphicsExposures | GCFunction | GCTile | GCFillStyle);
	xgcv.graphics_exposures = True;		/* default */
	xgcv.function = GXcopy;
	break;
#endif
#if OPT_DEC_CHRSET
    case gcCNorm:
    case gcCBold:
	break;
#endif
    case gcVTcursNormal:	/* FALLTHRU */
    case gcVTcursFilled:	/* FALLTHRU */
    case gcVTcursReverse:	/* FALLTHRU */
    case gcVTcursOutline:	/* FALLTHRU */
	break;
#if OPT_TEK4014
    case gcTKcurs:		/* FALLTHRU */
	/* FIXME */
#endif
    case gcMAX:		/* should not happen */
	return 0;
    }
    xgcv.foreground = NEXT(fg);
    xgcv.background = NEXT(bg);

    THIS(gc) = XCreateGC(myDisplay(xw), myDrawable(xw, cgsWin), mask, &xgcv);
    TRACE(("getCgsGC(%s) created gc %p(%d)\n",
	   traceCgsEnum(cgsId), (void *) THIS(gc), ITEM()));

    THIS(used) = 0;
    return THIS(gc);
}

static Boolean
SameFont(XTermFonts * a, XTermFonts * b)
{
    return (Boolean) (HaveFont(a)
		      && HaveFont(b)
		      && ((a->fs == b->fs)
			  || !memcmp(a->fs, b->fs, sizeof(*(a->fs)))));
}

#define SameColor(a,b) ((a) == (b))
#define SameCSet(a,b)  ((a) == (b))

static GC
chgCache(XtermWidget xw, CgsEnum cgsId GCC_UNUSED, CgsCache * me, Bool both)
{
    XGCValues xgcv;
    XtGCMask mask = (GCForeground | GCBackground | GCFont);

    memset(&xgcv, 0, sizeof(xgcv));

    TRACE2(("chgCache(%s) old data fg=%s, bg=%s, font=%s cset %s\n",
	    traceCgsEnum(cgsId),
	    tracePixel(xw, THIS(fg)),
	    tracePixel(xw, THIS(bg)),
	    traceFont(THIS(font)),
	    traceCSet(THIS(cset))));
#if OPT_TRACE > 1
    if (!SameFont(THIS(font), NEXT(font)))
	TRACE2(("...chgCache new font=%s\n", traceFont(NEXT(font))));
    if (!SameCSet(THIS(cset), NEXT(cset)))
	TRACE2(("...chgCache new cset=%s\n", traceCSet(NEXT(cset))));
    if (!SameColor(THIS(fg), NEXT(fg)))
	TRACE2(("...chgCache new fg=%s\n", tracePixel(xw, NEXT(fg))));
    if (!SameColor(THIS(bg), NEXT(bg)))
	TRACE2(("...chgCache new bg=%s\n", tracePixel(xw, NEXT(bg))));
#endif

    if (both) {
	THIS(font) = NEXT(font);
	THIS(cset) = NEXT(cset);
    }
    THIS(fg) = NEXT(fg);
    THIS(bg) = NEXT(bg);

    xgcv.font = THIS(font)->fs->fid;
    xgcv.foreground = THIS(fg);
    xgcv.background = THIS(bg);

    XChangeGC(myDisplay(xw), THIS(gc), mask, &xgcv);
    TRACE2(("...chgCache(%s) updated gc %p(%d)\n",
	    traceCgsEnum(cgsId), THIS(gc), ITEM()));

    THIS(used) = 0;
    return THIS(gc);
}

/*
 * Use the "setCgsXXXX()" calls to initialize parameters for a new GC.
 */
void
setCgsFore(XtermWidget xw, VTwin *cgsWin, CgsEnum cgsId, Pixel fg)
{
    CgsCache *me;

    if ((me = myCache(xw, cgsWin, cgsId)) != 0) {
	NEXT(fg) = fg;
	me->mask |= GCForeground;
    }
}

void
setCgsBack(XtermWidget xw, VTwin *cgsWin, CgsEnum cgsId, Pixel bg)
{
    CgsCache *me;

    if ((me = myCache(xw, cgsWin, cgsId)) != 0) {
	NEXT(bg) = bg;
	me->mask |= GCBackground;
    }
}

#if OPT_DEC_CHRSET
void
setCgsCSet(XtermWidget xw, VTwin *cgsWin, CgsEnum cgsId, unsigned cset)
{
    CgsCache *me;

    if ((me = myCache(xw, cgsWin, cgsId)) != 0) {
	NEXT(cset) = cset;
	me->mask |= GC_CSet;
    }
}
#else
#define setCgsCSet(xw, cgsWin, dstCgsId, cset)	/* nothing */
#endif

void
setCgsFont(XtermWidget xw, VTwin *cgsWin, CgsEnum cgsId, XTermFonts * font)
{
    CgsCache *me;

    if ((me = myCache(xw, cgsWin, cgsId)) != 0) {
	TScreen *screen = TScreenOf(xw);
	if (!HaveFont(font)) {
	    if (cgsId != gcNorm)
		(void) getCgsGC(xw, cgsWin, gcNorm);
#ifndef NO_ACTIVE_ICON
	    if (cgsWin == &(TScreenOf(xw)->iconVwin))
		font = getIconicFont(screen);
	    else
#endif
		font = getNormalFont(screen, fNorm);
	}
	if (HaveFont(font) && okFont(font->fs)) {
	    TRACE2(("setCgsFont next: %s for %s slot %p, gc %p\n",
		    traceFont(font), traceCgsEnum(cgsId),
		    me, THIS(gc)));
	    TRACE2(("...next font was %s\n", traceFont(NEXT(font))));
	    NEXT(font) = font;
	    me->mask |= GCFont;
	} else {
	    /* EMPTY */
	    TRACE2(("...NOT updated font for %s\n",
		    traceCgsEnum(cgsId)));
	}
    }
}

/*
 * Discard all of the font information, e.g., we are resizing the font.
 * Keep the GC's so we can simply change them rather than creating new ones.
 */
void
clrCgsFonts(XtermWidget xw, VTwin *cgsWin, XTermFonts * font)
{
    if (HaveFont(font)) {
	int j;
	for_each_gc(j) {
	    CgsCache *me;
	    if ((me = myCache(xw, cgsWin, (CgsEnum) j)) != 0) {
		int k;
		for (k = 0; k < DEPTH; ++k) {
		    if (SameFont(LIST(k).font, font)) {
			TRACE2(("clrCgsFonts %s gc %p(%d) %s\n",
				traceCgsEnum((CgsEnum) j),
				LIST(k).gc,
				k,
				traceFont(font)));
			LIST(k).font = 0;
			LIST(k).cset = 0;
		    }
		}
		if (SameFont(NEXT(font), font)) {
		    TRACE2(("clrCgsFonts %s next %s\n",
			    traceCgsEnum((CgsEnum) j),
			    traceFont(font)));
		    NEXT(font) = 0;
		    NEXT(cset) = 0;
		    me->mask &= (unsigned) ~(GCFont | GC_CSet);
		}
	    }
	}
    }
}

/*
 * Return a GC associated with the given id, allocating if needed.
 */
GC
getCgsGC(XtermWidget xw, VTwin *cgsWin, CgsEnum cgsId)
{
    CgsCache *me;
    GC result = 0;

    if ((me = myCache(xw, cgsWin, cgsId)) != 0) {
	TRACE2(("getCgsGC(%s, %s)\n",
		traceVTwin(xw, cgsWin), traceCgsEnum(cgsId)));
	if (me->mask != 0) {
	    int j;
	    unsigned used = 0;

	    /* fill in the unchanged fields */
	    if (!(me->mask & GC_CSet))
		NEXT(cset) = 0;	/* OPT_DEC_CHRSET */
	    if (!(me->mask & GCFont))
		NEXT(font) = THIS(font);
	    if (!(me->mask & GCForeground))
		NEXT(fg) = THIS(fg);
	    if (!(me->mask & GCBackground))
		NEXT(bg) = THIS(bg);

	    if (NEXT(font) == 0) {
		setCgsFont(xw, cgsWin, cgsId, 0);
	    }

	    TRACE2(("...Cgs new data fg=%s, bg=%s, font=%s cset %s\n",
		    tracePixel(xw, NEXT(fg)),
		    tracePixel(xw, NEXT(bg)),
		    traceFont(NEXT(font)),
		    traceCSet(NEXT(cset))));

	    /* try to find the given data in an already-created GC */
	    for (j = 0; j < DEPTH; ++j) {
		if (LIST(j).gc != 0
		    && SameFont(LIST(j).font, NEXT(font))
		    && SameCSet(LIST(j).cset, NEXT(cset))
		    && SameColor(LIST(j).fg, NEXT(fg))
		    && SameColor(LIST(j).bg, NEXT(bg))) {
		    LINK(j);
		    result = THIS(gc);
		    TRACE2(("getCgsGC existing %p(%d)\n", result, ITEM()));
		    break;
		}
	    }

	    if (result == 0) {
		/* try to find an empty slot, to create a new GC */
		used = 0;
		for (j = 0; j < DEPTH; ++j) {
		    if (LIST(j).gc == 0) {
			LINK(j);
			result = newCache(xw, cgsWin, cgsId, me);
			break;
		    }
		    if (used < LIST(j).used)
			used = LIST(j).used;
		}
	    }

	    if (result == 0) {
		int k;
		/* if none were empty, pick the least-used slot, to modify */
		for (j = 0, k = -1; j < DEPTH; ++j) {
		    if (used >= LIST(j).used) {
			used = LIST(j).used;
			k = j;
		    }
		}
		LINK(k);
		TRACE2(("...getCgsGC least-used(%d) was %d\n", k, THIS(used)));
		result = chgCache(xw, cgsId, me, True);
	    }
	    me->next = *(me->data);
	} else {
	    result = THIS(gc);
	}
	me->mask = 0;
	THIS(used) += 1;
	TRACE2(("...getCgsGC(%s, %s) gc %p(%d), used %d\n",
		traceVTwin(xw, cgsWin),
		traceCgsEnum(cgsId), result, ITEM(), THIS(used)));
    }
    return result;
}

/*
 * Return the font for the given GC.
 */
CgsEnum
getCgsId(XtermWidget xw, VTwin *cgsWin, GC gc)
{
    int n;
    CgsEnum result = gcNorm;

    for_each_gc(n) {
	CgsCache *me;

	if ((me = myCache(xw, cgsWin, (CgsEnum) n)) != 0) {
	    if (THIS(gc) == gc) {
		result = (CgsEnum) n;
		break;
	    }
	}
    }
    return result;
}

/*
 * Return the font for the given GC.
 */
XTermFonts *
getCgsFont(XtermWidget xw, VTwin *cgsWin, GC gc)
{
    int n;
    XTermFonts *result = 0;

    for_each_gc(n) {
	CgsCache *me;

	if ((me = myCache(xw, cgsWin, (CgsEnum) n)) != 0) {
	    if (THIS(gc) == gc) {
		result = THIS(font);
		break;
	    }
	}
    }
    return result;
}

/*
 * Return the foreground color for the given GC.
 */
Pixel
getCgsFore(XtermWidget xw, VTwin *cgsWin, GC gc)
{
    int n;
    Pixel result = 0;

    for_each_gc(n) {
	CgsCache *me;

	if ((me = myCache(xw, cgsWin, (CgsEnum) n)) != 0) {
	    if (THIS(gc) == gc) {
		result = THIS(fg);
		break;
	    }
	}
    }
    return result;
}

/*
 * Return the background color for the given GC.
 */
Pixel
getCgsBack(XtermWidget xw, VTwin *cgsWin, GC gc)
{
    int n;
    Pixel result = 0;

    for_each_gc(n) {
	CgsCache *me;

	if ((me = myCache(xw, cgsWin, (CgsEnum) n)) != 0) {
	    if (THIS(gc) == gc) {
		result = THIS(bg);
		break;
	    }
	}
    }
    return result;
}

/*
 * Copy the parameters (except GC of course) from one cache record to another.
 */
void
copyCgs(XtermWidget xw, VTwin *cgsWin, CgsEnum dstCgsId, CgsEnum srcCgsId)
{
    if (dstCgsId != srcCgsId) {
	CgsCache *me;

	if ((me = myCache(xw, cgsWin, srcCgsId)) != 0) {
	    TRACE(("copyCgs from %s to %s\n",
		   traceCgsEnum(srcCgsId),
		   traceCgsEnum(dstCgsId)));
	    TRACE2(("copyCgs from %s (me %p, fg %s, bg %s, cset %s) to %s {{\n",
		    traceCgsEnum(srcCgsId),
		    me,
		    tracePixel(xw, THIS(fg)),
		    tracePixel(xw, THIS(bg)),
		    traceCSet(THIS(cset)),
		    traceCgsEnum(dstCgsId)));
	    setCgsCSet(xw, cgsWin, dstCgsId, THIS(cset));
	    setCgsFore(xw, cgsWin, dstCgsId, THIS(fg));
	    setCgsBack(xw, cgsWin, dstCgsId, THIS(bg));
	    setCgsFont(xw, cgsWin, dstCgsId, THIS(font));
	    TRACE2(("...copyCgs }}\n"));
	}
    }
}

/*
 * Interchange colors in the cache, e.g., for reverse-video.
 */
void
redoCgs(XtermWidget xw, Pixel fg, Pixel bg, CgsEnum cgsId)
{
    VTwin *cgsWin = WhichVWin(TScreenOf(xw));
    CgsCache *me = myCache(xw, cgsWin, cgsId);

    if (me != 0) {
	CgsCacheData *save_data = me->data;
	int n;

	for (n = 0; n < DEPTH; ++n) {
	    if (LIST(n).gc != 0 && HaveFont(LIST(n).font)) {
		LINK(n);

		if (LIST(n).fg == fg
		    && LIST(n).bg == bg) {
		    setCgsFore(xw, cgsWin, cgsId, bg);
		    setCgsBack(xw, cgsWin, cgsId, fg);
		} else if (LIST(n).fg == bg
			   && LIST(n).bg == fg) {
		    setCgsFore(xw, cgsWin, cgsId, fg);
		    setCgsBack(xw, cgsWin, cgsId, bg);
		} else {
		    continue;
		}

		(void) chgCache(xw, cgsId, me, False);
	    }
	}
	me->data = save_data;
    }
}

/*
 * Swap the cache records, e.g., when doing reverse-video.
 */
void
swapCgs(XtermWidget xw, VTwin *cgsWin, CgsEnum dstCgsId, CgsEnum srcCgsId)
{
    if (dstCgsId != srcCgsId) {
	CgsCache *src;

	if ((src = myCache(xw, cgsWin, srcCgsId)) != 0) {
	    CgsCache *dst;

	    if ((dst = myCache(xw, cgsWin, dstCgsId)) != 0) {
		CgsCache tmp;
		int srcIndex = dataIndex(src);
		int dstIndex = dataIndex(dst);

		EXCHANGE(*src, *dst, tmp);

		relinkData(src, dstIndex);
		relinkData(dst, srcIndex);
	    }
	}
    }
}

/*
 * Free any GC associated with the given id.
 */
GC
freeCgs(XtermWidget xw, VTwin *cgsWin, CgsEnum cgsId)
{
    CgsCache *me;

    if ((me = myCache(xw, cgsWin, cgsId)) != 0) {
	int j;

	for (j = 0; j < DEPTH; ++j) {
	    if (LIST(j).gc != 0) {
		TRACE(("freeCgs(%s, %s) gc %p(%d)\n",
		       traceVTwin(xw, cgsWin),
		       traceCgsEnum(cgsId), (void *) LIST(j).gc, j));
		clrCgsFonts(xw, cgsWin, LIST(j).font);
#if OPT_BOX_CHARS
		if (cgsId == gcDots) {
		    XmuReleaseStippledPixmap(XtScreen((Widget) xw), LIST(j).tile);
		}
#endif
		XFreeGC(TScreenOf(xw)->display, LIST(j).gc);
		memset(&LIST(j), 0, sizeof(LIST(j)));
	    }
	    LINK(0);
	}
    }
    return 0;
}

#ifdef NO_LEAKS
void
noleaks_cachedCgs(XtermWidget xw)
{
#ifndef NO_ACTIVE_ICON
    free(TScreenOf(xw)->icon_cgs_cache);
#endif
    free(TScreenOf(xw)->main_cgs_cache);
}
#endif
