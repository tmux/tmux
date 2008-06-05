/* $Id: cmd-unlink-window.c,v 1.10 2008-06-05 16:35:32 nicm Exp $ */

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
 * Unlink a window, unless it would be destroyed by doing so (only one link).
 */

void	cmd_unlink_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_unlink_window_entry = {
	"unlink-window", "unlinkw",
	CMD_WINDOWONLY_USAGE,
	0,
	cmd_windowonly_parse,
	cmd_unlink_window_exec,
	cmd_windowonly_send,
	cmd_windowonly_recv,
	cmd_windowonly_free,
	NULL,
	NULL
};

void
cmd_unlink_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct winlink	*wl;
	struct session	*s;
	struct client	*c;
	u_int		 i;
	int		 destroyed;

	if ((wl = cmd_windowonly_get(self, ctx, &s)) == NULL)
		return;

	if (wl->window->references == 1) {
		ctx->error(ctx, "window is only linked to one session");
		return;
	}

 	destroyed = session_detach(s, wl);
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session != s)
			continue;
		if (destroyed) {
			c->session = NULL;
			server_write_client(c, MSG_EXIT, NULL, 0);
		} else
			server_redraw_client(c);
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
