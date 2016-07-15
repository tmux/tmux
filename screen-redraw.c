/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
int	screen_redraw_check_cell(struct client *, u_int, u_int, int,
	    struct window_pane **);
int	screen_redraw_check_is(u_int, u_int, int, int, struct window *,
	    struct window_pane *, struct window_pane *);

int 	screen_redraw_make_pane_status(struct client *, struct window *,
	    struct window_pane *);
void	screen_redraw_draw_pane_status(struct client *, int);

void	screen_redraw_draw_borders(struct client *, int, int, u_int);
void	screen_redraw_draw_panes(struct client *, u_int);
void	screen_redraw_draw_status(struct client *, u_int);
void	screen_redraw_draw_number(struct client *, struct window_pane *, u_int);

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

#define CELL_STATUS_OFF 0
#define CELL_STATUS_TOP 1
#define CELL_STATUS_BOTTOM 2

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
			return (2);
	}

	/* Top/bottom borders. */
	if ((wp->xoff == 0 || px >= wp->xoff - 1) && px <= wp->xoff + wp->sx) {
		if (wp->yoff != 0 && py == wp->yoff - 1)
			return (3);
		if (py == wp->yoff + wp->sy)
			return (4);
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
			return (!!retval);
	}

	return (0);
}

/* Check if cell inside a pane. */
int
screen_redraw_check_cell(struct client *c, u_int px, u_int py, int pane_status,
    struct window_pane **wpp)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	int			 borders;
	u_int			 right, line;

	if (px > w->sx || py > w->sy)
		return (CELL_OUTSIDE);

	if (pane_status != CELL_STATUS_OFF) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (!window_pane_visible(wp))
				continue;

			if (pane_status == CELL_STATUS_TOP)
				line = wp->yoff - 1;
			else
				line = wp->yoff + wp->sy;
			right = wp->xoff + 2 + wp->status_size - 1;

			if (py == line && px >= wp->xoff + 2 && px <= right)
				return (CELL_INSIDE);
		}
	}

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
		if (pane_status == CELL_STATUS_TOP) {
			if (py != 0 && screen_redraw_cell_border(c, px, py - 1))
				borders |= 2;
		} else {
			if (py == 0 || screen_redraw_cell_border(c, px, py - 1))
				borders |= 2;
		}
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

/* Check if the border of a particular pane. */
int
screen_redraw_check_is(u_int px, u_int py, int type, int pane_status,
    struct window *w, struct window_pane *wantwp, struct window_pane *wp)
{
	int	border;

	/* Is this off the active pane border? */
	border = screen_redraw_cell_border1(wantwp, px, py);
	if (border == 0 || border == -1)
		return (0);
	if (pane_status == CELL_STATUS_TOP && border == 4)
		return (0);
	if (pane_status == CELL_STATUS_BOTTOM && border == 3)
		return (0);

	/* If there are more than two panes, that's enough. */
	if (window_count_panes(w) != 2)
		return (1);

	/* Else if the cell is not a border cell, forget it. */
	if (wp == NULL || (type == CELL_OUTSIDE || type == CELL_INSIDE))
		return (1);

	/* With status lines mark the entire line. */
	if (pane_status != CELL_STATUS_OFF)
		return (1);

	/* Check if the pane covers the whole width. */
	if (wp->xoff == 0 && wp->sx == w->sx) {
		/* This can either be the top pane or the bottom pane. */
		if (wp->yoff == 0) { /* top pane */
			if (wp == wantwp)
				return (px <= wp->sx / 2);
			return (px > wp->sx / 2);
		}
		return (0);
	}

	/* Check if the pane covers the whole height. */
	if (wp->yoff == 0 && wp->sy == w->sy) {
		/* This can either be the left pane or the right pane. */
		if (wp->xoff == 0) { /* left pane */
			if (wp == wantwp)
				return (py <= wp->sy / 2);
			return (py > wp->sy / 2);
		}
		return (0);
	}

	return (1);
}

/* Update pane status. */
int
screen_redraw_make_pane_status(struct client *c, struct window *w,
    struct window_pane *wp)
{
	struct grid_cell	 gc;
	const char		*fmt;
	struct format_tree	*ft;
	char			*out;
	size_t			 outlen, old_size = wp->status_size;
	struct screen_write_ctx	 ctx;

	if (wp == w->active)
		style_apply(&gc, w->options, "pane-active-border-style");
	else
		style_apply(&gc, w->options, "pane-border-style");

	fmt = options_get_string(w->options, "pane-border-format");

	ft = format_create(NULL, 0);
	format_defaults(ft, c, NULL, NULL, wp);

	screen_free(&wp->status_screen);
	screen_init(&wp->status_screen, wp->sx, 1, 0);
	wp->status_screen.mode = 0;

	out = format_expand(ft, fmt);
	outlen = screen_write_cstrlen("%s", out);
	if (outlen > wp->sx - 4)
		outlen = wp->sx - 4;
	screen_resize(&wp->status_screen, outlen, 1, 0);

	screen_write_start(&ctx, NULL, &wp->status_screen);
	screen_write_cursormove(&ctx, 0, 0);
	screen_write_clearline(&ctx);
	screen_write_cnputs(&ctx, outlen, &gc, "%s", out);
	screen_write_stop(&ctx);

	format_free(ft);

	wp->status_size = outlen;
	return (wp->status_size != old_size);
}

/* Draw pane status. */
void
screen_redraw_draw_pane_status(struct client *c, int pane_status)
{
	struct window		*w = c->session->curw->window;
	struct options		*oo = c->session->options;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	int			 spos;
	u_int			 yoff;

	spos = options_get_number(oo, "status-position");
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (pane_status == CELL_STATUS_TOP)
			yoff = wp->yoff - 1;
		else
			yoff = wp->yoff + wp->sy;
		if (spos == 0)
			yoff += 1;

		tty_draw_line(tty, NULL, &wp->status_screen, 0, wp->xoff + 2,
		    yoff);
	}
	tty_cursor(tty, 0, 0);
}

/* Redraw entire screen. */
void
screen_redraw_screen(struct client *c, int draw_panes, int draw_status,
    int draw_borders)
{
	struct options		*oo = c->session->options;
	struct tty		*tty = &c->tty;
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	u_int			 top;
	int	 		 status, pane_status, spos;

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

	/* Update pane status lines. */
	pane_status = options_get_number(w->options, "pane-border-status");
	if (pane_status != CELL_STATUS_OFF && (draw_borders || draw_status)) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (screen_redraw_make_pane_status(c, w, wp))
				draw_borders = draw_status = 1;
		}
	}

	/* Draw the elements. */
	if (draw_borders)
		screen_redraw_draw_borders(c, status, pane_status, top);
	if (draw_panes)
		screen_redraw_draw_panes(c, top);
	if (draw_status)
		screen_redraw_draw_status(c, top);
	if (pane_status != CELL_STATUS_OFF && (draw_borders || draw_status))
		screen_redraw_draw_pane_status(c, pane_status);
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
		tty_draw_pane(&c->tty, wp, i, wp->xoff, yoff);
	tty_reset(&c->tty);
}

/* Draw the borders. */
void
screen_redraw_draw_borders(struct client *c, int status, int pane_status,
    u_int top)
{
	struct session		*s = c->session;
	struct window		*w = s->curw->window;
	struct options		*oo = w->options;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	struct grid_cell	 m_active_gc, active_gc, m_other_gc, other_gc;
	struct grid_cell	 msg_gc;
	u_int		 	 i, j, type, msgx = 0, msgy = 0;
	int			 active, small, flags;
	char			 msg[256];
	const char		*tmp;
	size_t			 msglen = 0;

	small = (tty->sy - status + top > w->sy) || (tty->sx > w->sx);
	if (small) {
		flags = w->flags & (WINDOW_FORCEWIDTH|WINDOW_FORCEHEIGHT);
		if (flags == (WINDOW_FORCEWIDTH|WINDOW_FORCEHEIGHT))
			tmp = "force-width, force-height";
		else if (flags == WINDOW_FORCEWIDTH)
			tmp = "force-width";
		else if (flags == WINDOW_FORCEHEIGHT)
			tmp = "force-height";
		else
			tmp = "a smaller client";
		xsnprintf(msg, sizeof msg, "(size %ux%u from %s)",
		    w->sx, w->sy, tmp);
		msglen = strlen(msg);

		if (tty->sy - 1 - status + top > w->sy && tty->sx >= msglen) {
			msgx = tty->sx - msglen;
			msgy = tty->sy - 1 - status + top;
		} else if (tty->sx - w->sx > msglen) {
			msgx = tty->sx - msglen;
			msgy = tty->sy - 1 - status + top;
		} else
			small = 0;
	}

	style_apply(&other_gc, oo, "pane-border-style");
	style_apply(&active_gc, oo, "pane-active-border-style");
	active_gc.attr = other_gc.attr = GRID_ATTR_CHARSET;

	memcpy(&m_other_gc, &other_gc, sizeof m_other_gc);
	m_other_gc.attr ^= GRID_ATTR_REVERSE;
	memcpy(&m_active_gc, &active_gc, sizeof m_active_gc);
	m_active_gc.attr ^= GRID_ATTR_REVERSE;

	for (j = 0; j < tty->sy - status; j++) {
		for (i = 0; i < tty->sx; i++) {
			type = screen_redraw_check_cell(c, i, j, pane_status,
			    &wp);
			if (type == CELL_INSIDE)
				continue;
			if (type == CELL_OUTSIDE && small &&
			    i > msgx && j == msgy)
				continue;
			active = screen_redraw_check_is(i, j, type, pane_status,
			    w, w->active, wp);
			if (server_is_marked(s, s->curw, marked_pane.wp) &&
			    screen_redraw_check_is(i, j, type, pane_status, w,
			    marked_pane.wp, wp)) {
				if (active)
					tty_attributes(tty, &m_active_gc, NULL);
				else
					tty_attributes(tty, &m_other_gc, NULL);
			} else if (active)
				tty_attributes(tty, &active_gc, NULL);
			else
				tty_attributes(tty, &other_gc, NULL);
			tty_cursor(tty, i, top + j);
			tty_putc(tty, CELL_BORDERS[type]);
		}
	}

	if (small) {
		memcpy(&msg_gc, &grid_default_cell, sizeof msg_gc);
		tty_attributes(tty, &msg_gc, NULL);
		tty_cursor(tty, msgx, msgy);
		tty_puts(tty, msg);
	}
}

/* Draw the panes. */
void
screen_redraw_draw_panes(struct client *c, u_int top)
{
	struct window		*w = c->session->curw->window;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	u_int		 	 i;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		for (i = 0; i < wp->sy; i++)
			tty_draw_pane(tty, wp, i, wp->xoff, top + wp->yoff);
		if (c->flags & CLIENT_IDENTIFY)
			screen_redraw_draw_number(c, wp, top);
	}
}

/* Draw the status line. */
void
screen_redraw_draw_status(struct client *c, u_int top)
{
	struct tty	*tty = &c->tty;

	if (top)
		tty_draw_line(tty, NULL, &c->status, 0, 0, 0);
	else
		tty_draw_line(tty, NULL, &c->status, 0, 0, tty->sy - 1);
}

/* Draw number on a pane. */
void
screen_redraw_draw_number(struct client *c, struct window_pane *wp, u_int top)
{
	struct tty		*tty = &c->tty;
	struct session		*s = c->session;
	struct options		*oo = s->options;
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

	if (top)
		yoff++;

	if (wp->sx < len * 6 || wp->sy < 5) {
		tty_cursor(tty, xoff + px - len / 2, yoff + py);
		goto draw_text;
	}

	px -= len * 3;
	py -= 2;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (w->active == wp)
		gc.bg = active_colour;
	else
		gc.bg = colour;
	tty_attributes(tty, &gc, wp);
	for (ptr = buf; *ptr != '\0'; ptr++) {
		if (*ptr < '0' || *ptr > '9')
			continue;
		idx = *ptr - '0';

		for (j = 0; j < 5; j++) {
			for (i = px; i < px + 5; i++) {
				tty_cursor(tty, xoff + i, yoff + py + j);
				if (window_clock_table[idx][j][i - px])
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
		gc.fg = active_colour;
	else
		gc.fg = colour;
	tty_attributes(tty, &gc, wp);
	tty_puts(tty, buf);

	tty_cursor(tty, 0, 0);
}
