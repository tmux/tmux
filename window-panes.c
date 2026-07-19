/* $OpenBSD: window-panes.c,v 1.3 2026/07/19 19:53:11 nicm Exp $ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static struct screen	*window_panes_init(struct window_mode_entry *,
			     struct cmdq_item *, struct cmd_find_state *,
			     struct args *);
static void		 window_panes_free(struct window_mode_entry *);
static void		 window_panes_resize(struct window_mode_entry *, u_int,
			     u_int);
static void		 window_panes_key(struct window_mode_entry *,
			     struct client *, struct session *,
			     struct winlink *, key_code, struct mouse_event *);

const struct window_mode window_panes_mode = {
	.name = "panes-mode",
	.flags = WINDOW_MODE_HIDE_PANE_STATUS|WINDOW_MODE_NO_STACK,

	.init = window_panes_init,
	.free = window_panes_free,
	.resize = window_panes_resize,
	.key = window_panes_key,
};

struct window_panes_area {
	u_int	id;
	u_int	x;
	u_int	y;
	u_int	sx;
	u_int	sy;
};

struct window_panes_modedata {
	struct window_pane		*wp;
	struct session			*session;
	u_int				 source_session;
	u_int				 source_window;
	struct screen			 screen;
	struct screen			*preview;

	struct event			 timer;

	struct args_command_state	*state;
	u_int				 delay;
	int				 ignore_keys;
	int				 zoomed;

	struct window_panes_area	*areas;
	u_int				 areas_size;
};

#define WINDOW_PANES_BORDER_L 0x1
#define WINDOW_PANES_BORDER_R 0x2
#define WINDOW_PANES_BORDER_U 0x4
#define WINDOW_PANES_BORDER_D 0x8

static int
window_panes_get_source(struct window_panes_modedata *data, struct session **sp,
    struct winlink **wlp, struct window **wp)
{
	struct session	*s;
	struct window	*w;
	struct winlink	*wl = NULL;

	w = window_find_by_id(data->source_window);
	if (w == NULL)
		return (0);

	s = session_find_by_id(data->source_session);
	if (s != NULL)
		wl = winlink_find_by_window(&s->windows, w);
	if (wl == NULL)
		s = data->session;
	if (s != NULL)
		wl = winlink_find_by_window(&s->windows, w);

	if (sp != NULL)
		*sp = s;
	if (wlp != NULL)
		*wlp = wl;
	if (wp != NULL)
		*wp = w;
	return (1);
}

static void
window_panes_set_preview(struct window_panes_modedata *data)
{
	struct window_pane	*wp = data->wp;
	struct screen		*src = &wp->base, *dst;
	struct screen_write_ctx	 ctx;
	u_int			 sx = screen_size_x(src), sy = screen_size_y(src);

	data->preview = dst = xmalloc(sizeof *data->preview);
	screen_init(dst, sx, sy, 0);
	screen_write_start(&ctx, dst);
	screen_write_fast_copy(&ctx, src, 0, src->grid->hsize, sx, sy);
	screen_write_stop(&ctx);
	dst->mode = src->mode;
	dst->cx = src->cx;
	dst->cy = src->cy;
}

static void
window_panes_free_areas(struct window_panes_modedata *data)
{
	free(data->areas);
	data->areas = NULL;
	data->areas_size = 0;
}

static void
window_panes_add_area(struct window_panes_modedata *data,
    struct window_pane *wp, u_int x, u_int y, u_int sx, u_int sy)
{
	struct window_panes_area	*area;

	data->areas = xreallocarray(data->areas, data->areas_size + 1,
	    sizeof *data->areas);
	area = &data->areas[data->areas_size++];
	area->id = wp->id;
	area->x = x;
	area->y = y;
	area->sx = sx;
	area->sy = sy;
}

static int
window_panes_pane_floating(struct window_pane *wp)
{
	struct layout_cell	*lc = wp->saved_layout_cell;

	if (lc == NULL)
		lc = wp->layout_cell;
	if (lc == NULL || (~lc->flags & LAYOUT_CELL_FLOATING))
		return (0);
	return (1);
}

static int
window_panes_pane_visible(struct window_pane *wp)
{
	if (wp->saved_layout_cell != NULL)
		return (1);
	return (window_pane_is_visible(wp));
}

static int
window_panes_get_geometry(struct window_pane *wp, struct layout_cell *root,
    u_int osx, u_int osy, u_int dsx, u_int dsy, u_int *xp, u_int *yp,
    u_int *sxp, u_int *syp)
{
	struct layout_cell	*lc = wp->saved_layout_cell;
	int			 status;
	u_int			 x, y, sx, sy, x2, y2;

	if (lc == NULL)
		lc = wp->layout_cell;
	if (lc == NULL || osx == 0 || osy == 0 || dsx == 0 || dsy == 0)
		return (0);

	if (osx <= dsx && osy <= dsy) {
		x = lc->g.xoff;
		y = lc->g.yoff;
		x2 = x + lc->g.sx;
		y2 = y + lc->g.sy;
	} else {
		x = (u_int)lc->g.xoff * dsx / osx;
		y = (u_int)lc->g.yoff * dsy / osy;
		x2 = ((u_int)lc->g.xoff + lc->g.sx) * dsx / osx;
		y2 = ((u_int)lc->g.yoff + lc->g.sy) * dsy / osy;
	}

	if (x >= dsx || y >= dsy)
		return (0);
	if (x2 <= x)
		x2 = x + 1;
	if (y2 <= y)
		y2 = y + 1;
	if (x2 > dsx)
		x2 = dsx;
	if (y2 > dsy)
		y2 = dsy;

	sx = x2 - x;
	sy = y2 - y;
	if (sx == 0 || sy == 0)
		return (0);

	status = window_get_pane_status(wp->window);
	if (layout_add_horizontal_border(root, lc, status) && sy > 1) {
		if (status == PANE_STATUS_TOP)
			y++;
		sy--;
	}

	*xp = x;
	*yp = y;
	*sxp = sx;
	*syp = sy;
	return (1);
}

static void
window_panes_get_border_cell(struct window_panes_modedata *data,
    struct grid_cell *gc)
{
	struct format_tree	*ft;
	struct window_pane	*wp = data->wp;
	struct session		*s = data->session;

	memcpy(gc, &grid_default_cell, sizeof *gc);
	ft = format_create_defaults(NULL, NULL, s, s->curw, wp);
	style_apply(gc, wp->window->options, "display-panes-border-style", ft);
	format_free(ft);
}

static int
window_panes_map_x(u_int x, u_int osx, u_int dsx)
{
	if (osx <= dsx)
		return (x);
	return (x * dsx / osx);
}

static int
window_panes_map_y(u_int y, u_int osy, u_int dsy)
{
	if (osy <= dsy)
		return (y);
	return (y * dsy / osy);
}

static struct layout_cell *
window_panes_next_tiled_cell(struct layout_cell *lc)
{
	struct layout_cell	*next;

	next = TAILQ_NEXT(lc, entry);
	while (next != NULL) {
		if (~next->flags & LAYOUT_CELL_FLOATING)
			return (next);
		next = TAILQ_NEXT(next, entry);
	}
	return (NULL);
}

static void
window_panes_mark_border(u_char *map, u_int dsx, u_int dsy, u_int x, u_int y,
    u_char mask)
{
	if (x < dsx && y < dsy)
		map[y * dsx + x] |= mask;
}

static void
window_panes_mark_vline(u_char *map, u_int dsx, u_int dsy, int x, int y, int y2)
{
	u_char	mask;
	int	yy;

	if (x < 0 || (u_int)x >= dsx || y2 <= y)
		return;
	if (y < 0)
		y = 0;
	if ((u_int)y2 > dsy)
		y2 = dsy;

	for (yy = y; yy < y2; yy++) {
		mask = 0;
		if (yy > y)
			mask |= WINDOW_PANES_BORDER_U;
		if (yy + 1 < y2)
			mask |= WINDOW_PANES_BORDER_D;
		if (mask == 0)
			mask = WINDOW_PANES_BORDER_U|WINDOW_PANES_BORDER_D;
		window_panes_mark_border(map, dsx, dsy, x, yy, mask);
	}
}

static void
window_panes_mark_hline(u_char *map, u_int dsx, u_int dsy, int x, int x2, int y)
{
	u_char	mask;
	int	xx;

	if (y < 0 || (u_int)y >= dsy || x2 <= x)
		return;
	if (x < 0)
		x = 0;
	if ((u_int)x2 > dsx)
		x2 = dsx;

	for (xx = x; xx < x2; xx++) {
		mask = 0;
		if (xx > x)
			mask |= WINDOW_PANES_BORDER_L;
		if (xx + 1 < x2)
			mask |= WINDOW_PANES_BORDER_R;
		if (mask == 0)
			mask = WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_R;
		window_panes_mark_border(map, dsx, dsy, xx, y, mask);
	}
}

static void
window_panes_mark_borders_cell(u_char *map, struct layout_cell *lc, u_int osx,
    u_int osy, u_int dsx, u_int dsy)
{
	struct layout_cell	*lcchild, *lcnext;
	int			 x, y, x2, y2;

	if (lc->type == LAYOUT_WINDOWPANE)
		return;

	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		window_panes_mark_borders_cell(map, lcchild, osx, osy, dsx,
		    dsy);
		if (lcchild->flags & LAYOUT_CELL_FLOATING)
			continue;
		lcnext = window_panes_next_tiled_cell(lcchild);
		if (lcnext == NULL)
			continue;

		if (lc->type == LAYOUT_LEFTRIGHT) {
			x = window_panes_map_x(lcchild->g.xoff + lcchild->g.sx,
			    osx, dsx);
			y = window_panes_map_y(lc->g.yoff, osy, dsy);
			y2 = window_panes_map_y(lc->g.yoff + lc->g.sy, osy,
			    dsy);
			window_panes_mark_vline(map, dsx, dsy, x, y, y2);
		} else {
			x = window_panes_map_x(lc->g.xoff, osx, dsx);
			x2 = window_panes_map_x(lc->g.xoff + lc->g.sx, osx,
			    dsx);
			y = window_panes_map_y(lcchild->g.yoff + lcchild->g.sy,
			    osy, dsy);
			window_panes_mark_hline(map, dsx, dsy, x, x2, y);
		}
	}
}

static void
window_panes_mark_pane_status_borders(u_char *map, struct window *w,
    struct layout_cell *root, u_int osx, u_int osy, u_int dsx, u_int dsy)
{
	struct window_pane	*wp;
	struct layout_cell	*lc;
	int			 status, x, y, x2, y2;

	status = window_get_pane_status(w);
	if (status != PANE_STATUS_TOP && status != PANE_STATUS_BOTTOM)
		return;

	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (!window_panes_pane_visible(wp))
			continue;

		lc = wp->saved_layout_cell;
		if (lc == NULL)
			lc = wp->layout_cell;
		if (lc == NULL ||
		    !layout_add_horizontal_border(root, lc, status))
			continue;

		x = window_panes_map_x(lc->g.xoff, osx, dsx);
		x2 = window_panes_map_x(lc->g.xoff + lc->g.sx, osx, dsx);
		if (status == PANE_STATUS_TOP)
			y = window_panes_map_y(lc->g.yoff, osy, dsy);
		else {
			y2 = window_panes_map_y(lc->g.yoff + lc->g.sy, osy,
			    dsy);
			y = y2 - 1;
		}
		window_panes_mark_hline(map, dsx, dsy, x, x2, y);
	}
}

static int
window_panes_get_floating_borders(struct window_pane *wp, u_int osx, u_int osy,
    u_int dsx, u_int dsy, int *xp, int *yp, int *x2p, int *y2p)
{
	struct layout_cell	*lc;

	lc = wp->saved_layout_cell;
	if (lc == NULL)
		lc = wp->layout_cell;
	if (lc == NULL || (~lc->flags & LAYOUT_CELL_FLOATING))
		return (0);

	if (lc->g.xoff == 0)
		*xp = -1;
	else
		*xp = window_panes_map_x(lc->g.xoff - 1, osx, dsx);
	if (lc->g.yoff == 0)
		*yp = -1;
	else
		*yp = window_panes_map_y(lc->g.yoff - 1, osy, dsy);
	*x2p = window_panes_map_x(lc->g.xoff + lc->g.sx, osx, dsx);
	*y2p = window_panes_map_y(lc->g.yoff + lc->g.sy, osy, dsy);
	return (1);
}

static int
window_panes_clip_floating_pane(struct window_pane *wp, u_int osx, u_int osy,
    u_int dsx, u_int dsy, u_int *xp, u_int *yp, u_int *sxp, u_int *syp)
{
	int	x, y, x2, y2, bx, by, bx2, by2;

	if (!window_panes_get_floating_borders(wp, osx, osy, dsx, dsy, &x, &y,
	    &x2, &y2))
		return (1);

	bx = *xp;
	by = *yp;
	bx2 = bx + *sxp - 1;
	by2 = by + *syp - 1;

	if (x >= 0 && bx <= x)
		bx = x + 1;
	if (y >= 0 && by <= y)
		by = y + 1;
	if ((u_int)x2 < dsx && bx2 >= x2)
		bx2 = x2 - 1;
	if ((u_int)y2 < dsy && by2 >= y2)
		by2 = y2 - 1;
	if (bx2 < bx || by2 < by)
		return (0);

	*xp = bx;
	*yp = by;
	*sxp = bx2 - bx + 1;
	*syp = by2 - by + 1;
	return (1);
}

static int
window_panes_border_cell_type(u_char mask)
{
	switch (mask) {
	case WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_R|
	    WINDOW_PANES_BORDER_U|WINDOW_PANES_BORDER_D:
		return (CELL_LRUD);
	case WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_R|WINDOW_PANES_BORDER_U:
		return (CELL_LRU);
	case WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_R|WINDOW_PANES_BORDER_D:
		return (CELL_LRD);
	case WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_R:
	case WINDOW_PANES_BORDER_L:
	case WINDOW_PANES_BORDER_R:
		return (CELL_LR);
	case WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_U|WINDOW_PANES_BORDER_D:
		return (CELL_ULD);
	case WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_U:
		return (CELL_LU);
	case WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_D:
		return (CELL_LD);
	case WINDOW_PANES_BORDER_R|WINDOW_PANES_BORDER_U|WINDOW_PANES_BORDER_D:
		return (CELL_URD);
	case WINDOW_PANES_BORDER_R|WINDOW_PANES_BORDER_U:
		return (CELL_RU);
	case WINDOW_PANES_BORDER_R|WINDOW_PANES_BORDER_D:
		return (CELL_RD);
	case WINDOW_PANES_BORDER_U|WINDOW_PANES_BORDER_D:
	case WINDOW_PANES_BORDER_U:
	case WINDOW_PANES_BORDER_D:
		return (CELL_UD);
	}
	return (CELL_NONE);
}

static int
window_panes_border_has_horizontal(u_char mask)
{
	return ((mask & (WINDOW_PANES_BORDER_L|WINDOW_PANES_BORDER_R)) != 0);
}

static int
window_panes_border_has_vertical(u_char mask)
{
	return ((mask & (WINDOW_PANES_BORDER_U|WINDOW_PANES_BORDER_D)) != 0);
}

static void
window_panes_mark_border_joins_cell(u_char *map, struct layout_cell *lc,
    u_int osx, u_int osy, u_int dsx, u_int dsy)
{
	struct layout_cell	*lcchild, *lcnext;
	int			 x, y, x2, y2;

	if (lc->type == LAYOUT_WINDOWPANE)
		return;

	TAILQ_FOREACH(lcchild, &lc->cells, entry) {
		window_panes_mark_border_joins_cell(map, lcchild, osx, osy,
		    dsx, dsy);
		if (lcchild->flags & LAYOUT_CELL_FLOATING)
			continue;
		lcnext = window_panes_next_tiled_cell(lcchild);
		if (lcnext == NULL)
			continue;

		if (lc->type == LAYOUT_LEFTRIGHT) {
			x = window_panes_map_x(lcchild->g.xoff + lcchild->g.sx,
			    osx, dsx);
			y = window_panes_map_y(lc->g.yoff, osy, dsy);
			y2 = window_panes_map_y(lc->g.yoff + lc->g.sy, osy,
			    dsy);
			if (x < 0 || (u_int)x >= dsx)
				continue;
			if (y > 0 &&
			    window_panes_border_has_horizontal(
			    map[(y - 1) * dsx + x])) {
				window_panes_mark_border(map, dsx, dsy, x,
				    y - 1, WINDOW_PANES_BORDER_D);
				window_panes_mark_border(map, dsx, dsy, x, y,
				    WINDOW_PANES_BORDER_U);
			}
			if ((u_int)y2 < dsy &&
			    window_panes_border_has_horizontal(
			    map[y2 * dsx + x])) {
				window_panes_mark_border(map, dsx, dsy, x, y2,
				    WINDOW_PANES_BORDER_U);
				window_panes_mark_border(map, dsx, dsy, x,
				    y2 - 1, WINDOW_PANES_BORDER_D);
			}
		} else {
			x = window_panes_map_x(lc->g.xoff, osx, dsx);
			x2 = window_panes_map_x(lc->g.xoff + lc->g.sx, osx,
			    dsx);
			y = window_panes_map_y(lcchild->g.yoff + lcchild->g.sy,
			    osy, dsy);
			if (y < 0 || (u_int)y >= dsy)
				continue;
			if (x > 0 &&
			    window_panes_border_has_vertical(
			    map[y * dsx + x - 1])) {
				window_panes_mark_border(map, dsx, dsy, x - 1,
				    y, WINDOW_PANES_BORDER_R);
				window_panes_mark_border(map, dsx, dsy, x, y,
				    WINDOW_PANES_BORDER_L);
			}
			if ((u_int)x2 < dsx &&
			    window_panes_border_has_vertical(
			    map[y * dsx + x2])) {
				window_panes_mark_border(map, dsx, dsy, x2, y,
				    WINDOW_PANES_BORDER_L);
				window_panes_mark_border(map, dsx, dsy, x2 - 1,
				    y, WINDOW_PANES_BORDER_R);
			}
		}
	}
}

static void
window_panes_draw_borders(struct screen_write_ctx *ctx, struct window *w,
    struct layout_cell *lc, const struct grid_cell *gc, u_int osx, u_int osy,
    u_int dsx, u_int dsy)
{
	struct grid_cell	 border_gc;
	u_char			*map;
	u_int			 xx, yy;
	int			 cell_type;

	if (dsx == 0 || dsy == 0)
		return;

	map = xcalloc(dsx, dsy);
	window_panes_mark_borders_cell(map, lc, osx, osy, dsx, dsy);
	window_panes_mark_pane_status_borders(map, w, lc, osx, osy, dsx,
	    dsy);
	window_panes_mark_border_joins_cell(map, lc, osx, osy, dsx, dsy);

	for (yy = 0; yy < dsy; yy++) {
		for (xx = 0; xx < dsx; xx++) {
			if (map[yy * dsx + xx] == 0)
				continue;
			cell_type = window_panes_border_cell_type(
			    map[yy * dsx + xx]);
			memcpy(&border_gc, gc, sizeof border_gc);
			border_gc.attr |= GRID_ATTR_CHARSET;
			utf8_set(&border_gc.data, CELL_BORDERS[cell_type]);
			screen_write_cursormove(ctx, xx, yy, 0);
			screen_write_cell(ctx, &border_gc);
		}
	}
	free(map);
}

static void
window_panes_draw_floating_border(struct screen_write_ctx *ctx,
    struct window_pane *wp, const struct grid_cell *gc, u_int osx, u_int osy,
    u_int dsx, u_int dsy)
{
	struct grid_cell	 border_gc;
	u_char			*map;
	u_int			 xx, yy;
	int			 x, y, x2, y2, cell_type;

	if (dsx == 0 || dsy == 0)
		return;
	if (!window_panes_get_floating_borders(wp, osx, osy, dsx, dsy, &x, &y,
	    &x2, &y2))
		return;

	map = xcalloc(dsx, dsy);
	window_panes_mark_hline(map, dsx, dsy, x, x2 + 1, y);
	window_panes_mark_hline(map, dsx, dsy, x, x2 + 1, y2);
	window_panes_mark_vline(map, dsx, dsy, x, y, y2 + 1);
	window_panes_mark_vline(map, dsx, dsy, x2, y, y2 + 1);

	for (yy = 0; yy < dsy; yy++) {
		for (xx = 0; xx < dsx; xx++) {
			if (map[yy * dsx + xx] == 0)
				continue;
			cell_type = window_panes_border_cell_type(
			    map[yy * dsx + xx]);
			memcpy(&border_gc, gc, sizeof border_gc);
			border_gc.attr |= GRID_ATTR_CHARSET;
			utf8_set(&border_gc.data, CELL_BORDERS[cell_type]);
			screen_write_cursormove(ctx, xx, yy, 0);
			screen_write_cell(ctx, &border_gc);
		}
	}
	free(map);
}

static void
window_panes_clear_floating_area(struct screen_write_ctx *ctx,
    struct window_pane *wp, u_int osx, u_int osy, u_int dsx, u_int dsy)
{
	struct grid_cell	gc;
	int			x, y, x2, y2, xx, yy;

	if (!window_panes_get_floating_borders(wp, osx, osy, dsx, dsy, &x, &y,
	    &x2, &y2))
		return;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	if (x < 0)
		x = 0;
	if (y < 0)
		y = 0;
	if ((u_int)x2 >= dsx)
		x2 = dsx - 1;
	if ((u_int)y2 >= dsy)
		y2 = dsy - 1;
	if (x2 < x || y2 < y)
		return;

	for (yy = y; yy <= y2; yy++) {
		screen_write_cursormove(ctx, x, yy, 0);
		for (xx = x; xx <= x2; xx++)
			screen_write_putc(ctx, &gc, ' ');
	}
}

static void
window_panes_draw_format(struct window_panes_modedata *data,
    struct screen_write_ctx *ctx, struct window_pane *wp, u_int x, u_int y,
    u_int sx, const struct grid_cell *gc)
{
	struct session	*s = data->session;
	struct winlink	*wl = s->curw;
	const char	*format;
	char		*expanded;

	if (sx == 0)
		return;
	format = options_get_string(data->wp->window->options,
	    "display-panes-format");
	if (*format == '\0')
		return;

	window_panes_get_source(data, &s, &wl, NULL);
	if (s == NULL)
		return;

	expanded = format_single(NULL, format, NULL, s, wl, wp);
	if (*expanded != '\0') {
		screen_write_cursormove(ctx, x, y, 0);
		format_draw(ctx, gc, sx, expanded, NULL, 0);
	}
	free(expanded);
}

static void
window_panes_draw_number(struct window_panes_modedata *data,
    struct screen_write_ctx *ctx, struct window_pane *wp, u_int pane, u_int x,
    u_int y, u_int sx, u_int sy)
{
	struct session		*s = data->session;
	struct window		*w = wp->window;
	struct winlink		*wl = s->curw;
	struct options		*oo = data->wp->window->options;
	struct grid_cell	 fgc, bgc;
	struct format_tree	*ft;
	const char		*name;
	char			 buf[16], lbuf[16] = { 0 }, *ptr;
	size_t			 len, llen = 0, width;
	u_int			 cx, cy, px, py, idx, i, j, format;

	len = xsnprintf(buf, sizeof buf, "%u", pane);
	if (pane > 9 && pane < 35)
		llen = xsnprintf(lbuf, sizeof lbuf, "%c", 'a' + (pane - 10));
	if (sx < len)
		return;

	window_panes_get_source(data, &s, &wl, NULL);
	if (s != NULL) {
		if (wl == NULL)
			wl = s->curw;
	}
	if (w->active == wp)
		name = "display-panes-active-colour";
	else
		name = "display-panes-colour";
	ft = format_create_defaults(NULL, NULL, s, wl, wp);
	style_apply(&fgc, oo, name, ft);
	format_free(ft);

	memcpy(&bgc, &grid_default_cell, sizeof bgc);
	bgc.bg = fgc.fg;

	format = 0;
	if (options_get_string(oo, "display-panes-format")[0] != '\0')
		format = 1;
	width = (len * 6) - 1;
	if (sx < width || sy < (format ? 7 : 5)) {
		width = len;
		if (llen != 0 && sx >= len + llen + 1)
			width += llen + 1;
		cx = x + (sx - width) / 2;
		cy = y + sy / 2;
		screen_write_cursormove(ctx, cx, cy, 0);
		screen_write_puts(ctx, &fgc, "%s", buf);
		if (width > len)
			screen_write_puts(ctx, &fgc, " %s", lbuf);
		if (format && sy > 1)
			window_panes_draw_format(data, ctx, wp, x, y, sx, &fgc);
		return;
	}

	px = (sx - width) / 2;
	py = (sy - 5) / 2;
	for (ptr = buf; *ptr != '\0'; ptr++) {
		if (*ptr < '0' || *ptr > '9')
			continue;
		idx = *ptr - '0';

		for (j = 0; j < 5; j++) {
			for (i = 0; i < 5; i++) {
				if (!window_clock_table[idx][j][i])
					continue;
				screen_write_cursormove(ctx, x + px + i,
				    y + py + j, 0);
				screen_write_putc(ctx, &bgc, ' ');
			}
		}
		px += 6;
	}

	if (sy <= 6)
		return;
	window_panes_draw_format(data, ctx, wp, x, y, sx, &fgc);
	if (llen != 0) {
		cx = x + px - llen - 1;
		cy = y + py + 5;
		screen_write_cursormove(ctx, cx, cy, 0);
		screen_write_puts(ctx, &fgc, "%s", lbuf);
	}
}

static void
window_panes_draw_pane(struct window_panes_modedata *data,
    struct screen_write_ctx *ctx, struct window_pane *wp,
    struct layout_cell *root, u_int osx, u_int osy, u_int dsx, u_int dsy)
{
	struct screen		*s = &wp->base;
	u_int			 pane, x, y, sx, sy;

	if (!window_panes_pane_visible(wp))
		return;
	if (!window_panes_get_geometry(wp, root, osx, osy, dsx, dsy, &x, &y,
	    &sx, &sy))
		return;
	if (!window_panes_clip_floating_pane(wp, osx, osy, dsx, dsy, &x, &y,
	    &sx, &sy))
		return;
	if (window_pane_index(wp, &pane) != 0)
		return;
	window_panes_add_area(data, wp, x, y, sx, sy);

	screen_write_cursormove(ctx, x, y, 0);
	if (data->preview != NULL &&
	    wp == data->wp &&
	    sx <= screen_size_x(data->preview) &&
	    sy <= screen_size_y(data->preview))
		s = data->preview;
	if (osx <= dsx && osy <= dsy)
		screen_write_fast_copy(ctx, s, 0, s->grid->hsize, sx, sy);
	else
		screen_write_preview(ctx, s, sx, sy);
	window_panes_draw_number(data, ctx, wp, pane, x, y, sx, sy);
}

static void
window_panes_draw_screen(struct window_mode_entry *wme)
{
	struct window_panes_modedata	*data = wme->data;
	struct window			*w;
	struct window_pane		*wp;
	struct screen_write_ctx		 ctx;
	struct layout_cell		*root;
	struct grid_cell		 border_gc;
	u_int				 osx, osy, sx, sy;

	if (!window_panes_get_source(data, NULL, NULL, &w))
		return;
	root = w->saved_layout_root;
	if (root == NULL)
		root = w->layout_root;
	if (root == NULL)
		return;

	osx = root->g.sx;
	osy = root->g.sy;
	sx = screen_size_x(&data->screen);
	sy = screen_size_y(&data->screen);

	window_panes_free_areas(data);
	screen_write_start(&ctx, &data->screen);
	screen_write_clearscreen(&ctx, 8);
	TAILQ_FOREACH(wp, &w->panes, entry) {
		if (window_panes_pane_floating(wp))
			continue;
		window_panes_draw_pane(data, &ctx, wp, root, osx, osy, sx, sy);
	}
	window_panes_get_border_cell(data, &border_gc);
	window_panes_draw_borders(&ctx, w, root, &border_gc, osx, osy, sx, sy);
	TAILQ_FOREACH_REVERSE(wp, &w->z_index, window_panes_zindex, zentry) {
		if (!window_panes_pane_floating(wp))
			continue;
		window_panes_clear_floating_area(&ctx, wp, osx, osy, sx, sy);
		window_panes_draw_pane(data, &ctx, wp, root, osx, osy, sx, sy);
		window_panes_draw_floating_border(&ctx, wp, &border_gc, osx,
		    osy, sx, sy);
	}
	screen_write_stop(&ctx);

	data->wp->flags |= PANE_REDRAW;
}

static void
window_panes_timer_callback(__unused int fd, __unused short events, void *arg)
{
	struct window_mode_entry	*wme = arg;

	window_pane_reset_mode(wme->wp);
}

static struct screen *
window_panes_init(struct window_mode_entry *wme, struct cmdq_item *item,
    __unused struct cmd_find_state *fs, struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window			*w = wp->window;
	struct window_panes_modedata	*data;
	struct cmd			*self;
	struct cmd_find_state		*source, *target;
	struct session			*s;
	struct timeval			 tv;
	char				*cause;
	u_int				 sx = screen_size_x(&wp->base);
	u_int				 sy = screen_size_y(&wp->base);
	u_int				 delay;

	if (item == NULL)
		return (NULL);
	self = cmdq_get_cmd(item);
	if (self == NULL)
		return (NULL);

	source = cmdq_get_source(item);
	target = cmdq_get_target(item);
	s = target->s;

	if (!args_has(args, 'd'))
		delay = options_get_number(w->options, "display-panes-time");
	else {
		delay = args_strtonum(args, 'd', 0, UINT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "delay %s", cause);
			free(cause);
			return (NULL);
		}
	}

	wme->data = data = xcalloc(1, sizeof *data);
	data->wp = wp;
	data->session = s;

	screen_init(&data->screen, sx, sy, 0);
	data->screen.mode &= ~MODE_CURSOR;

	data->state = args_make_commands_prepare(self, item, 0,
	    "select-pane -t \"%%%\"", 0, 0);
	if (args_has(args, 's')) {
		data->source_session = source->s->id;
		data->source_window = source->w->id;
	} else {
		data->source_session = target->s->id;
		data->source_window = target->w->id;
	}

	data->delay = delay;
	data->ignore_keys = args_has(args, 'N');

	if (args_has(args, 'Z'))
		data->zoomed = -1;
	else {
		data->zoomed = (w->flags & WINDOW_ZOOMED);
		if (!data->zoomed)
			window_panes_set_preview(data);
		if (!data->zoomed && window_zoom(wp) == 0)
			server_redraw_window(w);
	}

	evtimer_set(&data->timer, window_panes_timer_callback, wme);
	if (data->delay != 0) {
		tv.tv_sec = data->delay / 1000;
		tv.tv_usec = (data->delay % 1000) * 1000;
		evtimer_add(&data->timer, &tv);
	}

	window_panes_draw_screen(wme);
	return (&data->screen);
}

static void
window_panes_free(struct window_mode_entry *wme)
{
	struct window_panes_modedata	*data = wme->data;
	struct window			*w = wme->wp->window;

	evtimer_del(&data->timer);

	if (data->zoomed == 0)
		server_unzoom_window(w);
	server_redraw_window(w);
	server_redraw_window_borders(w);
	server_status_window(w);

	if (data->state != NULL)
		args_make_commands_free(data->state);
	window_panes_free_areas(data);
	if (data->preview != NULL) {
		screen_free(data->preview);
		free(data->preview);
	}
	screen_free(&data->screen);
	free(data);
}

static void
window_panes_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_panes_modedata	*data = wme->data;

	screen_resize(&data->screen, sx, sy, 0);
	window_panes_draw_screen(wme);
}

static void
window_panes_run_command(struct window_panes_modedata *data, struct client *c,
	struct window_pane *wp)
{
	struct cmdq_item	*new_item;
	struct cmd_list		*cmdlist;
	char			*expanded, *error;

	xasprintf(&expanded, "%%%u", wp->id);
	cmdlist = args_make_commands(data->state, 1, &expanded, &error);
	if (cmdlist == NULL) {
		cmdq_append(c, cmdq_get_error(error));
		free(error);
	} else {
		new_item = cmdq_get_command(cmdlist, NULL);
		cmdq_append(c, new_item);
	}
	free(expanded);
}

static struct window_pane *
window_panes_find_pane(struct window_panes_modedata *data, u_int x, u_int y)
{
	struct window_panes_area	*area;
	u_int			 i;

	for (i = data->areas_size; i > 0; i--) {
		area = &data->areas[i - 1];
		if (x < area->x || x >= area->x + area->sx)
			continue;
		if (y < area->y || y >= area->y + area->sy)
			continue;
		return (window_pane_find_by_id(area->id));
	}
	return (NULL);
}

static struct window_pane *
window_panes_key_pane(struct window_panes_modedata *data, key_code key)
{
	struct window		*w;
	u_int			 index;

	if (key >= '0' && key <= '9')
		index = key - '0';
	else if ((key & KEYC_MASK_MODIFIERS) == 0) {
		key &= KEYC_MASK_KEY;
		if (key < 'a' || key > 'z')
			return (NULL);
		index = 10 + (key - 'a');
	} else
		return (NULL);
	if (!window_panes_get_source(data, NULL, NULL, &w))
		return (NULL);
	return (window_pane_at_index(w, index));
}

static struct window_pane *
window_panes_get_target(struct window_mode_entry *wme, key_code key,
    struct mouse_event *m)
{
	struct window_panes_modedata	*data = wme->data;
	u_int				 x, y;

	if (data->ignore_keys)
		return (NULL);

	if (KEYC_IS_MOUSE(key)) {
		if (key != KEYC_MOUSEDOWN1_PANE ||
		    m == NULL ||
		    cmd_mouse_at(wme->wp, m, &x, &y, 0) != 0)
			return (NULL);
		return (window_panes_find_pane(data, x, y));
	}

	return (window_panes_key_pane(data, key));
}

static void
window_panes_key(struct window_mode_entry *wme, struct client *c,
    __unused struct session *s, __unused struct winlink *wl, key_code key,
    struct mouse_event *m)
{
	struct window_pane		*wp = wme->wp, *target = NULL;
	struct window_panes_modedata	*data = wme->data;

	if (key == '\033' || key == 'q') {
		window_pane_reset_mode(wp);
		return;
	}

	target = window_panes_get_target(wme, key, m);
	if (target == NULL) {
		if (!data->ignore_keys && !KEYC_IS_MOUSE(key))
			window_pane_reset_mode(wp);
		return;
	}

	if (wp->window->flags & WINDOW_ZOOMED)
		window_unzoom(wp->window, 1);
	window_panes_run_command(data, c, target);
	window_pane_reset_mode(wp);
}
