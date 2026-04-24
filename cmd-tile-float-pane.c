/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Michael Grant <mgrant@grant.org>
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

/*
 * float-pane: lift a tiled pane out of the layout tree into a floating pane.
 * tile-pane: insert a floating pane back into the tiled layout.
 *
 * saved_layout_cell is reused to remember the pane's tiled slot while it is
 * floating, using the same mechanism as minimise-pane.  The cell's wp pointer
 * is cleared while the pane is floating so that layout helpers treat the slot
 * as empty.
 */

static enum cmd_retval	cmd_float_pane_exec(struct cmd *, struct cmdq_item *);
static enum cmd_retval	cmd_tile_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_float_pane_entry = {
	.name = "float-pane",
	.alias = NULL,

	.args = { "t:x:X:y:Y:", 0, 0, NULL },
	.usage = "[-x width] [-X x-position] [-y height] [-Y y-position] "
		 CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_float_pane_exec
};

const struct cmd_entry cmd_tile_pane_entry = {
	.name = "tile-pane",
	.alias = NULL,

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_tile_pane_exec
};

/*
 * Parse geometry arguments for float-pane.
 * Returns 0 on success, -1 on error (error message already set on item).
 * x/y/sx/sy are set to parsed values or cascade defaults.
 */
static int
cmd_float_pane_parse_geometry(struct window *w, int *out_x, int *out_y,
    u_int *out_sx, u_int *out_sy, struct cmdq_item *item, struct args *args)
{
	char	*cause = NULL;

	if (window_pane_float_geometry(w, out_x, out_y, out_sx, out_sy, item,
	    args, &cause) != 0) {
		cmdq_error(item, "invalid float geometry %s", cause);
		free(cause);
		return (-1);
	}

	return (0);
}

static enum cmd_retval
cmd_float_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct window		*w = target->wl->window;
	struct window_pane	*wp = target->wp;
	int			 x, y;
	u_int			 sx, sy;
	struct layout_cell	*lc;

	if (wp->flags & PANE_FLOATING) {
		cmdq_error(item, "pane is already floating");
		return (CMD_RETURN_ERROR);
	}
	if (wp->flags & PANE_MINIMISED) {
		cmdq_error(item, "can't float a minimised pane");
		return (CMD_RETURN_ERROR);
	}
	if (w->flags & WINDOW_ZOOMED) {
		cmdq_error(item, "can't float a pane while window is zoomed");
		return (CMD_RETURN_ERROR);
	}

	/*
	 * If no geometry was given explicitly and we have a saved floating
	 * position from a previous tile-pane, restore it.
	 */
	if ((wp->flags & PANE_SAVED_FLOAT) &&
	    !args_has(args, 'x') && !args_has(args, 'y') &&
	    !args_has(args, 'X') && !args_has(args, 'Y')) {
		x  = wp->saved_float_xoff;
		y  = wp->saved_float_yoff;
		sx = wp->saved_float_sx;
		sy = wp->saved_float_sy;
	} else {
		if (cmd_float_pane_parse_geometry(w, &x, &y, &sx, &sy, item,
		    args) != 0)
			return (CMD_RETURN_ERROR);
	}

	/*
	 * Remove the pane from the tiled layout tree so neighbours reclaim
	 * the space.  layout_close_pane calls layout_destroy_cell which frees
	 * the tiled layout_cell and sets wp->layout_cell = NULL via
	 * layout_free_cell.  It also calls layout_fix_offsets/fix_panes and
	 * notify_window, which is fine to do here before we set up the
	 * floating cell.
	 */
	layout_close_pane(wp);		/* wp->layout_cell is NULL afterwards */

	/* Create a detached floating cell with the requested geometry. */
	lc = layout_create_cell(NULL);
	lc->xoff = x;
	lc->yoff = y;
	lc->sx = sx;
	lc->sy = sy;
	layout_make_leaf(lc, wp);	/* sets wp->layout_cell = lc, lc->wp = wp */

	wp->flags |= PANE_FLOATING;
	TAILQ_REMOVE(&w->z_index, wp, zentry);
	TAILQ_INSERT_HEAD(&w->z_index, wp, zentry);

	if (w->layout_root != NULL)
		layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	notify_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_tile_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	__attribute((unused)) struct args	*args = cmd_get_args(self);
	struct cmd_find_state			*target = cmdq_get_target(item);
	struct window				*w = target->wl->window;
	struct window_pane			*wp = target->wp;
	struct window_pane	*target_wp, *wpiter;
	struct layout_cell	*float_lc, *lc;
	int			 was_minimised;

	if (!(wp->flags & PANE_FLOATING)) {
		cmdq_error(item, "pane is not floating");
		return (CMD_RETURN_ERROR);
	}
	if (w->flags & WINDOW_ZOOMED) {
		cmdq_error(item, "can't tile a pane while window is zoomed");
		return (CMD_RETURN_ERROR);
	}

	was_minimised = (wp->flags & PANE_MINIMISED) != 0;

	/*
	 * Save the floating geometry so we can restore it next time this pane
	 * is floated without an explicit position/size.
	 */
	float_lc = wp->layout_cell;
	wp->saved_float_xoff = float_lc->xoff;
	wp->saved_float_yoff = float_lc->yoff;
	wp->saved_float_sx   = float_lc->sx;
	wp->saved_float_sy   = float_lc->sy;
	wp->flags |= PANE_SAVED_FLOAT;

	/*
	 * If the pane is also minimised, clear saved_layout_cell before
	 * freeing the floating cell — otherwise the pointer would dangle.
	 */
	if (was_minimised)
		wp->saved_layout_cell = NULL;

	/*
	 * Free the detached floating cell.  Clear its wp pointer first so
	 * layout_free_cell's WINDOWPANE case does not corrupt wp->layout_cell.
	 */
	float_lc->wp = NULL;
	layout_free_cell(float_lc);
	wp->layout_cell = NULL;

	/*
	 * Find the best tiled pane to split after, prefer a visible (non-
	 * minimised) tiled pane.  If all tiled panes are minimised, fall back
	 * to any tiled pane so the new pane enters the existing tree rather
	 * than becoming a disconnected root.
	 */
	target_wp = NULL;
	if (w->active != NULL && !(w->active->flags & PANE_FLOATING) &&
	    !(w->active->flags & PANE_MINIMISED))
		target_wp = w->active;
	if (target_wp == NULL) {
		TAILQ_FOREACH(wpiter, &w->last_panes, sentry) {
			if (!(wpiter->flags & (PANE_FLOATING|PANE_MINIMISED)) &&
			    window_pane_visible(wpiter)) {
				target_wp = wpiter;
				break;
			}
		}
	}
	if (target_wp == NULL) {
		TAILQ_FOREACH(wpiter, &w->panes, entry) {
			if (!(wpiter->flags & (PANE_FLOATING|PANE_MINIMISED)) &&
			    window_pane_visible(wpiter)) {
				target_wp = wpiter;
				break;
			}
		}
	}
	/* Fall back to any tiled pane (even minimised) to stay in the tree. */
	if (target_wp == NULL) {
		TAILQ_FOREACH(wpiter, &w->panes, entry) {
			if (!(wpiter->flags & PANE_FLOATING)) {
				target_wp = wpiter;
				break;
			}
		}
	}
	if (target_wp != NULL) {
		lc = layout_split_pane(target_wp, LAYOUT_TOPBOTTOM, -1, 0);
		if (lc == NULL)
			lc = layout_split_pane(target_wp, LAYOUT_LEFTRIGHT,
			    -1, 0);
		if (lc == NULL) {
			cmdq_error(item, "not enough space to tile pane");
			return (CMD_RETURN_ERROR);
		}
		layout_assign_pane(lc, wp, 0);
		/*
		 * Redistribute space equally among all visible panes at this
		 * level, so the new pane gets an equal share rather than just
		 * half of the split target.
		 */
		if (wp->layout_cell != NULL && wp->layout_cell->parent != NULL)
			layout_redistribute_cells(w, wp->layout_cell->parent,
			    wp->layout_cell->parent->type);
	} else {
		/*
		 * No tiled panes at all: make this pane the sole tiled pane
		 * (new layout root).
		 */
		lc = layout_create_cell(NULL);
		lc->sx = w->sx;
		lc->sy = w->sy;
		lc->xoff = 0;
		lc->yoff = 0;
		w->layout_root = lc;
		layout_make_leaf(lc, wp);
	}

	/*
	 * If the pane was minimised while floating, record its new tiled cell
	 * as the saved cell so unminimise can restore it correctly.
	 */
	if (was_minimised)
		wp->saved_layout_cell = wp->layout_cell;

	wp->flags &= ~PANE_FLOATING;
	TAILQ_REMOVE(&w->z_index, wp, zentry);
	TAILQ_INSERT_TAIL(&w->z_index, wp, zentry);

	if (!(wp->flags & PANE_MINIMISED))
		window_set_active_pane(w, wp, 1);

	if (w->layout_root != NULL)
		layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	notify_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}
