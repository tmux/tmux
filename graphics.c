/* $XTermId: graphics.c,v 1.73 2017/06/18 18:20:22 tom Exp $ */

/*
 * Copyright 2013-2016,2017 by Ross Combs
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
#include <ptyx.h>

#include <assert.h>
#include <graphics.h>

#undef DUMP_BITMAP
#undef DUMP_COLORS
#undef DEBUG_PALETTE
#undef DEBUG_PIXEL
#undef DEBUG_REFRESH

/*
 * graphics TODO list
 *
 * ReGIS:
 * - ship a default alphabet zero font instead of scaling Xft fonts
 * - input cursors
 * - output cursors
 * - mouse/tablet/arrow-key input
 * - fix graphic pages for ReGIS -- they should also apply to text and sixel graphics
 * - fix interpolated curves to more closely match implementation (identical despite direction and starting point)
 * - non-ASCII alphabets
 * - enter/leave anywhere in a command
 * - locator key definitions (DECLKD)
 * - command display mode
 * - re-rasterization on window resize
 * - macros
 * - improved fills for narrow angles (track actual lines not just pixels)
 * - hardcopy/screen-capture support (need dialog of some sort for safety)
 * - error reporting
 *
 * sixel:
 * - fix problem where new_row < 0 during sixel parsing (see FIXME)
 * - screen-capture support (need dialog of some sort for safety)
 *
 * VT55/VT105 waveform graphics
 * - everything
 *
 * Tektronix:
 * - color (VT340 4014 emulation, 41xx, IRAF GTERM, and also MS-DOS Kermit color support)
 * - polygon fill (41xx)
 * - clear area extension
 * - area fill extension
 * - pixel operations (RU/RS/RP)
 * - research other 41xx and 42xx extensions
 *
 * common graphics features:
 * - handle light/dark screen modes (CSI?5[hl])
 * - update text fg/bg color which overlaps images
 * - handle graphic updates in scroll regions (verify effect on graphics)
 * - handle rectangular area copies (verify they work with graphics)
 * - invalidate graphics under graphic if same origin, at least as big, and bg not transparent
 * - invalidate graphic if completely scrolled past end of scrollback
 * - invalidate graphic if all pixels are transparent/erased
 * - invalidate graphic if completely scrolled out of alt buffer
 * - posturize requested colors to match hardware palettes (e.g. only four possible shades on VT240)
 * - color register report/restore
 * - ability to select/copy graphics for pasting in other programs
 * - ability to show non-scroll-mode sixel graphics in a separate window
 * - ability to show ReGIS graphics in a separate window
 * - ability to show Tektronix graphics in VT100 window
 * - truncate graphics at bottom edge of terminal?
 * - locator events (DECEFR DECSLE DECELR DECLRP)
 * - locator controller mode (CSI6i / CSI7i)
 *
 * new escape sequences:
 * - way to query text font size without "window ops" (or make "window ops" permissions more fine grained)
 * - way to query and set the number of graphics pages
 *
 * ReGIS extensions:
 * - non-integer text scaling
 * - free distortionless text rotation (vs. simulating the distortion and aligning to 45deg increments)
 * - font characteristics: bold/underline/italic
 * - remove/increase arbitrary limits (pattern size, pages, alphabets, stack size, font names, etc.)
 * - shade/fill with borders
 * - sprites (copy portion of page into/out of buffer with scaling and rotation)
 * - ellipses
 * - 2D patterns
 * - option to set actual graphic size (not just coordinate range)
 * - gradients (for lines and fills)
 * - line width (RLogin has this and it is mentioned in docs for the DEC ReGIS to Postscript converter)
 * - transparency
 * - background color as stackable write control
 * - true color (virtual color registers created upon lookup)
 * - anti-aliasing
 * - variable-width (proportional) text
 */

/* font sizes:
 * VT510:
 *   80 Columns 132 Columns Maximum Number of Lines
 *   10 x 16   6 x 16  26 lines + keyboard indicator line
 *   10 x 13   6 x 13  26 lines + keyboard indicator line
 *   10 x 10   6 x 10  42 lines + keyboard indicator line
 *   10 x 8    6 x 8   53 lines + keyboard indicator line
 */

typedef struct allocated_color_register {
    struct allocated_color_register *next;
    Pixel pix;
    short r, g, b;
} AllocatedColorRegister;

#define LOOKUP_WIDTH 16
static AllocatedColorRegister *allocated_colors[LOOKUP_WIDTH][LOOKUP_WIDTH][LOOKUP_WIDTH];

#define FOR_EACH_SLOT(ii) for (ii = 0U; ii < MAX_GRAPHICS; ii++)

static ColorRegister *shared_color_registers;
static Graphic *displayed_graphics[MAX_GRAPHICS];
static unsigned next_graphic_id = 0U;

static ColorRegister *
allocRegisters(void)
{
    return TypeCallocN(ColorRegister, MAX_COLOR_REGISTERS);
}

static Graphic *
freeGraphic(Graphic *obj)
{
    if (obj) {
	if (obj->pixels)
	    free(obj->pixels);
	if (obj->private_color_registers)
	    free(obj->private_color_registers);
	free(obj);
    }
    return NULL;
}

static Graphic *
allocGraphic(int max_w, int max_h)
{
    Graphic *result = TypeCalloc(Graphic);
    if (result) {
	result->max_width = max_w;
	result->max_height = max_h;
	if (!(result->pixels = TypeCallocN(RegisterNum,
					     (size_t) max_w * (size_t) max_h))) {
	    result = freeGraphic(result);
	} else if (!(result->private_color_registers = allocRegisters())) {
	    result = freeGraphic(result);
	}
    }
    return result;
}

static Graphic *
getActiveSlot(unsigned n)
{
    if (n < MAX_GRAPHICS &&
	displayed_graphics[n] &&
	displayed_graphics[n]->valid) {
	return displayed_graphics[n];
    }
    return NULL;
}

static Graphic *
getInactiveSlot(const TScreen *screen, unsigned n)
{
    if (n < MAX_GRAPHICS &&
	(!displayed_graphics[n] ||
	 !displayed_graphics[n]->valid)) {
	if (!displayed_graphics[n]) {
	    displayed_graphics[n] = allocGraphic(screen->graphics_max_wide,
						 screen->graphics_max_high);
	}
	return displayed_graphics[n];
    }
    return NULL;
}

static ColorRegister *
getSharedRegisters(void)
{
    if (!shared_color_registers)
	shared_color_registers = allocRegisters();
    return shared_color_registers;
}

static void
deactivateSlot(unsigned n)
{
    if (n < MAX_GRAPHICS) {
	displayed_graphics[n] = freeGraphic(displayed_graphics[n]);
    }
}

extern RegisterNum
read_pixel(Graphic *graphic, int x, int y)
{
    if (x < 0 || x >= graphic->actual_width ||
	y < 0 || y >= graphic->actual_height) {
	return COLOR_HOLE;
    }

    return graphic->pixels[y * graphic->max_width + x];
}

#define _draw_pixel(G, X, Y, C) \
    do { \
        (G)->pixels[(Y) * (G)->max_width + (X)] = (RegisterNum) (C); \
    } while (0)

void
draw_solid_pixel(Graphic *graphic, int x, int y, unsigned color)
{
    assert(color <= MAX_COLOR_REGISTERS);

#ifdef DEBUG_PIXEL
    TRACE(("drawing pixel at %d,%d color=%hu (hole=%hu, [%d,%d,%d])\n",
	   x,
	   y,
	   color,
	   COLOR_HOLE,
	   ((color != COLOR_HOLE)
	    ? (unsigned) graphic->color_registers[color].r : 0U),
	   ((color != COLOR_HOLE)
	    ? (unsigned) graphic->color_registers[color].g : 0U),
	   ((color != COLOR_HOLE)
	    ? (unsigned) graphic->color_registers[color].b : 0U)));
#endif
    if (x >= 0 && x < graphic->actual_width &&
	y >= 0 && y < graphic->actual_height) {
	_draw_pixel(graphic, x, y, color);
	if (color < MAX_COLOR_REGISTERS)
	    graphic->color_registers_used[color] = 1;
    }
}

void
draw_solid_rectangle(Graphic *graphic, int x1, int y1, int x2, int y2, unsigned color)
{
    int x, y;
    int tmp;

    assert(color <= MAX_COLOR_REGISTERS);

    if (x1 > x2) {
	EXCHANGE(x1, x2, tmp);
    }
    if (y1 > y2) {
	EXCHANGE(y1, y2, tmp);
    }

    if (x2 < 0 || x1 >= graphic->actual_width ||
	y2 < 0 || y1 >= graphic->actual_height)
	return;

    if (x1 < 0)
	x1 = 0;
    if (x2 >= graphic->actual_width)
	x2 = graphic->actual_width - 1;
    if (y1 < 0)
	y1 = 0;
    if (y2 >= graphic->actual_height)
	y2 = graphic->actual_height - 1;

    if (color < MAX_COLOR_REGISTERS)
	graphic->color_registers_used[color] = 1;
    for (y = y1; y <= y2; y++)
	for (x = x1; x <= x2; x++)
	    _draw_pixel(graphic, x, y, color);
}

void
draw_solid_line(Graphic *graphic, int x1, int y1, int x2, int y2, unsigned color)
{
    int x, y;
    int dx, dy;
    int dir, diff;

    assert(color <= MAX_COLOR_REGISTERS);

    dx = abs(x1 - x2);
    dy = abs(y1 - y2);

    if (dx > dy) {
	if (x1 > x2) {
	    int tmp;
	    EXCHANGE(x1, x2, tmp);
	    EXCHANGE(y1, y2, tmp);
	}
	if (y1 < y2)
	    dir = 1;
	else if (y1 > y2)
	    dir = -1;
	else
	    dir = 0;

	diff = 0;
	y = y1;
	for (x = x1; x <= x2; x++) {
	    if (diff >= dx) {
		diff -= dx;
		y += dir;
	    }
	    diff += dy;
	    draw_solid_pixel(graphic, x, y, color);
	}
    } else {
	if (y1 > y2) {
	    int tmp;
	    EXCHANGE(x1, x2, tmp);
	    EXCHANGE(y1, y2, tmp);
	}
	if (x1 < x2)
	    dir = 1;
	else if (x1 > x2)
	    dir = -1;
	else
	    dir = 0;

	diff = 0;
	x = x1;
	for (y = y1; y <= y2; y++) {
	    if (diff >= dy) {
		diff -= dy;
		x += dir;
	    }
	    diff += dx;
	    draw_solid_pixel(graphic, x, y, color);
	}
    }
}

void
copy_overlapping_area(Graphic *graphic, int src_ul_x, int src_ul_y,
		      int dst_ul_x, int dst_ul_y, unsigned w, unsigned h,
		      unsigned default_color)
{
    int sx, ex, dx;
    int sy, ey, dy;
    int xx, yy;
    RegisterNum color;

    if (dst_ul_x <= src_ul_x) {
	sx = 0;
	ex = (int) w - 1;
	dx = +1;
    } else {
	sx = (int) w - 1;
	ex = 0;
	dx = -1;
    }

    if (dst_ul_y <= src_ul_y) {
	sy = 0;
	ey = (int) h - 1;
	dy = +1;
    } else {
	sy = (int) h - 1;
	ey = 0;
	dy = -1;
    }

    for (yy = sy; yy != ey + dy; yy += dy) {
	int dst_y = dst_ul_y + yy;
	int src_y = src_ul_y + yy;
	if (dst_y < 0 || dst_y >= (int) graphic->actual_height)
	    continue;

	for (xx = sx; xx != ex + dx; xx += dx) {
	    int dst_x = dst_ul_x + xx;
	    int src_x = src_ul_x + xx;
	    if (dst_x < 0 || dst_x >= (int) graphic->actual_width)
		continue;

	    if (src_x < 0 || src_x >= (int) graphic->actual_width ||
		src_y < 0 || src_y >= (int) graphic->actual_height)
		color = (RegisterNum) default_color;
	    else
		color = graphic->pixels[(unsigned) (src_y *
						    graphic->max_width) +
					(unsigned) src_x];

	    graphic->pixels[(unsigned) (dst_y * graphic->max_width) +
			    (unsigned) dst_x] = color;
	}
    }
}

static void
set_color_register(ColorRegister *color_registers,
		   unsigned color,
		   int r,
		   int g,
		   int b)
{
    ColorRegister *reg = &color_registers[color];
    reg->r = (short) r;
    reg->g = (short) g;
    reg->b = (short) b;
}

/* Graphics which don't use private colors will act as if they are using a
 * device-wide color palette.
 */
static void
set_shared_color_register(unsigned color, int r, int g, int b)
{
    unsigned ii;

    assert(color < MAX_COLOR_REGISTERS);

    set_color_register(getSharedRegisters(), color, r, g, b);

    FOR_EACH_SLOT(ii) {
	Graphic *graphic;

	if (!(graphic = getActiveSlot(ii)))
	    continue;
	if (graphic->private_colors)
	    continue;

	if (graphic->color_registers_used[ii]) {
	    graphic->dirty = 1;
	}
    }
}

void
update_color_register(Graphic *graphic,
		      unsigned color,
		      int r,
		      int g,
		      int b)
{
    assert(color < MAX_COLOR_REGISTERS);

    if (graphic->private_colors) {
	set_color_register(graphic->private_color_registers,
			   color, r, g, b);
	if (graphic->color_registers_used[color]) {
	    graphic->dirty = 1;
	}
	graphic->color_registers_used[color] = 1;
    } else {
	set_shared_color_register(color, r, g, b);
    }
}

#define SQUARE(X) ( (X) * (X) )

RegisterNum
find_color_register(ColorRegister const *color_registers, int r, int g, int b)
{
    unsigned i;
    unsigned closest_index;
    unsigned closest_distance;

    /* I have no idea what algorithm DEC used for this.
     * The documentation warns that it is unpredictable, especially with values
     * far away from any allocated color so it is probably a very simple
     * heuristic rather than something fancy like finding the minimum distance
     * in a linear perceptive color space.
     */
    closest_index = MAX_COLOR_REGISTERS;
    closest_distance = 0U;
    for (i = 0U; i < MAX_COLOR_REGISTERS; i++) {
	unsigned d = (unsigned) (SQUARE(2 * (color_registers[i].r - r)) +
				 SQUARE(3 * (color_registers[i].g - g)) +
				 SQUARE(1 * (color_registers[i].b - b)));
	if (closest_index == MAX_COLOR_REGISTERS || d < closest_distance) {
	    closest_index = i;
	    closest_distance = d;
	}
    }

    TRACE(("found closest color register to %d,%d,%d: %u (distance %u value %d,%d,%d)\n",
	   r, g, b,
	   closest_index,
	   closest_distance,
	   color_registers[closest_index].r,
	   color_registers[closest_index].g,
	   color_registers[closest_index].b));
    return (RegisterNum) closest_index;
}

static void
init_color_registers(ColorRegister *color_registers, int terminal_id)
{
    TRACE(("setting initial colors for terminal %d\n", terminal_id));
    {
	unsigned i;

	for (i = 0U; i < MAX_COLOR_REGISTERS; i++) {
	    set_color_register(color_registers, (RegisterNum) i, 0, 0, 0);
	}
    }

    /*
     * default color registers:
     *     (mono) (color)
     * VK100/GIGI (fixed)
     * VT125:
     *   0: 0%      0%
     *   1: 33%     blue
     *   2: 66%     red
     *   3: 100%    green
     * VT240:
     *   0: 0%      0%
     *   1: 33%     blue
     *   2: 66%     red
     *   3: 100%    green
     * VT241:
     *   0: 0%      0%
     *   1: 33%     blue
     *   2: 66%     red
     *   3: 100%    green
     * VT330:
     *   0: 0%      0%              (bg for light on dark mode)
     *   1: 33%     blue (red?)
     *   2: 66%     red (green?)
     *   3: 100%    green (yellow?) (fg for light on dark mode)
     * VT340:
     *   0: 0%      0%              (bg for light on dark mode)
     *   1: 14%     blue
     *   2: 29%     red
     *   3: 43%     green
     *   4: 57%     magenta
     *   5: 71%     cyan
     *   6: 86%     yellow
     *   7: 100%    50%             (fg for light on dark mode)
     *   8: 0%      25%
     *   9: 14%     gray-blue
     *  10: 29%     gray-red
     *  11: 43%     gray-green
     *  12: 57%     gray-magenta
     *  13: 71%     gray-cyan
     *  14: 86%     gray-yellow
     *  15: 100%    75%             ("white")
     * VT382:
     *   ? (FIXME: B&W only?)
     * dxterm:
     *  ?
     */
    switch (terminal_id) {
    case 125:
    case 241:
	set_color_register(color_registers, 0, 0, 0, 0);
	set_color_register(color_registers, 1, 0, 0, 100);
	set_color_register(color_registers, 2, 0, 100, 0);
	set_color_register(color_registers, 3, 100, 0, 0);
	break;
    case 240:
    case 330:
	set_color_register(color_registers, 0, 0, 0, 0);
	set_color_register(color_registers, 1, 33, 33, 33);
	set_color_register(color_registers, 2, 66, 66, 66);
	set_color_register(color_registers, 3, 100, 100, 100);
	break;
    case 340:
    default:
	set_color_register(color_registers, 0, 0, 0, 0);
	set_color_register(color_registers, 1, 20, 20, 80);
	set_color_register(color_registers, 2, 80, 13, 13);
	set_color_register(color_registers, 3, 20, 80, 20);
	set_color_register(color_registers, 4, 80, 20, 80);
	set_color_register(color_registers, 5, 20, 80, 80);
	set_color_register(color_registers, 6, 80, 80, 20);
	set_color_register(color_registers, 7, 53, 53, 53);
	set_color_register(color_registers, 8, 26, 26, 26);
	set_color_register(color_registers, 9, 33, 33, 60);
	set_color_register(color_registers, 10, 60, 26, 26);
	set_color_register(color_registers, 11, 33, 60, 33);
	set_color_register(color_registers, 12, 60, 33, 60);
	set_color_register(color_registers, 13, 33, 60, 60);
	set_color_register(color_registers, 14, 60, 60, 33);
	set_color_register(color_registers, 15, 80, 80, 80);
	break;
    case 382:			/* FIXME: verify */
	set_color_register(color_registers, 0, 0, 0, 0);
	set_color_register(color_registers, 1, 100, 100, 100);
	break;
    }

#ifdef DEBUG_PALETTE
    {
	unsigned i;

	for (i = 0U; i < MAX_COLOR_REGISTERS; i++) {
	    TRACE(("initial value for register %03u: %d,%d,%d\n",
		   i,
		   color_registers[i].r,
		   color_registers[i].g,
		   color_registers[i].b));
	}
    }
#endif
}

unsigned
get_color_register_count(TScreen const *screen)
{
    unsigned num_color_registers;

    if (screen->numcolorregisters >= 0) {
	num_color_registers = (unsigned) screen->numcolorregisters;
    } else {
	num_color_registers = 0U;
    }

    if (num_color_registers > 1U) {
	if (num_color_registers > MAX_COLOR_REGISTERS)
	    return MAX_COLOR_REGISTERS;
	return num_color_registers;
    }

    /*
     * color capabilities:
     * VK100/GIGI  1 plane (12x1 pixel attribute blocks) colorspace is 8 fixed colors (black, white, red, green, blue, cyan, yellow, magenta)
     * VT125       2 planes (4 registers) colorspace is (64?) (color), ? (grayscale)
     * VT240       2 planes (4 registers) colorspace is 4 shades (grayscale)
     * VT241       2 planes (4 registers) colorspace is ? (color), ? shades (grayscale)
     * VT330       2 planes (4 registers) colorspace is 4 shades (grayscale)
     * VT340       4 planes (16 registers) colorspace is r16g16b16 (color), 16 shades (grayscale)
     * VT382       1 plane (two fixed colors: black and white)  FIXME: verify
     * dxterm      ?
     */
    switch (screen->terminal_id) {
    case 125:
	return 4U;
    case 240:
	return 4U;
    case 241:
	return 4U;
    case 330:
	return 4U;
    case 340:
	return 16U;
    case 382:
	return 2U;
    default:
	/* unknown graphics model -- might as well be generous */
	return MAX_COLOR_REGISTERS;
    }
}

static void
init_graphic(Graphic *graphic,
	     unsigned type,
	     int terminal_id,
	     int charrow,
	     int charcol,
	     unsigned num_color_registers,
	     int private_colors)
{
    const unsigned max_pixels = (unsigned) (graphic->max_width *
					    graphic->max_height);
    unsigned i;

    TRACE(("initializing graphic object\n"));

    graphic->hidden = 0;
    graphic->dirty = 1;
    for (i = 0U; i < max_pixels; i++)
	graphic->pixels[i] = COLOR_HOLE;
    memset(graphic->color_registers_used, 0, sizeof(graphic->color_registers_used));

    /*
     * text and graphics interactions:
     * VK100/GIGI                text writes on top of graphics buffer, color attribute shared with text
     * VT240,VT241,VT330,VT340   text writes on top of graphics buffer
     * VT382                     text writes on top of graphics buffer FIXME: verify
     * VT125                     graphics buffer overlaid on top of text in B&W display, text not present in color display
     */

    /*
     * dimensions (ReGIS logical, physical):
     * VK100/GIGI  768x4??  768x240(status?)
     * VT125       768x460  768x230(+10status) (1:2 aspect ratio, ReGIS halves vertical addresses through "odd y emulation")
     * VT240       800x460  800x230(+10status) (1:2 aspect ratio, ReGIS halves vertical addresses through "odd y emulation")
     * VT241       800x460  800x230(+10status) (1:2 aspect ratio, ReGIS halves vertical addresses through "odd y emulation")
     * VT330       800x480  800x480(+?status)
     * VT340       800x480  800x480(+?status)
     * VT382       960x750  sixel only
     * dxterm      ?x? ?x?  variable?
     */

    graphic->actual_width = 0;
    graphic->actual_height = 0;

    graphic->pixw = 1;
    graphic->pixh = 1;

    graphic->valid_registers = num_color_registers;
    TRACE(("%d color registers\n", graphic->valid_registers));

    graphic->private_colors = private_colors;
    if (graphic->private_colors) {
	TRACE(("using private color registers\n"));
	init_color_registers(graphic->private_color_registers, terminal_id);
	graphic->color_registers = graphic->private_color_registers;
    } else {
	TRACE(("using shared color registers\n"));
	graphic->color_registers = getSharedRegisters();
    }

    graphic->charrow = charrow;
    graphic->charcol = charcol;
    graphic->type = type;
    graphic->valid = 0;
}

Graphic *
get_new_graphic(XtermWidget xw, int charrow, int charcol, unsigned type)
{
    TScreen const *screen = TScreenOf(xw);
    int bufferid = screen->whichBuf;
    int terminal_id = screen->terminal_id;
    Graphic *graphic;
    unsigned ii;

    FOR_EACH_SLOT(ii) {
	if ((graphic = getInactiveSlot(screen, ii))) {
	    TRACE(("using fresh graphic index=%u id=%u\n", ii, next_graphic_id));
	    break;
	}
    }

    /* if none are free, recycle the graphic scrolled back the farthest */
    if (!graphic) {
	int min_charrow = 0;
	Graphic *min_graphic = NULL;

	FOR_EACH_SLOT(ii) {
	    if (!(graphic = getActiveSlot(ii)))
		continue;
	    if (!min_graphic || graphic->charrow < min_charrow) {
		min_charrow = graphic->charrow;
		min_graphic = graphic;
	    }
	}
	TRACE(("recycling old graphic index=%u id=%u\n", ii, next_graphic_id));
	graphic = min_graphic;
    }

    if (graphic) {
	unsigned num_color_registers;
	num_color_registers = get_color_register_count(screen);
	graphic->xw = xw;
	graphic->bufferid = bufferid;
	graphic->id = next_graphic_id++;
	init_graphic(graphic,
		     type,
		     terminal_id,
		     charrow,
		     charcol,
		     num_color_registers,
		     screen->privatecolorregisters);
    }
    return graphic;
}

Graphic *
get_new_or_matching_graphic(XtermWidget xw,
			    int charrow,
			    int charcol,
			    int actual_width,
			    int actual_height,
			    unsigned type)
{
    TScreen const *screen = TScreenOf(xw);
    int bufferid = screen->whichBuf;
    Graphic *graphic;
    unsigned ii;

    FOR_EACH_SLOT(ii) {
	TRACE(("checking slot=%u for graphic at %d,%d %dx%d bufferid=%d type=%u\n", ii,
	       charrow, charcol,
	       actual_width, actual_height,
	       bufferid, type));
	if ((graphic = getActiveSlot(ii))) {
	    if (graphic->type == type &&
		graphic->bufferid == bufferid &&
		graphic->charrow == charrow &&
		graphic->charcol == charcol &&
		graphic->actual_width == actual_width &&
		graphic->actual_height == actual_height) {
		TRACE(("found existing graphic slot=%u id=%u\n", ii, graphic->id));
		return graphic;
	    }
	    TRACE(("not a match: graphic at %d,%d %dx%d bufferid=%d type=%u\n",
		   graphic->charrow, graphic->charcol,
		   graphic->actual_width, graphic->actual_height,
		   graphic->bufferid, graphic->type));
	}
    }

    /* if no match get a new graphic */
    if ((graphic = get_new_graphic(xw, charrow, charcol, type))) {
	graphic->actual_width = actual_width;
	graphic->actual_height = actual_height;
	TRACE(("no match; created graphic at %d,%d %dx%d bufferid=%d type=%u\n",
	       graphic->charrow, graphic->charcol,
	       graphic->actual_width, graphic->actual_height,
	       graphic->bufferid, graphic->type));
    }
    return graphic;
}

static int
lookup_allocated_color(const ColorRegister *reg, Pixel *pix)
{
    unsigned const rr = ((unsigned) reg->r * (LOOKUP_WIDTH - 1)) / CHANNEL_MAX;
    unsigned const gg = ((unsigned) reg->g * (LOOKUP_WIDTH - 1)) / CHANNEL_MAX;
    unsigned const bb = ((unsigned) reg->b * (LOOKUP_WIDTH - 1)) / CHANNEL_MAX;
    const AllocatedColorRegister *search;

    for (search = allocated_colors[rr][gg][bb]; search; search = search->next) {
	if (search->r == reg->r &&
	    search->g == reg->g &&
	    search->b == reg->b) {
	    *pix = search->pix;
	    return 1;
	}
    }

    *pix = 0UL;
    return 0;
}

#define ScaleForXColor(s) (unsigned short) ((long)(s) * 65535 / CHANNEL_MAX)

static int
save_allocated_color(const ColorRegister *reg, XtermWidget xw, Pixel *pix)
{
    unsigned const rr = ((unsigned) reg->r * (LOOKUP_WIDTH - 1)) / CHANNEL_MAX;
    unsigned const gg = ((unsigned) reg->g * (LOOKUP_WIDTH - 1)) / CHANNEL_MAX;
    unsigned const bb = ((unsigned) reg->b * (LOOKUP_WIDTH - 1)) / CHANNEL_MAX;
    XColor xcolor;
    AllocatedColorRegister *new_color;

    xcolor.pixel = 0UL;
    xcolor.red = ScaleForXColor(reg->r);
    xcolor.green = ScaleForXColor(reg->g);
    xcolor.blue = ScaleForXColor(reg->b);
    xcolor.flags = DoRed | DoGreen | DoBlue;
    if (!allocateBestRGB(xw, &xcolor)) {
	TRACE(("unable to allocate xcolor\n"));
	*pix = 0UL;
	return 0;
    }

    *pix = xcolor.pixel;

    if (!(new_color = malloc(sizeof(*new_color)))) {
	TRACE(("unable to save pixel %lu\n", (unsigned long) *pix));
	return 0;
    }
    new_color->r = reg->r;
    new_color->g = reg->g;
    new_color->b = reg->b;
    new_color->pix = *pix;
    new_color->next = allocated_colors[rr][gg][bb];

    allocated_colors[rr][gg][bb] = new_color;

    return 1;
}

static Pixel
color_register_to_xpixel(const ColorRegister *reg, XtermWidget xw)
{
    Pixel pix;

    if (!lookup_allocated_color(reg, &pix))
	save_allocated_color(reg, xw, &pix);

    /* FIXME: with so many possible colors we need to determine
     * when to free them to be nice to PseudoColor displays
     */
    return pix;
}

static void
refresh_graphic(TScreen const *screen,
		Graphic const *graphic,
		ColorRegister *buffer,
		int refresh_x,
		int refresh_y,
		int refresh_w,
		int refresh_h,
		int draw_x,
		int draw_y,
		int draw_w,
		int draw_h)
{
    int const pw = graphic->pixw;
    int const ph = graphic->pixh;
    int const graph_x = graphic->charcol * FontWidth(screen);
    int const graph_y = graphic->charrow * FontHeight(screen);
    int const graph_w = graphic->actual_width;
    int const graph_h = graphic->actual_height;
    int const mw = graphic->max_width;
    int r, c;
    int fillx, filly;
    int holes, total, out_of_range;
    RegisterNum regnum;

    TRACE(("refreshing graphic %u from %d,%d %dx%d (valid=%d, size=%dx%d, scale=%dx%d max=%dx%d)\n",
	   graphic->id,
	   graph_x, graph_y, draw_w, draw_h,
	   graphic->valid,
	   graphic->actual_width,
	   graphic->actual_height,
	   pw, ph,
	   graphic->max_width,
	   graphic->max_height));

    TRACE(("refresh pixmap starts at %d,%d\n", refresh_x, refresh_y));

    holes = total = 0;
    out_of_range = 0;
    for (r = 0; r < graph_h; r++) {
	int pmy = graph_y + r * ph;

	if (pmy + ph - 1 < draw_y)
	    continue;
	if (pmy > draw_y + draw_h - 1)
	    break;

	for (c = 0; c < graph_w; c++) {
	    int pmx = graph_x + c * pw;

	    if (pmx + pw - 1 < draw_x)
		continue;
	    if (pmx > draw_x + draw_w - 1)
		break;

	    total++;
	    regnum = graphic->pixels[r * mw + c];
	    if (regnum == COLOR_HOLE) {
		holes++;
		continue;
	    }

	    for (fillx = 0; fillx < pw; fillx++) {
		for (filly = 0; filly < ph; filly++) {
		    if (pmx < draw_x || pmx > draw_x + draw_w - 1 ||
			pmy < draw_y || pmy > draw_y + draw_h - 1) {
			out_of_range++;
			continue;
		    }

		    /* this shouldn't happen, but it doesn't hurt to check */
		    if (pmx < refresh_x || pmx > refresh_x + refresh_w - 1 ||
			pmy < refresh_y || pmy > refresh_y + refresh_h - 1) {
			TRACE(("OUT OF RANGE: %d,%d (%d,%d)\n", pmx, pmy, r, c));
			out_of_range++;
			continue;
		    }

		    buffer[(pmy - refresh_y) * refresh_w +
			   (pmx - refresh_x)] =
			graphic->color_registers[regnum];
		}
	    }
	}
    }

    TRACE(("done refreshing graphic: %d of %d refreshed pixels were holes; %d were out of pixmap range\n",
	   holes, total, out_of_range));
}

#ifdef DEBUG_REFRESH

#define BASEX(X) ( (draw_x - base_x) + (X) )
#define BASEY(Y) ( (draw_y - base_y) + (Y) )

static void
outline_refresh(TScreen const *screen,
		Graphic const *graphic,
		Pixmap output_pm,
		GC graphics_gc,
		int base_x,
		int base_y,
		int draw_x,
		int draw_y,
		int draw_w,
		int draw_h)
{
    Display *const display = screen->display;
    int const pw = graphic->pixw;
    int const ph = graphic->pixh;
    XGCValues xgcv;
    XColor def;

    def.red = (unsigned short) ((1.0 - 0.1 * (rand() / (double)
					      RAND_MAX) * 65535.0));
    def.green = (unsigned short) ((0.7 + 0.2 * (rand() / (double)
						RAND_MAX)) * 65535.0);
    def.blue = (unsigned short) ((0.1 + 0.1 * (rand() / (double)
					       RAND_MAX)) * 65535.0);
    def.flags = DoRed | DoGreen | DoBlue;
    if (allocateBestRGB(graphic->xw, &def)) {
	xgcv.foreground = def.pixel;
	XChangeGC(display, graphics_gc, GCForeground, &xgcv);
    }

    XDrawLine(display, output_pm, graphics_gc,
	      BASEX(0), BASEY(0),
	      BASEX(draw_w - 1), BASEY(0));
    XDrawLine(display, output_pm, graphics_gc,
	      BASEX(0), BASEY(draw_h - 1),
	      BASEX(draw_w - 1), BASEY(draw_h - 1));

    XDrawLine(display, output_pm, graphics_gc,
	      BASEX(0), BASEY(0),
	      BASEX(0), BASEY(draw_h - 1));
    XDrawLine(display, output_pm, graphics_gc,
	      BASEX(draw_w - 1), BASEY(0),
	      BASEX(draw_w - 1), BASEY(draw_h - 1));

    XDrawLine(display, output_pm, graphics_gc,
	      BASEX(draw_w - 1), BASEY(0),
	      BASEX(0), BASEY(draw_h - 1));
    XDrawLine(display, output_pm, graphics_gc,
	      BASEX(draw_w - 1), BASEY(draw_h - 1),
	      BASEX(0), BASEY(0));

    def.red = (short) (0.7 * 65535.0);
    def.green = (short) (0.1 * 65535.0);
    def.blue = (short) (1.0 * 65535.0);
    def.flags = DoRed | DoGreen | DoBlue;
    if (allocateBestRGB(graphic->xw, &def)) {
	xgcv.foreground = def.pixel;
	XChangeGC(display, graphics_gc, GCForeground, &xgcv);
    }
    XFillRectangle(display, output_pm, graphics_gc,
		   BASEX(0),
		   BASEY(0),
		   (unsigned) pw, (unsigned) ph);
    XFillRectangle(display, output_pm, graphics_gc,
		   BASEX(draw_w - 1 - pw),
		   BASEY(draw_h - 1 - ph),
		   (unsigned) pw, (unsigned) ph);
}
#endif

/*
 * Primary color hues:
 *  blue:    0 degrees
 *  red:   120 degrees
 *  green: 240 degrees
 */
void
hls2rgb(int h, int l, int s, short *r, short *g, short *b)
{
    const int hs = ((h + 240) / 60) % 6;
    const double lv = l / 100.0;
    const double sv = s / 100.0;
    double c, x, m, c2;
    double r1, g1, b1;

    if (s == 0) {
	*r = *g = *b = (short) l;
	return;
    }

    c2 = (2.0 * lv) - 1.0;
    if (c2 < 0.0)
	c2 = -c2;
    c = (1.0 - c2) * sv;
    x = (hs & 1) ? c : 0.0;
    m = lv - 0.5 * c;

    switch (hs) {
    case 0:
	r1 = c;
	g1 = x;
	b1 = 0.0;
	break;
    case 1:
	r1 = x;
	g1 = c;
	b1 = 0.0;
	break;
    case 2:
	r1 = 0.0;
	g1 = c;
	b1 = x;
	break;
    case 3:
	r1 = 0.0;
	g1 = x;
	b1 = c;
	break;
    case 4:
	r1 = x;
	g1 = 0.0;
	b1 = c;
	break;
    case 5:
	r1 = c;
	g1 = 0.0;
	b1 = x;
	break;
    default:
	TRACE(("Bad HLS input: [%d,%d,%d], returning white\n", h, l, s));
	*r = (short) 100;
	*g = (short) 100;
	*b = (short) 100;
	return;
    }

    *r = (short) ((r1 + m) * 100.0 + 0.5);
    *g = (short) ((g1 + m) * 100.0 + 0.5);
    *b = (short) ((b1 + m) * 100.0 + 0.5);

    if (*r < 0)
	*r = 0;
    else if (*r > 100)
	*r = 100;
    if (*g < 0)
	*g = 0;
    else if (*g > 100)
	*g = 100;
    if (*b < 0)
	*b = 0;
    else if (*b > 100)
	*b = 100;
}

void
dump_graphic(Graphic const *graphic)
{
#if defined(DUMP_COLORS) || defined(DUMP_BITMAP)
    RegisterNum color;
#endif
#ifdef DUMP_BITMAP
    int r, c;
    ColorRegister const *reg;
#endif

    (void) graphic;

    TRACE(("graphic stats: id=%u charrow=%d charcol=%d actual_width=%d actual_height=%d pixw=%d pixh=%d\n",
	   graphic->id,
	   graphic->charrow,
	   graphic->charcol,
	   graphic->actual_width,
	   graphic->actual_height,
	   graphic->pixw,
	   graphic->pixh));

#ifdef DUMP_COLORS
    TRACE(("graphic colors:\n"));
    for (color = 0; color < graphic->valid_registers; color++) {
	TRACE(("%03u: %d,%d,%d\n",
	       color,
	       graphic->color_registers[color].r,
	       graphic->color_registers[color].g,
	       graphic->color_registers[color].b));
    }
#endif

#ifdef DUMP_BITMAP
    TRACE(("graphic pixels:\n"));
    for (r = 0; r < graphic->actual_height; r++) {
	for (c = 0; c < graphic->actual_width; c++) {
	    color = graphic->pixels[r * graphic->max_width + c];
	    if (color == COLOR_HOLE) {
		TRACE(("?"));
	    } else {
		reg = &graphic->color_registers[color];
		if (reg->r + reg->g + reg->b > 200) {
		    TRACE(("#"));
		} else if (reg->r + reg->g + reg->b > 150) {
		    TRACE(("%%"));
		} else if (reg->r + reg->g + reg->b > 100) {
		    TRACE((":"));
		} else if (reg->r + reg->g + reg->b > 80) {
		    TRACE(("."));
		} else {
		    TRACE((" "));
		}
	    }
	}
	TRACE(("\n"));
    }

    TRACE(("\n"));
#endif
}

/* Erase the portion of any displayed graphic overlapping with a rectangle
 * of the given size and location in pixels relative to the start of the
 * graphic.  This is used to allow text to "erase" graphics underneath it.
 */
static void
erase_graphic(Graphic *graphic, int x, int y, int w, int h)
{
    RegisterNum hole = COLOR_HOLE;
    int pw, ph;
    int r, c;
    int rbase, cbase;

    pw = graphic->pixw;
    ph = graphic->pixh;

    TRACE(("erasing graphic %d,%d %dx%d\n", x, y, w, h));

    rbase = 0;
    for (r = 0; r < graphic->actual_height; r++) {
	if (rbase + ph - 1 >= y
	    && rbase <= y + h - 1) {
	    cbase = 0;
	    for (c = 0; c < graphic->actual_width; c++) {
		if (cbase + pw - 1 >= x
		    && cbase <= x + w - 1) {
		    graphic->pixels[r * graphic->max_width + c] = hole;
		}
		cbase += pw;
	    }
	}
	rbase += ph;
    }
}

static int
compare_graphic_ids(const void *left, const void *right)
{
    const Graphic *l = *(const Graphic *const *) left;
    const Graphic *r = *(const Graphic *const *) right;

    if (!l->valid || !r->valid)
	return 0;

    if (l->bufferid < r->bufferid)
	return -1;
    else if (l->bufferid > r->bufferid)
	return 1;

    if (l->id < r->id)
	return -1;
    else
	return 1;
}

static void
clip_area(int *orig_x, int *orig_y, int *orig_w, int *orig_h,
	  int clip_x, int clip_y, int clip_w, int clip_h)
{
    if (*orig_x < clip_x) {
	const int diff = clip_x - *orig_x;
	*orig_x += diff;
	*orig_w -= diff;
    }
    if (*orig_w > 0 && *orig_x + *orig_w > clip_x + clip_w) {
	*orig_w -= (*orig_x + *orig_w) - (clip_x + clip_w);
    }

    if (*orig_y < clip_y) {
	const int diff = clip_y - *orig_y;
	*orig_y += diff;
	*orig_h -= diff;
    }
    if (*orig_h > 0 && *orig_y + *orig_h > clip_y + clip_h) {
	*orig_h -= (*orig_y + *orig_h) - (clip_y + clip_h);
    }
}

/* the coordinates are relative to the screen */
static void
refresh_graphics(XtermWidget xw,
		 int leftcol,
		 int toprow,
		 int ncols,
		 int nrows,
		 int skip_clean)
{
    TScreen *const screen = TScreenOf(xw);
    Display *const display = screen->display;
    Window const drawable = VDrawable(screen);
    int const scroll_y = screen->topline * FontHeight(screen);
    int const refresh_x = leftcol * FontWidth(screen);
    int const refresh_y = toprow * FontHeight(screen) + scroll_y;
    int const refresh_w = ncols * FontWidth(screen);
    int const refresh_h = nrows * FontHeight(screen);
    int draw_x_min, draw_x_max;
    int draw_y_min, draw_y_max;
    Graphic *ordered_graphics[MAX_GRAPHICS];
    unsigned ii, jj;
    unsigned active_count;
    unsigned holes, non_holes;
    int xx, yy;
    ColorRegister *buffer;

    active_count = 0;
    FOR_EACH_SLOT(ii) {
	Graphic *graphic;
	if (!(graphic = getActiveSlot(ii)))
	    continue;
	TRACE(("refreshing graphic %d on buffer %d, current buffer %d\n",
	       graphic->id, graphic->bufferid, screen->whichBuf));
	if (screen->whichBuf == 0) {
	    if (graphic->bufferid != 0) {
		TRACE(("skipping graphic %d from alt buffer (%d) when drawing screen=%d\n",
		       graphic->id, graphic->bufferid, screen->whichBuf));
		continue;
	    }
	} else {
	    if (graphic->bufferid == 0 && graphic->charrow >= 0) {
		TRACE(("skipping graphic %d from normal buffer (%d) when drawing screen=%d because it is not in scrollback area\n",
		       graphic->id, graphic->bufferid, screen->whichBuf));
		continue;
	    }
	    if (graphic->bufferid == 1 &&
		graphic->charrow + (graphic->actual_height +
				    FontHeight(screen) - 1) /
		FontHeight(screen) < 0) {
		TRACE(("skipping graphic %d from alt buffer (%d) when drawing screen=%d because it is completely in scrollback area\n",
		       graphic->id, graphic->bufferid, screen->whichBuf));
		continue;
	    }
	}
	if (graphic->hidden)
	    continue;
	ordered_graphics[active_count++] = graphic;
    }

    if (active_count == 0)
	return;
    if (active_count > 1) {
	qsort(ordered_graphics,
	      (size_t) active_count,
	      sizeof(ordered_graphics[0]),
	      compare_graphic_ids);
    }

    if (skip_clean) {
	unsigned skip_count;

	for (jj = 0; jj < active_count; ++jj) {
	    if (ordered_graphics[jj]->dirty)
		break;
	}
	skip_count = jj;
	if (skip_count == active_count)
	    return;

	active_count -= skip_count;
	for (jj = 0; jj < active_count; ++jj) {
	    ordered_graphics[jj] = ordered_graphics[jj + skip_count];
	}
    }

    if (!(buffer = malloc(sizeof(*buffer) *
			  (unsigned) refresh_w * (unsigned) refresh_h))) {
	TRACE(("unable to allocate %dx%d buffer for graphics refresh\n",
	       refresh_w, refresh_h));
	return;
    }
    for (yy = 0; yy < refresh_h; yy++) {
	for (xx = 0; xx < refresh_w; xx++) {
	    buffer[yy * refresh_w + xx].r = -1;
	    buffer[yy * refresh_w + xx].g = -1;
	    buffer[yy * refresh_w + xx].b = -1;
	}
    }

    TRACE(("refresh: screen->topline=%d leftcol=%d toprow=%d nrows=%d ncols=%d (%d,%d %dx%d)\n",
	   screen->topline,
	   leftcol, toprow,
	   nrows, ncols,
	   refresh_x, refresh_y,
	   refresh_w, refresh_h));

    {
	int const altarea_x = 0;
	int const altarea_y = 0;
	int const altarea_w = Width(screen) * FontWidth(screen);
	int const altarea_h = Height(screen) * FontHeight(screen);

	int const scrollarea_x = 0;
	int const scrollarea_y = scroll_y;
	int const scrollarea_w = Width(screen) * FontWidth(screen);
	int const scrollarea_h = -scroll_y;

	int const mainarea_x = 0;
	int const mainarea_y = scroll_y;
	int const mainarea_w = Width(screen) * FontWidth(screen);
	int const mainarea_h = -scroll_y + Height(screen) * FontHeight(screen);

	draw_x_min = refresh_x + refresh_w;
	draw_x_max = refresh_x - 1;
	draw_y_min = refresh_y + refresh_h;
	draw_y_max = refresh_y - 1;
	for (jj = 0; jj < active_count; ++jj) {
	    Graphic *graphic = ordered_graphics[jj];
	    int draw_x = graphic->charcol * FontWidth(screen);
	    int draw_y = graphic->charrow * FontHeight(screen);
	    int draw_w = graphic->actual_width;
	    int draw_h = graphic->actual_height;

	    if (screen->whichBuf != 0) {
		if (graphic->bufferid != 0) {
		    /* clip to alt buffer */
		    clip_area(&draw_x, &draw_y, &draw_w, &draw_h,
			      altarea_x, altarea_y, altarea_w, altarea_h);
		} else {
		    /* clip to scrollback area */
		    clip_area(&draw_x, &draw_y, &draw_w, &draw_h,
			      scrollarea_x, scrollarea_y,
			      scrollarea_w, scrollarea_h);
		}
	    } else {
		/* clip to scrollback + normal area */
		clip_area(&draw_x, &draw_y, &draw_w, &draw_h,
			  mainarea_x, mainarea_y,
			  mainarea_w, mainarea_h);
	    }

	    clip_area(&draw_x, &draw_y, &draw_w, &draw_h,
		      refresh_x, refresh_y, refresh_w, refresh_h);

	    TRACE(("refresh: graph=%u\n", jj));
	    TRACE(("         refresh_x=%d refresh_y=%d refresh_w=%d refresh_h=%d\n",
		   refresh_x, refresh_y, refresh_w, refresh_h));
	    TRACE(("         draw_x=%d draw_y=%d draw_w=%d draw_h=%d\n",
		   draw_x, draw_y, draw_w, draw_h));

	    if (draw_w > 0 && draw_h > 0) {
		refresh_graphic(screen, graphic, buffer,
				refresh_x, refresh_y,
				refresh_w, refresh_h,
				draw_x, draw_y,
				draw_w, draw_h);
		if (draw_x < draw_x_min)
		    draw_x_min = draw_x;
		if (draw_x + draw_w - 1 > draw_x_max)
		    draw_x_max = draw_x + draw_w - 1;
		if (draw_y < draw_y_min)
		    draw_y_min = draw_y;
		if (draw_y + draw_h - 1 > draw_y_max)
		    draw_y_max = draw_y + draw_h - 1;
	    }
	    graphic->dirty = 0;
	}
    }

    if (draw_x_max < refresh_x ||
	draw_x_min > refresh_x + refresh_w - 1 ||
	draw_y_max < refresh_y ||
	draw_y_min > refresh_y + refresh_h - 1) {
	free(buffer);
	return;
    }

    holes = 0U;
    non_holes = 0U;
    for (yy = draw_y_min - refresh_y; yy <= draw_y_max - refresh_y; yy++) {
	for (xx = draw_x_min - refresh_x; xx <= draw_x_max - refresh_x; xx++) {
	    const ColorRegister color = buffer[yy * refresh_w + xx];
	    if (color.r < 0 || color.g < 0 || color.b < 0) {
		holes++;
	    } else {
		non_holes++;
	    }
	}
    }

    if (non_holes < 1U) {
	TRACE(("refresh: visible graphics areas are erased; nothing to do\n"));
	free(buffer);
	return;
    }

    /*
     * If we have any holes we can't just copy an image rectangle, and masking
     * with bitmaps is very expensive.  This fallback is surprisingly faster
     * than the XPutImage version in some cases, but I don't know why.
     * (This is even though there's no X11 primitive for drawing a horizontal
     * line of height one and no attempt is made to handle multiple lines at
     * once.)
     */
    if (holes > 0U) {
	GC graphics_gc;
	XGCValues xgcv;
	ColorRegister last_color;
	ColorRegister gc_color;
	int run;

	memset(&xgcv, 0, sizeof(xgcv));
	xgcv.graphics_exposures = False;
	graphics_gc = XCreateGC(display, drawable, GCGraphicsExposures, &xgcv);
	if (graphics_gc == None) {
	    TRACE(("unable to allocate GC for graphics refresh\n"));
	    free(buffer);
	    return;
	}

	last_color.r = -1;
	last_color.g = -1;
	last_color.b = -1;
	gc_color.r = -1;
	gc_color.g = -1;
	gc_color.b = -1;
	run = 0;
	for (yy = draw_y_min - refresh_y; yy <= draw_y_max - refresh_y; yy++) {
	    for (xx = draw_x_min - refresh_x; xx <= draw_x_max - refresh_x;
		 xx++) {
		const ColorRegister color = buffer[yy * refresh_w + xx];

		if (color.r < 0 || color.g < 0 || color.b < 0) {
		    last_color = color;
		    if (run > 0) {
			XDrawLine(display, drawable, graphics_gc,
				  OriginX(screen) + refresh_x + xx - run,
				  (OriginY(screen) - scroll_y) + refresh_y + yy,
				  OriginX(screen) + refresh_x + xx - 1,
				  (OriginY(screen) - scroll_y) + refresh_y + yy);
			run = 0;
		    }
		    continue;
		}

		if (color.r != last_color.r ||
		    color.g != last_color.g ||
		    color.b != last_color.b) {
		    last_color = color;
		    if (run > 0) {
			XDrawLine(display, drawable, graphics_gc,
				  OriginX(screen) + refresh_x + xx - run,
				  (OriginY(screen) - scroll_y) + refresh_y + yy,
				  OriginX(screen) + refresh_x + xx - 1,
				  (OriginY(screen) - scroll_y) + refresh_y + yy);
			run = 0;
		    }

		    if (color.r != gc_color.r ||
			color.g != gc_color.g ||
			color.b != gc_color.b) {
			xgcv.foreground =
			    color_register_to_xpixel(&color, xw);
			XChangeGC(display, graphics_gc, GCForeground, &xgcv);
			gc_color = color;
		    }
		}
		run++;
	    }
	    if (run > 0) {
		last_color.r = -1;
		last_color.g = -1;
		last_color.b = -1;
		XDrawLine(display, drawable, graphics_gc,
			  OriginX(screen) + refresh_x + xx - run,
			  (OriginY(screen) - scroll_y) + refresh_y + yy,
			  OriginX(screen) + refresh_x + xx - 1,
			  (OriginY(screen) - scroll_y) + refresh_y + yy);
		run = 0;
	    }
	}

	XFreeGC(display, graphics_gc);
    } else {
	XGCValues xgcv;
	GC graphics_gc;
	ColorRegister old_color;
	Pixel fg;
	XImage *image;
	char *imgdata;
	unsigned image_w, image_h;

	memset(&xgcv, 0, sizeof(xgcv));
	xgcv.graphics_exposures = False;
	graphics_gc = XCreateGC(display, drawable, GCGraphicsExposures, &xgcv);
	if (graphics_gc == None) {
	    TRACE(("unable to allocate GC for graphics refresh\n"));
	    free(buffer);
	    return;
	}

	/* FIXME: is it worth reusing the GC/Image/imagedata across calls? */
	/* FIXME: is it worth using shared memory when available? */
	image_w = (unsigned) draw_x_max + 1U - (unsigned) draw_x_min;
	image_h = (unsigned) draw_y_max + 1U - (unsigned) draw_y_min;
	image = XCreateImage(display, xw->visInfo->visual,
			     (unsigned) xw->visInfo->depth,
			     ZPixmap, 0, NULL,
			     image_w, image_h,
			     sizeof(int) * 8U, 0);
	if (!image) {
	    TRACE(("unable to allocate XImage for graphics refresh\n"));
	    XFreeGC(display, graphics_gc);
	    free(buffer);
	    return;
	}
	imgdata = malloc(image_h * (unsigned) image->bytes_per_line);
	if (!imgdata) {
	    TRACE(("unable to allocate XImage for graphics refresh\n"));
	    XDestroyImage(image);
	    XFreeGC(display, graphics_gc);
	    free(buffer);
	    return;
	}
	image->data = imgdata;

	fg = 0U;
	old_color.r = -1;
	old_color.g = -1;
	old_color.b = -1;
	for (yy = draw_y_min - refresh_y; yy <= draw_y_max - refresh_y; yy++) {
	    for (xx = draw_x_min - refresh_x; xx <= draw_x_max - refresh_x;
		 xx++) {
		const ColorRegister color = buffer[yy * refresh_w + xx];

		if (color.r != old_color.r ||
		    color.g != old_color.g ||
		    color.b != old_color.b) {
		    fg = color_register_to_xpixel(&color, xw);
		    old_color = color;
		}

		XPutPixel(image, xx + refresh_x - draw_x_min,
			  yy + refresh_y - draw_y_min, fg);
	    }
	}

	XPutImage(display, drawable, graphics_gc, image,
		  0, 0,
		  OriginX(screen) + draw_x_min,
		  (OriginY(screen) - scroll_y) + draw_y_min,
		  image_w, image_h);
	free(imgdata);
	image->data = NULL;
	XDestroyImage(image);
	XFreeGC(display, graphics_gc);
    }

    free(buffer);
    XFlush(display);
}

void
refresh_displayed_graphics(XtermWidget xw,
			   int leftcol,
			   int toprow,
			   int ncols,
			   int nrows)
{
    refresh_graphics(xw, leftcol, toprow, ncols, nrows, 0);
}

void
refresh_modified_displayed_graphics(XtermWidget xw)
{
    TScreen const *screen = TScreenOf(xw);
    refresh_graphics(xw, 0, 0, MaxCols(screen), MaxRows(screen), 1);
}

void
scroll_displayed_graphics(XtermWidget xw, int rows)
{
    TScreen const *screen = TScreenOf(xw);
    unsigned ii;

    TRACE(("graphics scroll: moving all up %d rows\n", rows));
    /* FIXME: VT125 ReGIS graphics are fixed at the upper left of the display; need to verify */

    FOR_EACH_SLOT(ii) {
	Graphic *graphic;

	if (!(graphic = getActiveSlot(ii)))
	    continue;
	if (graphic->bufferid != screen->whichBuf)
	    continue;
	if (graphic->hidden)
	    continue;

	graphic->charrow -= rows;
    }
}

void
pixelarea_clear_displayed_graphics(TScreen const *screen,
				   int winx,
				   int winy,
				   int w,
				   int h)
{
    unsigned ii;

    FOR_EACH_SLOT(ii) {
	Graphic *graphic;
	/* FIXME: are these coordinates (scrolled) screen-relative? */
	int const scroll_y = (screen->whichBuf == 0
			      ? screen->topline * FontHeight(screen)
			      : 0);
	int graph_x;
	int graph_y;
	int x, y;

	if (!(graphic = getActiveSlot(ii)))
	    continue;
	if (graphic->bufferid != screen->whichBuf)
	    continue;
	if (graphic->hidden)
	    continue;

	graph_x = graphic->charcol * FontWidth(screen);
	graph_y = graphic->charrow * FontHeight(screen);
	x = winx - graph_x;
	y = (winy - scroll_y) - graph_y;

	TRACE(("pixelarea clear graphics: screen->topline=%d winx=%d winy=%d w=%d h=%d x=%d y=%d\n",
	       screen->topline,
	       winx, winy,
	       w, h,
	       x, y));
	erase_graphic(graphic, x, y, w, h);
    }
}

void
chararea_clear_displayed_graphics(TScreen const *screen,
				  int leftcol,
				  int toprow,
				  int ncols,
				  int nrows)
{
    int const x = leftcol * FontWidth(screen);
    int const y = toprow * FontHeight(screen);
    int const w = ncols * FontWidth(screen);
    int const h = nrows * FontHeight(screen);

    TRACE(("chararea clear graphics: screen->topline=%d leftcol=%d toprow=%d nrows=%d ncols=%d x=%d y=%d w=%d h=%d\n",
	   screen->topline,
	   leftcol, toprow,
	   nrows, ncols,
	   x, y, w, h));
    pixelarea_clear_displayed_graphics(screen, x, y, w, h);
}

void
reset_displayed_graphics(TScreen const *screen)
{
    unsigned ii;

    init_color_registers(getSharedRegisters(), screen->terminal_id);

    TRACE(("resetting all graphics\n"));
    FOR_EACH_SLOT(ii) {
	deactivateSlot(ii);
    }
}

#ifdef NO_LEAKS
void
noleaks_graphics(void)
{
    unsigned ii;

    FOR_EACH_SLOT(ii) {
	deactivateSlot(ii);
    }
}
#endif
