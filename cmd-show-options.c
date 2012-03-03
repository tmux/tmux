/* $Id$ */

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

int	cmd_show_options_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_show_options_entry = {
	"show-options", "show",
	"gst:w", 0, 1,
	"[-gsw] [-t target-session|target-window] [option]",
	0,
	NULL,
	NULL,
	cmd_show_options_exec
};

const struct cmd_entry cmd_show_window_options_entry = {
	"show-window-options", "showw",
	"gt:", 0, 1,
	"[-g] " CMD_TARGET_WINDOW_USAGE " [option]",
	0,
	NULL,
	NULL,
	cmd_show_options_exec
};

int
cmd_show_options_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args				*args = self->args;
	const struct options_table_entry	*table, *oe;
	struct session				*s;
	struct winlink				*wl;
	struct options				*oo;
	struct options_entry			*o;
	const char				*optval;

	if (args_has(self->args, 's')) {
		oo = &global_options;
		table = server_options_table;
	} else if (args_has(self->args, 'w') ||
	    self->entry == &cmd_show_window_options_entry) {
		table = window_options_table;
		if (args_has(self->args, 'g'))
			oo = &global_w_options;
		else {
			wl = cmd_find_window(ctx, args_get(args, 't'), NULL);
			if (wl == NULL)
				return (-1);
			oo = &wl->window->options;
		}
	} else {
		table = session_options_table;
		if (args_has(self->args, 'g'))
			oo = &global_s_options;
		else {
			s = cmd_find_session(ctx, args_get(args, 't'), 0);
			if (s == NULL)
				return (-1);
			oo = &s->options;
		}
	}

	if (args->argc != 0) {
		table = oe = NULL;
		if (options_table_find(args->argv[0], &table, &oe) != 0) {
			ctx->error(ctx, "ambiguous option: %s", args->argv[0]);
			return (-1);
		}
		if (oe == NULL) {
			ctx->error(ctx, "unknown option: %s", args->argv[0]);
			return (-1);
		}
		if ((o = options_find1(oo, oe->name)) == NULL)
			return (0);
		optval = options_table_print_entry(oe, o);
		ctx->print(ctx, "%s %s", oe->name, optval);
	} else {
		for (oe = table; oe->name != NULL; oe++) {
			if ((o = options_find1(oo, oe->name)) == NULL)
				continue;
			optval = options_table_print_entry(oe, o);
			ctx->print(ctx, "%s %s", oe->name, optval);
		}
	}

	return (0);
}
