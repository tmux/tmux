/* $Id$ */

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

int	cmd_respawn_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_respawn_window_entry = {
	"respawn-window", "respawnw",
	"kt:", 0, 1,
	"[-k] " CMD_TARGET_WINDOW_USAGE " [command]",
	0,
	NULL,
	NULL,
	cmd_respawn_window_exec
};

int
cmd_respawn_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	struct session		*s;
	struct environ		 env;
	const char		*cmd;
	char		 	*cause;

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), &s)) == NULL)
		return (-1);
	w = wl->window;

	if (!args_has(self->args, 'k')) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd == -1)
				continue;
			ctx->error(ctx,
			    "window still active: %s:%d", s->name, wl->idx);
			return (-1);
		}
	}

	environ_init(&env);
	environ_copy(&global_environ, &env);
	environ_copy(&s->environ, &env);
	server_fill_environ(s, &env);

	wp = TAILQ_FIRST(&w->panes);
	TAILQ_REMOVE(&w->panes, wp, entry);
	layout_free(w);
	window_destroy_panes(w);
	TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	window_pane_resize(wp, w->sx, w->sy);
	if (args->argc != 0)
		cmd = args->argv[0];
	else
		cmd = NULL;
	if (window_pane_spawn(wp, cmd, NULL, NULL, &env, s->tio, &cause) != 0) {
		ctx->error(ctx, "respawn window failed: %s", cause);
		xfree(cause);
		environ_free(&env);
		server_destroy_pane(wp);
		return (-1);
	}
	layout_init(w);
	window_pane_reset_mode(wp);
	screen_reinit(&wp->base);
	input_init(wp);
	window_set_active_pane(w, wp);

	recalculate_sizes();
	server_redraw_window(w);

	environ_free(&env);
	return (0);
}
