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

static enum args_parse_type	cmd_choose_tree_args_parse(struct args *args,
				    u_int idx, char **cause);
static enum cmd_retval		cmd_choose_tree_exec(struct cmd *,
    				    struct cmdq_item *);

const struct cmd_entry cmd_choose_tree_entry = {
	.name = "choose-tree",
	.alias = NULL,

	.args = { "F:f:GK:NO:rst:wZ", 0, 1, cmd_choose_tree_args_parse },
	.usage = "[-GNrswZ] [-F format] [-f filter] [-K key-format] "
		 "[-O sort-order] " CMD_TARGET_PANE_USAGE " [template]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_choose_client_entry = {
	.name = "choose-client",
	.alias = NULL,

	.args = { "F:f:K:NO:rt:Z", 0, 1, cmd_choose_tree_args_parse },
	.usage = "[-NrZ] [-F format] [-f filter] [-K key-format] "
		 "[-O sort-order] " CMD_TARGET_PANE_USAGE " [template]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_choose_buffer_entry = {
	.name = "choose-buffer",
	.alias = NULL,

	.args = { "F:f:K:NO:rt:Z", 0, 1, cmd_choose_tree_args_parse },
	.usage = "[-NrZ] [-F format] [-f filter] [-K key-format] "
		 "[-O sort-order] " CMD_TARGET_PANE_USAGE " [template]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_customize_mode_entry = {
	.name = "customize-mode",
	.alias = NULL,

	.args = { "F:f:Nt:Z", 0, 0, NULL },
	.usage = "[-NZ] [-F format] [-f filter] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

static enum args_parse_type
cmd_choose_tree_args_parse(__unused struct args *args, __unused u_int idx,
    __unused char **cause)
{
	return (ARGS_PARSE_COMMANDS_OR_STRING);
}

static enum cmd_retval
cmd_choose_tree_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct window_pane		*wp = target->wp;
	const struct window_mode	*mode;

	if (cmd_get_entry(self) == &cmd_choose_buffer_entry) {
		if (paste_get_top(NULL) == NULL)
			return (CMD_RETURN_NORMAL);
		mode = &window_buffer_mode;
	} else if (cmd_get_entry(self) == &cmd_choose_client_entry) {
		if (server_client_how_many() == 0)
			return (CMD_RETURN_NORMAL);
		mode = &window_client_mode;
	} else if (cmd_get_entry(self) == &cmd_customize_mode_entry)
		mode = &window_customize_mode;
	else
		mode = &window_tree_mode;

	window_pane_set_mode(wp, NULL, mode, target, args);
	return (CMD_RETURN_NORMAL);
}
