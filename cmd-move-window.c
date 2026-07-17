/* $OpenBSD: cmd-move-window.c,v 1.34 2021/08/21 10:22:39 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Move a window.
 */

static enum cmd_retval	cmd_move_window_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_move_window_entry = {
	.name = "move-window",
	.alias = "movew",

	.args = { "abdkrs:t:", 0, 0, NULL },
	.usage = "[-abdkr] " CMD_SRCDST_WINDOW_USAGE,

	.source = { 's', CMD_FIND_WINDOW, 0 },
	/* -t is special */

	.flags = 0,
	.exec = cmd_move_window_exec
};

const struct cmd_entry cmd_link_window_entry = {
	.name = "link-window",
	.alias = "linkw",

	.args = { "abdks:t:", 0, 0, NULL },
	.usage = "[-abdk] " CMD_SRCDST_WINDOW_USAGE,

	.source = { 's', CMD_FIND_WINDOW, 0 },
	/* -t is special */

	.flags = 0,
	.exec = cmd_move_window_exec
};

static enum cmd_retval
cmd_move_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct cmd_find_state	 target;
	struct client		*c = cmdq_get_client(item);
	struct client		*tc = cmdq_get_target_client(item);
	struct client		*ac;
	const char		*tflag = args_get(args, 't');
	struct session		*src = source->s;
	struct session		*dst;
	struct winlink		*wl = source->wl, *dstwl, *current_wl;
	char			*cause;
	int			 idx, kflag, dflag, sflag, before, local;

	if (args_has(args, 'r')) {
		if (cmd_find_target(&target, item, tflag, CMD_FIND_SESSION,
		    CMD_FIND_QUIET) != 0)
			return (CMD_RETURN_ERROR);

		session_renumber_windows(target.s);
		recalculate_sizes();
		server_status_session(target.s);

		return (CMD_RETURN_NORMAL);
	}
	if (cmd_find_target(&target, item, tflag, CMD_FIND_WINDOW,
	    CMD_FIND_WINDOW_INDEX) != 0)
		return (CMD_RETURN_ERROR);
	dst = target.s;
	idx = target.idx;

	kflag = args_has(args, 'k');
	dflag = args_has(args, 'd');
	sflag = args_has(args, 's');

	ac = tc;
	if (ac == NULL || ac->session != dst)
		ac = c;
	local = (!dflag && active_is_local_window(ac, dst));

	before = args_has(args, 'b');
	if (args_has(args, 'a') || before) {
		if (target.wl != NULL)
			idx = winlink_shuffle_up(dst, target.wl, before);
		else {
			current_wl = active_get_effective_winlink(ac, dst);
			idx = winlink_shuffle_up(dst, current_wl, before);
		}
		if (idx == -1)
			return (CMD_RETURN_ERROR);
	}

	if (server_link_window(src, wl, dst, idx, kflag, !dflag && !local,
	    &cause) != 0) {
		cmdq_error(item, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}
	dstwl = winlink_find_by_window(&dst->windows, wl->window);
	if (cmd_get_entry(self) == &cmd_move_window_entry)
		server_unlink_window(src, wl);
	if (local && dstwl != NULL) {
		active_select_window(ac, dst, dstwl);
		if (ac != NULL && ac->session == dst)
			server_redraw_client(ac);
	}

	/*
	 * Renumber the winlinks in the src session only, the destination
	 * session already has the correct winlink id to us, either
	 * automatically or specified by -s.
	 */
	if (!sflag && options_get_number(src->options, "renumber-windows"))
		session_renumber_windows(src);

	recalculate_sizes();

	return (CMD_RETURN_NORMAL);
}
