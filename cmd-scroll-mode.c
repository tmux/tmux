/* $Id: cmd-scroll-mode.c,v 1.9 2008-06-02 22:09:49 nicm Exp $ */

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
 * Enter scroll mode. Only valid when bound to a key.
 */

void	cmd_scroll_mode_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_scroll_mode_entry = {
	"scroll-mode", NULL,
	CMD_WINDOWONLY_USAGE,
	0,
	cmd_windowonly_parse,
	cmd_scroll_mode_exec,
	cmd_windowonly_send,
	cmd_windowonly_recv,
	cmd_windowonly_free
};

void
cmd_scroll_mode_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct winlink	*wl;

	if ((wl = cmd_windowonly_get(ptr, ctx, NULL)) == NULL)
		return;

	window_set_mode(wl->window, &window_scroll_mode);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
