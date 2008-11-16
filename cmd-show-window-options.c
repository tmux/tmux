/* $Id: cmd-show-window-options.c,v 1.3 2008-11-16 13:28:59 nicm Exp $ */

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

#include "tmux.h"

/*
 * Show window options.
 */

void	cmd_show_window_options_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_show_window_options_entry = {
	"show-window-options", "showw",
	CMD_TARGET_WINDOW_USAGE,
	0,
	cmd_target_init,
	cmd_target_parse,
	cmd_show_window_options_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_show_window_options_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	struct session		*s;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return;

	if (wl->window->flags & WINDOW_AGGRESSIVE)
		ctx->print(ctx, "aggressive-resize");
	if (wl->window->limitx != UINT_MAX)
		ctx->print(ctx, "force-width %u", wl->window->limitx);
	if (wl->window->limity != UINT_MAX)
		ctx->print(ctx, "force-height %u", wl->window->limity);
	if (wl->window->flags & WINDOW_MONITOR)
		ctx->print(ctx, "monitor-activity");
	if (wl->window->flags & WINDOW_ZOMBIFY)
		ctx->print(ctx, "remain-on-exit");
	if (wl->window->flags & WINDOW_UTF8)
		ctx->print(ctx, "utf8");

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
