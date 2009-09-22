/* $Id: cmd-send-prefix.c,v 1.27 2009-09-22 14:22:20 tcunha Exp $ */

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

#include "tmux.h"

/*
 * Send prefix key as a key.
 */

int	cmd_send_prefix_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_send_prefix_entry = {
	"send-prefix", NULL,
	CMD_TARGET_PANE_USAGE,
	0, 0,
	cmd_target_init,
	cmd_target_parse,
	cmd_send_prefix_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_send_prefix_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;
	struct window_pane	*wp;
	struct keylist		*keylist;

	if (cmd_find_pane(ctx, data->target, &s, &wp) == NULL)
		return (-1);

	keylist = options_get_data(&s->options, "prefix");
	window_pane_key(wp, ctx->curclient, ARRAY_FIRST(keylist));

	return (0);
}
