/* $XTermId: cursor.c,v 1.71 2017/05/06 00:58:27 tom Exp $ */

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

/* cursor.c */

#include <xterm.h>
#include <data.h>
#include <menu.h>

#include <assert.h>

/*
 * Moves the cursor to the specified position, checking for bounds.
 * (this includes scrolling regions)
 * The origin is considered to be 0, 0 for this procedure.
 */
void
CursorSet(TScreen *screen, int row, int col, unsigned flags)
{
    int use_row = row;
    int use_col = col;
    int max_col = screen->max_col;
    int max_row = screen->max_row;

    if (flags & ORIGIN) {
	use_col += screen->lft_marg;
	max_col = screen->rgt_marg;
    }
    use_col = (use_col < 0 ? 0 : use_col);
    set_cur_col(screen, (use_col <= max_col ? use_col : max_col));

    if (flags & ORIGIN) {
	use_row += screen->top_marg;
	max_row = screen->bot_marg;
    }
    use_row = (use_row < 0 ? 0 : use_row);
    set_cur_row(screen, (use_row <= max_row ? use_row : max_row));

    ResetWrap(screen);

    TRACE(("CursorSet(%d,%d) margins V[%d..%d] H[%d..%d] -> %d,%d %s\n",
	   row, col,
	   screen->top_marg,
	   screen->bot_marg,
	   screen->lft_marg,
	   screen->rgt_marg,
	   screen->cur_row,
	   screen->cur_col,
	   ((flags & ORIGIN) ? "origin" : "normal")));
}

/*
 * moves the cursor left n, no wrap around
 */
void
CursorBack(XtermWidget xw, int n)
{
#define WRAP_MASK (REVERSEWRAP | WRAPAROUND)
    TScreen *screen = TScreenOf(xw);
    int rev;
    int left = ScrnLeftMargin(xw);
    int before = screen->cur_col;

    if ((rev = ((xw->flags & WRAP_MASK) == WRAP_MASK)) != 0
	&& screen->do_wrap) {
	n--;
    }

    /* if the cursor is already before the left-margin, we have to let it go */
    if (before < left)
	left = 0;

    if ((screen->cur_col -= n) < left) {
	if (rev) {
	    int in_row = ScrnRightMargin(xw) - left + 1;
	    int offset = (in_row * screen->cur_row) + screen->cur_col - left;
	    if (offset < 0) {
		int length = in_row * MaxRows(screen);
		offset += ((-offset) / length + 1) * length;
	    }
	    set_cur_row(screen, (offset / in_row));
	    set_cur_col(screen, (offset % in_row) + left);
	    do_xevents();
	} else {
	    set_cur_col(screen, left);
	}
    }
    ResetWrap(screen);
}

/*
 * moves the cursor forward n, no wraparound
 */
void
CursorForward(XtermWidget xw, int n)
{
    TScreen *screen = TScreenOf(xw);
#if OPT_DEC_CHRSET
    LineData *ld = getLineData(screen, screen->cur_row);
#endif
    int next = screen->cur_col + n;
    int max;

    if (IsLeftRightMode(xw)) {
	max = screen->rgt_marg;
	if (screen->cur_col > max)
	    max = screen->max_col;
    } else {
	max = LineMaxCol(screen, ld);
    }

    if (next > max)
	next = max;

    set_cur_col(screen, next);
    ResetWrap(screen);
}

/*
 * moves the cursor down n, no scrolling.
 * Won't pass bottom margin or bottom of screen.
 */
void
CursorDown(TScreen *screen, int n)
{
    int max;
    int next = screen->cur_row + n;

    max = (screen->cur_row > screen->bot_marg ?
	   screen->max_row : screen->bot_marg);
    if (next > max)
	next = max;
    if (next > screen->max_row)
	next = screen->max_row;

    set_cur_row(screen, next);
    ResetWrap(screen);
}

/*
 * moves the cursor up n, no linestarving.
 * Won't pass top margin or top of screen.
 */
void
CursorUp(TScreen *screen, int n)
{
    int min;
    int next = screen->cur_row - n;

    min = ((screen->cur_row < screen->top_marg)
	   ? 0
	   : screen->top_marg);
    if (next < min)
	next = min;
    if (next < 0)
	next = 0;

    set_cur_row(screen, next);
    ResetWrap(screen);
}

/*
 * Moves cursor down amount lines, scrolls if necessary.
 * Won't leave scrolling region. No carriage return.
 */
void
xtermIndex(XtermWidget xw, int amount)
{
    TScreen *screen = TScreenOf(xw);

    /*
     * indexing when below scrolling region is cursor down.
     * if cursor high enough, no scrolling necessary.
     */
    if (screen->cur_row > screen->bot_marg
	|| screen->cur_row + amount <= screen->bot_marg
	|| (IsLeftRightMode(xw)
	    && !ScrnIsColInMargins(screen, screen->cur_col))) {
	CursorDown(screen, amount);
    } else {
	int j;
	CursorDown(screen, j = screen->bot_marg - screen->cur_row);
	xtermScroll(xw, amount - j);
    }
}

/*
 * Moves cursor up amount lines, reverse scrolls if necessary.
 * Won't leave scrolling region. No carriage return.
 */
void
RevIndex(XtermWidget xw, int amount)
{
    TScreen *screen = TScreenOf(xw);

    /*
     * reverse indexing when above scrolling region is cursor up.
     * if cursor low enough, no reverse indexing needed
     */
    if (screen->cur_row < screen->top_marg
	|| screen->cur_row - amount >= screen->top_marg
	|| (IsLeftRightMode(xw)
	    && !ScrnIsColInMargins(screen, screen->cur_col))) {
	CursorUp(screen, amount);
    } else {
	RevScroll(xw, amount - (screen->cur_row - screen->top_marg));
	CursorUp(screen, screen->cur_row - screen->top_marg);
    }
}

/*
 * Moves Cursor To First Column In Line
 * (Note: xterm doesn't implement SLH, SLL which would affect use of this)
 */
void
CarriageReturn(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    int left = ScrnLeftMargin(xw);
    int col;

    if (xw->flags & ORIGIN) {
	col = left;
    } else if (screen->cur_col >= left) {
	col = left;
    } else {
	/*
	 * If origin-mode is not active, it is possible to use cursor
	 * addressing outside the margins.  In that case we will go to the
	 * first column rather than following the margin.
	 */
	col = 0;
    }

    set_cur_col(screen, col);
    ResetWrap(screen);
    do_xevents();
}

/*
 * When resizing the window, if we're showing the alternate screen, we still
 * have to adjust the saved cursor from the normal screen to account for
 * shifting of the saved-line region in/out of the viewable window.
 */
void
AdjustSavedCursor(XtermWidget xw, int adjust)
{
    TScreen *screen = TScreenOf(xw);

    if (screen->whichBuf) {
	SavedCursor *sc = &screen->sc[0];

	if (adjust > 0) {
	    TRACE(("AdjustSavedCursor %d -> %d\n", sc->row, sc->row - adjust));
	    sc->row += adjust;
	}
    }
}

/*
 * Save Cursor and Attributes
 */
void
CursorSave(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    SavedCursor *sc = &screen->sc[screen->whichBuf];

    sc->saved = True;
    sc->row = screen->cur_row;
    sc->col = screen->cur_col;
    sc->flags = xw->flags;
    sc->curgl = screen->curgl;
    sc->curgr = screen->curgr;
    sc->wrap_flag = screen->do_wrap;
#if OPT_ISO_COLORS
    sc->cur_foreground = xw->cur_foreground;
    sc->cur_background = xw->cur_background;
    sc->sgr_foreground = xw->sgr_foreground;
#endif
    memmove(sc->gsets, screen->gsets, sizeof(screen->gsets));
}

/*
 * We save/restore all visible attributes, plus wrapping, origin mode, and the
 * selective erase attribute.
 */
#define DECSC_FLAGS (ATTRIBUTES|ORIGIN|PROTECTED)

/*
 * Restore Cursor and Attributes
 */
void
CursorRestore(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    SavedCursor *sc = &screen->sc[screen->whichBuf];

    /* Restore the character sets, unless we never did a save-cursor op.
     * In that case, we'll reset the character sets.
     */
    if (sc->saved) {
	memmove(screen->gsets, sc->gsets, sizeof(screen->gsets));
	screen->curgl = sc->curgl;
	screen->curgr = sc->curgr;
    } else {
	resetCharsets(screen);
    }

    UIntClr(xw->flags, DECSC_FLAGS);
    UIntSet(xw->flags, sc->flags & DECSC_FLAGS);
    CursorSet(screen,
	      ((xw->flags & ORIGIN)
	       ? sc->row - screen->top_marg
	       : sc->row),
	      sc->col, xw->flags);
    screen->do_wrap = sc->wrap_flag;	/* after CursorSet/ResetWrap */

#if OPT_ISO_COLORS
    xw->sgr_foreground = sc->sgr_foreground;
    SGR_Foreground(xw, (xw->flags & FG_COLOR) ? sc->cur_foreground : -1);
    SGR_Background(xw, (xw->flags & BG_COLOR) ? sc->cur_background : -1);
#endif
}

/*
 * Move the cursor to the first column of the n-th next line.
 */
void
CursorNextLine(XtermWidget xw, int count)
{
    TScreen *screen = TScreenOf(xw);

    CursorDown(screen, count < 1 ? 1 : count);
    CarriageReturn(xw);
    do_xevents();
}

/*
 * Move the cursor to the first column of the n-th previous line.
 */
void
CursorPrevLine(XtermWidget xw, int count)
{
    TScreen *screen = TScreenOf(xw);

    CursorUp(screen, count < 1 ? 1 : count);
    CarriageReturn(xw);
    do_xevents();
}

/*
 * Return col/row values which can be passed to CursorSet() preserving the
 * current col/row, e.g., accounting for DECOM.
 */
int
CursorCol(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    int result = screen->cur_col;
    if (xw->flags & ORIGIN) {
	result -= ScrnLeftMargin(xw);
	if (result < 0)
	    result = 0;
    }
    return result;
}

int
CursorRow(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    int result = screen->cur_row;
    if (xw->flags & ORIGIN) {
	result -= screen->top_marg;
	if (result < 0)
	    result = 0;
    }
    return result;
}

#if OPT_TRACE
int
set_cur_row(TScreen *screen, int value)
{
    TRACE(("set_cur_row %d vs %d\n", value, screen ? screen->max_row : -1));

    assert(screen != 0);
    assert(value >= 0);
    assert(value <= screen->max_row);
    screen->cur_row = value;
    return value;
}

int
set_cur_col(TScreen *screen, int value)
{
    TRACE(("set_cur_col %d vs %d\n", value, screen ? screen->max_col : -1));

    assert(screen != 0);
    assert(value >= 0);
    assert(value <= screen->max_col);
    screen->cur_col = value;
    return value;
}
#endif /* OPT_TRACE */
