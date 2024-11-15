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
static void	screen_redraw_set_context(struct client *,
		    struct screen_redraw_ctx *);
static void	screen_redraw_draw_pane_scrollbars(struct screen_redraw_ctx *);
static void	screen_redraw_draw_scrollbar(struct screen_redraw_ctx *,
		    struct window_pane *, int, int, int, u_int, u_int, u_int);
static void	screen_redraw_draw_pane_scrollbar(struct screen_redraw_ctx *,
		    struct window_pane *);

#define START_ISOLATE "\342\201\246"
#define END_ISOLATE   "\342\201\251"

/* Border in relation to a pane. */
enum screen_redraw_border_type {
	SCREEN_REDRAW_OUTSIDE,
	SCREEN_REDRAW_INSIDE,
	SCREEN_REDRAW_BORDER_LEFT,
	SCREEN_REDRAW_BORDER_RIGHT,
	SCREEN_REDRAW_BORDER_TOP,
	SCREEN_REDRAW_BORDER_BOTTOM
};
#define BORDER_MARKERS "  +,.-"

/* Get cell border character. */
static void
screen_redraw_border_set(struct window *w, struct window_pane *wp,
    enum pane_lines pane_lines, int cell_type, struct grid_cell *gc)
{
	u_int	idx;

	if (cell_type == CELL_OUTSIDE && w->fill_character != NULL) {
		utf8_copy(&gc->data, &w->fill_character[0]);
		return;
	}

	switch (pane_lines) {
	case PANE_LINES_NUMBER:
		if (cell_type == CELL_OUTSIDE) {
			gc->attr |= GRID_ATTR_CHARSET;
			utf8_set(&gc->data, CELL_BORDERS[CELL_OUTSIDE]);
			break;
		}
		gc->attr &= ~GRID_ATTR_CHARSET;
		if (wp != NULL && window_pane_index(wp, &idx) == 0)
			utf8_set(&gc->data, '0' + (idx % 10));
		else
			utf8_set(&gc->data, '*');
		break;
	case PANE_LINES_DOUBLE:
		gc->attr &= ~GRID_ATTR_CHARSET;
		utf8_copy(&gc->data, tty_acs_double_borders(cell_type));
		break;
	case PANE_LINES_HEAVY:
		gc->attr &= ~GRID_ATTR_CHARSET;
		utf8_copy(&gc->data, tty_acs_heavy_borders(cell_type));
		break;
	case PANE_LINES_SIMPLE:
		gc->attr &= ~GRID_ATTR_CHARSET;
		utf8_set(&gc->data, SIMPLE_BORDERS[cell_type]);
		break;
	default:
		gc->attr |= GRID_ATTR_CHARSET;
		utf8_set(&gc->data, CELL_BORDERS[cell_type]);
		break;
	}
}

/* Return if window has only two panes. */
static int
screen_redraw_two_panes(struct window *w, int direction)
{
	struct window_pane	*wp;

	wp = TAILQ_NEXT(TAILQ_FIRST(&w->panes), entry);
	if (wp == NULL)
		return (0); /* one pane */
	if (TAILQ_NEXT(wp, entry) != NULL)
		return (0); /* more than two panes */
	if (direction == 0 && wp->xoff == 0)
		return (0);
	if (direction == 1 && wp->yoff == 0)
		return (0);
	return (1);
}

/* Check if cell is on the border of a pane. */
static enum screen_redraw_border_type
screen_redraw_pane_border(struct screen_redraw_ctx *ctx, struct window_pane *wp,
    u_int px, u_int py)
{
	struct options	*oo = wp->window->options;
	u_int		 ex = wp->xoff + wp->sx, ey = wp->yoff + wp->sy;
	int		 hsplit = 0, vsplit = 0, pane_status = ctx->pane_status;
	int		 pane_scrollbars = ctx->pane_scrollbars, sb_w = 0;
	int		 sb_pos = ctx->pane_scrollbars_pos;

	/* Inside pane. */
	if (px >= wp->xoff && px < ex && py >= wp->yoff && py < ey)
		return (SCREEN_REDRAW_INSIDE);

	/* Get pane indicator. */
	switch (options_get_number(oo, "pane-border-indicators")) {
	case PANE_BORDER_COLOUR:
	case PANE_BORDER_BOTH:
		hsplit = screen_redraw_two_panes(wp->window, 0);
		vsplit = screen_redraw_two_panes(wp->window, 1);
		break;
	}

	/* Are scrollbars enabled? */
	if (window_pane_show_scrollbar(wp, pane_scrollbars))
		sb_w = wp->scrollbar_style.width + wp->scrollbar_style.pad;

	/*
	 * Left/right borders. The wp->sy / 2 test is to colour only half the
	 * active window's border when there are two panes.
	 */
	if ((wp->yoff == 0 || py >= wp->yoff - 1) && py <= ey) {
		if (sb_pos == PANE_SCROLLBARS_LEFT) {
			if (wp->xoff - sb_w == 0 && px == wp->sx + sb_w)
				if (!hsplit || (hsplit && py <= wp->sy / 2))
					return (SCREEN_REDRAW_BORDER_RIGHT);
			if (wp->xoff - sb_w != 0 && px == wp->xoff - sb_w - 1)
				if (!hsplit || (hsplit && py > wp->sy / 2))
					return (SCREEN_REDRAW_BORDER_LEFT);
		} else { /* sb_pos == PANE_SCROLLBARS_RIGHT */
			if (wp->xoff == 0 && px == wp->sx + sb_w)
				if (!hsplit || (hsplit && py <= wp->sy / 2))
					return (SCREEN_REDRAW_BORDER_RIGHT);
			if (wp->xoff != 0 && px == wp->xoff - 1)
				if (!hsplit || (hsplit && py > wp->sy / 2))
					return (SCREEN_REDRAW_BORDER_LEFT);
		}
	}

	/* Top/bottom borders. */
	if (vsplit && pane_status == PANE_STATUS_OFF && sb_w == 0) {
		if (wp->yoff == 0 && py == wp->sy && px <= wp->sx / 2)
			return (SCREEN_REDRAW_BORDER_BOTTOM);
		if (wp->yoff != 0 && py == wp->yoff - 1 && px > wp->sx / 2)
			return (SCREEN_REDRAW_BORDER_TOP);
	} else {
		if (sb_pos == PANE_SCROLLBARS_LEFT) {
			if ((wp->xoff - sb_w == 0 || px >= wp->xoff - sb_w) &&
			    (px <= ex || (sb_w != 0 && px < ex + sb_w))) {
				if (wp->yoff != 0 && py == wp->yoff - 1)
					return (SCREEN_REDRAW_BORDER_TOP);
				if (py == ey)
					return (SCREEN_REDRAW_BORDER_BOTTOM);
			}
		} else { /* sb_pos == PANE_SCROLLBARS_RIGHT */
			if ((wp->xoff == 0 || px >= wp->xoff) &&
			    (px <= ex || (sb_w != 0 && px < ex + sb_w))) {
				if (wp->yoff != 0 && py == wp->yoff - 1)
					return (SCREEN_REDRAW_BORDER_TOP);
				if (py == ey)
					return (SCREEN_REDRAW_BORDER_BOTTOM);
			}
		}
	}

	/* Outside pane. */
	return (SCREEN_REDRAW_OUTSIDE);
}

/* Check if a cell is on a border. */
static int
screen_redraw_cell_border(struct screen_redraw_ctx *ctx, u_int px, u_int py)
{
	struct client		*c = ctx->c;
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;
	u_int			 sy = w->sy;

	if (ctx->pane_status == PANE_STATUS_BOTTOM)
		sy--;

	/* Outside the window? */
	if (px > w->sx || py > sy)
		return (0);

	/* On the window border? */
	if (px == w->sx || py == sy)
		return (1);

	/* Check all the panes. */
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_pane_visible(wp))
			continue;
		switch (screen_redraw_pane_border(ctx, wp, px, py)) {
		case SCREEN_REDRAW_INSIDE:
			return (0);
		case SCREEN_REDRAW_OUTSIDE:
			break;
		default:
			return (1);
		}
	}

	return (0);
}

/* Work out type of border cell from surrounding cells. */
static int
screen_redraw_type_of_cell(struct screen_redraw_ctx *ctx, u_int px, u_int py)
{
	struct client	*c = ctx->c;
	int		 pane_status = ctx->pane_status;
	struct window	*w = c->session->curw->window;
	u_int		 sx = w->sx, sy = w->sy;
	int		 borders = 0;

	if (pane_status == PANE_STATUS_BOTTOM)
		sy--;

	/* Is this outside the window? */
	if (px > sx || py > sy)
		return (CELL_OUTSIDE);

	/*
	 * Construct a bitmask of whether the cells to the left (bit 8), right,
	 * top, and bottom (bit 1) of this cell are borders.
	 *
	 * bits 8 4 2 1:     2
	 *		   8 + 4
	 *		     1
	 */
	if (px == 0 || screen_redraw_cell_border(ctx, px - 1, py))
		borders |= 8;
	if (px <= sx && screen_redraw_cell_border(ctx, px + 1, py))
		borders |= 4;
	if (pane_status == PANE_STATUS_TOP) {
		if (py != 0 &&
		    screen_redraw_cell_border(ctx, px, py - 1))
			borders |= 2;
		if (screen_redraw_cell_border(ctx, px, py + 1))
			borders |= 1;
	} else if (pane_status == PANE_STATUS_BOTTOM) {
		if (py == 0 ||
		    screen_redraw_cell_border(ctx, px, py - 1))
			borders |= 2;
		if (py != sy &&
		    screen_redraw_cell_border(ctx, px, py + 1))
			borders |= 1;
	} else {
		if (py == 0 ||
		    screen_redraw_cell_border(ctx, px, py - 1))
			borders |= 2;
		if (screen_redraw_cell_border(ctx, px, py + 1))
			borders |= 1;
	}

	/*
	 * Figure out what kind of border this cell is. Only one bit set
	 * doesn't make sense (can't have a border cell with no others
	 * connected).
	 */
	switch (borders) {
	case 15:	/* 1111, left right top bottom */
		return (CELL_JOIN);
	case 14:	/* 1110, left right top */
		return (CELL_BOTTOMJOIN);
	case 13:	/* 1101, left right bottom */
		return (CELL_TOPJOIN);
	case 12:	/* 1100, left right */
		return (CELL_LEFTRIGHT);
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
		return (CELL_TOPBOTTOM);
	}
	return (CELL_OUTSIDE);
}

/* Check if cell inside a pane. */
static int
screen_redraw_check_cell(struct screen_redraw_ctx *ctx, u_int px, u_int py,
    struct window_pane **wpp)
{
	struct client		*c = ctx->c;
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp, *active;
	int			 pane_status = ctx->pane_status;
	u_int			 sx = w->sx, sy = w->sy;
	int			 border, pane_scrollbars = ctx->pane_scrollbars;
	u_int			 right, line;
	int			 sb_pos = ctx->pane_scrollbars_pos;
	int			 sb_w;

	*wpp = NULL;

	if (px > sx || py > sy)
		return (CELL_OUTSIDE);
	if (px == sx || py == sy) /* window border */
		return (screen_redraw_type_of_cell(ctx, px, py));

	if (pane_status != PANE_STATUS_OFF) {
		active = wp = server_client_get_pane(c);
		do {
			if (!window_pane_visible(wp))
				goto next1;

			if (pane_status == PANE_STATUS_TOP)
				line = wp->yoff - 1;
			else
				line = wp->yoff + sy;
			right = wp->xoff + 2 + wp->status_size - 1;

			if (py == line && px >= wp->xoff + 2 && px <= right)
				return (CELL_INSIDE);

		next1:
			wp = TAILQ_NEXT(wp, entry);
			if (wp == NULL)
				wp = TAILQ_FIRST(&w->panes);
		} while (wp != active);
	}

	active = wp = server_client_get_pane(c);
	do {
		if (!window_pane_visible(wp))
			goto next2;
		*wpp = wp;

		/* Check if CELL_SCROLLBAR */
		if (window_pane_show_scrollbar(wp, pane_scrollbars)) {

			if (pane_status == PANE_STATUS_TOP)
				line = wp->yoff - 1;
			else
				line = wp->yoff + wp->sy;

			/*
			 * Check if py could lie within a scrollbar. If the
			 * pane is at the top then py == 0 to sy; if the pane
			 * is not at the top, then yoff to yoff + sy.
			 */
			sb_w = wp->scrollbar_style.width +
			    wp->scrollbar_style.pad;
			if ((pane_status && py != line) ||
			    (wp->yoff == 0 && py < wp->sy) ||
			     (py >= wp->yoff && py < wp->yoff + wp->sy)) {
				/* Check if px lies within a scrollbar. */
				if ((sb_pos == PANE_SCROLLBARS_RIGHT &&
				     (px >= wp->xoff + wp->sx &&
				      px < wp->xoff + wp->sx + sb_w)) ||
				    (sb_pos == PANE_SCROLLBARS_LEFT &&
				     (px >= wp->xoff - sb_w &&
				      px < wp->xoff)))
					return (CELL_SCROLLBAR);
			}
		}

		/*
		 * If definitely inside, return. If not on border, skip.
		 * Otherwise work out the cell.
		 */
		border = screen_redraw_pane_border(ctx, wp, px, py);
		if (border == SCREEN_REDRAW_INSIDE)
			return (CELL_INSIDE);
		if (border == SCREEN_REDRAW_OUTSIDE)
			goto next2;
		return (screen_redraw_type_of_cell(ctx, px, py));

	next2:
		wp = TAILQ_NEXT(wp, entry);
		if (wp == NULL)
			wp = TAILQ_FIRST(&w->panes);
	} while (wp != active);

	return (CELL_OUTSIDE);
}

/* Check if the border of a particular pane. */
static int
screen_redraw_check_is(struct screen_redraw_ctx *ctx, u_int px, u_int py,
    struct window_pane *wp)
{
	enum screen_redraw_border_type	border;

	border = screen_redraw_pane_border(ctx, wp, px, py);
	if (border != SCREEN_REDRAW_INSIDE && border != SCREEN_REDRAW_OUTSIDE)
		return (1);
	return (0);
}

/* Update pane status. */
static int
screen_redraw_make_pane_status(struct client *c, struct window_pane *wp,
    struct screen_redraw_ctx *rctx, enum pane_lines pane_lines)
{
	struct window		*w = wp->window;
	struct grid_cell	 gc;
	const char		*fmt;
	struct format_tree	*ft;
	char			*expanded;
	int			 pane_status = rctx->pane_status;
	u_int			 width, i, cell_type, px, py;
	struct screen_write_ctx	 ctx;
	struct screen		 old;

	ft = format_create(c, NULL, FORMAT_PANE|wp->id, FORMAT_STATUS);
	format_defaults(ft, c, c->session, c->session->curw, wp);

	if (wp == server_client_get_pane(c))
		style_apply(&gc, w->options, "pane-active-border-style", ft);
	else
		style_apply(&gc, w->options, "pane-border-style", ft);
	fmt = options_get_string(wp->options, "pane-border-format");

	expanded = format_expand_time(ft, fmt);
	if (wp->sx < 4)
		wp->status_size = width = 0;
	else
		wp->status_size = width = wp->sx - 4;

	memcpy(&old, &wp->status_screen, sizeof old);
	screen_init(&wp->status_screen, width, 1, 0);
	wp->status_screen.mode = 0;

	screen_write_start(&ctx, &wp->status_screen);

	for (i = 0; i < width; i++) {
		px = wp->xoff + 2 + i;
		if (pane_status == PANE_STATUS_TOP)
			py = wp->yoff - 1;
		else
			py = wp->yoff + wp->sy;
		cell_type = screen_redraw_type_of_cell(rctx, px, py);
		screen_redraw_border_set(w, wp, pane_lines, cell_type, &gc);
		screen_write_cell(&ctx, &gc);
	}
	gc.attr &= ~GRID_ATTR_CHARSET;

	screen_write_cursormove(&ctx, 0, 0, 0);
	format_draw(&ctx, &gc, width, expanded, NULL, 0);
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
		tty_draw_line(tty, s, i, 0, width, x, yoff - ctx->oy,
		    &grid_default_cell, NULL);
	}
	tty_cursor(tty, 0, 0);
}

/* Update status line and change flags if unchanged. */
static uint64_t
screen_redraw_update(struct screen_redraw_ctx *ctx, uint64_t flags)
{
	struct client			*c = ctx->c;
	struct window			*w = c->session->curw->window;
	struct window_pane		*wp;
	int				 redraw;
	enum pane_lines			 lines;

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

	if (ctx->pane_status != PANE_STATUS_OFF) {
		lines = ctx->pane_lines;
		redraw = 0;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (screen_redraw_make_pane_status(c, wp, ctx, lines))
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
	ctx->pane_lines = options_get_number(wo, "pane-border-lines");

	ctx->pane_scrollbars = options_get_number(wo, "pane-scrollbars");
	ctx->pane_scrollbars_pos = options_get_number(wo,
	    "pane-scrollbars-position");

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
	uint64_t			flags;

	if (c->flags & CLIENT_SUSPENDED)
		return;

	screen_redraw_set_context(c, &ctx);

	flags = screen_redraw_update(&ctx, c->flags);
	if ((flags & CLIENT_ALLREDRAWFLAGS) == 0)
		return;

	tty_sync_start(&c->tty);
	tty_update_mode(&c->tty, c->tty.mode, NULL);

	if (flags & (CLIENT_REDRAWWINDOW|CLIENT_REDRAWBORDERS)) {
		log_debug("%s: redrawing borders", c->name);
		screen_redraw_draw_borders(&ctx);
		if (ctx.pane_status != PANE_STATUS_OFF)
			screen_redraw_draw_pane_status(&ctx);
		screen_redraw_draw_pane_scrollbars(&ctx);
	}
	if (flags & CLIENT_REDRAWWINDOW) {
		log_debug("%s: redrawing panes", c->name);
		screen_redraw_draw_panes(&ctx);
		screen_redraw_draw_pane_scrollbars(&ctx);
	}
	if (ctx.statuslines != 0 &&
	    (flags & (CLIENT_REDRAWSTATUS|CLIENT_REDRAWSTATUSALWAYS))) {
		log_debug("%s: redrawing status", c->name);
		screen_redraw_draw_status(&ctx);
	}
	if (c->overlay_draw != NULL && (flags & CLIENT_REDRAWOVERLAY)) {
		log_debug("%s: redrawing overlay", c->name);
		c->overlay_draw(c, c->overlay_data, &ctx);
	}

	tty_reset(&c->tty);
}

/* Redraw a single pane and its scrollbar. */
void
screen_redraw_pane(struct client *c, struct window_pane *wp,
    int redraw_scrollbar_only)
{
	struct screen_redraw_ctx	ctx;

	if (!window_pane_visible(wp))
		return;

	screen_redraw_set_context(c, &ctx);
	tty_sync_start(&c->tty);
	tty_update_mode(&c->tty, c->tty.mode, NULL);

	if (!redraw_scrollbar_only)
		screen_redraw_draw_pane(&ctx, wp);

	if (window_pane_show_scrollbar(wp, ctx.pane_scrollbars))
		screen_redraw_draw_pane_scrollbar(&ctx, wp);

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
	struct window_pane	*active = server_client_get_pane(c);
	struct options		*oo = w->options;
	struct format_tree	*ft;

	if (wp->border_gc_set)
		return (&wp->border_gc);
	wp->border_gc_set = 1;

	ft = format_create_defaults(NULL, c, s, s->curw, wp);
	if (screen_redraw_check_is(ctx, x, y, active))
		style_apply(&wp->border_gc, oo, "pane-active-border-style", ft);
	else
		style_apply(&wp->border_gc, oo, "pane-border-style", ft);
	format_free(ft);

	return (&wp->border_gc);
}

/* Draw a border cell. */
static void
screen_redraw_draw_borders_cell(struct screen_redraw_ctx *ctx, u_int i, u_int j)
{
	struct client		*c = ctx->c;
	struct session		*s = c->session;
	struct window		*w = s->curw->window;
	struct options		*oo = w->options;
	struct tty		*tty = &c->tty;
	struct format_tree	*ft;
	struct window_pane	*wp, *active = server_client_get_pane(c);
	struct grid_cell	 gc;
	const struct grid_cell	*tmp;
	struct overlay_ranges	 r;
	u_int			 cell_type, x = ctx->ox + i, y = ctx->oy + j;
	int			 arrows = 0, border, isolates;

	if (c->overlay_check != NULL) {
		c->overlay_check(c, c->overlay_data, x, y, 1, &r);
		if (r.nx[0] + r.nx[1] == 0)
			return;
	}

	cell_type = screen_redraw_check_cell(ctx, x, y, &wp);
	if (cell_type == CELL_INSIDE || cell_type == CELL_SCROLLBAR)
		return;

	if (wp == NULL) {
		if (!ctx->no_pane_gc_set) {
			ft = format_create_defaults(NULL, c, s, s->curw, NULL);
			memcpy(&ctx->no_pane_gc, &grid_default_cell, sizeof gc);
			style_add(&ctx->no_pane_gc, oo, "pane-border-style",
			    ft);
			format_free(ft);
			ctx->no_pane_gc_set = 1;
		}
		memcpy(&gc, &ctx->no_pane_gc, sizeof gc);
	} else {
		tmp = screen_redraw_draw_borders_style(ctx, x, y, wp);
		if (tmp == NULL)
			return;
		memcpy(&gc, tmp, sizeof gc);

		if (server_is_marked(s, s->curw, marked_pane.wp) &&
		    screen_redraw_check_is(ctx, x, y, marked_pane.wp))
			gc.attr ^= GRID_ATTR_REVERSE;
	}
	screen_redraw_border_set(w, wp, ctx->pane_lines, cell_type, &gc);

	if (cell_type == CELL_TOPBOTTOM &&
	    (c->flags & CLIENT_UTF8) &&
	    tty_term_has(tty->term, TTYC_BIDI))
		isolates = 1;
	else
		isolates = 0;

	if (ctx->statustop)
		tty_cursor(tty, i, ctx->statuslines + j);
	else
		tty_cursor(tty, i, j);
	if (isolates)
		tty_puts(tty, END_ISOLATE);

	switch (options_get_number(oo, "pane-border-indicators")) {
	case PANE_BORDER_ARROWS:
	case PANE_BORDER_BOTH:
		arrows = 1;
		break;
	}

	if (wp != NULL && arrows) {
		border = screen_redraw_pane_border(ctx, active, x, y);
		if (((i == wp->xoff + 1 &&
		    (cell_type == CELL_LEFTRIGHT ||
		    (cell_type == CELL_TOPJOIN &&
		    border == SCREEN_REDRAW_BORDER_BOTTOM) ||
		    (cell_type == CELL_BOTTOMJOIN &&
		    border == SCREEN_REDRAW_BORDER_TOP))) ||
		    (j == wp->yoff + 1 &&
		    (cell_type == CELL_TOPBOTTOM ||
		    (cell_type == CELL_LEFTJOIN &&
		    border == SCREEN_REDRAW_BORDER_RIGHT) ||
		    (cell_type == CELL_RIGHTJOIN &&
		    border == SCREEN_REDRAW_BORDER_LEFT)))) &&
		    screen_redraw_check_is(ctx, x, y, active)) {
			gc.attr |= GRID_ATTR_CHARSET;
			utf8_set(&gc.data, BORDER_MARKERS[border]);
		}
	}

	tty_cell(tty, &gc, &grid_default_cell, NULL, NULL);
	if (isolates)
		tty_puts(tty, START_ISOLATE);
}

/* Draw the borders. */
static void
screen_redraw_draw_borders(struct screen_redraw_ctx *ctx)
{
	struct client		*c = ctx->c;
	struct session		*s = c->session;
	struct window		*w = s->curw->window;
	struct window_pane	*wp;
	u_int			 i, j;

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
	for (i = 0; i < ctx->statuslines; i++) {
		tty_draw_line(tty, s, 0, i, UINT_MAX, 0, y + i,
		    &grid_default_cell, NULL);
	}
}

/* Draw one pane. */
static void
screen_redraw_draw_pane(struct screen_redraw_ctx *ctx, struct window_pane *wp)
{
	struct client		*c = ctx->c;
	struct window		*w = c->session->curw->window;
	struct tty		*tty = &c->tty;
	struct screen		*s = wp->screen;
	struct colour_palette	*palette = &wp->palette;
	struct grid_cell	 defaults;
	u_int			 i, j, top, x, y, width;

	log_debug("%s: %s @%u %%%u", __func__, c->name, w->id, wp->id);

	if (wp->xoff + wp->sx <= ctx->ox || wp->xoff >= ctx->ox + ctx->sx)
		return;
	if (ctx->statustop)
		top = ctx->statuslines;
	else
		top = 0;
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

		tty_default_colours(&defaults, wp);
		tty_draw_line(tty, s, i, j, width, x, y, &defaults, palette);
	}

#ifdef ENABLE_SIXEL
	tty_draw_images(c, wp, s);
#endif
}

/* Draw the panes scrollbars */
static void
screen_redraw_draw_pane_scrollbars(struct screen_redraw_ctx *ctx)
{
	struct client		*c = ctx->c;
	struct window		*w = c->session->curw->window;
	struct window_pane	*wp;

	log_debug("%s: %s @%u", __func__, c->name, w->id);

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (window_pane_show_scrollbar(wp, ctx->pane_scrollbars) &&
		    window_pane_visible(wp))
			screen_redraw_draw_pane_scrollbar(ctx, wp);
	}
}

/* Draw pane scrollbar. */
void
screen_redraw_draw_pane_scrollbar(struct screen_redraw_ctx *ctx,
    struct window_pane *wp)
{
	struct screen	*s = wp->screen;
	double		 percent_view;
	u_int		 sb = ctx->pane_scrollbars, total_height, sb_h = wp->sy;
	u_int		 sb_pos = ctx->pane_scrollbars_pos, slider_h, slider_y;
	int		 sb_w = wp->scrollbar_style.width;
	int		 sb_pad = wp->scrollbar_style.pad;
	int		 cm_y, cm_size, xoff = wp->xoff, ox = ctx->ox;
	int		 sb_x, sb_y = (int)(wp->yoff - ctx->oy); /* sb top */

	if (window_pane_mode(wp) == WINDOW_PANE_NO_MODE) {
		if (sb == PANE_SCROLLBARS_MODAL)
			return;
		/* Show slider at the bottom of the scrollbar. */
		total_height = screen_size_y(s) + screen_hsize(s);
		percent_view = (double)sb_h / total_height;
		slider_h = (double)sb_h * percent_view;
		slider_y = sb_h - slider_h;
	} else {
		if (TAILQ_FIRST(&wp->modes) == NULL)
			return;
		if (window_copy_get_current_offset(wp, &cm_y, &cm_size) == 0)
			return;
		total_height = cm_size + sb_h;
		percent_view = (double)sb_h / (cm_size + sb_h);
		slider_h = (double)sb_h * percent_view;
		slider_y = (sb_h + 1) * ((double)cm_y / total_height);
	}

	if (sb_pos == PANE_SCROLLBARS_LEFT)
		sb_x = xoff - sb_w - sb_pad - ox;
	else
		sb_x = xoff + wp->sx - ox;

	if (slider_h < 1)
		slider_h = 1;
	if (slider_y >= sb_h)
		slider_y = sb_h - 1;

	screen_redraw_draw_scrollbar(ctx, wp, sb_pos, sb_x, sb_y, sb_h,
	    slider_h, slider_y);

	/* Store current position and height of the slider */
	wp->sb_slider_y = slider_y;  /* top of slider y pos in scrollbar */
	wp->sb_slider_h = slider_h;  /* height of slider */
}

static void
screen_redraw_draw_scrollbar(struct screen_redraw_ctx *ctx,
    struct window_pane *wp, int sb_pos, int sb_x, int sb_y, u_int sb_h,
    u_int slider_h, u_int slider_y)
{
	struct client		*c = ctx->c;
	struct tty		*tty = &c->tty;
	struct grid_cell	 gc, slgc, *gcp;
	struct style		*sb_style = &wp->scrollbar_style;
	u_int			 i, j, imax, jmax;
	u_int			 sb_w = sb_style->width, sb_pad = sb_style->pad;
	int			 px, py, ox = ctx->ox, oy = ctx->oy;
	int			 sx = ctx->sx, sy = ctx->sy, xoff = wp->xoff;
	int			 yoff = wp->yoff;

	/* Set up style for slider. */
	gc = sb_style->gc;
	memcpy(&slgc, &gc, sizeof slgc);
	slgc.fg = gc.bg;
	slgc.bg = gc.fg;

	imax = sb_w + sb_pad;
	if ((int)imax + sb_x > sx)
		imax = sx - sb_x;
	jmax = sb_h;
	if ((int)jmax + sb_y > sy)
		jmax = sy - sb_y;

	for (j = 0; j < jmax; j++) {
		py = sb_y + j;
		for (i = 0; i < imax; i++) {
			px = sb_x + i;
			if (px < xoff - ox - (int)sb_w - (int)sb_pad ||
			    px >= sx || px < 0 ||
			    py < yoff - oy - 1 ||
			    py >= sy || py < 0)
				continue;
			tty_cursor(tty, px, py);
			if ((sb_pos == PANE_SCROLLBARS_LEFT &&
			    i >= sb_w && i < sb_w + sb_pad) ||
			    (sb_pos == PANE_SCROLLBARS_RIGHT &&
			     i < sb_pad)) {
				tty_cell(tty, &grid_default_cell,
				    &grid_default_cell, NULL, NULL);
			} else {
				if (j >= slider_y && j < slider_y + slider_h)
					gcp = &slgc;
				else
					gcp = &gc;
				tty_cell(tty, gcp, &grid_default_cell, NULL,
				    NULL);
			}
		}
	}
}
