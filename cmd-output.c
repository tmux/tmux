/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include "tmux.h"

static enum cmd_retval	cmd_output_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_select_output_entry = {
	.name = "select-output",
	.alias = NULL,

	.args = { "at:", 0, 0, NULL },
	.usage = "[-a] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_output_exec
};

const struct cmd_entry cmd_copy_output_entry = {
	.name = "copy-output",
	.alias = NULL,

	.args = { "aCPt:", 0, 1, NULL },
	.usage = "[-aCP] " CMD_TARGET_PANE_USAGE " [prefix]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_output_exec
};

const struct cmd_entry cmd_copy_pipe_output_entry = {
	.name = "copy-pipe-output",
	.alias = NULL,

	.args = { "aCPt:", 0, 2, NULL },
	.usage = "[-aCP] " CMD_TARGET_PANE_USAGE " [command] [prefix]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_output_exec
};

const struct cmd_entry cmd_pipe_output_entry = {
	.name = "pipe-output",
	.alias = NULL,

	.args = { "at:", 0, 1, NULL },
	.usage = "[-a] " CMD_TARGET_PANE_USAGE " [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_output_exec
};

const struct cmd_entry cmd_open_output_entry = {
	.name = "open-output",
	.alias = NULL,

	.args = { "at:", 0, 1, NULL },
	.usage = "[-a] " CMD_TARGET_PANE_USAGE " [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_output_exec
};

const struct cmd_entry cmd_open_selection_entry = {
	.name = "open-selection",
	.alias = NULL,

	.args = { "t:", 0, 1, NULL },
	.usage = CMD_TARGET_PANE_USAGE " [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_output_exec
};

static enum cmd_retval
cmd_output_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct client		*c = cmdq_get_client(item);

	window_copy_output(target->wp, c, target->s, target->wl, item,
	    cmd_get_entry(self)->name, args);
	return (CMD_RETURN_NORMAL);
}
