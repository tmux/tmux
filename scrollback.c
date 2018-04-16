/* $XTermId: scrollback.c,v 1.18 2016/05/22 16:34:38 tom Exp $ */

/*
 * Copyright 2009-2014,2016 by Thomas E. Dickey
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

#include <xterm.h>

#define REAL_ROW(screen, row) ((row) + (screen)->saved_fifo)
#define ROW2FIFO(screen, row) \
	(unsigned) (REAL_ROW(screen, row) % (screen)->savelines)

/*
 * Given a row-number, find the corresponding data for the line in the VT100
 * widget's saved-line FIFO.  The row-number (from getLineData) is negative.
 * So we just count backwards from the last saved line.
 */
LineData *
getScrollback(TScreen *screen, int row)
{
    LineData *result = 0;

    if (screen->saved_fifo > 0 && REAL_ROW(screen, row) >= 0) {
	unsigned which = ROW2FIFO(screen, row);
	ScrnBuf where = scrnHeadAddr(screen, screen->saveBuf_index, which);
	result = (LineData *) where;
    }

    TRACE(("getScrollback %d -> %d -> %p\n",
	   row, ROW2FIFO(screen, row),
	   (void *) result));
    return result;
}

/*
 * Allocate a new row in the scrollback FIFO, returning a pointer to it.
 */
LineData *
addScrollback(TScreen *screen)
{
    ScrnBuf where = 0;
    unsigned ncols = (unsigned) MaxCols(screen);

    if (screen->saveBuf_index != 0) {
	unsigned which;
	Char *block;

	TRACE(("addScrollback %lu\n", screen->saved_fifo));

	/* first, see which index we'll use */
	which = (unsigned) (screen->saved_fifo % screen->savelines);
	where = scrnHeadAddr(screen, screen->saveBuf_index, which);

	/* discard any obsolete index data */
	if (screen->saved_fifo > screen->savelines) {
	    LineData *prior = (LineData *) where;
	    /*
	     * setupLineData uses the attribs as the first address used from the
	     * data block.
	     */
	    if (prior->attribs != 0) {
		TRACE(("...freeing prior FIFO data in slot %d: %p->%p\n",
		       which, (void *) prior, (void *) prior->attribs));
		free(prior->attribs);
		prior->attribs = 0;
	    }
	    if (screen->saved_fifo > 2 * screen->savelines) {
		screen->saved_fifo -= screen->savelines;
	    }
	}

	/* allocate the new data */
	block = allocScrnData(screen, 1, ncols);

	/* record the new data in the index */
	setupLineData(screen, where, (Char *) block, 1, ncols);

	TRACE(("...storing new FIFO data in slot %d: %p->%p\n",
	       which, (void *) where, block));

	screen->saved_fifo++;
    }
    return (LineData *) where;
}

void
deleteScrollback(TScreen *screen)
{
    unsigned which = ROW2FIFO(screen, -1);
    ScrnBuf where = scrnHeadAddr(screen, screen->saveBuf_index, which);
    LineData *prior = (LineData *) where;
    /*
     * setupLineData uses the attribs as the first address used from the
     * data block.
     */
    if (prior->attribs != 0) {
	TRACE(("...freeing prior FIFO data in slot %d: %p->%p\n",
	       which, (void *) prior, (void *) prior->attribs));
	free(prior->attribs);
	prior->attribs = 0;
    }
    screen->saved_fifo--;
}
