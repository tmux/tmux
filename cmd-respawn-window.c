/* $Id: cmd-respawn-window.c,v 1.4 2008-07-01 05:06:11 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <unistd.h>

#include "tmux.h"

/*
 * Respawn a window (restart the command). Kill existing if -k given.
 */

void	cmd_respawn_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_respawn_window_entry = {
	"respawn-window", "respawnw",
	"[-k] " CMD_TARGET_WINDOW_USAGE " [command]",
	CMD_ZEROONEARG|CMD_KFLAG,
	cmd_target_init,
	cmd_target_parse,
	cmd_respawn_window_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_respawn_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	struct session		*s;
	const char		*env[] = { NULL /* TMUX= */, "TERM=screen", NULL };
	char		 	 buf[256];
	u_int			 i;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return;

	if (wl->window->fd != -1 && !(data->flags & CMD_KFLAG)) {
		ctx->error(ctx, "window still active: %s:%d", s->name, wl->idx);
		return;
	}

	if (session_index(s, &i) != 0)
		fatalx("session not found");
	xsnprintf(buf, sizeof buf, "TMUX=%ld,%u", (long) getpid(), i);
	env[0] = buf;

	if (window_spawn(wl->window, data->arg, env) != 0) {
		ctx->error(ctx, "respawn failed: %s:%d", s->name, wl->idx);
		return;
	}
	screen_reset(&wl->window->base);

	recalculate_sizes();
	server_redraw_window(wl->window);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
