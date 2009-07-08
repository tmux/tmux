/* $OpenBSD: cmd-show-options.c,v 1.2 2009/07/07 19:49:19 nicm Exp $ */

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
	"[-g] " CMD_TARGET_SESSION_USAGE,
	CMD_GFLAG,
	cmd_target_init,
	cmd_target_parse,
	cmd_show_options_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

int
cmd_show_options_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct session			*s;
	struct options			*oo;
	const struct set_option_entry   *entry;
	u_int				 i;
	char				*vs;
	long long			 vn;

	if (data->flags & CMD_GFLAG)
		oo = &global_s_options;
	else {
		if ((s = cmd_find_session(ctx, data->target)) == NULL)
			return (-1);
		oo = &s->options;
	}

	for (i = 0; i < NSETOPTION; i++) {
		entry = &set_option_table[i];

		if (options_find1(oo, entry->name) == NULL)
			continue;

		switch (entry->type) {
		case SET_OPTION_STRING:
			vs = options_get_string(oo, entry->name);
			ctx->print(ctx, "%s \"%s\"", entry->name, vs);
			break;
		case SET_OPTION_NUMBER:
			vn = options_get_number(oo, entry->name);
			ctx->print(ctx, "%s %lld", entry->name, vn);
			break;
		case SET_OPTION_KEY:
			vn = options_get_number(oo, entry->name);
 			ctx->print(ctx, "%s %s",
			    entry->name, key_string_lookup_key(vn));
			break;
		case SET_OPTION_COLOUR:
			vn = options_get_number(oo, entry->name);
 			ctx->print(ctx, "%s %s",
			    entry->name, colour_tostring(vn));
			break;
		case SET_OPTION_ATTRIBUTES:
			vn = options_get_number(oo, entry->name);
 			ctx->print(ctx, "%s %s",
			    entry->name, attributes_tostring(vn));
			break;
		case SET_OPTION_FLAG:
			vn = options_get_number(oo, entry->name);
			if (vn)
				ctx->print(ctx, "%s on", entry->name);
			else
				ctx->print(ctx, "%s off", entry->name);
			break;
		case SET_OPTION_CHOICE:
			vn = options_get_number(oo, entry->name);
			ctx->print(ctx, "%s %s",
			    entry->name, entry->choices[vn]);
			break;
		}
	}

	return (0);
}
