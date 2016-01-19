/* $OpenBSD$ */

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

enum cmd_retval	 cmd_move_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_move_window_entry = {
	.name = "move-window",
	.alias = "movew",

	.args = { "adkrs:t:", 0, 0 },
	.usage = "[-dkr] " CMD_SRCDST_WINDOW_USAGE,

	.sflag = CMD_WINDOW,
	.tflag = CMD_MOVEW_R,

	.flags = 0,
	.exec = cmd_move_window_exec
};

const struct cmd_entry cmd_link_window_entry = {
	.name = "link-window",
	.alias = "linkw",

	.args = { "adks:t:", 0, 0 },
	.usage = "[-dk] " CMD_SRCDST_WINDOW_USAGE,

	.sflag = CMD_WINDOW,
	.tflag = CMD_WINDOW_INDEX,

	.flags = 0,
	.exec = cmd_move_window_exec
};

enum cmd_retval
cmd_move_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct session	*src = cmdq->state.sflag.s;
	struct session	*dst = cmdq->state.tflag.s;
	struct winlink	*wl = cmdq->state.sflag.wl;
	char		*cause;
	int		 idx = cmdq->state.tflag.idx, kflag, dflag, sflag;

	kflag = args_has(self->args, 'k');
	dflag = args_has(self->args, 'd');

	if (args_has(args, 'r')) {
		session_renumber_windows(dst);
		recalculate_sizes();

		return (CMD_RETURN_NORMAL);
	}

	kflag = args_has(self->args, 'k');
	dflag = args_has(self->args, 'd');
	sflag = args_has(self->args, 's');

	if (args_has(self->args, 'a')) {
		if ((idx = winlink_shuffle_up(dst, dst->curw)) == -1)
			return (CMD_RETURN_ERROR);
	}

	if (server_link_window(src, wl, dst, idx, kflag, !dflag,
	    &cause) != 0) {
		cmdq_error(cmdq, "can't link window: %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}
	if (self->entry == &cmd_move_window_entry)
		server_unlink_window(src, wl);

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
