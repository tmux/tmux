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
	.usage = "[-bdfhv] [-l size] " CMD_SRCDST_PANE_USAGE,

	.source = { 's', CMD_FIND_PANE, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_join_pane_exec
};

const struct cmd_entry cmd_move_pane_entry = {
	.name = "move-pane",
	.alias = "movep",

	.args = { "bdfhvp:l:s:t:", 0, 0, NULL },
	.usage = "[-bdfhv] [-l size] " CMD_SRCDST_PANE_USAGE,

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
	int			 size, dst_idx;
	int			 flags;
	enum layout_type	 type;
	struct layout_cell	*lc;
	u_int			 curval = 0;

	dst_s = target->s;
	dst_wl = target->wl;
	dst_wp = target->wp;
	dst_w = dst_wl->window;
	dst_idx = dst_wl->idx;
	server_unzoom_window(dst_w);

	src_wl = source->wl;
	src_wp = source->wp;
	src_w = src_wl->window;
	server_unzoom_window(src_w);

	if (src_wp == dst_wp) {
		cmdq_error(item, "source and target panes must be different");
		return (CMD_RETURN_ERROR);
	}

	type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;

	/* If the 'p' flag is dropped then this bit can be moved into 'l'. */
	if (args_has(args, 'l') || args_has(args, 'p')) {
		if (args_has(args, 'f')) {
			if (type == LAYOUT_TOPBOTTOM)
				curval = dst_w->sy;
			else
				curval = dst_w->sx;
		} else {
			if (type == LAYOUT_TOPBOTTOM)
				curval = dst_wp->sy;
			else
				curval = dst_wp->sx;
		}
	}

	size = -1;
	if (args_has(args, 'l')) {
		size = args_percentage_and_expand(args, 'l', 0, INT_MAX, curval,
			   item, &cause);
	} else if (args_has(args, 'p')) {
		size = args_strtonum_and_expand(args, 'l', 0, 100, item,
			   &cause);
		if (cause == NULL)
			size = curval * size / 100;
	}
	if (cause != NULL) {
		cmdq_error(item, "size %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	flags = 0;
	if (args_has(args, 'b'))
		flags |= SPAWN_BEFORE;
	if (args_has(args, 'f'))
		flags |= SPAWN_FULLSIZE;

	lc = layout_split_pane(dst_wp, type, size, flags);
	if (lc == NULL) {
		cmdq_error(item, "create pane failed: pane too small");
		return (CMD_RETURN_ERROR);
	}

	layout_close_pane(src_wp);

	server_client_remove_pane(src_wp);
	window_lost_pane(src_w, src_wp);
	TAILQ_REMOVE(&src_w->panes, src_wp, entry);

	src_wp->window = dst_w;
	options_set_parent(src_wp->options, dst_w->options);
	src_wp->flags |= PANE_STYLECHANGED;
	if (flags & SPAWN_BEFORE)
		TAILQ_INSERT_BEFORE(dst_wp, src_wp, entry);
	else
		TAILQ_INSERT_AFTER(&dst_w->panes, dst_wp, src_wp, entry);
	layout_assign_pane(lc, src_wp, 0);
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

	if (window_count_panes(src_w) == 0)
		server_kill_window(src_w, 1);
	else
		notify_window("window-layout-changed", src_w);
	notify_window("window-layout-changed", dst_w);

	return (CMD_RETURN_NORMAL);
}
