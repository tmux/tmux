/* $Id: screen.c,v 1.3 2007-07-25 23:13:18 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <string.h>

#include "tmux.h"

/*
 * Virtual screen and basic ANSI terminal emulator.
 */

size_t	 screen_store_attributes(struct buffer *, u_char);
size_t	 screen_store_colours(struct buffer *, u_char);
void	 screen_free_lines(struct screen *, u_int, u_int);
void	 screen_make_lines(struct screen *, u_int, u_int);
void	 screen_move_lines(struct screen *, u_int, u_int, u_int);
void	 screen_fill_lines(
	     struct screen *, u_int, u_int, u_char, u_char, u_char);
uint16_t screen_extract(u_char *);
void	 screen_write_character(struct screen *, u_char);
void	 screen_cursor_up_scroll(struct screen *, u_int);
void	 screen_cursor_down_scroll(struct screen *, u_int);
void	 screen_scroll_up(struct screen *, u_int);
void	 screen_scroll_down(struct screen *, u_int);
void	 screen_fill_screen(struct screen *, u_char, u_char, u_char);
void	 screen_fill_line(struct screen *, u_int, u_char, u_char, u_char);
void	 screen_fill_end_of_screen(
    	     struct screen *, u_int, u_int, u_char, u_char, u_char);
void	 screen_fill_end_of_line(
    	     struct screen *, u_int, u_int, u_char, u_char, u_char);
void	 screen_fill_start_of_line(
    	     struct screen *, u_int, u_int, u_char, u_char, u_char);
void	 screen_insert_lines(struct screen *, u_int, u_int);
void	 screen_delete_lines(struct screen *, u_int, u_int);
void	 screen_insert_characters(struct screen *, u_int, u_int, u_int);
void	 screen_delete_characters(struct screen *, u_int, u_int, u_int);

#define SCREEN_DEFDATA ' '
#define SCREEN_DEFATTR 0
#define SCREEN_DEFCOLR 0x88

#define screen_last_y(s) ((s)->sy - 1)
#define screen_last_x(s) ((s)->sx - 1)

#define screen_range_y(lx, rx) (((rx) - (lx)) + 1)
#define screen_range_x(ux, lx) (((lx) - (ux)) + 1)

#define screen_offset_y(py, ny) ((py) + (ny) - 1)
#define screen_offset_x(px, nx) ((px) + (nx) - 1)

/* Create a new screen. */
void
screen_create(struct screen *s, u_int sx, u_int sy)
{
	s->sx = sx;
	s->sy = sy;
	s->cx = 0;
	s->cy = 0;

	s->ry_upper = 0;
	s->ry_lower = screen_last_y(s);

	s->attr = SCREEN_DEFATTR;
	s->colr = SCREEN_DEFCOLR;

	s->mode = MODE_CURSOR;
	*s->title = '\0';

	s->grid_data = xmalloc(sy * (sizeof *s->grid_data));
	s->grid_attr = xmalloc(sy * (sizeof *s->grid_attr));
	s->grid_colr = xmalloc(sy * (sizeof *s->grid_colr));
	screen_make_lines(s, 0, screen_last_y(s));
	screen_fill_screen(s, SCREEN_DEFDATA, 0, SCREEN_DEFCOLR);
}

/* Resize screen. */
void
screen_resize(struct screen *s, u_int sx, u_int sy)
{
	u_int	i, ox, oy, ny;

	if (sx < 1)
		sx = 1;
	if (sy < 1)
		sy = 1;

	ox = s->sx;
	oy = s->sy;
	s->sx = sx;
	s->sy = sy;

	log_debug("resizing screen (%u, %u) -> (%u, %u)", ox, oy, sx, sy);

	if (sy < oy) {
		ny = oy - sy;
		if (ny > s->cy)
			ny = s->cy;

		if (ny != 0) {
			log_debug("removing %u lines from top", ny);
			for (i = 0; i < ny; i++) {
				log_debug("freeing line %u", i);
				xfree(s->grid_data[i]);
				xfree(s->grid_attr[i]);
				xfree(s->grid_colr[i]);
			}
			memmove(s->grid_data, s->grid_data + ny,
			    (oy - ny) * (sizeof *s->grid_data));
			memmove(s->grid_attr, s->grid_attr + ny,
			    (oy - ny) * (sizeof *s->grid_attr));
			memmove(s->grid_colr, s->grid_colr + ny,
			    (oy - ny) * (sizeof *s->grid_colr));
			s->cy -= ny;
		}
		if (ny < oy - sy) {
			log_debug(
			    "removing %u lines from bottom", oy - sy - ny);
			for (i = sy; i < oy - ny; i++) {
				log_debug("freeing line %u", i);
				  xfree(s->grid_data[i]);
				  xfree(s->grid_attr[i]);
				  xfree(s->grid_colr[i]);
			}
			if (s->cy >= sy)
				s->cy = sy - 1;
		}
	}
	if (sy != oy) {
		s->grid_data = xrealloc(s->grid_data, sy, sizeof *s->grid_data);
		s->grid_attr = xrealloc(s->grid_attr, sy, sizeof *s->grid_attr);
		s->grid_colr = xrealloc(s->grid_colr, sy, sizeof *s->grid_colr);
	}
	if (sy > oy) {
		for (i = oy; i < sy; i++) {
			log_debug("allocating line %u", i);
			s->grid_data[i] = xmalloc(sx);
			s->grid_attr[i] = xmalloc(sx);
			s->grid_colr[i] = xmalloc(sx);
			screen_fill_line(s, i,
			    SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
		}
		sy = oy;
	}

	if (sx != ox) {
		for (i = 0; i < sy; i++) {
			log_debug("adjusting line %u to %u", i, sx);
			s->grid_data[i] = xrealloc(s->grid_data[i], sx, 1);
			s->grid_attr[i] = xrealloc(s->grid_attr[i], sx, 1);
			s->grid_colr[i] = xrealloc(s->grid_colr[i], sx, 1);
			if (sx > ox) {
				screen_fill_end_of_line(s, ox, i,
				    SCREEN_DEFDATA, SCREEN_DEFATTR,
				    SCREEN_DEFCOLR);
			}
		}
		if (s->cx >= sx)
			s->cx = sx - 1;
	}
}

/* Draw a set of lines on the screen. */
void
screen_draw(struct screen *s, struct buffer *b, u_int uy, u_int ly)
{
	u_char		 attr, colr;
	size_t		 size;
	u_int		 i, j;
	uint16_t	 n;

	if (uy > screen_last_y(s) || ly > screen_last_y(s) || ly < uy)
		fatalx("bad range");

	/* XXX. This is naive and rough right now. */
	attr = 0;
	colr = SCREEN_DEFCOLR;

	input_store_zero(b, CODE_CURSOROFF);

	input_store_one(b, CODE_ATTRIBUTES, 0);

	for (j = uy; j <= ly; j++) {
		input_store_two(b, CODE_CURSORMOVE, j + 1, 1);

		for (i = 0; i <= screen_last_x(s); i++) {
			
			size = BUFFER_USED(b);
			input_store_one(b, CODE_ATTRIBUTES, 0);
			
			n = 0;
			if (s->grid_attr[j][i] != attr) {
				attr = s->grid_attr[j][i];
				n += screen_store_attributes(b, attr);
				if (attr == 0)
				colr = SCREEN_DEFCOLR;
			}
			if (s->grid_colr[j][i] != colr) {
				colr = s->grid_colr[j][i];
				n += screen_store_colours(b, colr);
			}
			if (n == 0)
				buffer_reverse_add(b, 4);
			else {
				size = BUFFER_USED(b) - size;
				memcpy(BUFFER_IN(b) - size + 2, &n, 2);
			}
		
			input_store8(b, s->grid_data[j][i]);
		}
	}

	size = BUFFER_USED(b);
	input_store_one(b, CODE_ATTRIBUTES, 0);
	n = screen_store_attributes(b, s->attr);
	n += screen_store_colours(b, s->colr);
	size = BUFFER_USED(b) - size;
	memcpy(BUFFER_IN(b) - size + 2, &n, 2);

	input_store_two(b, CODE_CURSORMOVE, s->cy + 1, s->cx + 1);

	if (s->mode & MODE_CURSOR)
		input_store_zero(b, CODE_CURSORON);
}

/* Store screen atttributes in buffer. */
size_t
screen_store_attributes(struct buffer *b, u_char attr)
{
	size_t	n;

	if (attr == 0) {
		input_store16(b, 0);
		return (1);
	}

	n = 0;
	if (attr & ATTR_BRIGHT) {
		input_store16(b, 1);
		n++;
	}
	if (attr & ATTR_DIM) {
		input_store16(b, 2);
		n++;
	}
	if (attr & ATTR_ITALICS) {
		input_store16(b, 3);
		n++;
	}
	if (attr & ATTR_UNDERSCORE) {
		input_store16(b, 4);
		n++;
	}
	if (attr & ATTR_BLINK) {
		input_store16(b, 5);
		n++;
	}
	if (attr & ATTR_REVERSE) {
		input_store16(b, 7);
		n++;
	}
	if (attr & ATTR_HIDDEN) {
		input_store16(b, 8);
		n++;
	}
	return (n);
}

/* Store screen colours in buffer. */
size_t
screen_store_colours(struct buffer *b, u_char colr)
{
	uint16_t	v;

	v = colr >> 4;
	if (v == 8)
		v = 9;
	input_store16(b, 30 + v);
	v = colr & 0xf;
	if (v == 8)
		v = 9;
	input_store16(b, 40 + v);

	return (2);
}

/* Make a range of lines. */
void
screen_make_lines(struct screen *s, u_int uy, u_int ly)
{
	u_int	i;

	log_debug("making lines %u:%u", uy, ly);

	if (uy > screen_last_y(s) || ly > screen_last_y(s) || ly < uy)
		fatalx("bad range");
	
	for (i = uy; i <= ly; i++) {
		s->grid_data[i] = xmalloc(s->sx);
		s->grid_attr[i] = xmalloc(s->sx);
		s->grid_colr[i] = xmalloc(s->sx);
	}
}

/* Free a range of lines. */
void
screen_free_lines(struct screen *s, u_int uy, u_int ly)
{
	u_int	i;

	log_debug("freeing lines %u:%u", uy, ly);

	if (uy > screen_last_y(s) || ly > screen_last_y(s) || ly < uy)
		fatalx("bad range");

	for (i = uy; i <= ly; i++) {
		xfree(s->grid_data[i]);
		s->grid_data[i] = (u_char *) 0xffffffff;
		xfree(s->grid_attr[i]);
		s->grid_attr[i] = (u_char *) 0xffffffff;
		xfree(s->grid_colr[i]);
		s->grid_colr[i] = (u_char *) 0xffffffff;
	}
}

/* Move a range of lines. */
void
screen_move_lines(struct screen *s, u_int dy, u_int uy, u_int ly)
{
	u_int	ny;

	log_debug("moving lines %u:%u to %u", uy, ly, dy);

	ny = (ly - uy) + 1;

	if (uy > screen_last_y(s) || ly > screen_last_y(s) || ly < uy)
		fatalx("bad range");
	if (dy > screen_last_y(s))
		fatalx("bad destination");
	if (dy + ny - 1 > screen_last_y(s))
		fatalx("bad size");
	if (dy == uy)
		fatalx("null move");

	memmove(
	    &s->grid_data[dy], &s->grid_data[uy], ny * (sizeof *s->grid_data));
	memmove(
	    &s->grid_attr[dy], &s->grid_attr[uy], ny * (sizeof *s->grid_attr));
	memmove(
	    &s->grid_colr[dy], &s->grid_colr[uy], ny * (sizeof *s->grid_colr));
}

/* Fill a range of lines. */
void
screen_fill_lines(
    struct screen *s, u_int uy, u_int ly, u_char data, u_char attr, u_char colr)
{
	u_int	i;

	log_debug("filling lines %u:%u", uy, ly);

	if (uy > screen_last_y(s) || ly > screen_last_y(s) || ly < uy)
		fatalx("bad range");

	for (i = uy; i <= ly; i++)
		screen_fill_line(s, i, data, attr, colr);
}

/* Update screen with character. */
void
screen_character(struct screen *s, u_char ch)
{
	switch (ch) {
	case '\n':	/* LF */
		screen_cursor_down_scroll(s, 1);
		break;
	case '\r':	/* CR */
		s->cx = 0;
		break;
	case '\010':	/* BS */
		if (s->cx > 0)
			s->cx--;
		break;
	default:
		if (ch < ' ')
			fatalx("bad control");
		screen_write_character(s, ch);
		break;
	}
}

/* Extract 16-bit value from pointer. */
uint16_t
screen_extract(u_char *ptr)
{
	uint16_t	n;

	memcpy(&n, ptr, sizeof n);
	return (n);
}

/* Update screen with escape sequence. */
void
screen_sequence(struct screen *s, u_char *ptr)
{
	uint16_t	ua, ub;

	ptr++;
	log_debug("processing code %hhu", *ptr);
	switch (*ptr++) {
	case CODE_CURSORUP:
		ua = screen_extract(ptr);
		if (ua > s->cy)
			ua = s->cy;
		s->cy -= ua;
		break;
	case CODE_CURSORDOWN:
		ua = screen_extract(ptr);
		if (s->cy + ua > screen_last_y(s))
			ua = screen_last_y(s) - s->cy;
		s->cy += ua;
		break;
	case CODE_CURSORLEFT:
		ua = screen_extract(ptr);
		if (ua > s->cx)
			ua = s->cx;
		s->cx -= ua;
		break;
	case CODE_CURSORRIGHT:
		ua = screen_extract(ptr);
		if (s->cx + ua > screen_last_x(s))
			ua = screen_last_x(s) - s->cx;
		s->cx += ua;
		break;
	case CODE_CURSORMOVE:
		ua = screen_extract(ptr);
		ptr += 2;
		ub = screen_extract(ptr);
		if (ub > s->sx)
			ub = s->sx;
		s->cx = ub - 1;
		if (ua > s->sy)
			ua = s->sy;
		s->cy = ua - 1;
		break;
	case CODE_CLEARENDOFSCREEN:
		screen_fill_end_of_screen(
		    s, s->cx, s->cy, SCREEN_DEFDATA, s->attr, s->colr);
		break;
	case CODE_CLEARSCREEN:
		screen_fill_screen(s, SCREEN_DEFDATA, s->attr, s->colr);
		break;
	case CODE_CLEARENDOFLINE:
		screen_fill_end_of_line(
		    s, s->cx, s->cy, SCREEN_DEFDATA, s->attr, s->colr);
		break;
	case CODE_CLEARSTARTOFLINE:
		screen_fill_start_of_line(
		    s, s->cx, s->cy, SCREEN_DEFDATA, s->attr, s->colr);
		break;
	case CODE_CLEARLINE:
		screen_fill_line(s, s->cy, SCREEN_DEFDATA, s->attr, s->colr);
		break;
	case CODE_INSERTLINE:
		ua = screen_extract(ptr);
		screen_insert_lines(s, s->cy, ua);
		break;
	case CODE_DELETELINE:
		ua = screen_extract(ptr);
		screen_delete_lines(s, s->cy, ua);
		break;
	case CODE_INSERTCHARACTER:
		ua = screen_extract(ptr);
		screen_insert_characters(s, s->cx, s->cy, ua);
		break;
	case CODE_DELETECHARACTER:
		ua = screen_extract(ptr);
		screen_delete_characters(s, s->cx, s->cy, ua);
		break;
	case CODE_CURSORON:
		s->mode |= MODE_CURSOR;
		break;
	case CODE_CURSOROFF:
		s->mode &= ~MODE_CURSOR;
		break;
	case CODE_CURSORDOWNSCROLL:
		ua = screen_extract(ptr);
		screen_cursor_down_scroll(s, ua);
		break;
	case CODE_CURSORUPSCROLL:
		ua = screen_extract(ptr);
		screen_cursor_up_scroll(s, ua);
		break;
	case CODE_SCROLLREGION:
		ua = screen_extract(ptr);
		ptr += 2;
		ub = screen_extract(ptr);
		if (ua > s->sy)
			ua = s->sy;
		s->ry_upper = ua - 1;
		if (ub > s->sy)
			ub = s->sy;
		s->ry_lower = ub - 1;
		break;
	case CODE_INSERTOFF:
		s->mode &= ~MODE_INSERT;
		break;
	case CODE_INSERTON:
		s->mode |= MODE_INSERT;
		break;
	case CODE_KCURSOROFF:
		s->mode &= ~MODE_KCURSOR;
		break;
	case CODE_KCURSORON:
		s->mode |= MODE_KCURSOR;
		break;
	case CODE_KKEYPADOFF:
		s->mode &= ~MODE_KKEYPAD;
		break;
	case CODE_KKEYPADON:
		s->mode |= MODE_KKEYPAD;
		break;
	case CODE_TITLE:
		ua = screen_extract(ptr);
		ptr += 2;
		log_debug("new title: %u:%.*s", ua, (int) ua, ptr);
		if (ua > MAXTITLELEN - 1)
			ua = MAXTITLELEN - 1;
		memcpy(s->title, ptr, ua);
		s->title[ua] = '\0';
		break;
	case CODE_ATTRIBUTES:
		ua = screen_extract(ptr);
		if (ua == 0) {
			s->attr = 0;
			s->colr = SCREEN_DEFCOLR;
			break;
		}

		while (ua-- > 0) {
			ptr += 2;
			ub = screen_extract(ptr);
			switch (ub) {
			case 0:
			case 10:
				s->attr = 0;
				s->colr = SCREEN_DEFCOLR;
				break;
			case 1:
				s->attr |= ATTR_BRIGHT;
				break;
			case 2:
				s->attr |= ATTR_DIM;
				break;
			case 3:
				s->attr |= ATTR_ITALICS;
				break;
			case 4:
				s->attr |= ATTR_UNDERSCORE;
				break;
			case 5:
				s->attr |= ATTR_BLINK;
				break;
			case 7:
				s->attr |= ATTR_REVERSE;
				break;
			case 8:
				s->attr |= ATTR_HIDDEN;
				break;
			case 23:
				s->attr &= ~ATTR_ITALICS;
				break;
			case 24:
				s->attr &= ~ATTR_UNDERSCORE;
				break;
			case 30:
			case 31:
			case 32:
			case 33:
			case 34:
			case 35:
			case 36:
			case 37:
				s->colr &= 0x0f;
				s->colr |= (ub - 30) << 4;
				break;
			case 39:
				s->colr &= 0x0f;
				s->colr |= 0x80;
				break;
			case 40:
			case 41:
			case 42:
			case 43:
			case 44:
			case 45:
			case 46:
			case 47:
				s->colr &= 0xf0;
				s->colr |= ub - 40;
				break;
			case 49:
				s->colr &= 0xf0;
				s->colr |= 0x08;
				break;
			}
		}
	}
}

/* Write a single character to the screen at the cursor and move forward. */
void
screen_write_character(struct screen *s, u_char ch)
{
	s->grid_data[s->cy][s->cx] = ch;
	s->grid_attr[s->cy][s->cx] = s->attr;
	s->grid_colr[s->cy][s->cx] = s->colr;

	s->cx++;
	if (s->cx > screen_last_x(s)) {
		s->cx = 0;
		screen_cursor_down_scroll(s, 1);
	}
}

/* Move cursor up and scroll if necessary. */
void
screen_cursor_up_scroll(struct screen *s, u_int ny)
{
	if (s->cy < ny) {
		screen_scroll_down(s, ny - s->cy);
		s->cy = 0;
	} else
		s->cy -= ny;
}

/* Move cursor down and scroll if necessary. */
void
screen_cursor_down_scroll(struct screen *s, u_int ny)
{
	if (screen_last_y(s) - s->cy < ny) {
		screen_scroll_up(s, ny - (screen_last_y(s) - s->cy));
		s->cy = screen_last_y(s);
	} else
		s->cy += ny;
}

/* Scroll screen up. */
void
screen_scroll_up(struct screen *s, u_int ny)
{
	if (s->ry_upper == 0 && s->ry_lower == screen_last_y(s)) {
		screen_delete_lines(s, 0, ny);
		return;
	}

	fatalx("unimplemented");
}

/* Scroll screen down. */
void
screen_scroll_down(struct screen *s, u_int ny)
{
	if (s->ry_upper == 0 && s->ry_lower == screen_last_y(s)) {
		screen_insert_lines(s, 0, ny);
		return;
	}

	fatalx("unimplemented");
}

/* Fill entire screen. */
void
screen_fill_screen(struct screen *s, u_char data, u_char attr, u_char colr)
{
	screen_fill_end_of_screen(s, 0, 0, data, attr, colr);
}

/* Fill single line. */
void
screen_fill_line(
    struct screen *s, u_int py, u_char data, u_char attr, u_char colr)
{
	screen_fill_end_of_line(s, 0, py, data, attr, colr);
}

/* Fill to end of screen. */
void
screen_fill_end_of_screen(
    struct screen *s, u_int px, u_int py, u_char data, u_char attr, u_char colr)
{
	if (py > screen_last_y(s))
		return;

	if (px != 0) {
		screen_fill_end_of_line(s, px, py, data, attr, colr);
		if (py++ > screen_last_y(s))
			return;
	}

	while (py <= screen_last_y(s)) {
		screen_fill_line(s, py, data, attr, colr);
		py++;
	}
}

/* Fill to end of line. */
void
screen_fill_end_of_line(
    struct screen *s, u_int px, u_int py, u_char data, u_char attr, u_char colr)
{
	if (px > screen_last_x(s))
		return;
	if (py > screen_last_y(s))
		return;

	memset(&s->grid_data[py][px], data, s->sx - px);
	memset(&s->grid_attr[py][px], attr, s->sx - px);
	memset(&s->grid_colr[py][px], colr, s->sx - px);
}

/* Fill to start of line. */
void
screen_fill_start_of_line(
    struct screen *s, u_int px, u_int py, u_char data, u_char attr, u_char colr)
{
	if (px > screen_last_x(s))
		return;
	if (py > screen_last_y(s))
		return;

	memset(s->grid_data[py], data, px);
	memset(s->grid_attr[py], attr, px);
	memset(s->grid_colr[py], colr, px);
}

/* Insert lines. */
void
screen_insert_lines(struct screen *s, u_int py, u_int ny)
{
	if (py > screen_last_y(s))
		return;

	if (py + ny > screen_last_y(s))
		ny = screen_last_y(s) - py;
	log_debug("inserting lines: %u,%u", py, ny);

	screen_free_lines(s, (screen_last_y(s) - ny) + 1, screen_last_y(s));

	if (py != screen_last_y(s))
		screen_move_lines(s, py + ny, py, screen_last_y(s) - ny);

	screen_fill_lines(
	    s, py, py + ny - 1, SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}

/* Delete lines. */
void
screen_delete_lines(struct screen *s, u_int py, u_int ny)
{
	if (py > screen_last_y(s))
		return;

	if (py + ny > screen_last_y(s))
		ny = screen_last_y(s) - py;
	log_debug("deleting lines: %u,%u", py, ny);

	screen_free_lines(s, py, py + ny - 1);

	if (py != screen_last_y(s))
		screen_move_lines(s, py, py + ny, screen_last_y(s));

	screen_make_lines(s, (screen_last_y(s) - ny) + 1, screen_last_y(s));
	screen_fill_lines(s, (screen_last_y(s) - ny) + 1,
	    screen_last_y(s), SCREEN_DEFDATA, SCREEN_DEFATTR, SCREEN_DEFCOLR);
}

/* Insert characters. */
void
screen_insert_characters(struct screen *s, u_int px, u_int py, u_int nx)
{
	u_int	lx, rx;

	if (px > screen_last_x(s) || py > screen_last_y(s))
		return;

	lx = px;
	rx = screen_offset_x(px, nx);
	if (rx > screen_last_x(s))
		rx = screen_last_x(s);

	/*
	 * Inserting a range from lx to rx, inclusive.
	 *
	 * - If rx is not the last x, move from lx to rx + 1.
	 * - Clear the range from lx to rx.
	 */
	if (rx != screen_last_x(s)) {
		nx = screen_range_x(rx + 1, screen_last_x(s));
		memmove(&s->grid_data[py][rx + 1], &s->grid_data[py][lx], nx);
		memmove(&s->grid_attr[py][rx + 1], &s->grid_attr[py][lx], nx);
		memmove(&s->grid_colr[py][rx + 1], &s->grid_colr[py][lx], nx);
	}
	memset(&s->grid_data[py][lx], SCREEN_DEFDATA, screen_range_x(lx, rx));
	memset(&s->grid_attr[py][lx], SCREEN_DEFATTR, screen_range_x(lx, rx));
	memset(&s->grid_colr[py][lx], SCREEN_DEFCOLR, screen_range_x(lx, rx));
}

/* Delete characters. */
void
screen_delete_characters(struct screen *s, u_int px, u_int py, u_int nx)
{
	u_int	lx, rx;

	if (px > screen_last_x(s) || py > screen_last_y(s))
		return;

	lx = px;
	rx = screen_offset_x(px, nx);
	if (rx > screen_last_x(s))
		rx = screen_last_x(s);

	/*
	 * Deleting the range from lx to rx, inclusive.
	 *
	 * - If rx is not the last x, move the range from rx + 1 to lx.
	 * - Clear the range from the last x - (rx - lx)  to the last x.
	 */

	if (rx != screen_last_x(s)) {
		nx = screen_range_x(rx + 1, screen_last_x(s));
		memmove(&s->grid_data[py][lx], &s->grid_data[py][rx + 1], nx);
		memmove(&s->grid_attr[py][lx], &s->grid_attr[py][rx + 1], nx);
		memmove(&s->grid_colr[py][lx], &s->grid_colr[py][rx + 1], nx);
	}

	/* If lx == rx, then nx = 1. */ 
	nx = screen_range_x(lx, rx);
	memset(&s->grid_data[py][s->sx - nx], SCREEN_DEFDATA, nx);
	memset(&s->grid_attr[py][s->sx - nx], SCREEN_DEFATTR, nx);
	memset(&s->grid_colr[py][s->sx - nx], SCREEN_DEFCOLR, nx);
}
