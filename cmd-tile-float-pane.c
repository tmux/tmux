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
 * floating, using the same mechanism as hide-pane.  The cell's wp pointer
 * is cleared while the pane is floating so that layout helpers treat the slot
 * as empty.
 */

static enum cmd_retval	cmd_float_pane_exec(struct cmd *, struct cmdq_item *);
static enum cmd_retval	cmd_tile_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_float_pane_entry = {
	.name = "float-pane",
	.alias = "floatp",

	.args = { "dt:x:X:y:Y:", 0, 0, NULL },
	.usage = "[-x height] [-y width] [-X x-position] [-Y y-position] "
		 CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_float_pane_exec
};

const struct cmd_entry cmd_tile_pane_entry = {
	.name = "tile-pane",
	.alias = "tilep",

	.args = { "dt:", 0, 0, NULL },
	.usage = CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_tile_pane_exec
};

static enum cmd_retval
cmd_float_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct window		*w = target->wl->window;
	struct window_pane	*wp = target->wp;
	struct layout_cell	*lc = wp->layout_cell;
	u_int			 sx = lc->saved_sx, sy = lc->saved_sy;
	int			 ox = lc->saved_xoff, oy = lc->saved_yoff;
	char			*cause = NULL;

	if (window_pane_is_floating(wp)) {
		cmdq_error(item, "pane is already floating");
		return (CMD_RETURN_ERROR);
	}
	if (window_pane_is_hidden(wp)) {
		cmdq_error(item, "can't float a hidden pane");
		return (CMD_RETURN_ERROR);
	}
	if (w->flags & WINDOW_ZOOMED) {
		cmdq_error(item, "can't float a pane while window is zoomed");
		return (CMD_RETURN_ERROR);
	}

	layout_remove_tile(w, lc);
	layout_cell_floating_args(item, args, w, &sx, &sy, &ox, &oy, &cause);
	if (cause != NULL) {
		cmdq_error(item, "failed to float pane: %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}
	layout_set_size(lc, sx, sy, ox, oy);

	lc->flags |= LAYOUT_CELL_FLOATING;
	TAILQ_REMOVE(&w->z_index, wp, zentry);
	TAILQ_INSERT_HEAD(&w->z_index, wp, zentry);

	if (!args_has(args, 'd'))
		window_set_active_pane(w, wp, 1);
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	notify_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_tile_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	__unused struct args	*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct window		*w = target->wl->window;
	struct window_pane	*wp = target->wp;
	struct layout_cell	*lc = wp->layout_cell;

	if (!window_pane_is_floating(wp)) {
		cmdq_error(item, "pane is not floating");
		return (CMD_RETURN_ERROR);
	}
	if (window_pane_is_hidden(wp)) {
		cmdq_error(item, "can't tile a hidden pane");
		return (CMD_RETURN_ERROR);
	}
	if (w->flags & WINDOW_ZOOMED) {
		cmdq_error(item, "can't tile a pane while window is zoomed");
		return (CMD_RETURN_ERROR);
	}

	layout_save_size(lc);
	if (layout_insert_tile(w, lc) == 0)
		return (CMD_RETURN_ERROR);


	lc->flags &= ~LAYOUT_CELL_FLOATING;
	TAILQ_REMOVE(&w->z_index, wp, zentry);
	TAILQ_INSERT_TAIL(&w->z_index, wp, zentry);

	if (!args_has(args, 'd'))
		window_set_active_pane(w, wp, 1);
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	notify_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}
