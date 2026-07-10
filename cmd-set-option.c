/* $OpenBSD: cmd-set-option.c,v 1.145 2026/07/10 13:38:45 nicm Exp $ */

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

static enum args_parse_type	cmd_set_option_args_parse(struct args *,
				    u_int, char **);
static enum cmd_retval		cmd_set_option_exec(struct cmd *,
				    struct cmdq_item *);
static enum cmd_retval		cmd_set_hook_event_exec(struct cmd *,
				    struct cmdq_item *);
static enum cmd_retval		cmd_set_hook_monitor_exec(struct cmdq_item *,
				    struct args *, int);

const struct cmd_entry cmd_set_option_entry = {
	.name = "set-option",
	.alias = "set",

	.args = { "aFgopqst:uUw", 1, 2, cmd_set_option_args_parse },
	.usage = "[-aFgopqsuUw] " CMD_TARGET_PANE_USAGE " option [value]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

const struct cmd_entry cmd_set_window_option_entry = {
	.name = "set-window-option",
	.alias = "setw",

	.args = { "aFgoqt:u", 1, 2, cmd_set_option_args_parse },
	.usage = "[-aFgoqu] " CMD_TARGET_WINDOW_USAGE " option [value]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

const struct cmd_entry cmd_set_hook_entry = {
	.name = "set-hook",
	.alias = NULL,

	.args = { "agpERt:uB:w", 0, 2, cmd_set_option_args_parse },
	.usage = "[-agpERuw] [-B name:what:format] " CMD_TARGET_PANE_USAGE " "
		 "[hook] [command]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_set_option_exec
};

static enum args_parse_type
cmd_set_option_args_parse(struct args *args, u_int idx,
    __unused char **cause)
{
	if (args_has(args, 'B'))
		return (ARGS_PARSE_COMMANDS_OR_STRING);
	if (idx == 1)
		return (ARGS_PARSE_COMMANDS_OR_STRING);
	return (ARGS_PARSE_STRING);
}

static enum cmd_retval
cmd_set_hook_event_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct event_payload	*ep;
	struct client		*c;
	char			*argument;

	if (args_count(args) == 0) {
		cmdq_error(item, "missing argument");
		return (CMD_RETURN_ERROR);
	}
	if (args_count(args) != 1) {
		cmdq_error(item, "too many arguments");
		return (CMD_RETURN_ERROR);
	}

	argument = format_single_from_target(item, args_string(args, 0));
	if (*argument != '@') {
		cmdq_error(item, "event name must start with @");
		free(argument);
		return (CMD_RETURN_ERROR);
	}

	ep = event_payload_create();
	event_payload_set_target(ep, target);
	c = cmdq_get_client(item);
	if (c != NULL)
		event_payload_set_client(ep, "client", c);
	if (target->s != NULL)
		event_payload_set_session(ep, "session", target->s);
	if (target->w != NULL)
		event_payload_set_window(ep, "window", target->w);
	if (target->wl != NULL)
		event_payload_set_int(ep, "window_index", target->wl->idx);
	else if (target->idx != -1)
		event_payload_set_int(ep, "window_index", target->idx);
	if (target->wp != NULL)
		event_payload_set_pane(ep, "pane", target->wp);
	events_fire(argument, ep);
	free(argument);
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_set_hook_monitor_exec(struct cmdq_item *item, struct args *args, int window)
{
	struct cmd_find_state	*target = cmdq_get_target(item), fs;
	struct options		*oo;
	struct options_entry	*o;
	struct session		*s = NULL;
	char			*cause = NULL, *name = NULL, *format = NULL;
	char			*expanded = NULL, *newvalue = NULL;
	const char		*value, *old;
	enum monitor_type	 type;
	int			 id, scope;

	if (args_count(args) > 1) {
		cmdq_error(item, "too many arguments");
		return (CMD_RETURN_ERROR);
	}

	value = args_get(args, 'B');
	if (args_has(args, 'u')) {
		if (monitor_parse(value, &name, &type, &id, &format) != 0)
			name = xstrdup(value);
		free(format);
		format = NULL;
	} else {
		if (monitor_parse(value, &name, &type, &id, &format) != 0) {
			cmdq_error(item, "invalid subscription: %s", value);
			return (CMD_RETURN_ERROR);
		}
	}

	if (*name != '@') {
		cmdq_error(item, "monitor hook name must start with @");
		goto fail;
	}

	scope = options_scope_from_name(args, window, name, target, &oo,
	    &cause);
	if (scope == OPTIONS_TABLE_NONE) {
		cmdq_error(item, "%s", cause);
		free(cause);
		goto fail;
	}
	cmd_find_copy_state(&fs, target);

	if (args_has(args, 'u')) {
		hooks_monitor_remove(oo, name);
		goto out;
	}

	if (args_count(args) != 0) {
		value = args_string(args, 0);
		if (args_has(args, 'F')) {
			expanded = format_single_from_target(item, value);
			value = expanded;
		}
		o = options_get_only(oo, name);
		if (!args_has(args, 'o') || o == NULL) {
			if (args_has(args, 'a') && o != NULL) {
				old = options_get_string(oo, name);
				xasprintf(&newvalue, "%s%s", old, value);
				value = newvalue;
			}
			options_set_string(oo, name, 0, "%s", value);
			options_push_changes(name);
		}
	}

	if (oo != global_options &&
	    oo != global_s_options &&
	    oo != global_w_options)
		s = target->s;
	hooks_monitor_add(item, oo, name, type, id, format, &fs, s);

out:
	free(newvalue);
	free(expanded);
	free(name);
	free(format);
	return (CMD_RETURN_NORMAL);

fail:
	free(newvalue);
	free(expanded);
	free(name);
	free(format);
	return (CMD_RETURN_ERROR);
}

static enum cmd_retval
cmd_set_option_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	int				 append = args_has(args, 'a');
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct window_pane		*loop;
	struct options			*oo;
	struct options_entry		*parent, *o, *po;
	char				*name, *argument, *cause;
	char				*expanded = NULL, *array_key = NULL;
	const char			*value;
	int				 window, already, error, ambiguous;
	int				 scope;

	window = (cmd_get_entry(self) == &cmd_set_window_option_entry);
	if (cmd_get_entry(self) == &cmd_set_hook_entry && args_has(args, 'E'))
		return (cmd_set_hook_event_exec(self, item));
	if (cmd_get_entry(self) == &cmd_set_hook_entry && args_has(args, 'B'))
		return (cmd_set_hook_monitor_exec(item, args, window));
	if (args_count(args) == 0) {
		cmdq_error(item, "missing argument");
		return (CMD_RETURN_ERROR);
	}

	/* Expand argument. */
	argument = format_single_from_target(item, args_string(args, 0));

	/* If set-hook -R, fire the hook straight away. */
	if (cmd_get_entry(self) == &cmd_set_hook_entry && args_has(args, 'R')) {
		hooks_run(item, argument);
		free(argument);
		return (CMD_RETURN_NORMAL);
	}

	/* Parse option name and array key. */
	name = options_match(argument, &array_key, &ambiguous);
	if (name == NULL) {
		if (args_has(args, 'q'))
			goto out;
		if (ambiguous)
			cmdq_error(item, "ambiguous option: %s", argument);
		else
			cmdq_error(item, "invalid option: %s", argument);
		goto fail;
	}
	if (args_count(args) < 2)
		value = NULL;
	else
		value = args_string(args, 1);
	if (value != NULL && args_has(args, 'F')) {
		expanded = format_single_from_target(item, value);
		value = expanded;
	}

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

	/* Check that array options and keys match up. */
	if (array_key != NULL && (*name == '@' || !options_is_array(parent))) {
		cmdq_error(item, "not an array: %s", argument);
		goto fail;
	}

	/* With -o, check this option is not already set. */
	if (!args_has(args, 'u') && args_has(args, 'o')) {
		if (array_key == NULL)
			already = (o != NULL);
		else {
			if (o == NULL)
				already = 0;
			else if (options_array_get(o, array_key) != NULL)
				already = 1;
			else
				already = 0;
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
			if (options_remove_or_default(po, array_key,
			    &cause) != 0) {
				cmdq_error(item, "%s", cause);
				free(cause);
				goto fail;
			}
		}
	}
	if (args_has(args, 'u') || args_has(args, 'U')) {
		if (o == NULL)
			goto out;
		if (options_remove_or_default(o, array_key, &cause) != 0) {
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
		if (cmd_get_entry(self) == &cmd_set_hook_entry)
			hooks_add_event(name);
	} else if (array_key == NULL && !options_is_array(parent)) {
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
		if (array_key == NULL) {
			if (!append)
				options_array_clear(o);
			if (options_array_assign(o, value, &cause) != 0) {
				cmdq_error(item, "%s", cause);
				free(cause);
				goto fail;
			}
		} else if (options_array_set(o, array_key, value, append,
		    &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			goto fail;
		}
	}

	options_push_changes(name);

out:
	free(argument);
	free(expanded);
	free(name);
	free(array_key);
	return (CMD_RETURN_NORMAL);

fail:
	free(argument);
	free(expanded);
	free(name);
	free(array_key);
	return (CMD_RETURN_ERROR);
}
