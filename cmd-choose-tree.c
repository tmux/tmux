/* $OpenBSD: cmd-choose-tree.c,v 1.62 2026/10/20 12:00:58 nicm Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Enter a mode.
 */

static enum args_parse_type	cmd_choose_tree_args_parse(struct args *args,
				    u_int idx, char **cause);
static enum cmd_retval		cmd_choose_tree_exec(struct cmd *,
    				    struct cmdq_item *);
static void			cmd_choose_tree_read_done(struct client *,
				    const char *, int, int, struct evbuffer *,
				    void *);

const struct cmd_entry cmd_choose_tree_entry = {
	.name = "choose-tree",
	.description = "Browse sessions and windows interactively.",
	.alias = NULL,

	.args = { "F:f:GhK:kNO:rst:wyZ", 0, 1, cmd_choose_tree_args_parse },
	.usage = "[-GhkNrswZ] [-F format] [-f filter] [-K key-format] "
		 "[-O sort-order] " CMD_TARGET_PANE_USAGE " [template]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_choose_client_entry = {
	.name = "choose-client",
	.description = "Choose a client interactively.",
	.alias = NULL,

	.args = { "F:f:hiK:kNO:rt:yZ", 0, 1, cmd_choose_tree_args_parse },
	.usage = "[-hikNrZ] [-F format] [-f filter] [-K key-format] "
		 "[-O sort-order] " CMD_TARGET_PANE_USAGE " [template]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_choose_buffer_entry = {
	.name = "choose-buffer",
	.description = "Choose a paste buffer interactively.",
	.alias = NULL,

	.args = { "F:f:K:kNO:rt:yZ", 0, 1, cmd_choose_tree_args_parse },
	.usage = "[-kNrZ] [-F format] [-f filter] [-K key-format] "
		 "[-O sort-order] " CMD_TARGET_PANE_USAGE " [template]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_customize_mode_entry = {
	.name = "customize-mode",
	.description = "Browse and edit options and key bindings.",
	.alias = NULL,

	.args = { "F:f:kNt:yZ", 0, 0, NULL },
	.usage = "[-kNZ] [-F format] [-f filter] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_switch_mode_entry = {
	.name = "switch-mode",
	.description = "Switch the active pane mode.",
	.alias = NULL,

	.args = { "F:f:kO:rst:wZ", 0, 1, cmd_choose_tree_args_parse },
	.usage = "[-krsZw] [-F format] [-f path] [-O sort-order] "
		 CMD_TARGET_PANE_USAGE " [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_choose_tree_exec
};

const struct cmd_entry cmd_display_panes_entry = {
	.name = "display-panes",
	.description = "Display pane numbers.",
	.alias = "displayp",

	.args = { "d:kNPs:t:Z", 0, 1, cmd_choose_tree_args_parse },
	.usage = "[-kNPZ] [-d duration] [-s source-window] "
		 CMD_TARGET_PANE_USAGE " [template]",

	.source = { 's', CMD_FIND_WINDOW, 0 },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
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
	struct client			*c = cmdq_get_client(item);
	struct window_pane		*wp = target->wp;
	const struct window_mode	*mode;
	enum sort_order			 order;
	char				*path;

	order = sort_order_from_string(args_get(args, 'O'));
	if (order == SORT_END && args_has(args, 'O')) {
		cmdq_error(item, "invalid sort order");
		return (CMD_RETURN_ERROR);
	}

	if (cmd_get_entry(self) == &cmd_choose_buffer_entry) {
		if (paste_is_empty())
			return (CMD_RETURN_NORMAL);
		mode = &window_buffer_mode;
	} else if (cmd_get_entry(self) == &cmd_choose_client_entry) {
		if (server_client_how_many() == 0)
			return (CMD_RETURN_NORMAL);
		mode = &window_client_mode;
	} else if (cmd_get_entry(self) == &cmd_customize_mode_entry)
		mode = &window_customize_mode;
	else if (cmd_get_entry(self) == &cmd_switch_mode_entry)
		mode = &window_switch_mode;
	else if (cmd_get_entry(self) == &cmd_display_panes_entry)
		mode = &window_panes_mode;
	else
		mode = &window_tree_mode;

	if (mode == &window_switch_mode && args_has(args, 'f')) {
		path = format_single_from_target(item, args_get(args, 'f'));
		file_read(c, path, cmd_choose_tree_read_done, item);
		free(path);
		return (CMD_RETURN_WAIT);
	}
	if (window_pane_set_mode(wp, NULL, mode, item, target, args) == 0 &&
	    mode == &window_panes_mode &&
	    args_has(args, 'P') &&
	    (c == NULL || c->session == NULL || (c->flags & CLIENT_CONTROL)))
		return (CMD_RETURN_WAIT);
	return (CMD_RETURN_NORMAL);
}

static void
cmd_choose_tree_read_done(__unused struct client *c, const char *path,
    int error, int closed, struct evbuffer *buffer, void *data)
{
	struct cmdq_item		*item = data;
	struct cmd			*cmd = cmdq_get_cmd(item);
	struct args			*args = cmd_get_args(cmd);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct window_pane		*wp = target->wp;
	struct window_mode_entry	*wme;
	char				*buf = EVBUFFER_DATA(buffer);
	size_t				 len = EVBUFFER_LENGTH(buffer);

	if (!closed)
		return;

	if (error != 0) {
		cmdq_error(item, "%s: %s", strerror(error), path);
		goto out;
	}
	if (len != 0 && memchr(buf, '\0', len) != NULL) {
		cmdq_error(item, "%s: contains binary data", path);
		goto out;
	}
	if (!cmd_find_valid_state(target)) {
		cmdq_error(item, "target pane no longer exists");
		goto out;
	}

	TAILQ_FOREACH(wme, &wp->modes, entry) {
		if (wme->mode == &window_switch_mode)
			break;
	}
	if (window_pane_set_mode(wp, NULL, &window_switch_mode, item, target,
	    args) == 0 && wme == NULL)
		window_switch_set_input(TAILQ_FIRST(&wp->modes), buffer);

out:
	cmdq_continue(item);
}
