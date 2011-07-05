/* $Id: cmd-respawn-pane.c,v 1.2 2011/07/04 13:35:37 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
 * Copyright (c) 2011 Marcel P. Partap <mpartap@gmx.net>
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
 * Respawn a pane (restart the command). Kill existing if -k given.
 */

int	cmd_respawn_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_respawn_pane_entry = {
	"respawn-pane", "respawnp",
	"kt:", 0, 1,
	"[-k] " CMD_TARGET_PANE_USAGE " [command]",
	0,
	NULL,
	NULL,
	cmd_respawn_pane_exec
};

int
cmd_respawn_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	u_int			 hlimit;
	struct session		*s;
	struct environ		 env;
	const char		*cmd;
	char		 	*cause;

	if ((wl = cmd_find_pane(ctx, args_get(args, 't'), &s, &wp)) == NULL)
		return (-1);
	w = wl->window;

	if (!args_has(self->args, 'k') && wp->fd != -1) {
		ctx->error(ctx, "pane still active: %s:%u.%u",
		    s->name, wl->idx, window_pane_index(w, wp));
		return (-1);
	}

	environ_init(&env);
	environ_copy(&global_environ, &env);
	environ_copy(&s->environ, &env);
	server_fill_environ(s, &env);

	window_pane_reset_mode(wp);
	screen_reinit(&wp->base);
	input_init(wp);

	if (args->argc != 0)
		cmd = args->argv[0];
	else
		cmd = NULL;
	if (window_pane_spawn(wp, cmd, NULL, NULL, &env, s->tio, &cause) != 0) {
		ctx->error(ctx, "respawn pane failed: %s", cause);
		xfree(cause);
		environ_free(&env);
		return (-1);
	}
	wp->flags |= PANE_REDRAW;
	server_status_window(w);

	environ_free(&env);
	return (0);
}
