/* $OpenBSD: cmd-swap-pane.c,v 1.56 2026/10/02 12:23:44 nicm Exp $ */

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

#include <stdlib.h>

#include "tmux.h"

/*
 * Swap two panes.
 */

static enum cmd_retval	cmd_swap_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_swap_pane_entry = {
	.name = "swap-pane",
	.alias = "swapp",

	.args = { "dDs:t:UZ", 0, 0, NULL },
	.usage = "[-dDUZ] " CMD_SRCDST_PANE_USAGE,

	.source = { 's', CMD_FIND_PANE, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_swap_pane_exec
};

static struct window_pane *
cmd_swap_pane_next_tiled_pane(struct window_pane *wp)
{
	while (wp != NULL && !layout_cell_is_tiled(wp->layout_cell))
		wp = TAILQ_NEXT(wp, entry);
	return (wp);
}

static struct window_pane *
cmd_swap_pane_prev_tiled_pane(struct window_pane *wp)
{
	while (wp != NULL && !layout_cell_is_tiled(wp->layout_cell))
		wp = TAILQ_PREV(wp, window_panes, entry);
	return (wp);
}

static void
cmd_swap_pane_zoom(struct window *w)
{
	window_unzoom(w, 1);
	window_zoom(w->active);
}

static enum cmd_retval
cmd_swap_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct window		*src_w, *dst_w;
	struct window_pane	*tmp_wp, *src_wp, *dst_wp;
	struct layout_cell	*src_lc, *dst_lc;
	u_int			 sx, sy, xoff, yoff;
	int			 src_idx, dst_idx, flags;
	int			 src_zoomed, dst_zoomed;

	dst_w = target->wl->window;
	dst_wp = target->wp;
	dst_idx = target->wl->idx;
	src_w = source->wl->window;
	src_wp = source->wp;
	src_idx = source->wl->idx;

	if (src_wp == src_w->modal || dst_wp == dst_w->modal) {
		cmdq_error(item, "pane is modal");
		return (CMD_RETURN_ERROR);
	}

	dst_zoomed = (args_has(args, 'Z') && (dst_w->flags & WINDOW_ZOOMED));

	if (args_has(args, 'D')) {
		if (window_pane_is_floating(dst_wp)) {
			cmdq_error(item, "cannot swap down on floating pane");
			return (CMD_RETURN_ERROR);
		}
		src_w = dst_w;
		src_wp = TAILQ_NEXT(dst_wp, entry);
		src_wp = cmd_swap_pane_next_tiled_pane(src_wp);
		if (src_wp == NULL) {
			src_wp = TAILQ_FIRST(&dst_w->panes);
			src_wp = cmd_swap_pane_next_tiled_pane(src_wp);
		}
	} else if (args_has(args, 'U')) {
		if (window_pane_is_floating(dst_wp)) {
			cmdq_error(item, "cannot swap up on floating pane");
			return (CMD_RETURN_ERROR);
		}
		src_w = dst_w;
		src_wp = TAILQ_PREV(dst_wp, window_panes, entry);
		src_wp = cmd_swap_pane_prev_tiled_pane(src_wp);
		if (src_wp == NULL) {
			src_wp = TAILQ_LAST(&dst_w->panes, window_panes);
			src_wp = cmd_swap_pane_prev_tiled_pane(src_wp);
		}
	}

	src_zoomed = (args_has(args, 'Z') && (src_w->flags & WINDOW_ZOOMED));

	if (src_wp == NULL || src_wp == dst_wp)
		goto out;

	server_client_remove_pane(src_wp);
	server_client_remove_pane(dst_wp);

	tmp_wp = TAILQ_PREV(dst_wp, window_panes, entry);
	TAILQ_REMOVE(&dst_w->panes, dst_wp, entry);
	TAILQ_REPLACE(&src_w->panes, src_wp, dst_wp, entry);
	if (tmp_wp == src_wp)
		tmp_wp = dst_wp;
	if (tmp_wp == NULL)
		TAILQ_INSERT_HEAD(&dst_w->panes, src_wp, entry);
	else
		TAILQ_INSERT_AFTER(&dst_w->panes, tmp_wp, src_wp, entry);

	tmp_wp = TAILQ_PREV(dst_wp, window_panes, zentry);
	TAILQ_REMOVE(&dst_w->z_index, dst_wp, zentry);
	TAILQ_REPLACE(&src_w->z_index, src_wp, dst_wp, zentry);
	if (tmp_wp == src_wp)
		tmp_wp = dst_wp;
	if (tmp_wp == NULL)
		TAILQ_INSERT_HEAD(&dst_w->z_index, src_wp, zentry);
	else
		TAILQ_INSERT_AFTER(&dst_w->z_index, tmp_wp, src_wp, zentry);

	/* Zoom and being hidden belong to the position, like the cell. */
	flags = (src_wp->flags ^ dst_wp->flags) &
	    (PANE_ZOOMED|PANE_HIDDEN|PANE_HIDDENALL);
	src_wp->flags ^= flags;
	dst_wp->flags ^= flags;

	src_lc = src_wp->layout_cell;
	dst_lc = dst_wp->layout_cell;
	src_lc->wp = dst_wp;
	dst_wp->layout_cell = src_lc;
	dst_lc->wp = src_wp;
	src_wp->layout_cell = dst_lc;

	src_wp->window = dst_w;
	options_set_parent(src_wp->options, dst_w->options);
	src_wp->flags |= (PANE_STYLECHANGED|PANE_THEMECHANGED);
	dst_wp->window = src_w;
	options_set_parent(dst_wp->options, src_w->options);
	dst_wp->flags |= (PANE_STYLECHANGED|PANE_THEMECHANGED);

	sx = src_wp->sx; sy = src_wp->sy;
	xoff = src_wp->xoff; yoff = src_wp->yoff;
	src_wp->xoff = dst_wp->xoff; src_wp->yoff = dst_wp->yoff;
	window_pane_resize(src_wp, dst_wp->sx, dst_wp->sy);
	dst_wp->xoff = xoff; dst_wp->yoff = yoff;
	window_pane_resize(dst_wp, sx, sy);

	if (!args_has(args, 'd')) {
		if (src_w != dst_w) {
			window_set_active_pane(src_w, dst_wp, 1);
			window_set_active_pane(dst_w, src_wp, 1);
		} else {
			tmp_wp = dst_wp;
			window_set_active_pane(src_w, tmp_wp, 1);
		}
	} else {
		/*
		 * Keep the same pane active, unless the swap has covered it
		 * with a zoomed pane: activating it would hide the zoom. With
		 * -Z the zoom moves to the active pane afterwards instead.
		 */
		if (src_w->active == src_wp)
			window_set_active_pane(src_w, dst_wp, 1);
		if (dst_w->active == dst_wp &&
		    (args_has(args, 'Z') || window_pane_is_visible(src_wp)))
			window_set_active_pane(dst_w, src_wp, 1);
	}
	if (src_w != dst_w) {
		window_pane_stack_remove(&src_w->last_panes, src_wp);
		window_pane_stack_remove(&dst_w->last_panes, dst_wp);
		colour_palette_from_option(&src_wp->palette, src_wp->options);
		colour_palette_from_option(&dst_wp->palette, dst_wp->options);
		layout_fix_panes(src_w, NULL);
		redraw_invalidate_scene(src_w);
		server_redraw_window(src_w);
	}
	layout_fix_panes(dst_w, NULL);
	redraw_invalidate_scene(dst_w);
	server_redraw_window(dst_w);

	if (src_w != dst_w) {
		window_fire_pane_moved(src_wp, src_w, src_idx, dst_w, dst_idx);
		window_fire_pane_moved(dst_wp, dst_w, dst_idx, src_w, src_idx);
	}
	events_fire_window("window-layout-changed", src_w);
	if (src_w != dst_w)
		events_fire_window("window-layout-changed", dst_w);

out:
	/* With -Z, leave the active pane zoomed if the window was. */
	if (src_zoomed)
		cmd_swap_pane_zoom(src_w);
	if (src_w != dst_w && dst_zoomed)
		cmd_swap_pane_zoom(dst_w);
	return (CMD_RETURN_NORMAL);
}
