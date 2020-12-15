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
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Set an option.
 */

static enum cmd_retval	cmd_set_option_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_set_option_entry = {
	.name = "set-option",
	.alias = "set",

	.args = { "aFgopqst:uUw", 1, 2 },
	.usage = "[-aFgopqsuUw] " CMD_TARGET_PANE_USAGE " option [value]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

const struct cmd_entry cmd_set_window_option_entry = {
	.name = "set-window-option",
	.alias = "setw",

	.args = { "aFgoqt:u", 1, 2 },
	.usage = "[-aFgoqu] " CMD_TARGET_WINDOW_USAGE " option [value]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

const struct cmd_entry cmd_set_hook_entry = {
	.name = "set-hook",
	.alias = NULL,

	.args = { "agpRt:uw", 1, 2 },
	.usage = "[-agpRuw] " CMD_TARGET_PANE_USAGE " hook [command]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

static enum cmd_retval
cmd_set_option_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	int				 append = args_has(args, 'a');
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct window_pane		*loop;
	struct options			*oo;
	struct options_entry		*parent, *o, *po;
	char				*name, *argument, *value = NULL, *cause;
	int				 window, idx, already, error, ambiguous;
	int				 scope;

	window = (cmd_get_entry(self) == &cmd_set_window_option_entry);

	/* Expand argument. */
	argument = format_single_from_target(item, args->argv[0]);

	/* If set-hook -R, fire the hook straight away. */
	if (cmd_get_entry(self) == &cmd_set_hook_entry && args_has(args, 'R')) {
		notify_hook(item, argument);
		free(argument);
		return (CMD_RETURN_NORMAL);
	}

	/* Parse option name and index. */
	name = options_match(argument, &idx, &ambiguous);
	if (name == NULL) {
		if (args_has(args, 'q'))
			goto out;
		if (ambiguous)
			cmdq_error(item, "ambiguous option: %s", argument);
		else
			cmdq_error(item, "invalid option: %s", argument);
		goto fail;
	}
	if (args->argc < 2)
		value = NULL;
	else if (args_has(args, 'F'))
		value = format_single_from_target(item, args->argv[1]);
	else
		value = xstrdup(args->argv[1]);

	/* Get the scope and table for the option .*/
	scope = options_scope_from_name(args, window, name, target, &oo,
	    &cause);
	if (scope == OPTIONS_TABLE_NONE) {
		if (args_has(args, 'q'))
			goto out;
		cmdq_error(item, "%s", cause);
		free(cause);
		goto fail;
	}
	o = options_get_only(oo, name);
	parent = options_get(oo, name);

	/* Check that array options and indexes match up. */
	if (idx != -1 && (*name == '@' || !options_is_array(parent))) {
		cmdq_error(item, "not an array: %s", argument);
		goto fail;
	}

	/* With -o, check this option is not already set. */
	if (!args_has(args, 'u') && args_has(args, 'o')) {
		if (idx == -1)
			already = (o != NULL);
		else {
			if (o == NULL)
				already = 0;
			else
				already = (options_array_get(o, idx) != NULL);
		}
		if (already) {
			if (args_has(args, 'q'))
				goto out;
			cmdq_error(item, "already set: %s", argument);
			goto fail;
		}
	}

	/* Change the option. */
	if (args_has(args, 'U') && scope == OPTIONS_TABLE_WINDOW) {
		TAILQ_FOREACH(loop, &target->w->panes, entry) {
			po = options_get_only(loop->options, name);
			if (po == NULL)
				continue;
			if (options_remove_or_default(po, idx, &cause) != 0) {
				cmdq_error(item, "%s", cause);
				free(cause);
				goto fail;
			}
		}
	}
	if (args_has(args, 'u') || args_has(args, 'U')) {
		if (o == NULL)
			goto out;
		if (options_remove_or_default(o, idx, &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			goto fail;
		}
	} else if (*name == '@') {
		if (value == NULL) {
			cmdq_error(item, "empty value");
			goto fail;
		}
		options_set_string(oo, name, append, "%s", value);
	} else if (idx == -1 && !options_is_array(parent)) {
		error = options_from_string(oo, options_table_entry(parent),
		    options_table_entry(parent)->name, value,
		    args_has(args, 'a'), &cause);
		if (error != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			goto fail;
		}
	} else {
		if (value == NULL) {
			cmdq_error(item, "empty value");
			goto fail;
		}
		if (o == NULL)
			o = options_empty(oo, options_table_entry(parent));
		if (idx == -1) {
			if (!append)
				options_array_clear(o);
			if (options_array_assign(o, value, &cause) != 0) {
				cmdq_error(item, "%s", cause);
				free(cause);
				goto fail;
			}
		} else if (options_array_set(o, idx, value, append,
		    &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			goto fail;
		}
	}

	options_push_changes(name);

out:
	free(argument);
	free(value);
	free(name);
	return (CMD_RETURN_NORMAL);

fail:
	free(argument);
	free(value);
	free(name);
	return (CMD_RETURN_ERROR);
}
