/* $Id: cmd-last-window.c,v 1.1 2007-10-04 00:02:10 nicm Exp $ */

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
 * Move to last window.
 */

void	cmd_last_window_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_last_window_entry = {
	CMD_LASTWINDOW, "last-window", "last", 0,
	NULL,
	NULL,
	cmd_last_window_exec,
	NULL,
	NULL,
	NULL
};

void
cmd_last_window_exec(unused void *ptr, struct cmd_ctx *ctx)
{
	struct client	*c = ctx->client;
	struct session	*s = ctx->session;

	if (session_last(s) == 0)
		server_redraw_session(s);
	else
		ctx->error(ctx, "no last window"); 
	
	if (!(ctx->flags & CMD_KEY))
		server_write_client(c, MSG_EXIT, NULL, 0);
}
