/* $Id: cmd-send-prefix.c,v 1.1 2007-10-12 13:03:58 nicm Exp $ */

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

#include <getopt.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Send prefix key as a key.
 */

void	cmd_send_prefix_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_send_prefix_entry = {
	"send-prefix", NULL, 0,
	NULL,
	NULL,
	cmd_send_prefix_exec,
	NULL,
	NULL,
	NULL
};

void
cmd_send_prefix_exec(unused void *ptr, struct cmd_ctx *ctx)
{
	struct client	*c = ctx->client;

	if (!(ctx->flags & CMD_KEY)) {
		server_write_client(c, MSG_EXIT, NULL, 0);
		return;
	}

	window_key(c->session->window, prefix_key);
}
