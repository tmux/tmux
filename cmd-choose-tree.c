/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Thomas Adam <thomas@xteddy.org>
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
 * Enter a mode.
 */

static enum cmd_retval	cmd_choose_tree_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_choose_tree_entry = {
	.name = "choose-tree",
	.alias = NULL,

	.args = { "F:f:NO:st:w", 0, 1 },
	.usage = "[-Nsw] [-F format] [-f filter] [-O sort-order] "
	         CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_choose_client_entry = {
	.name = "choose-client",
	.alias = NULL,

	.args = { "F:f:NO:t:", 0, 1 },
	.usage = "[-N] [-F format] [-f filter] [-O sort-order] "
	         CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_choose_buffer_entry = {
	.name = "choose-buffer",
	.alias = NULL,

	.args = { "F:f:NO:t:", 0, 1 },
	.usage = "[-N] [-F format] [-f filter] [-O sort-order] "
	         CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

static enum cmd_retval
cmd_choose_tree_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = self->args;
	struct window_pane		*wp = item->target.wp;
	const struct window_mode	*mode;

	if (self->entry == &cmd_choose_buffer_entry) {
		if (paste_get_top(NULL) == NULL)
			return (CMD_RETURN_NORMAL);
		mode = &window_buffer_mode;
	} else if (self->entry == &cmd_choose_client_entry) {
		if (server_client_how_many() == 0)
			return (CMD_RETURN_NORMAL);
		mode = &window_client_mode;
	} else
		mode = &window_tree_mode;

	window_pane_set_mode(wp, mode, &item->target, args);
	return (CMD_RETURN_NORMAL);
}
