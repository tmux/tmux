/* $Id: cmd-previous-window.c,v 1.12 2008-06-06 20:02:27 nicm Exp $ */

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
 * Move to previous window.
 */

void	cmd_previous_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_previous_window_entry = {
	"previous-window", "prev",
	CMD_TARGET_SESSION_USAGE,
	0,
	cmd_target_init,
	cmd_target_parse,
	cmd_previous_window_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_previous_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return;

	if (session_previous(s) == 0)
		server_redraw_session(s);
	else
		ctx->error(ctx, "no previous window");
	recalculate_sizes();

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
