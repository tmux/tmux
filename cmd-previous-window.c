/* $Id: cmd-previous-window.c,v 1.5 2007-11-16 21:12:31 nicm Exp $ */

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
 * Move to previous window.
 */

void	cmd_previous_window_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_previous_window_entry = {
	"previous-window", "prev", "",
	CMD_NOCLIENT,
	NULL,
	cmd_previous_window_exec,
	NULL,
	NULL,
	NULL
};

void
cmd_previous_window_exec(unused void *ptr, struct cmd_ctx *ctx)
{
	if (session_previous(ctx->session) == 0)
		server_redraw_session(ctx->session);
	else
		ctx->error(ctx, "no previous window"); 
	
	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
