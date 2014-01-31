/* $Id$ */

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

int	screen_redraw_cell_border1(struct window_pane *, u_int, u_int);
int	screen_redraw_cell_border(struct client *, u_int, u_int);
int	screen_redraw_check_cell(struct client *, u_int, u_int,
	    struct window_pane **);
int	screen_redraw_check_active(u_int, u_int, int, struct window *,
	    struct window_pane *);

void	screen_redraw_draw_borders(struct client *, int, u_int);
void	screen_redraw_draw_panes(struct client *, u_int);
void	screen_redraw_draw_status(struct client *, u_int);
void	screen_redraw_draw_number(struct client *, struct window_pane *);

#define CELL_INSIDE 0
#define CELL_LEFTRIGHT 1
#define CELL_TOPBOTTOM 2
#define CELL_TOPLEFT 3
#define CELL_TOPRIGHT 4
#define CELL_BOTTOMLEFT 5
#define CELL_BOTTOMRIGHT 6
#define CELL_TOPJOIN 7
#define CELL_BOTTOMJOIN 8
#define CELL_LEFTJOIN 9
#define CELL_RIGHTJOIN 10
#define CELL_JOIN 11
#define CELL_OUTSIDE 12

#define CELL_BORDERS " xqlkmjwvtun~"

/* Check if cell is on the border of a particular pane. */
int
screen_redraw_cell_border1(struct window_pane *wp, u_int px, u_int py)
{
	/* Inside pane. */
	if (px >= wp->xoff && px < wp->xoff + wp->sx &&
	    py >= wp->yoff && py < wp->yoff + wp->sy)
		return (0);

	/* Left/right borders. */
	if ((wp->yoff == 0 || py >= wp->yoff - 1) && py <= wp->yoff + wp->sy) {
		if (wp->xoff != 0 && px == wp->xoff - 1)
			return (1);
		if (px == wp->xoff + wp->sx)
			return (1);
	}

	/* Top/bottom borders. */
	if ((wp->xoff == 0 || px >= wp->xoff - 1) && px <= wp->xoff + wp->sx) {
		if (wp->yoff != 0 && py == wp->yoff - 1)
			return (1);
		if (py == wp->yoff + wp->sy)
			return (1);
	}

	/* Outside pane. */
	return (-1);
}

/* Check if a cell is on the pane border. */
int
screen_redraw_cell_border(struct client *c, u_int px, u_int py)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	int			 retval;

	/* Check all the panes. */
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		if ((retval = screen_redraw_cell_border1(wp, px, py)) != -1)
			return (retval);
	}

	return (0);
}

/* Check if cell inside a pane. */
int
screen_redraw_check_cell(struct client *c, u_int px, u_int py,
    struct window_pane **wpp)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	int			 borders;

	if (px > w->sx || py > w->sy)
		return (CELL_OUTSIDE);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		*wpp = wp;

		/* If outside the pane and its border, skip it. */
		if ((wp->xoff != 0 && px < wp->xoff - 1) ||
		    px > wp->xoff + wp->sx ||
		    (wp->yoff != 0 && py < wp->yoff - 1) ||
		    py > wp->yoff + wp->sy)
			continue;

		/* If definitely inside, return so. */
		if (!screen_redraw_cell_border(c, px, py))
			return (CELL_INSIDE);

		/*
		 * Construct a bitmask of whether the cells to the left (bit
		 * 4), right, top, and bottom (bit 1) of this cell are borders.
		 */
		borders = 0;
		if (px == 0 || screen_redraw_cell_border(c, px - 1, py))
			borders |= 8;
		if (px <= w->sx && screen_redraw_cell_border(c, px + 1, py))
			borders |= 4;
		if (py == 0 || screen_redraw_cell_border(c, px, py - 1))
			borders |= 2;
		if (py <= w->sy && screen_redraw_cell_border(c, px, py + 1))
			borders |= 1;

		/*
		 * Figure out what kind of border this cell is. Only one bit
		 * set doesn't make sense (can't have a border cell with no
		 * others connected).
		 */
		switch (borders) {
		case 15:	/* 1111, left right top bottom */
			return (CELL_JOIN);
		case 14:	/* 1110, left right top */
			return (CELL_BOTTOMJOIN);
		case 13:	/* 1101, left right bottom */
			return (CELL_TOPJOIN);
		case 12:	/* 1100, left right */
			return (CELL_TOPBOTTOM);
		case 11:	/* 1011, left top bottom */
			return (CELL_RIGHTJOIN);
		case 10:	/* 1010, left top */
			return (CELL_BOTTOMRIGHT);
		case 9:		/* 1001, left bottom */
			return (CELL_TOPRIGHT);
		case 7:		/* 0111, right top bottom */
			return (CELL_LEFTJOIN);
		case 6:		/* 0110, right top */
			return (CELL_BOTTOMLEFT);
		case 5:		/* 0101, right bottom */
			return (CELL_TOPLEFT);
		case 3:		/* 0011, top bottom */
			return (CELL_LEFTRIGHT);
		}
	}

	*wpp = NULL;
	return (CELL_OUTSIDE);
}

/* Check active pane indicator. */
int
screen_redraw_check_active(u_int px, u_int py, int type, struct window *w,
    struct window_pane *wp)
{
	/* Is this off the active pane border? */
	if (screen_redraw_cell_border1(w->active, px, py) != 1)
		return (0);

	/* If there are more than two panes, that's enough. */
	if (window_count_panes(w) != 2)
		return (1);

	/* Else if the cell is not a border cell, forget it. */
	if (wp == NULL || (type == CELL_OUTSIDE || type == CELL_INSIDE))
		return (1);

	/* Check if the pane covers the whole width. */
	if (wp->xoff == 0 && wp->sx == w->sx) {
		/* This can either be the top pane or the bottom pane. */
		if (wp->yoff == 0) { /* top pane */
			if (wp == w->active)
				return (px <= wp->sx / 2);
			return (px > wp->sx / 2);
		}
		return (0);
	}

	/* Check if the pane covers the whole height. */
	if (wp->yoff == 0 && wp->sy == w->sy) {
		/* This can either be the left pane or the right pane. */
		if (wp->xoff == 0) { /* left pane */
			if (wp == w->active)
				return (py <= wp->sy / 2);
			return (py > wp->sy / 2);
		}
		return (0);
	}

	return (type);
}

/* Redraw entire screen. */
void
screen_redraw_screen(struct client *c, int draw_panes, int draw_status,
    int draw_borders)
{
	struct options	*oo = &c->session->options;
	struct tty	*tty = &c->tty;
	u_int		 top;
	int	 	 status, spos;

	/* Suspended clients should not be updated. */
	if (c->flags & CLIENT_SUSPENDED)
		return;

	/* Get status line, er, status. */
	spos = options_get_number(oo, "status-position");
	if (c->message_string != NULL || c->prompt_string != NULL)
		status = 1;
	else
		status = options_get_number(oo, "status");
	top = 0;
	if (status && spos == 0)
		top = 1;
	if (!status)
		draw_status = 0;

	if (draw_borders)
		screen_redraw_draw_borders(c, status, top);
	if (draw_panes)
		screen_redraw_draw_panes(c, top);
	if (draw_status)
		screen_redraw_draw_status(c, top);
	tty_reset(tty);
}

/* Draw a single pane. */
void
screen_redraw_pane(struct client *c, struct window_pane *wp)
{
	u_int	i, yoff;

	if (!window_pane_visible(wp))
		return;

	yoff = wp->yoff;
	if (status_at_line(c) == 0)
		yoff++;

	for (i = 0; i < wp->sy; i++)
		tty_draw_line(&c->tty, wp->screen, i, wp->xoff, yoff);
	tty_reset(&c->tty);
}

/* Draw the borders. */
void
screen_redraw_draw_borders(struct client *c, int status, u_int top)
{
	struct window		*w = c->session->curw->window;
	struct options		*oo = &c->session->options;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	struct grid_cell	 active_gc, other_gc;
	u_int		 	 i, j, type;

	style_apply(&other_gc, oo, "pane-border-style");
	style_apply(&active_gc, oo, "pane-active-border-style");
	active_gc.attr = other_gc.attr = GRID_ATTR_CHARSET;

	for (j = 0; j < tty->sy - status; j++) {
		for (i = 0; i < tty->sx; i++) {
			type = screen_redraw_check_cell(c, i, j, &wp);
			if (type == CELL_INSIDE)
				continue;
			if (screen_redraw_check_active(i, j, type, w, wp))
				tty_attributes(tty, &active_gc);
			else
				tty_attributes(tty, &other_gc);
			tty_cursor(tty, i, top + j);
			tty_putc(tty, CELL_BORDERS[type]);
		}
	}
}

/* Draw the panes. */
void
screen_redraw_draw_panes(struct client *c, u_int top)
{
	struct window		*w = c->session->curw->window;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	struct screen		*s;
	u_int		 	 i;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		s = wp->screen;
		for (i = 0; i < wp->sy; i++)
			tty_draw_line(tty, s, i, wp->xoff, top + wp->yoff);
		if (c->flags & CLIENT_IDENTIFY)
			screen_redraw_draw_number(c, wp);
	}
}

/* Draw the status line. */
void
screen_redraw_draw_status(struct client *c, u_int top)
{
	struct tty	*tty = &c->tty;

	if (top)
		tty_draw_line(tty, &c->status, 0, 0, 0);
	else
		tty_draw_line(tty, &c->status, 0, 0, tty->sy - 1);
}

/* Draw number on a pane. */
void
screen_redraw_draw_number(struct client *c, struct window_pane *wp)
{
	struct tty		*tty = &c->tty;
	struct session		*s = c->session;
	struct options		*oo = &s->options;
	struct window		*w = wp->window;
	struct grid_cell	 gc;
	u_int			 idx, px, py, i, j, xoff, yoff;
	int			 colour, active_colour;
	char			 buf[16], *ptr;
	size_t			 len;

	if (window_pane_index(wp, &idx) != 0)
		fatalx("index not found");
	len = xsnprintf(buf, sizeof buf, "%u", idx);

	if (wp->sx < len)
		return;
	colour = options_get_number(oo, "display-panes-colour");
	active_colour = options_get_number(oo, "display-panes-active-colour");

	px = wp->sx / 2; py = wp->sy / 2;
	xoff = wp->xoff; yoff = wp->yoff;

	if (wp->sx < len * 6 || wp->sy < 5) {
		tty_cursor(tty, xoff + px - len / 2, yoff + py);
		goto draw_text;
	}

	px -= len * 3;
	py -= 2;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (w->active == wp)
		colour_set_bg(&gc, active_colour);
	else
		colour_set_bg(&gc, colour);
	tty_attributes(tty, &gc);
	for (ptr = buf; *ptr != '\0'; ptr++) {
		if (*ptr < '0' || *ptr > '9')
			continue;
		idx = *ptr - '0';

		for (j = 0; j < 5; j++) {
			for (i = px; i < px + 5; i++) {
				tty_cursor(tty, xoff + i, yoff + py + j);
				if (clock_table[idx][j][i - px])
					tty_putc(tty, ' ');
			}
		}
		px += 6;
	}

	len = xsnprintf(buf, sizeof buf, "%ux%u", wp->sx, wp->sy);
	if (wp->sx < len || wp->sy < 6)
		return;
	tty_cursor(tty, xoff + wp->sx - len, yoff);

draw_text:
	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (w->active == wp)
		colour_set_fg(&gc, active_colour);
	else
		colour_set_fg(&gc, colour);
	tty_attributes(tty, &gc);
	tty_puts(tty, buf);

	tty_cursor(tty, 0, 0);
}
