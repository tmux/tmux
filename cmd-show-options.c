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

int	cmd_show_options_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_show_options_entry = {
	"show-options", "show",
	"[-g] " CMD_TARGET_SESSION_USAGE,
	0, "g",
	cmd_target_init,
	cmd_target_parse,
	cmd_show_options_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_show_options_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct session			*s;
	struct options			*oo;
	struct options_entry		*o;
	const struct set_option_entry   *entry;
	const char			*optval;

	if (cmd_check_flag(data->chflags, 'g'))
		oo = &global_s_options;
	else {
		if ((s = cmd_find_session(ctx, data->target)) == NULL)
			return (-1);
		oo = &s->options;
	}

	for (entry = set_option_table; entry->name != NULL; entry++) {
		if ((o = options_find1(oo, entry->name)) == NULL)
			continue;
		optval = set_option_print(entry, o);
		ctx->print(ctx, "%s %s", entry->name, optval);
	}

	return (0);
}
