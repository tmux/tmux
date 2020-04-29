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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static void	screen_redraw_draw_borders(struct screen_redraw_ctx *);
static void	screen_redraw_draw_panes(struct screen_redraw_ctx *);
static void	screen_redraw_draw_status(struct screen_redraw_ctx *);
static void	screen_redraw_draw_pane(struct screen_redraw_ctx *,
		    struct window_pane *);

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

/* Return if window has only two panes. */
static int
screen_redraw_two_panes(struct window *w)
{
	struct window_pane	*wp;

	wp = TAILQ_NEXT(TAILQ_FIRST(&w->panes), entry);
	if (wp == NULL)
		return (0); /* one pane */
	if (TAILQ_NEXT(wp, entry) != NULL)
		return (0); /* more than two panes */
	return (1);
}

/* Check if cell is on the border of a particular pane. */
static int
screen_redraw_cell_border1(struct window_pane *wp, u_int px, u_int py,
    int pane_status)
{
	u_int	ex = wp->xoff + wp->sx, ey = wp->yoff + wp->sy;

	/* Inside pane. */
	if (px >= wp->xoff && px < ex && py >= wp->yoff && py < ey)
		return (0);

	/* Left/right borders. */
	if (pane_status == PANE_STATUS_OFF) {
		if (screen_redraw_two_panes(wp->window)) {
			if (wp->xoff == 0 && px == wp->sx && py <= wp->sy / 2)
				return (2);
			if (wp->xoff != 0 &&
			    px == wp->xoff - 1 &&
			    py > wp->sy / 2)
				return (1);
		} else {
			if ((wp->yoff == 0 || py >= wp->yoff - 1) && py <= ey) {
				if (wp->xoff != 0 && px == wp->xoff - 1)
					return (1);
				if (px == ex)
					return (2);
			}
		}
	} else {
		if ((wp->yoff == 0 || py >= wp->yoff - 1) && py <= ey) {
			if (wp->xoff != 0 && px == wp->xoff - 1)
				return (1);
			if (px == ex)
				return (2);
		}
	}

	/* Top/bottom borders. */
	if (pane_status == PANE_STATUS_OFF) {
		if (screen_redraw_two_panes(wp->window)) {
			if (wp->yoff == 0 && py == wp->sy && px <= wp->sx / 2)
				return (4);
			if (wp->yoff != 0 &&
			    py == wp->yoff - 1 &&
			    px > wp->sx / 2)
				return (3);
		} else {
			if ((wp->xoff == 0 || px >= wp->xoff - 1) && px <= ex) {
				if (wp->yoff != 0 && py == wp->yoff - 1)
					return (3);
				if (py == ey)
					return (4);
			}
		}
	} else if (pane_status == PANE_STATUS_TOP) {
		if ((wp->xoff == 0 || px >= wp->xoff - 1) && px <= ex) {
			if (wp->yoff != 0 && py == wp->yoff - 1)
				return (3);
		}
	} else {
		if ((wp->xoff == 0 || px >= wp->xoff - 1) && px <= ex) {
			if (py == ey)
				return (4);
		}
	}

	/* Outside pane. */
	return (-1);
}

/* Check if a cell is on the pane border. */
static int
screen_redraw_cell_border(struct client *c, u_int px, u_int py, int pane_status)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	int			 retval;

	/* Check all the panes. */
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		retval = screen_redraw_cell_border1(wp, px, py, pane_status);
		if (retval != -1)
			return (!!retval);
	}

	return (0);
}

/* Check if cell inside a pane. */
static int
screen_redraw_check_cell(struct client *c, u_int px, u_int py, int pane_status,
    struct window_pane **wpp)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	int			 borders, border;
	u_int			 right, line;

	*wpp = NULL;

	if (px > w->sx || py > w->sy)
		return (CELL_OUTSIDE);

	if (pane_status != PANE_STATUS_OFF) {
		wp = w->active;
		do {
			if (!window_pane_visible(wp))
				goto next1;

			if (pane_status == PANE_STATUS_TOP)
				line = wp->yoff - 1;
			else
				line = wp->yoff + wp->sy;
			right = wp->xoff + 2 + wp->status_size - 1;

			if (py == line && px >= wp->xoff + 2 && px <= right)
				return (CELL_INSIDE);

		next1:
			wp = TAILQ_NEXT(wp, entry);
			if (wp == NULL)
				wp = TAILQ_FIRST(&w->panes);
		} while (wp != w->active);
	}

	wp = w->active;
	do {
		if (!window_pane_visible(wp))
			goto next2;
		*wpp = wp;

		/* If outside the pane and its border, skip it. */
		if ((wp->xoff != 0 && px < wp->xoff - 1) ||
		    px > wp->xoff + wp->sx ||
		    (wp->yoff != 0 && py < wp->yoff - 1) ||
		    py > wp->yoff + wp->sy)
			goto next2;

		/* If definitely inside, return. If not on border, skip. */
		border = screen_redraw_cell_border1(wp, px, py, pane_status);
		if (border == 0)
			return (CELL_INSIDE);
		if (border == -1)
			goto next2;

		/*
		 * Construct a bitmask of whether the cells to the left (bit
		 * 4), right, top, and bottom (bit 1) of this cell are borders.
		 */
		borders = 0;
		if (px == 0 ||
		    screen_redraw_cell_border(c, px - 1, py, pane_status))
			borders |= 8;
		if (px <= w->sx &&
		    screen_redraw_cell_border(c, px + 1, py, pane_status))
			borders |= 4;
		if (pane_status == PANE_STATUS_TOP &&
		    py != 0 &&
		    screen_redraw_cell_border(c, px, py - 1, pane_status))
			borders |= 2;
		else if (pane_status != PANE_STATUS_TOP &&
		    (py == 0 ||
		    screen_redraw_cell_border(c, px, py - 1, pane_status)))
			borders |= 2;
		if (py <= w->sy &&
		    screen_redraw_cell_border(c, px, py + 1, pane_status))
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

	next2:
		wp = TAILQ_NEXT(wp, entry);
		if (wp == NULL)
			wp = TAILQ_FIRST(&w->panes);
	} while (wp != w->active);

	return (CELL_OUTSIDE);
}

/* Check if the border of a particular pane. */
static int
screen_redraw_check_is(u_int px, u_int py, int pane_status,
    struct window_pane *wp)
{
	int	border;

	border = screen_redraw_cell_border1(wp, px, py, pane_status);
	if (border == 0 || border == -1)
		return (0);
	return (1);
}

/* Update pane status. */
static int
screen_redraw_make_pane_status(struct client *c, struct window *w,
    struct window_pane *wp)
{
	struct grid_cell	 gc;
	const char		*fmt;
	struct format_tree	*ft;
	char			*expanded;
	u_int			 width, i;
	struct screen_write_ctx	 ctx;
	struct screen		 old;

	ft = format_create(c, NULL, FORMAT_PANE|wp->id, FORMAT_STATUS);
	format_defaults(ft, c, c->session, c->session->curw, wp);

	if (wp == w->active)
		style_apply(&gc, w->options, "pane-active-border-style", ft);
	else
		style_apply(&gc, w->options, "pane-border-style", ft);
	fmt = options_get_string(w->options, "pane-border-format");

	expanded = format_expand_time(ft, fmt);
	if (wp->sx < 4)
		wp->status_size = width = 0;
	else
		wp->status_size = width = wp->sx - 4;

	memcpy(&old, &wp->status_screen, sizeof old);
	screen_init(&wp->status_screen, width, 1, 0);
	wp->status_screen.mode = 0;

	screen_write_start(&ctx, NULL, &wp->status_screen);

	gc.attr |= GRID_ATTR_CHARSET;
	for (i = 0; i < width; i++)
		screen_write_putc(&ctx, &gc, 'q');
	gc.attr &= ~GRID_ATTR_CHARSET;

	screen_write_cursormove(&ctx, 0, 0, 0);
	format_draw(&ctx, &gc, width, expanded, NULL);
	screen_write_stop(&ctx);

	free(expanded);
	format_free(ft);

	if (grid_compare(wp->status_screen.grid, old.grid) == 0) {
		screen_free(&old);
		return (0);
	}
	screen_free(&old);
	return (1);
}

/* Draw pane status. */
static void
screen_redraw_draw_pane_status(struct screen_redraw_ctx *ctx)
{
	struct client		*c = ctx->c;
	struct window		*w = c->session->curw->window;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	struct screen		*s;
	u_int			 i, x, width, xoff, yoff, size;

	log_debug("%s: %s @%u", __func__, c->name, w->id);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		s = &wp->status_screen;

		size = wp->status_size;
		if (ctx->pane_status == PANE_STATUS_TOP)
			yoff = wp->yoff - 1;
		else
			yoff = wp->yoff + wp->sy;
		xoff = wp->xoff + 2;

		if (xoff + size <= ctx->ox ||
		    xoff >= ctx->ox + ctx->sx ||
		    yoff < ctx->oy ||
		    yoff >= ctx->oy + ctx->sy)
			continue;

		if (xoff >= ctx->ox && xoff + size <= ctx->ox + ctx->sx) {
			/* All visible. */
			i = 0;
			x = xoff - ctx->ox;
			width = size;
		} else if (xoff < ctx->ox && xoff + size > ctx->ox + ctx->sx) {
			/* Both left and right not visible. */
			i = ctx->ox;
			x = 0;
			width = ctx->sx;
		} else if (xoff < ctx->ox) {
			/* Left not visible. */
			i = ctx->ox - xoff;
			x = 0;
			width = size - i;
		} else {
			/* Right not visible. */
			i = 0;
			x = xoff - ctx->ox;
			width = size - x;
		}

		if (ctx->statustop)
			yoff += ctx->statuslines;
		tty_draw_line(tty, NULL, s, i, 0, width, x, yoff - ctx->oy);
	}
	tty_cursor(tty, 0, 0);
}

/* Update status line and change flags if unchanged. */
static int
screen_redraw_update(struct client *c, int flags)
{
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	struct options		*wo = w->options;
	int			 redraw;

	if (c->message_string != NULL)
		redraw = status_message_redraw(c);
	else if (c->prompt_string != NULL)
		redraw = status_prompt_redraw(c);
	else
		redraw = status_redraw(c);
	if (!redraw && (~flags & CLIENT_REDRAWSTATUSALWAYS))
		flags &= ~CLIENT_REDRAWSTATUS;

	if (c->overlay_draw != NULL)
		flags |= CLIENT_REDRAWOVERLAY;

	if (options_get_number(wo, "pane-border-status") != PANE_STATUS_OFF) {
		redraw = 0;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (screen_redraw_make_pane_status(c, w, wp))
				redraw = 1;
		}
		if (redraw)
			flags |= CLIENT_REDRAWBORDERS;
	}
	return (flags);
}

/* Set up redraw context. */
static void
screen_redraw_set_context(struct client *c, struct screen_redraw_ctx *ctx)
{
	struct session	*s = c->session;
	struct options	*oo = s->options;
	struct window	*w = s->curw->window;
	struct options	*wo = w->options;
	u_int		 lines;

	memset(ctx, 0, sizeof *ctx);
	ctx->c = c;

	lines = status_line_size(c);
	if (c->message_string != NULL || c->prompt_string != NULL)
		lines = (lines == 0) ? 1 : lines;
	if (lines != 0 && options_get_number(oo, "status-position") == 0)
		ctx->statustop = 1;
	ctx->statuslines = lines;

	ctx->pane_status = options_get_number(wo, "pane-border-status");

	tty_window_offset(&c->tty, &ctx->ox, &ctx->oy, &ctx->sx, &ctx->sy);

	log_debug("%s: %s @%u ox=%u oy=%u sx=%u sy=%u %u/%d", __func__, c->name,
	    w->id, ctx->ox, ctx->oy, ctx->sx, ctx->sy, ctx->statuslines,
	    ctx->statustop);
}

/* Redraw entire screen. */
void
screen_redraw_screen(struct client *c)
{
	struct screen_redraw_ctx	ctx;
	int				flags;

	if (c->flags & CLIENT_SUSPENDED)
		return;

	flags = screen_redraw_update(c, c->flags);
	if ((flags & CLIENT_ALLREDRAWFLAGS) == 0)
		return;

	screen_redraw_set_context(c, &ctx);
	tty_update_mode(&c->tty, c->tty.mode, NULL);
	tty_sync_start(&c->tty);

	if (flags & (CLIENT_REDRAWWINDOW|CLIENT_REDRAWBORDERS)) {
		log_debug("%s: redrawing borders", c->name);
		if (ctx.pane_status != PANE_STATUS_OFF)
			screen_redraw_draw_pane_status(&ctx);
		screen_redraw_draw_borders(&ctx);
	}
	if (flags & CLIENT_REDRAWWINDOW) {
		log_debug("%s: redrawing panes", c->name);
		screen_redraw_draw_panes(&ctx);
	}
	if (ctx.statuslines != 0 &&
	    (flags & (CLIENT_REDRAWSTATUS|CLIENT_REDRAWSTATUSALWAYS))) {
		log_debug("%s: redrawing status", c->name);
		screen_redraw_draw_status(&ctx);
	}
	if (c->overlay_draw != NULL && (flags & CLIENT_REDRAWOVERLAY)) {
		log_debug("%s: redrawing overlay", c->name);
		c->overlay_draw(c, &ctx);
	}

	tty_reset(&c->tty);
}

/* Redraw a single pane. */
void
screen_redraw_pane(struct client *c, struct window_pane *wp)
{
	struct screen_redraw_ctx	 ctx;

	if (c->overlay_draw != NULL || !window_pane_visible(wp))
		return;

	screen_redraw_set_context(c, &ctx);
	tty_update_mode(&c->tty, c->tty.mode, NULL);
	tty_sync_start(&c->tty);

	screen_redraw_draw_pane(&ctx, wp);

	tty_reset(&c->tty);
}

/* Get border cell style. */
static const struct grid_cell *
screen_redraw_draw_borders_style(struct screen_redraw_ctx *ctx, u_int x,
    u_int y, struct window_pane *wp)
{
	struct client		*c = ctx->c;
	struct session		*s = c->session;
	struct window		*w = s->curw->window;
	struct options		*oo = w->options;
	struct grid_cell	*gc;
	struct format_tree	*ft;

	if (wp->border_gc_set)
		return (&wp->border_gc);
	wp->border_gc_set = 1;

	ft = format_create_defaults(NULL, c, s, s->curw, wp);
	gc = &wp->border_gc;

	if (screen_redraw_check_is(x, y, ctx->pane_status, w->active)) {
		style_apply(gc, oo, "pane-active-border-style", ft);
		gc->attr |= GRID_ATTR_CHARSET;
	} else {
		style_apply(gc, oo, "pane-border-style", ft);
		gc->attr |= GRID_ATTR_CHARSET;
	}

	format_free(ft);
	return (gc);
}

/* Draw a border cell. */
static void
screen_redraw_draw_borders_cell(struct screen_redraw_ctx *ctx, u_int i, u_int j)
{
	struct client		*c = ctx->c;
	struct session		*s = c->session;
	struct tty		*tty = &c->tty;
	struct window_pane	*wp;
	u_int			 type, x = ctx->ox + i, y = ctx->oy + j;
	const struct grid_cell	*gc;
	struct grid_cell	 copy;

	if (c->overlay_check != NULL && !c->overlay_check(c, x, y))
		return;

	type = screen_redraw_check_cell(c, x, y, ctx->pane_status, &wp);
	if (type == CELL_INSIDE)
		return;

	gc = screen_redraw_draw_borders_style(ctx, x, y, wp);
	if (gc == NULL)
		return;

	if (server_is_marked(s, s->curw, marked_pane.wp) &&
	    screen_redraw_check_is(x, y, ctx->pane_status, marked_pane.wp)) {
		memcpy(&copy, gc, sizeof copy);
		copy.attr ^= GRID_ATTR_REVERSE;
		gc = &copy;
	}

	tty_attributes(tty, gc, NULL);
	if (ctx->statustop)
		tty_cursor(tty, i, ctx->statuslines + j);
	else
		tty_cursor(tty, i, j);
	tty_putc(tty, CELL_BORDERS[type]);
}

/* Draw the borders. */
static void
screen_redraw_draw_borders(struct screen_redraw_ctx *ctx)
{
	struct client		*c = ctx->c;
	struct session		*s = c->session;
	struct window		*w = s->curw->window;
	struct window_pane	*wp;
	u_int		 	 i, j;

	log_debug("%s: %s @%u", __func__, c->name, w->id);

	TAILQ_FOREACH(wp, &w->panes, entry)
		wp->border_gc_set = 0;

	for (j = 0; j < c->tty.sy - ctx->statuslines; j++) {
		for (i = 0; i < c->tty.sx; i++)
			screen_redraw_draw_borders_cell(ctx, i, j);
	}
}

/* Draw the panes. */
static void
screen_redraw_draw_panes(struct screen_redraw_ctx *ctx)
{
	struct client		*c = ctx->c;
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;

	log_debug("%s: %s @%u", __func__, c->name, w->id);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (window_pane_visible(wp))
			screen_redraw_draw_pane(ctx, wp);
	}
}

/* Draw the status line. */
static void
screen_redraw_draw_status(struct screen_redraw_ctx *ctx)
{
	struct client	*c = ctx->c;
	struct window	*w = c->session->curw->window;
	struct tty	*tty = &c->tty;
	struct screen	*s = c->status.active;
	u_int		 i, y;

	log_debug("%s: %s @%u", __func__, c->name, w->id);

	if (ctx->statustop)
		y = 0;
	else
		y = c->tty.sy - ctx->statuslines;
	for (i = 0; i < ctx->statuslines; i++)
		tty_draw_line(tty, NULL, s, 0, i, UINT_MAX, 0, y + i);
}

/* Draw one pane. */
static void
screen_redraw_draw_pane(struct screen_redraw_ctx *ctx, struct window_pane *wp)
{
	struct client	*c = ctx->c;
	struct window	*w = c->session->curw->window;
	struct tty	*tty = &c->tty;
	struct screen	*s;
	u_int		 i, j, top, x, y, width;

	log_debug("%s: %s @%u %%%u", __func__, c->name, w->id, wp->id);

	if (wp->xoff + wp->sx <= ctx->ox || wp->xoff >= ctx->ox + ctx->sx)
		return;
	if (ctx->statustop)
		top = ctx->statuslines;
	else
		top = 0;

	s = wp->screen;
	for (j = 0; j < wp->sy; j++) {
		if (wp->yoff + j < ctx->oy || wp->yoff + j >= ctx->oy + ctx->sy)
			continue;
		y = top + wp->yoff + j - ctx->oy;

		if (wp->xoff >= ctx->ox &&
		    wp->xoff + wp->sx <= ctx->ox + ctx->sx) {
			/* All visible. */
			i = 0;
			x = wp->xoff - ctx->ox;
			width = wp->sx;
		} else if (wp->xoff < ctx->ox &&
		    wp->xoff + wp->sx > ctx->ox + ctx->sx) {
			/* Both left and right not visible. */
			i = ctx->ox;
			x = 0;
			width = ctx->sx;
		} else if (wp->xoff < ctx->ox) {
			/* Left not visible. */
			i = ctx->ox - wp->xoff;
			x = 0;
			width = wp->sx - i;
		} else {
			/* Right not visible. */
			i = 0;
			x = wp->xoff - ctx->ox;
			width = ctx->sx - x;
		}
		log_debug("%s: %s %%%u line %u,%u at %u,%u, width %u",
		    __func__, c->name, wp->id, i, j, x, y, width);

		tty_draw_line(tty, wp, s, i, j, width, x, y);
	}
}
