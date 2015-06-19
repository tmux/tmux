/* $OpenBSD$ */

/*
 * Copyright (c) 2011 George Nachman <tmux@georgester.com>
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <unistd.h>

#include "tmux.h"

/*
 * Join or move a pane into another (like split/swap/kill).
 */

enum cmd_retval	 cmd_join_pane_exec(struct cmd *, struct cmd_q *);

enum cmd_retval	 join_pane(struct cmd *, struct cmd_q *, int);

const struct cmd_entry cmd_join_pane_entry = {
	"join-pane", "joinp",
	"bdhvp:l:s:t:", 0, 0,
	"[-bdhv] [-p percentage|-l size] " CMD_SRCDST_PANE_USAGE,
	0,
	cmd_join_pane_exec
};

const struct cmd_entry cmd_move_pane_entry = {
	"move-pane", "movep",
	"bdhvp:l:s:t:", 0, 0,
	"[-bdhv] [-p percentage|-l size] " CMD_SRCDST_PANE_USAGE,
	0,
	cmd_join_pane_exec
};

enum cmd_retval
cmd_join_pane_exec(struct cmd *self, struct cmd_q *cmdq)
{
	return (join_pane(self, cmdq, self->entry == &cmd_join_pane_entry));
}

enum cmd_retval
join_pane(struct cmd *self, struct cmd_q *cmdq, int not_same_window)
{
	struct args		*args = self->args;
	struct session		*dst_s;
	struct winlink		*src_wl, *dst_wl;
	struct window		*src_w, *dst_w;
	struct window_pane	*src_wp, *dst_wp;
	char			*cause;
	int			 size, percentage, dst_idx;
	enum layout_type	 type;
	struct layout_cell	*lc;

	dst_wl = cmd_find_pane(cmdq, args_get(args, 't'), &dst_s, &dst_wp);
	if (dst_wl == NULL)
		return (CMD_RETURN_ERROR);
	dst_w = dst_wl->window;
	dst_idx = dst_wl->idx;
	server_unzoom_window(dst_w);

	src_wl = cmd_find_pane_marked(cmdq, args_get(args, 's'), NULL, &src_wp);
	if (src_wl == NULL)
		return (CMD_RETURN_ERROR);
	src_w = src_wl->window;
	server_unzoom_window(src_w);

	if (not_same_window && src_w == dst_w) {
		cmdq_error(cmdq, "can't join a pane to its own window");
		return (CMD_RETURN_ERROR);
	}
	if (!not_same_window && src_wp == dst_wp) {
		cmdq_error(cmdq, "source and target panes must be different");
		return (CMD_RETURN_ERROR);
	}

	type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;

	size = -1;
	if (args_has(args, 'l')) {
		size = args_strtonum(args, 'l', 0, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(cmdq, "size %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'p')) {
		percentage = args_strtonum(args, 'p', 0, 100, &cause);
		if (cause != NULL) {
			cmdq_error(cmdq, "percentage %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (type == LAYOUT_TOPBOTTOM)
			size = (dst_wp->sy * percentage) / 100;
		else
			size = (dst_wp->sx * percentage) / 100;
	}
	lc = layout_split_pane(dst_wp, type, size, args_has(args, 'b'));
	if (lc == NULL) {
		cmdq_error(cmdq, "create pane failed: pane too small");
		return (CMD_RETURN_ERROR);
	}

	layout_close_pane(src_wp);

	window_lost_pane(src_w, src_wp);
	TAILQ_REMOVE(&src_w->panes, src_wp, entry);

	if (window_count_panes(src_w) == 0)
		server_kill_window(src_w);
	else
		notify_window_layout_changed(src_w);

	src_wp->window = dst_w;
	TAILQ_INSERT_AFTER(&dst_w->panes, dst_wp, src_wp, entry);
	layout_assign_pane(lc, src_wp);

	recalculate_sizes();

	server_redraw_window(src_w);
	server_redraw_window(dst_w);

	if (!args_has(args, 'd')) {
		window_set_active_pane(dst_w, src_wp);
		session_select(dst_s, dst_idx);
		server_redraw_session(dst_s);
	} else
		server_status_session(dst_s);

	notify_window_layout_changed(dst_w);
	return (CMD_RETURN_NORMAL);
}
