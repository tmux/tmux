/* $Id: cmd-paste-buffer.c,v 1.7 2008-06-05 16:35:32 nicm Exp $ */

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

#include <string.h>

#include "tmux.h"

/*
 * Paste paste buffer if present.
 */

void	cmd_paste_buffer_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_paste_buffer_entry = {
	"paste-buffer", "paste",
	CMD_WINDOWONLY_USAGE,
	0,
	cmd_windowonly_parse,
	cmd_paste_buffer_exec,
	cmd_windowonly_send,
	cmd_windowonly_recv,
	cmd_windowonly_free,
	NULL,
	NULL
};

void
cmd_paste_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct winlink	*wl;

	if ((wl = cmd_windowonly_get(self, ctx, NULL)) == NULL)
		return;

	if (paste_buffer != NULL && *paste_buffer != '\0') {
		buffer_write(
		    wl->window->out, paste_buffer, strlen(paste_buffer));
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
