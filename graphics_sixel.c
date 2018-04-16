/* $XTermId: graphics_sixel.c,v 1.18 2016/06/05 20:04:01 tom Exp $ */

/*
 * Copyright 2014,2016 by Ross Combs
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

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include <data.h>
#include <VTparse.h>
#include <ptyx.h>

#include <assert.h>
#include <graphics.h>
#include <graphics_sixel.h>

/***====================================================================***/
/*
 * Parse numeric parameters which have the operator as a prefix rather than a
 * suffix as in ANSI format.
 *
 *  #             0
 *  #1            1
 *  #1;           1
 *  "1;2;640;480  4
 *  #1;2;0;0;0    5
 */
static void
parse_prefixedtype_params(ANSI *params, const char **string)
{
    const char *cp = *string;
    ParmType nparam = 0;
    int last_empty = 1;

    memset(params, 0, sizeof(*params));
    params->a_final = CharOf(*cp);
    if (*cp != '\0')
	cp++;

    while (*cp != '\0') {
	Char ch = CharOf(*cp);

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
	} else if (ch == ' ' || ch == '\r' || ch == '\n') {
	    /* EMPTY */ ;
	} else {
	    break;
	}
	cp++;
    }

    *string = cp;
    if (!last_empty)
	nparam++;
    if (nparam > NPARAM)
	params->a_nparam = NPARAM;
    else
	params->a_nparam = nparam;
}

typedef struct {
    RegisterNum current_register;
    RegisterNum background;	/* current background color register or hole */
    int aspect_vertical;
    int aspect_horizontal;
    int declared_width;		/* size as reported by the application */
    int declared_height;	/* size as reported by the application */
    int row;			/* context used during parsing */
    int col;			/* context used during parsing */
} SixelContext;

/* sixel scrolling:
 * VK100/GIGI ? (did it even support Sixel?)
 * VT125      unsupported
 * VT240      unsupported
 * VT241      unsupported
 * VT330      mode setting
 * VT382      ?
 * VT340      mode setting
 * dxterm     ?
 */

static void
init_sixel_background(Graphic *graphic, SixelContext const *context)
{
    const int mw = graphic->max_width;
    int r, c;

    TRACE(("initializing sixel background to size=%dx%d bgcolor=%hu\n",
	   context->declared_width,
	   context->declared_height,
	   context->background));

    if (context->background == COLOR_HOLE)
	return;

    for (r = 0; r < graphic->actual_height; r++) {
	for (c = 0; c < graphic->actual_width; c++) {
	    graphic->pixels[r * mw + c] = context->background;
	}
    }
    graphic->color_registers_used[context->background] = 1;
}

static void
set_sixel(Graphic *graphic, SixelContext const *context, int sixel)
{
    const int mh = graphic->max_height;
    const int mw = graphic->max_width;
    RegisterNum color;
    int pix;

    color = context->current_register;
    TRACE(("drawing sixel at pos=%d,%d color=%hu (hole=%d, [%d,%d,%d])\n",
	   context->col,
	   context->row,
	   color,
	   color == COLOR_HOLE,
	   ((color != COLOR_HOLE)
	    ? (unsigned) graphic->color_registers[color].r : 0U),
	   ((color != COLOR_HOLE)
	    ? (unsigned) graphic->color_registers[color].g : 0U),
	   ((color != COLOR_HOLE)
	    ? (unsigned) graphic->color_registers[color].b : 0U)));
    for (pix = 0; pix < 6; pix++) {
	if (context->col < mw && context->row + pix < mh) {
	    if (sixel & (1 << pix)) {
		if (context->col + 1 > graphic->actual_width) {
		    graphic->actual_width = context->col + 1;
		}
		if (context->row + pix + 1 > graphic->actual_height) {
		    graphic->actual_height = context->row + pix + 1;
		}
		graphic->pixels[((context->row + pix) * mw) + context->col] =
		    color;
	    }
	} else {
	    TRACE(("sixel pixel %d out of bounds\n", pix));
	}
    }
}

static void
update_sixel_aspect(SixelContext const *context, Graphic *graphic)
{
    /* We want to keep the ratio accurate but would like every pixel to have
     * the same size so keep these as whole numbers.
     */
    /* FIXME: DEC terminals had pixels about twice as tall as they were wide,
     * and it seems the VT125 and VT24x only used data from odd graphic rows.
     * This means it basically cancels out if we ignore both, except that
     * the even rows of pixels may not be written by the application such that
     * they are suitable for display.  In practice this doesn't seem to be
     * an issue but I have very few test files/programs.
     */
    if (context->aspect_vertical < context->aspect_horizontal) {
	graphic->pixw = 1;
	graphic->pixh = ((context->aspect_vertical
			  + context->aspect_horizontal - 1)
			 / context->aspect_horizontal);
    } else {
	graphic->pixw = ((context->aspect_horizontal
			  + context->aspect_vertical - 1)
			 / context->aspect_vertical);
	graphic->pixh = 1;
    }
    TRACE(("sixel aspect ratio: an=%d ad=%d -> pixw=%d pixh=%d\n",
	   context->aspect_vertical,
	   context->aspect_horizontal,
	   graphic->pixw,
	   graphic->pixh));
}

static int
finished_parsing(XtermWidget xw, Graphic *graphic)
{
    TScreen *screen = TScreenOf(xw);

    /* Update the screen scrolling and do a refresh.
     * The refresh may not cover the whole graphic.
     */
    if (screen->scroll_amt)
	FlushScroll(xw);

    if (xw->keyboard.flags & MODE_DECSDM) {
	int new_row, new_col;

	if (screen->sixel_scrolls_right) {
	    new_row = (graphic->charrow
		       + (((graphic->actual_height * graphic->pixh)
			   + FontHeight(screen) - 1)
			  / FontHeight(screen))
		       - 1);
	    new_col = (graphic->charcol
		       + (((graphic->actual_width * graphic->pixw)
			   + FontWidth(screen) - 1)
			  / FontWidth(screen)));
	} else {
	    /* FIXME: At least of the VT382 the vertical position appears to be
	     * truncated (rounded toward zero after converting to character row.
	     * This code rounds up, which seems more useful, but it would be
	     * better to be compatible.  Verify this is true on a VT3[34]0 as
	     * well.
	     */
	    new_row = (graphic->charrow
		       + (((graphic->actual_height * graphic->pixh)
			   + FontHeight(screen) - 1)
			  / FontHeight(screen)));
	    new_col = 0;
	}

	TRACE(("setting text position after %dx%d graphic starting on row=%d col=%d: cursor new_row=%d new_col=%d\n",
	       graphic->actual_width * graphic->pixw,
	       graphic->actual_height * graphic->pixh,
	       graphic->charrow,
	       graphic->charcol,
	       new_row, new_col));

	if (new_col > screen->rgt_marg) {
	    new_col = screen->lft_marg;
	    new_row++;
	    TRACE(("column past left margin, overriding to row=%d col=%d\n",
		   new_row, new_col));
	}

	while (new_row > screen->bot_marg) {
	    xtermScroll(xw, 1);
	    new_row--;
	    TRACE(("bottom row was past screen.  new start row=%d, cursor row=%d\n",
		   graphic->charrow, new_row));
	}

	if (new_row < 0) {
	    /* FIXME: this was triggering, now it isn't */
	    TRACE(("new row is going to be negative (%d); skipping position update!",
		   new_row));
	} else {
	    set_cur_row(screen, new_row);
	    set_cur_col(screen, new_col <= screen->rgt_marg ? new_col : screen->rgt_marg);
	}
    }

    graphic->dirty = 1;
    refresh_modified_displayed_graphics(xw);

    TRACE(("DONE parsed sixel data\n"));
    dump_graphic(graphic);
    return 0;
}

/*
 * Interpret sixel graphics sequences.
 *
 * Resources:
 *  http://vt100.net/docs/vt3xx-gp/chapter14.html
 *  ftp://ftp.cs.utk.edu/pub/shuford/terminal/sixel_graphics_news.txt
 *  ftp://ftp.cs.utk.edu/pub/shuford/terminal/all_about_sixels.txt
 */
int
parse_sixel(XtermWidget xw, ANSI *params, char const *string)
{
    TScreen *screen = TScreenOf(xw);
    Graphic *graphic;
    SixelContext context;

    switch (screen->terminal_id) {
    case 240:
    case 241:
    case 330:
    case 340:
	context.aspect_vertical = 2;
	context.aspect_horizontal = 1;
	break;
    case 382:
	context.aspect_vertical = 1;
	context.aspect_horizontal = 1;
	break;
    default:
	context.aspect_vertical = 2;
	context.aspect_horizontal = 1;
	break;
    }

    context.declared_width = 0;
    context.declared_height = 0;

    context.row = 0;
    context.col = 0;

    /* default isn't white on the VT240, but not sure what it is */
    context.current_register = 3;	/* FIXME: using green, but not sure what it should be */

    if (xw->keyboard.flags & MODE_DECSDM) {
	TRACE(("sixel scrolling enabled: inline positioning for graphic at %d,%d\n",
	       screen->cur_row, screen->cur_col));
	graphic = get_new_graphic(xw, screen->cur_row, screen->cur_col, 0U);
    } else {
	TRACE(("sixel scrolling disabled: inline positioning for graphic at %d,%d\n",
	       0, 0));
	graphic = get_new_graphic(xw, 0, 0, 0U);
    }

    {
	int Pmacro = params->a_param[0];
	int Pbgmode = params->a_param[1];
	int Phgrid = params->a_param[2];
	int Pan = params->a_param[3];
	int Pad = params->a_param[4];
	int Ph = params->a_param[5];
	int Pv = params->a_param[6];

	(void) Phgrid;

	TRACE(("sixel bitmap graphics sequence: params=%d (Pmacro=%d Pbgmode=%d Phgrid=%d) scroll_amt=%d\n",
	       params->a_nparam,
	       Pmacro,
	       Pbgmode,
	       Phgrid,
	       screen->scroll_amt));

	switch (params->a_nparam) {
	case 7:
	    if (Pan == 0 || Pad == 0) {
		TRACE(("DATA_ERROR: invalid raster ratio %d/%d\n", Pan, Pad));
		return -1;
	    }
	    context.aspect_vertical = Pan;
	    context.aspect_horizontal = Pad;

	    if (Ph <= 0 || Pv <= 0) {
		TRACE(("DATA_ERROR: raster image dimensions are invalid %dx%d\n",
		       Ph, Pv));
		return -1;
	    }
	    if (Ph > graphic->max_width || Pv > graphic->max_height) {
		TRACE(("DATA_ERROR: raster image dimensions are too large %dx%d\n",
		       Ph, Pv));
		return -1;
	    }
	    context.declared_width = Ph;
	    context.declared_height = Pv;
	    if (context.declared_width > graphic->actual_width) {
		graphic->actual_width = context.declared_width;
	    }
	    if (context.declared_height > graphic->actual_height) {
		graphic->actual_height = context.declared_height;
	    }
	    break;
	case 3:
	case 2:
	case 1:
	    switch (Pmacro) {
	    case 0:
		/* keep default aspect settings */
		break;
	    case 1:
	    case 5:
	    case 6:
		context.aspect_vertical = 2;
		context.aspect_horizontal = 1;
		break;
	    case 2:
		context.aspect_vertical = 5;
		context.aspect_horizontal = 1;
		break;
	    case 3:
	    case 4:
		context.aspect_vertical = 3;
		context.aspect_horizontal = 1;
		break;
	    case 7:
	    case 8:
	    case 9:
		context.aspect_vertical = 1;
		context.aspect_horizontal = 1;
		break;
	    default:
		TRACE(("DATA_ERROR: unknown sixel macro mode parameter\n"));
		return -1;
	    }
	    break;
	case 0:
	    break;
	default:
	    TRACE(("DATA_ERROR: unexpected parameter count (found %d)\n", params->a_nparam));
	    return -1;
	}

	if (Pbgmode == 1) {
	    context.background = COLOR_HOLE;
	} else {
	    /* FIXME: is the default background register always zero?  what about in light background mode? */
	    context.background = 0;
	}

	/* Ignore the grid parameter because it seems only printers paid attention to it.
	 * The VT3xx was always 0.0195 cm.
	 */
    }

    update_sixel_aspect(&context, graphic);

    for (;;) {
	Char ch = CharOf(*string);
	if (ch == '\0')
	    break;

	if (ch >= 0x3f && ch <= 0x7e) {
	    int sixel = ch - 0x3f;
	    TRACE(("sixel=%x (%c)\n", sixel, (char) ch));
	    if (!graphic->valid) {
		init_sixel_background(graphic, &context);
		graphic->valid = 1;
	    }
	    set_sixel(graphic, &context, sixel);
	    context.col++;
	} else if (ch == '$') {	/* DECGCR */
	    /* ignore DECCRNLM in sixel mode */
	    TRACE(("sixel CR\n"));
	    context.col = 0;
	} else if (ch == '-') {	/* DECGNL */
	    int scroll_lines;
	    TRACE(("sixel NL\n"));
	    scroll_lines = 0;
	    while (graphic->charrow - scroll_lines +
		   (((context.row + 6) * graphic->pixh
		     + FontHeight(screen) - 1)
		    / FontHeight(screen)) > screen->bot_marg) {
		scroll_lines++;
	    }
	    context.col = 0;
	    context.row += 6;
	    /* If we hit the bottom margin on the graphics page (well, we just use the
	     * text margin for now), the behavior is to either scroll or to discard
	     * the remainder of the graphic depending on this setting.
	     */
	    if (scroll_lines > 0) {
		if (xw->keyboard.flags & MODE_DECSDM) {
		    Display *display = screen->display;
		    xtermScroll(xw, scroll_lines);
		    XSync(display, False);
		    TRACE(("graphic scrolled the screen %d lines. screen->scroll_amt=%d screen->topline=%d, now starting row is %d\n",
			   scroll_lines,
			   screen->scroll_amt,
			   screen->topline,
			   graphic->charrow));
		} else {
		    break;
		}
	    }
	} else if (ch == '!') {	/* DECGRI */
	    int Pcount;
	    const char *start;
	    int sixel;
	    int i;

	    start = ++string;
	    for (;;) {
		ch = CharOf(*string);
		if (ch != '0' &&
		    ch != '1' &&
		    ch != '2' &&
		    ch != '3' &&
		    ch != '4' &&
		    ch != '5' &&
		    ch != '6' &&
		    ch != '7' &&
		    ch != '8' &&
		    ch != '9' &&
		    ch != ' ' &&
		    ch != '\r' &&
		    ch != '\n')
		    break;
		string++;
	    }
	    if (ch == '\0') {
		TRACE(("DATA_ERROR: sixel data string terminated in the middle of a repeat operator\n"));
		return finished_parsing(xw, graphic);
	    }
	    if (string == start) {
		TRACE(("DATA_ERROR: sixel data string contains a repeat operator with empty count\n"));
		return finished_parsing(xw, graphic);
	    }
	    Pcount = atoi(start);
	    sixel = ch - 0x3f;
	    TRACE(("sixel repeat operator: sixel=%d (%c), count=%d\n",
		   sixel, (char) ch, Pcount));
	    if (!graphic->valid) {
		init_sixel_background(graphic, &context);
		graphic->valid = 1;
	    }
	    for (i = 0; i < Pcount; i++) {
		set_sixel(graphic, &context, sixel);
		context.col++;
	    }
	} else if (ch == '#') {	/* DECGCI */
	    ANSI color_params;
	    int Pregister;

	    parse_prefixedtype_params(&color_params, &string);
	    Pregister = color_params.a_param[0];
	    if (Pregister >= (int) graphic->valid_registers) {
		TRACE(("DATA_WARNING: sixel color operator uses out-of-range register %d\n", Pregister));
		/* FIXME: supposedly the DEC terminals wrapped register indicies -- verify */
		while (Pregister >= (int) graphic->valid_registers)
		    Pregister -= (int) graphic->valid_registers;
		TRACE(("DATA_WARNING: converted to %d\n", Pregister));
	    }

	    if (color_params.a_nparam > 2 && color_params.a_nparam <= 5) {
		int Pspace = color_params.a_param[1];
		int Pc1 = color_params.a_param[2];
		int Pc2 = color_params.a_param[3];
		int Pc3 = color_params.a_param[4];
		short r, g, b;

		TRACE(("sixel set color register=%d space=%d color=[%d,%d,%d] (nparams=%d)\n",
		       Pregister, Pspace, Pc1, Pc2, Pc3, color_params.a_nparam));

		switch (Pspace) {
		case 1:	/* HLS */
		    if (Pc1 > 360 || Pc2 > 100 || Pc3 > 100) {
			TRACE(("DATA_ERROR: sixel set color operator uses out-of-range HLS color coordinates %d,%d,%d\n",
			       Pc1, Pc2, Pc3));
			return finished_parsing(xw, graphic);
		    }
		    hls2rgb(Pc1, Pc2, Pc3, &r, &g, &b);
		    break;
		case 2:	/* RGB */
		    if (Pc1 > 100 || Pc2 > 100 || Pc3 > 100) {
			TRACE(("DATA_ERROR: sixel set color operator uses out-of-range RGB color coordinates %d,%d,%d\n",
			       Pc1, Pc2, Pc3));
			return finished_parsing(xw, graphic);
		    }
		    r = (short) Pc1;
		    g = (short) Pc2;
		    b = (short) Pc3;
		    break;
		default:	/* unknown */
		    TRACE(("DATA_ERROR: sixel set color operator uses unknown color space %d\n", Pspace));
		    return finished_parsing(xw, graphic);
		}
		update_color_register(graphic,
				      (RegisterNum) Pregister,
				      r, g, b);
	    } else if (color_params.a_nparam == 1) {
		TRACE(("sixel switch to color register=%d (nparams=%d)\n",
		       Pregister, color_params.a_nparam));
		context.current_register = (RegisterNum) Pregister;
	    } else {
		TRACE(("DATA_ERROR: sixel switch color operator with unexpected parameter count (nparams=%d)\n", color_params.a_nparam));
		return finished_parsing(xw, graphic);
	    }
	    continue;
	} else if (ch == '"') /* DECGRA */  {
	    ANSI raster_params;

	    parse_prefixedtype_params(&raster_params, &string);
	    if (raster_params.a_nparam < 2) {
		TRACE(("DATA_ERROR: sixel raster attribute operator with incomplete parameters (found %d, expected 2 or 4)\n", raster_params.a_nparam));
		return finished_parsing(xw, graphic);
	    } {
		int Pan = raster_params.a_param[0];
		int Pad = raster_params.a_param[1];
		TRACE(("sixel raster attribute with h:w=%d:%d\n", Pan, Pad));
		if (Pan == 0 || Pad == 0) {
		    TRACE(("DATA_ERROR: invalid raster ratio %d/%d\n", Pan, Pad));
		    return finished_parsing(xw, graphic);
		}
		context.aspect_vertical = Pan;
		context.aspect_horizontal = Pad;
		update_sixel_aspect(&context, graphic);
	    }

	    if (raster_params.a_nparam >= 4) {
		int Ph = raster_params.a_param[2];
		int Pv = raster_params.a_param[3];

		TRACE(("sixel raster attribute with h=%d v=%d\n", Ph, Pv));
		if (Ph <= 0 || Pv <= 0) {
		    TRACE(("DATA_ERROR: raster image dimensions are invalid %dx%d\n",
			   Ph, Pv));
		    return finished_parsing(xw, graphic);
		}
		if (Ph > graphic->max_width || Pv > graphic->max_height) {
		    TRACE(("DATA_ERROR: raster image dimensions are too large %dx%d\n",
			   Ph, Pv));
		    return finished_parsing(xw, graphic);
		}
		context.declared_width = Ph;
		context.declared_height = Pv;
		if (context.declared_width > graphic->actual_width) {
		    graphic->actual_width = context.declared_width;
		}
		if (context.declared_height > graphic->actual_height) {
		    graphic->actual_height = context.declared_height;
		}
	    }

	    continue;
	} else if (ch == ' ' || ch == '\r' || ch == '\n') {
	    /* EMPTY */ ;
	} else {
	    TRACE(("DATA_ERROR: skipping unknown sixel command %04x (%c)\n",
		   (int) ch, ch));
	}

	string++;
    }

    return finished_parsing(xw, graphic);
}
