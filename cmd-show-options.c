/* $OpenBSD: cmd-show-options.c,v 1.74 2026/07/22 18:58:48 nicm Exp $ */

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
#include <vis.h>

#include "tmux.h"

/*
 * Show options.
 */

static enum cmd_retval	cmd_show_options_exec(struct cmd *, struct cmdq_item *);

static void		cmd_show_options_print(struct cmd *, struct cmdq_item *,
			    struct options_entry *, const char *, int);
static void		cmd_show_hooks_print_monitor(struct cmdq_item *,
			    struct options_entry *);
static enum cmd_retval	cmd_show_hooks_monitor(struct cmd *, struct cmdq_item *,
			    int, struct options *);
static enum cmd_retval	cmd_show_options_all(struct cmd *, struct cmdq_item *,
			    int, struct options *);

const struct cmd_entry cmd_show_options_entry = {
	.name = "show-options",
	.alias = "show",

	.args = { "AgHpqst:vw", 0, 1, NULL },
	.usage = "[-AgHpqsvw] " CMD_TARGET_PANE_USAGE " [option]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_options_exec
};

const struct cmd_entry cmd_show_window_options_entry = {
	.name = "show-window-options",
	.alias = "showw",

	.args = { "gvt:", 0, 1, NULL },
	.usage = "[-gv] " CMD_TARGET_WINDOW_USAGE " [option]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_options_exec
};

const struct cmd_entry cmd_show_hooks_entry = {
	.name = "show-hooks",
	.alias = NULL,

	.args = { "Bgpt:w", 0, 1, NULL },
	.usage = "[-Bgpw] " CMD_TARGET_PANE_USAGE " [hook]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_options_exec
};

static enum cmd_retval
cmd_show_options_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct options			*oo;
	char				*argument, *name = NULL, *cause;
	char				*array_key = NULL;
	int				 window, ambiguous, parent, scope;
	struct options_entry		*o;

	window = (cmd_get_entry(self) == &cmd_show_window_options_entry);

	if (args_count(args) == 0) {
		scope = options_scope_from_flags(args, window, target, &oo,
		    &cause);
		if (scope == OPTIONS_TABLE_NONE) {
			if (args_has(args, 'q'))
				return (CMD_RETURN_NORMAL);
			cmdq_error(item, "%s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (cmd_get_entry(self) == &cmd_show_hooks_entry &&
		    args_has(args, 'B'))
			return (cmd_show_hooks_monitor(self, item, scope, oo));
		return (cmd_show_options_all(self, item, scope, oo));
	}
	argument = format_single_from_target(item, args_string(args, 0));

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
	if (args_has(args, 'A') && o == NULL) {
		o = options_get(oo, name);
		parent = 1;
	} else
		parent = 0;
	if (o != NULL) {
		if (cmd_get_entry(self) == &cmd_show_hooks_entry &&
		    args_has(args, 'B'))
			cmd_show_hooks_print_monitor(item, o);
		else
			cmd_show_options_print(self, item, o, array_key, parent);
	}
	else if (*name == '@') {
		if (args_has(args, 'q'))
			goto out;
		cmdq_error(item, "invalid option: %s", argument);
		goto fail;
	}

out:
	free(name);
	free(array_key);
	free(argument);
	return (CMD_RETURN_NORMAL);

fail:
	free(name);
	free(array_key);
	free(argument);
	return (CMD_RETURN_ERROR);
}

static void
cmd_show_options_print(struct cmd *self, struct cmdq_item *item,
    struct options_entry *o, const char *array_key, int parent)
{
	struct args			*args = cmd_get_args(self);
	struct options_array_item	*a;
	const char			*name = options_name(o);
	char				*value, *tmp = NULL, *escaped;

	if (array_key != NULL) {
		xasprintf(&tmp, "%s[%s]", name, array_key);
		name = tmp;
	} else {
		if (options_is_array(o)) {
			a = options_array_first(o);
			if (a == NULL) {
				if (!args_has(args, 'v'))
					cmdq_print(item, "%s", name);
				return;
			}
			while (a != NULL) {
				array_key = options_array_item_key(a);
				cmd_show_options_print(self, item, o, array_key,
				    parent);
				a = options_array_next(a);
			}
			return;
		}
	}

	value = options_to_string(o, array_key, 0);
	if (args_has(args, 'v'))
		cmdq_print(item, "%s", value);
	else if (options_is_string(o)) {
		escaped = args_escape(value);
		if (parent)
			cmdq_print(item, "%s* %s", name, escaped);
		else
			cmdq_print(item, "%s %s", name, escaped);
		free(escaped);
	} else {
		if (parent)
			cmdq_print(item, "%s* %s", name, value);
		else
			cmdq_print(item, "%s %s", name, value);
	}
	free(value);

	free(tmp);
}

static void
cmd_show_hooks_print_monitor(struct cmdq_item *item, struct options_entry *o)
{
	char	*value;

	value = hooks_monitor_to_string(o);
	if (value == NULL)
		return;
	cmdq_print(item, "%s", value);
	free(value);
}

/* Show all hook monitors. */
static enum cmd_retval
cmd_show_hooks_monitor(__unused struct cmd *self, struct cmdq_item *item,
    __unused int scope, struct options *oo)
{
	struct options_entry	*o;

	o = options_first(oo);
	while (o != NULL) {
		cmd_show_hooks_print_monitor(item, o);
		o = options_next(o);
	}
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_show_options_all(struct cmd *self, struct cmdq_item *item, int scope,
    struct options *oo)
{
	struct args				*args = cmd_get_args(self);
	const struct options_table_entry	*oe;
	struct options_entry			*o;
	struct options_array_item		*a;
	const char				*name, *array_key;
	int					 parent;

	o = options_first(oo);
	while (o != NULL) {
		if (options_table_entry(o) == NULL) {
			name = options_name(o);
			if (cmd_get_entry(self) != &cmd_show_hooks_entry)
				cmd_show_options_print(self, item, o, NULL, 0);
			else if (*name == '@' && (hooks_is_event(name) ||
			    options_get_monitor_data(o) != NULL))
				cmd_show_options_print(self, item, o, NULL, 0);
		}
		o = options_next(o);
	}
	for (oe = options_table; oe->name != NULL; oe++) {
		if (~oe->scope & scope)
			continue;

		if ((cmd_get_entry(self) != &cmd_show_hooks_entry &&
		    !args_has(args, 'H') &&
		    (oe->flags & OPTIONS_TABLE_IS_HOOK)) ||
		    (cmd_get_entry(self) == &cmd_show_hooks_entry &&
		    (~oe->flags & OPTIONS_TABLE_IS_HOOK)))
			continue;

		o = options_get_only(oo, oe->name);
		if (o == NULL) {
			if (!args_has(args, 'A'))
				continue;
			o = options_get(oo, oe->name);
			if (o == NULL)
				continue;
			parent = 1;
		} else
			parent = 0;

		if (!options_is_array(o))
			cmd_show_options_print(self, item, o, NULL, parent);
		else if ((a = options_array_first(o)) == NULL) {
			if (!args_has(args, 'v')) {
				name = options_name(o);
				if (parent)
					cmdq_print(item, "%s*", name);
				else
					cmdq_print(item, "%s", name);
			}
		} else {
			while (a != NULL) {
				array_key = options_array_item_key(a);
				cmd_show_options_print(self, item, o, array_key,
				    parent);
				a = options_array_next(a);
			}
		}
	}
	return (CMD_RETURN_NORMAL);
}
