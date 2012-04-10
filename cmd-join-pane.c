/* $Id$ */

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

void	cmd_join_pane_key_binding(struct cmd *, int);
int	cmd_join_pane_exec(struct cmd *, struct cmd_ctx *);

int	join_pane(struct cmd *, struct cmd_ctx *, int);

const struct cmd_entry cmd_join_pane_entry = {
	"join-pane", "joinp",
	"bdhvp:l:s:t:", 0, 0,
	"[-bdhv] [-p percentage|-l size] [-s src-pane] [-t dst-pane]",
	0,
	cmd_join_pane_key_binding,
	NULL,
	cmd_join_pane_exec
};

const struct cmd_entry cmd_move_pane_entry = {
	"move-pane", "movep",
	"bdhvp:l:s:t:", 0, 0,
	"[-bdhv] [-p percentage|-l size] [-s src-pane] [-t dst-pane]",
	0,
	NULL,
	NULL,
	cmd_join_pane_exec
};

void
cmd_join_pane_key_binding(struct cmd *self, int key)
{
	switch (key) {
	case '%':
		self->args = args_create(0);
		args_set(self->args, 'h', NULL);
		break;
	default:
		self->args = args_create(0);
		break;
	}
}

int
cmd_join_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	return (join_pane(self, ctx, self->entry == &cmd_join_pane_entry));
}

int
join_pane(struct cmd *self, struct cmd_ctx *ctx, int not_same_window)
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

	dst_wl = cmd_find_pane(ctx, args_get(args, 't'), &dst_s, &dst_wp);
	if (dst_wl == NULL)
		return (-1);
	dst_w = dst_wl->window;
	dst_idx = dst_wl->idx;

	src_wl = cmd_find_pane(ctx, args_get(args, 's'), NULL, &src_wp);
	if (src_wl == NULL)
		return (-1);
	src_w = src_wl->window;

	if (not_same_window && src_w == dst_w) {
		ctx->error(ctx, "can't join a pane to its own window");
		return (-1);
	}
	if (!not_same_window && src_wp == dst_wp) {
		ctx->error(ctx, "source and target panes must be different");
		return (-1);
	}

	type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;

	size = -1;
	if (args_has(args, 'l')) {
		size = args_strtonum(args, 'l', 0, INT_MAX, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "size %s", cause);
			xfree(cause);
			return (-1);
		}
	} else if (args_has(args, 'p')) {
		percentage = args_strtonum(args, 'p', 0, 100, &cause);
		if (cause != NULL) {
			ctx->error(ctx, "percentage %s", cause);
			xfree(cause);
			return (-1);
		}
		if (type == LAYOUT_TOPBOTTOM)
			size = (dst_wp->sy * percentage) / 100;
		else
			size = (dst_wp->sx * percentage) / 100;
	}
	lc = layout_split_pane(dst_wp, type, size, args_has(args, 'b'));
	if (lc == NULL) {
		ctx->error(ctx, "create pane failed: pane too small");
		return (-1);
	}

	layout_close_pane(src_wp);

	if (src_w->active == src_wp) {
		src_w->active = TAILQ_PREV(src_wp, window_panes, entry);
		if (src_w->active == NULL)
			src_w->active = TAILQ_NEXT(src_wp, entry);
	}
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
	return (0);
}
