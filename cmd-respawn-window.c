/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Respawn a window (restart the command). Kill existing if -k given.
 */

static enum cmd_retval	cmd_respawn_window_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_respawn_window_entry = {
	.name = "respawn-window",
	.alias = "respawnw",

	.args = { "c:kt:", 0, -1 },
	.usage = "[-c start-directory] [-k] " CMD_TARGET_WINDOW_USAGE
	         " [command]",

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_respawn_window_exec
};

static enum cmd_retval
cmd_respawn_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct session		*s = item->target.s;
	struct winlink		*wl = item->target.wl;
	struct window		*w = wl->window;
	struct window_pane	*wp;
	struct client           *c = cmd_find_client(item, NULL, 1);
	struct environ		*env;
	const char		*path = NULL, *cp;
	char		 	*cause, *cwd = NULL;
	struct environ_entry	*envent;

	if (!args_has(self->args, 'k')) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd == -1)
				continue;
			cmdq_error(item, "window still active: %s:%d", s->name,
			    wl->idx);
			return (CMD_RETURN_ERROR);
		}
	}

	wp = TAILQ_FIRST(&w->panes);
	TAILQ_REMOVE(&w->panes, wp, entry);
	layout_free(w);
	window_destroy_panes(w);
	TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	window_pane_resize(wp, w->sx, w->sy);

	if (item->client != NULL && item->client->session == NULL)
		envent = environ_find(item->client->environ, "PATH");
	else
		envent = environ_find(s->environ, "PATH");
	if (envent != NULL)
		path = envent->value;

	if ((cp = args_get(args, 'c')) != NULL)
		cwd = format_single(item, cp, c, s, NULL, NULL);

	env = environ_for_session(s, 0);
	if (window_pane_spawn(wp, args->argc, args->argv, path, NULL, cwd, env,
	    s->tio, &cause) != 0) {
		cmdq_error(item, "respawn window failed: %s", cause);
		free(cause);
		environ_free(env);
		free(cwd);
		server_destroy_pane(wp, 0);
		return (CMD_RETURN_ERROR);
	}
	environ_free(env);
	free(cwd);

	layout_init(w, wp);
	window_pane_reset_mode(wp);
	screen_reinit(&wp->base);
	input_init(wp);
	window_set_active_pane(w, wp);

	recalculate_sizes();
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}
