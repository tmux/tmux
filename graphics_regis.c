/* $XTermId: graphics_regis.c,v 1.101 2017/12/30 15:06:36 tom Exp $ */

/*
 * Copyright 2014-2016,2017 by Ross Combs
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
#include <math.h>
#include <stdlib.h>

#if OPT_DOUBLE_BUFFER
#include <X11/extensions/Xdbe.h>
#endif

#include <fontutils.h>
#include <ptyx.h>

#include <assert.h>
#include <graphics.h>
#include <graphics_regis.h>

/* get rid of shadowing warnings (we will not draw Bessel functions) */
#define y1 my_y1
#define y0 my_y0

#define IS_HEX_DIGIT(CH) ( \
  (CH) == '0' || \
  (CH) == '1' || \
  (CH) == '2' || \
  (CH) == '3' || \
  (CH) == '4' || \
  (CH) == '5' || \
  (CH) == '6' || \
  (CH) == '7' || \
  (CH) == '8' || \
  (CH) == '9' || \
  (CH) == 'a' || \
  (CH) == 'b' || \
  (CH) == 'c' || \
  (CH) == 'd' || \
  (CH) == 'e' || \
  (CH) == 'f' || \
  (CH) == 'A' || \
  (CH) == 'B' || \
  (CH) == 'C' || \
  (CH) == 'D' || \
  (CH) == 'E' || \
  (CH) == 'F' )

#define SCALE_FIXED_POINT 16U

#undef DEBUG_PARSING
#undef DEBUG_ALPHABET_LOOKUP
#undef DEBUG_ALPHABETS
#undef DEBUG_BEZIER
#undef DEBUG_SPLINE_SEGMENTS
#undef DEBUG_SPLINE_POINTS
#undef DEBUG_SPLINE_WITH_ROTATION
#undef DEBUG_SPLINE_WITH_OVERDRAW
#undef DEBUG_ARC_POINTS
#undef DEBUG_ARC_CENTER
#undef DEBUG_ARC_START
#undef DEBUG_ARC_END
#undef DEBUG_SPECIFIC_CHAR_METRICS
#define IS_DEBUG_CHAR(CH) ((CH) == 'W')		/* glyphs to dump to terminal */
#undef DEBUG_COMPUTED_FONT_METRICS
#undef DEBUG_FONT_NAME
#undef DEBUG_FONT_SIZE_SEARCH
#undef DEBUG_XFT_GLYPH
#undef DEBUG_USER_GLYPH
#undef DEBUG_LOAD

/* controls for extensions over VT3x0 limitations */
#define ENABLE_RGB_COLORSPECS
#undef ENABLE_FREE_ROTATION
#undef ENABLE_DISTORTIONLESS_ROTATION
#define ENABLE_UPLOAD_ALPHABET_FROM_FONT
#define ENABLE_UPLOAD_ALPHABET_ZERO
#define ENABLE_USER_FONT_SIZE
#define ENABLE_VARIABLE_ITALICS

#define MIN_ITERATIONS_BEFORE_REFRESH 10U
#define MIN_MS_BEFORE_REFRESH 33
/* *INDENT-OFF* */
typedef struct RegisPoint {
    int  x, y;
} RegisPoint;

typedef struct RegisWriteControls {
    unsigned     pv_multiplier;
    unsigned     pattern;
    unsigned     pattern_multiplier;
    unsigned     invert_pattern;
    unsigned     plane_mask;
    unsigned     write_style;
    RegisterNum  foreground;
    unsigned     shading_enabled;
    char         shading_character;
    int          shading_reference;
    unsigned     shading_reference_dim;
    unsigned     line_width;
} RegisWriteControls;

typedef struct RegisTextControls {
    unsigned  alphabet_num;
    unsigned  character_set_l; /* default: "(B" (ASCII) */
    unsigned  character_set_r; /* default: "-@" (Latin-1) */
    unsigned  character_display_w;
    unsigned  character_display_h;
    unsigned  character_unit_cell_w;
    unsigned  character_unit_cell_h;
    int       character_inc_x;
    int       character_inc_y;
    int       string_rotation;
    int       character_rotation;
    int       slant; /* for italic/oblique */
} RegisTextControls;

#define FixedCopy(dst, src, len) strncpy(dst, src, len - 1)[len - 1] = '\0'
#define CopyFontname(dst, src) FixedCopy(dst, src, REGIS_FONTNAME_LEN)

#define MAX_REGIS_PAGES 8U

#define MAX_REGIS_ALPHABETS 8U
#define REGIS_ALPHABET_NAME_LEN 11U
#define REGIS_FONTNAME_LEN 256U
/* enough for a 16x24 font (about 100KB) */
#define MAX_REGIS_ALPHABET_BYTES (256U * 16U * 24U)
#define MAX_GLYPH_PIXELS 8192U
#define MAX_GLYPHS 256U
#define INVALID_ALPHABET_NUM ~0U

typedef struct RegisAlphabet {
    unsigned       alphabet_num;
    unsigned       pixw, pixh;
    char           name[REGIS_ALPHABET_NAME_LEN];
    char           fontname[REGIS_FONTNAME_LEN];
    int            use_font;
    int            loaded[MAX_GLYPHS];
    unsigned char *bytes;
} RegisAlphabet;

typedef struct RegisDataFragment {
    char const  *start;
    unsigned     pos;
    unsigned     len;
} RegisDataFragment;
/* *INDENT-ON* */

#define POSITION_STACK_SIZE 16U
#define DUMMY_STACK_X -32768
#define DUMMY_STACK_Y -32768

#define CURVE_POSITION_ARC_EDGE     0U
#define CURVE_POSITION_ARC_CENTER   1U
#define CURVE_POSITION_OPEN_CURVE   2U
#define CURVE_POSITION_CLOSED_CURVE 3U

#define MAX_INPUT_CURVE_POINTS 16U
#define MAX_CURVE_POINTS (MAX_INPUT_CURVE_POINTS + 4U)

#define MAX_FILL_POINTS 2048U

typedef struct RegisParseState {
    RegisDataFragment input;
    char *temp;
    unsigned templen;
    char command;
    char option;
    /* position stack */
    int stack_x[POSITION_STACK_SIZE];
    int stack_y[POSITION_STACK_SIZE];
    unsigned stack_next;	/* next empty position */
    /* curve options */
    int curve_mode;
    int arclen;
    int x_points[MAX_CURVE_POINTS];
    int y_points[MAX_CURVE_POINTS];
    unsigned num_points;
    /* load options */
    char load_name[REGIS_ALPHABET_NAME_LEN];
    unsigned load_alphabet;
    unsigned load_w, load_h;
    unsigned load_index;
    unsigned load_glyph;
    unsigned load_row;
    /* text options */
    unsigned text_tilt_state;
} RegisParseState;

#define TEXT_TILT_STATE_READY    0U
#define TEXT_TILT_STATE_GOT_D    1U
#define TEXT_TILT_STATE_GOT_DS   2U
#define TEXT_TILT_STATE_GOT_DSD  3U

typedef struct RegisGraphicsContext {
    XtermWidget current_widget;
    Graphic *destination_graphic;
    Graphic *display_graphic;
    int terminal_id;
    int x_off, y_off;
    int x_div, y_div;
    int width, height;
    unsigned all_planes;
    RegisterNum background;
    char const *builtin_font;
    RegisAlphabet alphabets[MAX_REGIS_ALPHABETS];
    RegisWriteControls persistent_write_controls;
    RegisWriteControls temporary_write_controls;
    RegisTextControls persistent_text_controls;
    RegisTextControls temporary_text_controls;
    RegisTextControls *current_text_controls;
    int multi_input_mode;
    int graphics_output_cursor_x;
    int graphics_output_cursor_y;
    unsigned pattern_count;
    unsigned pattern_bit;
    int fill_mode;
    RegisPoint fill_points[MAX_FILL_POINTS];
    unsigned fill_point_count;
    unsigned destination_page;
    unsigned display_page;
    int force_refresh;
} RegisGraphicsContext;

static RegisGraphicsContext persistent_context;
static RegisParseState persistent_state;

#define MAX_PATTERN_BITS 8U

#define WRITE_STYLE_OVERLAY 1U
#define WRITE_STYLE_REPLACE 2U
#define WRITE_STYLE_COMPLEMENT 3U
#define WRITE_STYLE_ERASE 4U

#define WRITE_SHADING_REF_Y 0U
#define WRITE_SHADING_REF_X 1U
#define WRITE_SHADING_REF_NONE 2U

/* keypress event example: http://iraf.net/forum/viewtopic.php?showtopic=61692 */

#define MIN2(X, Y) ( (X) < (Y) ? (X) : (Y) )
#define MIN3(X, Y, Z) ( MIN2(MIN2((X), (Y)), MIN2((Y), (Z))) )
#define MAX2(X, Y) ( (X) > (Y) ? (X) : (Y) )
#define MAX3(X, Y, Z) ( MAX2(MAX2((X), (Y)), MAX2((Y), (Z))) )

#define ROT_LEFT_N(V, N) ( (((V) << ((N) & 3U )) & 255U) | \
			   ((V) >> (8U - ((N) & 3U))) )
#define ROT_LEFT(V) ( (((V) << 1U) & 255U) | ((V) >> 7U) )

/* convert user coordinates to absolute pixel coordinates */
#define SCALE_XCOORD(C, X, S) ( ( (X) * ((C)->width  - 1) ) / ( (C)->x_div * (S) ) )
#define SCALE_YCOORD(C, Y, S) ( ( (Y) * ((C)->height - 1) ) / ( (C)->y_div * (S) ) )
#define TRANSLATE_XCOORD(C, X, S) SCALE_XCOORD((C), (X) - (C)->x_off * (S), (S) )
#define TRANSLATE_YCOORD(C, Y, S) SCALE_YCOORD((C), (Y) - (C)->y_off * (S), (S) )

#if 0
/* convert absolute pixel coordinate to user coordinates */
#define SCALE_XPIX(C, X, S) ( ( (X) * ((C)->x_div * (S) ) ) / ((C)->width  - 1) )
#define SCALE_YPIX(C, Y, S) ( ( (Y) * ((C)->y_div * (S) ) ) / ((C)->height - 1) )
#define TRANSLATE_XPIX(C, X, S) ( SCALE_XPIX((C), (X), (S) ) + (C)->x_off * (S) )
#define TRANSLATE_YPIX(C, Y, S) ( SCALE_YPIX((C), (Y), (S) ) + (C)->y_off * (S) )
#endif

#define READ_PIXEL(C, X, Y) read_pixel((C)->destination_graphic, (X), (Y))
#define DRAW_PIXEL(C, X, Y, COL) draw_solid_pixel((C)->destination_graphic, (X), (Y), (COL))
#define DRAW_ALL(C, COL) \
    draw_solid_rectangle((C)->destination_graphic, 0, 0, (C)->width, (C)->height, (COL))

static unsigned get_shade_character_pixel(unsigned char const *pixels,
					  unsigned w, unsigned h,
					  unsigned smaxf, unsigned scale,
					  int slant_dx, int px, int py);
static void get_bitmap_of_character(RegisGraphicsContext const *context,
				    int ch, unsigned maxw, unsigned maxh,
				    unsigned char *pixels,
				    unsigned *w, unsigned *h,
				    unsigned max_pixels);

static void
init_regis_load_state(RegisParseState *state)
{
    state->load_index = MAX_REGIS_ALPHABETS;
    state->load_w = 8U;
    state->load_h = 10U;
    state->load_alphabet = 1U;	/* FIXME: is this the correct default */
    state->load_name[0] = '\0';
    state->load_glyph = (unsigned) (unsigned char) '\0';
    state->load_row = 0U;
}

static void
init_regis_parse_state(RegisParseState *state)
{
    state->command = '_';
    state->option = '_';
    state->stack_next = 0U;
    state->load_index = MAX_REGIS_ALPHABETS;
    init_regis_load_state(state);
}

static int
ifloor(double d)
{
    double dl = floor(d);
    return (int) dl;
}

static int
isqrt(double d)
{
    double dl = sqrt(d);
    return (int) dl;
}

static void
draw_regis_pixel(RegisGraphicsContext *context, int x, int y,
		 unsigned value)
{
    unsigned color = 0;

    switch (context->temporary_write_controls.write_style) {
    case WRITE_STYLE_OVERLAY:
	/*
	 * Update pixels with foreground when pattern is 1,
	 * don't change when pattern is 0.
	 */
	if (!value) {
	    return;
	}

	if (context->temporary_write_controls.invert_pattern) {
	    color = context->background;
	} else {
	    color = context->temporary_write_controls.foreground;
	}
	break;

    case WRITE_STYLE_REPLACE:
	/*
	 * Update pixels with foreground when pattern is 1,
	 * set to background when pattern is 0.
	 */
	{
	    unsigned fg, bg;

	    if (context->temporary_write_controls.invert_pattern) {
		fg = context->background;
		bg = context->temporary_write_controls.foreground;
	    } else {
		fg = context->temporary_write_controls.foreground;
		bg = context->background;
	    }
	    color = value ? fg : bg;
	}
	break;

    case WRITE_STYLE_COMPLEMENT:
	/*
	 * Update pixels with background when pattern is 1,
	 * don't change when pattern is 0.
	 */
	if (!value) {
	    return;
	}

	color = READ_PIXEL(context, x, y);
	if (color == COLOR_HOLE)
	    color = context->background;
	color = color ^ context->all_planes;
	break;

    case WRITE_STYLE_ERASE:
	/* Update pixels to foreground. */
	if (context->temporary_write_controls.invert_pattern) {
	    color = context->temporary_write_controls.foreground;
	} else {
	    color = context->background;
	}
	break;
    }

    if (context->temporary_write_controls.plane_mask != context->all_planes) {
	unsigned old_color = READ_PIXEL(context, x, y);
	if (old_color == COLOR_HOLE)
	    old_color = context->background;
	color = (color & context->temporary_write_controls.plane_mask) |
	    (old_color & ~context->temporary_write_controls.plane_mask);
    }

    DRAW_PIXEL(context, x, y, color);
}

static void
shade_pattern_to_pixel(RegisGraphicsContext *context, unsigned dim, int ref,
		       int x, int y)
{
    unsigned value;

    if (dim == WRITE_SHADING_REF_X) {
	int delta = x > ref ? 1 : -1;
	int curr_x;

	context->pattern_bit = 1U << (((unsigned) y) & 7U);
	for (curr_x = ref; curr_x != x + delta; curr_x += delta) {
	    value = context->temporary_write_controls.pattern &
		context->pattern_bit;
	    draw_regis_pixel(context, curr_x, y, value);
	}
    } else if (dim == WRITE_SHADING_REF_Y) {
	int delta = y > ref ? 1 : -1;
	int curr_y;

	for (curr_y = ref; curr_y != y + delta; curr_y += delta) {
	    context->pattern_bit = 1U << (((unsigned) curr_y) & 7U);
	    value = context->temporary_write_controls.pattern &
		context->pattern_bit;
	    draw_regis_pixel(context, x, curr_y, value);
	}
    } else {
	TRACE(("ERROR: shading requested, but there is no reference axis\n"));
    }
}

static void
shade_char_to_pixel(RegisGraphicsContext *context, unsigned char const *pixels,
		    unsigned w, unsigned h, unsigned dim, int ref, int x, int y)
{
    unsigned xmaxf = context->current_text_controls->character_unit_cell_w;
    unsigned ymaxf = context->current_text_controls->character_unit_cell_h;
    unsigned smaxf;
    unsigned s;
    unsigned scale;
    unsigned value;

    if (xmaxf > ymaxf) {
	smaxf = ymaxf;
	s = h;
    } else {
	smaxf = xmaxf;
	s = w;
    }
    scale = (s << SCALE_FIXED_POINT) / smaxf;

    if (dim == WRITE_SHADING_REF_X) {
	int delta = x > ref ? 1 : -1;
	int curr_x;

	for (curr_x = ref; curr_x != x + delta; curr_x += delta) {
	    value = get_shade_character_pixel(pixels, w, h, smaxf, scale, 0,
					      curr_x, y);
	    draw_regis_pixel(context, curr_x, y, value);
	}
    } else if (dim == WRITE_SHADING_REF_Y) {
	int delta = y > ref ? 1 : -1;
	int curr_y;

	for (curr_y = ref; curr_y != y + delta; curr_y += delta) {
	    value = get_shade_character_pixel(pixels, w, h, smaxf, scale, 0, x,
					      curr_y);
	    draw_regis_pixel(context, x, curr_y, value);
	}
    } else {
	TRACE(("ERROR: shading requested, but there is no reference axis\n"));
    }
}

static void
draw_patterned_pixel(RegisGraphicsContext *context, int x, int y)
{
    if (context->pattern_count >=
	context->temporary_write_controls.pattern_multiplier) {
	context->pattern_count = 0U;
	context->pattern_bit = ROT_LEFT(context->pattern_bit);
    }
    context->pattern_count++;

    draw_regis_pixel(context, x, y,
		     context->temporary_write_controls.pattern &
		     context->pattern_bit);
}

static void
shade_to_pixel(RegisGraphicsContext *context, unsigned dim, int ref,
	       int x, int y)
{
    if (context->temporary_write_controls.shading_character != '\0') {
	unsigned xmaxf = context->current_text_controls->character_unit_cell_w;
	unsigned ymaxf = context->current_text_controls->character_unit_cell_h;
	char ch = context->temporary_write_controls.shading_character;
	unsigned char pixels[MAX_GLYPH_PIXELS];
	unsigned w, h;

	get_bitmap_of_character(context, ch, xmaxf, ymaxf, pixels, &w, &h,
				MAX_GLYPH_PIXELS);
	if (w > 0 && h > 0) {
	    shade_char_to_pixel(context, pixels, w, h, dim, ref, x, y);
	}
    } else {
	shade_pattern_to_pixel(context, dim, ref, x, y);
    }
}

static void
draw_or_save_patterned_pixel(RegisGraphicsContext *context, int x, int y)
{
    if (context->fill_mode == 1) {
	if (context->fill_point_count >= MAX_FILL_POINTS) {
	    TRACE(("point %d,%d can not be added to filled polygon\n",
		   x, y));
	    return;
	}
	if (context->fill_point_count > 0U &&
	    context->fill_points[context->fill_point_count - 1U].x == x &&
	    context->fill_points[context->fill_point_count - 1U].y == y) {
	    return;
	}
	context->fill_points[context->fill_point_count].x = x;
	context->fill_points[context->fill_point_count].y = y;
	context->fill_point_count++;
	return;
    }

    if (context->temporary_write_controls.shading_enabled) {
	unsigned dim = context->temporary_write_controls.shading_reference_dim;
	int ref = context->temporary_write_controls.shading_reference;

	shade_to_pixel(context, dim, ref, x, y);
	return;
    }

    draw_patterned_pixel(context, x, y);
}

static int
sort_points(void const *l, void const *r)
{
    RegisPoint const *const lp = l;
    RegisPoint const *const rp = r;

    if (lp->y < rp->y)
	return -1;
    if (lp->y > rp->y)
	return +1;
    if (lp->x < rp->x)
	return -1;
    if (lp->x > rp->x)
	return +1;
    return 0;
}

static void
draw_filled_polygon(RegisGraphicsContext *context)
{
    unsigned p;
    int old_x, old_y;
    int inside;
    unsigned char pixels[MAX_GLYPH_PIXELS];
    unsigned w = 1, h = 1;

    if (context->temporary_write_controls.shading_character != '\0') {
	char ch = context->temporary_write_controls.shading_character;
	unsigned xmaxf = context->current_text_controls->character_unit_cell_w;
	unsigned ymaxf = context->current_text_controls->character_unit_cell_h;

	get_bitmap_of_character(context, ch, xmaxf, ymaxf, pixels, &w, &h,
				MAX_GLYPH_PIXELS);
	if (w < 1U || h < 1U) {
	    return;
	}
    }

    qsort(context->fill_points, context->fill_point_count,
	  sizeof(context->fill_points[0]), sort_points);

    old_x = DUMMY_STACK_X;
    old_y = DUMMY_STACK_Y;
    inside = 0;
    for (p = 0U; p < context->fill_point_count; p++) {
	int new_x = context->fill_points[p].x;
	int new_y = context->fill_points[p].y;
#if 0
	printf("got %d,%d (%d,%d) inside=%d\n", new_x, new_y, old_x, old_y, inside);
#endif

	/*
	 * FIXME: This is using pixels to represent lines which loses
	 * information about exact slope and how many lines are present which
	 * causes misbehavior with some inputs (especially complex polygons).
	 * It also takes more room than remembering vertices, but I'd rather
	 * not have to implement line segments for arcs.  Maybe store a count
	 * at each vertex instead (doesn't fix the slope problem).
	 */
	/*
	 * FIXME: Change this to only draw inside of polygons, and round
	 * points in a uniform direction to avoid overlapping drawing.  As an
	 * option we could continue to support drawing the outline.
	 */
	if (new_y != old_y) {
	    if (inside) {
		/*
		 * Just draw the vertical line when there is not a matching
		 * edge on the right side.
		 */
		if (context->temporary_write_controls.shading_character != '\0') {
		    shade_char_to_pixel(context, pixels, w, h,
					WRITE_SHADING_REF_X,
					old_x, old_x, old_y);
		} else {
		    shade_pattern_to_pixel(context, WRITE_SHADING_REF_X,
					   old_x, old_x, old_y);
		}
	    }
	    inside = 1;
	} else {
	    if (inside) {
		if (context->temporary_write_controls.shading_character != '\0') {
		    shade_char_to_pixel(context, pixels, w, h,
					WRITE_SHADING_REF_X,
					old_x, new_x, new_y);
		} else {
		    shade_pattern_to_pixel(context, WRITE_SHADING_REF_X,
					   old_x, new_x, new_y);
		}
	    }
	    if (new_x > old_x + 1) {
		inside = !inside;
	    }
	}

	old_x = new_x;
	old_y = new_y;
    }

    context->destination_graphic->dirty = 1;
}

static void
draw_patterned_line(RegisGraphicsContext *context, int x1, int y1,
		    int x2, int y2)
{
    int x, y;
    int dx, dy;
    int dir, diff;

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
	    draw_or_save_patterned_pixel(context, x, y);
	}
    } else {
	if (y1 > y2) {
	    int tmp;
	    EXCHANGE(y1, y2, tmp);
	    EXCHANGE(x1, x2, tmp);
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
	    draw_or_save_patterned_pixel(context, x, y);
	}
    }

    context->destination_graphic->dirty = 1;
}

typedef struct {
    int dxx;
    int dxy;
    int dyx;
    int dyy;
} quadmap_coords;

static void
draw_patterned_arc(RegisGraphicsContext *context,
		   int cx, int cy,
		   int ex, int ey,
		   int a_start, int a_length,
		   int *ex_final, int *ey_final)
{
    const double third = hypot((double) (cx - ex), (double) (cy - ey));
    const int radius = (int) third;
    const int ra = radius;
    const int rb = radius;
    const quadmap_coords neg_quadmap[4] =
    {
	{-1, 0, 0, +1},
	{0, -1, -1, 0},
	{+1, 0, 0, -1},
	{0, +1, +1, 0},
    };
    const quadmap_coords pos_quadmap[4] =
    {
	{-1, 0, 0, -1},
	{0, -1, +1, 0},
	{+1, 0, 0, +1},
	{0, +1, -1, 0},
    };
    const quadmap_coords *quadmap;
    int total_points;
    int half_degree;
    int points_start, points_stop;
    int points;
    unsigned iterations;
    long rx, ry;
    long dx, dy;
    int x, y;
    long e2;
    long error;

    TRACE(("orig a_length=%d a_start=%d\n", a_length, a_start));
    if (a_length == 0)
	return;
    if (a_length > 0) {
	quadmap = pos_quadmap;
    } else {
	quadmap = neg_quadmap;
	if (a_start != 0)
	    a_start = 3600 - a_start;
	a_length = abs(a_length);
    }
    TRACE(("positive a_length=%d a_start=%d\n", a_length, a_start));

    rx = -ra;
    ry = 0;
    e2 = rb;
    dx = (2 * rx + 1) * e2 * e2;
    dy = rx * rx;
    error = dx + dy;
    total_points = 0;
    do {
	total_points += 4;
	e2 = 2 * error;
	if (e2 >= dx) {
	    rx++;
	    dx += 2 * rb * rb;
	    error += dx;
	}
	if (e2 <= dy) {
	    ry++;
	    dy += 2 * ra * ra;
	    error += dy;
	}
    }
    while (rx <= 0);

    /* FIXME: This is apparently not accurate enough because some arcs start or
     * end a few pixels off.  Maybe compare line slopes in the loop below
     * instead?
     */
    half_degree = total_points * 5;
    points_start = (total_points * a_start - half_degree) / 3600;
    points_stop = (total_points * a_start +
		   total_points * a_length + half_degree) / 3600;
    TRACE(("drawing arc with %d points clockwise from %g degrees for %g degrees (from point %d to %d out of %d)\n",
	   total_points, a_start / 10.0, a_length / 10.0, points_start, points_stop,
	   total_points));

    /* FIXME: The four pixels at the cardinal directions are double-drawn. */
    points = 0;
    for (iterations = 0U; iterations < 8U; iterations++) {
	int q2 = iterations & 0x3;

	rx = -ra;
	ry = 0;
	e2 = rb;
	dx = (2 * rx + 1) * e2 * e2;
	dy = rx * rx;
	error = dx + dy;
	do {
#ifdef DEBUG_ARC_POINTS
	    double rad = atan2(
				  (double) (quadmap[q2].dyx * rx +
					    quadmap[q2].dyy * ry),
				  (double) (quadmap[q2].dxx * rx +
					    quadmap[q2].dxy * ry));
	    double deg = (360.0 * rad / (2.0 * M_PI));
	    if (deg < 0.0)
		deg += 360.0;
#endif

	    if (points >= points_start && points <= points_stop) {
		x = (int) (cx +
			   quadmap[q2].dxx * rx +
			   quadmap[q2].dxy * ry);
		y = (int) (cy +
			   quadmap[q2].dyx * rx +
			   quadmap[q2].dyy * ry);
#ifdef DEBUG_ARC_POINTS
		TRACE(("drawing point %u at %d,%d (%.5g deg)\n",
		       points, x, y, deg));
#endif
		draw_or_save_patterned_pixel(context, x, y);
		if (ex_final)
		    *ex_final = x;
		if (ey_final)
		    *ey_final = y;
	    } else {
#ifdef DEBUG_ARC_POINTS
		x = (int) (cx + quadmap[q2].dxx * rx + quadmap[q2].dxy * ry);
		y = (int) (cy + quadmap[q2].dyx * rx + quadmap[q2].dyy * ry);
		TRACE(("skipping point %u at %d,%d which is outside of range (%.5g deg)\n",
		       points, x, y, deg));
#endif
	    }
	    points++;

	    e2 = 2 * error;
	    if (e2 >= dx) {
		rx++;
		dx += 2 * rb * rb;
		error += dx;
	    }
	    if (e2 <= dy) {
		ry++;
		dy += 2 * ra * ra;
		error += dy;
	    }
	}
	while (rx <= 0);
    }

    context->destination_graphic->dirty = 1;
}

/*
 * The plot* functions are based on optimized rasterization primitves written by Zingl Alois.
 * See http://members.chello.at/easyfilter/bresenham.html
 */

/*
 * FIXME:
 * This is a terrible temporary hack.  The plot functions below can be adapted
 * to work like the other rasterization functions but there's no point in doing
 * that until we know we don't have to write something completely different.
 */
static RegisGraphicsContext *global_context;
static void
setPixel(int x, int y)
{
    draw_or_save_patterned_pixel(global_context, x, y);
}

static void
plotLine(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;		/* error value e_xy */

    for (;;) {			/* loop */
	int e2;
	setPixel(x0, y0);
	e2 = 2 * err;
	if (e2 >= dy) {		/* e_xy+e_x > 0 */
	    if (x0 == x1)
		break;
	    err += dy;
	    x0 += sx;
	}
	if (e2 <= dx) {		/* e_xy+e_y < 0 */
	    if (y0 == y1)
		break;
	    err += dx;
	    y0 += sy;
	}
    }
}

static void
plotQuadBezierSeg(int x0, int y0, int x1, int y1, int x2, int y2)
{				/* plot a limited quadratic Bezier segment */
    int sx = x2 - x1;
    int sy = y2 - y1;
    long xx = (x0 - x1);	/* relative values for checks */
    long yy = (y0 - y1);
    double cur = (double) (xx * sy - yy * sx);	/* curvature */

    assert(xx * sx <= 0 && yy * sy <= 0);	/* sign of gradient must not change */

    if (sx * (long) sx + sy * (long) sy > xx * xx + yy * yy) {	/* begin with longer part */
	x2 = x0;
	x0 = sx + x1;
	y2 = y0;
	y0 = sy + y1;
	cur = -cur;		/* swap P0 P2 */
    }
    if (cur != 0) {		/* no straight line */
	long xy;
	double dx, dy, err;

	xx += sx;
	xx *= sx = x0 < x2 ? 1 : -1;	/* x step direction */
	yy += sy;
	yy *= sy = y0 < y2 ? 1 : -1;	/* y step direction */
	xy = 2 * xx * yy;
	xx *= xx;
	yy *= yy;		/* differences 2nd degree */
	if (cur * sx * sy < 0) {	/* negated curvature? */
	    xx = -xx;
	    yy = -yy;
	    xy = -xy;
	    cur = -cur;
	}
	/* differences 1st degree */
	dx = ((4.0 * sy * cur * (x1 - x0)) + (double) xx) - (double) xy;
	dy = ((4.0 * sx * cur * (y0 - y1)) + (double) yy) - (double) xy;
	xx += xx;
	yy += yy;
	err = dx + dy + (double) xy;	/* error 1st step */
	do {
	    setPixel(x0, y0);	/* plot curve */
	    if (x0 == x2 && y0 == y2)
		return;		/* last pixel -> curve finished */
	    y1 = (2 * err) < dx;	/* save value for test of y step */
	    if ((2 * err) > dy) {
		x0 += sx;
		dx -= (double) xy;
		dy += (double) yy;
		err += dy;
	    }			/* x step */
	    if (y1) {
		y0 += sy;
		dy -= (double) xy;
		dx += (double) xx;
		err += dx;
	    }			/* y step */
	} while (dy < 0 && dx > 0);	/* gradient negates -> algorithm fails */
    }
    plotLine(x0, y0, x2, y2);	/* plot remaining part to end */
}

#if 0
static void
plotQuadBezier(int x0, int y0, int x1, int y1, int x2, int y2)
{				/* plot any quadratic Bezier curve */
    int x = x0 - x1;
    int y = y0 - y1;
    double t = x0 - 2 * x1 + x2;
    double r;

    if ((long) x * (x2 - x1) > 0) {	/* horizontal cut at P4? */
	if ((long) y * (y2 - y1) > 0)	/* vertical cut at P6 too? */
	    if (fabs((y0 - 2 * y1 + y2) / t * x) > abs(y)) {	/* which first? */
		x0 = x2;
		x2 = x + x1;
		y0 = y2;
		y2 = y + y1;	/* swap points */
	    }			/* now horizontal cut at P4 comes first */
	t = (x0 - x1) / t;
	r = (1 - t) * ((1 - t) * y0 + 2.0 * t * y1) + t * t * y2;	/* By(t=P4) */
	t = (x0 * x2 - x1 * x1) * t / (x0 - x1);	/* gradient dP4/dx=0 */
	x = ifloor(t + 0.5);
	y = ifloor(r + 0.5);
	r = (y1 - y0) * (t - x0) / (x1 - x0) + y0;	/* intersect P3 | P0 P1 */
	plotQuadBezierSeg(x0, y0, x, ifloor(r + 0.5), x, y);
	r = (y1 - y2) * (t - x2) / (x1 - x2) + y2;	/* intersect P4 | P1 P2 */
	x0 = x1 = x;
	y0 = y;
	y1 = ifloor(r + 0.5);	/* P0 = P4, P1 = P8 */
    }
    if ((long) (y0 - y1) * (y2 - y1) > 0) {	/* vertical cut at P6? */
	t = y0 - 2 * y1 + y2;
	t = (y0 - y1) / t;
	r = (1 - t) * ((1 - t) * x0 + 2.0 * t * x1) + t * t * x2;	/* Bx(t=P6) */
	t = (y0 * y2 - y1 * y1) * t / (y0 - y1);	/* gradient dP6/dy=0 */
	x = ifloor(r + 0.5);
	y = ifloor(t + 0.5);
	r = (x1 - x0) * (t - y0) / (y1 - y0) + x0;	/* intersect P6 | P0 P1 */
	plotQuadBezierSeg(x0, y0, ifloor(r + 0.5), y, x, y);
	r = (x1 - x2) * (t - y2) / (y1 - y2) + x2;	/* intersect P7 | P1 P2 */
	x0 = x;
	x1 = ifloor(r + 0.5);
	y0 = y1 = y;		/* P0 = P6, P1 = P7 */
    }
    plotQuadBezierSeg(x0, y0, x1, y1, x2, y2);	/* remaining part */
}
#endif

static void
plotCubicBezierSeg(int x0, int y0,
		   double x1, double y1,
		   double x2, double y2,
		   int x3, int y3)
{				/* plot limited cubic Bezier segment */
    int f, fx, fy, tt;
    int leg = 1;
    int sx = x0 < x3 ? 1 : -1;
    int sy = y0 < y3 ? 1 : -1;	/* step direction */
    double xc = -fabs(x0 + x1 - x2 - x3);
    double xa = xc - 4 * sx * (x1 - x2);
    double xb = sx * (x0 - x1 - x2 + x3);
    double yc = -fabs(y0 + y1 - y2 - y3);
    double ya = yc - 4 * sy * (y1 - y2);
    double yb = sy * (y0 - y1 - y2 + y3);
    double ab, ac, bc, cb, xx, xy, yy, dx, dy, ex, *pxy;
    double EP = 0.01;
    /* check for curve restrains */
    /* slope P0-P1 == P2-P3    and  (P0-P3 == P1-P2      or   no slope change) */
    assert((x1 - x0) * (x2 - x3) < EP &&
	   ((x3 - x0) * (x1 - x2) < EP || xb * xb < xa * xc + EP));
    assert((y1 - y0) * (y2 - y3) < EP &&
	   ((y3 - y0) * (y1 - y2) < EP || yb * yb < ya * yc + EP));

    if (xa == 0 && ya == 0) {	/* quadratic Bezier */
	sx = ifloor((3 * x1 - x0 + 1) / 2);
	sy = ifloor((3 * y1 - y0 + 1) / 2);	/* new midpoint */
	plotQuadBezierSeg(x0, y0, sx, sy, x3, y3);
	return;
    }
    x1 = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0) + 1;	/* line lengths */
    x2 = (x2 - x3) * (x2 - x3) + (y2 - y3) * (y2 - y3) + 1;
    do {			/* loop over both ends */
	ab = xa * yb - xb * ya;
	ac = xa * yc - xc * ya;
	bc = xb * yc - xc * yb;
	ex = ab * (ab + ac - 3 * bc) + ac * ac;		/* P0 part of self-intersection loop? */
	f = ((ex > 0.0)
	     ? 1
	     : isqrt(1 + 1024 / x1));	/* calculate resolution */
	ab *= f;
	ac *= f;
	bc *= f;
	ex *= f * f;		/* increase resolution */
	xy = 9 * (ab + ac + bc) / 8;
	cb = 8 * (xa - ya);	/* init differences of 1st degree */
	dx = 27 * (8 * ab * (yb * yb - ya * yc) +
		   ex * (ya + 2 * yb + yc)) / 64 - ya * ya * (xy - ya);
	dy = 27 * (8 * ab * (xb * xb - xa * xc) -
		   ex * (xa + 2 * xb + xc)) / 64 - xa * xa * (xy + xa);
	/* init differences of 2nd degree */
	xx = 3 * (3 * ab * (3 * yb * yb - ya * ya - 2 * ya * yc) -
		  ya * (3 * ac * (ya + yb) + ya * cb)) / 4;
	yy = 3 * (3 * ab * (3 * xb * xb - xa * xa - 2 * xa * xc) -
		  xa * (3 * ac * (xa + xb) + xa * cb)) / 4;
	xy = xa * ya * (6 * ab + 6 * ac - 3 * bc + cb);
	ac = ya * ya;
	cb = xa * xa;
	xy = 3 * (xy + 9 * f * (cb * yb * yc - xb * xc * ac) -
		  18 * xb * yb * ab) / 8;

	if (ex < 0) {		/* negate values if inside self-intersection loop */
	    dx = -dx;
	    dy = -dy;
	    xx = -xx;
	    yy = -yy;
	    xy = -xy;
	    ac = -ac;
	    cb = -cb;
	}			/* init differences of 3rd degree */
	ab = 6 * ya * ac;
	ac = -6 * xa * ac;
	bc = 6 * ya * cb;
	cb = -6 * xa * cb;
	dx += xy;
	ex = dx + dy;
	dy += xy;		/* error of 1st step */

	for (pxy = &xy, fx = fy = f; x0 != x3 && y0 != y3;) {
	    setPixel(x0, y0);	/* plot curve */
	    do {		/* move sub-steps of one pixel */
		if (dx > *pxy || dy < *pxy)
		    goto exit;	/* confusing values */
		y1 = 2 * ex - dy;	/* save value for test of y step */
		if (2 * ex >= dx) {	/* x sub-step */
		    fx--;
		    ex += dx += xx;
		    dy += xy += ac;
		    yy += bc;
		    xx += ab;
		}
		if (y1 <= 0) {	/* y sub-step */
		    fy--;
		    ex += dy += yy;
		    dx += xy += bc;
		    xx += ac;
		    yy += cb;
		}
	    } while (fx > 0 && fy > 0);		/* pixel complete? */
	    if (2 * fx <= f) {
		x0 += sx;
		fx += f;
	    }			/* x step */
	    if (2 * fy <= f) {
		y0 += sy;
		fy += f;
	    }			/* y step */
	    if (pxy == &xy && dx < 0 && dy > 0)
		pxy = &EP;	/* pixel ahead valid */
	}
      exit:
	EXCHANGE(x0, x3, tt);
	sx = -sx;
	xb = -xb;		/* swap legs */
	EXCHANGE(y0, y3, tt);
	sy = -sy;
	yb = -yb;
	x1 = x2;
    } while (leg--);		/* try other end */
    plotLine(x0, y0, x3, y3);	/* remaining part in case of cusp or crunode */
}

static void
plotCubicBezier(int x0, int y0, int x1, int y1,
		int x2, int y2, int x3, int y3)
{				/* plot any cubic Bezier curve */
    int n = 0, i = 0;
    long xc = x0 + x1 - x2 - x3;
    long xa = xc - 4 * (x1 - x2);
    long xb = x0 - x1 - x2 + x3;
    long xd = xb + 4 * (x1 + x2);
    long yc = y0 + y1 - y2 - y3;
    long ya = yc - 4 * (y1 - y2);
    long yb = y0 - y1 - y2 + y3;
    long yd = yb + 4 * (y1 + y2);
    double fx0 = x0;
    double fy0 = y0;
    double t1 = (double) (xb * xb - xa * xc), t2, t[5];

#ifdef DEBUG_BEZIER
    printf("plotCubicBezier(%d,%d, %d,%d, %d,%d, %d,%d\n",
	   x0, y0, x1, y1, x2, y2, x3, y3);
#endif
    /* sub-divide curve at gradient sign changes */
    if (xa == 0) {		/* horizontal */
	if (labs(xc) < 2 * labs(xb))
	    t[n++] = (double) xc / (2.0 * (double) xb);		/* one change */
    } else if (t1 > 0.0) {	/* two changes */
	t2 = sqrt(t1);
	t1 = ((double) xb - t2) / (double) xa;
	if (fabs(t1) < 1.0)
	    t[n++] = t1;
	t1 = ((double) xb + t2) / (double) xa;
	if (fabs(t1) < 1.0)
	    t[n++] = t1;
    }
    t1 = (double) (yb * yb - ya * yc);
    if (ya == 0) {		/* vertical */
	if (labs(yc) < 2 * labs(yb))
	    t[n++] = (double) yc / (2.0 * (double) yb);		/* one change */
    } else if (t1 > 0.0) {	/* two changes */
	t2 = sqrt(t1);
	t1 = ((double) yb - t2) / (double) ya;
	if (fabs(t1) < 1.0)
	    t[n++] = t1;
	t1 = ((double) yb + t2) / (double) ya;
	if (fabs(t1) < 1.0)
	    t[n++] = t1;
    }
    for (i = 1; i < n; i++)	/* bubble sort of 4 points */
	if ((t1 = t[i - 1]) > t[i]) {
	    t[i - 1] = t[i];
	    t[i] = t1;
	    i = 0;
	}

    t1 = -1.0;
    t[n] = 1.0;			/* begin / end point */
    for (i = 0; i <= n; i++) {	/* plot each segment separately */
	double fx1, fx2, fx3;
	double fy1, fy2, fy3;

	t2 = t[i];		/* sub-divide at t[i-1], t[i] */
	fx1 = (t1 * (t1 * (double) xb - (double) (2 * xc)) -
	       t2 * (t1 * (t1 * (double) xa - (double) (2 * xb)) + (double)
		     xc) + (double) xd) / 8 - fx0;
	fy1 = (t1 * (t1 * (double) yb - (double) (2 * yc)) -
	       t2 * (t1 * (t1 * (double) ya - (double) (2 * yb)) + (double)
		     yc) + (double) yd) / 8 - fy0;
	fx2 = (t2 * (t2 * (double) xb - (double) (2 * xc)) -
	       t1 * (t2 * (t2 * (double) xa - (double) (2 * xb)) + (double)
		     xc) + (double) xd) / 8 - fx0;
	fy2 = (t2 * (t2 * (double) yb - (double) (2 * yc)) -
	       t1 * (t2 * (t2 * (double) ya - (double) (2 * yb)) + (double)
		     yc) + (double) yd) / 8 - fy0;
	fx0 -= fx3 = (t2 * (t2 * ((double) (3 * xb) - t2 * (double) xa) -
			    (double) (3 * xc)) + (double) xd) / 8;
	fy0 -= fy3 = (t2 * (t2 * ((double) (3 * yb) - t2 * (double) ya) -
			    (double) (3 * yc)) + (double) yd) / 8;
	x3 = ifloor(fx3 + 0.5);
	y3 = ifloor(fy3 + 0.5);	/* scale bounds to int */
	if (fx0 != 0.0) {
	    fx1 *= fx0 = (x0 - x3) / fx0;
	    fx2 *= fx0;
	}
	if (fy0 != 0.0) {
	    fy1 *= fy0 = (y0 - y3) / fy0;
	    fy2 *= fy0;
	}
	if (x0 != x3 || y0 != y3)	/* segment t1 - t2 */
	    plotCubicBezierSeg(x0, y0,
			       x0 + fx1, y0 + fy1,
			       x0 + fx2, y0 + fy2,
			       x3, y3);
	x0 = x3;
	y0 = y3;
	fx0 = fx3;
	fy0 = fy3;
	t1 = t2;
    }
}

#if 0
static void
plotQuadSpline(int n, int x[], int y[], int skip_segments)
{				/* plot quadratic spline, destroys input arrays x,y */
#define M_MAX 12
    double mi = 1, m[M_MAX];	/* diagonal constants of matrix */
    int i, x0, y0, x1, y1, x2, y2;
#ifdef DEBUG_SPLINE_SEGMENTS
    int color = 0;
#endif

    assert(n > 1);		/* need at least 3 points P[0]..P[n] */

#ifdef DEBUG_SPLINE_POINTS
    {
	int save_pattern;

	i = 0;
	global_context->temporary_write_controls.foreground = 11;
	save_pattern = global_context->temporary_write_controls.pattern;
	global_context->temporary_write_controls.pattern = 0xff;
	draw_patterned_arc(global_context, x[i], y[i], x[i] + 2, y[i], 0,
			   3600, NULL, NULL);
	i++;
	global_context->temporary_write_controls.foreground = 15;
	for (; i < n; i++) {
	    draw_patterned_arc(global_context,
			       x[i], y[i],
			       x[i] + 2, y[i],
			       0, 3600, NULL, NULL);
	}
	global_context->temporary_write_controls.foreground = 10;
	draw_patterned_arc(global_context, x[i], y[n], x[i] + 2, y[i], 0,
			   3600, NULL, NULL);
	global_context->temporary_write_controls.pattern = save_pattern;
    }
#endif

    x2 = x[n];
    y2 = y[n];

    x[1] = x0 = 8 * x[1] - 2 * x[0];	/* first row of matrix */
    y[1] = y0 = 8 * y[1] - 2 * y[0];

    for (i = 2; i < n; i++) {	/* forward sweep */
	if (i - 2 < M_MAX)
	    m[i - 2] = mi = 1.0 / (6.0 - mi);
	x[i] = x0 = ifloor(8 * x[i] - x0 * mi + 0.5);	/* store yi */
	y[i] = y0 = ifloor(8 * y[i] - y0 * mi + 0.5);
    }
    x1 = ifloor((x0 - 2 * x2) / (5.0 - mi) + 0.5);	/* correction last row */
    y1 = ifloor((y0 - 2 * y2) / (5.0 - mi) + 0.5);

    for (i = n - 2; i > 0; i--) {	/* back substitution */
	if (i <= M_MAX)
	    mi = m[i - 1];
	x0 = ifloor((x[i] - x1) * mi + 0.5);	/* next corner */
	y0 = ifloor((y[i] - y1) * mi + 0.5);
#ifdef DEBUG_SPLINE_SEGMENTS
	color++;
	global_context->temporary_write_controls.foreground = color;
#endif
	if ((n - 2) - i < skip_segments)
	    plotQuadBezier((x0 + x1) / 2, (y0 + y1) / 2, x1, y1, x2, y2);
	x2 = (x0 + x1) / 2;
	x1 = x0;
	y2 = (y0 + y1) / 2;
	y1 = y0;
    }
#ifdef DEBUG_SPLINE_SEGMENTS
    color++;
    global_context->temporary_write_controls.foreground = color;
#endif
    if (skip_segments > 0)
	plotQuadBezier(x[0], y[0], x1, y1, x2, y2);
}
#endif

static void
plotCubicSpline(int n, int x[], int y[], int skip_first_last)
{
#define M_MAX 12
    double mi = 0.25, m[M_MAX];	/* diagonal constants of matrix */
    int x3, y3, x4, y4;
    int i, x0, y0, x1, y1, x2, y2;
#ifdef DEBUG_SPLINE_SEGMENTS
    RegisterNum color = 0;
#endif

    assert(n > 2);		/* need at least 4 points P[0]..P[n] */

#ifdef DEBUG_SPLINE_POINTS
    {
	unsigned save_pattern;

	i = 0;
	global_context->temporary_write_controls.foreground = 11;
	save_pattern = global_context->temporary_write_controls.pattern;
	global_context->temporary_write_controls.pattern = 0xff;
	draw_patterned_arc(global_context, x[i], y[i], x[i] + 2, y[i], 0,
			   3600, NULL, NULL);
	i++;
	global_context->temporary_write_controls.foreground = 15;
	for (; i < n; i++) {
	    draw_patterned_arc(global_context,
			       x[i], y[i],
			       x[i] + 2, y[i],
			       0, 3600, NULL, NULL);
	}
	global_context->temporary_write_controls.foreground = 10;
	draw_patterned_arc(global_context, x[i], y[i], x[i] + 2, y[i], 0,
			   3600, NULL, NULL);
	global_context->temporary_write_controls.pattern = save_pattern;
    }
#endif

    x3 = x[n - 1];
    y3 = y[n - 1];
    x4 = x[n];
    y4 = y[n];

    x[1] = x0 = 12 * x[1] - 3 * x[0];	/* first row of matrix */
    y[1] = y0 = 12 * y[1] - 3 * y[0];

    for (i = 2; i < n; i++) {	/* forward sweep */
	if (i - 2 < M_MAX)
	    m[i - 2] = mi = 0.25 / (2.0 - mi);
	x[i] = x0 = ifloor(12 * x[i] - 2 * x0 * mi + 0.5);
	y[i] = y0 = ifloor(12 * y[i] - 2 * y0 * mi + 0.5);
    }
    x2 = ifloor((x0 - 3 * x4) / (7 - 4 * mi) + 0.5);	/* correct last row */
    /* printf("y0=%d, y4=%d mi=%g\n", y0, y4, mi); */
    y2 = ifloor((y0 - 3 * y4) / (7 - 4 * mi) + 0.5);
    /* printf("y2=%d, y3=%d, y4=%d\n", y2, y3, y4); */
#ifdef DEBUG_SPLINE_SEGMENTS
    color++;
    global_context->temporary_write_controls.foreground = color;
#endif
    if (!skip_first_last)
	plotCubicBezier(x3, y3, (x2 + x4) / 2, (y2 + y4) / 2, x4, y4, x4, y4);

    if (n - 3 < M_MAX)
	mi = m[n - 3];
    x1 = ifloor((x[n - 2] - 2 * x2) * mi + 0.5);
    y1 = ifloor((y[n - 2] - 2 * y2) * mi + 0.5);
    for (i = n - 3; i > 0; i--) {	/* back substitution */
	if (i <= M_MAX)
	    mi = m[i - 1];
	x0 = ifloor((x[i] - 2 * x1) * mi + 0.5);
	y0 = ifloor((y[i] - 2 * y1) * mi + 0.5);
	x4 = ifloor((x0 + 4 * x1 + x2 + 3) / 6.0);	/* reconstruct P[i] */
	y4 = ifloor((y0 + 4 * y1 + y2 + 3) / 6.0);
#ifdef DEBUG_SPLINE_SEGMENTS
	color++;
	global_context->temporary_write_controls.foreground = color;
#endif
#define CB_PARM(num) ifloor((num) / 3.0 + 0.5)
	plotCubicBezier(x4, y4,
			CB_PARM(2 * x1 + x2),
			CB_PARM(2 * y1 + y2),
			CB_PARM(x1 + 2 * x2),
			CB_PARM(y1 + 2 * y2),
			x3, y3);
	x3 = x4;
	y3 = y4;
	x2 = x1;
	y2 = y1;
	x1 = x0;
	y1 = y0;
    }
    x0 = x[0];
    x4 = ifloor((3 * x0 + 7 * x1 + 2 * x2 + 6) / 12.0);		/* reconstruct P[1] */
    y0 = y[0];
    y4 = ifloor((3 * y0 + 7 * y1 + 2 * y2 + 6) / 12.0);
#ifdef DEBUG_SPLINE_SEGMENTS
    global_context->temporary_write_controls.foreground = 4;
#endif
    plotCubicBezier(x4, y4,
		    CB_PARM(2 * x1 + x2),
		    CB_PARM(2 * y1 + y2),
		    CB_PARM(x1 + 2 * x2),
		    CB_PARM(y1 + 2 * y2),
		    x3, y3);
#ifdef DEBUG_SPLINE_SEGMENTS
    color++;
    global_context->temporary_write_controls.foreground = color;
#endif
    if (!skip_first_last)
	plotCubicBezier(x0, y0, x0, y0, (x0 + x1) / 2, (y0 + y1) / 2, x4, y4);
}

static unsigned
find_free_alphabet_index(RegisGraphicsContext *context, unsigned alphabet,
			 unsigned pixw, unsigned pixh)
{
    unsigned ii, jj;

    /* try an exact match */
    for (ii = 0U; ii < MAX_REGIS_ALPHABETS; ii++) {
	if (context->alphabets[ii].alphabet_num == alphabet &&
	    context->alphabets[ii].pixw == pixw &&
	    context->alphabets[ii].pixh == pixh) {
	    return ii;
	}
    }

    /* otherwise use any empty slot */
    for (ii = 0U; ii < MAX_REGIS_ALPHABETS; ii++) {
	if (context->alphabets[ii].alphabet_num == INVALID_ALPHABET_NUM) {
	    context->alphabets[ii].alphabet_num = alphabet;
	    context->alphabets[ii].pixw = pixw;
	    context->alphabets[ii].pixh = pixh;
	    return ii;
	}
    }

    /* otherwise recycle a slot with a different font size */
    for (ii = 0U; ii < MAX_REGIS_ALPHABETS; ii++) {
	if (context->alphabets[ii].alphabet_num == alphabet) {
	    context->alphabets[ii].pixw = pixw;
	    context->alphabets[ii].pixh = pixh;
	    context->alphabets[ii].name[0] = '\0';
	    context->alphabets[ii].fontname[0] = '\0';
	    context->alphabets[ii].use_font = 0;
	    if (context->alphabets[ii].bytes != NULL) {
		free(context->alphabets[ii].bytes);
		context->alphabets[ii].bytes = NULL;
	    }
	    for (jj = 0U; jj < MAX_GLYPHS; jj++) {
		context->alphabets[ii].loaded[jj] = 0;
	    }
	    return ii;
	}
    }

    /* finally just recycle this arbitrary slot */
    context->alphabets[0U].alphabet_num = alphabet;
    context->alphabets[0U].pixw = pixw;
    context->alphabets[0U].pixh = pixh;
    context->alphabets[0U].name[0] = '\0';
    context->alphabets[0U].fontname[0] = '\0';
    context->alphabets[0U].use_font = 0;
    if (context->alphabets[0U].bytes != NULL) {
	free(context->alphabets[0U].bytes);
	context->alphabets[0U].bytes = NULL;
    }
    for (jj = 0U; jj < MAX_GLYPHS; jj++) {
	context->alphabets[0U].loaded[jj] = 0;
    }

    return 0U;
}

#ifdef DEBUG_SPECIFIC_CHAR_METRICS
static void
dump_bitmap_pixels(unsigned char const *pixels, unsigned w, unsigned h)
{
    unsigned yy, xx;

    for (yy = 0U; yy < h; yy++) {
	printf(" ");
	for (xx = 0U; xx < w; xx++) {
	    if (pixels[yy * w + xx]) {
		printf("#");
	    } else {
		printf("_");
	    }
	}
	printf("\n");
    }
}
#endif

#if OPT_RENDERFONT && defined(HAVE_TYPE_FCCHAR32)
static int
copy_bitmap_from_xft_font(Display *display, XftFont *font, FcChar32 ch,
			  unsigned char *pixels, unsigned w, unsigned h,
			  unsigned xmin, unsigned ymin)
{
    /*
     * FIXME: cache:
     * - the bitmap for the last M characters and target dimensions
     * - resuse the pixmap object where possible
     */
    XftColor bg, fg;
    Pixmap bitmap;
    XftDraw *draw;
    XImage *image;
    unsigned bmw, bmh;
    unsigned xx, yy;

    bg.pixel = 0UL;
    bg.color.red = 0;
    bg.color.green = 0;
    bg.color.blue = 0;
    bg.color.alpha = 0x0;

    fg.pixel = 1UL;
    fg.color.red = 0xffff;
    fg.color.green = 0xffff;
    fg.color.blue = 0xffff;
    fg.color.alpha = 0xffff;

    bmw = w + xmin;
    bmh = h;
    bitmap = XCreatePixmap(display,
			   DefaultRootWindow(display),
			   bmw, bmh,
			   1);
    if (bitmap == None) {
	TRACE(("Unable to create Pixmap\n"));
	return 0;
    }
    draw = XftDrawCreateBitmap(display, bitmap);
    if (!draw) {
	TRACE(("Unable to create XftDraw\n"));
	XFreePixmap(display, bitmap);
	return 0;
    }

    XftDrawRect(draw, &bg, 0, 0, bmw, bmh);
    XftDrawString32(draw, &fg, font, 0, font->ascent - (int) ymin,
		    &ch, 1);

    image = XGetImage(display, bitmap, (int) xmin, 0, w, h, 1, XYPixmap);
    if (!image) {
	TRACE(("Unable to create XImage\n"));
	XftDrawDestroy(draw);
	XFreePixmap(display, bitmap);
	return 0;
    }

    for (yy = 0U; yy < h; yy++) {
	for (xx = 0U; xx < w; xx++) {
	    pixels[yy * w + xx] = (unsigned char) XGetPixel(image,
							    (int) xx,
							    (int) yy);
	}
    }

    XDestroyImage(image);
    XftDrawDestroy(draw);
    XFreePixmap(display, bitmap);
    return 1;
}

static void
get_xft_glyph_dimensions(Display *display, XftFont *font, unsigned *w,
			 unsigned *h, unsigned *xmin, unsigned *ymin)
{
    unsigned workw, workh;
    FcChar32 ch;
    unsigned char *pixels;
    unsigned yy, xx;
    unsigned char_count, pixel_count;
    unsigned real_minx, real_maxx, real_miny, real_maxy;
    unsigned char_minx, char_maxx, char_miny, char_maxy;

    /*
     * For each ASCII or ISO-8859-1 printable code, find out what its
     * dimensions are.
     *
     * We actually render the glyphs and determine the extents ourselves
     * because the font library can lie by several pixels, and since we are
     * doing manual character placement in fixed areas the glyph boundary needs
     * to be accurate.
     *
     * Ignore control characters and spaces - their extent information is
     * misleading.
     */

    /* Our "work area" is just a buffer which should be big enough to hold the
     * largest glyph even if its size is under-reported by a couple of pixels
     * in each dimension.
     */
    workw = (unsigned) font->max_advance_width + 2U;
    if (font->ascent + font->descent > font->height) {
	workh = (unsigned) (font->ascent + font->descent) + 2U;
    } else {
	workh = (unsigned) font->height + 2U;
    }

    if (!(pixels = malloc(workw * workh))) {
	*w = 0U;
	*h = 0U;
	return;
    }

    /* FIXME: ch is in UCS32 -- try to support non-ASCII characters */
    char_count = 0U;
    real_minx = workw - 1U;
    real_maxx = 0U;
    real_miny = workh - 1U;
    real_maxy = 0U;
    for (ch = 33; ch < 256; ++ch) {
	if (ch >= 127 && ch <= 160)
	    continue;
	if (!FcCharSetHasChar(font->charset, ch))
	    continue;

	copy_bitmap_from_xft_font(display, font, ch, pixels,
				  workw, workh, 0U, 0U);

	pixel_count = 0U;
	char_minx = workh - 1U;
	char_maxx = 0U;
	char_miny = workh - 1U;
	char_maxy = 0U;
	for (yy = 0U; yy < workh; yy++) {
	    for (xx = 0U; xx < workw; xx++) {
		if (pixels[yy * workw + xx]) {
		    if (xx < char_minx)
			char_minx = xx;
		    if (xx > char_maxx)
			char_maxx = xx;
		    if (yy < char_miny)
			char_miny = yy;
		    if (yy > char_maxy)
			char_maxy = yy;
		    pixel_count++;
		}
	    }
	}
	if (pixel_count < 1U)
	    continue;

#ifdef DEBUG_SPECIFIC_CHAR_METRICS
	if (IS_DEBUG_CHAR(ch)) {
	    printf("char: '%c' (%d)\n", (char) ch, ch);
	    printf(" minx: %u\n", char_minx);
	    printf(" maxx: %u\n", char_maxx);
	    printf(" miny: %u\n", char_miny);
	    printf(" maxy: %u\n", char_maxy);
	    dump_bitmap_pixels(pixels, workw, workh);
	    printf("\n");
	}
#endif

	if (char_minx < real_minx)
	    real_minx = char_minx;
	if (char_maxx > real_maxx)
	    real_maxx = char_maxx;
	if (char_miny < real_miny)
	    real_miny = char_miny;
	if (char_maxy > real_maxy)
	    real_maxy = char_maxy;
	char_count++;
    }

    free(pixels);

    if (char_count < 1U) {
	*w = 0U;
	*h = 0U;
	return;
    }

    *w = (unsigned) (1 + real_maxx - real_minx);
    *h = (unsigned) (1 + real_maxy - real_miny);
    *xmin = real_minx;
    *ymin = real_miny;

#ifdef DEBUG_COMPUTED_FONT_METRICS
    printf("reported metrics:\n");
    printf(" %ux%u ascent=%u descent=%u\n", font->max_advance_width,
	   font->height, font->ascent, font->descent);
    printf("computed metrics:\n");
    printf(" real_minx=%u real_maxx=%u real_miny=%u real_maxy=%u\n",
	   real_minx, real_maxx, real_miny, real_maxy);
    printf(" final: %ux%u xmin=%u ymin=%u\n", *w, *h, *xmin, *ymin);
#endif
}

#define FONT_SIZE_CACHE_SIZE 32U

/* Find the font pixel size which returns the font which is closest to the given
 * maxw and maxh without overstepping either dimension.
 */
static XftFont *
find_best_xft_font_size(XtermWidget xw,
			char const *fontname,
			unsigned maxw, unsigned maxh, unsigned max_pixels,
			unsigned *w, unsigned *h,
			unsigned *xmin, unsigned *ymin)
{
    Display *display = XtDisplay(xw);
    Screen *screen = XtScreen(xw);
    XftFont *font;
    unsigned targeth;
    unsigned ii, cacheindex;
    static struct {
	char fontname[REGIS_FONTNAME_LEN];
	unsigned maxw, maxh, max_pixels;
	unsigned targeth;
	unsigned w, h;
	unsigned xmin;
	unsigned ymin;
    } cache[FONT_SIZE_CACHE_SIZE];

    assert(display);
    assert(screen);
    assert(fontname);
    assert(w);
    assert(h);
    assert(xmin);
    assert(ymin);

    cacheindex = FONT_SIZE_CACHE_SIZE;
    for (ii = 0U; ii < FONT_SIZE_CACHE_SIZE; ii++) {
	if (cache[ii].maxw == maxw && cache[ii].maxh == maxh &&
	    cache[ii].max_pixels == max_pixels &&
	    strcmp(cache[ii].fontname, fontname) == 0) {
	    cacheindex = ii;
	    break;
	}
    }

    if (cacheindex < FONT_SIZE_CACHE_SIZE) {
	targeth = cache[cacheindex].targeth;
    } else {
	targeth = maxh * 10U + 5U;
    }
    for (;;) {
	if (targeth <= 5U) {
	    TRACE(("Giving up finding suitable Xft font size for %ux%u.\n",
		   maxw, maxh));
	    return NULL;
	}

	/*
	 * Xft does a bad job at:
	 *  - two-color low-resolution anti-aliased fonts
	 *  - non-anti-aliased fonts at low resolution unless a font size is
	 *    given (pixel size does not help, and the value of the font size
	 *    doesn't appear to matter).
	 *
	 * In those two cases it literally drops pixels, sometimes whole
	 * columns, making the glyphs unreadable and ugly even when readable.
	 */
	/*
	 * FIXME:
	 * Also, we need to scale the width and height separately.  The
	 * CHAR_WIDTH and CHAR_HEIGHT attributes would seem to be ideal, but
	 * don't appear to have any effect if set.  Instead we will manually
	 * scale the bitmap later, which may be very ugly because we won't try
	 * to identify different parts of glyphs or preserve density.
	 */
	{
	    XftPattern *pat;
	    XftPattern *match;
	    XftResult status;

	    font = NULL;
	    if ((pat = XftNameParse(fontname))) {
		XftPatternBuild(pat,
		/* arbitrary value */
				XFT_SIZE, XftTypeDouble, 12.0,
				XFT_PIXEL_SIZE, XftTypeDouble, (double)
				targeth / 10.0,
#if 0
				XFT_CHAR_WIDTH, XftTypeInteger, (int) maxw,
				XFT_CHAR_HEIGHT, XftTypeInteger, (int)
				(targeth / 10U),
#endif
				XFT_SPACING, XftTypeInteger, XFT_MONO,
				XFT_SLANT, XftTypeInteger, 0,
				XFT_ANTIALIAS, XftTypeBool, False,
				NULL);
		if ((match = XftFontMatch(display,
					  XScreenNumberOfScreen(screen),
					  pat, &status))) {
		    font = XftFontOpenPattern(display, match);
		    maybeXftCache(xw, font);
		}
	    }
	}
	if (!font) {
	    TRACE(("Unable to open a monospaced Xft font.\n"));
	    return NULL;
	}
#ifdef DEBUG_FONT_SIZE_SEARCH
	{
	    char buffer[1024];

	    if (XftNameUnparse(font->pattern, buffer, (int) sizeof(buffer))) {
		printf("Testing font named \"%s\"\n", buffer);
	    } else {
		printf("Testing unknown font\n");
	    }
	}
#endif

	if (cacheindex < FONT_SIZE_CACHE_SIZE &&
	    targeth == cache[cacheindex].targeth) {
	    *w = cache[cacheindex].w;
	    *h = cache[cacheindex].h;
	    *xmin = cache[cacheindex].xmin;
	    *ymin = cache[cacheindex].ymin;
	} else {
	    get_xft_glyph_dimensions(display, font, w, h, xmin, ymin);
	}
#ifdef DEBUG_FONT_SIZE_SEARCH
	printf("checking max=%ux%u targeth=%u.%u\n", maxw, maxh, targeth /
	       10U, targeth % 10U);
#endif

	if (*h > maxh) {
	    XftFontClose(display, font);
#ifdef DEBUG_FONT_SIZE_SEARCH
	    printf("got %ux%u glyph; too tall; reducing target size\n", *w, *h);
#endif
	    if (*h > 2U * maxh) {
		targeth /= (*h / maxh);
	    } else if (targeth > 10U && *h > maxh + 1U) {
		targeth -= 10U;
	    } else {
		targeth--;
	    }
	    continue;
	}
	if (*w > maxw) {
	    XftFontClose(display, font);
#ifdef DEBUG_FONT_SIZE_SEARCH
	    printf("got %ux%u glyph; too wide; reducing target size\n", *w, *h);
#endif
	    if (*w > 2U * maxw) {
		targeth /= (*w / maxw);
	    } else if (targeth > 10U && *w > maxw + 1U) {
		targeth -= 10U;
	    } else {
		targeth--;
	    }
	    continue;
	}
	if (*w * *h > max_pixels) {
	    XftFontClose(display, font);
#ifdef DEBUG_FONT_SIZE_SEARCH
	    printf("got %ux%u glyph; too many pixels; reducing target size\n",
		   *w, *h);
#endif
	    if (*w * *h > 2U * max_pixels) {
		unsigned min = *w < *h ? *w : *h;
		unsigned divisor = (*w * *h) / (max_pixels * min);
		if (divisor > 1U) {
		    targeth /= divisor;
		} else if (targeth > 10U) {
		    targeth -= 10U;
		} else {
		    targeth--;
		}
	    } else {
		targeth--;
	    }
	    continue;
	}
#ifdef DEBUG_FONT_NAME
	{
	    char buffer[1024];

	    if (XftNameUnparse(font->pattern, buffer, (int) sizeof(buffer))) {
		printf("Final font for \"%s\" max %dx%d is \"%s\"\n",
		       fontname, maxw, maxh, buffer);
	    } else {
		printf("Final font for \"%s\" max %dx%d is unknown\n",
		       fontname, maxw, maxh);
	    }
	}
#endif

	if (cacheindex == FONT_SIZE_CACHE_SIZE) {
	    for (ii = 0U; ii < FONT_SIZE_CACHE_SIZE; ii++) {
		if (cache[ii].maxw == 0U || cache[ii].maxh == 0U ||
		    cache[ii].max_pixels == 0U) {
		    CopyFontname(cache[ii].fontname, fontname);
		    cache[ii].maxw = maxw;
		    cache[ii].maxh = maxh;
		    cache[ii].max_pixels = max_pixels;
		    cache[ii].targeth = targeth;
		    cache[ii].w = *w;
		    cache[ii].h = *h;
		    cache[ii].xmin = *xmin;
		    cache[ii].ymin = *ymin;
		    break;
		}
	    }
	    if (ii == FONT_SIZE_CACHE_SIZE) {
		ii = targeth % FONT_SIZE_CACHE_SIZE;
		CopyFontname(cache[ii].fontname, fontname);
		cache[ii].maxw = maxw;
		cache[ii].maxh = maxh;
		cache[ii].max_pixels = max_pixels;
		cache[ii].targeth = targeth;
		cache[ii].w = *w;
		cache[ii].h = *h;
		cache[ii].xmin = *xmin;
		cache[ii].ymin = *ymin;
	    }
	}
	return font;
    }
}
#endif

static int
get_xft_bitmap_of_character(RegisGraphicsContext const *context,
			    char const *fontname, int ch,
			    unsigned maxw, unsigned maxh, unsigned char *pixels,
			    unsigned max_pixels, unsigned *w, unsigned *h)
{
    /*
     * See Xft / RENDERFONT stuff in fontutils.c and used in utils.c
     * Add a separate configuration for ReGIS.
     */
    /*
     * FIXME: cache:
     * - resuse the font where possible
     */
#ifdef XRENDERFONT
    XtermWidget xw = context->destination_graphic->xw;
    Display *display = XtDisplay(xw);
    XftFont *font;
    unsigned xmin = 0U, ymin = 0U;

    if (!(font = find_best_xft_font_size(xw, fontname, maxw, maxh,
					 max_pixels, w, h, &xmin, &ymin))) {
	TRACE(("Unable to find suitable Xft font\n"));
	return 0;
    }

    if (!copy_bitmap_from_xft_font(display, font, CharOf(ch), pixels, *w, *h,
				   xmin, ymin)) {
	TRACE(("Unable to create bitmap for '%c'\n", ch));
	XftFontClose(display, font);
	return 0;
    }
    XftFontClose(display, font);
    return 1;
#else
    (void) context;
    (void) context;
    (void) ch;
    (void) maxw;
    (void) maxh;
    (void) pixels;
    (void) max_pixels;
    (void) w;
    (void) h;

    return 0;
#endif
}

static unsigned
find_best_alphabet_index(RegisGraphicsContext const *context,
			 unsigned minw, unsigned minh,
			 unsigned maxw, unsigned maxh,
			 unsigned max_pixels)
{
    unsigned ii;
    unsigned bestmatch;
    unsigned bestw, besth;

    assert(context);
    assert(maxw);
    assert(maxh);

    bestmatch = MAX_REGIS_ALPHABETS;
    bestw = 0U;
    besth = 0U;
    for (ii = 0U; ii < MAX_REGIS_ALPHABETS; ii++) {
	if (context->alphabets[ii].alphabet_num ==
	    context->current_text_controls->alphabet_num &&
	    context->alphabets[ii].pixw >= minw &&
	    context->alphabets[ii].pixh >= minh &&
	    context->alphabets[ii].pixw <= maxw &&
	    context->alphabets[ii].pixh <= maxh &&
	    context->alphabets[ii].pixw > bestw &&
	    context->alphabets[ii].pixh > besth &&
	    context->alphabets[ii].pixw *
	    context->alphabets[ii].pixh <= max_pixels) {
	    bestmatch = ii;
	    bestw = context->alphabets[ii].pixw;
	    besth = context->alphabets[ii].pixh;
	}
    }

#ifdef DEBUG_ALPHABET_LOOKUP
    if (bestmatch < MAX_REGIS_ALPHABETS) {
	TRACE(("found alphabet %u at index %u size %ux%u font=%s\n",
	       context->current_text_controls->alphabet_num, bestmatch,
	       bestw, besth,
	       context->alphabets[bestmatch].use_font ?
	       context->alphabets[bestmatch].fontname : "(none)"));
    }
#endif

    return bestmatch;
}

#define GLYPH_WIDTH_BYTES(PIXW) ( ((PIXW) + 7U) >> 3U )

static int
get_user_bitmap_of_character(RegisGraphicsContext const *context,
			     int ch,
			     unsigned alphabet_index,
			     unsigned char *pixels)
{
    const unsigned char *glyph;
    unsigned w, h;
    unsigned xx, yy;
    unsigned byte, bit;

    assert(context);
    assert(pixels);

    if (!context->alphabets[alphabet_index].loaded[(unsigned char) ch]) {
	TRACE(("in alphabet %u with alphabet index %u user glyph for '%c' not loaded\n",
	       context->current_text_controls->alphabet_num, alphabet_index,
	       ch));
	return 0;
    }

    assert(context->alphabets[alphabet_index].bytes);

    w = context->alphabets[alphabet_index].pixw;
    h = context->alphabets[alphabet_index].pixh;
    glyph = &context->alphabets[alphabet_index]
	.bytes[(unsigned char) ch * GLYPH_WIDTH_BYTES(w) * h];

    for (yy = 0U; yy < h; yy++) {
	for (xx = 0U; xx < w; xx++) {
	    byte = yy * GLYPH_WIDTH_BYTES(w) + (xx >> 3U);
	    bit = xx & 7U;
	    pixels[yy * w + xx] = ((unsigned) glyph[byte] >> (7U - bit)) & 1U;
	}
    }

    return 1;
}

/*
 * alphabets
 *  0    built-in
 *  1-N  custom (max is 3 on VT3X0 -- up to MAX_REGIS_ALPHABETS with xterm)
 *
 * built-in 7-bit charsets
 *  (B    ASCII
 *  (0    DEC special graphics
 *  (>    DEC technical
 *  (A    NCR British
 *  (4    NCR Dutch
 *  (5    NCR Finnish
 *  (R    NCR French
 *  (9    NCR French Canadian
 *  (K    NCR German
 *  (Y    NCR Italian
 *  ('    NCR Norwegian/Danish
 *  (!6   NCR Portuguese
 *  (Z    NCR Spanish
 *  (7    NCR Swedish
 *  (-    NCR Swiss
 *
 * -@   ???
 *
 * built-in 8-bit charsets
 *  )%5   DEC supplemental graphics
 *  -A    ISO Latin-1 supplemental
 *  )<    user-preferred supplemental (94 chars)
 *
 * defaults
 *  terminal  char cell size   charsets      angle
 *  VT3x0     S1               0:ASCII(94)   0 (positive)
 *
 */
static void
get_bitmap_of_character(RegisGraphicsContext const *context, int ch,
			unsigned maxw, unsigned maxh, unsigned char *pixels,
			unsigned *w, unsigned *h, unsigned max_pixels)
{
    unsigned bestmatch;
    char const *fontname = NULL;

    assert(context);
    assert(w);
    assert(h);

    if (context->current_text_controls->alphabet_num == 0)
	fontname = context->builtin_font;

    *w = 0U;
    *h = 0U;

    bestmatch = find_best_alphabet_index(context, 1U, 1U, maxw, maxh,
					 max_pixels);
    if (bestmatch < MAX_REGIS_ALPHABETS) {
	RegisAlphabet const *alpha = &context->alphabets[bestmatch];

	if (!alpha->use_font &&
	    get_user_bitmap_of_character(context, ch, bestmatch, pixels)) {
#ifdef DEBUG_USER_GLYPH
	    TRACE(("found user glyph for alphabet number %d (index %u)\n\n",
		   context->current_text_controls->alphabet_num, bestmatch));
#endif
	    *w = alpha->pixw;
	    *h = alpha->pixh;
	    return;
	}

	if (alpha->use_font)
	    fontname = alpha->fontname;
    }

    if (fontname) {
	if (get_xft_bitmap_of_character(context, fontname, ch,
					maxw, maxh, pixels,
					max_pixels, w, h)) {
	    if (*w > maxw) {
		TRACE(("BUG: Xft glyph is too wide: %ux%u but max is %ux%u\n",
		       *w, *h, maxw, maxh));
	    } else if (*h > maxh) {
		TRACE(("BUG: Xft glyph is too tall: %ux%u but max is %ux%u\n",
		       *w, *h, maxw, maxh));
	    } else if (*w * *h > max_pixels) {
		TRACE(("BUG: Xft glyph has too many pixels: %u but max is %u\n",
		       *w * *h, max_pixels));
	    } else {
		TRACE(("got glyph from \"%s\" for alphabet number %d\n",
		       fontname, context->current_text_controls->alphabet_num));
#ifdef DEBUG_SPECIFIC_CHAR_METRICS
		if (IS_DEBUG_CHAR(ch)) {
		    printf("got %ux%u Xft bitmap for '%c' target size %ux%u:\n",
			   *w, *h,
			   ch, maxw, maxh);
		    dump_bitmap_pixels(pixels, *w, *h);
		    printf("\n");
		}
#endif
		return;
	    }
	}
    }

    TRACE(("unable to load any bitmap for character '%c' in alphabet number %u at %ux%u\n",
	   ch, context->current_text_controls->alphabet_num, maxw, maxh));

    /*
     * The VT3x0 series (and probably earlier ReGIS implementations) use a solid
     * block glyph for unknown glyphs.
     */
    {
	unsigned xx, yy;

	*w = MIN2(8U, maxh);
	*h = MIN2(10U, maxw);
	for (yy = 0U; yy < *h; yy++)
	    for (xx = 0U; xx < *w; xx++)
		pixels[yy * *w + xx] = '\1';
    }
}

#define ROT_SHEAR_SCALE 8192
#define SIGNED_UNSIGNED_MOD(VAL, BASE) ( (((VAL) % (int) (BASE)) + (int) (BASE)) % (int) (BASE) )

static unsigned
get_shade_character_pixel(unsigned char const *pixels, unsigned w, unsigned h,
			  unsigned smaxf, unsigned scale, int slant_dx,
			  int px, int py)
{
    unsigned wx, wy;
    unsigned fx, fy;

    wx = (unsigned) SIGNED_UNSIGNED_MOD(px -
					(slant_dx * SIGNED_UNSIGNED_MOD(py, smaxf))
					/ ROT_SHEAR_SCALE, smaxf);
    wy = (unsigned) SIGNED_UNSIGNED_MOD(py, smaxf);

    fx = (wx * scale) >> SCALE_FIXED_POINT;
    fy = (wy * scale) >> SCALE_FIXED_POINT;
    if (fx < w && fy < h) {
	return (unsigned) pixels[fy * w + fx];
    }
    return 0U;
}

static void
draw_character(RegisGraphicsContext *context, int ch,
	       int slant_dx, int rot_shear_x,
	       int rot_shear_y, int x_sign_x, int x_sign_y,
	       int y_sign_x, int y_sign_y)
{
    const unsigned xmaxd = context->current_text_controls->character_display_w;
    const unsigned ymaxd = context->current_text_controls->character_display_h;
    const unsigned xmaxf = context->current_text_controls->character_unit_cell_w;
    const unsigned ymaxf = context->current_text_controls->character_unit_cell_h;
    unsigned w, h;
    unsigned xscale, yscale;
    unsigned fx, fy;
    unsigned px, py;
    int sx;
    int rx, ry;
    int ox, oy;
    unsigned pad_left, pad_right;
    unsigned pad_top, pad_bottom;
    unsigned char pixels[MAX_GLYPH_PIXELS];
    unsigned value;

    get_bitmap_of_character(context, ch, xmaxf, ymaxf, pixels, &w, &h,
			    MAX_GLYPH_PIXELS);
    if (w < 1 || h < 1) {
	return;
    }

    if (xmaxd > xmaxf) {
	pad_left = (xmaxd - xmaxf) / 2U;
	pad_right = (xmaxd - xmaxf) - pad_left;
    } else {
	pad_left = 0U;
	pad_right = 0U;
    }
    if (ymaxd > ymaxf) {
	pad_top = (ymaxd - ymaxf) / 2U;
	pad_bottom = (ymaxd - ymaxf) - pad_top;
    } else {
	pad_top = 0U;
	pad_bottom = 0U;
    }

    xscale = (w << SCALE_FIXED_POINT) / xmaxf;
    yscale = (h << SCALE_FIXED_POINT) / ymaxf;

    for (py = 0U; py < ymaxd; py++) {
	for (px = 0U; px < xmaxd; px++) {
	    if (py < pad_top || px < pad_left ||
		py >= ymaxd - pad_bottom || px >= xmaxd - pad_right) {
		value = 0U;
	    } else {
		fx = ((px - pad_left) * xscale) >> SCALE_FIXED_POINT;
		fy = ((py - pad_top) * yscale) >> SCALE_FIXED_POINT;
		if (fx < w && fy < h) {
		    value = (unsigned) pixels[fy * w + fx];
		} else {
		    value = 0U;
		}
	    }

	    sx = (int) px + (slant_dx * (int) py) / ROT_SHEAR_SCALE;
	    rx = x_sign_x * sx + x_sign_y * (int) py;
	    ry = y_sign_x * sx + y_sign_y * (int) py;
	    ox = rx + (rot_shear_x * ry) / ROT_SHEAR_SCALE;
	    oy = ry + (rot_shear_y * ox) / ROT_SHEAR_SCALE;
	    ox += (rot_shear_x * oy) / ROT_SHEAR_SCALE;

	    draw_regis_pixel(context,
			     (int) context->graphics_output_cursor_x + ox,
			     (int) context->graphics_output_cursor_y + oy,
			     value);
	}
    }
}

static void
move_text(RegisGraphicsContext *context, int dx, int dy)
{
    double total_rotation;
    int str_invert;
    int str_shear_x, str_shear_y;
    int ox, oy;

    total_rotation = 2.0 * M_PI *
	context->current_text_controls->string_rotation / 360.0;
    while (total_rotation > 1.5 * M_PI) {
	total_rotation -= 2.0 * M_PI;
    }
    if (total_rotation > 0.5 * M_PI) {
	total_rotation -= M_PI;
	str_invert = -1;
    } else {
	str_invert = 1;
    }
    str_shear_x = (int) (ROT_SHEAR_SCALE * -tan(0.5 * -total_rotation));
    str_shear_y = (int) (ROT_SHEAR_SCALE * sin(-total_rotation));

    total_rotation = 2.0 * M_PI *
	context->current_text_controls->character_rotation / 360.0;
    while (total_rotation > 1.5 * M_PI) {
	total_rotation -= 2.0 * M_PI;
    }

    TRACE(("str_shear: %.5f, %.5f (sign=%d)\n",
	   str_shear_x / (double) ROT_SHEAR_SCALE,
	   str_shear_y / (double) ROT_SHEAR_SCALE,
	   str_invert));

    ox = str_invert * dx + (str_shear_x * dy) / ROT_SHEAR_SCALE;
    oy = str_invert * dy + (str_shear_y * ox) / ROT_SHEAR_SCALE;
    ox += (str_shear_x * oy) / ROT_SHEAR_SCALE;

    TRACE(("after pv output updating position %+d,%+d\n", ox, oy));
    context->graphics_output_cursor_x += ox;
    context->graphics_output_cursor_y += oy;

    return;
}

#define UPSCALE_TEXT_DIMENSION(D) do { \
	    *(D) = (unsigned)((double)(*(D)) * M_SQRT2); \
	} while (0)

static void
draw_text(RegisGraphicsContext *context, char const *str)
{
#ifndef ENABLE_DISTORTIONLESS_ROTATION
    RegisTextControls *old_text_controls = NULL;
    RegisTextControls scratch_text_controls;
#endif
    double total_rotation;
    size_t ii;
    int str_invert;
    int str_shear_x, str_shear_y;
    int slant_dx;
    int chr_x_sign_x, chr_x_sign_y;
    int chr_y_sign_x, chr_y_sign_y;
    int chr_shear_x, chr_shear_y;
    int begin_x, begin_y;
    int rx, ry;
    int ox, oy;

#ifdef DEBUG_ALPHABETS
    {
	unsigned n;

	for (n = 0U; n < MAX_REGIS_ALPHABETS; n++) {
	    printf("alphabet index %u\n", n);
	    if (context->alphabets[n].alphabet_num != INVALID_ALPHABET_NUM) {
		printf(" alphabet_num=%u\n", context->alphabets[n].alphabet_num);
		printf(" pixw=%d\n", context->alphabets[n].pixw);
		printf(" pixh=%d\n", context->alphabets[n].pixh);
		printf(" name=\"%s\"\n", context->alphabets[n].name);
		printf(" use_font=%d\n", context->alphabets[n].use_font);
		printf(" fontname=\"%s\"\n", context->alphabets[n].fontname);
		printf(" bytes=%p\n", context->alphabets[n].bytes);
	    }
	}
    }
#endif

    if (context->current_text_controls->slant <= -75 ||
	context->current_text_controls->slant >= +75) {
	TRACE(("ERROR: unsupported character slant angle %d\n",
	       context->current_text_controls->slant));
	return;
    }

    /* FIXME: grab when first entering command */
    begin_x = context->graphics_output_cursor_x;
    begin_y = context->graphics_output_cursor_y;

#ifndef ENABLE_DISTORTIONLESS_ROTATION
    if (context->current_text_controls->character_rotation != 0 &&
	context->current_text_controls->character_rotation != 90 &&
	context->current_text_controls->character_rotation != 180 &&
	context->current_text_controls->character_rotation != 270) {
	old_text_controls = context->current_text_controls;
	scratch_text_controls = *context->current_text_controls;
	UPSCALE_TEXT_DIMENSION(&scratch_text_controls.character_display_w);
	UPSCALE_TEXT_DIMENSION(&scratch_text_controls.character_display_h);
	/* FIXME: Not sure if this is really scaled.  The increment seems to
	 * _not_ be scaled.
	 */
	UPSCALE_TEXT_DIMENSION(&scratch_text_controls.character_unit_cell_w);
	UPSCALE_TEXT_DIMENSION(&scratch_text_controls.character_unit_cell_h);
	context->current_text_controls = &scratch_text_controls;
	TRACE(("scaled up text to %dx%d\n",
	       scratch_text_controls.character_display_w,
	       scratch_text_controls.character_display_h));
    }
#endif

    total_rotation = 2.0 * M_PI *
	context->current_text_controls->string_rotation / 360.0;
    while (total_rotation > 1.5 * M_PI) {
	total_rotation -= 2.0 * M_PI;
    }
    if (total_rotation > 0.5 * M_PI) {
	total_rotation -= M_PI;
	str_invert = -1;
    } else {
	str_invert = 1;
    }
    str_shear_x = (int) (ROT_SHEAR_SCALE * -tan(0.5 * -total_rotation));
    str_shear_y = (int) (ROT_SHEAR_SCALE * sin(-total_rotation));

    total_rotation = 2.0 * M_PI *
	context->current_text_controls->character_rotation / 360.0;
    while (total_rotation > 1.5 * M_PI) {
	total_rotation -= 2.0 * M_PI;
    }
    if (total_rotation > 0.5 * M_PI) {
	total_rotation -= M_PI;
	chr_x_sign_x = -1;
	chr_x_sign_y = 0;
	chr_y_sign_x = 0;
	chr_y_sign_y = -1;
    } else {
	chr_x_sign_x = 1;
	chr_x_sign_y = 0;
	chr_y_sign_x = 0;
	chr_y_sign_y = 1;
    }
    chr_shear_x = (int) (ROT_SHEAR_SCALE * -tan(0.5 * -total_rotation));
    chr_shear_y = (int) (ROT_SHEAR_SCALE * sin(-total_rotation));

    {
	const int slant = context->current_text_controls->slant;

	TRACE(("float version: %.5f\n", tan(2.0 * M_PI * abs(slant) / 360.0)));
	/* The slant is negative for forward-leaning characters. */
	if (slant > 0) {
	    slant_dx = (int) +(tan(2.0 * M_PI * abs(slant) / 360.0) * ROT_SHEAR_SCALE);
	} else if (slant < 0) {
	    slant_dx = (int) -(tan(2.0 * M_PI * abs(slant) / 360.0) * ROT_SHEAR_SCALE);
	} else {
	    slant_dx = 0;
	}
	TRACE(("string rotation: %d\n",
	       context->current_text_controls->string_rotation));
	TRACE(("character rotation: %d\n",
	       context->current_text_controls->character_rotation));
	TRACE(("character slant: %d (%.5f pixels per line)\n",
	       slant, slant_dx / (double) ROT_SHEAR_SCALE));
    }

    TRACE(("str_shear: %.5f, %.5f (sign=%d)\n",
	   str_shear_x / (double) ROT_SHEAR_SCALE,
	   str_shear_y / (double) ROT_SHEAR_SCALE,
	   str_invert));
    TRACE(("chr_shear: %.5f, %.5f (xsign=%d,%d, ysign=%d,%d)\n",
	   chr_shear_x / (double) ROT_SHEAR_SCALE,
	   chr_shear_y / (double) ROT_SHEAR_SCALE,
	   chr_x_sign_x, chr_x_sign_y,
	   chr_y_sign_x, chr_y_sign_y));
    TRACE(("character_inc: %d,%d\n",
	   context->current_text_controls->character_inc_x, context->current_text_controls->character_inc_y));

    rx = 0;
    ry = 0;
    for (ii = 0U; ii < strlen(str); ii++) {
	switch (str[ii]) {
	case '\r':
	    rx = 0;
	    break;
	case '\n':
	    ry += (int) context->current_text_controls->character_display_h;
	    break;
	case '\b':
	    rx -= context->current_text_controls->character_inc_x;
	    ry -= context->current_text_controls->character_inc_y;
	    break;
	case '\t':
	    rx += context->current_text_controls->character_inc_x;
	    ry += context->current_text_controls->character_inc_y;
	    break;
	default:
	    ox = str_invert * rx + (str_shear_x * ry) / ROT_SHEAR_SCALE;
	    oy = str_invert * ry + (str_shear_y * ox) / ROT_SHEAR_SCALE;
	    ox += (str_shear_x * oy) / ROT_SHEAR_SCALE;
	    TRACE(("during text output updating position to %d,%d + %+d,%+d for '%c'\n",
		   begin_x, begin_y, ox, oy, str[ii]));
	    context->graphics_output_cursor_x = begin_x + ox;
	    context->graphics_output_cursor_y = begin_y + oy;
	    draw_character(context, str[ii], slant_dx,
			   chr_shear_x, chr_shear_y,
			   chr_x_sign_x, chr_x_sign_y,
			   chr_y_sign_x, chr_y_sign_y);
	    rx += context->current_text_controls->character_inc_x;
	    ry += context->current_text_controls->character_inc_y;
	}
    }

    ox = str_invert * rx + (str_shear_x * ry) / ROT_SHEAR_SCALE;
    oy = str_invert * ry + (str_shear_y * ox) / ROT_SHEAR_SCALE;
    ox += (str_shear_x * oy) / ROT_SHEAR_SCALE;
    TRACE(("after text output updating position to %d,%d + %+d,%+d\n",
	   begin_x, begin_y, ox, oy));
    context->graphics_output_cursor_x = begin_x + ox;
    context->graphics_output_cursor_y = begin_y + oy;

#ifndef ENABLE_DISTORTIONLESS_ROTATION
    if (context->current_text_controls->character_rotation != 0 &&
	context->current_text_controls->character_rotation != 90 &&
	context->current_text_controls->character_rotation != 180 &&
	context->current_text_controls->character_rotation != 270) {
	context->current_text_controls = old_text_controls;
    }
#endif

    context->destination_graphic->dirty = 1;
    return;
}

/*
 * standard character cell sizes
 *   number  disp cell   unit cell       offset
 *   S0      [  9, 10]   [  8, disp_h]   [disp_w, 0]
 *   S1      [  9, 20]   [  8, disp_h]   [disp_w, 0]
 *   S2      [ 18, 30]   [ 16, disp_h]   [disp_w, 0]
 *   S3      [ 27, 45]   [ 24, disp_h]   [disp_w, 0]
 *   S4      [ 36, 60]   [ 32, disp_h]   [disp_w, 0]
 *   S5      [ 45, 75]   [ 40, disp_h]   [disp_w, 0]
 *   S6      [ 54, 90]   [ 48, disp_h]   [disp_w, 0]
 *   S7      [ 63,105]   [ 56, disp_h]   [disp_w, 0]
 *   S8      [ 72,120]   [ 64, disp_h]   [disp_w, 0]
 *   S9      [ 81,135]   [ 72, disp_h]   [disp_w, 0]
 *   S10     [ 90,150]   [ 80, disp_h]   [disp_w, 0]
 *   S11     [ 99,165]   [ 88, disp_h]   [disp_w, 0]
 *   S12     [108,180]   [ 96, disp_h]   [disp_w, 0]
 *   S13     [117,195]   [104, disp_h]   [disp_w, 0]
 *   S14     [126,210]   [112, disp_h]   [disp_w, 0]
 *   S15     [135,225]   [120, disp_h]   [disp_w, 0]
 *   S16     [144,240]   [128, disp_h]   [disp_w, 0]
 */
static int
get_standard_character_size(int standard, unsigned *disp_w, unsigned
			    *disp_h, unsigned *unit_w, unsigned *unit_h,
			    int *off_x, int *off_y)
{
    switch (standard) {
    case 0:
	*disp_w = 9U;
	*disp_h = 10U;
	*unit_w = 8U;
	break;
    case 1:
	*disp_w = 9U;
	*disp_h = 20U;
	*unit_w = 8U;
	break;
    case 2:
	*disp_w = 18U;
	*disp_h = 30U;
	*unit_w = 16U;
	break;
    case 3:
	*disp_w = 27U;
	*disp_h = 45U;
	*unit_w = 24U;
	break;
    case 4:
	*disp_w = 36U;
	*disp_h = 60U;
	*unit_w = 32U;
	break;
    case 5:
	*disp_w = 45U;
	*disp_h = 75U;
	*unit_w = 40U;
	break;
    case 6:
	*disp_w = 54U;
	*disp_h = 90U;
	*unit_w = 48U;
	break;
    case 7:
	*disp_w = 63U;
	*disp_h = 105U;
	*unit_w = 56U;
	break;
    case 8:
	*disp_w = 72U;
	*disp_h = 120U;
	*unit_w = 64U;
	break;
    case 9:
	*disp_w = 81U;
	*disp_h = 135U;
	*unit_w = 72U;
	break;
    case 10:
	*disp_w = 90U;
	*disp_h = 150U;
	*unit_w = 80U;
	break;
    case 11:
	*disp_w = 99U;
	*disp_h = 165U;
	*unit_w = 88U;
	break;
    case 12:
	*disp_w = 108U;
	*disp_h = 180U;
	*unit_w = 96U;
	break;
    case 13:
	*disp_w = 117U;
	*disp_h = 195U;
	*unit_w = 104U;
	break;
    case 14:
	*disp_w = 126U;
	*disp_h = 210U;
	*unit_w = 112U;
	break;
    case 15:
	*disp_w = 135U;
	*disp_h = 225U;
	*unit_w = 120U;
	break;
    case 16:
	*disp_w = 144U;
	*disp_h = 240U;
	*unit_w = 128U;
	break;
    default:
	return 1;
    }
    *unit_h = *disp_h;
    *off_x = (int) *disp_w;
    *off_y = 0;

    return 0;
}

static void
init_fragment(RegisDataFragment *fragment, char const *str)
{
    assert(fragment);
    assert(str);

    fragment->start = str;
    fragment->len = (unsigned) strlen(str);
    fragment->pos = 0U;
}

static void
copy_fragment(RegisDataFragment *dst, RegisDataFragment const *src)
{
    assert(dst);
    assert(src);

    dst->start = src->start;
    dst->len = src->len;
    dst->pos = src->pos;
}

static char
peek_fragment(RegisDataFragment const *fragment)
{
    assert(fragment);

    if (fragment->pos < fragment->len) {
	return fragment->start[fragment->pos];
    }
    return '\0';
}

static char
pop_fragment(RegisDataFragment *fragment)
{
    assert(fragment);

    if (fragment->pos < fragment->len) {
	return fragment->start[fragment->pos++];
    }
    return '\0';
}

static char
get_fragment(RegisDataFragment const *fragment, unsigned pos)
{
    assert(fragment);

    if (fragment->pos + pos < fragment->len) {
	return fragment->start[fragment->pos + pos];
    }
    return '\0';
}

#define fragment_length(f) (f)->len

static unsigned
fragment_remaining(RegisDataFragment const *fragment)
{
    assert(fragment);

    if (fragment->pos > fragment->len)
	return 0U;
    return fragment->len - fragment->pos;
}

static int
fragment_consumed(RegisDataFragment const *fragment)
{
    assert(fragment);

    return fragment->pos >= fragment->len;
}

static void
fragment_to_string(RegisDataFragment const *fragment, char *out,
		   unsigned outlen)
{
    unsigned remaininglen;
    unsigned endpos;

    assert(fragment);
    assert(out);

    if (!outlen)
	return;
    remaininglen = fragment->len - fragment->pos;
    if (remaininglen < outlen - 1U) {
	endpos = remaininglen;
    } else {
	endpos = outlen - 1U;
    }
    strncpy(out, &fragment->start[fragment->pos], endpos);
    out[endpos] = '\0';
}

#define MAX_FRAG 1024
static char const *
fragment_to_tempstr(RegisDataFragment const *fragment)
{
    static char tempstr[MAX_FRAG];

    assert(fragment);

    fragment_to_string(fragment, tempstr, MAX_FRAG);
    return tempstr;
}

static int
skip_regis_whitespace(RegisDataFragment *input)
{
    int skipped = 0;

    assert(input);

    while (!fragment_consumed(input)) {
	char ch = peek_fragment(input);
	if (ch != ',' && !IsSpace(ch)) {
	    break;
	}
	if (ch == '\n') {
	    TRACE(("end of input line\n\n"));
	}
	skipped = 1;
	pop_fragment(input);
    }

    if (skipped)
	return 1;
    return 0;
}

static int
extract_regis_extent(RegisDataFragment *input, RegisDataFragment *output)
{
    char ch;

    assert(input);
    assert(output);

    output->start = &input->start[input->pos];
    output->len = 0U;
    output->pos = 0U;

    if (input->pos >= input->len)
	return 0;

    ch = input->start[input->pos];
    if (ch != '[')
	return 0;
    input->pos++;
    output->start++;

    /* FIXME: truncate to 16 bit signed integers */
    for (; input->pos < input->len; input->pos++, output->len++) {
	ch = input->start[input->pos];
	if (ch == ';') {
	    TRACE(("DATA_ERROR: end of input before closing bracket\n"));
	    break;
	}
	if (ch == ']')
	    break;
    }
    if (ch == ']')
	input->pos++;

    return 1;
}

static int
extract_regis_num(RegisDataFragment *input, RegisDataFragment *output)
{
    char ch = 0;
    int has_digits = 0;

    assert(input);
    assert(output);

    output->start = &input->start[input->pos];
    output->len = 0U;
    output->pos = 0U;

    if (input->start[input->pos] == '-' ||
	input->start[input->pos] == '+') {
	input->pos++;
	output->len++;
    }

    for (; input->pos < input->len; input->pos++, output->len++) {
	ch = input->start[input->pos];
	if (ch != '0' && ch != '1' && ch != '2' && ch != '3' &&
	    ch != '4' && ch != '5' && ch != '6' && ch != '7' &&
	    ch != '8' && ch != '9') {
	    break;
	}
	has_digits = 1;
    }

    /* FIXME: what degenerate forms should be accepted ("E10" "1E" "1e" "1." "1ee10")? */
    /* FIXME: the terminal is said to support "floating point values", truncating to int... what do these look like? */
    if (has_digits && ch == 'E') {
	input->pos++;
	output->len++;
	for (; input->pos < input->len; input->pos++, output->len++) {
	    ch = input->start[input->pos];
	    if (ch != '0' && ch != '1' && ch != '2' && ch != '3' &&
		ch != '4' && ch != '5' && ch != '6' && ch != '7' &&
		ch != '8' && ch != '9') {
		break;
	    }
	}
    }

    return has_digits;
}

static int
extract_regis_pixelvector(RegisDataFragment *input, RegisDataFragment *output)
{
    char ch;
    int has_digits;

    assert(input);
    assert(output);

    output->start = &input->start[input->pos];
    output->len = 0U;
    output->pos = 0U;

    if (input->pos < input->len) {
	ch = input->start[input->pos];
	if (ch == '+' || ch == '-') {
	    input->pos++;
	    output->len++;
	}
    }

    has_digits = 0;
    for (; input->pos < input->len; input->pos++, output->len++) {
	ch = input->start[input->pos];
	if (ch != '0' && ch != '1' && ch != '2' && ch != '3' &&
	    ch != '4' && ch != '5' && ch != '6' && ch != '7') {
	    break;
	}
	has_digits = 1;
    }

    return has_digits;
}

static int
extract_regis_command(RegisDataFragment *input, char *command)
{
    char ch;

    assert(input);
    assert(command);

    if (input->pos >= input->len)
	return 0;

    ch = input->start[input->pos];
    if (ch == '\0' || ch == ';') {
	return 0;
    }
    if (!islower(CharOf(ch)) && !isupper(CharOf(ch)) && ch != '@') {
	return 0;
    }
    *command = ch;
    input->pos++;

    return 1;
}

static int
extract_regis_string(RegisDataFragment *input, char *out, unsigned maxlen)
{
    char open_quote_ch;
    char ch;
    unsigned outlen;

    assert(input);
    assert(out);
    assert(maxlen > 0U);

    if (input->pos >= input->len)
	return 0;

    ch = peek_fragment(input);
    if (ch != '\'' && ch != '"')
	return 0;
    open_quote_ch = ch;
    outlen = 0U;
    pop_fragment(input);

    ch = '\0';
    while (!fragment_consumed(input)) {
	char prev_ch = ch;
	ch = peek_fragment(input);
	/* ';' (resync) and '@' (macrograph) are not recognized in strings */
	if (prev_ch == open_quote_ch) {
	    if (ch == open_quote_ch) {
		if (outlen < maxlen) {
		    out[outlen] = ch;
		}
		outlen++;
		pop_fragment(input);
		ch = '\0';
		continue;
	    }
	    if (outlen < maxlen)
		out[outlen] = '\0';
	    else
		out[maxlen] = '\0';
	    return 1;
	}
	if (ch == '\0')
	    break;
	if (ch != open_quote_ch) {
	    if (outlen < maxlen)
		out[outlen] = ch;
	    outlen++;
	}
	pop_fragment(input);
    }
    if (ch == open_quote_ch) {
	pop_fragment(input);
	if (outlen < maxlen)
	    out[outlen] = '\0';
	else
	    out[maxlen] = '\0';
	return 1;
    }
    /* FIXME: handle multiple strings concatenated with commas */

    TRACE(("DATA_ERROR: end of input before closing quote\n"));
    return 0;
}

static int
extract_regis_parenthesized_data(RegisDataFragment *input,
				 RegisDataFragment *output)
{
    char ch;
    char open_quote_ch;
    int nesting;

    assert(input);
    assert(output);

    output->start = &input->start[input->pos];
    output->len = 0U;
    output->pos = 0U;

    if (input->pos >= input->len)
	return 0;

    ch = input->start[input->pos];
    if (ch != '(')
	return 0;
    input->pos++;
    output->start++;
    nesting = 1;
    open_quote_ch = '\0';

    ch = '\0';
    for (; input->pos < input->len; input->pos++, output->len++) {
	char prev_ch = ch;
	ch = input->start[input->pos];
	if (ch == '\'' || ch == '"') {
	    if (open_quote_ch == '\0') {
		open_quote_ch = ch;
	    } else {
		if (ch == prev_ch && prev_ch == open_quote_ch) {
		    ch = '\0';
		} else if (ch == open_quote_ch) {
		    open_quote_ch = '\0';
		}
	    }
	    continue;
	}
	if (open_quote_ch != '\0')
	    continue;

	if (ch == ';') {
	    TRACE(("leaving parenthesized data nested %d levels deep due to command termination character\n",
		   nesting));
	    break;
	}
	if (ch == '(')
	    nesting++;
	if (ch == ')') {
	    nesting--;
	    if (nesting == 0) {
		input->pos++;
		return 1;
	    }
	}
    }

    TRACE(("DATA_ERROR: end of input before closing paren (%d levels deep)\n",
	   nesting));
    return 0;
}

static int
extract_regis_option(RegisDataFragment *input,
		     char *option,
		     RegisDataFragment *output)
{
    char ch;
    int paren_level, bracket_level;
    char open_quote_ch;

    assert(input);
    assert(option);
    assert(output);

    /* LETTER suboptions* value? */
    /*
     * FIXME: what are the rules for using separate parens vs. sharing between
     * options?
     */

    output->start = &input->start[input->pos];
    output->len = 0U;
    output->pos = 0U;

    if (input->pos >= input->len) {
	return 0;
    }

    ch = input->start[input->pos];
    /* FIXME: are options always letters or are some special characters ok? */
    if (ch == ';' || ch == ',' ||
	ch == '(' || ch == ')' ||
	ch == '[' || ch == ']' ||
	ch == '"' || ch == '\'' ||
	isdigit(CharOf(ch))) {
	return 0;
    }
    *option = ch;
    input->pos++;
    output->start++;
    paren_level = 0;
    bracket_level = 0;

    open_quote_ch = '\0';
    for (; input->pos < input->len; input->pos++, output->len++) {
	ch = input->start[input->pos];
	TRACE(("looking at char '%c' in option '%c'\n", ch, *option));
	/* FIXME: any special rules for commas? */
	/* FIXME: handle escaped quotes */
	if (ch == '\'' || ch == '"') {
	    if (open_quote_ch == ch) {
		open_quote_ch = '\0';
	    } else {
		open_quote_ch = ch;
	    }
	    continue;
	}
	if (open_quote_ch != '\0')
	    continue;
	if (ch == '(') {
	    paren_level++;
	}
	if (ch == ')') {
	    paren_level--;
	    if (paren_level < 0) {
		TRACE(("DATA_ERROR: found ReGIS option has value with too many close parens \"%c\"\n",
		       *option));
		return 0;
	    }
	}
	if (ch == '[') {
	    bracket_level++;
	}
	if (ch == ']') {
	    bracket_level--;
	    if (bracket_level < 0) {
		TRACE(("DATA_ERROR: found ReGIS option has value with too many close brackets \"%c\"\n",
		       *option));
		return 0;
	    }
	}
	if (paren_level == 0 && bracket_level == 0) {
	    /*
	     * Top-level commas indicate the end of this option and the start of
	     * another.
	     */
	    if (ch == ',')
		break;
	    /*
	     * Top-level command/option/suboption names also indicate the end of
	     * this option.  "E" is valid as the exponent indicator in a numeric
	     * parameter.
	     */
	    if (ch != 'E' && ch != 'e' &&
		((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')))
		break;
	}
	if (ch == ';')
	    break;
    }
    if (paren_level != 0) {
	TRACE(("DATA_ERROR: mismatched parens in argument to ReGIS option \"%c\"\n",
	       *option));
	return 0;
    }
    if (bracket_level != 0) {
	TRACE(("DATA_ERROR: mismatched brackets in argument to ReGIS option \"%c\"\n",
	       *option));
	return 0;
    }

    TRACE(("found ReGIS option and value \"%c\" \"%s\"\n",
	   *option,
	   fragment_to_tempstr(output)));
    return 1;
}

static int
regis_num_to_int(RegisDataFragment const *input, int *out)
{
    char ch;

    assert(input);
    assert(out);

    /* FIXME: handle exponential notation and rounding */
    /* FIXME: check for junk after the number */
    ch = peek_fragment(input);
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
	ch != '+' &&
	ch != '-') {
	return 0;
    }

    TRACE(("converting \"%s\" to an int\n", fragment_to_tempstr(input)));
    *out = atoi(fragment_to_tempstr(input));
    return 1;
}

static int
load_regis_colorspec(RegisGraphicsContext const *context,
		     RegisDataFragment const *input,
		     short *r_out, short *g_out, short *b_out)
{
    RegisDataFragment colorspec;
    short r = -1, g = -1, b = -1;
    short l = -1;
    int simple;

    assert(context);
    assert(input);
    assert(r_out);
    assert(g_out);
    assert(b_out);

    copy_fragment(&colorspec, input);
    TRACE(("colorspec option: \"%s\"\n", fragment_to_tempstr(&colorspec)));

    skip_regis_whitespace(&colorspec);
    simple = 0;
    if (fragment_remaining(&colorspec) == 1U) {
	simple = 1;
    } else if (fragment_remaining(&colorspec) > 1U) {
	char after = get_fragment(&colorspec, 1U);
	if (IsSpace(after))
	    simple = 1;
    }
    if (simple) {
	char ch = pop_fragment(&colorspec);

	TRACE(("got ReGIS RGB colorspec pattern '%c' with arguments: \"%s\"\n",
	       ch, fragment_to_tempstr(&colorspec)));
	switch (ch) {
	case 'D':
	case 'd':
	    r = 0;
	    g = 0;
	    b = 0;
	    l = 0;
	    break;
	case 'R':
	case 'r':
	    r = 100;
	    g = 0;
	    b = 0;
	    l = 46;
	    break;
	case 'G':
	case 'g':
	    r = 0;
	    g = 100;
	    b = 0;
	    l = 50;
	    break;
	case 'B':
	case 'b':
	    r = 0;
	    g = 0;
	    b = 100;
	    l = 50;
	    break;
	case 'C':
	case 'c':
	    r = 0;
	    g = 100;
	    b = 100;
	    l = 50;
	    break;
	case 'Y':
	case 'y':
	    r = 100;
	    g = 100;
	    b = 0;
	    l = 50;
	    break;
	case 'M':
	case 'm':
	    r = 100;
	    g = 0;
	    b = 100;
	    l = 50;
	    break;
	case 'W':
	case 'w':
	    r = 100;
	    g = 100;
	    b = 100;
	    l = 100;
	    break;
	default:
	    TRACE(("DATA_ERROR: unknown RGB color name: \"%c\"\n", ch));
	    return 0;
	}
    } else {
	RegisDataFragment num;
	int max, val;
	char comp;
	short h = -1;
	short s = -1;

	while (!fragment_consumed(&colorspec)) {
	    if (skip_regis_whitespace(&colorspec))
		continue;

	    comp = pop_fragment(&colorspec);
	    switch (comp) {
	    case ',':
		/* not sure if this is valid, but it is easy to handle */
		continue;
	    case 'H':
	    case 'h':
		max = 360;
		comp = 'H';
		break;
	    case 'L':
	    case 'l':
		max = 100;
		comp = 'L';
		break;
	    case 'S':
	    case 's':
		max = 100;
		comp = 'S';
		break;
#ifdef ENABLE_RGB_COLORSPECS
	    case 'R':		/* RLogin extension */
	    case 'r':
		max = 100;
		comp = 'R';
		break;
	    case 'G':		/* RLogin extension */
	    case 'g':
		max = 100;
		comp = 'G';
		break;
	    case 'B':		/* RLogin extension */
	    case 'b':
		max = 100;
		comp = 'B';
		break;
#endif
	    default:
		TRACE(("DATA_ERROR: unrecognized component in colorspec: '%c'\n",
		       comp));
		return 0;
	    }

	    skip_regis_whitespace(&colorspec);
	    if (!extract_regis_num(&colorspec, &num)) {
		TRACE(("DATA_ERROR: expected int after '%c' component in colorspec: \"%s\"\n",
		       comp, fragment_to_tempstr(&colorspec)));
		return 0;
	    }
	    if (!regis_num_to_int(&num, &val)) {
		TRACE(("DATA_ERROR: component value %s is not a number\n",
		       fragment_to_tempstr(&num)));
		return 0;
	    }
	    /* FIXME: error, truncate, wrap, ...? */
	    if (val < 0 || val > max) {
		TRACE(("DATA_ERROR: component value %d out of range\n", val));
		return 0;
	    }

	    switch (comp) {
	    case 'H':
		h = (short) val;
		break;
	    case 'L':
		l = (short) val;
		break;
	    case 'S':
		s = (short) val;
		break;
	    case 'R':
		r = (short) val;
		break;
	    case 'G':
		g = (short) val;
		break;
	    case 'B':
		b = (short) val;
		break;
	    }
	}

	if (h >= 0 && l >= 0 && s >= 0 && r < 0 && g < 0 && b < 0) {
	    TRACE(("found HLS colorspec to be converted: %hd,%hd,%hd\n",
		   h, l, s));
	    hls2rgb(h, l, s, &r, &g, &b);
	    TRACE(("converted to RGB: %hd,%hd,%hd\n", r, g, b));
	} else if (h < 0 && l < 0 && s < 0 && r >= 0 && g >= 0 && b >= 0) {
	    TRACE(("found RGB colorspec: %hd,%hd,%hd\n", r, g, b));
	    l = (short) ((MIN3(r, g, b) + MAX3(r, g, b)) / 2);
	    TRACE(("calculated L: %d\n", l));
	} else if (h < 0 && l >= 0 && s < 0 && r < 0 && g < 0 && b < 0) {
	    TRACE(("found L colorspec to be converted: %hd,%hd,%hd\n",
		   h, l, s));
	    hls2rgb(0, l, 0, &r, &g, &b);
	    TRACE(("converted to RGB: %hd,%hd,%hd\n", r, g, b));
	} else {
	    TRACE(("DATA_ERROR: unrecognized colorspec format\n"));
	    return 0;
	}
    }

    /*
     * The VT240 and VT330 models convert to the closest grayscale value.
     */
    if (context->terminal_id == 240 || context->terminal_id == 330) {
	hls2rgb(0, l, 0, &r, &g, &b);
	TRACE(("converted to grayscale: %hd,%hd,%hd\n", r, g, b));
    }

    *r_out = r;
    *g_out = g;
    *b_out = b;

    skip_regis_whitespace(&colorspec);
    if (!fragment_consumed(&colorspec)) {
	char skip;

	skip = pop_fragment(&colorspec);
	(void) skip;		/* variable needed only if tracing */
	TRACE(("DATA_ERROR: ignoring unexpected character in ReGIS colorspec \"%c\"\n",
	       skip));
    }

    return 1;
}

static int
load_regis_regnum_or_colorspec(RegisGraphicsContext const *context,
			       RegisDataFragment const *input,
			       RegisterNum *out)
{
    int val;
    RegisDataFragment colorspec;
    RegisDataFragment num;
    RegisDataFragment coloroption;

    copy_fragment(&colorspec, input);
    TRACE(("looking at colorspec pattern: \"%s\"\n",
	   fragment_to_tempstr(&colorspec)));

    skip_regis_whitespace(&colorspec);

    if (extract_regis_num(&colorspec, &num)) {
	if (!regis_num_to_int(&num, &val)) {
	    TRACE(("DATA_ERROR: colorspec value %s is not a valid register\n",
		   fragment_to_tempstr(&num)));
	    return 0;
	}
	if (val < 0) {
	    /* FIXME: error, truncate, wrap, ...? */
	    TRACE(("DATA_ERROR: ignoring negative colorspec value: %d\n", val));
	    return 0;
	}
	if (val >= (int) context->destination_graphic->valid_registers) {
	    /* FIXME: error, truncate, wrap, ...? */
	    TRACE(("DATA_ERROR: colorspec value %d is too big; wrapping\n",
		   val));
	    val %= (int) context->destination_graphic->valid_registers;
	}

	TRACE(("colorspec contains index for register %u\n", val));
	*out = (RegisterNum) val;

	skip_regis_whitespace(&colorspec);
	if (!fragment_consumed(&colorspec)) {
	    char skip;

	    skip = pop_fragment(&colorspec);
	    (void) skip;	/* variable needed only if tracing */
	    TRACE(("DATA_ERROR: unexpected character after register \"%c\"\n",
		   skip));
	    return 0;
	}

	return 1;
    }

    if (extract_regis_parenthesized_data(&colorspec, &coloroption)) {
	short r, g, b;

	if (!load_regis_colorspec(context, &coloroption, &r, &g, &b)) {
	    TRACE(("unable to parse colorspec\n"));
	    return 0;
	}

	*out = find_color_register(context->destination_graphic->color_registers,
				   r, g, b);
	TRACE(("colorspec maps to closest register %u\n", *out));

	return 1;
    }

    TRACE(("expected register number or colorspec, but found: \"%s\"\n",
	   fragment_to_tempstr(&colorspec)));
    return 0;
}

static int
to_scaled_int(char const *num, int scale, int *value)
{
    unsigned long whole, frac;
    char *end;

    /* FIXME: handle whitespace? how about trailing junk? */
    whole = strtoul(num, &end, 10);
    if (end[0] == '.') {
	char temp[5] = "0000";

	if (end[1] != '\0') {
	    temp[0] = end[1];
	    if (end[2] != '\0') {
		temp[1] = end[2];
		if (end[3] != '\0') {
		    temp[2] = end[3];
		    if (end[4] != '\0') {
			temp[3] = end[4];
		    }
		}
	    }
	}
	frac = strtoul(temp, NULL, 10);
    } else if (end[0] == '\0' || end[0] == ',' || IsSpace(end[0])) {
	frac = 0;
    } else {
	TRACE(("unexpected character %c in number %s\n", end[0], num));
	return 0;
    }

    *value = (int) (whole * (unsigned) scale +
		    (frac * (unsigned) scale) / 10000);

    return 1;
}

static int
load_regis_raw_extent(char const *extent, int *relx, int *rely,
		      int *xloc, int *yloc, int scale)
{
    int xsign, ysign;
    char const *xpart;
    char const *ypart;

    xpart = extent;
    if ((ypart = strchr(extent, ','))) {
	ypart++;
    } else {
	ypart = "";
    }

    while (IsSpace(xpart[0]))
	xpart++;
    while (IsSpace(ypart[0]))
	ypart++;

    if (xpart[0] == '-') {
	xsign = -1;
	xpart++;
    } else if (xpart[0] == '+') {
	xsign = +1;
	xpart++;
    } else {
	xsign = 0;
    }
    if (ypart[0] == '-') {
	ysign = -1;
	ypart++;
    } else if (ypart[0] == '+') {
	ysign = +1;
	ypart++;
    } else {
	ysign = 0;
    }

    if (xpart[0] == '\0' || xpart[0] == ',') {
	*relx = 1;
	*xloc = 0;
    } else if (xsign == 0) {
	int val;

	if (!to_scaled_int(xpart, scale, &val))
	    return 0;
	*relx = 0;
	*xloc = val;
    } else {
	int val;

	if (!to_scaled_int(xpart, scale, &val))
	    return 0;
	*relx = 1;
	*xloc = xsign * val;
    }
    if (ypart[0] == '\0') {
	*rely = 1;
	*yloc = 0;
    } else if (ysign == 0) {
	int val;

	if (!to_scaled_int(ypart, scale, &val))
	    return 0;
	*rely = 0;
	*yloc = val;
    } else {
	int val;

	if (!to_scaled_int(ypart, scale, &val))
	    return 0;
	*rely = 1;
	*yloc = ysign * val;
    }

    return 1;
}

static int
load_regis_mult_extent(char const *extent, int *w, int *h)
{
    int relx, rely;
    int px, py;

    if (!load_regis_raw_extent(extent, &relx, &rely, &px, &py, 1)) {
	TRACE(("invalid coordinates in extent %s\n", extent));
	return 0;
    }
    if (relx | rely) {
	TRACE(("invalid relative value in multiplier extent %s\n", extent));
	return 0;
    }

    *w = px;
    *h = py;

    return 1;
}

static int
load_regis_pixel_extent(char const *extent, int origx, int origy,
			int *xloc, int *yloc)
{
    int relx, rely;
    int px, py;

    if (!load_regis_raw_extent(extent, &relx, &rely, &px, &py, 1)) {
	TRACE(("invalid coordinates in extent %s\n", extent));
	return 0;
    }

    *xloc = px;
    *yloc = py;

    if (relx)
	*xloc += origx;
    if (rely)
	*yloc += origy;

    return 1;
}

#define COORD_SCALE 1000

static int
load_regis_coord_extent(RegisGraphicsContext const *context, char const *extent,
			int origx, int origy, int *xloc, int *yloc)
{
    int relx, rely;
    int ux, uy;

    if (!load_regis_raw_extent(extent, &relx, &rely, &ux, &uy, COORD_SCALE)) {
	TRACE(("invalid coordinates in extent %s\n", extent));
	return 0;
    }

    if (relx) {
	const int px = SCALE_XCOORD(context, ux, COORD_SCALE);
	TRACE(("converted relative user X coord %.03f to relative pixel X coord %d (width=%d xoff=%d xdiv=%d)\n",
	       ux / (double) COORD_SCALE, px, context->width,
	       context->x_off, context->x_div));
	*xloc = origx + px;
    } else {
	const int px = TRANSLATE_XCOORD(context, ux, COORD_SCALE);
	TRACE(("converted absolute user X coord %.03f to absolute pixel X coord %d\n",
	       ux / (double) COORD_SCALE, px));
	*xloc = px;
    }
    if (rely) {
	const int py = SCALE_YCOORD(context, uy, COORD_SCALE);
	TRACE(("converted relative user Y coord %.03f to relative pixel Y coord %d (height=%d yoff=%d ydiv=%d)\n",
	       uy / (double) COORD_SCALE, py, context->height,
	       context->y_off, context->y_div));
	*yloc = origy + py;
    } else {
	const int py = TRANSLATE_YCOORD(context, uy, COORD_SCALE);
	TRACE(("converted absolute user Y coord %.03f to absolute pixel Y coord %d\n",
	       uy / (double) COORD_SCALE, py));
	*yloc = py;
    }

    return 1;
}

static int
load_regis_raw_pixelvector_digit(char const *pixelvector,
				 unsigned *offset,
				 int *dx, int *dy, int mul)
{
    switch (pixelvector[*offset]) {
    case '0':
	*dx += mul;
	break;
    case '1':
	*dx += mul;
	*dy -= mul;
	break;
    case '2':
	*dy -= mul;
	break;
    case '3':
	*dx -= mul;
	*dy -= mul;
	break;
    case '4':
	*dx -= mul;
	break;
    case '5':
	*dx -= mul;
	*dy += mul;
	break;
    case '6':
	*dy += mul;
	break;
    case '7':
	*dx += mul;
	*dy += mul;
	break;
    default:
	return 0;
    }

    (*offset)++;
    return 1;
}

static int
load_regis_pixel_pixelvector(char const *pixelvector,
			     int mul,
			     int origx, int origy,
			     int *xloc, int *yloc)
{
    int found = 0;
    int px = 0, py = 0;
    unsigned offset = 0U;
    while (load_regis_raw_pixelvector_digit(pixelvector, &offset,
					    &px, &py,
					    mul))
	found = 1;
    if (pixelvector[offset] != '\0') {
	TRACE(("DATA_ERROR: ignoring unknown pixel vector digits: \"%s\"\n",
	       &pixelvector[offset]));
    }

    *xloc = origx + px;
    *yloc = origy + py;

    return found;
}

static int
load_regis_coord_pixelvector(RegisGraphicsContext const *context,
			     char const *pixelvector,
			     int origx, int origy,
			     int *xloc, int *yloc)
{
    const int mul = (int) (context->temporary_write_controls.pv_multiplier
			   * COORD_SCALE);
    int found = 0;
    int ux = 0, uy = 0;
    unsigned offset = 0U;

    while (load_regis_raw_pixelvector_digit(pixelvector, &offset,
					    &ux, &uy,
					    mul))
	found = 1;
    if (pixelvector[offset] != '\0') {
	TRACE(("DATA_ERROR: ignoring unknown pixel vector digits: \"%s\"\n",
	       &pixelvector[offset]));
    } {
	const int px = SCALE_XCOORD(context, ux, COORD_SCALE);
	const int py = SCALE_YCOORD(context, uy, COORD_SCALE);

	TRACE(("converted relative X coord %.03f to relative pixel X coord %d (width=%d xoff=%d xdiv=%d)\n",
	       ux / (double) COORD_SCALE, px, context->width,
	       context->x_off, context->x_div));
	*xloc = origx + px;

	TRACE(("converted relative Y coord %.03f to relative pixel Y coord %d (height=%d yoff=%d ydiv=%d)\n",
	       uy / (double) COORD_SCALE, py, context->height,
	       context->y_off, context->y_div));
	*yloc = origy + py;
    }

    return found;
}

static int
load_regis_coord_pixelvector_step(RegisGraphicsContext const *context,
				  char const *pixelvector,
				  unsigned *offset,
				  int origx, int origy,
				  int *xloc, int *yloc)
{
    const int mul = (int) (context->temporary_write_controls.pv_multiplier
			   * COORD_SCALE);
    int found = 0;
    int ux = 0, uy = 0;
    if (load_regis_raw_pixelvector_digit(pixelvector, offset, &ux, &uy, mul))
	found = 1;
    if (!found && pixelvector[*offset] != '\0') {
	TRACE(("DATA_ERROR: ignoring unknown pixel vector digits: \"%s\"\n",
	       &pixelvector[*offset]));
    } {
	const int px = SCALE_XCOORD(context, ux, COORD_SCALE);
	const int py = SCALE_YCOORD(context, uy, COORD_SCALE);

	TRACE(("converted relative X coord %.03f to relative pixel X coord %d (width=%d xoff=%d xdiv=%d)\n",
	       ux / (double) COORD_SCALE, px, context->width,
	       context->x_off, context->x_div));
	*xloc = origx + px;

	TRACE(("converted relative Y coord %.03f to relative pixel Y coord %d (height=%d yoff=%d ydiv=%d)\n",
	       uy / (double) COORD_SCALE, py, context->height,
	       context->y_off, context->y_div));
	*yloc = origy + py;
    }

    return found;
}

static int
load_regis_write_control(RegisParseState *state,
			 RegisGraphicsContext const *context,
			 int cur_x, int cur_y,
			 int option,
			 RegisDataFragment *arg,
			 RegisWriteControls *out)
{
    TRACE(("checking write control option \"%c\" with arg \"%s\"\n",
	   option, fragment_to_tempstr(arg)));
    switch (option) {
    case 'A':
    case 'a':
	TRACE(("write control alternate display method \"%s\"\n",
	       fragment_to_tempstr(arg)));
	{
	    int val;
	    if (!regis_num_to_int(arg, &val) || val < 0 || val >= 1) {
		TRACE(("DATA_ERROR: interpreting out of range value as 0 FIXME\n"));
		break;
	    }
	    if (val == 1) {
		TRACE(("ERROR: blink display method not supported FIXME\n"));
	    }
	}
	break;
    case 'C':
    case 'c':
	TRACE(("write control compliment writing mode \"%s\"\n",
	       fragment_to_tempstr(arg)));
	out->write_style = WRITE_STYLE_COMPLEMENT;
	break;
    case 'E':
    case 'e':
	TRACE(("write control erase writing mode \"%s\"\n",
	       fragment_to_tempstr(arg)));
	out->write_style = WRITE_STYLE_ERASE;
	break;
    case 'F':
    case 'f':
	TRACE(("write control plane write mask \"%s\"\n",
	       fragment_to_tempstr(arg)));
	{
	    int val;
	    if (!regis_num_to_int(arg, &val) ||
		val < 0 || val >= (int) context->destination_graphic->valid_registers) {
		TRACE(("DATA_ERROR: interpreting out of range value as 0 FIXME\n"));
		out->plane_mask = 0U;
	    } else {
		out->plane_mask = (unsigned) val;
	    }
	}
	break;
    case 'I':
    case 'i':
	TRACE(("write control foreground color \"%s\"\n",
	       fragment_to_tempstr(arg)));
	if (!load_regis_regnum_or_colorspec(context, arg, &out->foreground)) {
	    TRACE(("DATA_ERROR: write control foreground color specifier not recognized: \"%s\"\n",
		   fragment_to_tempstr(arg)));
	    return 0;
	}
	break;
    case 'L':
    case 'l':
	TRACE(("write control line width \"%s\" (FIXME: currently ignored)\n",
	       fragment_to_tempstr(arg)));
	{
	    int val;
	    if (!regis_num_to_int(arg, &val) ||
		val < 0 || val >= (int) 9) {
		TRACE(("interpreting out of range value as 1 FIXME\n"));
		out->line_width = 1U;
	    } else {
		out->line_width = (unsigned) val;
	    }
	}
	break;
    case 'M':
    case 'm':
	TRACE(("write control found pixel multiplication factor \"%s\"\n",
	       fragment_to_tempstr(arg)));
	{
	    int val;
	    if (!regis_num_to_int(arg, &val) || val <= 0) {
		TRACE(("interpreting out of range value %d as 1 FIXME\n", val));
		out->pv_multiplier = 1U;
	    } else {
		out->pv_multiplier = (unsigned) val;
	    }
	}
	break;
    case 'N':
    case 'n':
	TRACE(("write control negative pattern control \"%s\"\n",
	       fragment_to_tempstr(arg)));
	{
	    int val;
	    if (!regis_num_to_int(arg, &val)) {
		val = -1;
	    }
	    switch (val) {
	    default:
		TRACE(("interpreting out of range value %d as 0 FIXME\n", val));
		out->invert_pattern = 0U;
		break;
	    case 0:
		out->invert_pattern = 0U;
		break;
	    case 1:
		out->invert_pattern = 1U;
		break;
	    }
	}
	break;
    case 'P':
    case 'p':
	TRACE(("write control found pattern control \"%s\"\n",
	       fragment_to_tempstr(arg)));
	{
	    RegisDataFragment suboptionset;
	    RegisDataFragment suboptionarg;
	    RegisDataFragment item;
	    char suboption;

	    while (!fragment_consumed(arg)) {
		if (skip_regis_whitespace(arg))
		    continue;

		TRACE(("looking for option in \"%s\"\n",
		       fragment_to_tempstr(arg)));
		if (extract_regis_parenthesized_data(arg, &suboptionset)) {
		    TRACE(("got write pattern suboptionset: \"%s\"\n",
			   fragment_to_tempstr(&suboptionset)));
		    while (!fragment_consumed(&suboptionset)) {
			skip_regis_whitespace(&suboptionset);
			if (extract_regis_option(&suboptionset, &suboption,
						 &suboptionarg)) {
			    skip_regis_whitespace(&suboptionarg);
			    TRACE(("inspecting write pattern suboption \"%c\" with value \"%s\"\n",
				   suboption,
				   fragment_to_tempstr(&suboptionarg)));
			    switch (suboption) {
			    case 'M':
			    case 'm':
				TRACE(("found pattern multiplier \"%s\"\n",
				       fragment_to_tempstr(&suboptionarg)));
				{
				    RegisDataFragment num;
				    int val;

				    if (extract_regis_num(&suboptionarg,
							  &num)) {
					if (!regis_num_to_int(&num, &val)
					    || val < 1) {
					    TRACE(("interpreting out of range pattern multiplier \"%s\" as 2 FIXME\n",
						   fragment_to_tempstr(&num)));
					    out->pattern_multiplier = 2U;
					} else {
					    out->pattern_multiplier =
						(unsigned) val;
					}
					skip_regis_whitespace(&suboptionarg);
				    }

				    if (!fragment_consumed(&suboptionarg)) {
					TRACE(("DATA_ERROR: unknown content after pattern multiplier \"%s\"\n",
					       fragment_to_tempstr(&suboptionarg)));
					return 0;
				    }
				}
				break;
			    default:
				TRACE(("DATA_ERROR: unknown ReGIS write pattern suboption '%c' arg \"%s\"\n",
				       suboption,
				       fragment_to_tempstr(&suboptionarg)));
				return 0;
			    }
			    continue;
			}

			TRACE(("DATA_ERROR: skipping unknown token in pattern control suboptionset (expecting option): \"%s\"\n",
			       fragment_to_tempstr(&suboptionset)));
			pop_fragment(&suboptionset);
		    }
		    continue;
		}

		TRACE(("looking for int in \"%s\"\n",
		       fragment_to_tempstr(arg)));
		if (extract_regis_num(arg, &item)) {
		    if (peek_fragment(&item) == '0' ||
			peek_fragment(&item) == '1') {
			unsigned pattern = 0U;
			unsigned bitcount;

			TRACE(("converting pattern bits \"%s\"\n",
			       fragment_to_tempstr(&item)));
			for (bitcount = 0;; bitcount++) {
			    char ch = pop_fragment(&item);
			    if (ch == '\0')
				break;
			    switch (ch) {
			    case '0':
				if (bitcount < MAX_PATTERN_BITS) {
				    pattern <<= 1U;
				}
				break;
			    case '1':
				if (bitcount < MAX_PATTERN_BITS) {
				    pattern <<= 1U;
				    pattern |= 1U;
				}
				break;
			    default:
				TRACE(("DATA_ERROR: unknown ReGIS write pattern bit value \"%c\"\n",
				       ch));
				return 0;
			    }
			}

			if (bitcount > 0U) {
			    unsigned extrabits;

			    for (extrabits = 0;
				 bitcount + extrabits < MAX_PATTERN_BITS;
				 extrabits++) {
				if (pattern & (1U << (bitcount - 1U))) {
				    pattern <<= 1U;
				    pattern |= 1U;
				} else {
				    pattern <<= 1U;
				}
			    }
			}

			out->pattern = pattern;
		    } else {
			int val;

			TRACE(("converting pattern id \"%s\"\n",
			       fragment_to_tempstr(&item)));
			if (!regis_num_to_int(&item, &val))
			    val = -1;
			switch (val) {	/* FIXME: exponential allowed? */
			case 0:
			    out->pattern = 0x00;	/* solid bg */
			    break;
			case 1:
			    out->pattern = 0xff;	/* solid fg */
			    break;
			case 2:
			    out->pattern = 0xf0;	/* dash */
			    break;
			case 3:
			    out->pattern = 0xe4;	/* dash dot */
			    break;
			case 4:
			    out->pattern = 0xaa;	/* dot */
			    break;
			case 5:
			    out->pattern = 0xea;	/* dash dot dot */
			    break;
			case 6:
			    out->pattern = 0x88;	/* sparse dot */
			    break;
			case 7:
			    out->pattern = 0x84;	/* asymmetric sparse dot */
			    break;
			case 8:
			    out->pattern = 0xc8;	/* sparse dash dot */
			    break;
			case 9:
			    out->pattern = 0x86;	/* sparse dot dash */
			    break;
			default:
			    TRACE(("DATA_ERROR: unknown ReGIS standard write pattern \"%d\"\n",
				   val));
			    return 0;
			}
		    }

		    TRACE(("final pattern is %02x\n", out->pattern));
		    continue;
		}
		skip_regis_whitespace(arg);

		TRACE(("DATA_ERROR: skipping unknown token in pattern suboption: \"%s\"\n",
		       fragment_to_tempstr(arg)));
		pop_fragment(arg);
	    }
	}
	break;
    case 'R':
    case 'r':
	TRACE(("write control switch to replacement writing mode \"%s\"\n",
	       fragment_to_tempstr(arg)));
	out->write_style = WRITE_STYLE_REPLACE;
	break;
    case 'S':
    case 's':
	TRACE(("write control shading control \"%s\"\n",
	       fragment_to_tempstr(arg)));
	{
	    RegisDataFragment suboptionset;
	    RegisDataFragment suboptionarg;
	    RegisDataFragment item;
	    char suboption;
	    char shading_character = '\0';
	    unsigned reference_dim = WRITE_SHADING_REF_Y;
	    /* FIXME: are relative offsets additive? */
	    int ref_x = cur_x, ref_y = cur_y;
	    int shading_enabled = 0;

	    while (!fragment_consumed(arg)) {
		if (skip_regis_whitespace(arg))
		    continue;

		if (extract_regis_string(arg, state->temp, state->templen)) {
		    TRACE(("found fill char \"%s\"\n", state->temp));
		    /* FIXME: allow longer strings, ignore extra chars, or treat as error? */
		    if (strlen(state->temp) != 1) {
			TRACE(("DATA_ERROR: expected exactly one char in fill string FIXME\n"));
			return 0;
		    }
		    shading_character = state->temp[0];
		    shading_enabled = 1;
		    TRACE(("shading character is: '%c' (%d)\n",
			   shading_character, (int) shading_character));
		    continue;
		}

		if (extract_regis_parenthesized_data(arg, &suboptionset)) {
		    skip_regis_whitespace(&suboptionset);
		    TRACE(("got shading control suboptionset: \"%s\"\n",
			   fragment_to_tempstr(&suboptionset)));
		    while (!fragment_consumed(&suboptionset)) {
			if (skip_regis_whitespace(&suboptionset))
			    continue;
			if (extract_regis_option(&suboptionset, &suboption,
						 &suboptionarg)) {
			    TRACE(("inspecting write shading suboption \"%c\" with value \"%s\"\n",
				   suboption,
				   fragment_to_tempstr(&suboptionarg)));
			    switch (suboption) {
			    case 'X':
			    case 'x':
				TRACE(("found horizontal shading suboption \"%s\"\n",
				       fragment_to_tempstr(&suboptionarg)));
				if (!fragment_consumed(&suboptionarg)) {
				    TRACE(("DATA_ERROR: unexpected value to horizontal shading suboption FIXME\n"));
				    return 0;
				}
				reference_dim = WRITE_SHADING_REF_X;
				shading_enabled = 1;
				break;
			    default:
				TRACE(("DATA_ERROR: unknown ReGIS write pattern suboption '%c' arg \"%s\"\n",
				       suboption,
				       fragment_to_tempstr(&suboptionarg)));
				return 0;
			    }
			    continue;
			}

			TRACE(("DATA_ERROR: skipping unknown token in shading control suboptionset (expecting option): \"%s\"\n",
			       fragment_to_tempstr(&suboptionset)));
			pop_fragment(&suboptionset);
		    }
		    continue;
		}

		if (extract_regis_extent(arg, &item)) {
		    TRACE(("found extent in shading option curr=%d,%d ref=%d,%d\n",
			   cur_x, cur_y, ref_x, ref_y));
		    if (!load_regis_coord_extent(context,
						 fragment_to_tempstr(&item),
						 ref_x, ref_y,
						 &ref_x, &ref_y)) {
			TRACE(("DATA_ERROR: unable to parse extent in write shading option '%c': \"%s\"\n",
			       option, fragment_to_tempstr(&item)));
			return 0;
		    }
		    TRACE(("shading reference = %d,%d (%s)\n", ref_x, ref_y,
			   ((reference_dim == WRITE_SHADING_REF_X)
			    ? "X"
			    : "Y")));
		    continue;
		}

		if (extract_regis_num(arg, &item)) {
		    if (!regis_num_to_int(&item, &shading_enabled)) {
			TRACE(("DATA_ERROR: unable to parse int in write shading option '%c': \"%s\"\n",
			       option, fragment_to_tempstr(&item)));
			return 0;
		    }
		    if (shading_enabled < 0 || shading_enabled > 1) {
			TRACE(("interpreting out of range value %d as 0 FIXME\n",
			       shading_enabled));
			shading_enabled = 0;
		    }
		    TRACE(("shading enabled = %d\n", shading_enabled));
		    continue;
		}

		if (skip_regis_whitespace(arg)) {
		    continue;
		}

		TRACE(("DATA_ERROR: skipping unknown token in shade suboption: \"%s\"\n",
		       fragment_to_tempstr(arg)));
		pop_fragment(arg);
	    }

	    if (shading_enabled) {
		out->shading_enabled = 1U;
		out->shading_reference_dim = reference_dim;
		out->shading_reference = ((reference_dim == WRITE_SHADING_REF_X)
					  ? ref_x
					  : ref_y);
		out->shading_character = shading_character;
		TRACE(("final shading state: enabled, dim=%d ref=%d, char=%c\n",
		       out->shading_reference_dim, out->shading_reference,
		       out->shading_character));
	    } else {
		/* FIXME: confirm there is no effect if shading isn't enabled
		 * in the same command
		 */
		out->shading_enabled = 0U;
		TRACE(("final shading state: shading disabled\n"));
	    }
	}
	break;
    case 'V':
    case 'v':
	TRACE(("write control switch to overlay writing mode \"%s\"\n",
	       fragment_to_tempstr(arg)));
	out->write_style = WRITE_STYLE_OVERLAY;
	break;
    default:
	TRACE(("DATA_ERROR: ignoring unknown ReGIS write option \"%c\" arg \"%s\"\n",
	       option, fragment_to_tempstr(arg)));
	return 0;
    }

    return 1;
}

static int
load_regis_write_control_set(RegisParseState *state,
			     RegisGraphicsContext const *context,
			     int cur_x, int cur_y,
			     RegisDataFragment *controls,
			     RegisWriteControls *out)
{
    RegisDataFragment optionset;
    RegisDataFragment arg;
    char option;

    while (!fragment_consumed(controls)) {
	if (skip_regis_whitespace(controls))
	    continue;

	if (extract_regis_parenthesized_data(controls, &optionset)) {
	    TRACE(("got write control optionset: \"%s\"\n",
		   fragment_to_tempstr(&optionset)));
	    while (!fragment_consumed(&optionset)) {
		skip_regis_whitespace(&optionset);
		if (extract_regis_option(&optionset, &option, &arg)) {
		    skip_regis_whitespace(&arg);
		    TRACE(("got write control option and value: \"%c\" \"%s\"\n",
			   option, fragment_to_tempstr(&arg)));
		    if (!load_regis_write_control(state, context,
						  cur_x, cur_y,
						  option, &arg, out)) {
			return 0;
		    }
		    continue;
		}

		TRACE(("DATA_ERROR: skipping unknown token in write control optionset (expecting option): \"%s\"\n",
		       fragment_to_tempstr(&optionset)));
		pop_fragment(&optionset);
	    }
	    continue;
	}

	TRACE(("DATA_ERROR: skipping unknown token in write controls (expecting optionset): \"%s\"\n",
	       fragment_to_tempstr(controls)));
	pop_fragment(controls);
    }

    return 1;
}

static void
init_regis_write_controls(int terminal_id, unsigned all_planes,
			  RegisWriteControls *controls)
{
    controls->pv_multiplier = 1U;
    controls->pattern = 0xff;	/* solid */
    controls->pattern_multiplier = 2U;
    controls->invert_pattern = 0U;
    controls->plane_mask = all_planes;
    controls->write_style = WRITE_STYLE_OVERLAY;
    switch (terminal_id) {
    case 125:			/* FIXME: verify */
    case 240:			/* FIXME: verify */
    case 241:			/* FIXME: verify */
    case 330:
	controls->foreground = 3U;
	break;
    case 340:
    default:
	controls->foreground = 7U;
	break;
    case 382:
	controls->foreground = 1U;	/* FIXME: verify */
	break;
    }
    controls->shading_enabled = 0U;
    controls->shading_character = '\0';
    controls->shading_reference = 0;	/* no meaning if shading is disabled */
    controls->shading_reference_dim = WRITE_SHADING_REF_NONE;
    controls->line_width = 1U;
    /* FIXME: add the rest */
}

static void
map_regis_graphics_pages(XtermWidget xw, RegisGraphicsContext *context)
{
    const int charrow = 0;
    const int charcol = 0;
    unsigned old_display_id = ~0U;

    if (context->destination_graphic)
	context->destination_graphic->hidden = 1;
    if (context->display_graphic) {
	context->display_graphic->hidden = 1;
	old_display_id = context->display_graphic->id;
    }

    context->destination_graphic =
	get_new_or_matching_graphic(xw,
				    charrow, charcol,
				    context->width,
				    context->height,
				    context->destination_page);
    if (context->destination_graphic) {
	context->destination_graphic->hidden = 1;
	context->destination_graphic->valid = 1;
    }

    context->display_graphic =
	get_new_or_matching_graphic(xw,
				    charrow, charcol,
				    context->width,
				    context->height,
				    context->display_page);
    if (context->display_graphic) {
	context->display_graphic->hidden = 0;
	if (old_display_id != context->display_graphic->id) {
	    if (!context->display_graphic->valid) {
		draw_solid_rectangle(context->display_graphic, 0, 0,
				     context->width, context->height,
				     context->background);
	    }
	    context->display_graphic->dirty = 1;
	    context->force_refresh = 1;
	    /* FIXME: This isn't really enough.  If there are holes in the new
	     * graphic they should be cleared and set to the text from the same
	     * page.  But we don't have pages for text in xterm (the alt buffer
	     * is similar though).
	     */
	}
	context->display_graphic->valid = 1;
    }

    TRACE(("using graphics destination=[%d -> %u] display=[%d -> %u]\n",
	   context->destination_page,
	   (context->destination_graphic
	    ? context->destination_graphic->id
	    : 0U),
	   context->display_page,
	   (context->display_graphic
	    ? context->display_graphic->id
	    : 0U)));
}

static void
copy_regis_write_controls(RegisWriteControls const *src,
			  RegisWriteControls *dst)
{
    dst->pv_multiplier = src->pv_multiplier;
    dst->pattern = src->pattern;
    dst->pattern_multiplier = src->pattern_multiplier;
    dst->invert_pattern = src->invert_pattern;
    dst->foreground = src->foreground;
    dst->plane_mask = src->plane_mask;
    dst->write_style = src->write_style;
    dst->shading_enabled = src->shading_enabled;
    dst->shading_character = src->shading_character;
    dst->shading_reference = src->shading_reference;
    dst->shading_reference_dim = src->shading_reference_dim;
    dst->line_width = src->line_width;
}

static void
init_regis_text_controls(RegisTextControls *controls)
{
    controls->alphabet_num = 0U;	/* built-in */
    controls->character_set_l = 0U;	/* ASCII */
    controls->character_set_r = 0U;	/* Latin-1 */
    get_standard_character_size(1, &controls->character_display_w,
				&controls->character_display_h,
				&controls->character_unit_cell_w,
				&controls->character_unit_cell_h,
				&controls->character_inc_x,
				&controls->character_inc_y);
    controls->string_rotation = 0;
    controls->character_rotation = 0;
    controls->slant = 0;
}

static void
copy_regis_text_controls(RegisTextControls const *src, RegisTextControls *dst)
{
    dst->alphabet_num = src->alphabet_num;
    dst->character_set_l = src->character_set_l;
    dst->character_set_r = src->character_set_r;
    dst->character_display_w = src->character_display_w;
    dst->character_display_h = src->character_display_h;
    dst->character_unit_cell_w = src->character_unit_cell_w;
    dst->character_unit_cell_h = src->character_unit_cell_h;
    dst->character_inc_x = src->character_inc_x;
    dst->character_inc_y = src->character_inc_y;
    dst->string_rotation = src->string_rotation;
    dst->character_rotation = src->character_rotation;
    dst->slant = src->slant;
}

static void
init_regis_alphabets(RegisGraphicsContext *context)
{
    unsigned alphabet_index;

    for (alphabet_index = 0U; alphabet_index < MAX_REGIS_ALPHABETS;
	 alphabet_index++) {
	context->alphabets[alphabet_index].alphabet_num = INVALID_ALPHABET_NUM;
	context->alphabets[alphabet_index].pixw = 0U;
	context->alphabets[alphabet_index].pixh = 0U;
	context->alphabets[alphabet_index].name[0] = '\0';
	context->alphabets[alphabet_index].fontname[0] = '\0';
	context->alphabets[alphabet_index].use_font = 0;
	context->alphabets[alphabet_index].bytes = NULL;
    }
}

static void
init_regis_graphics_context(int terminal_id, int width, int height,
			    unsigned max_colors, const char *builtin_font,
			    RegisGraphicsContext *context)
{
    context->destination_graphic = NULL;
    context->display_graphic = NULL;
    context->display_page = 0U;
    context->destination_page = 0U;
    context->terminal_id = terminal_id;

    /* reset addressing / clear user coordinates */
    context->width = width;
    context->height = height;
    context->x_off = 0;
    context->y_off = 0;
    context->x_div = width - 1;
    context->y_div = height - 1;

    /*
     * Generate a mask covering all valid color register address bits
     * (but don't bother past 2**16).
     */
    context->all_planes = max_colors;
    context->all_planes--;
    context->all_planes |= 1U;
    context->all_planes |= context->all_planes >> 1U;
    context->all_planes |= context->all_planes >> 2U;
    context->all_planes |= context->all_planes >> 4U;
    context->all_planes |= context->all_planes >> 8U;

    context->builtin_font = builtin_font;

    init_regis_write_controls(terminal_id, context->all_planes,
			      &context->persistent_write_controls);
    copy_regis_write_controls(&context->persistent_write_controls,
			      &context->temporary_write_controls);

    init_regis_text_controls(&context->persistent_text_controls);
    context->current_text_controls = &context->persistent_text_controls;
    init_regis_alphabets(context);

    context->multi_input_mode = 0;
    /* FIXME: coordinates */
    /* FIXME: scrolling */
    context->background = 0U;
    /* FIXME: input cursor location */
    /* FIXME: input cursor style */
    context->graphics_output_cursor_x = 0;
    context->graphics_output_cursor_y = 0;
    /* FIXME: output cursor style */

    context->force_refresh = 0;
}

static int
parse_regis_command(RegisParseState *state)
{
    char ch;

    if (!extract_regis_command(&state->input, &ch))
	return 0;

    switch (ch) {
    case 'C':
    case 'c':
	/* Curve

	 * C
	 * (A)  # set the arc length in degrees (+ or nothing for
	 *      # counter-clockwise, - for clockwise, rounded to the
	 *      # closest integer degree)
	 * (B)  # begin closed curve sequence (must have at least two
	 *      # values; this option can not be nested)
	 * (C)  # position is the center, current location is the
	 *      # circumference (stays in effect until next command)
	 * (E)  # end curve sequence (drawing is performed here)
	 * (S)  # begin open curve sequence
	 * (W)  # temporary write options (see write command)
	 * [<center, circumference position>]  # center if (C), otherwise point on circumference
	 * [<point in curve sequence>]...  # if between (B) and (E)
	 * <pv>...  # if between (B) and (E)
	 */
	TRACE(("found ReGIS command \"%c\" (curve)\n", ch));
	state->command = 'c';
	state->curve_mode = CURVE_POSITION_ARC_EDGE;
	state->arclen = 360;
	state->num_points = 0U;
	break;
    case 'F':
    case 'f':
	/* Fill

	 * F
	 * (V)  # polygon (see vector command)
	 * (C)  # curve (see curve command)
	 * (W)  # temporary write options (see write command)
	 */
	TRACE(("found ReGIS command \"%c\" (filled polygon)\n", ch));
	state->command = 'f';
	break;
    case 'L':
    case 'l':
	/* Load

	 * L
	 * (A)  # set alphabet number or name
	 * (F)"fontname"  # load from font (xterm extension)
	 * (S)[w,h]  # set glyph size (xterm extension)
	 * "ascii"xx,xx,xx,xx,xx,xx,xx,xx  # pixel values
	 */
	TRACE(("found ReGIS command \"%c\" (load charset)\n", ch));
	state->command = 'l';
	break;
    case 'P':
    case 'p':
	/* Position

	 * P
	 * (B)  # begin bounded position stack (last point returns to first)
	 * (E)  # end position stack
	 * (P)  # select graphics page for the input and output cursors
	 * (S)  # begin unbounded position stack
	 * (W)  # temporary write options (see write command)
	 * <pv>  # move: 0 == right, 1 == upper right, ..., 7 == lower right
	 * [<position>]  # move to position (X, Y, or both)
	 *
	 * Note the stack does not need to be ended before the next command
	 * Note: maximum depth is 16 levels
	 */
	TRACE(("found ReGIS command \"%c\" (position)\n", ch));
	state->command = 'p';
	break;
    case 'R':
    case 'r':
	/* Report

	 * R
	 * (E)  # parse error
	 * (I<val>)  # set input mode (0 == one-shot, 1 == multiple) (always returns CR)
	 * (L)  # current alphabet number and name
	 * (M(<name>)  # macrograph contents
	 * (M(=)  # macrograph storage (free bytes of total bytes)
	 * (P)  # absolute output cursor position
	 * (P(I))  # interactive locator mode (in one-shot or multiple mode)
	 * (P(I[xmul,ymul]))  # interactive locator mode with arrow key movement multiplers
	 */
	TRACE(("found ReGIS command \"%c\" (report status)\n", ch));
	state->command = 'r';
	break;
    case 'S':
    case 's':
	/* Screen

	 * S
	 * (A[<upper left>][<lower right>])  # adjust screen cordinates
	 * (C<setting>  # 0 (cursor output off), 1 (cursor output on)
	 * (E)  # erase to background color, resets shades, curves, and stacks
	 * (F)  # print the graphic and erase the screen (DECprint extension)
	 * (H(P<printer offset>)[<print area cornet>][<print area corner>)
	 * (I<color register>)  # set the background to a specific register
	 * (I(<rgbcode>))  # set the background to the register closest to an "RGB" color
	 * (I(R<r>G<g>B<b>))  # set the background to the register closest to an RGB triplet (RLogin extension)
	 * (I(H<h>L<l>S<s>))  # set the background to the register closest to an HLS triplet
	 * (I(L<l>))  # set the background to the register closest to a grayscale value
	 * (M<color index to set>(<rgbcode>)...)  # codes are D (black), R (red), G (green), B (blue), C (cyan), Y (yellow), M (magenta), W (white) (sets color and grayscale registers)
	 * (M<color index to set>(A<rgbcode>)...)  # codes are D (black), R (red), G (green), B (blue), C (cyan), Y (yellow), M (magenta), W (white) (sets color registers only)
	 * (M<color index to set>(R<red>G<green>B<blue>)...)  # 0..100, 0..100, 0..100 (sets color and grayscale registers) (RLogin extension)
	 * (M<color index to set>(AR<red>G<green>B<blue>)...)  # 0..100, 0..100, 0..100 (sets color registers only) (RLogin extension)
	 * (M<color index to set>(H<hue>L<lightness>S<saturation>)...)  # 0..360, 0..100, 0..100 (sets color and grayscale registers)
	 * (M<color index to set>(AH<hue>L<lightness>S<saturation>)...)  # 0..360, 0..100, 0..100 (sets color registers only)
	 * (M<color index to set>(L<mono level>)...)  # level is 0 ... 100 (sets grayscale registers only)
	 * (P<graphics page number>)  # 0 (default) or 1
	 * (S(<scale>)  # scale screen output by scale (default 1, VT125:max=2, VT3x0:unsupported) FIXME
	 * (S(X<scale>)  # scale screen output horizontally by scale (default 1, VT125:max=2, VT3x0:unsupported) FIXME
	 * (S(Y<scale>)  # scale screen output vertically by scale (default 1, VT125:max=2, VT3x0:unsupported) FIXME
	 * (T(<time delay ticks>)  # delay (60 ticks is one second, up to 32767 ticks)
	 * (N<setting>)  # 0 == normal video, 1 == negative/reverse video (not supported on VT3x0)
	 * (W(M<factor>)  # PV multiplier
	 * <PV scroll offset>  # scroll data so given coordinate is at the upper-left
	 * [scroll offset]  # scroll data so given coordinate is at the upper-left
	 */
	TRACE(("found ReGIS command \"%c\" (screen)\n", ch));
	state->command = 's';
	break;
    case 'T':
    case 't':
	/* Text

	 * T
	 * (A)  # specify which alphabet/font to select glyphs from (0==builtin)
	 * (A0L"<designator>"))  # specify a built-in set for GL via two-char designator
	 * (A0R"<designator>"))  # specify a built-in set for GR via two-char or three-char designator
	 * (A<num>R"<designator>"))  # specify a user-loaded (1-3) set for GR via two-char or three-char designator
	 * (B)  # begin temporary text control
	 * (D<char angle>)  # specify a character tilt
	 * (D<str angle>S<size id>)  # specify a string tilt
	 * (D<str angle>S<size id>D<char angle>)  # specify a string and character tilt
	 * (E)  # end temporary text control
	 * (H<factor>)  # select a height multiplier (GIGI:1-16, VT340:1-256)
	 * (I<angle>)  # italic/oblique: no slant (0), lean forward (-1 though -45), lean back (+1 through +45)
	 * (M[width factor,height factor])  # select size multipliers (width 1-16) (height 1-256)
	 * (S<size id>)  # select one of the 17 standard character sizes
	 * (S[dimensions])  # set a custom display cell size (char with border)
	 * (U[dimensions])  # set a custom unit cell size (char size)
	 * (W<write command>)  # temporary write options (see write command)
	 * [<char offset>]  # optional manual offset between characters
	 * <PV spacing>  # move half-increments for subscripts and superscripts
	 * '<text>'  # optional
	 * "<text>"  # optional
	 */
	TRACE(("found ReGIS command \"%c\" (text)\n", ch));
	state->command = 't';
	state->text_tilt_state = TEXT_TILT_STATE_READY;
	break;
    case 'V':
    case 'v':
	/* Vector

	 * V
	 * (B)  # begin bounded position stack (last point returns to first)
	 * (E)  # end position stack
	 * (S)  # begin unbounded position stack
	 * (W)  # temporary write options (see write command)
	 * <pv>  # draw a line to the pixel vector
	 * []  # draw a dot at the current location
	 * [<position>]  # draw a line to position
	 */
	TRACE(("found ReGIS command \"%c\" (vector)\n", ch));
	state->command = 'v';
	break;
    case 'W':
    case 'w':
	/* Write

	 * W
	 * (A<setting>)  # 0 == disable alternate, 1 == enable alternate/blink FIXME
	 * (C)  # complement writing mode
	 * (E)  # erase writing mode
	 * (F<plane>)  # set the plane mask to control which pixel bits are updated
	 * (I<color register>)  # set the foreground to a specific register
	 * (I(<rgbcode>))  # set the foreground to the register closest to an "RGB" color
	 * (I(R<r>G<g>B<b>))  # set the foreground to the register closest to an RGB triplet (RLogin extension)
	 * (I(H<h>L<l>S<s>))  # set the foreground to the register closest to an HLS triplet
	 * (I(L<l>))  # set the foreground to the register closest to a grayscale value
	 * (L<width>)  # set the line width (RLogin extension) FIXME
	 * (M<pixel vector multiplier>)  # set the multiplication factor
	 * (N<setting>)  # 0 == negative patterns disabled, 1 == negative patterns enabled
	 * (P<pattern number>)  # 0..9: 0 == none, 1 == solid, 2 == 50% dash, 3 == dash-dot
	 * (P<pattern bits>)  # 2 to 8 bits represented as a 0/1 sequence
	 * (P<(M<pattern multiplier>))  # set the pattern multiplier
	 * (R)  # replacement writing mode
	 * (S'<character>')  # set shading character
	 * (S<setting>)  # 0 == disable shading, 1 == enable shading
	 * (S[reference point])  # set a horizontal reference line including this point (X ignored)
	 * (S(X)[reference point])  # set a vertical reference line including this point
	 * (V)  # overlay writing mode
	 */
	TRACE(("found ReGIS command \"%c\" (write parameters)\n", ch));
	state->command = 'w';
	break;
    case '@':
	/* Macrograph

	 * .  # clear all macrographs
	 * :<letter> ...@;  # define macrograph for letter
	 * <letter>  # expand macrograph for letter
	 */
	TRACE(("found ReGIS macrograph command\n"));
	state->command = '@';
	break;
    default:
	TRACE(("DATA_ERROR: unknown ReGIS command %04x (%c), setting to '_'\n",
	       (int) ch, ch));
	state->command = '_';
	state->option = '_';
	return 0;
    }

    state->option = '_';

    return 1;
}

static int
parse_regis_option(RegisParseState *state, RegisGraphicsContext *context)
{
    RegisDataFragment optionarg;

    if (!extract_regis_option(&state->input, &state->option, &optionarg))
	return 0;
    skip_regis_whitespace(&optionarg);

    TRACE(("found ReGIS option '%c', parsing argument \"%s\"\n",
	   state->option, fragment_to_tempstr(&optionarg)));

    switch (state->command) {
    case 'c':
	TRACE(("inspecting curve option \"%c\" with value \"%s\"\n",
	       state->option, fragment_to_tempstr(&optionarg)));
	switch (state->option) {
	case 'A':
	case 'a':
	    TRACE(("found arc length \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment arclen;

		if (!extract_regis_num(&optionarg, &arclen)) {
		    TRACE(("DATA_ERROR: expected int in curve arclen option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("arc length string %s\n",
		       fragment_to_tempstr(&arclen)));
		if (!regis_num_to_int(&arclen, &state->arclen)) {
		    TRACE(("DATA_ERROR: unable to parse int in curve arclen option: \"%s\"\n",
			   fragment_to_tempstr(&arclen)));
		    break;
		}
		TRACE(("value of arc length is %d\n", state->arclen));
		while (state->arclen < -360)
		    state->arclen += 360;
		while (state->arclen > 360)
		    state->arclen -= 360;
		TRACE(("using final arc length %d\n", state->arclen));

		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: ignoring trailing junk in arc length option \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
	    }
	    break;
	case 'B':
	case 'b':
	    TRACE(("begin closed curve \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: invalid closed curve option \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		break;
	    }
	    state->curve_mode = CURVE_POSITION_CLOSED_CURVE;
	    state->num_points = 0U;
	    state->x_points[state->num_points] =
		context->graphics_output_cursor_x;
	    state->y_points[state->num_points] =
		context->graphics_output_cursor_y;
	    state->num_points++;
	    break;
	case 'C':
	case 'c':
	    TRACE(("found center position mode \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: invalid center position option \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		break;
	    }
	    state->curve_mode = CURVE_POSITION_ARC_CENTER;
	    break;
	case 'E':
	case 'e':
	    TRACE(("found end curve \"%s\"\n", fragment_to_tempstr(&optionarg)));
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected arguments to curve option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
	    }

	    switch (state->curve_mode) {
	    case CURVE_POSITION_CLOSED_CURVE:
		{
		    unsigned i;

#ifdef DEBUG_SPLINE_POINTS
		    printf("points: \n");
		    for (i = 0; i < state->num_points; i++)
			printf("  %d,%d\n",
			       state->x_points[i], state->y_points[i]);
#endif

#ifdef DEBUG_SPLINE_WITH_ROTATION
		    {
			static unsigned shift = 0;
			int temp_x[MAX_CURVE_POINTS], temp_y[MAX_CURVE_POINTS];
			shift++;
			shift = shift % state->num_points;
			for (i = 0; i < state->num_points; i++) {
			    temp_x[i] = state->x_points[i];
			    temp_y[i] = state->y_points[i];
			}
			for (i = 0; i < state->num_points; i++) {
			    state->x_points[i] =
				temp_x[(i + shift) % state->num_points];
			    state->y_points[i] =
				temp_y[(i + shift) % state->num_points];
			}

#ifdef DEBUG_SPLINE_POINTS
			printf("after shift %d: \n", shift);
			for (i = 0; i < state->num_points; i++)
			    printf("  %d,%d\n",
				   state->x_points[i], state->y_points[i]);
#endif
		    }
#endif

		    for (i = state->num_points; i > 0; i--) {
			state->x_points[i] = state->x_points[i - 1];
			state->y_points[i] = state->y_points[i - 1];
		    }
		    state->x_points[0] = state->x_points[state->num_points];
		    state->y_points[0] = state->y_points[state->num_points];
		    state->num_points++;
		    for (i = state->num_points; i != 0; i--) {
			state->x_points[i] = state->x_points[i - 1];
			state->y_points[i] = state->y_points[i - 1];
		    }
		    state->x_points[0] = state->x_points[state->num_points - 1];
		    state->y_points[0] = state->y_points[state->num_points - 1];
		    state->num_points++;
		    state->x_points[state->num_points] = state->x_points[2];
		    state->y_points[state->num_points] = state->y_points[2];
		    state->num_points++;
#ifdef DEBUG_SPLINE_WITH_OVERDRAW
		    state->x_points[state->num_points] = state->x_points[3];
		    state->y_points[state->num_points] = state->y_points[3];
		    state->num_points++;
		    state->x_points[state->num_points] = state->x_points[4];
		    state->y_points[state->num_points] = state->y_points[4];
		    state->num_points++;
#endif
#ifdef DEBUG_SPLINE_POINTS
		    printf("after points added: \n");
		    for (i = 0; i < state->num_points; i++)
			printf("  %d,%d\n",
			       state->x_points[i], state->y_points[i]);
#endif
		}

		TRACE(("drawing closed spline\n"));
		TRACE(("output location was: %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));
		global_context = context;	/* FIXME: remove after updating spline code */
		plotCubicSpline((int) state->num_points - 1,
				state->x_points, state->y_points,
				1);
		TRACE(("output location now: %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));
		TRACE(("output location finally: %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));
		state->num_points = 0U;
		break;
	    case CURVE_POSITION_OPEN_CURVE:
#ifdef DEBUG_SPLINE_POINTS
		{
		    unsigned i;

		    printf("points: \n");
		    for (i = 0U; i < state->num_points; i++)
			printf("  %d,%d\n",
			       state->x_points[i], state->y_points[i]);
		}
#endif
		TRACE(("drawing open spline\n"));
		TRACE(("output location was: %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));
		global_context = context;	/* FIXME: remove after updating spline code */
		plotCubicSpline((int) state->num_points - 1,
				state->x_points, state->y_points,
				1);
		TRACE(("output location now: %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));

		context->graphics_output_cursor_x =
		    state->x_points[state->num_points - 1];
		context->graphics_output_cursor_y =
		    state->y_points[state->num_points - 1];
		TRACE(("output location finally: %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));
		state->num_points = 0U;
		break;
	    default:
		TRACE(("DATA_ERROR: end curve option unexpected \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		break;
	    }
	    break;
	case 'S':
	case 's':
	    TRACE(("begin open curve \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: invalid open curve option \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		break;
	    }
	    state->curve_mode = CURVE_POSITION_OPEN_CURVE;
	    state->num_points = 0U;
	    state->x_points[state->num_points] =
		context->graphics_output_cursor_x;
	    state->y_points[state->num_points] =
		context->graphics_output_cursor_y;
	    state->num_points++;
	    TRACE(("first point on curve with location %d,%d\n",
		   context->graphics_output_cursor_x,
		   context->graphics_output_cursor_y));
	    break;
	case 'W':
	case 'w':
	    TRACE(("found temporary write options \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!load_regis_write_control_set(state, context,
					      context->graphics_output_cursor_x,
					      context->graphics_output_cursor_y,
					      &optionarg,
					      &context->temporary_write_controls)) {
		TRACE(("DATA_ERROR: invalid temporary write options \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		break;
	    }
	    break;
	default:
	    TRACE(("DATA_ERROR: ignoring unknown ReGIS curve command option '%c' arg \"%s\"\n",
		   state->option, fragment_to_tempstr(&optionarg)));
	    break;
	}
	break;
    case 'f':
	TRACE(("ERROR: fill commands should not be handled here\n"));
	break;
    case 'l':
	TRACE(("inspecting load option \"%c\" with value \"%s\"\n",
	       state->option, fragment_to_tempstr(&optionarg)));
	switch (state->option) {
	case 'A':
	case 'a':
	    TRACE(("found alphabet specifier option \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    for (;;) {
		RegisDataFragment alphabetarg;

		if (extract_regis_num(&optionarg, &alphabetarg)) {
		    int alphabet;

		    TRACE(("alphabet number: %s\n",
			   fragment_to_tempstr(&alphabetarg)));
		    if (!regis_num_to_int(&alphabetarg, &alphabet)) {
			TRACE(("DATA_ERROR: unable to parse int in load alphabet option: \"%s\"\n",
			       fragment_to_tempstr(&alphabetarg)));
			break;
		    }
		    if (alphabet < 0 ||
			(unsigned) alphabet >= MAX_REGIS_ALPHABETS) {
			TRACE(("DATA_ERROR: invalid alphabet: \"%d\"\n",
			       alphabet));
			break;
		    }
#ifndef ENABLE_UPLOAD_ALPHABET_ZERO
		    if (alphabet == 0) {
			TRACE(("DATA_ERROR: alphabet 0 can not be modified\n"));
			break;
		    }
#endif

		    TRACE(("setting load alphabet: %d\n", alphabet));
		    init_regis_load_state(state);
		    state->load_alphabet = (unsigned) alphabet;
		} else if (extract_regis_string(&optionarg, state->temp,
						state->templen)) {
		    TRACE(("alphabet name: %s\n", state->temp));
		    if (strlen(state->temp) == 0U ||
			strlen(state->temp) >= REGIS_ALPHABET_NAME_LEN) {
			TRACE(("DATA_ERROR: alphabet names must be between 1 and %u characters long: \"%s\" FIXME\n",
			       REGIS_ALPHABET_NAME_LEN - 1U, state->temp));
			break;
		    }

		    strcpy(state->load_name, state->temp);
		    TRACE(("using name for alphabet %u: %s\n",
			   state->load_alphabet, state->load_name));
		} else if (skip_regis_whitespace(&optionarg)) {
		    ;
		} else if (fragment_consumed(&optionarg)) {
		    break;
		} else {
		    TRACE(("DATA_ERROR: expected int or string in load alphabet option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
	    }
	    break;
#ifdef ENABLE_UPLOAD_ALPHABET_FROM_FONT
	case 'F':
	case 'f':
	    TRACE(("found font option \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));

	    if (state->load_index == MAX_REGIS_ALPHABETS) {
		state->load_index = find_free_alphabet_index(context,
							     state->load_alphabet,
							     state->load_w,
							     state->load_h);
		TRACE(("current alphabet is %u and size is %ux%u; assigning alphabet index %u\n",
		       state->load_alphabet, state->load_w,
		       state->load_h, state->load_index));
	    }

	    for (;;) {
		RegisDataFragment fontarg;

		if (skip_regis_whitespace(&optionarg))
		    continue;
		if (extract_regis_num(&optionarg, &fontarg)) {
		    int enabled;

		    TRACE(("fontname enabled: %s\n",
			   fragment_to_tempstr(&fontarg)));
		    if (!regis_num_to_int(&fontarg, &enabled)) {
			TRACE(("DATA_ERROR: unable to parse int in load fontname option: \"%s\"\n",
			       fragment_to_tempstr(&fontarg)));
			break;
		    }
		    if (enabled != 0U && enabled != 1U) {
			TRACE(("DATA_ERROR: invalid fontname enable state: \"%d\"\n", enabled));
			break;
		    }

		    TRACE(("fontname enabled: %d\n", enabled));
		    context->alphabets[state->load_index].use_font = enabled;
		    continue;
		}
		if (extract_regis_string(&optionarg, state->temp,
					 state->templen)) {
		    if (strlen(state->temp) == 0U ||
			strlen(state->temp) >= REGIS_FONTNAME_LEN) {
			TRACE(("DATA_ERROR: font names must be between 1 and %u characters long: \"%s\"\n",
			       REGIS_FONTNAME_LEN - 1U, state->temp));
			break;
		    }

		    strcpy(context->alphabets[state->load_index].fontname,
			   state->temp);
		    context->alphabets[state->load_index].use_font = 1;
		    TRACE(("using backing font: %s\n",
			   context->alphabets[state->load_index].fontname));
		}

		if (fragment_consumed(&optionarg)) {
		    break;
		} else {
		    TRACE(("DATA_ERROR: unexpected text in load fontname option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
	    }
	    break;
#endif
#ifdef ENABLE_USER_FONT_SIZE
	case 'S':
	case 's':
	    TRACE(("found glyph size option \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    while (!fragment_consumed(&optionarg)) {
		RegisDataFragment sizearg;

		if (skip_regis_whitespace(&optionarg))
		    continue;

		if (extract_regis_extent(&optionarg, &sizearg)) {
		    int w, h;
		    unsigned size;

		    TRACE(("glyph size: %s\n", fragment_to_tempstr(&sizearg)));
		    /* FIXME: verify that relative values don't work */
		    if (!load_regis_mult_extent(fragment_to_tempstr(&sizearg),
						&w, &h)) {
			TRACE(("DATA_ERROR: unable to parse extent in glyph size option: \"%s\"\n",
			       fragment_to_tempstr(&sizearg)));
			break;
		    }
		    if (w < 1 || h < 1) {
			TRACE(("DATA_ERROR: glyph dimensions must not be negative or zero: %dx%d\n",
			       w, h));
			break;
		    }
		    size = GLYPH_WIDTH_BYTES((unsigned) w) * (unsigned) h;
		    if (size > MAX_REGIS_ALPHABET_BYTES) {
			TRACE(("DATA_ERROR: glyph is too large (%u bytes, limit is %u bytes)\n",
			       size, MAX_REGIS_ALPHABET_BYTES));
			break;
		    }

		    if (state->load_index != MAX_REGIS_ALPHABETS) {
			TRACE(("DATA_ERROR: glyph size can not be changed after any data is loaded\n"));
			break;
		    }

		    TRACE(("using glyph size: %dx%d\n", w, h));
		    state->load_w = (unsigned) w;
		    state->load_h = (unsigned) h;
		    continue;
		}

		TRACE(("DATA_ERROR: expected extent in glyph size option: \"%s\"\n",
		       fragment_to_tempstr(&sizearg)));
		break;
	    }
	    break;
#endif
	default:
	    TRACE(("DATA_ERROR: ignoring unknown ReGIS load command option '%c' arg \"%s\"\n",
		   state->option, fragment_to_tempstr(&optionarg)));
	    break;
	}
	break;
    case 'p':
	TRACE(("inspecting position option \"%c\" with value \"%s\"\n",
	       state->option, fragment_to_tempstr(&optionarg)));
	switch (state->option) {
	case 'B':
	case 'b':
	    TRACE(("found begin bounded position stack \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    skip_regis_whitespace(&optionarg);
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected arguments to position option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
	    }
	    if (state->stack_next >= POSITION_STACK_SIZE) {
		/* FIXME: ignore, error, update counter? */
		TRACE(("unable to push position to full stack\n"));
		break;
	    }
	    TRACE(("pushing location: %d,%d\n",
		   context->graphics_output_cursor_x,
		   context->graphics_output_cursor_y));

	    state->stack_x[state->stack_next] =
		context->graphics_output_cursor_x;
	    state->stack_y[state->stack_next] =
		context->graphics_output_cursor_y;
	    state->stack_next++;
	    break;
	case 'E':
	case 'e':
	    TRACE(("found end position stack \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    skip_regis_whitespace(&optionarg);
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected arguments to position option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
	    }
	    if (state->stack_next == 0U) {
		TRACE(("DATA_ERROR: unable to pop position from empty stack\n"));
		break;
	    }

	    state->stack_next--;
	    if (state->stack_x[state->stack_next] != DUMMY_STACK_X ||
		state->stack_y[state->stack_next] != DUMMY_STACK_Y) {
		context->graphics_output_cursor_x =
		    state->stack_x[state->stack_next];
		context->graphics_output_cursor_y =
		    state->stack_y[state->stack_next];
		TRACE(("popped location: %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));
	    } else {
		TRACE(("not popping location\n"));
	    }
	    break;
	case 'P':
	case 'p':
	    TRACE(("found graphics page destination option \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment pagearg;
		int page;

		if (!extract_regis_num(&optionarg, &pagearg)) {
		    TRACE(("DATA_ERROR: expected int in page destination option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("page option arg: %s\n", fragment_to_tempstr(&pagearg)));
		if (!regis_num_to_int(&pagearg, &page)) {
		    TRACE(("DATA_ERROR: unable to parse int in page destination option: \"%s\"\n",
			   fragment_to_tempstr(&pagearg)));
		    break;
		}
		if (page < 0 || (unsigned) page >= MAX_REGIS_PAGES) {
		    TRACE(("DATA_ERROR: invalid page: \"%d\"\n", page));
		    break;
		}

		TRACE(("using destination page number: %d\n", page));
		context->destination_page = (unsigned) page;
		map_regis_graphics_pages(context->destination_graphic->xw,
					 context);
	    }
	    break;
	case 'S':
	case 's':
	    TRACE(("found begin unbounded position stack \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    skip_regis_whitespace(&optionarg);
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected arguments to end position option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
	    }
	    if (state->stack_next >= POSITION_STACK_SIZE) {
		/* FIXME: ignore, error, update counter? */
		TRACE(("unable to push dummy position to full stack\n"));
		break;
	    }

	    TRACE(("pushing dummy positions instead of %d,%d\n",
		   context->graphics_output_cursor_x,
		   context->graphics_output_cursor_y));
	    state->stack_x[state->stack_next] = DUMMY_STACK_X;
	    state->stack_y[state->stack_next] = DUMMY_STACK_Y;
	    state->stack_next++;
	    break;
	case 'W':
	case 'w':
	    TRACE(("found temporary write options \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!load_regis_write_control_set(state, context,
					      context->graphics_output_cursor_x,
					      context->graphics_output_cursor_y,
					      &optionarg,
					      &context->temporary_write_controls)) {
		TRACE(("DATA_ERROR: invalid temporary write options \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
	    }
	    break;
	default:
	    TRACE(("DATA_ERROR: ignoring unknown ReGIS position command option '%c' arg \"%s\"\n",
		   state->option, fragment_to_tempstr(&optionarg)));
	    break;
	}
	break;
    case 'r':
	TRACE(("inspecting report option \"%c\" with value \"%s\"\n",
	       state->option, fragment_to_tempstr(&optionarg)));
	switch (state->option) {
	case 'E':
	case 'e':
	    TRACE(("found parse error report \"%s\" FIXME\n",
		   fragment_to_tempstr(&optionarg)));
	    skip_regis_whitespace(&optionarg);
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: unexpected arguments to ReGIS report command option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
		break;
	    } {
		char reply[64];

		TRACE(("got report last error condition\n"));
		/* FIXME: implement after adding error tracking */
		sprintf(reply, "\"%u,%u\"\r", 0U, 0U);
		unparseputs(context->display_graphic->xw, reply);
		unparse_end(context->display_graphic->xw);
	    }
	    break;
	case 'I':
	case 'i':
	    TRACE(("found set input mode \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment modearg;
		int mode;

		if (!extract_regis_num(&optionarg, &modearg)) {
		    TRACE(("DATA_ERROR: expected int in report input mode option: \"%s\"\n",
			   fragment_to_tempstr(&modearg)));
		    break;
		}

		TRACE(("input mode: %s\n", fragment_to_tempstr(&modearg)));
		if (!regis_num_to_int(&modearg, &mode)) {
		    TRACE(("DATA_ERROR: unable to parse int in report input mode option: \"%s\"\n",
			   fragment_to_tempstr(&modearg)));
		    break;
		}
		if (mode != 0 && mode != 1) {
		    TRACE(("DATA_ERROR: ignoring invalid input mode: \"%d\"\n",
			   mode));
		    break;
		}

		TRACE(("using input mode: %d (%s)\n", mode,
		       (mode == 0)
		       ? "one-shot"
		       : "multiple"));
		context->multi_input_mode = mode;
		if (context->multi_input_mode) {
		    TRACE(("ERROR: multi-mode input not implemented FIXME\n"));
		    /* FIXME: enable input cursor, send location on mouse clicks or non-arrowkey keypresses */
		} else {
		    /* FIXME: if in multi-mode, disable input cursor, stop tracking mouse clicks and keypresses */
		    /* FIXME: enable input cursor and disable drawing until location report request command is received */
		    /* FIXME: upon mouse click or keypress respond with location report */
		}
		/* FIXME: implement input cursor */
		/* FIXME: implement mouse tracking */
		/* FIXME: implement arrow key movement */
		/* FIXME: implement button/key collection */

		unparseputs(context->display_graphic->xw, "\r");
		unparse_end(context->display_graphic->xw);

		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: unexpected arguments to ReGIS report command option '%c' arg \"%s\"\n",
			   state->option, fragment_to_tempstr(&optionarg)));
		}
		/* FIXME: buffer commands until report request received */
	    }
	    break;
	case 'L':
	case 'l':
	    TRACE(("found character set load report \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: unexpected arguments to ReGIS report command option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
		break;
	    } {
		char buffer[32];

		if (state->load_index == MAX_REGIS_ALPHABETS) {
		    /* If this happens something went wrong elsewhere. */
		    TRACE(("DATA_ERROR: unable to report current load alphabet\n"));
		    unparseputs(context->display_graphic->xw, "A0\"\"\r");
		    unparse_end(context->display_graphic->xw);
		    break;
		}

		unparseputs(context->display_graphic->xw, "A");
		sprintf(buffer, "%u", state->load_alphabet);
		unparseputs(context->display_graphic->xw, buffer);
		unparseputs(context->display_graphic->xw, "\"");
		unparseputs(context->display_graphic->xw, state->load_name);
		unparseputs(context->display_graphic->xw, "\"\r");
		unparse_end(context->display_graphic->xw);
	    }
	    break;
	case 'M':
	case 'm':
	    TRACE(("found macrograph report \"%s\" request\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment suboptionarg;
		char name = '\0';

		if (extract_regis_parenthesized_data(&optionarg,
						     &suboptionarg)) {
		    skip_regis_whitespace(&suboptionarg);
		    TRACE(("got macrograph report character request: \"%s\"\n",
			   fragment_to_tempstr(&suboptionarg)));
		    if (!fragment_consumed(&suboptionarg)) {
			name = pop_fragment(&suboptionarg);
			if (islower(name))
			    name = (char) toupper(name);

			skip_regis_whitespace(&suboptionarg);
			if (!fragment_consumed(&optionarg)) {
			    TRACE(("DATA_ERROR: unexpected content in ReGIS macrograph report suboptions: \"%s\"\n",
				   fragment_to_tempstr(&suboptionarg)));
			    break;
			}
		    }
		}
		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: unexpected arguments to ReGIS report command option '%c' arg \"%s\"\n",
			   state->option, fragment_to_tempstr(&optionarg)));
		    break;
		}

		if (name == '\0') {
		    TRACE(("DATA_ERROR: no macro name given to ReGIS macrograph report command\n"));
		    break;
		}

		if (name == '=') {
		    char reply[64];

		    TRACE(("got report macrograph storage request\n"));
		    /* FIXME: Implement when macrographs are supported. */
		    sprintf(reply, "\"%u,%u\"\r", 1000U, 1000U);
		    unparseputs(context->display_graphic->xw, reply);
		    unparse_end(context->display_graphic->xw);
		} else if (name < 'A' || name > 'Z') {
		    TRACE(("DATA_ERROR: invalid macrograph name: \"%c\"\n", name));
		    /* FIXME: what should happen? */
		    break;
		} else {
		    char temp[8];

		    TRACE(("got report macrograph request for name '%c'\n", name));
		    sprintf(temp, "@=%c", name);
		    unparseputs(context->display_graphic->xw, temp);
		    /* FIXME: Allow this to be disabled for security reasons. */
		    /* FIXME: implement when macrographs are supported. */
		    unparseputs(context->display_graphic->xw, "@;\r");
		    unparse_end(context->display_graphic->xw);
		}
	    }
	    break;
	case 'P':
	case 'p':
	    TRACE(("found cursor position report \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment suboptionarg;
		int output = 1;

		if (extract_regis_parenthesized_data(&optionarg,
						     &suboptionarg)) {
		    skip_regis_whitespace(&suboptionarg);
		    TRACE(("got cursor position report suboption: \"%s\"\n",
			   fragment_to_tempstr(&suboptionarg)));
		    if (!fragment_consumed(&suboptionarg)) {
			char suboption;

			/* FIXME: handle cursor movement multipliers */
			suboption = pop_fragment(&suboptionarg);
			if (suboption == 'i' || suboption == 'I') {
			    output = 0;		/* input location report */
			} else {
			    TRACE(("DATA_ERROR: unknown ReGIS postion report suboption '%c'\n",
				   suboption));
			    break;
			}

			skip_regis_whitespace(&suboptionarg);
			if (!fragment_consumed(&optionarg)) {
			    TRACE(("DATA_ERROR: unexpected content in ReGIS postion report suboptions: \"%s\"\n",
				   fragment_to_tempstr(&suboptionarg)));
			    break;
			}
		    }
		}
		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: unexpected arguments to ReGIS report command option '%c' arg \"%s\"\n",
			   state->option, fragment_to_tempstr(&optionarg)));
		    break;
		}

		TRACE(("got report cursor position (output=%d)\n", output));

		/* FIXME: look into supporting ANSI locator reports (DECLRP) */
		if (output == 1) {
		    char reply[64];

		    /* FIXME: verify in absolute, not user, coordinates */
		    sprintf(reply, "[%d,%d]\r",
			    context->graphics_output_cursor_x,
			    context->graphics_output_cursor_y);
		    unparseputs(context->display_graphic->xw, reply);
		    unparse_end(context->display_graphic->xw);
		} else {
		    char reply[64];
		    int x, y;

		    if (context->multi_input_mode) {
			/* FIXME: track input coordinates */
			x = y = 0;	/* placeholders */

			/* send CSI240~[x,y]\r with current input cursor location */

			/* FIXME: verify no leading char or button sequence */
			/* FIXME: should we ever send an eight-bit CSI? */
			/* FIXME: verify in absolute, not user, coordinates */
			TRACE(("sending multi-mode input report at %d,%d\n",
			       x, y));
			sprintf(reply, "[%d,%d]\r", x, y);
			unparseputs(context->display_graphic->xw, reply);
			unparse_end(context->display_graphic->xw);
			break;
		    } else {
			char ch;

			/* FIXME: wait for first non-arrow keypress or mouse click, and don't update graphics while waiting */
			ch = ' ';	/* placeholder */
			x = y = 0;	/* placeholders */

			/* send <key or button>[x,y]\r to report input cursor location */

			/* null button: CSI240~ */
			/* left button: CSI241~ */
			/* middle button: CSI243~ */
			/* right button: CSI245~ */
			/* extra button: CSI247~ */
			/* FIXME: support DECLBD to change button assignments */
			/* FIXME: verify no leading char or button sequence */
			TRACE(("sending one-shot input report with %c at %d,%d\n",
			       ch, x, y));
#if 0				/* FIXME - dead code */
			if (ch == '\r') {
			    /* Return only reports the location. */
			    sprintf(reply, "[%d,%d]\r", x, y);
			} else if (ch == '\177') {
			    /* DEL exits locator mode reporting nothing. */
			    sprintf(reply, "\r");
			} else
#endif
			{
			    sprintf(reply, "%c[%d,%d]\r", ch, x, y);
			}
			unparseputs(context->display_graphic->xw, reply);
			unparse_end(context->display_graphic->xw);
			/* FIXME: exit one-shot mode and disable input cursor */
			break;
		    }
		}
	    }
	    break;
	default:
	    TRACE(("DATA_ERROR: sending empty report for unknown ReGIS report command option '%c' arg \"%s\"\n",
		   state->option, fragment_to_tempstr(&optionarg)));
	    /* Unknown report request types must receive empty reports. */
	    unparseputs(context->display_graphic->xw, "\r");
	    unparse_end(context->display_graphic->xw);
	    break;
	}
	break;
    case 's':
	TRACE(("inspecting screen option \"%c\" with value \"%s\"\n",
	       state->option, fragment_to_tempstr(&optionarg)));
	switch (state->option) {
	case 'A':
	case 'a':
	    TRACE(("found address definition \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment address_extent;
		int got_ul = 0;
		int got_lr = 0;
		int ulx = 0, uly = 0, lrx = 0, lry = 0;

		while (!fragment_consumed(&optionarg)) {
		    if (skip_regis_whitespace(&optionarg))
			continue;

		    if (extract_regis_extent(&optionarg, &address_extent)) {
			int x, y;

			/* FIXME: are relative values supposed to be handled? */
			if (!load_regis_pixel_extent(fragment_to_tempstr(&address_extent),
						     0, 0, &x, &y)) {
			    TRACE(("DATA_ERROR: unable to parse extent in address definition: \"%s\"\n",
				   fragment_to_tempstr(&address_extent)));
			    break;
			}

			if (!got_ul) {
			    ulx = x;
			    uly = y;
			    got_ul = 1;
			} else if (!got_lr) {
			    lrx = x;
			    lry = y;
			    got_lr = 1;
			} else {
			    TRACE(("DATA_ERROR: ignoring extra extent argument in address definition: \"%s\"\n",
				   fragment_to_tempstr(&address_extent)));
			}
			continue;
		    }

		    TRACE(("DATA_ERROR: ignoring malformed ReGIS screen address definition: expected extent argument but found: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    return 1;
		}

		if (!got_ul || !got_lr) {
		    TRACE(("DATA_ERROR: ignoring malformed ReGIS screen address definition: one or both locations missing in definition\n"));
		    return 1;
		}
		if (ulx == lrx || uly == lry) {
		    TRACE(("DATA_ERROR: ignoring malformed ReGIS screen address definition: one or both dimensions are zero: ul=%d,%d lr=%d,%d\n",
			   ulx, uly, lrx, lry));
		    return 1;
		} {
		    const int cw = abs(ulx - lrx) + 1;
		    const int ch = abs(uly - lry) + 1;
		    int width, height;

		    /*
		     * FIXME: Should we attempt to resize existing contents?
		     * We are actually changing the output size, but terminals
		     * just changed coordinates.
		     */
#if 1
		    int scale;
		    const int mw = context->destination_graphic->max_width;
		    const int mh = context->destination_graphic->max_height;

		    TRACE(("custom screen size pre scaling: %dx%d\n", cw, ch));

		    width = cw;
		    height = ch;

		    scale = 1;
		    while (width * scale < 200 || height * scale < 200) {
			scale++;
		    }
		    width *= scale;
		    height *= scale;

		    scale = 1;
		    while (width / scale > mw || height / scale > mh) {
			scale++;
		    }
		    width /= scale;
		    height /= scale;
#else
		    width = context->width;
		    height = context->height;
#endif

		    TRACE(("custom screen address: ul=%d,%d lr=%d,%d\n",
			   ulx, uly, lrx, lry));

		    context->x_off = ulx;
		    context->y_off = uly;
		    context->x_div = lrx - ulx;
		    context->y_div = lry - uly;
		    context->width = width;
		    context->height = height;
		    context->destination_graphic->actual_width = width;
		    context->destination_graphic->actual_height = height;
		    context->destination_graphic->dirty = 1;

		    TRACE(("conversion factors: off=%+d,%+d div=%+d,%+d width=%d, height=%d\n",
			   context->x_off, context->y_off,
			   context->x_div, context->y_div,
			   context->width, context->height));
		}
	    }
	    break;
	case 'C':
	case 'c':
	    TRACE(("found cursor control \"%s\" FIXME\n",
		   fragment_to_tempstr(&optionarg)));
	    /* FIXME: handle */
	    /* C0 == output cursor off, C1 == output cursor on */
	    /* C(H0) == output cursor diamond (default), C(H1) == output cursor diamond, C(H2) == output cursor crosshair */
	    /* C(I) == input cursor crosshair (default), C(I0) == input cursor crosshair, C(I1) == input cursor diamond, C(I2) == input cursor crosshair, C(I3) == input cursor rubber band line, C(I4) == input cursor rubber band rectangle */
	    /* C(I[X,Y]"FB")) == set input cursor to F in foreground B in background with hotspot at X,Y (using current text settings, trimmed to 16x24 max) */
	    if (fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring malformed ReGIS screen cursor control option value \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		return 1;
	    }
	    break;
	case 'E':
	case 'e':
	    TRACE(("found erase request \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected argument to ReGIS erase request \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		return 1;
	    }
	    DRAW_ALL(context, context->background);
	    /* FIXME: also set origin to 0,0 (presumably upper-left address, or maybe addressing is reset) */
	    context->fill_point_count = 0U;
	    context->fill_mode = 0;
	    state->num_points = 0U;
	    state->stack_next = 0U;
	    context->destination_graphic->dirty = 1;
	    context->force_refresh = 1;
	    break;
	case 'F':
	case 'f':
	    TRACE(("found page eject request \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected argument to ReGIS page eject request \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		return 1;
	    }
	    /* We aren't going to print anything so no need to deduplicate. */
	    DRAW_ALL(context, context->background);
	    context->destination_graphic->dirty = 1;
	    context->force_refresh = 1;
	    break;
	case 'H':
	case 'h':
	    TRACE(("found hardcopy control \"%s\" FIXME\n",
		   fragment_to_tempstr(&optionarg)));
	    /* FIXME: handle */
	    /* screen S(H), area to (input?/output?) cursor S(H[X,Y]), or area S(H[X,Y][X,Y]) */
	    if (fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring malformed ReGIS screen hardcopy control option value \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		return 1;
	    }
	    break;
	case 'I':
	case 'i':
	    TRACE(("found screen background color index \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!load_regis_regnum_or_colorspec(context, &optionarg,
						&context->background)) {
		TRACE(("DATA_ERROR: screen background color specifier not recognized: \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		return 1;
	    }
	    break;
	case 'M':
	case 'm':
	    TRACE(("found screen color register mapping \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment regnum;
		RegisDataFragment colorspec;

		while (!fragment_consumed(&optionarg)) {
		    if (skip_regis_whitespace(&optionarg))
			continue;

		    if (extract_regis_num(&optionarg, &regnum)) {
			int register_num;
			int color_only;
			short r, g, b;

			if (!regis_num_to_int(&regnum, &register_num)) {
			    TRACE(("DATA_ERROR: unable to parse int in screen color register mapping option: \"%s\"\n",
				   fragment_to_tempstr(&regnum)));
			    return 1;
			}
			if (register_num < 0 ||
			    register_num > (int) context->destination_graphic->valid_registers) {
			    TRACE(("interpreting out of range register number %d as 0 FIXME\n",
				   register_num));
			    register_num = 0;
			}
			skip_regis_whitespace(&optionarg);
			if (!extract_regis_parenthesized_data(&optionarg,
							      &colorspec)) {
			    TRACE(("DATA_ERROR: expected to find parentheses after register number: \"%s\"\n",
				   fragment_to_tempstr(&optionarg)));
			    return 1;
			}

			skip_regis_whitespace(&colorspec);
			switch (peek_fragment(&colorspec)) {
			case 'A':
			case 'a':
			    pop_fragment(&colorspec);
			    color_only = 1;
			    break;
			default:
			    color_only = 0;
			    break;
			}

			TRACE(("mapping register %d to color spec: \"%s\"\n",
			       register_num, fragment_to_tempstr(&colorspec)));
			if (!load_regis_colorspec(context, &colorspec,
						  &r, &g, &b)) {
			    TRACE(("DATA_ERROR: unable to use colorspec for mapping of register %d\n",
				   register_num));
			    return 1;
			}

			if (color_only &&
			    (context->terminal_id == 240 ||
			     context->terminal_id == 330)) {
			    TRACE(("NOT setting color register %d to %hd,%hd,%hd\n",
				   register_num, r, g, b));
			} else {
			    TRACE(("setting color register %d to %hd,%hd,%hd\n",
				   register_num, r, g, b));
			    update_color_register(context->destination_graphic,
						  (RegisterNum) register_num,
						  r, g, b);
			}
			continue;
		    } {
			char skip;

			skip_regis_whitespace(&optionarg);
			skip = pop_fragment(&optionarg);
			(void) skip;	/* variable needed only if tracing */
			TRACE(("DATA_ERROR: ignoring mapping request with unexpected character \"%c\"\n",
			       skip));
			return 1;
		    }
		}
	    }
	    break;
	case 'P':
	case 'p':
	    TRACE(("found graphics page display option \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment pagearg;
		int page;

		if (!extract_regis_num(&optionarg, &pagearg)) {
		    TRACE(("DATA_ERROR: expected int in page display option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("page option arg: %s\n", fragment_to_tempstr(&pagearg)));
		if (!regis_num_to_int(&pagearg, &page)) {
		    TRACE(("DATA_ERROR: unable to parse int in page display option: \"%s\"\n",
			   fragment_to_tempstr(&pagearg)));
		    break;
		}
		if (page < 0 || (unsigned) page >= MAX_REGIS_PAGES) {
		    TRACE(("DATA_ERROR: invalid page: \"%d\"\n", page));
		    break;
		}

		TRACE(("using display page number: %d\n", page));
		context->display_page = (unsigned) page;
		map_regis_graphics_pages(context->display_graphic->xw, context);
	    }
	    break;
	case 'T':
	case 't':
	    TRACE(("found time delay \"%s\" FIXME\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment delayarg;
		int delay;

		if (!extract_regis_num(&optionarg, &delayarg)) {
		    TRACE(("DATA_ERROR: expected int in delay display option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("delay option arg: %s\n",
		       fragment_to_tempstr(&delayarg)));
		if (!regis_num_to_int(&delayarg, &delay)) {
		    TRACE(("DATA_ERROR: unable to parse int in delay display option: \"%s\"\n",
			   fragment_to_tempstr(&delayarg)));
		    break;
		}
		if (delay < 0 || delay > 32767) {
		    TRACE(("DATA_ERROR: delay out of range: \"%d\"\n", delay));
		    break;
		}
		/* FIXME: terminals allow much larger values, but this is an easy DoS vector */
		if (delay > 60)
		    delay = 60;
		TRACE(("using delay for %d ticks\n", delay));
		refresh_modified_displayed_graphics(context->current_widget);
		usleep((useconds_t) delay * 10000U);
	    }
	    break;
	case 'W':
	case 'w':
	    /* Only M (pixel vector multiplier) is useful -- for the scrolling multiplier. */
	    TRACE(("found temporary write options \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!load_regis_write_control_set(state, context,
					      context->graphics_output_cursor_x,
					      context->graphics_output_cursor_y,
					      &optionarg,
					      &context->temporary_write_controls)) {
		TRACE(("DATA_ERROR: invalid temporary write options \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		break;
	    }
	    break;
	default:
	    TRACE(("DATA_ERROR: ignoring unknown ReGIS screen command option '%c' arg \"%s\"\n",
		   state->option, fragment_to_tempstr(&optionarg)));
	    break;
	}
	break;
    case 't':
	TRACE(("inspecting text option \"%c\" with value \"%s\"\n",
	       state->option, fragment_to_tempstr(&optionarg)));
	switch (state->option) {
	case 'A':
	case 'a':
	    TRACE(("found alphabet specifier option \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment alphabetarg;
		int alphabet;

		if (!extract_regis_num(&optionarg, &alphabetarg)) {
		    TRACE(("DATA_ERROR: expected int in text alphabet option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("alphabet: %s\n", fragment_to_tempstr(&alphabetarg)));
		if (!regis_num_to_int(&alphabetarg, &alphabet)) {
		    TRACE(("DATA_ERROR: unable to parse int in text alphabet option: \"%s\"\n",
			   fragment_to_tempstr(&alphabetarg)));
		    break;
		}
		if (alphabet < 0 ||
		    (unsigned) alphabet >= MAX_REGIS_ALPHABETS) {
		    TRACE(("DATA_ERROR: invalid alphabet: \"%d\"\n", alphabet));
		    break;
		}

		TRACE(("using alphabet number: %d\n", alphabet));
		context->current_text_controls->alphabet_num =
		    (unsigned) alphabet;

		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: ignoring trailing junk in text alphabet option \"%s\"\n",
			   fragment_to_tempstr(&alphabetarg)));
		    break;
		}
	    }
	    break;
	case 'B':
	case 'b':
	    TRACE(("found beginning of temporary text control \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    copy_regis_text_controls(&context->persistent_text_controls,
				     &context->temporary_text_controls);
	    context->current_text_controls = &context->temporary_text_controls;
	    break;
	case 'D':
	case 'd':
	    TRACE(("found text tilt control \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment rotationarg;
		int rotation;

		if (!extract_regis_num(&optionarg, &rotationarg)) {
		    TRACE(("DATA_ERROR: expected int in text tilt option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("tilt: %s\n", fragment_to_tempstr(&rotationarg)));
		if (!regis_num_to_int(&rotationarg, &rotation)) {
		    TRACE(("DATA_ERROR: unable to parse int in text tilt option: \"%s\"\n",
			   fragment_to_tempstr(&rotationarg)));
		    break;
		}
		while (rotation < 0) {
		    rotation += 360;
		}
		while (rotation >= 360) {
		    rotation -= 360;
		}
		/* FIXME: we don't have to be this restrictive, though the
		 * VT3x0 apparently was. What might depend on this?
		 */
#ifndef ENABLE_FREE_ROTATION
		/* Use closest value which is a multiple of 45 degrees. */
		rotation = 45 * ((rotation + 22) / 45);
#endif

		/* For some reason ReGIS reused the "D" option for the text
		 * command to represent two different attributes.  String tilt
		 * can only be modified if an "S" option is given after the
		 * "D" option.  In that case a second "D" option changes the
		 * character tilt.  But complicating things further, if no "S"
		 * or second "D" option is given, the "D" option refers to the
		 * character tilt.
		 */
		switch (state->text_tilt_state) {
		case TEXT_TILT_STATE_READY:
		    /* Setting a tilt after a cell size only affects the
		     * character tilt and not the string tilt.
		     */
		    TRACE(("character rotation (direction): %d\n", rotation));
		    context->current_text_controls->character_rotation
			= rotation;
		    state->text_tilt_state = TEXT_TILT_STATE_GOT_D;
		    break;
		case TEXT_TILT_STATE_GOT_D:
		    /* If there are multiple angles with no size only the last
		     * value is used.
		     */
		    TRACE(("character rotation (direction): %d\n", rotation));
		    context->current_text_controls->character_rotation
			= rotation;
		    break;
		case TEXT_TILT_STATE_GOT_DS:
		    TRACE(("changing character rotation (direction): %d\n",
			   rotation));
		    context->current_text_controls->character_rotation
			= rotation;
		    state->text_tilt_state = TEXT_TILT_STATE_GOT_DSD;
		    break;
		case TEXT_TILT_STATE_GOT_DSD:
		default:
		    /* If there are multiple angles with no size only the last
		     * value is used.
		     */
		    TRACE(("changing character rotation (direction): %d\n",
			   rotation));
		    context->current_text_controls->character_rotation
			= rotation;
		    break;
		}

		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: ignoring trailing junk in text tilt option \"%s\"\n",
			   fragment_to_tempstr(&rotationarg)));
		    break;
		}
	    }
	    break;
	case 'E':
	case 'e':
	    TRACE(("found end of temporary text control \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    context->current_text_controls = &context->persistent_text_controls;
	    break;
	case 'H':
	case 'h':
	    TRACE(("found height multiplier \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment multiarg;
		int multiplier;
		unsigned height;

		if (!extract_regis_num(&optionarg, &multiarg)) {
		    TRACE(("DATA_ERROR: expected int in text height multiplier option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("multiplier: %s\n", fragment_to_tempstr(&multiarg)));
		if (!regis_num_to_int(&multiarg, &multiplier)) {
		    TRACE(("DATA_ERROR: unable to parse int in text height multiplier option: \"%s\"\n",
			   fragment_to_tempstr(&multiarg)));
		    break;
		}
		if (multiplier < 0) {
		    TRACE(("DATA_ERROR: out of range height multiplier: \"%d\", using 0 FIXME\n",
			   multiplier));
		    multiplier = 0;	/* FIXME: verify zero is accepted */
		}
		if (multiplier > 256) {
		    TRACE(("DATA_ERROR: out of range height multiplier: \"%d\", using 256 FIXME\n",
			   multiplier));
		    multiplier = 256;
		}
		TRACE(("using height multiplier: %d\n", multiplier));
		height = (unsigned) multiplier *10U;	/* base character height */
		context->current_text_controls->character_display_h
		    = height;
		context->current_text_controls->character_unit_cell_h
		    = height;

		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: ignoring trailing junk in text multiplier option \"%s\"\n",
			   fragment_to_tempstr(&multiarg)));
		    break;
		}
	    }
	    break;
	case 'I':
	case 'i':
	    TRACE(("found italic control \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment italicarg;
		int italic;

		if (!extract_regis_num(&optionarg, &italicarg)) {
		    TRACE(("DATA_ERROR: expected int in text italic option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("italic angle: %s\n", fragment_to_tempstr(&italicarg)));
		if (!regis_num_to_int(&italicarg, &italic)) {
		    TRACE(("DATA_ERROR: unable to parse int in text italic option: \"%s\"\n",
			   fragment_to_tempstr(&italicarg)));
		    break;
		}

		/*
		 * This is overly-restrictive but matches what the docs say
		 * should happen.  Add an option to allow exact angles?
		 */
#ifndef ENABLE_VARIABLE_ITALICS
		if (italic <= -31) {
		    italic = -45;
		} else if (italic < 0) {
		    italic = -27;	/* docs say 22, but that gives .404 x:y ratio */
		} else if (italic >= 31) {
		    italic = 45;
		} else if (italic > 0) {
		    italic = 27;	/* docs say 22, but that gives .404 x:y ratio */
		}
#else
		if (italic <= -72) {
		    italic = -72;
		} else if (italic >= 72) {
		    italic = 72;
		}
#endif

		TRACE(("using italic angle: %d\n", italic));
		context->current_text_controls->slant = italic;

		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: ignoring trailing junk in text italic option \"%s\"\n",
			   fragment_to_tempstr(&italicarg)));
		    break;
		}
	    }
	    break;
	case 'M':
	case 'm':
	    TRACE(("found text command unit size multiplier \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment sizemultiplierarg;
		int sizemultiplier;
		int ww, hh;

		if (!extract_regis_extent(&optionarg, &sizemultiplierarg)) {
		    TRACE(("DATA_ERROR: expected extent in unit size multiplier option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("size multiplier: %s\n",
		       fragment_to_tempstr(&sizemultiplierarg)));
		/* FIXME: verify that relative values don't work */
		if (!load_regis_mult_extent(fragment_to_tempstr(&sizemultiplierarg),
					    &ww, &hh)) {
		    TRACE(("DATA_ERROR: unable to parse extent in '%c' command: \"%s\"\n",
			   state->option, fragment_to_tempstr(&sizemultiplierarg)));
		    break;
		}
		/* FIXME: support fractional values */
		if (!regis_num_to_int(&sizemultiplierarg, &sizemultiplier)) {
		    TRACE(("DATA_ERROR: unable to parse extent in size multiplier option: \"%s\"\n",
			   fragment_to_tempstr(&sizemultiplierarg)));
		    break;
		}
		if (ww < 1 || hh < 1) {
		    TRACE(("DATA_ERROR: invalid size multiplier: %d,%d FIXME\n",
			   ww, hh));
		    break;
		}
		if (ww > 16) {
		    ww = 16;
		}
		if (hh > 16) {
		    hh = 16;
		}

		TRACE(("using unit size multiplier: %d,%d\n", ww, hh));

		/* Times the base character unit cell dimensions (the VT3x0
		 * manual says this is with the height multiplier, but that
		 * seems to be incorrect).
		 */
		context->current_text_controls->character_unit_cell_w =
		    (unsigned) ww *8U;
		context->current_text_controls->character_unit_cell_h =
		    (unsigned) hh *10U;

		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: ignoring trailing junk in text unit cell size option \"%s\"\n",
			   fragment_to_tempstr(&sizemultiplierarg)));
		    break;
		}
	    }
	    break;
	case 'S':
	case 's':
	    TRACE(("found display size or standard character cell size \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    for (;;) {
		RegisDataFragment displaysizearg;

		skip_regis_whitespace(&optionarg);
		if (extract_regis_extent(&optionarg, &displaysizearg)) {
		    int disp_w, disp_h;

		    TRACE(("custom display size: %s\n",
			   fragment_to_tempstr(&displaysizearg)));
		    /* FIXME: verify that relative values don't work */
		    if (!load_regis_mult_extent(fragment_to_tempstr(&displaysizearg),
						&disp_w, &disp_h)) {
			TRACE(("DATA_ERROR: unable to parse extent in text display size option: \"%s\"\n",
			       fragment_to_tempstr(&displaysizearg)));
			break;
		    }
		    if (disp_w < 1 || disp_h < 1) {
			TRACE(("DATA_ERROR: invalid text display size: %dx%d FIXME\n",
			       disp_w, disp_h));
			break;
		    }

		    TRACE(("using display cell size: %d,%d\n", disp_w, disp_h));
		    context->current_text_controls->character_display_w =
			(unsigned) disp_w;
		    context->current_text_controls->character_display_h =
			(unsigned) disp_h;
		    TRACE(("using offset: %d,%d\n", disp_w, 0));
		    context->current_text_controls->character_inc_x = disp_w;
		    context->current_text_controls->character_inc_y = 0;

		    continue;
		}

		if (extract_regis_num(&optionarg, &displaysizearg)) {
		    int standard;
		    unsigned disp_w, disp_h, unit_w, unit_h;
		    int off_x, off_y;

		    TRACE(("standard display cell size: %s\n",
			   fragment_to_tempstr(&displaysizearg)));
		    if (!regis_num_to_int(&displaysizearg, &standard)) {
			TRACE(("DATA_ERROR: unable to parse int in text standard cell size option: \"%s\"\n",
			       fragment_to_tempstr(&displaysizearg)));
			break;
		    }
		    if (get_standard_character_size(standard,
						    &disp_w, &disp_h,
						    &unit_w, &unit_h,
						    &off_x, &off_y)) {
			TRACE(("DATA_ERROR: unrecognized standard cell size: \"%d\"\n",
			       standard));
			break;
		    }

		    TRACE(("using display cell size: %u,%u\n", disp_w, disp_h));
		    context->current_text_controls->character_display_w
			= disp_w;
		    context->current_text_controls->character_display_h
			= disp_h;
		    TRACE(("using offset: %d,%d\n", off_x, off_y));
		    context->current_text_controls->character_inc_x = off_x;
		    context->current_text_controls->character_inc_y = off_y;

		    /*
		     * Some ReGIS documentation implies that the "S" option only
		     * affects character spacing after a rotation option ("ReGIS
		     * uses the spacing value associated with the cell size to
		     * space the characters in the tilted string").  The 7-13
		     * example in the VT330/VT340 Programmer Reference Manual
		     * vol 2 appears to say otherwise.  FIXME: verify
		     */
		    if (1) {	/* forced for now */
			TRACE(("using unit cell size: %u,%u\n", unit_w, unit_h));
			context->current_text_controls->character_unit_cell_w =
			    unit_w;
			context->current_text_controls->character_unit_cell_h =
			    unit_h;
		    }

		    switch (state->text_tilt_state) {
		    case TEXT_TILT_STATE_READY:
			/* Nothing to do. */
			break;
		    case TEXT_TILT_STATE_GOT_D:
			TRACE(("upgrading character rotation to string and character rotation: %d\n", context->current_text_controls->character_rotation));
			context->current_text_controls->string_rotation
			    = context->current_text_controls->character_rotation;
			state->text_tilt_state = TEXT_TILT_STATE_GOT_DS;
			break;
		    case TEXT_TILT_STATE_GOT_DS:
			/* FIXME: It isn't clear what to do if there are two
			 * size options in a row after a tilt option.
			 */
			TRACE(("DATA_ERROR: unexpected duplicate size option: \"%s\" (state=%d)\n",
			       fragment_to_tempstr(&displaysizearg),
			       state->text_tilt_state));
			break;
		    case TEXT_TILT_STATE_GOT_DSD:
		    default:
			/* FIXME: It isn't clear what to do if there is a size
			 * option after both types of tilt angle have been set.
			 */
			TRACE(("DATA_ERROR: unexpected duplicate size option: \"%s\" (state=%d)\n",
			       fragment_to_tempstr(&displaysizearg),
			       state->text_tilt_state));
			break;
		    }
		    continue;
		}

		if (skip_regis_whitespace(&optionarg)) {
		    continue;
		}

		if (fragment_consumed(&optionarg)) {
		    break;
		}

		TRACE(("DATA_ERROR: expected int or extent in text display size option: \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		break;
	    }
	    break;
	case 'U':
	case 'u':
	    TRACE(("found text command custom unit cell size \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    {
		RegisDataFragment unitsizearg;
		int unitsize;
		int unit_w, unit_h;

		if (!extract_regis_extent(&optionarg, &unitsizearg)) {
		    TRACE(("DATA_ERROR: expected extent in text unit cell size option: \"%s\"\n",
			   fragment_to_tempstr(&optionarg)));
		    break;
		}
		TRACE(("unitsize cell size: %s\n",
		       fragment_to_tempstr(&unitsizearg)));
		/* FIXME: verify that relative values don't work */
		if (!load_regis_mult_extent(fragment_to_tempstr(&unitsizearg),
					    &unit_w, &unit_h)) {
		    TRACE(("DATA_ERROR: unable to parse extent in '%c' command: \"%s\"\n",
			   state->option, fragment_to_tempstr(&unitsizearg)));
		    break;
		}
		if (!regis_num_to_int(&unitsizearg, &unitsize)) {
		    TRACE(("DATA_ERROR: unable to parse extent in text unit cell size option: \"%s\"\n",
			   fragment_to_tempstr(&unitsizearg)));
		    break;
		}
		if (unit_w < 1 || unit_h < 1) {
		    TRACE(("DATA_ERROR: invalid text unit cell size: %dx%d FIXME\n",
			   unit_w, unit_h));
		    break;
		}

		TRACE(("using unit cell size: %d,%d\n", unit_w, unit_h));

		context->current_text_controls->character_unit_cell_w =
		    (unsigned) unit_w;
		context->current_text_controls->character_unit_cell_h =
		    (unsigned) unit_h;

		skip_regis_whitespace(&optionarg);
		if (!fragment_consumed(&optionarg)) {
		    TRACE(("DATA_ERROR: ignoring trailing junk in text unit cell size option \"%s\"\n",
			   fragment_to_tempstr(&unitsizearg)));
		    break;
		}
	    }
	    break;
	case 'W':
	case 'w':
	    TRACE(("found temporary write options \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!load_regis_write_control_set(state, context,
					      context->graphics_output_cursor_x,
					      context->graphics_output_cursor_y,
					      &optionarg,
					      &context->temporary_write_controls)) {
		TRACE(("DATA_ERROR: invalid temporary write options \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
		break;
	    }
	    break;
	default:
	    TRACE(("DATA_ERROR: ignoring unknown ReGIS text command option '%c' arg \"%s\"\n",
		   state->option, fragment_to_tempstr(&optionarg)));
	    break;
	}
	break;
    case 'v':
	TRACE(("inspecting vector option \"%c\" with value \"%s\"\n",
	       state->option, fragment_to_tempstr(&optionarg)));
	switch (state->option) {
	case 'B':
	case 'b':
	    TRACE(("found begin bounded position stack \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    skip_regis_whitespace(&optionarg);
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected arguments to vector option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
	    }
	    if (state->stack_next >= POSITION_STACK_SIZE) {
		/* FIXME: ignore, error, update counter? */
		TRACE(("unable to push position to full stack\n"));
		break;
	    }
	    TRACE(("pushing location: %d,%d\n",
		   context->graphics_output_cursor_x,
		   context->graphics_output_cursor_y));

	    state->stack_x[state->stack_next] =
		context->graphics_output_cursor_x;
	    state->stack_y[state->stack_next] =
		context->graphics_output_cursor_y;
	    state->stack_next++;
	    break;
	case 'E':
	case 'e':
	    TRACE(("found end vector position stack \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    skip_regis_whitespace(&optionarg);
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected arguments to vector option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
	    }
	    if (state->stack_next == 0U) {
		TRACE(("DATA_ERROR: unable to pop position from empty stack\n"));
		break;
	    }

	    state->stack_next--;
	    if (state->stack_x[state->stack_next] != DUMMY_STACK_X ||
		state->stack_y[state->stack_next] != DUMMY_STACK_Y) {
		int orig_x, orig_y;

		orig_x = context->graphics_output_cursor_x;
		orig_y = context->graphics_output_cursor_y;
		context->graphics_output_cursor_x =
		    state->stack_x[state->stack_next];
		context->graphics_output_cursor_y =
		    state->stack_y[state->stack_next];
		TRACE(("popped location: %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));

		TRACE(("drawing line to popped location\n"));
		draw_patterned_line(context,
				    orig_x, orig_y,
				    context->graphics_output_cursor_x,
				    context->graphics_output_cursor_y);
	    } else {
		TRACE(("not popping location\n"));
	    }
	    break;
	case 'S':
	case 's':
	    TRACE(("found begin unbounded position stack \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    skip_regis_whitespace(&optionarg);
	    if (!fragment_consumed(&optionarg)) {
		TRACE(("DATA_ERROR: ignoring unexpected arguments to vector option '%c' arg \"%s\"\n",
		       state->option, fragment_to_tempstr(&optionarg)));
	    }
	    if (state->stack_next >= POSITION_STACK_SIZE) {
		/* FIXME: ignore, error, update counter? */
		TRACE(("unable to push dummy position to full stack\n"));
		break;
	    }

	    TRACE(("pushing dummy vector positions instead of %d,%d\n",
		   context->graphics_output_cursor_x,
		   context->graphics_output_cursor_y));
	    state->stack_x[state->stack_next] = DUMMY_STACK_X;
	    state->stack_y[state->stack_next] = DUMMY_STACK_Y;
	    state->stack_next++;
	    break;
	case 'W':
	case 'w':
	    TRACE(("found temporary write options \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!load_regis_write_control_set(state, context,
					      context->graphics_output_cursor_x,
					      context->graphics_output_cursor_y,
					      &optionarg,
					      &context->temporary_write_controls)) {
		TRACE(("DATA_ERROR: invalid temporary write options \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
	    }
	    break;
	default:
	    TRACE(("DATA_ERROR: ignoring unknown ReGIS vector command option '%c' arg \"%s\"\n",
		   state->option, fragment_to_tempstr(&optionarg)));
	    break;
	}
	break;
    case 'w':
	skip_regis_whitespace(&optionarg);
	TRACE(("inspecting permanent write option \"%c\" with value \"%s\"\n",
	       state->option, fragment_to_tempstr(&optionarg)));
	if (!load_regis_write_control(state, context,
				      context->graphics_output_cursor_x,
				      context->graphics_output_cursor_y,
				      state->option, &optionarg,
				      &context->persistent_write_controls)) {
	    TRACE(("DATA_ERROR: invalid write options\n"));
	    return 1;
	}
	break;
    default:
	TRACE(("DATA_ERROR: unexpected option in \"%c\" command: \"%s\"\n",
	       state->command, fragment_to_tempstr(&optionarg)));
	return 1;
    }

    return 1;
}

static int
expand_macrographs(RegisDataFragment *input, RegisGraphicsContext const *context)
{
    char operator;
    char name;

    (void) context;		/* to be used later */
    operator = get_fragment(input, 0U);
    if (operator != '@')
	return 0;
    name = get_fragment(input, 1U);
    if (islower(name))
	name = (char) toupper(name);
    if (name < 'A' || name > 'Z')
	return 0;

    TRACE(("expanding macrograph '%c' with %u characters FIXME\n", name, 0U));
    pop_fragment(input);
    pop_fragment(input);
    /* FIXME: implement */
    return 1;
}

static int
parse_regis_items(RegisParseState *state, RegisGraphicsContext *context)
{
    RegisDataFragment *const input = &state->input;
    RegisDataFragment item;

    if (fragment_consumed(input))
	return 0;

    if (extract_regis_extent(input, &item)) {
	TRACE(("found extent \"%s\"\n", fragment_to_tempstr(&item)));
	switch (state->command) {
	case 'c':
	    {
		int orig_x, orig_y;
		int new_x, new_y;

		if (state->num_points > 0) {
		    orig_x = state->x_points[state->num_points - 1];
		    orig_y = state->y_points[state->num_points - 1];
		} else {
		    orig_x = context->graphics_output_cursor_x;
		    orig_y = context->graphics_output_cursor_y;
		}
		if (!load_regis_coord_extent(context,
					     fragment_to_tempstr(&item),
					     orig_x, orig_y,
					     &new_x, &new_y)) {
		    TRACE(("DATA_ERROR: unable to parse extent in '%c' command: \"%s\"\n",
			   state->command, fragment_to_tempstr(&item)));
		    break;
		}

		switch (state->curve_mode) {
		case CURVE_POSITION_ARC_CENTER:
		case CURVE_POSITION_ARC_EDGE:
		    {
			double radians;
			int tenthdegs;
			int c_x, c_y;
			int e_x, e_y;
			int e_x_final = 0, e_y_final = 0;

			TRACE(("drawing arc: curve_mode=%d\n", state->curve_mode));
			TRACE(("drawing arc: new=%d,%d orig=%d,%d\n",
			       new_x, new_y, orig_x, orig_y));
			if (state->curve_mode == CURVE_POSITION_ARC_CENTER) {
			    c_x = new_x;
			    c_y = new_y;
			    e_x = orig_x;
			    e_y = orig_y;
			} else {
			    c_x = orig_x;
			    c_y = orig_y;
			    e_x = new_x;
			    e_y = new_y;
			}

			radians = atan2((double) (c_y - e_y),
					(double) (e_x - c_x));
			tenthdegs = (int) (0.5 + 3600.0 * radians / (2.0 * M_PI));
			if (tenthdegs < 0)
			    tenthdegs += 3600;

			TRACE(("drawing arc centered at location %d,%d to location %d,%d from %g degrees (%g radians) for %d degrees\n",
			       c_x, c_y,
			       e_x, e_y,
			       tenthdegs / 10., radians, state->arclen));
			draw_patterned_arc(context,
					   c_x, c_y,
					   e_x, e_y,
					   tenthdegs, state->arclen * 10,
					   &e_x_final, &e_y_final);

#ifdef DEBUG_ARC_CENTER
			DRAW_PIXEL(context, c_x + 1, c_y, 3U);
			DRAW_PIXEL(context, c_x - 1, c_y, 3U);
			DRAW_PIXEL(context, c_x, c_y + 1, 3U);
			DRAW_PIXEL(context, c_x, c_y - 1, 3U);
			DRAW_PIXEL(context, c_x, c_y, 3U);
#endif

#ifdef DEBUG_ARC_START
			DRAW_PIXEL(context, e_x + 1, e_y, 2U);
			DRAW_PIXEL(context, e_x - 1, e_y, 2U);
			DRAW_PIXEL(context, e_x, e_y + 1, 2U);
			DRAW_PIXEL(context, e_x, e_y - 1, 2U);
			DRAW_PIXEL(context, e_x, e_y, 2U);
#endif

#ifdef DEBUG_ARC_END
			DRAW_PIXEL(context, e_x_final + 1, e_y_final + 1, 1U);
			DRAW_PIXEL(context, e_x_final + 1, e_y_final - 1, 1U);
			DRAW_PIXEL(context, e_x_final - 1, e_y_final + 1, 1U);
			DRAW_PIXEL(context, e_x_final - 1, e_y_final - 1, 1U);
			DRAW_PIXEL(context, e_x_final, e_y_final, 1U);
#endif

			if (state->curve_mode == CURVE_POSITION_ARC_CENTER) {
			    TRACE(("moving cursor to final point on arc %d,%d\n",
				   e_x_final, e_y_final));
			    if (state->num_points > 0) {
				state->x_points[state->num_points - 1] =
				    e_x_final;
				state->y_points[state->num_points - 1] =
				    e_y_final;
			    }
			    context->graphics_output_cursor_x = e_x_final;
			    context->graphics_output_cursor_y = e_y_final;
			}
		    }
		    break;
		case CURVE_POSITION_OPEN_CURVE:
		case CURVE_POSITION_CLOSED_CURVE:
		    if (state->num_points >= MAX_INPUT_CURVE_POINTS) {
			TRACE(("DATA_ERROR: got curve point, but already have max points (%d)\n",
			       state->num_points));
			break;
		    }
		    state->x_points[state->num_points] = new_x;
		    state->y_points[state->num_points] = new_y;
		    state->num_points++;
		    TRACE(("adding point to curve with location %d,%d\n",
			   new_x, new_y));
		    break;
		default:
		    TRACE(("ERROR: got position, but curve mode %d is unknown\n",
			   state->curve_mode));
		    break;
		}
	    }
	    break;
	case 'p':
	    if (!load_regis_coord_extent(context,
					 fragment_to_tempstr(&item),
					 context->graphics_output_cursor_x,
					 context->graphics_output_cursor_y,
					 &context->graphics_output_cursor_x,
					 &context->graphics_output_cursor_y)) {
		TRACE(("DATA_ERROR: unable to parse extent in '%c' command: \"%s\"\n",
		       state->command, fragment_to_tempstr(&item)));
		break;
	    }
	    TRACE(("moving pen to location %d,%d\n",
		   context->graphics_output_cursor_x,
		   context->graphics_output_cursor_y));
	    break;
	case 's':
	    TRACE(("extent scroll argument to screen command: \"%s\"\n",
		   fragment_to_tempstr(&item)));
	    {
		int old_ul_x, old_ul_y;
		int new_ul_x, new_ul_y;
		int copy_w, copy_h;

		old_ul_x = 0;
		old_ul_y = 0;
		TRACE(("current upper-left coordinate in pixel coordinates: %d,%d\n",
		       old_ul_x, old_ul_y));
		/* FIXME: verify this is in user coordinates, not pixels */
		if (!load_regis_coord_extent(context,
					     fragment_to_tempstr(&item),
					     old_ul_x, old_ul_y,
					     &new_ul_x, &new_ul_y)) {
		    TRACE(("DATA_ERROR: unable to parse extent in '%c' command: \"%s\"\n",
			   state->command, fragment_to_tempstr(&item)));
		    break;
		}
		TRACE(("scrolling image to updated upper-left coordinate in pixel coordinates: %d,%d\n",
		       new_ul_x, new_ul_y));

		/* FIXME: does any write mode affect revealed background? */
		if (new_ul_x > 0)
		    copy_w = context->width - new_ul_x;
		else
		    copy_w = context->width;
		if (new_ul_y > 0)
		    copy_h = context->height - new_ul_y;
		else
		    copy_h = context->height;
		/* FIXME: verify this applies to write page, not display page */
		copy_overlapping_area(context->destination_graphic,
				      new_ul_x, new_ul_y,
				      0, 0,
				      (unsigned) copy_w, (unsigned) copy_h,
				      context->background);
		context->destination_graphic->dirty = 1;
		context->force_refresh = 1;
	    }
	    break;
	case 't':
	    /* FIXME: verify this is in pixels, not user coordinates */
	    if (!load_regis_pixel_extent(fragment_to_tempstr(&item),
					 0, 0,
					 &context->current_text_controls->character_inc_x,
					 &context->current_text_controls->character_inc_y)) {
		TRACE(("DATA_ERROR: unable to parse extent in '%c' command: \"%s\"\n",
		       state->command, fragment_to_tempstr(&item)));
		break;
	    }
	    TRACE(("setting character spacing to %d,%d\n",
		   context->current_text_controls->character_inc_x,
		   context->current_text_controls->character_inc_y));
	    break;
	case 'v':
	    {
		int orig_x, orig_y;

		orig_x = context->graphics_output_cursor_x;
		orig_y = context->graphics_output_cursor_y;
		if (!load_regis_coord_extent(context,
					     fragment_to_tempstr(&item),
					     orig_x, orig_y,
					     &context->graphics_output_cursor_x,
					     &context->graphics_output_cursor_y)) {
		    TRACE(("DATA_ERROR: unable to parse extent in '%c' command: \"%s\"\n",
			   state->command, fragment_to_tempstr(&item)));
		    break;
		}
		TRACE(("drawing line to location %d,%d\n",
		       context->graphics_output_cursor_x,
		       context->graphics_output_cursor_y));
		draw_patterned_line(context,
				    orig_x, orig_y,
				    context->graphics_output_cursor_x,
				    context->graphics_output_cursor_y);
	    }
	    break;
	default:
	    TRACE(("DATA_ERROR: unexpected extent in \"%c\" command: \"%s\"\n",
		   state->command, fragment_to_tempstr(&item)));
	    break;
	}
	return 1;
    }

    if (state->command != 'l' && extract_regis_pixelvector(input, &item)) {
	TRACE(("found pixel vector \"%s\"\n", fragment_to_tempstr(&item)));
	switch (state->command) {
	case 'c':
	    /* FIXME: parse, handle */
	    TRACE(("pixelvector in curve command FIXME\n"));
	    break;
	    /* FIXME: not sure if 'f' supports PVs */
	case 'p':
	    /* FIXME: error checking */
	    if (!load_regis_coord_pixelvector(context,
					      fragment_to_tempstr(&item),
					      context->graphics_output_cursor_x,
					      context->graphics_output_cursor_y,
					      &context->graphics_output_cursor_x,
					      &context->graphics_output_cursor_y)) {
		TRACE(("DATA_ERROR: unable to parse pixel vector in '%c' command: \"%s\"\n",
		       state->command, fragment_to_tempstr(&item)));
		break;
	    }
	    TRACE(("moving pen to location %d,%d\n",
		   context->graphics_output_cursor_x,
		   context->graphics_output_cursor_y));
	    break;
	case 's':
	    TRACE(("pixelvector scroll argument to screen command: \"%s\"\n",
		   fragment_to_tempstr(&item)));
	    {
		int old_ul_x, old_ul_y;
		int new_ul_x, new_ul_y;
		int copy_w, copy_h;

		old_ul_x = 0;
		old_ul_y = 0;
		TRACE(("current upper-left coordinate in pixel coordinates: %d,%d\n",
		       old_ul_x, old_ul_y));
		/* FIXME: verify this is in user coordinates, not pixels */
		/* FIXME: verify that multiple PV digits result in a single
		 * copy (this changes the contents of any exposed edge)
		 */
		if (!load_regis_coord_pixelvector(context,
						  fragment_to_tempstr(&item),
						  old_ul_x, old_ul_y,
						  &new_ul_x, &new_ul_y)) {
		    TRACE(("DATA_ERROR: unable to parse pixel vector in '%c' command: \"%s\"\n",
			   state->command, fragment_to_tempstr(&item)));
		    break;
		}
		TRACE(("scrolling image to updated upper-left coordinate in pixel coordinates: %d,%d\n",
		       new_ul_x, new_ul_y));

		/* FIXME: does any write mode affect revealed background? */
		if (new_ul_x > 0)
		    copy_w = context->width - new_ul_x;
		else
		    copy_w = context->width;
		if (new_ul_y > 0)
		    copy_h = context->height - new_ul_y;
		else
		    copy_h = context->height;
		/* FIXME: verify this applies to write page, not display page */
		copy_overlapping_area(context->destination_graphic,
				      new_ul_x, new_ul_y,
				      0, 0,
				      (unsigned) copy_w, (unsigned) copy_h,
				      context->background);
		context->destination_graphic->dirty = 1;
		context->force_refresh = 1;
	    }
	    break;
	case 't':
	    {
		int dx, dy;

		/* FIXME: verify this does not use user coordinates */
		if (!load_regis_pixel_pixelvector(fragment_to_tempstr(&item),
						  1, 0, 0, &dx, &dy)) {
		    TRACE(("DATA_ERROR: unable to parse pixel vector in '%c' command: \"%s\"\n",
			   state->command, fragment_to_tempstr(&item)));
		    break;
		}

		dx *= (int) (context->current_text_controls->character_display_w
			     >> 1U);
		dy *= (int) (context->current_text_controls->character_display_h
			     >> 1U);
		TRACE(("adding character offset %d,%d\n (ds=%dx%d)", dx, dy,
		       context->current_text_controls->character_display_w,
		       context->current_text_controls->character_display_h));
		move_text(context, dx, dy);
	    }
	    break;
	case 'v':
	    {
		char const *const pixelvector = fragment_to_tempstr(&item);
		unsigned offset;

		for (offset = 0U; pixelvector[offset] != '\0';) {
		    int orig_x = context->graphics_output_cursor_x;
		    int orig_y = context->graphics_output_cursor_y;
		    if (!load_regis_coord_pixelvector_step(context, pixelvector,
							   &offset,
							   orig_x, orig_y,
							   &context->graphics_output_cursor_x,
							   &context->graphics_output_cursor_y)) {
			TRACE(("DATA_ERROR: unable to parse pixel vector in '%c' command: \"%s\"\n",
			       state->command, fragment_to_tempstr(&item)));
			break;
		    }
		    TRACE(("drawing line to location %d,%d\n",
			   context->graphics_output_cursor_x,
			   context->graphics_output_cursor_y));
		    draw_patterned_line(context, orig_x, orig_y,
					context->graphics_output_cursor_x,
					context->graphics_output_cursor_y);
		}
	    }
	    break;
	default:
	    TRACE(("DATA_ERROR: unexpected pixel vector in \"%c\" command: \"%s\"\n",
		   state->command, fragment_to_tempstr(&item)));
	    break;
	}
	return 1;
    }

    if (extract_regis_string(input, state->temp, state->templen)) {
	switch (state->command) {
	case 'l':
	    /* FIXME: confirm that extra characters are ignored */
	    TRACE(("found character to load: \"%s\"\n", state->temp));
	    state->load_glyph = (unsigned) (unsigned char) state->temp[0];
	    state->load_row = 0U;
	    break;
	case 't':
	    TRACE(("found string to draw: \"%s\"\n", state->temp));
	    draw_text(context, state->temp);
	    break;
	case '_':
	    TRACE(("found comment: \"%s\"\n", state->temp));
	    break;
	default:
	    TRACE(("DATA_ERROR: unexpected string argument to \"%c\" command: \"%s\"\n",
		   state->command, state->temp));
	    break;
	}
	return 1;
    }

    /* hex values */
    if (state->command == 'l') {
	unsigned digit;

	for (digit = 0U; digit < (state->load_w + 3U) >> 2U; digit++) {
	    char ch = peek_fragment(input);

	    if (!IS_HEX_DIGIT(ch)) {
		if (ch != ',' && ch != ';' &&
		    ch != ' ' && ch != '\r' &&
		    ch != '\n') {
		    TRACE(("found end of hexadecimal string witch '%c' on digit %u\n",
			   ch, digit));
		    /* FIXME: need to unput the digits up to this point */
		    /*
		     * Report success since we ate some characters,
		     * and the new char needs to be compared with commands
		     * and other top-level things.
		     */
		    if (digit != 0U)
			return 1;
		    return 0;
		}
		pop_fragment(input);
		break;
	    }

	    state->temp[digit] = ch;
	    pop_fragment(input);
	}
	state->temp[digit] = '\0';

	if (strlen(state->temp) > 0) {
	    unsigned long val;
	    unsigned glyph_size;

	    val = strtoul(state->temp, NULL, 16);
	    TRACE(("found row %u for glyph %u: \"%s\" value %02lx (%lu)\n",
		   state->load_row, state->load_glyph, state->temp, val, val));

	    if (state->load_row >= state->load_h) {
		TRACE(("DATA_ERROR: ignoring extra row for glyph %u\n",
		       state->load_glyph));
		return 0;
	    }

	    if (state->load_index == MAX_REGIS_ALPHABETS) {
		state->load_index = find_free_alphabet_index(context,
							     state->load_alphabet,
							     state->load_w,
							     state->load_h);
		TRACE(("current alphabet is %u and size is %ux%u; assigning alphabet index %u\n",
		       state->load_alphabet, state->load_w,
		       state->load_h, state->load_index));
	    }

	    glyph_size =
		GLYPH_WIDTH_BYTES(context->alphabets[state->load_index].pixw)
		* context->alphabets[state->load_index].pixh;
	    if (context->alphabets[state->load_index].bytes == NULL) {
		if (!(context->alphabets[state->load_index].bytes =
		      calloc(MAX_GLYPHS * glyph_size, sizeof(unsigned char)))) {
		    TRACE(("ERROR: unable to allocate %u bytes for glyph storage\n",
			   MAX_GLYPHS * glyph_size));
		    return 0;
		}
	    } {
		unsigned char *glyph;
		unsigned bytew;
		unsigned byte;
		unsigned unused_bits;

		glyph = &context->alphabets[state->load_index]
		    .bytes[state->load_glyph * glyph_size];
		bytew = GLYPH_WIDTH_BYTES(context->alphabets[state->load_index]
					  .pixw);
		unused_bits = 8U - (context->alphabets[state->load_index].pixw
				    & 3U);
		if (unused_bits == 8U) {
		    unused_bits = 0U;
		}
		for (byte = 0U; byte < bytew; byte++) {
		    glyph[state->load_row * bytew + byte] =
			(unsigned char) (((val << unused_bits) >>
					  ((bytew - (byte + 1U)) << 3U))
					 & 255U);
#ifdef DEBUG_LOAD
		    TRACE(("bytew=%u val=%lx byte=%u output=%x\n",
			   bytew, val,
			   byte,
			   (unsigned) glyph[state->load_row * bytew + byte]));
#endif
		}

		state->load_row++;
		context->alphabets[state->load_index]
		    .loaded[state->load_glyph] = 1;
#ifdef DEBUG_LOAD
		TRACE(("marking alphabet %u at index %u glyph %u as loaded\n",
		       state->load_alphabet, state->load_index,
		       state->load_glyph));
#endif
		return 1;
	    }
	}
    }

    /* special symbols */
    if (state->command == '@') {
	char ch = peek_fragment(input);
	TRACE(("inspecting macrograph character \"%c\"\n", ch));
	switch (ch) {
	case '.':
	    pop_fragment(input);
	    TRACE(("clearing all macrographs\n"));
	    /* FIXME: implement when macrographs are supported. */
	    return 1;
	case ':':
	    pop_fragment(input);
	    TRACE(("defining macrograph\n"));
	    /* FIXME: what about whitespace before the name? */
	    if (fragment_consumed(input)) {
		TRACE(("DATA_ERROR: macrograph definition without name, ignoring\n"));
		return 1;
	    } {
#define MAX_MACROGRAPH_LEN 1024
		char temp[MAX_MACROGRAPH_LEN];
		char name;
		char prev = '\0';
		int len = 0;

		name = pop_fragment(input);
		if (islower(name))
		    name = (char) toupper(name);
		TRACE(("defining macrograph for \"%c\"\n", name));
		if (name < 'A' || name > 'Z') {
		    TRACE(("DATA_ERROR: invalid macrograph name\n"));
		    /* FIXME: what should happen? */
		    return 1;
		}
		for (;;) {
		    char next = peek_fragment(input);
		    if (prev == '@' && next == ';') {
			/* FIXME: parse, handle  :<name><definition>; */
			pop_fragment(input);
			len--;
			break;
		    } else if (next == '\0') {
			TRACE(("DATA_ERROR: macrograph definition ends before semicolon\n"));
			/* FIXME: what should happen? */
			return 1;
		    }
		    pop_fragment(input);
		    if (len < MAX_MACROGRAPH_LEN) {
			temp[len++] = next;
		    }
		    prev = next;
		}
		if (len == MAX_MACROGRAPH_LEN) {
		    TRACE(("DATA_ERROR: macrograph definition too long\n"));
		    /* FIXME: what should happen? */
		    return 1;
		}
		temp[len] = '\0';

		(void) temp;	/* variable needed only if tracing */
		/* FIXME: implement when macrographs are supported. */
		TRACE(("ERROR: would have saved macrograph: \"%s\"\n", temp));
	    }
	    return 1;
	case ';':
	    pop_fragment(input);
	    TRACE(("DATA_ERROR: found extraneous terminator for macrograph definition\n"));
	    return 1;
	default:
	    pop_fragment(input);
	    TRACE(("DATA_ERROR: unknown macrograph command '%c'\n", ch));
	    return 1;
	}
    }

    return 0;
}

static int
parse_regis_toplevel(RegisParseState *state, RegisGraphicsContext *context)
{
    RegisDataFragment parenthesized;
    char ch;
#if 0
    TRACE(("reference line: shading=%d ref=%u loc=%d\n",
	   context->temporary_write_controls.shading_enabled,
	   context->temporary_write_controls.shading_reference_dim,
	   context->temporary_write_controls.shading_reference));
#endif

#ifdef DEBUG_PARSING
    TRACE(("parsing top level: char %u of %u (next char '%c')\n",
	   state->input.pos,
	   fragment_length(&state->input),
	   peek_fragment(&state->input)));
#endif
    if (skip_regis_whitespace(&state->input))
	return 0;
    if (expand_macrographs(&state->input, context))
	return 0;

    /* FIXME: the semicolon terminates the current command even if inside of an optionset or extent */
    if (peek_fragment(&state->input) == ';') {
	pop_fragment(&state->input);
	TRACE(("ending '%c' command\n", state->command));
	state->command = '_';
	state->option = '_';
	return 1;
    }
    /* Load statements contain hex values which may look like commands. */
    ch = peek_fragment(&state->input);
    if (state->command != 'l' || !IS_HEX_DIGIT(ch)) {
#ifdef DEBUG_PARSING
	TRACE(("checking for top level command...\n"));
#endif
	if (parse_regis_command(state)) {
	    /* FIXME: verify that these are the things reset on a new command */
	    TRACE(("resetting temporary write controls and pattern state before handling command\n"));
	    copy_regis_write_controls(&context->persistent_write_controls,
				      &context->temporary_write_controls);
	    context->pattern_count = 0U;
	    context->pattern_bit = 1U;

	    /* FIXME: what happens if temporary text controls aren't closed? */
	    /* FIXME: what if temporary text controls are nested? */
	    context->current_text_controls = &context->persistent_text_controls;
	    return 1;
	}
    }
#ifdef DEBUG_PARSING
    TRACE(("checking for top level parentheses...\n"));
#endif
    if (extract_regis_parenthesized_data(&state->input, &parenthesized)) {
	RegisDataFragment keep;

	if (state->command == 'f') {	/* Fill */
	    TRACE(("found commands in fill mode \"%s\"\n",
		   fragment_to_tempstr(&parenthesized)));
	    copy_fragment(&keep, &state->input);
	    copy_fragment(&state->input, &parenthesized);
	    state->command = '_';
	    state->option = '_';
	    context->fill_mode = 1;
	    context->fill_point_count = 0U;
	    while (!fragment_consumed(&state->input))
		parse_regis_toplevel(state, context);
	    draw_filled_polygon(context);
	    context->fill_point_count = 0U;
	    context->fill_mode = 0;
	    state->command = 'f';
	    copy_fragment(&state->input, &keep);
	    return 1;
	} else {
	    TRACE(("parsing optionset \"%s\"\n",
		   fragment_to_tempstr(&parenthesized)));
	    copy_fragment(&keep, &state->input);
	    copy_fragment(&state->input, &parenthesized);
	    state->option = '_';
	    TRACE(("parsing at optionset level (char %u of %u)\n",
		   state->input.pos,
		   fragment_length(&state->input)));
	    for (;;) {
		if (fragment_consumed(&state->input))
		    break;
		TRACE(("looking at optionset character %u: \"%c\"\n",
		       state->input.pos, peek_fragment(&state->input)));
		if (skip_regis_whitespace(&state->input))
		    continue;
		if (parse_regis_option(state, context))
		    continue;
		if (parse_regis_items(state, context))
		    continue;
		if (fragment_consumed(&state->input))
		    break;
		{
		    char skip;

		    skip = pop_fragment(&state->input);
		    (void) skip;	/* variable needed only if tracing */
		    TRACE(("DATA_ERROR: skipping unexpected character in optionset: \"%c\"\n",
			   skip));
		}
		/* FIXME: suboptions */
	    }
	    state->option = '_';
	    copy_fragment(&state->input, &keep);
	    return 1;
	}
    }
    if (state->command == 'f') {	/* Fill */
	RegisDataFragment optionarg;
	if (extract_regis_option(&state->input, &state->option, &optionarg)) {
	    skip_regis_whitespace(&optionarg);

	    TRACE(("found temporary write options \"%s\"\n",
		   fragment_to_tempstr(&optionarg)));
	    if (!load_regis_write_control_set(state, context,
					      context->graphics_output_cursor_x,
					      context->graphics_output_cursor_y,
					      &optionarg,
					      &context->temporary_write_controls)) {
		TRACE(("DATA_ERROR: invalid temporary write options \"%s\"\n",
		       fragment_to_tempstr(&optionarg)));
	    }
	    return 1;
	}
	TRACE(("checking for top level items (though none should be present)...\n"));
	if (parse_regis_items(state, context))
	    return 1;
    } else {
#ifdef DEBUG_PARSING
	TRACE(("checking for top level items...\n"));
#endif
	if (parse_regis_items(state, context))
	    return 1;
    }

    if (!fragment_consumed(&state->input)) {
	char skip;

	skip = pop_fragment(&state->input);
	(void) skip;		/* variable needed only if tracing */
	TRACE(("DATA_ERROR: skipping unexpected character at top level: \"%c\"\n", ch));
    }
    return 0;
}

void
parse_regis(XtermWidget xw, ANSI *params, char const *string)
{
    TScreen *screen = TScreenOf(xw);
    RegisGraphicsContext *const context = &persistent_context;
    RegisParseState *const state = &persistent_state;
    struct timeval prev_tv;
    struct timeval curr_tv;
    unsigned iterations;
    int Pmode;

    assert(params);
    assert(string);

    if (params->a_nparam > 0)
	Pmode = params->a_param[0];
    else
	Pmode = 0;

    TRACE(("ReGIS vector graphics mode, param_count=%d mode=%d\n",
	   params->a_nparam, Pmode));

    init_fragment(&state->input, string);
    state->templen = (unsigned) strlen(string) + 1U;
    if (!(state->temp = malloc((size_t) state->templen))) {
	TRACE(("Unable to allocate temporary buffer of size %u\n",
	       state->templen));
	return;
    }

    context->current_widget = xw;

    /* Update the screen scrolling and do a refresh.
     * The refresh may not cover the whole graphic.
     */
    if (screen->scroll_amt)
	FlushScroll(xw);

    /* Only reset on the first ReGIS image unless it is being requested. */
    if (context->width == 0 || context->height == 0 ||
	Pmode == 1 || Pmode == 3) {
	init_regis_parse_state(state);
	init_regis_graphics_context(screen->terminal_id,
				    screen->graphics_regis_def_wide,
				    screen->graphics_regis_def_high,
				    get_color_register_count(screen),
				    screen->graphics_regis_default_font,
				    context);
    }

    map_regis_graphics_pages(xw, context);

    X_GETTIMEOFDAY(&prev_tv);
    iterations = 0U;
    refresh_modified_displayed_graphics(xw);

    for (;;) {
	if (skip_regis_whitespace(&state->input))
	    continue;
	if (parse_regis_toplevel(state, context)) {
	    int need_refresh = 0;

	    /* FIXME: Move refresh logic out of the top level so that long
	     * sequences of filled drawing commands can be refreshed before the
	     * end of the fill command.
	     */
	    iterations++;
	    if (context->force_refresh) {
		X_GETTIMEOFDAY(&curr_tv);
		need_refresh = 1;
	    } else if (iterations > MIN_ITERATIONS_BEFORE_REFRESH) {
		X_GETTIMEOFDAY(&curr_tv);
		if ((Time) curr_tv.tv_sec > (Time) prev_tv.tv_sec + 1UL) {
		    need_refresh = 1;
		} else {
#define DiffTime(TV) (TV.tv_sec * 1000L + TV.tv_usec / 1000L)
		    long diff = (long) (DiffTime(curr_tv) - DiffTime(prev_tv));
		    if (diff > MIN_MS_BEFORE_REFRESH)
			need_refresh = 1;
		}
	    }

	    if (need_refresh) {
		TRACE(("refreshing after %u iterations and %ldms\n",
		       iterations,
		       DiffTime(curr_tv) - DiffTime(prev_tv)));
		context->force_refresh = 0;
		/* FIXME: pre-ANSI compilers need memcpy() */
		prev_tv = curr_tv;
		iterations = 0U;
		refresh_modified_displayed_graphics(xw);
#if OPT_DOUBLE_BUFFER
		{
		    XdbeSwapInfo swap;

		    swap.swap_window = VWindow(screen);
		    swap.swap_action = XdbeCopied;
		    XdbeSwapBuffers(XtDisplay(term), &swap, 1);
		    XFlush(XtDisplay(xw));
		}
#endif
	    }

	    continue;
	}

	if (fragment_consumed(&state->input))
	    break;
    }

    free(state->temp);

    refresh_modified_displayed_graphics(xw);
    TRACE(("DONE! Successfully parsed ReGIS data.\n"));
}
