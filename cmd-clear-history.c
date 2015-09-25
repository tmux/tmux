/* $OpenBSD$ */

/*
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

#include "tmux.h"

/*
 * Clear pane history.
 */

enum cmd_retval	 cmd_clear_history_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_clear_history_entry = {
	"clear-history", "clearhist",
	"t:", 0, 0,
	CMD_TARGET_PANE_USAGE,
	0,
	cmd_clear_history_exec
};

enum cmd_retval
cmd_clear_history_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct window_pane	*wp;
	struct grid		*gd;

	if (cmd_find_pane(cmdq, args_get(args, 't'), NULL, &wp) == NULL)
		return (CMD_RETURN_ERROR);
	gd = wp->base.grid;

	if (wp->mode == &window_copy_mode)
		window_pane_reset_mode(wp);
	grid_clear_history(gd);

	return (CMD_RETURN_NORMAL);
}
