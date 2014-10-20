/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

enum cmd_retval	 cmd_show_options_exec(struct cmd *, struct cmd_q *);

enum cmd_retval	cmd_show_options_one(struct cmd *, struct cmd_q *,
		    struct options *, int);
enum cmd_retval cmd_show_options_all(struct cmd *, struct cmd_q *,
		    const struct options_table_entry *, struct options *);

const struct cmd_entry cmd_show_options_entry = {
	"show-options", "show",
	"gqst:vw", 0, 1,
	"[-gqsvw] [-t target-session|target-window] [option]",
	0,
	cmd_show_options_exec
};

const struct cmd_entry cmd_show_window_options_entry = {
	"show-window-options", "showw",
	"gvt:", 0, 1,
	"[-gv] " CMD_TARGET_WINDOW_USAGE " [option]",
	0,
	cmd_show_options_exec
};

enum cmd_retval
cmd_show_options_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args				*args = self->args;
	struct session				*s;
	struct winlink				*wl;
	const struct options_table_entry	*table;
	struct options				*oo;
	int					 quiet;

	if (args_has(self->args, 's')) {
		oo = &global_options;
		table = server_options_table;
	} else if (args_has(self->args, 'w') ||
	    self->entry == &cmd_show_window_options_entry) {
		table = window_options_table;
		if (args_has(self->args, 'g'))
			oo = &global_w_options;
		else {
			wl = cmd_find_window(cmdq, args_get(args, 't'), NULL);
			if (wl == NULL)
				return (CMD_RETURN_ERROR);
			oo = &wl->window->options;
		}
	} else {
		table = session_options_table;
		if (args_has(self->args, 'g'))
			oo = &global_s_options;
		else {
			s = cmd_find_session(cmdq, args_get(args, 't'), 0);
			if (s == NULL)
				return (CMD_RETURN_ERROR);
			oo = &s->options;
		}
	}

	quiet = args_has(self->args, 'q');
	if (args->argc == 0)
		return (cmd_show_options_all(self, cmdq, table, oo));
	else
		return (cmd_show_options_one(self, cmdq, oo, quiet));
}

enum cmd_retval
cmd_show_options_one(struct cmd *self, struct cmd_q *cmdq,
    struct options *oo, int quiet)
{
	struct args				*args = self->args;
	const char				*name = args->argv[0];
	const struct options_table_entry	*table, *oe;
	struct options_entry			*o;
	const char				*optval;

retry:
	if (*name == '@') {
		if ((o = options_find1(oo, name)) == NULL) {
			if (quiet)
				return (CMD_RETURN_NORMAL);
			cmdq_error(cmdq, "unknown option: %s", name);
			return (CMD_RETURN_ERROR);
		}
		if (args_has(self->args, 'v'))
			cmdq_print(cmdq, "%s", o->str);
		else
			cmdq_print(cmdq, "%s \"%s\"", o->name, o->str);
		return (CMD_RETURN_NORMAL);
	}

	table = oe = NULL;
	if (options_table_find(name, &table, &oe) != 0) {
		cmdq_error(cmdq, "ambiguous option: %s", name);
		return (CMD_RETURN_ERROR);
	}
	if (oe == NULL) {
		if (quiet)
		    return (CMD_RETURN_NORMAL);
		cmdq_error(cmdq, "unknown option: %s", name);
		return (CMD_RETURN_ERROR);
	}
	if (oe->style != NULL) {
		name = oe->style;
		goto retry;
	}
	if ((o = options_find1(oo, oe->name)) == NULL)
		return (CMD_RETURN_NORMAL);
	optval = options_table_print_entry(oe, o, args_has(self->args, 'v'));
	if (args_has(self->args, 'v'))
		cmdq_print(cmdq, "%s", optval);
	else
		cmdq_print(cmdq, "%s %s", oe->name, optval);
	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_show_options_all(struct cmd *self, struct cmd_q *cmdq,
    const struct options_table_entry *table, struct options *oo)
{
	const struct options_table_entry	*oe;
	struct options_entry			*o;
	const char				*optval;

	RB_FOREACH(o, options_tree, &oo->tree) {
		if (*o->name == '@') {
			if (args_has(self->args, 'v'))
				cmdq_print(cmdq, "%s", o->str);
			else
				cmdq_print(cmdq, "%s \"%s\"", o->name, o->str);
		}
	}

	for (oe = table; oe->name != NULL; oe++) {
		if (oe->style != NULL)
			continue;
		if ((o = options_find1(oo, oe->name)) == NULL)
			continue;
		optval = options_table_print_entry(oe, o,
		    args_has(self->args, 'v'));
		if (args_has(self->args, 'v'))
			cmdq_print(cmdq, "%s", optval);
		else
			cmdq_print(cmdq, "%s %s", oe->name, optval);
	}

	return (CMD_RETURN_NORMAL);
}
