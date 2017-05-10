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

static enum cmd_retval	cmd_show_options_one(struct cmd *, struct cmdq_item *,
			    struct options *);
static enum cmd_retval	cmd_show_options_all(struct cmd *, struct cmdq_item *,
		    	    struct options *);

const struct cmd_entry cmd_show_options_entry = {
	.name = "show-options",
	.alias = "show",

	.args = { "gqst:vw", 0, 1 },
	.usage = "[-gqsvw] [-t target-session|target-window] [option]",

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

static enum cmd_retval
cmd_show_options_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = self->args;
	struct cmd_find_state		*fs = &item->target;
	struct options			*oo;
	enum options_table_scope	 scope;
	char				*cause;
	int				 window;

	window = (self->entry == &cmd_show_window_options_entry);
	scope = options_scope_from_flags(args, window, fs, &oo, &cause);
	if (scope == OPTIONS_TABLE_NONE) {
		cmdq_error(item, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	if (args->argc == 0)
		return (cmd_show_options_all(self, item, oo));
	else
		return (cmd_show_options_one(self, item, oo));
}

static void
cmd_show_options_print(struct cmd *self, struct cmdq_item *item,
    struct options_entry *o, int idx)
{
	const char	*name;
	const char	*value;
	char		*tmp, *escaped;
	u_int		 size, i;

	if (idx != -1) {
		xasprintf(&tmp, "%s[%d]", options_name(o), idx);
		name = tmp;
	} else {
		if (options_array_size(o, &size) != -1) {
			for (i = 0; i < size; i++) {
				if (options_array_get(o, i) == NULL)
					continue;
				cmd_show_options_print(self, item, o, i);
			}
			return;
		}
		tmp = NULL;
		name = options_name(o);
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

	free(tmp);
}

static enum cmd_retval
cmd_show_options_one(struct cmd *self, struct cmdq_item *item,
    struct options *oo)
{
	struct args		*args = self->args;
	struct client		*c = cmd_find_client(item, NULL, 1);
	struct session		*s = item->target.s;
	struct winlink		*wl = item->target.wl;
	struct options_entry	*o;
	int			 idx, ambiguous;
	char			*name;

	name = format_single(item, args->argv[0], c, s, wl, NULL);
	o = options_match_get(oo, name, &idx, 1, &ambiguous);
	if (o == NULL) {
		if (args_has(args, 'q')) {
			free(name);
			return (CMD_RETURN_NORMAL);
		}
		if (ambiguous) {
			cmdq_error(item, "ambiguous option: %s", name);
			free(name);
			return (CMD_RETURN_ERROR);
		}
		if (*name != '@' &&
		    options_match_get(oo, name, &idx, 0, &ambiguous) != NULL) {
			free(name);
			return (CMD_RETURN_NORMAL);
		}
		cmdq_error(item, "unknown option: %s", name);
		free(name);
		return (CMD_RETURN_ERROR);
	}
	cmd_show_options_print(self, item, o, idx);
	free(name);
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_show_options_all(struct cmd *self, struct cmdq_item *item,
    struct options *oo)
{
	struct options_entry			 *o;
	const struct options_table_entry	*oe;
	u_int					 size, idx;

	o = options_first(oo);
	while (o != NULL) {
		oe = options_table_entry(o);
		if (oe != NULL && oe->style != NULL) {
			o = options_next(o);
			continue;
		}
		if (options_array_size(o, &size) == -1)
			cmd_show_options_print(self, item, o, -1);
		else {
			for (idx = 0; idx < size; idx++) {
				if (options_array_get(o, idx) == NULL)
					continue;
				cmd_show_options_print(self, item, o, idx);
			}
		}
		o = options_next(o);
	}
	return (CMD_RETURN_NORMAL);
}
