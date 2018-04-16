/* $XTermId: screen.c,v 1.521 2017/12/19 23:48:26 tom Exp $ */

/*
 * Copyright 1999-2015,2017 by Thomas E. Dickey
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

/* screen.c */

#include <stdio.h>
#include <xterm.h>
#include <error.h>
#include <data.h>
#include <xterm_io.h>

#include <X11/Xatom.h>

#if OPT_WIDE_ATTRS || OPT_WIDE_CHARS
#include <fontutils.h>
#endif

#include <menu.h>

#include <assert.h>
#include <signal.h>

#include <graphics.h>

#define inSaveBuf(screen, buf, inx) \
	((buf) == (screen)->saveBuf_index && \
	 ((inx) < (screen)->savelines || (screen)->savelines == 0))

#define getMinRow(screen) ((xw->flags & ORIGIN) ? (screen)->top_marg : 0)
#define getMaxRow(screen) ((xw->flags & ORIGIN) ? (screen)->bot_marg : (screen)->max_row)
#define getMinCol(screen) ((xw->flags & ORIGIN) ? (screen)->lft_marg : 0)
#define getMaxCol(screen) ((xw->flags & ORIGIN) ? (screen)->rgt_marg : (screen)->max_col)

#define MoveLineData(base, dst, src, len) \
	memmove(scrnHeadAddr(screen, base, (unsigned) (dst)), \
		scrnHeadAddr(screen, base, (unsigned) (src)), \
		(size_t) scrnHeadSize(screen, (unsigned) (len)))

#define SaveLineData(base, src, len) \
	(void) ScrnPointers(screen, len); \
	memcpy (screen->save_ptr, \
		scrnHeadAddr(screen, base, src), \
		(size_t) scrnHeadSize(screen, (unsigned) (len)))

#define RestoreLineData(base, dst, len) \
	memcpy (scrnHeadAddr(screen, base, dst), \
		screen->save_ptr, \
		(size_t) scrnHeadSize(screen, (unsigned) (len)))

#if OPT_SAVE_LINES
#define VisBuf(screen) screen->editBuf_index[screen->whichBuf]
#else
#define VisBuf(screen) scrnHeadAddr(screen, screen->saveBuf_index, (unsigned) savelines)
#endif

/*
 * ScrnPtr's can point to different types of data.
 */
#define SizeofScrnPtr(name) \
	(unsigned) sizeof(*((LineData *)0)->name)

/*
 * The pointers in LineData point into a block of text allocated as a single
 * chunk for the given number of rows.  Ensure that these pointers are aligned
 * at least to int-boundaries.
 */
#define AlignMask()      (sizeof(int) - 1)
#define IsAligned(value) (((unsigned long) (value) & AlignMask()) == 0)

#define AlignValue(value) \
		if (!IsAligned(value)) \
		    value = (value | (unsigned) AlignMask()) + 1

#define SetupScrnPtr(dst,src,type) \
		dst = (type *) (void *) src; \
		assert(IsAligned(dst)); \
		src += skipNcol##type

#define ScrnBufAddr(ptrs, offset)  (ScrnBuf)    ((void *) ((char *) (ptrs) + (offset)))
#define LineDataAddr(ptrs, offset) (LineData *) ((void *) ((char *) (ptrs) + (offset)))

#if OPT_TRACE > 1
static void
traceScrnBuf(const char *tag, TScreen *screen, ScrnBuf sb, unsigned len)
{
    unsigned j;

    TRACE(("traceScrnBuf %s\n", tag));
    for (j = 0; j < len; ++j) {
	LineData *src = (LineData *) scrnHeadAddr(screen, sb, j);
	TRACE(("%p %s%3d:%s\n",
	       src, ((int) j >= screen->savelines) ? "*" : " ",
	       j, visibleIChars(src->charData, src->lineSize)));
    }
    TRACE(("...traceScrnBuf %s\n", tag));
}

#define TRACE_SCRNBUF(tag, screen, sb, len) traceScrnBuf(tag, screen, sb, len)
#else
#define TRACE_SCRNBUF(tag, screen, sb, len)	/*nothing */
#endif

static unsigned
scrnHeadSize(TScreen *screen, unsigned count)
{
    unsigned result = SizeOfLineData;

    (void) screen;

#if OPT_WIDE_CHARS
    if (screen->wide_chars) {
	result += (unsigned) screen->lineExtra;
    }
#endif
    result *= count;

    return result;
}

ScrnBuf
scrnHeadAddr(TScreen *screen, ScrnBuf base, unsigned offset)
{
    unsigned size = scrnHeadSize(screen, offset);
    ScrnBuf result = ScrnBufAddr(base, size);

    assert((int) offset >= 0);

    return result;
}

/*
 * Given a block of data, build index to it in the 'base' parameter.
 */
void
setupLineData(TScreen *screen, ScrnBuf base, Char *data, unsigned nrow, unsigned ncol)
{
    unsigned i;
    unsigned offset = 0;
    unsigned jump = scrnHeadSize(screen, 1);
    LineData *ptr;
#if OPT_WIDE_CHARS
    unsigned j;
#endif
    /* these names are based on types */
    unsigned skipNcolIAttr;
    unsigned skipNcolCharData;
#if OPT_ISO_COLORS
    unsigned skipNcolCellColor;
#endif

    AlignValue(ncol);

    skipNcolIAttr = (ncol * SizeofScrnPtr(attribs));
    skipNcolCharData = (ncol * SizeofScrnPtr(charData));
#if OPT_ISO_COLORS
    skipNcolCellColor = (ncol * SizeofScrnPtr(color));
#endif

    for (i = 0; i < nrow; i++, offset += jump) {
	ptr = LineDataAddr(base, offset);

	ptr->lineSize = (Dimension) ncol;
	ptr->bufHead = 0;
#if OPT_DEC_CHRSET
	SetLineDblCS(ptr, 0);
#endif
	SetupScrnPtr(ptr->attribs, data, IAttr);
#if OPT_ISO_COLORS
	SetupScrnPtr(ptr->color, data, CellColor);
#endif
	SetupScrnPtr(ptr->charData, data, CharData);
#if OPT_WIDE_CHARS
	if (screen->wide_chars) {
	    unsigned extra = (unsigned) screen->max_combining;

	    ptr->combSize = (Char) extra;
	    for (j = 0; j < extra; ++j) {
		SetupScrnPtr(ptr->combData[j], data, CharData);
	    }
	}
#endif
    }
}

#define ExtractScrnData(name) \
		memcpy(dstPtrs->name, \
		       ((LineData *) srcPtrs)->name,\
		       dstCols * sizeof(dstPtrs->name[0])); \
		nextPtr += (srcCols * sizeof(dstPtrs->name[0]))

/*
 * As part of reallocating the screen buffer when resizing, extract from
 * the old copy of the screen buffer the data which will be used in the
 * new copy of the screen buffer.
 */
static void
extractScrnData(TScreen *screen,
		ScrnBuf dstPtrs,
		ScrnBuf srcPtrs,
		unsigned nrows,
		unsigned move_down)
{
    unsigned j;

    TRACE(("extractScrnData(nrows %d)\n", nrows));

    TRACE_SCRNBUF("extract from", screen, srcPtrs, nrows);
    for (j = 0; j < nrows; j++) {
	LineData *dst = (LineData *) scrnHeadAddr(screen,
						  dstPtrs, j + move_down);
	LineData *src = (LineData *) scrnHeadAddr(screen,
						  srcPtrs, j);
	copyLineData(dst, src);
    }
}

static ScrnPtr *
allocScrnHead(TScreen *screen, unsigned nrow)
{
    ScrnPtr *result;
    unsigned size = scrnHeadSize(screen, 1);

    result = (ScrnPtr *) calloc((size_t) nrow, (size_t) size);
    if (result == 0)
	SysError(ERROR_SCALLOC);

    TRACE(("allocScrnHead %d -> %d -> %p..%p\n", nrow, nrow * size,
	   (void *) result,
	   (char *) result + (nrow * size) - 1));
    return result;
}

/*
 * Return the size of a line's data.
 */
static unsigned
sizeofScrnRow(TScreen *screen, unsigned ncol)
{
    unsigned result;
    unsigned sizeAttribs;
#if OPT_ISO_COLORS
    unsigned sizeColors;
#endif

    (void) screen;

    result = (ncol * (unsigned) sizeof(CharData));
    AlignValue(result);

#if OPT_WIDE_CHARS
    if (screen->wide_chars) {
	result *= (unsigned) (1 + screen->max_combining);
    }
#endif

    sizeAttribs = (ncol * SizeofScrnPtr(attribs));
    AlignValue(sizeAttribs);
    result += sizeAttribs;

#if OPT_ISO_COLORS
    sizeColors = (ncol * SizeofScrnPtr(color));
    AlignValue(sizeColors);
    result += sizeColors;
#endif

    return result;
}

Char *
allocScrnData(TScreen *screen, unsigned nrow, unsigned ncol)
{
    Char *result;
    size_t length;

    AlignValue(ncol);
    length = ((nrow + 1) * sizeofScrnRow(screen, ncol));
    if (length == 0
	|| (result = (Char *) calloc(length, sizeof(Char))) == 0)
	  SysError(ERROR_SCALLOC2);

    TRACE(("allocScrnData %ux%u -> %lu -> %p..%p\n",
	   nrow, ncol, (unsigned long) length, result, result + length - 1));
    return result;
}

/*
 * Allocates memory for a 2-dimensional array of chars and returns a pointer
 * thereto.  Each line is formed from a set of char arrays, with an index
 * (i.e., the ScrnBuf type).  The first pointer in the index is reserved for
 * per-line flags, and does not point to data.
 *
 * After the per-line flags, we have a series of pointers to char arrays:  The
 * first one is the actual character array, the second one is the attributes,
 * the third is the foreground and background colors, and the fourth denotes
 * the character set.
 *
 * We store it all as pointers, because of alignment considerations.
 */
ScrnBuf
allocScrnBuf(XtermWidget xw, unsigned nrow, unsigned ncol, Char **addr)
{
    TScreen *screen = TScreenOf(xw);
    ScrnBuf base = 0;

    if (nrow != 0) {
	base = allocScrnHead(screen, nrow);
	*addr = allocScrnData(screen, nrow, ncol);

	setupLineData(screen, base, *addr, nrow, ncol);
    }

    TRACE(("allocScrnBuf %dx%d ->%p\n", nrow, ncol, (void *) base));
    return (base);
}

#if OPT_SAVE_LINES
/*
 * Copy line-data from the visible (edit) buffer to the save-lines buffer.
 */
static void
saveEditBufLines(TScreen *screen, ScrnBuf sb, unsigned n)
{
    unsigned j;

    TRACE(("...copying %d lines from editBuf to saveBuf\n", n));
#if OPT_FIFO_LINES
    (void) sb;
#endif
    for (j = 0; j < n; ++j) {
#if OPT_FIFO_LINES
	LineData *dst = addScrollback(screen);
#else
	unsigned k = (screen->savelines + j - n);
	LineData *dst = (LineData *) scrnHeadAddr(screen, sb, k);
#endif
	LineData *src = getLineData(screen, (int) j);
	copyLineData(dst, src);
    }
}

/*
 * Copy line-data from the save-lines buffer to the visible (edit) buffer.
 */
static void
unsaveEditBufLines(TScreen *screen, ScrnBuf sb, unsigned n)
{
    unsigned j;

    TRACE(("...copying %d lines from saveBuf to editBuf\n", n));
    for (j = 0; j < n; ++j) {
	int extra = (int) (n - j);
	LineData *dst = (LineData *) scrnHeadAddr(screen, sb, j);
#if OPT_FIFO_LINES
	CLineData *src;

	if (extra > screen->saved_fifo || extra > screen->savelines) {
	    TRACE(("...FIXME: must clear text!\n"));
	    continue;
	}
	src = getScrollback(screen, -extra);
#else
	unsigned k = (screen->savelines - extra);
	CLineData *src = CLineData *scrnHeadAddr(screen,
						 screen->saveBuf_index, k);
#endif
	copyLineData(dst, src);
    }
}
#endif

/*
 *  This is called when the screen is resized.
 *  Returns the number of lines the text was moved down (neg for up).
 *  (Return value only necessary with SouthWestGravity.)
 */
static int
Reallocate(XtermWidget xw,
	   ScrnBuf *sbuf,
	   Char **sbufaddr,
	   unsigned nrow,
	   unsigned ncol,
	   unsigned oldrow)
{
    TScreen *screen = TScreenOf(xw);
    ScrnBuf oldBufHead;
    ScrnBuf newBufHead;
    Char *newBufData;
    unsigned minrows;
    Char *oldBufData;
    int move_down = 0, move_up = 0;

    if (sbuf == NULL || *sbuf == NULL) {
	return 0;
    }

    oldBufData = *sbufaddr;

    TRACE(("Reallocate %dx%d -> %dx%d\n", oldrow, MaxCols(screen), nrow, ncol));

    /*
     * realloc sbuf, the pointers to all the lines.
     * If the screen shrinks, remove lines off the top of the buffer
     * if resizeGravity resource says to do so.
     */
    TRACE(("Check move_up, nrow %d vs oldrow %d (resizeGravity %s)\n",
	   nrow, oldrow,
	   BtoS(GravityIsSouthWest(xw))));
    if (GravityIsSouthWest(xw)) {
	if (nrow < oldrow) {
	    /* Remove lines off the top of the buffer if necessary. */
	    move_up = (int) (oldrow - nrow)
		- (TScreenOf(xw)->max_row - TScreenOf(xw)->cur_row);
	    if (move_up < 0)
		move_up = 0;
	    /* Overlapping move here! */
	    TRACE(("move_up %d\n", move_up));
	    if (move_up) {
		ScrnBuf dst = *sbuf;
		unsigned len = (unsigned) ((int) oldrow - move_up);

		TRACE_SCRNBUF("before move_up", screen, dst, oldrow);
		SaveLineData(dst, 0, (size_t) move_up);
		MoveLineData(dst, 0, (size_t) move_up, len);
		RestoreLineData(dst, len, (size_t) move_up);
		TRACE_SCRNBUF("after move_up", screen, dst, oldrow);
	    }
	}
    }
    oldBufHead = *sbuf;
    *sbuf = allocScrnHead(screen, (unsigned) nrow);
    newBufHead = *sbuf;

    /*
     * Create the new buffer space and copy old buffer contents there, line by
     * line.
     */
    newBufData = allocScrnData(screen, nrow, ncol);
    *sbufaddr = newBufData;

    minrows = (oldrow < nrow) ? oldrow : nrow;
    if (GravityIsSouthWest(xw)) {
	if (nrow > oldrow) {
	    /* move data down to bottom of expanded screen */
	    move_down = Min((int) (nrow - oldrow), TScreenOf(xw)->savedlines);
	}
    }

    setupLineData(screen, newBufHead, *sbufaddr, nrow, ncol);
    extractScrnData(screen, newBufHead, oldBufHead, minrows,
#if OPT_SAVE_LINES
		    0
#else
		    (unsigned) move_down
#endif
	);
    free(oldBufHead);

    /* Now free the old data */
    free(oldBufData);

    TRACE(("...Reallocate %dx%d ->%p\n", nrow, ncol, (void *) newBufHead));
    return move_down ? move_down : -move_up;	/* convert to rows */
}

#if OPT_WIDE_CHARS
/*
 * This function reallocates memory if changing the number of Buf offsets.
 * The code is based on Reallocate().
 */
static void
ReallocateBufOffsets(XtermWidget xw,
		     ScrnBuf *sbuf,
		     Char **sbufaddr,
		     unsigned nrow,
		     unsigned ncol)
{
    TScreen *screen = TScreenOf(xw);
    unsigned i;
    ScrnBuf newBufHead;
    Char *oldBufData;
    ScrnBuf oldBufHead;

    unsigned old_jump = scrnHeadSize(screen, 1);
    unsigned new_jump;
    unsigned new_ptrs = 1 + (unsigned) (screen->max_combining);
    unsigned dstCols = ncol;
    unsigned srcCols = ncol;
    LineData *dstPtrs;
    LineData *srcPtrs;
    Char *nextPtr;

    assert(nrow != 0);
    assert(ncol != 0);

    oldBufData = *sbufaddr;
    oldBufHead = *sbuf;

    /*
     * Allocate a new LineData array, retain the old one until we've copied
     * the data that it points to, as well as non-pointer data, e.g., bufHead.
     *
     * Turn on wide-chars temporarily when constructing pointers, since that is
     * used to decide whether to address the combData[] array, which affects
     * the length of the LineData structure.
     */
    screen->wide_chars = True;

    new_jump = scrnHeadSize(screen, 1);
    newBufHead = allocScrnHead(screen, nrow);
    *sbufaddr = allocScrnData(screen, nrow, ncol);
    setupLineData(screen, newBufHead, *sbufaddr, nrow, ncol);

    screen->wide_chars = False;

    nextPtr = *sbufaddr;

    srcPtrs = (LineData *) oldBufHead;
    dstPtrs = (LineData *) newBufHead;
    for (i = 0; i < nrow; i++) {
	dstPtrs->bufHead = srcPtrs->bufHead;
	ExtractScrnData(attribs);
#if OPT_ISO_COLORS
	ExtractScrnData(color);
#endif
	ExtractScrnData(charData);

	nextPtr += ncol * new_ptrs;
	srcPtrs = LineDataAddr(srcPtrs, old_jump);
	dstPtrs = LineDataAddr(dstPtrs, new_jump);
    }

    /* Now free the old data */
    free(oldBufData);
    free(oldBufHead);

    *sbuf = newBufHead;

    TRACE(("ReallocateBufOffsets %dx%d ->%p\n", nrow, ncol, *sbufaddr));
}

#if OPT_FIFO_LINES
/*
 * Allocate a new FIFO index.
 */
static void
ReallocateFifoIndex(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->savelines > 0 && screen->saveBuf_index != 0) {
	ScrnBuf newBufHead;
	LineData *dstPtrs;
	LineData *srcPtrs;
	unsigned i;
	unsigned old_jump = scrnHeadSize(screen, 1);
	unsigned new_jump;

	screen->wide_chars = True;
	newBufHead = allocScrnHead(screen, (unsigned) screen->savelines);
	new_jump = scrnHeadSize(screen, 1);

	srcPtrs = (LineData *) screen->saveBuf_index;
	dstPtrs = (LineData *) newBufHead;

	for (i = 0; i < (unsigned) screen->savelines; ++i) {
	    memcpy(dstPtrs, srcPtrs, SizeOfLineData);
	    srcPtrs = LineDataAddr(srcPtrs, old_jump);
	    dstPtrs = LineDataAddr(dstPtrs, new_jump);
	}

	screen->wide_chars = False;
	free(screen->saveBuf_index);
	screen->saveBuf_index = newBufHead;
    }
}
#endif

/*
 * This function dynamically adds support for wide-characters.
 */
void
ChangeToWide(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->wide_chars)
	return;

    TRACE(("ChangeToWide\n"));
    if (xtermLoadWideFonts(xw, True)) {
	int whichBuf = screen->whichBuf;

#if !OPT_FIFO_LINES || !OPT_SAVE_LINES
	int savelines = screen->scrollWidget ? screen->savelines : 0;

	if (savelines < 0)
	    savelines = 0;
#endif

	/*
	 * If we're displaying the alternate screen, switch the pointers back
	 * temporarily so ReallocateBufOffsets() will operate on the proper
	 * data in the alternate buffer.
	 */
	if (screen->whichBuf)
	    SwitchBufPtrs(screen, 0);

#if OPT_SAVE_LINES
#if OPT_FIFO_LINES
	ReallocateFifoIndex(xw);
#else
	ReallocateBufOffsets(xw,
			     &screen->saveBuf_index,
			     &screen->saveBuf_data,
			     (unsigned) savelines,
			     (unsigned) MaxCols(screen));
#endif
	if (screen->editBuf_index[0]) {
	    ReallocateBufOffsets(xw,
				 &screen->editBuf_index[0],
				 &screen->editBuf_data[0],
				 (unsigned) MaxRows(screen),
				 (unsigned) MaxCols(screen));
	}
#else
	ReallocateBufOffsets(xw,
			     &screen->saveBuf_index,
			     &screen->saveBuf_data,
			     (unsigned) (MaxRows(screen) + savelines),
			     (unsigned) MaxCols(screen));
#endif
	if (screen->editBuf_index[1]) {
	    ReallocateBufOffsets(xw,
				 &screen->editBuf_index[1],
				 &screen->editBuf_data[1],
				 (unsigned) MaxRows(screen),
				 (unsigned) MaxCols(screen));
	}

	screen->wide_chars = True;
	screen->visbuf = VisBuf(screen);

	/*
	 * Switch the pointers back before we start painting on the screen.
	 */
	if (whichBuf)
	    SwitchBufPtrs(screen, whichBuf);

	update_font_utf8_mode();
	SetVTFont(xw, screen->menu_font_number, True, NULL);
    }
    TRACE(("...ChangeToWide\n"));
}
#endif

/*
 * Copy cells, no side-effects.
 */
void
CopyCells(TScreen *screen, LineData *src, LineData *dst, int col, int len)
{
    (void) screen;

    if (len > 0) {
	int n;
	int last = col + len;

	for (n = col; n < last; ++n) {
	    dst->charData[n] = src->charData[n];
	    dst->attribs[n] = src->attribs[n];
	}

	if_OPT_ISO_COLORS(screen, {
	    for (n = col; n < last; ++n) {
		dst->color[n] = src->color[n];
	    }
	});
	if_OPT_WIDE_CHARS(screen, {
	    size_t off;
	    for (n = col; n < last; ++n) {
		for_each_combData(off, src) {
		    dst->combData[off][n] = src->combData[off][n];
		}
	    }
	});
    }
}

static void
FillIAttr(IAttr * target, unsigned source, size_t count)
{
    while (count-- != 0) {
	*target++ = (IAttr) source;
    }
}

/*
 * Clear cells, no side-effects.
 */
void
ClearCells(XtermWidget xw, int flags, unsigned len, int row, int col)
{
    if (len != 0) {
	TScreen *screen = TScreenOf(xw);
	LineData *ld;
	unsigned n;

	ld = getLineData(screen, row);

	flags = (int) ((unsigned) flags | TERM_COLOR_FLAGS(xw));

	for (n = 0; n < len; ++n)
	    ld->charData[(unsigned) col + n] = (CharData) ' ';

	FillIAttr(ld->attribs + col, (unsigned) flags, (size_t) len);

	if_OPT_ISO_COLORS(screen, {
	    CellColor p = xtermColorPair(xw);
	    for (n = 0; n < len; ++n) {
		ld->color[(unsigned) col + n] = p;
	    }
	});
	if_OPT_WIDE_CHARS(screen, {
	    size_t off;
	    for_each_combData(off, ld) {
		memset(ld->combData[off] + col, 0, (size_t) len * sizeof(CharData));
	    }
	});
    }
}

/*
 * Clear data in the screen-structure (no I/O).
 * Check for wide-character damage as well, clearing the damaged cells.
 */
void
ScrnClearCells(XtermWidget xw, int row, int col, unsigned len)
{
#if OPT_WIDE_CHARS
    TScreen *screen = TScreenOf(xw);
#endif
    int flags = 0;

    if_OPT_WIDE_CHARS(screen, {
	int kl;
	int kr;

	if (DamagedCells(screen, len, &kl, &kr, row, col)
	    && kr >= kl) {
	    ClearCells(xw, flags, (unsigned) (kr - kl + 1), row, kl);
	}
    });
    ClearCells(xw, flags, len, row, col);
}

/*
 * Disown the selection and repaint the area that is highlighted so it is no
 * longer highlighted.
 */
void
ScrnDisownSelection(XtermWidget xw)
{
    if (ScrnHaveSelection(TScreenOf(xw))) {
	if (TScreenOf(xw)->keepSelection) {
	    UnhiliteSelection(xw);
	} else {
	    DisownSelection(xw);
	}
    }
}

/*
 * Writes str into buf at screen's current row and column.  Characters are set
 * to match flags.
 */
void
ScrnWriteText(XtermWidget xw,
	      IChar *str,
	      unsigned flags,
	      CellColor cur_fg_bg,
	      unsigned length)
{
    TScreen *screen = TScreenOf(xw);
    LineData *ld;
    IAttr *attrs;
    int avail = MaxCols(screen) - screen->cur_col;
    IChar *chars;
#if OPT_WIDE_CHARS
    IChar starcol1;
#endif
    unsigned n;
    unsigned real_width = visual_width(str, length);

    (void) cur_fg_bg;		/* quiet compiler warnings when unused */

    if (real_width + (unsigned) screen->cur_col > (unsigned) MaxCols(screen)) {
	real_width = (unsigned) (MaxCols(screen) - screen->cur_col);
    }

    if (avail <= 0)
	return;
    if (length > (unsigned) avail)
	length = (unsigned) avail;
    if (length == 0 || real_width == 0)
	return;

    ld = getLineData(screen, screen->cur_row);

    chars = ld->charData + screen->cur_col;
    attrs = ld->attribs + screen->cur_col;

#if OPT_WIDE_CHARS
    starcol1 = *chars;
#endif

    /* write blanks if we're writing invisible text */
    for (n = 0; n < length; ++n) {
	if ((flags & INVISIBLE))
	    chars[n] = ' ';
	else
	    chars[n] = str[n];
    }

#if OPT_BLINK_TEXT
    if ((flags & BLINK) && !(screen->blink_as_bold)) {
	LineSetBlinked(ld);
    }
#endif

    if_OPT_WIDE_CHARS(screen, {

	if (real_width != length) {
	    IChar *char1 = chars;
	    if (screen->cur_col
		&& starcol1 == HIDDEN_CHAR
		&& isWide((int) char1[-1])) {
		char1[-1] = (CharData) ' ';
	    }
	    /* if we are overwriting the right hand half of a
	       wide character, make the other half vanish */
	    while (length) {
		int ch = (int) str[0];

		*char1++ = *str++;
		length--;

		if (isWide(ch)) {
		    *char1++ = (CharData) HIDDEN_CHAR;
		}
	    }

	    if (*char1 == HIDDEN_CHAR
		&& char1[-1] == HIDDEN_CHAR) {
		*char1 = (CharData) ' ';
	    }
	    /* if we are overwriting the left hand half of a
	       wide character, make the other half vanish */
	} else {
	    if (screen->cur_col
		&& starcol1 == HIDDEN_CHAR
		&& isWide((int) chars[-1])) {
		chars[-1] = (CharData) ' ';
	    }
	    /* if we are overwriting the right hand half of a
	       wide character, make the other half vanish */
	    if (chars[length] == HIDDEN_CHAR
		&& isWide((int) chars[length - 1])) {
		chars[length] = (CharData) ' ';
	    }
	}
    });

    flags &= ATTRIBUTES;
    flags |= CHARDRAWN;
    FillIAttr(attrs, flags, (size_t) real_width);

    if_OPT_WIDE_CHARS(screen, {
	size_t off;
	for_each_combData(off, ld) {
	    memset(ld->combData[off] + screen->cur_col,
		   0,
		   real_width * sizeof(CharData));
	}
    });
    if_OPT_ISO_COLORS(screen, {
	unsigned j;
	for (j = 0; j < real_width; ++j)
	    ld->color[screen->cur_col + (int) j] = cur_fg_bg;
    });

#if OPT_WIDE_CHARS
    screen->last_written_col = screen->cur_col + (int) real_width - 1;
    screen->last_written_row = screen->cur_row;
#endif

    TRACE(("text erasing cur_col=%d cur_row=%d real_width=%d\n",
	   screen->cur_col,
	   screen->cur_row,
	   real_width));
    chararea_clear_displayed_graphics(screen,
				      screen->cur_col,
				      screen->cur_row,
				      (int) real_width, 1);

    if_OPT_XMC_GLITCH(screen, {
	Resolve_XMC(xw);
    });

    return;
}

/*
 * Saves pointers to the n lines beginning at sb + where, and clears the lines
 */
static void
ScrnClearLines(XtermWidget xw, ScrnBuf sb, int where, unsigned n, unsigned size)
{
    TScreen *screen = TScreenOf(xw);
    ScrnPtr *base;
    unsigned jump = scrnHeadSize(screen, 1);
    unsigned i;
    LineData *work;
    unsigned flags = TERM_COLOR_FLAGS(xw);
#if OPT_ISO_COLORS
    unsigned j;
#endif

    TRACE(("ScrnClearLines(%s:where %d, n %d, size %d)\n",
	   (sb == screen->saveBuf_index) ? "save" : "edit",
	   where, n, size));

    assert((int) n > 0);
    assert(size != 0);

    /* save n lines at where */
    SaveLineData(sb, (unsigned) where, (size_t) n);

    /* clear contents of old rows */
    base = screen->save_ptr;
    for (i = 0; i < n; ++i) {
	work = (LineData *) base;
	work->bufHead = 0;
#if OPT_DEC_CHRSET
	SetLineDblCS(work, 0);
#endif

	memset(work->charData, 0, size * sizeof(CharData));
	if (TERM_COLOR_FLAGS(xw)) {
	    FillIAttr(work->attribs, flags, (size_t) size);
#if OPT_ISO_COLORS
	    {
		CellColor p = xtermColorPair(xw);
		for (j = 0; j < size; ++j) {
		    work->color[j] = p;
		}
	    }
#endif
	} else {
	    FillIAttr(work->attribs, 0, (size_t) size);
#if OPT_ISO_COLORS
	    memset(work->color, 0, size * sizeof(work->color[0]));
#endif
	}
#if OPT_WIDE_CHARS
	if (screen->wide_chars) {
	    size_t off;

	    for (off = 0; off < work->combSize; ++off) {
		memset(work->combData[off], 0, size * sizeof(CharData));
	    }
	}
#endif
	base = ScrnBufAddr(base, jump);
    }

    TRACE(("clear lines erasing where=%d screen->savelines=%d n=%d screen->max_col=%d\n",
	   where,
	   screen->savelines,
	   n,
	   screen->max_col));
    /* FIXME: this looks wrong -- rcombs */
    chararea_clear_displayed_graphics(screen,
				      where + screen->savelines,
				      0,
				      screen->max_col + 1,
				      (int) n);
}

/*
 * We're always ensured of having a visible buffer, but may not have saved
 * lines.  Check the pointer that's sure to work.
 */
#if OPT_SAVE_LINES
#define OkAllocBuf(screen) (screen->editBuf_index[0] != 0)
#else
#define OkAllocBuf(screen) (screen->saveBuf_index != 0)
#endif

void
ScrnAllocBuf(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);

    if (!OkAllocBuf(screen)) {
	int nrows = MaxRows(screen);
#if !OPT_SAVE_LINES
	int savelines = screen->scrollWidget ? screen->savelines : 0;
#endif

	TRACE(("ScrnAllocBuf %dx%d (%d)\n",
	       nrows, MaxCols(screen), screen->savelines));

#if OPT_SAVE_LINES
	if (screen->savelines != 0) {
#if OPT_FIFO_LINES
	    /* for FIFO, we only need space for the index - addScrollback inits */
	    screen->saveBuf_index = allocScrnHead(screen,
						  (unsigned) (screen->savelines));
#else
	    screen->saveBuf_index = allocScrnBuf(xw,
						 (unsigned) screen->savelines,
						 (unsigned) MaxCols(screen),
						 &screen->saveBuf_data);
#endif
	} else {
	    screen->saveBuf_index = 0;
	}
	screen->editBuf_index[0] = allocScrnBuf(xw,
						(unsigned) nrows,
						(unsigned) MaxCols(screen),
						&screen->editBuf_data[0]);
#else /* !OPT_SAVE_LINES */
	screen->saveBuf_index = allocScrnBuf(xw,
					     (unsigned) (nrows + screen->savelines),
					     (unsigned) (MaxCols(screen)),
					     &screen->saveBuf_data);
#endif /* OPT_SAVE_LINES */
	screen->visbuf = VisBuf(screen);
    }
    return;
}

size_t
ScrnPointers(TScreen *screen, size_t len)
{
    size_t result = scrnHeadSize(screen, (unsigned) len);

    if (result > screen->save_len) {
	if (screen->save_len)
	    screen->save_ptr = (ScrnPtr *) realloc(screen->save_ptr, result);
	else
	    screen->save_ptr = (ScrnPtr *) malloc(result);
	screen->save_len = len;
	if (screen->save_ptr == 0)
	    SysError(ERROR_SAVE_PTR);
    }
    TRACE2(("ScrnPointers %ld ->%p\n", (long) len, screen->save_ptr));
    return result;
}

/*
 * Inserts n blank lines at sb + where, treating last as a bottom margin.
 */
void
ScrnInsertLine(XtermWidget xw, ScrnBuf sb, int last, int where, unsigned n)
{
    TScreen *screen = TScreenOf(xw);
    unsigned size = (unsigned) MaxCols(screen);

    TRACE(("ScrnInsertLine(last %d, where %d, n %d, size %d)\n",
	   last, where, n, size));

    if ((int) n > last)
	n = (unsigned) last;

    assert(where >= 0);
    assert(last >= where);

    assert((int) n > 0);
    assert(size != 0);

    /* save n lines at bottom */
    ScrnClearLines(xw, sb, (last -= (int) n - 1), n, size);

    /*
     * WARNING, overlapping copy operation.  Move down lines (pointers).
     *
     *   +----|---------|--------+
     *
     * is copied in the array to:
     *
     *   +--------|---------|----+
     */
    assert(last >= where);
    /*
     * This will never shift from the saveBuf to editBuf, so there is no need
     * to handle that case.
     */
    MoveLineData(sb,
		 (unsigned) (where + (int) n),
		 (unsigned) where,
		 (unsigned) (last - where));

    /* reuse storage for new lines at where */
    RestoreLineData(sb, (unsigned) where, n);
}

/*
 * Deletes n lines at sb + where, treating last as a bottom margin.
 */
void
ScrnDeleteLine(XtermWidget xw, ScrnBuf sb, int last, int where, unsigned n)
{
    TScreen *screen = TScreenOf(xw);
    unsigned size = (unsigned) MaxCols(screen);

    TRACE(("ScrnDeleteLine(%s:last %d, where %d, n %d, size %d)\n",
	   (sb == screen->saveBuf_index) ? "save" : "edit",
	   last, where, n, size));

    assert(where >= 0);
    assert(last >= where + (int) n - 1);

    assert((int) n > 0);
    assert(size != 0);

    /* move up lines */
    last -= ((int) n - 1);
#if OPT_SAVE_LINES
    if (inSaveBuf(screen, sb, where)) {
#if !OPT_FIFO_LINES
	int from = where + n;
#endif

	/* we shouldn't be editing the saveBuf, only scroll into it */
	assert(last >= screen->savelines);

	if (sb != 0) {
#if OPT_FIFO_LINES
	    /* copy lines from editBuf to saveBuf (allocating as we go...) */
	    saveEditBufLines(screen, sb, n);
#else
	    ScrnClearLines(xw, sb, where, n, size);

	    /* move the pointers within saveBuf */
	    TRACE(("...%smoving pointers in saveBuf (compare %d %d)\n",
		   ((screen->savelines > from)
		    ? ""
		    : "SKIP "),
		   screen->savelines,
		   from));
	    if (screen->savelines > from) {
		MoveLineData(sb,
			     (unsigned) where,
			     (unsigned) from,
			     (unsigned) (screen->savelines - from));
	    }

	    /* reuse storage in saveBuf */
	    TRACE(("...reuse %d lines storage in saveBuf\n", n));
	    RestoreLineData(sb, (unsigned) screen->savelines - n, n);

	    /* copy lines from editBuf to saveBuf (into the reused storage) */
	    saveEditBufLines(screen, sb, n);
#endif
	}

	/* adjust variables to fall-thru into changes only to editBuf */
	TRACE(("...adjusting variables, to work on editBuf alone\n"));
	last -= screen->savelines;
	where = 0;
	sb = screen->visbuf;
    }
#endif
    /*
     * Scroll the visible buffer (editBuf).
     */
    ScrnClearLines(xw, sb, where, n, size);

    MoveLineData(sb,
		 (unsigned) where,
		 (unsigned) (where + (int) n),
		 (size_t) (last - where));

    /* reuse storage for new bottom lines */
    RestoreLineData(sb, (unsigned) last, n);
}

/*
 * Inserts n blanks in screen at current row, col.  Size is the size of each
 * row.
 */
void
ScrnInsertChar(XtermWidget xw, unsigned n)
{
#define MemMove(data) \
    	for (j = last; j >= (col + (int) n); --j) \
	    data[j] = data[j - (int) n]

    TScreen *screen = TScreenOf(xw);
    int first = ScrnLeftMargin(xw);
    int last = ScrnRightMargin(xw);
    int row = screen->cur_row;
    int col = screen->cur_col;
    int j;
    LineData *ld;

    if (col < first || col > last) {
	TRACE(("ScrnInsertChar - col %d outside [%d..%d]\n", col, first, last));
	return;
    } else if (last < (col + (int) n)) {
	n = (unsigned) (last + 1 - col);
    }

    assert(screen->cur_col >= 0);
    assert(screen->cur_row >= 0);
    assert((int) n >= 0);
    assert((last + 1) >= (int) n);

    if_OPT_WIDE_CHARS(screen, {
	int xx = screen->cur_row;
	int kl;
	int kr = screen->cur_col;
	if (DamagedCells(screen, n, &kl, (int *) 0, xx, kr) && kr > kl) {
	    ClearCells(xw, 0, (unsigned) (kr - kl + 1), row, kl);
	}
	kr = last - (int) n + 1;
	if (DamagedCells(screen, n, &kl, (int *) 0, xx, kr) && kr > kl) {
	    ClearCells(xw, 0, (unsigned) (kr - kl + 1), row, kl);
	}
    });

    if ((ld = getLineData(screen, row)) != 0) {
	MemMove(ld->charData);
	MemMove(ld->attribs);

	if_OPT_ISO_COLORS(screen, {
	    MemMove(ld->color);
	});
	if_OPT_WIDE_CHARS(screen, {
	    size_t off;
	    for_each_combData(off, ld) {
		MemMove(ld->combData[off]);
	    }
	});
    }
    ClearCells(xw, CHARDRAWN, n, row, col);

#undef MemMove
}

/*
 * Deletes n characters at current row, col.
 */
void
ScrnDeleteChar(XtermWidget xw, unsigned n)
{
#define MemMove(data) \
    	for (j = col; j <= last - (int) n; ++j) \
	    data[j] = data[j + (int) n]

    TScreen *screen = TScreenOf(xw);
    int first = ScrnLeftMargin(xw);
    int last = ScrnRightMargin(xw) + 1;
    int row = screen->cur_row;
    int col = screen->cur_col;
    int j;
    LineData *ld;

    if (col < first || col > last) {
	TRACE(("ScrnDeleteChar - col %d outside [%d..%d]\n", col, first, last));
	return;
    } else if (last <= (col + (int) n)) {
	n = (unsigned) (last - col);
    }

    assert(screen->cur_col >= 0);
    assert(screen->cur_row >= 0);
    assert((int) n >= 0);
    assert(last >= (int) n);

    if_OPT_WIDE_CHARS(screen, {
	int kl;
	int kr;
	if (DamagedCells(screen, n, &kl, &kr,
			 screen->cur_row,
			 screen->cur_col))
	    ClearCells(xw, 0, (unsigned) (kr - kl + 1), row, kl);
    });

    if ((ld = getLineData(screen, row)) != 0) {
	MemMove(ld->charData);
	MemMove(ld->attribs);

	if_OPT_ISO_COLORS(screen, {
	    MemMove(ld->color);
	});
	if_OPT_WIDE_CHARS(screen, {
	    size_t off;
	    for_each_combData(off, ld) {
		MemMove(ld->combData[off]);
	    }
	});
	LineClrWrapped(ld);
	if (screen->show_wrap_marks) {
	    ShowWrapMarks(xw, row, ld);
	}
    }
    ClearCells(xw, 0, n, row, (last - (int) n));

#undef MemMove
}

/*
 * This is useful for debugging both xterm and applications that may manipulate
 * its line-wrapping state.
 */
void
ShowWrapMarks(XtermWidget xw, int row, CLineData *ld)
{
    TScreen *screen = TScreenOf(xw);
    Boolean set = (Boolean) LineTstWrapped(ld);
    CgsEnum cgsId = set ? gcVTcursFilled : gcVTcursReverse;
    VTwin *currentWin = WhichVWin(screen);
    int y = row * FontHeight(screen) + screen->border;
    int x = LineCursorX(screen, ld, screen->max_col + 1);

    TRACE2(("ShowWrapMarks %d:%s\n", row, BtoS(set)));

    XFillRectangle(screen->display, VDrawable(screen),
		   getCgsGC(xw, currentWin, cgsId),
		   x, y,
		   (unsigned) screen->border,
		   (unsigned) FontHeight(screen));
}

#if OPT_WIDE_ATTRS
static unsigned
refreshFontGCs(XtermWidget xw, unsigned new_attrs, unsigned old_attrs)
{
    if ((new_attrs & ATR_ITALIC) && !(old_attrs & ATR_ITALIC)) {
	xtermLoadItalics(xw);
	xtermUpdateFontGCs(xw, True);
    } else if (!(new_attrs & ATR_ITALIC) && (old_attrs & ATR_ITALIC)) {
	xtermUpdateFontGCs(xw, False);
    }
    return new_attrs;
}
#endif

/*
 * Repaints the area enclosed by the parameters.
 * Requires: (toprow, leftcol), (toprow + nrows, leftcol + ncols) are
 *	     coordinates of characters in screen;
 *	     nrows and ncols positive.
 *	     all dimensions are based on single-characters.
 */
void
ScrnRefresh(XtermWidget xw,
	    int toprow,
	    int leftcol,
	    int nrows,
	    int ncols,
	    Bool force)		/* ... leading/trailing spaces */
{
    TScreen *screen = TScreenOf(xw);
    CLineData *ld;
    int y = toprow * FontHeight(screen) + screen->border;
    int row;
    int maxrow = toprow + nrows - 1;
    int scrollamt = screen->scroll_amt;
    unsigned gc_changes = 0;
#ifdef __CYGWIN__
    static char first_time = 1;
#endif
    static int recurse = 0;
#if OPT_WIDE_ATTRS
    unsigned old_attrs = xw->flags;
#endif

    TRACE(("ScrnRefresh top %d (%d,%d) - (%d,%d)%s {{\n",
	   screen->topline, toprow, leftcol,
	   nrows, ncols,
	   force ? " force" : ""));

    ++recurse;

    if (screen->cursorp.col >= leftcol
	&& screen->cursorp.col <= (leftcol + ncols - 1)
	&& screen->cursorp.row >= ROW2INX(screen, toprow)
	&& screen->cursorp.row <= ROW2INX(screen, maxrow))
	screen->cursor_state = OFF;

    for (row = toprow; row <= maxrow; y += FontHeight(screen), row++) {
#if OPT_ISO_COLORS
	CellColor *fb = 0;
#define ColorOf(col) (fb ? fb[col] : initCColor)
#endif
#if OPT_WIDE_CHARS
	int wideness = 0;
#endif
#define BLANK_CEL(cell) (chars[cell] == ' ')
	IChar *chars;
	const IAttr *attrs;
	int col = leftcol;
	int maxcol = leftcol + ncols - 1;
	int hi_col = maxcol;
	int lastind;
	unsigned flags;
	unsigned test;
	CellColor fg_bg = initCColor;
	Pixel fg = 0, bg = 0;
	int x;
	GC gc;
	Bool hilite;

	(void) fg;
	(void) bg;
#if !OPT_ISO_COLORS
	fg_bg = 0;
#endif

	if (row < screen->top_marg || row > screen->bot_marg)
	    lastind = row;
	else
	    lastind = row - scrollamt;

	if (lastind < 0 || lastind > screen->max_row)
	    continue;

	TRACE2(("ScrnRefresh row=%d lastind=%d ->%d\n",
		row, lastind, ROW2INX(screen, lastind)));

	if ((ld = getLineData(screen, ROW2INX(screen, lastind))) == 0
	    || ld->charData == 0
	    || ld->attribs == 0) {
	    break;
	}

	if (screen->show_wrap_marks) {
	    ShowWrapMarks(xw, lastind, ld);
	}

	if (maxcol >= (int) ld->lineSize) {
	    maxcol = ld->lineSize - 1;
	    hi_col = maxcol;
	}

	chars = ld->charData;
	attrs = ld->attribs;

	if_OPT_WIDE_CHARS(screen, {
	    /* This fixes an infinite recursion bug, that leads
	       to display anomalies. It seems to be related to
	       problems with the selection. */
	    if (recurse < 3) {
		/* adjust to redraw all of a widechar if we just wanted
		   to draw the right hand half */
		if (leftcol > 0 &&
		    chars[leftcol] == HIDDEN_CHAR &&
		    isWide((int) chars[leftcol - 1])) {
		    leftcol--;
		    ncols++;
		    col = leftcol;
		}
	    } else {
		xtermWarning("Unexpected recursion drawing hidden characters.\n");
	    }
	});

	if (row < screen->startH.row || row > screen->endH.row ||
	    (row == screen->startH.row && maxcol < screen->startH.col) ||
	    (row == screen->endH.row && col >= screen->endH.col)) {
#if OPT_DEC_CHRSET
	    /*
	     * Temporarily change dimensions to double-sized characters so
	     * we can reuse the recursion on this function.
	     */
	    if (CSET_DOUBLE(GetLineDblCS(ld))) {
		col /= 2;
		maxcol /= 2;
	    }
#endif
	    /*
	     * If row does not intersect selection; don't hilite blanks.
	     */
	    if (!force) {
		while (col <= maxcol && (attrs[col] & ~BOLD) == 0 &&
		       BLANK_CEL(col))
		    col++;

		while (col <= maxcol && (attrs[maxcol] & ~BOLD) == 0 &&
		       BLANK_CEL(maxcol))
		    maxcol--;
	    }
#if OPT_DEC_CHRSET
	    if (CSET_DOUBLE(GetLineDblCS(ld))) {
		col *= 2;
		maxcol *= 2;
	    }
#endif
	    hilite = False;
	} else {
	    /* row intersects selection; split into pieces of single type */
	    if (row == screen->startH.row && col < screen->startH.col) {
		ScrnRefresh(xw, row, col, 1, screen->startH.col - col,
			    force);
		col = screen->startH.col;
	    }
	    if (row == screen->endH.row && maxcol >= screen->endH.col) {
		ScrnRefresh(xw, row, screen->endH.col, 1,
			    maxcol - screen->endH.col + 1, force);
		maxcol = screen->endH.col - 1;
	    }

	    /*
	     * If we're highlighting because the user is doing cut/paste,
	     * trim the trailing blanks from the highlighted region so we're
	     * showing the actual extent of the text that'll be cut.  If
	     * we're selecting a blank line, we'll highlight one column
	     * anyway.
	     *
	     * We don't do this if the mouse-hilite mode is set because that
	     * would be too confusing.
	     *
	     * The default if the highlightSelection resource isn't set will
	     * highlight the whole width of the terminal, which is easy to
	     * see, but harder to use (because trailing blanks aren't as
	     * apparent).
	     */
	    if (screen->highlight_selection
		&& screen->send_mouse_pos != VT200_HIGHLIGHT_MOUSE) {
		hi_col = screen->max_col;
		while (hi_col > 0 && !(attrs[hi_col] & CHARDRAWN))
		    hi_col--;
	    }

	    /* remaining piece should be hilited */
	    hilite = True;
	}

	if (col > maxcol)
	    continue;

	/*
	 * Go back to double-sized character dimensions if the line has
	 * double-width characters.  Note that 'hi_col' is already in the
	 * right units.
	 */
	if_OPT_DEC_CHRSET({
	    if (CSET_DOUBLE(GetLineDblCS(ld))) {
		col /= 2;
		maxcol /= 2;
	    }
	});

	flags = attrs[col];

	if_OPT_WIDE_CHARS(screen, {
	    wideness = isWide((int) chars[col]);
	});

	if_OPT_ISO_COLORS(screen, {
	    fb = ld->color;
	    fg_bg = ColorOf(col);
	    fg = extract_fg(xw, fg_bg, flags);
	    bg = extract_bg(xw, fg_bg, flags);
	});

#if OPT_WIDE_ATTRS
	old_attrs = refreshFontGCs(xw, flags, old_attrs);
#endif
	gc = updatedXtermGC(xw, flags, fg_bg, hilite);
	gc_changes |= (flags & (FG_COLOR | BG_COLOR));

	x = LineCursorX(screen, ld, col);
	lastind = col;

	for (; col <= maxcol; col++) {
	    if ((attrs[col] != flags)
		|| (hilite && (col > hi_col))
#if OPT_ISO_COLORS
		|| ((flags & FG_COLOR)
		    && (extract_fg(xw, ColorOf(col), attrs[col]) != fg))
		|| ((flags & BG_COLOR)
		    && (extract_bg(xw, ColorOf(col), attrs[col]) != bg))
#endif
#if OPT_WIDE_CHARS
		|| (isWide((int) chars[col]) != wideness
		    && chars[col] != HIDDEN_CHAR)
#endif
		) {
		assert(col >= lastind);
		TRACE(("ScrnRefresh looping drawXtermText %d..%d:%s\n",
		       lastind, col,
		       visibleIChars((&chars[lastind]),
				     (unsigned) (col - lastind))));

		test = flags;
		checkVeryBoldColors(test, fg);

		x = drawXtermText(xw,
				  test & DRAWX_MASK,
				  0,
				  gc, x, y,
				  GetLineDblCS(ld),
				  &chars[lastind],
				  (unsigned) (col - lastind), 0);

		if_OPT_WIDE_CHARS(screen, {
		    int i;
		    size_t off;

		    for_each_combData(off, ld) {
			IChar *com_off = ld->combData[off];

			for (i = lastind; i < col; i++) {
			    int my_x = LineCursorX(screen, ld, i);
			    IChar base = chars[i];

			    if (isWide((int) base))
				my_x = LineCursorX(screen, ld, i - 1);

			    if (com_off[i] != 0)
				drawXtermText(xw,
					      (test & DRAWX_MASK),
					      NOBACKGROUND,
					      gc, my_x, y,
					      GetLineDblCS(ld),
					      com_off + i,
					      1, isWide((int) base));
			}
		    }
		});

		resetXtermGC(xw, flags, hilite);

		lastind = col;

		if (hilite && (col > hi_col))
		    hilite = False;

		flags = attrs[col];
		if_OPT_ISO_COLORS(screen, {
		    fg_bg = ColorOf(col);
		    fg = extract_fg(xw, fg_bg, flags);
		    bg = extract_bg(xw, fg_bg, flags);
		});
		if_OPT_WIDE_CHARS(screen, {
		    wideness = isWide((int) chars[col]);
		});

#if OPT_WIDE_ATTRS
		old_attrs = refreshFontGCs(xw, flags, old_attrs);
#endif
		gc = updatedXtermGC(xw, flags, fg_bg, hilite);
		gc_changes |= (flags & (FG_COLOR | BG_COLOR));
	    }

	    if (chars[col] == 0) {
		chars[col] = ' ';
	    }
	}

	assert(col >= lastind);
	TRACE(("ScrnRefresh calling drawXtermText %d..%d:%s\n",
	       lastind, col,
	       visibleIChars(&chars[lastind], (unsigned) (col - lastind))));

	test = flags;
	checkVeryBoldColors(test, fg);

	drawXtermText(xw,
		      test & DRAWX_MASK,
		      0,
		      gc, x, y,
		      GetLineDblCS(ld),
		      &chars[lastind],
		      (unsigned) (col - lastind), 0);

	if_OPT_WIDE_CHARS(screen, {
	    int i;
	    size_t off;

	    for_each_combData(off, ld) {
		IChar *com_off = ld->combData[off];

		for (i = lastind; i < col; i++) {
		    int my_x = LineCursorX(screen, ld, i);
		    int base = (int) chars[i];

		    if (isWide(base))
			my_x = LineCursorX(screen, ld, i - 1);

		    if (com_off[i] != 0)
			drawXtermText(xw,
				      (test & DRAWX_MASK),
				      NOBACKGROUND,
				      gc, my_x, y,
				      GetLineDblCS(ld),
				      com_off + i,
				      1, isWide(base));
		}
	    }
	});

	resetXtermGC(xw, flags, hilite);
    }

    refresh_displayed_graphics(xw, leftcol, toprow, ncols, nrows);

    /*
     * If we're in color mode, reset the various GC's to the current
     * screen foreground and background so that other functions (e.g.,
     * ClearRight) will get the correct colors.
     */
#if OPT_WIDE_ATTRS
    (void) refreshFontGCs(xw, xw->flags, old_attrs);
#endif
    if_OPT_ISO_COLORS(screen, {
	if (gc_changes & FG_COLOR)
	    SGR_Foreground(xw, xw->cur_foreground);
	if (gc_changes & BG_COLOR)
	    SGR_Background(xw, xw->cur_background);
    });

#if defined(__CYGWIN__) && defined(TIOCSWINSZ)
    if (first_time == 1) {
	first_time = 0;
	update_winsize(screen->respond, nrows, ncols, xw->core.height, xw->core.width);
    }
#endif
    recurse--;

    TRACE(("...}} ScrnRefresh\n"));
    return;
}

/*
 * Call this wrapper to ScrnRefresh() when the data has changed.  If the
 * refresh region overlaps the selection, we will release the primary selection.
 */
void
ScrnUpdate(XtermWidget xw,
	   int toprow,
	   int leftcol,
	   int nrows,
	   int ncols,
	   Bool force)		/* ... leading/trailing spaces */
{
    TScreen *screen = TScreenOf(xw);

    if (ScrnHaveSelection(screen)
	&& (toprow <= screen->endH.row)
	&& (toprow + nrows - 1 >= screen->startH.row)) {
	ScrnDisownSelection(xw);
    }
    ScrnRefresh(xw, toprow, leftcol, nrows, ncols, force);
}

/*
 * Sets the rows first though last of the buffer of screen to spaces.
 * Requires first <= last; first, last are rows of screen->buf.
 */
void
ClearBufRows(XtermWidget xw,
	     int first,
	     int last)
{
    TScreen *screen = TScreenOf(xw);
    unsigned len = (unsigned) MaxCols(screen);
    int row;

    TRACE(("ClearBufRows %d..%d\n", first, last));
    for (row = first; row <= last; row++) {
	LineData *ld = getLineData(screen, row);
	if (ld != 0) {
	    if_OPT_DEC_CHRSET({
		/* clearing the whole row resets the doublesize characters */
		SetLineDblCS(ld, CSET_SWL);
	    });
	    LineClrWrapped(ld);
	    if (screen->show_wrap_marks) {
		ShowWrapMarks(xw, row, ld);
	    }
	    ClearCells(xw, 0, len, row, 0);
	}
    }
}

/*
  Resizes screen:
  1. If new window would have fractional characters, sets window size so as to
  discard fractional characters and returns -1.
  Minimum screen size is 1 X 1.
  Note that this causes another ExposeWindow event.
  2. Enlarges screen->buf if necessary.  New space is appended to the bottom
  and to the right
  3. Reduces  screen->buf if necessary.  Old space is removed from the bottom
  and from the right
  4. Cursor is positioned as closely to its former position as possible
  5. Sets screen->max_row and screen->max_col to reflect new size
  6. Maintains the inner border (and clears the border on the screen).
  7. Clears origin mode and sets scrolling region to be entire screen.
  8. Returns 0
  */
int
ScreenResize(XtermWidget xw,
	     int width,
	     int height,
	     unsigned *flags)
{
    TScreen *screen = TScreenOf(xw);
    int rows, cols;
    const int border = 2 * screen->border;
    int move_down_by = 0;

    TRACE(("ScreenResize %dx%d border %d font %dx%d\n",
	   height, width, border,
	   FontHeight(screen), FontWidth(screen)));

    assert(width > 0);
    assert(height > 0);

    if (screen->is_running) {
	/* clear the right and bottom internal border because of NorthWest
	   gravity might have left junk on the right and bottom edges */
	if (width >= (int) FullWidth(screen)) {
#if OPT_DOUBLE_BUFFER
	    XFillRectangle(screen->display, VDrawable(screen),
			   ReverseGC(xw, screen),
			   FullWidth(screen), 0,
			   width - FullWidth(screen), height);
#else
	    XClearArea(screen->display, VDrawable(screen),
		       FullWidth(screen), 0,	/* right edge */
		       0, (unsigned) height,	/* from top to bottom */
		       False);
#endif
	}
	if (height >= (int) FullHeight(screen)) {
#if OPT_DOUBLE_BUFFER
	    XFillRectangle(screen->display, VDrawable(screen),
			   ReverseGC(xw, screen),
			   0, FullHeight(screen),
			   width, height - FullHeight(screen));
#else
	    XClearArea(screen->display, VDrawable(screen),
		       0, FullHeight(screen),	/* bottom */
		       (unsigned) width, 0,	/* all across the bottom */
		       False);
#endif
	}
    }

    TRACE(("...computing rows/cols: %.2f %.2f\n",
	   (double) (height - border) / FontHeight(screen),
	   (double) (width - border - ScrollbarWidth(screen)) / FontWidth(screen)));

    rows = (height - border) / FontHeight(screen);
    cols = (width - border - ScrollbarWidth(screen)) / FontWidth(screen);
    if (rows < 1)
	rows = 1;
    if (cols < 1)
	cols = 1;

    /* update buffers if the screen has changed size */
    if (MaxRows(screen) != rows || MaxCols(screen) != cols) {
#if !OPT_SAVE_LINES
	int whichBuf = 0;
#endif
	int delta_rows = rows - MaxRows(screen);
#if OPT_TRACE
	int delta_cols = cols - MaxCols(screen);
#endif

	TRACE(("...ScreenResize chars %dx%d delta %dx%d\n",
	       rows, cols, delta_rows, delta_cols));

	if (screen->is_running) {
#if !OPT_FIFO_LINES
	    int savelines = (screen->scrollWidget
			     ? screen->savelines
			     : 0);
#endif
	    if (screen->cursor_state)
		HideCursor();
#if OPT_SAVE_LINES
	    /*
	     * The non-visible buffer is simple, since we will not copy data
	     * to/from the saved-lines.  Do that first.
	     */
	    if (screen->editBuf_index[!screen->whichBuf]) {
		(void) Reallocate(xw,
				  &screen->editBuf_index[!screen->whichBuf],
				  &screen->editBuf_data[!screen->whichBuf],
				  (unsigned) rows,
				  (unsigned) cols,
				  (unsigned) MaxRows(screen));
	    }

	    /*
	     * The save-lines buffer may change width, but will not change its
	     * height.  Deal with the cases where we copy data to/from the
	     * saved-lines buffer.
	     */
	    if (GravityIsSouthWest(xw)
		&& delta_rows
		&& screen->saveBuf_index != 0) {

		if (delta_rows < 0) {
		    unsigned move_up = (unsigned) (-delta_rows);
		    ScrnBuf dst = screen->saveBuf_index;

#if OPT_FIFO_LINES
		    int amount = ((MaxRows(screen) - (int) move_up - 1)
				  - screen->cur_row);

		    if (amount < 0) {
			/* move line-data from visible-buffer to save-buffer */
			saveEditBufLines(screen, dst, (unsigned) -amount);
			move_down_by = amount;
		    } else {
			move_down_by = 0;
		    }
#else /* !OPT_FIFO_LINES */
		    int amount = screen->savelines - (int) move_up;

		    TRACE_SCRNBUF("before save", screen, dst, screen->savelines);

		    /* shift lines in save-buffer to make room */
		    TRACE(("...%smoving pointers in saveBuf (compare %d %d)\n",
			   (amount > 0
			    ? ""
			    : "SKIP "),
			   screen->savelines,
			   move_up));
		    if (amount > 0) {
			SaveLineData(dst, 0, move_up);

			MoveLineData(dst,
				     0,
				     move_up,
				     (unsigned) amount);

			TRACE(("...reuse %d lines storage in saveBuf\n", move_up));
			RestoreLineData(dst,
					(unsigned) amount,
					move_up);
			TRACE_SCRNBUF("restoresave", screen, dst, screen->savelines);
		    }

		    /* copy line-data from visible-buffer to save-buffer */
		    saveEditBufLines(screen, dst, move_up);

		    /* after data is copied, reallocate saved-lines */
		    (void) Reallocate(xw,
				      &screen->saveBuf_index,
				      &screen->saveBuf_data,
				      (unsigned) savelines,
				      (unsigned) cols,
				      (unsigned) savelines);
		    TRACE_SCRNBUF("reallocSAVE",
				  screen,
				  screen->saveBuf_index,
				  savelines);
#endif /* OPT_FIFO_LINES */

		    /* decrease size of visible-buffer */
		    (void) Reallocate(xw,
				      &screen->editBuf_index[screen->whichBuf],
				      &screen->editBuf_data[screen->whichBuf],
				      (unsigned) rows,
				      (unsigned) cols,
				      (unsigned) MaxRows(screen));
		    TRACE_SCRNBUF("reallocEDIT",
				  screen,
				  screen->editBuf_index[screen->whichBuf],
				  rows);
		} else {
		    unsigned move_down = (unsigned) delta_rows;
#if OPT_FIFO_LINES
		    long unsave_fifo;
#else
		    ScrnBuf src = screen->saveBuf_index;
#endif
		    ScrnBuf dst;
		    int amount;

		    if ((int) move_down > screen->savedlines) {
			move_down = (unsigned) screen->savedlines;
		    }
		    move_down_by = (int) move_down;
		    amount = rows - (int) move_down;

		    /* increase size of visible-buffer */
		    (void) Reallocate(xw,
				      &screen->editBuf_index[screen->whichBuf],
				      &screen->editBuf_data[screen->whichBuf],
				      (unsigned) rows,
				      (unsigned) cols,
				      (unsigned) MaxRows(screen));

		    dst = screen->editBuf_index[screen->whichBuf];
		    TRACE_SCRNBUF("reallocEDIT", screen, dst, rows);

		    TRACE(("...%smoving pointers in editBuf (compare %d %d)\n",
			   (amount > 0
			    ? ""
			    : "SKIP "),
			   rows,
			   move_down));
		    if (amount > 0) {
			/* shift lines in visible-buffer to make room */
			SaveLineData(dst, (unsigned) amount, (size_t) move_down);

			MoveLineData(dst,
				     move_down,
				     0,
				     (unsigned) amount);

			TRACE(("...reuse %d lines storage in editBuf\n", move_down));
			RestoreLineData(dst,
					0,
					move_down);

			TRACE_SCRNBUF("shifted", screen, dst, rows);
		    }

		    /* copy line-data from save-buffer to visible-buffer */
		    unsaveEditBufLines(screen, dst, move_down);
		    TRACE_SCRNBUF("copied", screen, dst, rows);

#if OPT_FIFO_LINES
		    unsave_fifo = (long) move_down;
		    if (screen->saved_fifo < (int) unsave_fifo)
			unsave_fifo = screen->saved_fifo;

		    /* free up storage in fifo from the copied lines */
		    while (unsave_fifo-- > 0) {
			deleteScrollback(screen);
		    }
#else
		    amount = (screen->savelines - (int) move_down);
		    TRACE(("...%smoving pointers in saveBuf (compare %d %d)\n",
			   (amount > 0
			    ? ""
			    : "SKIP "),
			   rows,
			   move_down));
		    if (amount > 0) {
			/* shift lines in save-buffer to account for copy */
			src = screen->saveBuf_index;
			SaveLineData(src, amount, move_down);

			MoveLineData(src,
				     move_down,
				     0,
				     (unsigned) amount);

			TRACE(("...reuse %d lines storage in saveBuf\n", move_down));
			RestoreLineData(src,
					0,
					move_down);
		    }
#endif

		    /* recover storage in save-buffer */
		}
	    } else {
#if !OPT_FIFO_LINES
		(void) Reallocate(xw,
				  &screen->saveBuf_index,
				  &screen->saveBuf_data,
				  (unsigned) savelines,
				  (unsigned) cols,
				  (unsigned) savelines);
#endif
		(void) Reallocate(xw,
				  &screen->editBuf_index[screen->whichBuf],
				  &screen->editBuf_data[screen->whichBuf],
				  (unsigned) rows,
				  (unsigned) cols,
				  (unsigned) MaxRows(screen));
	    }
#else /* !OPT_SAVE_LINES */
	    if (screen->whichBuf
		&& GravityIsSouthWest(xw)) {
		/* swap buffer pointers back to make this work */
		whichBuf = screen->whichBuf;
		SwitchBufPtrs(screen, 0);
	    } else {
		whichBuf = 0;
	    }
	    if (screen->editBuf_index[1])
		(void) Reallocate(xw,
				  &screen->editBuf_index[1],
				  &screen->editBuf_data[1],
				  (unsigned) rows,
				  (unsigned) cols,
				  (unsigned) MaxRows(screen));
	    move_down_by = Reallocate(xw,
				      &screen->saveBuf_index,
				      &screen->saveBuf_data,
				      (unsigned) (rows + savelines),
				      (unsigned) cols,
				      (unsigned) (MaxRows(screen) + savelines));
#endif /* OPT_SAVE_LINES */
	    screen->visbuf = VisBuf(screen);
	}

	AdjustSavedCursor(xw, move_down_by);
	set_max_row(screen, screen->max_row + delta_rows);
	set_max_col(screen, cols - 1);

	if (screen->is_running) {
	    if (GravityIsSouthWest(xw)) {
		screen->savedlines -= move_down_by;
		if (screen->savedlines < 0)
		    screen->savedlines = 0;
		if (screen->savedlines > screen->savelines)
		    screen->savedlines = screen->savelines;
		if (screen->topline < -screen->savedlines)
		    screen->topline = -screen->savedlines;
		set_cur_row(screen, screen->cur_row + move_down_by);
		screen->cursorp.row += move_down_by;
		ScrollSelection(screen, move_down_by, True);

#if !OPT_SAVE_LINES
		if (whichBuf)
		    SwitchBufPtrs(screen, whichBuf);	/* put the pointers back */
#endif
	    }
	}

	/* adjust scrolling region */
	set_tb_margins(screen, 0, screen->max_row);
	set_lr_margins(screen, 0, screen->max_col);
	UIntClr(*flags, ORIGIN);

	if (screen->cur_row > screen->max_row)
	    set_cur_row(screen, screen->max_row);
	if (screen->cur_col > screen->max_col)
	    set_cur_col(screen, screen->max_col);

	screen->fullVwin.height = height - border;
	screen->fullVwin.width = width - border - screen->fullVwin.sb_info.width;

	scroll_displayed_graphics(xw, -move_down_by);
    } else if (FullHeight(screen) == height && FullWidth(screen) == width)
	return (0);		/* nothing has changed at all */

    screen->fullVwin.fullheight = (Dimension) height;
    screen->fullVwin.fullwidth = (Dimension) width;

    ResizeScrollBar(xw);
    ResizeSelection(screen, rows, cols);

#ifndef NO_ACTIVE_ICON
    if (screen->iconVwin.window) {
	XWindowChanges changes;
	screen->iconVwin.width =
	    MaxCols(screen) * screen->iconVwin.f_width;

	screen->iconVwin.height =
	    MaxRows(screen) * screen->iconVwin.f_height;

	changes.width = screen->iconVwin.fullwidth =
	    (Dimension) ((unsigned) screen->iconVwin.width
			 + 2 * xw->misc.icon_border_width);

	changes.height = screen->iconVwin.fullheight =
	    (Dimension) ((unsigned) screen->iconVwin.height
			 + 2 * xw->misc.icon_border_width);

	changes.border_width = (int) xw->misc.icon_border_width;

	TRACE(("resizing icon window %dx%d\n", changes.height, changes.width));
	XConfigureWindow(XtDisplay(xw), screen->iconVwin.window,
			 CWWidth | CWHeight | CWBorderWidth, &changes);
    }
#endif /* NO_ACTIVE_ICON */

#ifdef TTYSIZE_STRUCT
    update_winsize(screen->respond, rows, cols, height, width);

#if defined(SIGWINCH) && defined(TIOCGPGRP)
    if (screen->pid > 1) {
	int pgrp;

	TRACE(("getting process-group\n"));
	if (ioctl(screen->respond, TIOCGPGRP, &pgrp) != -1) {
	    TRACE(("sending SIGWINCH to process group %d\n", pgrp));
	    kill_process_group(pgrp, SIGWINCH);
	}
    }
#endif /* SIGWINCH */

#else
    TRACE(("ScreenResize cannot do anything to pty\n"));
#endif /* TTYSIZE_STRUCT */
    return (0);
}

/*
 * Return true if any character cell starting at [row,col], for len-cells is
 * nonnull.
 */
Bool
non_blank_line(TScreen *screen,
	       int row,
	       int col,
	       int len)
{
    int i;
    Bool found = False;
    LineData *ld = getLineData(screen, row);

    if (ld != 0) {
	for (i = col; i < len; i++) {
	    if (ld->charData[i]) {
		found = True;
		break;
	    }
	}
    }
    return found;
}

/*
 * Limit/map rectangle parameters.
 */
#define minRectRow(screen) (getMinRow(screen) + 1)
#define minRectCol(screen) (getMinCol(screen) + 1)
#define maxRectRow(screen) (getMaxRow(screen) + 1)
#define maxRectCol(screen) (getMaxCol(screen) + 1)

static int
limitedParseRow(XtermWidget xw, int row)
{
    TScreen *screen = TScreenOf(xw);
    int min_row = minRectRow(screen);
    int max_row = maxRectRow(screen);

    if (xw->flags & ORIGIN)
	row += screen->top_marg;

    if (row < min_row)
	row = min_row;
    else if (row > max_row)
	row = max_row;

    return row;
}

static int
limitedParseCol(XtermWidget xw, int col)
{
    TScreen *screen = TScreenOf(xw);
    int min_col = minRectCol(screen);
    int max_col = maxRectCol(screen);

    if (xw->flags & ORIGIN)
	col += screen->lft_marg;

    if (col < min_col)
	col = min_col;
    else if (col > max_col)
	col = max_col;

    return col;
}

#define LimitedParse(num, func, dft) \
	func(xw, (nparams > num) ? params[num] : dft)

/*
 * Copy the rectangle boundaries into a struct, providing default values as
 * needed.
 */
void
xtermParseRect(XtermWidget xw, int nparams, int *params, XTermRect *target)
{
    TScreen *screen = TScreenOf(xw);

    memset(target, 0, sizeof(*target));
    target->top = LimitedParse(0, limitedParseRow, minRectRow(screen));
    target->left = LimitedParse(1, limitedParseCol, minRectCol(screen));
    target->bottom = LimitedParse(2, limitedParseRow, maxRectRow(screen));
    target->right = LimitedParse(3, limitedParseCol, maxRectCol(screen));
    TRACE(("parsed rectangle %d,%d %d,%d\n",
	   target->top,
	   target->left,
	   target->bottom,
	   target->right));
}

static Bool
validRect(XtermWidget xw, XTermRect *target)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("comparing against screensize %dx%d\n",
	   maxRectRow(screen),
	   maxRectCol(screen)));
    return (target != 0
	    && target->top >= minRectRow(screen)
	    && target->left >= minRectCol(screen)
	    && target->top <= target->bottom
	    && target->left <= target->right
	    && target->top <= maxRectRow(screen)
	    && target->right <= maxRectCol(screen));
}

/*
 * Fills a rectangle with the given 8-bit character and video-attributes.
 * Colors and double-size attribute are unmodified.
 */
void
ScrnFillRectangle(XtermWidget xw,
		  XTermRect *target,
		  int value,
		  unsigned flags,
		  Bool keepColors)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("filling rectangle with '%c' flags %#x\n", value, flags));
    if (validRect(xw, target)) {
	LineData *ld;
	unsigned left = (unsigned) (target->left - 1);
	unsigned size = (unsigned) (target->right - (int) left);
	unsigned attrs = flags;
	int row, col;

	(void) size;

	attrs &= ATTRIBUTES;
	attrs |= CHARDRAWN;
	for (row = target->bottom - 1; row >= (target->top - 1); row--) {
	    ld = getLineData(screen, row);

	    TRACE(("filling %d [%d..%d]\n", row, left, left + size));

	    /*
	     * Fill attributes, preserving colors.
	     */
	    for (col = (int) left; col < target->right; ++col) {
		unsigned temp = ld->attribs[col];

		if (!keepColors) {
		    UIntClr(temp, (FG_COLOR | BG_COLOR));
		}
		temp = attrs | (temp & (FG_COLOR | BG_COLOR)) | CHARDRAWN;
		ld->attribs[col] = (IAttr) temp;
		if_OPT_ISO_COLORS(screen, {
		    if (attrs & (FG_COLOR | BG_COLOR)) {
			ld->color[col] = xtermColorPair(xw);
		    }
		});
	    }

	    for (col = (int) left; col < target->right; ++col)
		ld->charData[col] = (CharData) value;

	    if_OPT_WIDE_CHARS(screen, {
		size_t off;
		for_each_combData(off, ld) {
		    memset(ld->combData[off] + left, 0, size * sizeof(CharData));
		}
	    })
	}
	ScrnUpdate(xw,
		   target->top - 1,
		   target->left - 1,
		   (target->bottom - target->top) + 1,
		   (target->right - target->left) + 1,
		   False);
    }
}

#if OPT_DEC_RECTOPS
/*
 * Copies the source rectangle to the target location, including video
 * attributes.
 *
 * This implementation ignores page numbers.
 *
 * The reference manual does not indicate if it handles overlapping copy
 * properly - so we make a local copy of the source rectangle first, then apply
 * the target from that.
 */
void
ScrnCopyRectangle(XtermWidget xw, XTermRect *source, int nparam, int *params)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("copying rectangle\n"));

    if (nparam > 4)
	nparam = 4;

    if (validRect(xw, source)) {
	XTermRect target;
	xtermParseRect(xw,
		       ((nparam > 3) ? 2 : (nparam - 1)),
		       params,
		       &target);
	if (validRect(xw, &target)) {
	    Cardinal high = (Cardinal) (source->bottom - source->top) + 1;
	    Cardinal wide = (Cardinal) (source->right - source->left) + 1;
	    Cardinal size = (high * wide);
	    int row, col;
	    Cardinal j, k;
	    LineData *ld;

	    CellData *cells = newCellData(xw, size);

	    if (cells != 0) {

		TRACE(("OK - make copy %dx%d\n", high, wide));
		target.bottom = target.top + (int) (high - 1);
		target.right = target.left + (int) (wide - 1);

		for (row = source->top - 1; row < source->bottom; ++row) {
		    ld = getLineData(screen, row);
		    if (ld == 0)
			continue;
		    j = (Cardinal) (row - (source->top - 1));
		    for (col = source->left - 1; col < source->right; ++col) {
			k = (Cardinal) (col - (source->left - 1));
			saveCellData(screen, cells,
				     (j * wide) + k,
				     ld, col);
		    }
		}
		for (row = target.top - 1; row < target.bottom; ++row) {
		    ld = getLineData(screen, row);
		    if (ld == 0)
			continue;
		    j = (Cardinal) (row - (target.top - 1));
		    for (col = target.left - 1; col < target.right; ++col) {
			k = (Cardinal) (col - (target.left - 1));
			if (row >= getMinRow(screen)
			    && row <= getMaxRow(screen)
			    && col >= getMinCol(screen)
			    && col <= getMaxCol(screen)) {
			    if (j < high && k < wide) {
				restoreCellData(screen, cells,
						(j * wide) + k,
						ld, col);
			    } else {
				/* EMPTY */
				/* FIXME - clear the target cell? */
			    }
			    ld->attribs[col] |= CHARDRAWN;
			}
		    }
#if OPT_BLINK_TEXT
		    if (LineHasBlinking(screen, ld)) {
			LineSetBlinked(ld);
		    } else {
			LineClrBlinked(ld);
		    }
#endif
		}
		free(cells);

		ScrnUpdate(xw,
			   (target.top - 1),
			   (target.left - 1),
			   (target.bottom - target.top) + 1,
			   ((target.right - target.left) + 1),
			   False);
	    }
	}
    }
}

/*
 * Modifies the video-attributes only - so selection (not a video attribute) is
 * unaffected.  Colors and double-size flags are unaffected as well.
 *
 * FIXME: our representation for "invisible" does not work with this operation,
 * since the attribute byte is fully-allocated for other flags.  The logic
 * is shown for INVISIBLE because it's harmless, and useful in case the
 * CHARDRAWN or PROTECTED flags are reassigned.
 */
void
ScrnMarkRectangle(XtermWidget xw,
		  XTermRect *target,
		  Bool reverse,
		  int nparam,
		  int *params)
{
    TScreen *screen = TScreenOf(xw);
    Bool exact = (screen->cur_decsace == 2);

    TRACE(("%s %s\n",
	   reverse ? "reversing" : "marking",
	   (exact
	    ? "rectangle"
	    : "region")));

    if (validRect(xw, target)) {
	LineData *ld;
	int top = target->top - 1;
	int bottom = target->bottom - 1;
	int row, col;
	int n;

	for (row = top; row <= bottom; ++row) {
	    int left = ((exact || (row == top))
			? (target->left - 1)
			: getMinCol(screen));
	    int right = ((exact || (row == bottom))
			 ? (target->right - 1)
			 : getMaxCol(screen));

	    ld = getLineData(screen, row);

	    TRACE(("marking %d [%d..%d]\n", row, left, right));
	    for (col = left; col <= right; ++col) {
		unsigned flags = ld->attribs[col];

		for (n = 0; n < nparam; ++n) {
#if OPT_TRACE
		    if (row == top && col == left)
			TRACE(("attr param[%d] %d\n", n + 1, params[n]));
#endif
		    if (reverse) {
			switch (params[n]) {
			case 1:
			    flags ^= BOLD;
			    break;
			case 4:
			    flags ^= UNDERLINE;
			    break;
			case 5:
			    flags ^= BLINK;
			    break;
			case 7:
			    flags ^= INVERSE;
			    break;
			case 8:
			    flags ^= INVISIBLE;
			    break;
			}
		    } else {
			switch (params[n]) {
			case 0:
			    UIntClr(flags, SGR_MASK);
			    break;
			case 1:
			    flags |= BOLD;
			    break;
			case 4:
			    flags |= UNDERLINE;
			    break;
			case 5:
			    flags |= BLINK;
			    break;
			case 7:
			    flags |= INVERSE;
			    break;
			case 8:
			    flags |= INVISIBLE;
			    break;
			case 22:
			    UIntClr(flags, BOLD);
			    break;
			case 24:
			    UIntClr(flags, UNDERLINE);
			    break;
			case 25:
			    UIntClr(flags, BLINK);
			    break;
			case 27:
			    UIntClr(flags, INVERSE);
			    break;
			case 28:
			    UIntClr(flags, INVISIBLE);
			    break;
			}
		    }
		}
#if OPT_TRACE
		if (row == top && col == left)
		    TRACE(("first mask-change is %#x\n",
			   ld->attribs[col] ^ flags));
#endif
		ld->attribs[col] = (IAttr) flags;
	    }
	}
	ScrnRefresh(xw,
		    (target->top - 1),
		    (exact ? (target->left - 1) : getMinCol(screen)),
		    (target->bottom - target->top) + 1,
		    (exact
		     ? ((target->right - target->left) + 1)
		     : (getMaxCol(screen) - getMinCol(screen) + 1)),
		    False);
    }
}

/*
 * Resets characters to space, except where prohibited by DECSCA.  Video
 * attributes (including color) are untouched.
 */
void
ScrnWipeRectangle(XtermWidget xw,
		  XTermRect *target)
{
    TScreen *screen = TScreenOf(xw);

    TRACE(("wiping rectangle\n"));

    if (validRect(xw, target)) {
	LineData *ld;
	int top = target->top - 1;
	int bottom = target->bottom - 1;
	int row, col;

	for (row = top; row <= bottom; ++row) {
	    int left = (target->left - 1);
	    int right = (target->right - 1);

	    TRACE(("wiping %d [%d..%d]\n", row, left, right));

	    ld = getLineData(screen, row);
	    for (col = left; col <= right; ++col) {
		if (!((screen->protected_mode == DEC_PROTECT)
		      && (ld->attribs[col] & PROTECTED))) {
		    ld->attribs[col] |= CHARDRAWN;
		    ld->charData[col] = ' ';
		    if_OPT_WIDE_CHARS(screen, {
			size_t off;
			for_each_combData(off, ld) {
			    ld->combData[off][col] = '\0';
			}
		    })
		}
	    }
	}
	ScrnUpdate(xw,
		   (target->top - 1),
		   (target->left - 1),
		   (target->bottom - target->top) + 1,
		   ((target->right - target->left) + 1),
		   False);
    }
}

/*
 * Compute a checksum, ignoring the page number (since we have only one page).
 */
void
xtermCheckRect(XtermWidget xw,
	       int nparam,
	       int *params,
	       int *result)
{
    TScreen *screen = TScreenOf(xw);
    XTermRect target;
    LineData *ld;

    *result = 0;
    if (nparam > 2) {
	nparam -= 2;
	params += 2;
    }
    xtermParseRect(xw, nparam, params, &target);
    if (validRect(xw, &target)) {
	int top = target.top - 1;
	int bottom = target.bottom - 1;
	int row, col;

	for (row = top; row <= bottom; ++row) {
	    int left = (target.left - 1);
	    int right = (target.right - 1);

	    ld = getLineData(screen, row);
	    for (col = left; col <= right; ++col) {
		if (ld->attribs[col] & CHARDRAWN) {
		    *result += (int) ld->charData[col];
		    if_OPT_WIDE_CHARS(screen, {
			size_t off;
			for_each_combData(off, ld) {
			    *result += (int) ld->combData[off][col];
			}
		    })
		}
	    }
	}
    }
}
#endif /* OPT_DEC_RECTOPS */

#if OPT_MAXIMIZE

static _Xconst char *
ewmhProperty(int mode)
{
    _Xconst char *result;
    switch (mode) {
    default:
	result = 0;
	break;
    case 1:
	result = "_NET_WM_STATE_FULLSCREEN";
	break;
    case 2:
	result = "_NET_WM_STATE_MAXIMIZED_VERT";
	break;
    case 3:
	result = "_NET_WM_STATE_MAXIMIZED_HORZ";
	break;
    }
    return result;
}

static void
set_resize_increments(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    int min_width = (2 * screen->border) + screen->fullVwin.sb_info.width;
    int min_height = (2 * screen->border);
    XSizeHints sizehints;

    memset(&sizehints, 0, sizeof(XSizeHints));
    sizehints.width_inc = FontWidth(screen);
    sizehints.height_inc = FontHeight(screen);
    sizehints.flags = PResizeInc;
    XSetWMNormalHints(screen->display, VShellWindow(xw), &sizehints);

    XtVaSetValues(SHELL_OF(xw),
		  XtNbaseWidth, min_width,
		  XtNbaseHeight, min_height,
		  XtNminWidth, min_width + FontWidth(screen),
		  XtNminHeight, min_height + FontHeight(screen),
		  XtNwidthInc, FontWidth(screen),
		  XtNheightInc, FontHeight(screen),
		  (XtPointer) 0);

    XFlush(XtDisplay(xw));
}

static void
unset_resize_increments(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    XSizeHints sizehints;

    memset(&sizehints, 0, sizeof(XSizeHints));
    sizehints.width_inc = 1;
    sizehints.height_inc = 1;
    sizehints.flags = PResizeInc;
    XSetWMNormalHints(screen->display, VShellWindow(xw), &sizehints);

    XtVaSetValues(SHELL_OF(xw),
		  XtNwidthInc, 1,
		  XtNheightInc, 1,
		  (XtPointer) 0);

    XFlush(XtDisplay(xw));
}

static void
set_ewmh_hint(Display *dpy, Window window, int operation, _Xconst char *prop)
{
    XEvent e;
    Atom atom_fullscreen = XInternAtom(dpy, prop, False);
    Atom atom_state = XInternAtom(dpy, "_NET_WM_STATE", False);

    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.message_type = atom_state;
    e.xclient.display = dpy;
    e.xclient.window = window;
    e.xclient.format = 32;
    e.xclient.data.l[0] = operation;
    e.xclient.data.l[1] = (long) atom_fullscreen;

    XSendEvent(dpy, DefaultRootWindow(dpy), False,
	       SubstructureRedirectMask, &e);
}

/*
 * Check if the given property is supported on the root window.
 *
 * The XGetWindowProperty function returns a list of Atom's which corresponds
 * to the output of xprop.  The actual list (ignore the manpage, which refers
 * to an array of 32-bit values) is constructed by _XRead32, which uses long
 * as a datatype.
 *
 * Alternatively, we could check _NET_WM_ALLOWED_ACTIONS on the application's
 * window.
 */
static Boolean
probe_netwm(Display *dpy, _Xconst char *propname)
{
    Atom atom_fullscreen = XInternAtom(dpy, propname, False);
    Atom atom_supported = XInternAtom(dpy, "_NET_SUPPORTED", False);
    Atom actual_type;
    int actual_format;
    long long_offset = 0;
    long long_length = 128;	/* number of items to ask for at a time */
    unsigned int i;
    unsigned long nitems, bytes_after;
    unsigned char *args;
    long *ldata;
    Boolean has_capability = False;
    Boolean rc;

    while (!has_capability) {
	rc = xtermGetWinProp(dpy,
			     DefaultRootWindow(dpy),
			     atom_supported,
			     long_offset,
			     long_length,
			     AnyPropertyType,	/* req_type */
			     &actual_type,	/* actual_type_return */
			     &actual_format,	/* actual_format_return */
			     &nitems,	/* nitems_return */
			     &bytes_after,	/* bytes_after_return */
			     &args	/* prop_return */
	    );
	if (!rc
	    || actual_type != XA_ATOM) {
	    break;
	}
	ldata = (long *) (void *) args;
	for (i = 0; i < nitems; i++) {
	    if ((Atom) ldata[i] == atom_fullscreen) {
		has_capability = True;
		break;
	    }
	}
	XFree(ldata);

	if (!has_capability) {
	    if (bytes_after != 0) {
		long remaining = (long) (bytes_after / sizeof(long));
		if (long_length > remaining)
		    long_length = remaining;
		long_offset += (long) nitems;
	    } else {
		break;
	    }
	}
    }

    TRACE(("probe_netwm(%s) ->%d\n", propname, has_capability));
    return has_capability;
}

/*
 * Alter fullscreen mode for the xterm widget, if the window manager supports
 * that feature.
 */
void
FullScreen(XtermWidget xw, int new_ewmh_mode)
{
    TScreen *screen = TScreenOf(xw);
    Display *dpy = screen->display;
    _Xconst char *oldprop = ewmhProperty(xw->work.ewmh[0].mode);
    _Xconst char *newprop = ewmhProperty(new_ewmh_mode);

    int which = 0;
    Window window;

#if OPT_TEK4014
    if (TEK4014_ACTIVE(xw)) {
	which = 1;
	window = TShellWindow;
    } else
#endif
	window = VShellWindow(xw);

    TRACE(("FullScreen %d:%s\n", new_ewmh_mode, BtoS(new_ewmh_mode)));

    if (new_ewmh_mode < 0 || new_ewmh_mode >= MAX_EWMH_MODE) {
	TRACE(("BUG: FullScreen %d\n", new_ewmh_mode));
	return;
    } else if (new_ewmh_mode == 0) {
	xw->work.ewmh[which].checked[new_ewmh_mode] = True;
	xw->work.ewmh[which].allowed[new_ewmh_mode] = True;
    } else if (resource.fullscreen == esNever) {
	xw->work.ewmh[which].checked[new_ewmh_mode] = True;
	xw->work.ewmh[which].allowed[new_ewmh_mode] = False;
    } else if (!xw->work.ewmh[which].checked[new_ewmh_mode]) {
	xw->work.ewmh[which].checked[new_ewmh_mode] = True;
	xw->work.ewmh[which].allowed[new_ewmh_mode] = probe_netwm(dpy, newprop);
    }

    if (xw->work.ewmh[which].allowed[new_ewmh_mode]) {
	if (new_ewmh_mode && !xw->work.ewmh[which].mode) {
	    unset_resize_increments(xw);
	    set_ewmh_hint(dpy, window, _NET_WM_STATE_ADD, newprop);
	} else if (xw->work.ewmh[which].mode && !new_ewmh_mode) {
	    set_resize_increments(xw);
	    set_ewmh_hint(dpy, window, _NET_WM_STATE_REMOVE, oldprop);
	} else {
	    set_ewmh_hint(dpy, window, _NET_WM_STATE_REMOVE, oldprop);
	    set_ewmh_hint(dpy, window, _NET_WM_STATE_ADD, newprop);
	}
	xw->work.ewmh[which].mode = new_ewmh_mode;
	update_fullscreen();
    } else {
	Bell(xw, XkbBI_MinorError, 100);
    }
}
#endif /* OPT_MAXIMIZE */
