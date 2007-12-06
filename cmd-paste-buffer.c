/* $Id: cmd-paste-buffer.c,v 1.2 2007-12-06 09:46:22 nicm Exp $ */

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
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Paste paste buffer if present.
 */

void	cmd_paste_buffer_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_paste_buffer_entry = {
	"paste-buffer", NULL, "paste",
	CMD_NOCLIENT,
	NULL,
	cmd_paste_buffer_exec,
	NULL,
	NULL,
	NULL
};

void
cmd_paste_buffer_exec(unused void *ptr, struct cmd_ctx *ctx)
{
	struct window	*w = ctx->session->curw->window;

	if (ctx->flags & CMD_KEY) {
		if (paste_buffer != NULL && *paste_buffer != '\0') {
			buffer_write(
			    w->out, paste_buffer, strlen(paste_buffer));
		}
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
