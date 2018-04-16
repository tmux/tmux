/* $XTermId: testxmc.c,v 1.52 2014/05/11 14:05:38 tom Exp $ */

/*
 * Copyright 1997-2011,2014 by Thomas E. Dickey
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
 * This module provides test support for curses applications that must work
 * with terminals that have the xmc (magic cookie) glitch.  The xmc_glitch
 * resource denotes the number of spaces that are emitted when switching to or
 * from standout (reverse) mode.  Some terminals implement this by storing the
 * attribute controls in the character cell that is skipped.  So if the cell is
 * overwritten by text, then the attribute change in the cell is cancelled,
 * causing attributes to the left of the change to propagate.
 *
 * We implement the glitch by writing a character that won't be mistaken for
 * other normal characters (and mapping normal writes to that character to a
 * different one).
 *
 * Since xmc isn't normally part of xterm, we document it here rather than in
 * the man-page.  This module is driven by resources rather than by the
 * termcap/terminfo description to make it a little more flexible for testing
 * purposes.
 *
 * Resources:
 *
 * xmcGlitch (class XmcGlitch)
 *	When true, enables this extension.  The default is `0', which disables
 *	the module.  (termcap sg, terminfo xmc).
 *
 * xmcAttributes (class XmcAttributes)
 *	The attributes for which we'll generate a glitch, as a bitmask.
 *
 *		INVERSE		1
 *		UNDERLINE	2
 *		BOLD		4
 *		BLINK		8
 *
 *	The default is `1' (INVERSE).  Some terminals emit glitches for
 *	underline.  Just for completeness, we recognize all of the video
 *	attributes.
 *
 * xmcInline (class XmcInline)
 *	When true, limits the extent of an SGR change to the current line.
 *	The default is `false'.  (No termcap or terminfo equivalent, though
 *	there are comments in some entries relating to this issue).
 *
 * xmcMoveSGR (class XmcMoveSGR)
 *	When false, a cursor movement will leave a glitch when SGR's are
 *	active.  The default is `true'.  (termcap ms, terminfo msgr).
 *
 * TODO:
 *	When xmc is active, the terminfo max_attributes (ma) capability is
 *	assumed to be 1.
 *
 *	The xmcAttributes resource should also apply to alternate character
 *	sets and to color.
 */

#include <xterm.h>
#include <data.h>

#define MARK_ON(a)  (Bool) ((my_attrs & a) != 0 && (xw->flags & (whichone = CharOf(a))) == 0)
#define MARK_OFF(a) (Bool) ((my_attrs & a) != 0 && (xw->flags & (whichone = CharOf(a))) != 0)

void
Mark_XMC(XtermWidget xw, int param)
{
    static IChar *glitch;

    TScreen *screen = TScreenOf(xw);
    Bool found = False;
    unsigned my_attrs = CharOf(screen->xmc_attributes & XMC_FLAGS);
    unsigned whichone = 0;

    if (glitch == 0) {
	unsigned len = screen->xmc_glitch;
	glitch = TypeMallocN(IChar, len);
	while (len--)
	    glitch[len] = XMC_GLITCH;
    }
    switch (param) {
    case -1:			/* DEFAULT */
    case 0:			/* FALLTHRU */
	found = MARK_OFF((xw->flags & XMC_FLAGS));
	break;
    case 1:
	found = MARK_ON(BOLD);
	break;
    case 4:
	found = MARK_ON(UNDERLINE);
	break;
    case 5:
	found = MARK_ON(BLINK);
	break;
    case 7:
	found = MARK_ON(INVERSE);
	break;
    case 22:
	found = MARK_OFF(BOLD);
	break;
    case 24:
	found = MARK_OFF(UNDERLINE);
	break;
    case 25:
	found = MARK_OFF(BLINK);
	break;
    case 27:
	found = MARK_OFF(INVERSE);
	break;
    }

    /*
     * Write a glitch with the attributes temporarily set to the new(er)
     * ones.
     */
    if (found) {
	unsigned save = xw->flags;
	xw->flags ^= whichone;
	TRACE(("XMC Writing glitch (%d/%d) after SGR %d\n", my_attrs,
	       whichone, param));
	dotext(xw, '?', glitch, screen->xmc_glitch);
	xw->flags = save;
    }
}

/*
 * Force a glitch on cursor movement when we're in standout mode and not at the
 * end of a line.
 */
void
Jump_XMC(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    if (!screen->move_sgr_ok
	&& screen->cur_col <= LineMaxCol(screen,
					 getLineData(screen, screen->cur_row))) {
	Mark_XMC(xw, -1);
    }
}

/*
 * After writing text to the screen, resolve mismatch between the current
 * location and any attributes that would have been set by preceding locations.
 */
void
Resolve_XMC(XtermWidget xw)
{
    TScreen *screen = TScreenOf(xw);
    LineData *ld;
    Bool changed = False;
    IAttr start;
    IAttr my_attrs = CharOf(screen->xmc_attributes & XMC_FLAGS);
    int row = screen->cur_row;
    int col = screen->cur_col;

    /* Find the preceding cell.
     */
    ld = getLineData(screen, row);
    if (ld->charData[col] != XMC_GLITCH) {
	if (col != 0) {
	    col--;
	} else if (!screen->xmc_inline && row != 0) {
	    ld = getLineData(screen, --row);
	    col = LineMaxCol(screen, ld);
	}
    }
    start = (ld->attribs[col] & my_attrs);

    /* Now propagate the starting state until we reach a cell which holds
     * a glitch.
     */
    for (;;) {
	if (col < LineMaxCol(screen, ld)) {
	    col++;
	} else if (!screen->xmc_inline && row < screen->max_row) {
	    col = 0;
	    ld = getLineData(screen, ++row);
	} else
	    break;
	if (ld->charData[col] == XMC_GLITCH)
	    break;
	if ((ld->attribs[col] & my_attrs) != start) {
	    ld->attribs[col] =
		(IAttr) (start | (ld->attribs[col] & ~my_attrs));
	    changed = True;
	}
    }

    TRACE(("XMC %s (%s:%d/%d) from %d,%d to %d,%d\n",
	   changed ? "Ripple" : "Nochange",
	   BtoS(xw->flags & my_attrs),
	   my_attrs, start,
	   screen->cur_row, screen->cur_col,
	   row, col));

    if (changed) {
	ScrnUpdate(xw, screen->cur_row, 0, row + 1 - screen->cur_row,
		   MaxCols(screen), True);
    }
}
