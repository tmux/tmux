/* $Id$ */

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
	"drt:", 0, 0,
	"[-dr] " CMD_TARGET_SESSION_USAGE,
	CMD_CANTNEST|CMD_STARTSERVER|CMD_SENDENVIRON,
	NULL,
	NULL,
	cmd_attach_session_exec
};

int
cmd_attach_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct session	*s;
	struct client	*c;
	const char	*update;
	char		*overrides, *cause;
	u_int		 i;

	if (RB_EMPTY(&sessions)) {
		ctx->error(ctx, "no sessions");
		return (-1);
	}

	if ((s = cmd_find_session(ctx, args_get(args, 't'), 1)) == NULL)
		return (-1);

	if (ctx->cmdclient == NULL && ctx->curclient == NULL)
		return (0);

	if (ctx->cmdclient == NULL) {
		if (args_has(self->args, 'd')) {
			/*
			 * Can't use server_write_session in case attaching to
			 * the same session as currently attached to.
			 */
			for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
				c = ARRAY_ITEM(&clients, i);
				if (c == NULL || c->session != s)
					continue;
				if (c == ctx->curclient)
					continue;
				server_write_client(c, MSG_DETACH, NULL, 0);
			}
		}

		ctx->curclient->session = s;
		session_update_activity(s);
		server_redraw_client(ctx->curclient);
		s->curw->flags &= ~WINLINK_ALERTFLAGS;
	} else {
		if (!(ctx->cmdclient->flags & CLIENT_TERMINAL)) {
			ctx->error(ctx, "not a terminal");
			return (-1);
		}

		overrides =
		    options_get_string(&s->options, "terminal-overrides");
		if (tty_open(&ctx->cmdclient->tty, overrides, &cause) != 0) {
			ctx->error(ctx, "terminal open failed: %s", cause);
			xfree(cause);
			return (-1);
		}

		if (args_has(self->args, 'r'))
			ctx->cmdclient->flags |= CLIENT_READONLY;

		if (args_has(self->args, 'd'))
			server_write_session(s, MSG_DETACH, NULL, 0);

		ctx->cmdclient->session = s;
		session_update_activity(s);
		server_write_client(ctx->cmdclient, MSG_READY, NULL, 0);

		update = options_get_string(&s->options, "update-environment");
		environ_update(update, &ctx->cmdclient->environ, &s->environ);

		server_redraw_client(ctx->cmdclient);
		s->curw->flags &= ~WINLINK_ALERTFLAGS;
	}
	recalculate_sizes();
	server_update_socket();

	return (1);	/* 1 means don't tell command client to exit */
}
