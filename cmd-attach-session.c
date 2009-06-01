/* $OpenBSD$ */

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
 * Attach existing session to the current terminal.
 */

int	cmd_attach_session_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_attach_session_entry = {
	"attach-session", "attach",
	"[-d] " CMD_TARGET_SESSION_USAGE,
       	CMD_DFLAG|CMD_CANTNEST|CMD_STARTSERVER,
	cmd_target_init,
	cmd_target_parse,
	cmd_attach_session_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

int
cmd_attach_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;
	char			*cause;

	if (ctx->curclient != NULL)
		return (0);

	if (ARRAY_LENGTH(&sessions) == 0) {
		ctx->error(ctx, "no sessions");
		return (-1);
	}
	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return (-1);

	if (!(ctx->cmdclient->flags & CLIENT_TERMINAL)) {
		ctx->error(ctx, "not a terminal");
		return (-1);
	}

	if (tty_open(&ctx->cmdclient->tty, &cause) != 0) {
		ctx->error(ctx, "terminal open failed: %s", cause);
		xfree(cause);
		return (-1);
	}

	if (data->flags & CMD_DFLAG)
		server_write_session(s, MSG_DETACH, NULL, 0);
	ctx->cmdclient->session = s;

	server_write_client(ctx->cmdclient, MSG_READY, NULL, 0);
	recalculate_sizes();
	server_redraw_client(ctx->cmdclient);

	return (1);
}

