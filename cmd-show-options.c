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
 * Show options.
 */

static enum cmd_retval	cmd_show_options_exec(struct cmd *, struct cmdq_item *);

static void		cmd_show_options_print(struct cmd *, struct cmdq_item *,
			    struct options_entry *, int);
static enum cmd_retval	cmd_show_options_all(struct cmd *, struct cmdq_item *,
		    	    struct options *);

const struct cmd_entry cmd_show_options_entry = {
	.name = "show-options",
	.alias = "show",

	.args = { "gHqst:vw", 0, 1 },
	.usage = "[-gHqsvw] [-t target-session|target-window] [option]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_options_exec
};

const struct cmd_entry cmd_show_window_options_entry = {
	.name = "show-window-options",
	.alias = "showw",

	.args = { "gvt:", 0, 1 },
	.usage = "[-gv] " CMD_TARGET_WINDOW_USAGE " [option]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_options_exec
};

const struct cmd_entry cmd_show_hooks_entry = {
	.name = "show-hooks",
	.alias = NULL,

	.args = { "gt:", 0, 1 },
	.usage = "[-g] " CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_options_exec
};

static enum cmd_retval
cmd_show_options_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = self->args;
	struct cmd_find_state		*fs = &item->target;
	struct client			*c = cmd_find_client(item, NULL, 1);
	struct session			*s = item->target.s;
	struct winlink			*wl = item->target.wl;
	struct options			*oo;
	enum options_table_scope	 scope;
	char				*argument, *name = NULL, *cause;
	const char			*target;
	int				 window, idx, ambiguous;
	struct options_entry		*o;

	window = (self->entry == &cmd_show_window_options_entry);
	if (args->argc == 0) {
		scope = options_scope_from_flags(args, window, fs, &oo, &cause);
		return (cmd_show_options_all(self, item, oo));
	}
	argument = format_single(item, args->argv[0], c, s, wl, NULL);

	name = options_match(argument, &idx, &ambiguous);
	if (name == NULL) {
		if (args_has(args, 'q'))
			goto fail;
		if (ambiguous)
			cmdq_error(item, "ambiguous option: %s", argument);
		else
			cmdq_error(item, "invalid option: %s", argument);
		goto fail;
	}
	if (*name == '@')
		scope = options_scope_from_flags(args, window, fs, &oo, &cause);
	else {
		if (options_get_only(global_options, name) != NULL)
			scope = OPTIONS_TABLE_SERVER;
		else if (options_get_only(global_s_options, name) != NULL)
			scope = OPTIONS_TABLE_SESSION;
		else if (options_get_only(global_w_options, name) != NULL)
			scope = OPTIONS_TABLE_WINDOW;
		else {
			scope = OPTIONS_TABLE_NONE;
			xasprintf(&cause, "unknown option: %s", argument);
		}
		if (scope == OPTIONS_TABLE_SERVER)
			oo = global_options;
		else if (scope == OPTIONS_TABLE_SESSION) {
			if (args_has(self->args, 'g'))
				oo = global_s_options;
			else if (s == NULL) {
				target = args_get(args, 't');
				if (target != NULL) {
					cmdq_error(item, "no such session: %s",
					    target);
				} else
					cmdq_error(item, "no current session");
				goto fail;
			} else
				oo = s->options;
		} else if (scope == OPTIONS_TABLE_WINDOW) {
			if (args_has(self->args, 'g'))
				oo = global_w_options;
			else if (wl == NULL) {
				target = args_get(args, 't');
				if (target != NULL) {
					cmdq_error(item, "no such window: %s",
					    target);
				} else
					cmdq_error(item, "no current window");
				goto fail;
			} else
				oo = wl->window->options;
		}
	}
	if (scope == OPTIONS_TABLE_NONE) {
		if (args_has(args, 'q'))
			goto fail;
		cmdq_error(item, "%s", cause);
		free(cause);
		goto fail;
	}
	o = options_get_only(oo, name);
	if (o != NULL)
		cmd_show_options_print(self, item, o, idx);

	free(name);
	free(argument);
	return (CMD_RETURN_NORMAL);

fail:
	free(name);
	free(argument);
	return (CMD_RETURN_ERROR);
}

static void
cmd_show_options_print(struct cmd *self, struct cmdq_item *item,
    struct options_entry *o, int idx)
{
	struct options_array_item	*a;
	const char			*name = options_name(o);
	char				*value, *tmp = NULL, *escaped;

	if (idx != -1) {
		xasprintf(&tmp, "%s[%d]", name, idx);
		name = tmp;
	} else {
		if (options_isarray(o)) {
			a = options_array_first(o);
			if (a == NULL) {
				if (!args_has(self->args, 'v'))
					cmdq_print(item, "%s", name);
				return;
			}
			while (a != NULL) {
				idx = options_array_item_index(a);
				cmd_show_options_print(self, item, o, idx);
				a = options_array_next(a);
			}
			return;
		}
	}

	value = options_tostring(o, idx, 0);
	if (args_has(self->args, 'v'))
		cmdq_print(item, "%s", value);
	else if (options_isstring(o)) {
		utf8_stravis(&escaped, value, VIS_OCTAL|VIS_TAB|VIS_NL|VIS_DQ);
		cmdq_print(item, "%s \"%s\"", name, escaped);
		free(escaped);
	} else
		cmdq_print(item, "%s %s", name, value);
	free(value);

	free(tmp);
}

static enum cmd_retval
cmd_show_options_all(struct cmd *self, struct cmdq_item *item,
    struct options *oo)
{
	struct options_entry			*o;
	struct options_array_item		*a;
	u_int					 idx;
	const struct options_table_entry	*oe;

	o = options_first(oo);
	while (o != NULL) {
		oe = options_table_entry(o);
		if ((self->entry != &cmd_show_hooks_entry &&
		    !args_has(self->args, 'H') &&
		    oe != NULL &&
		    (oe->flags & OPTIONS_TABLE_IS_HOOK)) ||
		    (self->entry == &cmd_show_hooks_entry &&
		    (oe == NULL ||
		    (~oe->flags & OPTIONS_TABLE_IS_HOOK)))) {
			o = options_next(o);
			continue;
		}
		if (!options_isarray(o))
			cmd_show_options_print(self, item, o, -1);
		else if ((a = options_array_first(o)) == NULL) {
			if (!args_has(self->args, 'v'))
				cmdq_print(item, "%s", options_name(o));
		} else {
			while (a != NULL) {
				idx = options_array_item_index(a);
				cmd_show_options_print(self, item, o, idx);
				a = options_array_next(a);
			}
		}
		o = options_next(o);
	}
	return (CMD_RETURN_NORMAL);
}
