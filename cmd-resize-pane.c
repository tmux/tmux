/* $OpenBSD: cmd-resize-pane.c,v 1.67 2026/07/10 13:38:45 nicm Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Increase or decrease pane size.
 */

static enum cmd_retval	cmd_resize_pane_exec(struct cmd *, struct cmdq_item *);

static enum cmd_retval	cmd_resize_pane_mouse_update(struct cmd *,
			    struct cmdq_item *);
static void		cmd_resize_pane_mouse_resize_move_floating(
			    struct client *, struct mouse_event *);
static void		cmd_resize_pane_mouse_resize_tiled(struct client *,
			    struct mouse_event *);

const struct cmd_entry cmd_resize_pane_entry = {
	.name = "resize-pane",
	.alias = "resizep",

	.args = { "D::L::MR::Tt:U::x:y:Z", 0, 1, NULL },
	.usage = "[-MTZ] [-D lines] [-L columns] [-R columns] [-U lines] "
		 "[-x width] [-y height] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_resize_pane_exec
};

static enum cmd_retval
cmd_resize_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct window_pane	*wp = target->wp;
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct layout_cell	*lc = wp->layout_cell;
	enum layout_type	 type;
	const char		*errstr, *argval;
	const char		 flags[4] = { 'U', 'D', 'L', 'R' };
	char			*cause = NULL, flag;
	int			 adjust, x, y, status, opposite = 0;
	long unsigned		 i;
	struct grid		*gd = wp->base.grid;

	if (args_has(args, 'T')) {
		if (!TAILQ_EMPTY(&wp->modes))
			return (CMD_RETURN_NORMAL);
		adjust = screen_size_y(&wp->base) - 1 - wp->base.cy;
		if (adjust > (int)gd->hsize)
			adjust = gd->hsize;
		grid_remove_history(gd, adjust);
		wp->base.cy += adjust;
		wp->flags |= PANE_REDRAW;
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'M'))
		return (cmd_resize_pane_mouse_update(self, item));

	if (args_has(args, 'Z')) {
		if (w->flags & WINDOW_ZOOMED)
			window_unzoom(w, 1);
		else
			window_zoom(wp);
		server_redraw_window(w);
		return (CMD_RETURN_NORMAL);
	}
	server_unzoom_window(w);

	if (args_has(args, 'x')) {
		x = args_percentage(args, 'x', 0, PANE_MAXIMUM, w->sx, &cause);
		if (cause != NULL) {
			cmdq_error(item, "width %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (window_pane_is_floating(wp)) {
			if (layout_resize_floating_pane_to(wp, LAYOUT_LEFTRIGHT,
			    x, &cause) != 0) {
				cmdq_error(item, "size %s", cause);
				free(cause);
				return (CMD_RETURN_ERROR);
			}
		} else
			layout_resize_pane_to(wp, LAYOUT_LEFTRIGHT, x);
	}
	if (args_has(args, 'y')) {
		y = args_percentage(args, 'y', 0, PANE_MAXIMUM, w->sy, &cause);
		if (cause != NULL) {
			cmdq_error(item, "height %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		status = window_get_pane_status(w);
		switch (status) {
		case PANE_STATUS_TOP:
			if (y != INT_MAX && wp->yoff == 1)
				y++;
			break;
		case PANE_STATUS_BOTTOM:
			if (y != INT_MAX && wp->yoff + wp->sy == w->sy - 1)
				y++;
			break;
		}
		if (window_pane_is_floating(wp)) {
			if (layout_resize_floating_pane_to(wp, LAYOUT_TOPBOTTOM,
			    y, &cause) != 0) {
				cmdq_error(item, "size %s", cause);
				free(cause);
				return (CMD_RETURN_ERROR);
			}
		} else
			layout_resize_pane_to(wp, LAYOUT_TOPBOTTOM, y);
	}

	for (i = 0; i < nitems(flags); i++) {
		flag = flags[i];
		if (!args_has(args, flag))
			continue;

		argval = args_get(args, flag);
		if (argval == NULL) {
			if (args_count(args) == 0)
				argval = "1";
			else
				argval = args_string(args, 0);
		}

		adjust = strtonum(argval, INT_MIN, INT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(item, "adjustment %s", errstr);
			return (CMD_RETURN_ERROR);
		}

		type = LAYOUT_TOPBOTTOM;
		if (flag == 'L' || flag == 'R')
			type = LAYOUT_LEFTRIGHT;

		if (window_pane_is_floating(wp)) {
			if (flag == 'L' || flag == 'U')
				opposite = 1;

			if (layout_resize_floating_pane(wp, type, adjust,
			    opposite, &cause) != 0) {
				cmdq_error(item, "adjustment %s", cause);
				free(cause);
				return (CMD_RETURN_ERROR);
			}
		} else {
			if (flag == 'L' || flag == 'U')
				adjust = -adjust;
			layout_resize_pane(wp, type, adjust, 1);
		}
	}

	if (lc->parent != NULL)
		layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_resize_pane_mouse_update(__unused struct cmd *self, struct cmdq_item *item)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct key_event	*event = cmdq_get_event(item);
	struct window_pane	*wp = target->wp;
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct client		*c = cmdq_get_client(item);
	struct session		*s = target->s;

	if (!event->m.valid)
		return (CMD_RETURN_NORMAL);
	wp = cmd_mouse_pane(&event->m, &s, NULL);
	if (wp == NULL || c == NULL || c->session != s)
		return (CMD_RETURN_NORMAL);

	if (!window_pane_is_floating(wp)) {
		c->tty.mouse_drag_update = cmd_resize_pane_mouse_resize_tiled;
		cmd_resize_pane_mouse_resize_tiled(c, &event->m);
		return (CMD_RETURN_NORMAL);
	}

	window_redraw_active_switch(w, wp);
	window_set_active_pane(w, wp, 1);

	c->tty.mouse_drag_update = cmd_resize_pane_mouse_resize_move_floating;
	cmd_resize_pane_mouse_resize_move_floating(c, &event->m);
	return (CMD_RETURN_NORMAL);
}

/*
 * Resizes or moves the pane by dragging. Resize a floating pane by dragging
 * the borders or corners. Grabbing an edge only resizes that axis (special
 * case). Moves the pane if dragging the top border. Since characters are
 * generally rectangular, to make it easier to grab the corner, the character
 * next to the corner is also considered the corner.
 */
static void
cmd_resize_pane_mouse_resize_move_floating(struct client *c,
    struct mouse_event *m)
{
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	struct layout_cell	*lc;
	int			 y, ly, x, lx, sx, sy, new_sx, new_sy;
	int			 left, right;
	int			 new_xoff, new_yoff, resizes = 0;

	wp = cmd_mouse_pane(m, NULL, &wl);
	if (wp == NULL) {
		c->tty.mouse_drag_update = NULL;
		return;
	}
	w = wl->window;
	lc = wp->layout_cell;
	sx = wp->sx;
	sy = wp->sy;
	left = wp->xoff - 1;
	right = wp->xoff + sx;
	if (window_pane_scrollbar_reserve(wp) &&
	    w->sb_pos == PANE_SCROLLBARS_LEFT) {
		left -= wp->scrollbar_style.width + wp->scrollbar_style.pad;
	} else if (window_pane_scrollbar_reserve(wp) &&
	    w->sb_pos == PANE_SCROLLBARS_RIGHT) {
		right += wp->scrollbar_style.width + wp->scrollbar_style.pad;
	}

	y = m->y + m->oy; x = m->x + m->ox;
	if (m->statusat == 0 && y >= (int)m->statuslines)
		y -= m->statuslines;
	else if (m->statusat > 0 && y >= m->statusat)
		y = m->statusat - 1;
	ly = m->ly + m->oy; lx = m->lx + m->ox;
	if (m->statusat == 0 && ly >= (int)m->statuslines)
		ly -= m->statuslines;
	else if (m->statusat > 0 && ly >= m->statusat)
		ly = m->statusat - 1;

	if ((lx == left || lx == left + 1) && ly == wp->yoff - 1) {
		/* Top left corner. */
		new_sx = lc->g.sx + (lx - x);
		if (new_sx < PANE_MINIMUM)
			new_sx = PANE_MINIMUM;
		new_sy = lc->g.sy + (ly - y);
		if (new_sy < PANE_MINIMUM)
			new_sy = PANE_MINIMUM;
		new_xoff = x + 1; /* because mouse is on border at xoff - 1 */
		new_yoff = y + 1;
		layout_set_size(lc, new_sx, new_sy, new_xoff, new_yoff);
		resizes++;
	} else if ((lx == right + 1 || lx == right) &&
	    ly == wp->yoff - 1) {
		/* Top right corner. */
		new_sx = x - lc->g.xoff;
		if (new_sx < PANE_MINIMUM)
			new_sx = PANE_MINIMUM;
		new_sy = lc->g.sy + (ly - y);
		if (new_sy < PANE_MINIMUM)
			new_sy = PANE_MINIMUM;
		new_yoff = y + 1;
		layout_set_size(lc, new_sx, new_sy, lc->g.xoff, new_yoff);
		resizes++;
	} else if ((lx == left || lx == left + 1) &&
	    ly == wp->yoff + sy) {
		/* Bottom left corner. */
		new_sx = lc->g.sx + (lx - x);
		if (new_sx < PANE_MINIMUM)
			new_sx = PANE_MINIMUM;
		new_sy = y - lc->g.yoff;
		if (new_sy < PANE_MINIMUM)
			return;
		new_xoff = x + 1;
		layout_set_size(lc, new_sx, new_sy, new_xoff, lc->g.yoff);
		resizes++;
	} else if ((lx == right + 1 || lx == right) &&
	    ly == wp->yoff + sy) {
		/* Bottom right corner. */
		new_sx = x - lc->g.xoff;
		if (new_sx < PANE_MINIMUM)
			new_sx = PANE_MINIMUM;
		new_sy = y - lc->g.yoff;
		if (new_sy < PANE_MINIMUM)
			new_sy = PANE_MINIMUM;
		layout_set_size(lc, new_sx, new_sy, lc->g.xoff, lc->g.yoff);
		resizes++;
	} else if (lx == right) {
		/* Right border. */
		new_sx = x - lc->g.xoff;
		if (new_sx < PANE_MINIMUM)
			return;
		layout_set_size(lc, new_sx, lc->g.sy, lc->g.xoff, lc->g.yoff);
		resizes++;
	} else if (lx == left) {
		/* Left border. */
		new_sx = lc->g.sx + (lx - x);
		if (new_sx < PANE_MINIMUM)
			return;
		new_xoff = x + 1;
		layout_set_size(lc, new_sx, lc->g.sy, new_xoff, lc->g.yoff);
		resizes++;
	} else if (ly == wp->yoff + sy) {
		/* Bottom border. */
		new_sy = y - lc->g.yoff;
		if (new_sy < PANE_MINIMUM)
			return;
		layout_set_size(lc, lc->g.sx, new_sy, lc->g.xoff, lc->g.yoff);
		resizes++;
	} else if (ly == wp->yoff - 1) {
		/* Top border (move instead of resize). */
		new_xoff = lc->g.xoff + (x - lx);
		new_yoff = y + 1;
		layout_set_size(lc, lc->g.sx, lc->g.sy, new_xoff, new_yoff);
		resizes++;
	}
	if (resizes != 0) {
		layout_fix_panes(w, NULL);
		server_redraw_window(w);
		server_redraw_window_borders(w);
	}
}

static void
cmd_resize_pane_mouse_resize_tiled(struct client *c, struct mouse_event *m)
{
	struct winlink		*wl;
	struct window		*w;
	u_int			 y, ly, x, lx;
	static const int	 offsets[][2] = {
	    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 },
	};
	struct layout_cell	*cells[nitems(offsets)], *lc;
	u_int			 ncells = 0, i, j, resizes = 0;
	enum layout_type	 type;

	wl = cmd_mouse_window(m, NULL);
	if (wl == NULL) {
		c->tty.mouse_drag_update = NULL;
		return;
	}
	w = wl->window;

	y = m->y + m->oy; x = m->x + m->ox;
	if (m->statusat == 0 && y >= m->statuslines)
		y -= m->statuslines;
	else if (m->statusat > 0 && y >= (u_int)m->statusat)
		y = m->statusat - 1;
	ly = m->ly + m->oy; lx = m->lx + m->ox;
	if (m->statusat == 0 && ly >= m->statuslines)
		ly -= m->statuslines;
	else if (m->statusat > 0 && ly >= (u_int)m->statusat)
		ly = m->statusat - 1;

	for (i = 0; i < nitems(cells); i++) {
		lc = layout_search_by_border(w->layout_root, lx + offsets[i][0],
		    ly + offsets[i][1]);
		if (lc == NULL)
			continue;

		for (j = 0; j < ncells; j++) {
			if (cells[j] == lc) {
				lc = NULL;
				break;
			}
		}
		if (lc == NULL)
			continue;

		cells[ncells] = lc;
		ncells++;
	}
	if (ncells == 0)
		return;

	for (i = 0; i < ncells; i++) {
		type = cells[i]->parent->type;
		if (y != ly && type == LAYOUT_TOPBOTTOM) {
			layout_resize_layout(w, cells[i], type, y - ly, 0);
			resizes++;
		} else if (x != lx && type == LAYOUT_LEFTRIGHT) {
			layout_resize_layout(w, cells[i], type, x - lx, 0);
			resizes++;
		}
	}
	if (resizes != 0)
		server_redraw_window(w);
}
