/* $OpenBSD$ */

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
	"[-k] " CMD_TARGET_WINDOW_USAGE " [command]",
	CMD_ARG01|CMD_KFLAG,
	cmd_target_init,
	cmd_target_parse,
	cmd_respawn_window_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

int
cmd_respawn_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	struct session		*s;
	const char	       **env;
	char		 	*cause;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return (-1);
	w = wl->window;

	if (!(data->flags & CMD_KFLAG)) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd == -1)
				continue;
			ctx->error(ctx,
			    "window still active: %s:%d", s->name, wl->idx);
			return (-1);
		}
	}

	env = server_fill_environ(s);

	wp = TAILQ_FIRST(&w->panes);
	TAILQ_REMOVE(&w->panes, wp, entry);
 	window_destroy_panes(w);
	TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	window_pane_resize(wp, w->sx, w->sy);
	if (window_pane_spawn(wp, data->arg, NULL, env, &cause) != 0) {
		ctx->error(ctx, "respawn window failed: %s", cause);
		xfree(cause);
		return (-1);
	}
	screen_reinit(&wp->base);

	recalculate_sizes();
	server_redraw_window(w);

	return (0);
}
