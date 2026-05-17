/* $OpenBSD$ */

/*
 * Copyright (c) 2011 George Nachman <tmux@georgester.com>
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
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Join or move a pane into another (like split/swap/kill).
 */

static enum cmd_retval	cmd_join_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_join_pane_entry = {
	.name = "join-pane",
	.alias = "joinp",

	.args = { "bdfhvp:l:s:t:", 0, 0, NULL },
	.usage = "[-bdfhv] [-l size] [-p percentage] "
		 CMD_SRCDST_PANE_USAGE,

	.source = { 's', CMD_FIND_PANE, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_join_pane_exec
};

const struct cmd_entry cmd_move_pane_entry = {
	.name = "move-pane",
	.alias = "movep",

	.args = { "bdfhvp:l:Ls:t:Wx:X:y:Y:", 0, 0, NULL },
	.usage = "[-bdfhv] [-l size] [-p percentage] "
		 "[-x width] [-y height] [-X x-position] [-Y y-position] "
		 CMD_SRCDST_PANE_USAGE,

	.source = { 's', CMD_FIND_PANE, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_join_pane_exec
};

static enum cmd_retval
cmd_join_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct session		*dst_s;
	struct winlink		*src_wl, *dst_wl;
	struct window		*src_w, *dst_w;
	struct window_pane	*src_wp, *dst_wp;
	char			*cause = NULL;
	int			 dst_idx;
	int			 float_pane, flags = 0;
	struct layout_cell	*lc;

	dst_s = target->s;
	dst_wl = target->wl;
	dst_wp = target->wp;
	dst_w = dst_wl->window;
	dst_idx = dst_wl->idx;
	server_unzoom_window(dst_w);

	src_wl = source->wl;
	src_wp = source->wp;
	lc = src_wp->layout_cell;
	src_w = src_wl->window;
	server_unzoom_window(src_w);

	if (window_pane_is_hidden(src_wp)) {
		cmdq_error(item, "cannot move a hidden pane");
		return (CMD_RETURN_ERROR);
	}

	if (cmd_get_entry(self) == &cmd_join_pane_entry)
		float_pane = 0;
	else
		if (args_has(args, 'W'))
			float_pane = 1;
		else if (args_has(args, 'L'))
			float_pane = 0;
		else
			float_pane = (window_pane_is_floating(src_wp)) > 0;

	if (float_pane) { /* destination will be floating */
		if (!window_pane_is_floating(src_wp))
			lc = src_wp->placeholder_layout_cell;
		lc = layout_get_floating_cell(item, args, src_w, src_wp, lc,
		    &cause);
		if (lc == NULL) {
			cmdq_error(item, "%s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}

		if (src_w == dst_w) {
			if (!window_pane_is_floating(src_wp)) {
				src_wp->placeholder_layout_cell = lc;
				src_wp->layout_cell = layout_make_placeholder(
				    src_w, src_wp, src_wp->layout_cell);
			}
			layout_make_leaf(lc, src_wp);
		} else {
			layout_destroy_cell(src_w, src_wp->placeholder_layout_cell,
			    &src_w->layout_root);
			src_wp->placeholder_layout_cell = NULL;
			layout_make_leaf(lc, src_wp);
			layout_make_placeholder(dst_w, src_wp, NULL);
		}
	} else { /* destination will be tiled */
		if (src_wp == dst_wp) { /* called with no target */
			lc = layout_make_placeholder(src_w, src_wp,
			    src_wp->layout_cell);
			if (lc == NULL) {
				cmdq_error(item, "can't tile cell");
				return (CMD_RETURN_ERROR);
			}
			layout_reveal_cell(src_w, lc);
		} else {
			if (window_pane_is_floating(src_wp)) {
				lc = layout_make_placeholder(src_w, src_wp,
				    src_wp->layout_cell);
			} else
				lc = src_wp->layout_cell;
			layout_destroy_cell(src_w, lc, &src_w->layout_root);
			lc = layout_get_tiled_cell(item, args, dst_w, dst_wp,
			    flags, &cause);
			if (lc == NULL) {
				cmdq_error(item, "%s", cause);
				free(cause);
				return (CMD_RETURN_ERROR);
			}
			src_wp->layout_cell = lc;
		}
	}

	server_client_remove_pane(src_wp);
	window_lost_pane(src_w, src_wp);
	if (!window_pane_is_floating(src_wp) || src_w != dst_w)
		TAILQ_REMOVE(&src_w->panes, src_wp, entry);
	TAILQ_REMOVE(&src_w->z_index, src_wp, zentry);

	src_wp->window = dst_w;
	options_set_parent(src_wp->options, dst_w->options);
	src_wp->flags |= (PANE_STYLECHANGED|PANE_THEMECHANGED);
	if (window_pane_is_floating(src_wp)) {
		if (src_w != dst_w)
			TAILQ_INSERT_TAIL(&dst_w->panes, src_wp, entry);
		TAILQ_INSERT_HEAD(&dst_w->z_index, src_wp, zentry);
		if (dst_w->layout_root != NULL)
			layout_fix_offsets(dst_w);
	} else if (flags & SPAWN_BEFORE) {
		TAILQ_INSERT_BEFORE(dst_wp, src_wp, entry);
		TAILQ_INSERT_BEFORE(dst_wp, src_wp, zentry);
	} else {
		TAILQ_INSERT_AFTER(&dst_w->panes, dst_wp, src_wp, entry);
		TAILQ_INSERT_AFTER(&dst_w->z_index, dst_wp, src_wp, zentry);
	}

	layout_make_leaf(lc, src_wp);
	layout_fix_panes(src_w, NULL);
	layout_fix_panes(dst_w, NULL);
	colour_palette_from_option(&src_wp->palette, src_wp->options);

	recalculate_sizes();

	server_redraw_window(src_w);
	server_redraw_window(dst_w);

	if (!args_has(args, 'd')) {
		window_set_active_pane(dst_w, src_wp, 1);
		session_select(dst_s, dst_idx);
		cmd_find_from_session(current, dst_s, 0);
		server_redraw_session(dst_s);
	} else
		server_status_session(dst_s);

	if (window_count_panes(src_w, 1) == 0)
		server_kill_window(src_w, 1);
	else
		notify_window("window-layout-changed", src_w);
	notify_window("window-layout-changed", dst_w);

	return (CMD_RETURN_NORMAL);
}
