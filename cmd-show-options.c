/* $OpenBSD: cmd-show-options.c,v 1.75 2026/07/22 20:12:58 nicm Exp $ */

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

#define SHOW_OPTIONS_TEMPLATE						\
	"#{?option_value_only,"						\
	"#{option_value},"						\
	"#{option_name}#{?option_has_array_key,"			\
	"[#{option_array_key}],}"					\
	"#{?option_is_parent,*,}"					\
	"#{?option_has_value, "						\
	"#{?option_is_string,#{q/a:option_value},#{option_value}},}}"
#define SHOW_HOOKS_MONITOR_TEMPLATE					\
	"#{option_name}:#{hook_monitor_target}:#{hook_monitor_format}"

static enum cmd_retval	cmd_show_options_exec(struct cmd *, struct cmdq_item *);

static void		cmd_show_options_print(struct cmd *, struct cmdq_item *,
			    struct options_entry *, const char *, int);
static void		cmd_show_hooks_print_monitor(struct cmd *,
			    struct cmdq_item *, struct options_entry *);
static enum cmd_retval	cmd_show_options_all(struct cmd *, struct cmdq_item *,
			    int, struct options *);

const struct cmd_entry cmd_show_options_entry = {
	.name = "show-options",
	.alias = "show",

	.args = { "AgF:Hpqst:vw", 0, 1, NULL },
	.usage = "[-AgHpqsvw] [-F format] " CMD_TARGET_PANE_USAGE " [option]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_options_exec
};

const struct cmd_entry cmd_show_window_options_entry = {
	.name = "show-window-options",
	.alias = "showw",

	.args = { "F:gvt:", 0, 1, NULL },
	.usage = "[-gv] [-F format] " CMD_TARGET_WINDOW_USAGE " [option]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_options_exec
};

const struct cmd_entry cmd_show_hooks_entry = {
	.name = "show-hooks",
	.alias = NULL,

	.args = { "BF:gpt:w", 0, 1, NULL },
	.usage = "[-Bgpw] [-F format] " CMD_TARGET_PANE_USAGE " [hook]",

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
	int				 window, ambiguous, parent, print_parent, scope;
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
		    args_has(args, 'B')) {
			o = options_first(oo);
			while (o != NULL) {
				cmd_show_hooks_print_monitor(self, item, o);
				o = options_next(o);
			}
			return (CMD_RETURN_NORMAL);
		}
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
			cmd_show_hooks_print_monitor(self, item, o);
		else {
			print_parent = parent;
			if (array_key == NULL && options_is_array(o) &&
			    options_array_first(o) == NULL) {
				print_parent = 0;
			}
			cmd_show_options_print(self, item, o, array_key,
			    print_parent);
		}
	} else if (*name == '@') {
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
	struct args			 *args = cmd_get_args(self);
	struct options_array_item	 *a;
	struct format_tree		 *ft;
	const char			 *name = options_name(o);
	const char			 *template = args_get(args, 'F');
	char				 *value, *line;
	int				  is_hook = 0, is_user = 0;
	int				  has_value = 1;
	const struct options_table_entry *oe = options_table_entry(o);

	if (array_key != NULL)
		value = options_to_string(o, array_key, 0);
	else if (options_is_array(o)) {
		a = options_array_first(o);
		if (a != NULL) {
			while (a != NULL) {
				array_key = options_array_item_key(a);
				cmd_show_options_print(self, item, o, array_key,
				    parent);
				a = options_array_next(a);
			}
			return;
		}
		if (template == NULL && args_has(args, 'v'))
			return;
		value = xstrdup("");
		has_value = 0;
	} else
		value = options_to_string(o, NULL, 0);

	if (template == NULL)
		template = SHOW_OPTIONS_TEMPLATE;

	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_HOOK))
		is_hook = 1;
	else if (oe == NULL)
		is_user = 1;

	ft = format_create_from_target(item);
	format_add(ft, "option_name", "%s", name);
	format_add(ft, "option_value", "%s", value);
	format_add(ft, "option_value_only", "%d", args_has(args, 'v'));
	format_add(ft, "option_is_parent", "%d", parent);
	format_add(ft, "option_is_array", "%d", options_is_array(o));
	format_add(ft, "option_is_string", "%d", options_is_string(o));
	format_add(ft, "option_is_hook", "%d", is_hook);
	format_add(ft, "option_is_user", "%d", is_user);
	format_add(ft, "option_has_value", "%d", has_value);
	if (array_key != NULL) {
		format_add(ft, "option_array_key", "%s", array_key);
		format_add(ft, "option_has_array_key", "1");
	} else {
		format_add(ft, "option_array_key", "%s", "");
		format_add(ft, "option_has_array_key", "0");
	}
	line = format_expand(ft, template);
	format_free(ft);

	cmdq_print(item, "%s", line);
	free(line);
	free(value);
}

static void
cmd_show_hooks_print_monitor(struct cmd *self, struct cmdq_item *item,
    struct options_entry *o)
{
	struct args		*args = cmd_get_args(self);
	struct format_tree	*ft;
	enum monitor_type	 type;
	const char		*template = args_get(args, 'F'), *format;
	char			*value, *target, *line;
	int			 id;

	value = hooks_monitor_to_string(o);
	if (value == NULL)
		return;
	if (!hooks_monitor_get(o, &type, &id, &format)) {
		free(value);
		return;
	}
	if (template == NULL)
		template = SHOW_HOOKS_MONITOR_TEMPLATE;

	switch (type) {
	case MONITOR_SESSION:
		target = xstrdup("");
		break;
	case MONITOR_PANE:
		xasprintf(&target, "%%%d", id);
		break;
	case MONITOR_ALL_PANES:
		target = xstrdup("%*");
		break;
	case MONITOR_WINDOW:
		xasprintf(&target, "@%d", id);
		break;
	case MONITOR_ALL_WINDOWS:
		target = xstrdup("@*");
		break;
	}

	ft = format_create_from_target(item);
	format_add(ft, "option_name", "%s", options_name(o));
	format_add(ft, "option_value", "%s", value);
	format_add(ft, "option_value_only", "%d", 0);
	format_add(ft, "option_is_parent", "%d", 0);
	format_add(ft, "option_is_array", "%d", 0);
	format_add(ft, "option_is_string", "%d", 1);
	format_add(ft, "option_is_hook", "%d", 1);
	format_add(ft, "option_is_user", "%d", 1);
	format_add(ft, "option_has_value", "%d", 1);
	format_add(ft, "option_array_key", "%s", "");
	format_add(ft, "option_has_array_key", "0");
	format_add(ft, "hook_monitor_target", "%s", target);
	format_add(ft, "hook_monitor_format", "%s", format);
	line = format_expand(ft, template);
	format_free(ft);

	cmdq_print(item, "%s", line);
	free(line);
	free(target);
	free(value);
}

static enum cmd_retval
cmd_show_options_all(struct cmd *self, struct cmdq_item *item, int scope,
    struct options *oo)
{
	struct args				*args = cmd_get_args(self);
	const struct options_table_entry	*oe;
	struct options_entry			*o;
	const char				*name;
	int					 parent, is_user_hook;

	o = options_first(oo);
	while (o != NULL) {
		if (options_table_entry(o) == NULL) {
			name = options_name(o);
			is_user_hook = 0;
			if (*name == '@') {
				if (hooks_is_event(name) ||
				    options_get_monitor_data(o) != NULL) {
					is_user_hook = 1;
				}
			}
			if (cmd_get_entry(self) != &cmd_show_hooks_entry) {
				if (!is_user_hook || args_has(args, 'H')) {
					cmd_show_options_print(self, item, o,
					    NULL, 0);
				}
			} else if (is_user_hook)
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

		cmd_show_options_print(self, item, o, NULL, parent);
	}
	return (CMD_RETURN_NORMAL);
}
