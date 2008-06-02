/* $Id: cmd-next-window.c,v 1.7 2008-06-02 18:08:16 nicm Exp $ */

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
#include <string.h>

#include "tmux.h"

/*
 * Move to next window.
 */

void	cmd_next_window_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_next_window_entry = {
	"next-window", "next",
	CMD_SESSIONONLY_USAGE,
	0,
	cmd_sessiononly_parse,
	cmd_next_window_exec,
	cmd_sessiononly_send,
	cmd_sessiononly_recv,
	cmd_sessiononly_free
};

void
cmd_next_window_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct session	*s;

	if ((s = cmd_sessiononly_get(ptr, ctx)) == NULL)
		return;

	if (session_next(s) == 0)
		server_redraw_session(s);
	else
		ctx->error(ctx, "no next window");

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
