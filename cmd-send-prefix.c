/* $Id: cmd-send-prefix.c,v 1.16 2008-06-19 22:04:02 nicm Exp $ */

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

void	cmd_send_prefix_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_send_prefix_entry = {
	"send-prefix", NULL,
	CMD_TARGET_WINDOW_USAGE,
	0,
	cmd_target_init,
	cmd_target_parse,
	cmd_send_prefix_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_send_prefix_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;
	struct winlink		*wl;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return;

	window_key(wl->window, options_get_key(&s->options, "prefix"));

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
