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

#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Respawn a window (restart the command). Kill existing if -k given.
 */

enum cmd_retval	 cmd_respawn_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_respawn_window_entry = {
	"respawn-window", "respawnw",
	"kt:", 0, -1,
	"[-k] " CMD_TARGET_WINDOW_USAGE " [command]",
	0,
	cmd_respawn_window_exec
};

enum cmd_retval
cmd_respawn_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	struct session		*s;
	struct environ		 env;
	const char		*path;
	char		 	*cause;
	struct environ_entry	*envent;

	if ((wl = cmd_find_window(cmdq, args_get(args, 't'), &s)) == NULL)
		return (CMD_RETURN_ERROR);
	w = wl->window;

	if (!args_has(self->args, 'k')) {
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if (wp->fd == -1)
				continue;
			cmdq_error(cmdq,
			    "window still active: %s:%d", s->name, wl->idx);
			return (CMD_RETURN_ERROR);
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

	path = NULL;
	if (cmdq->client != NULL && cmdq->client->session == NULL)
		envent = environ_find(&cmdq->client->environ, "PATH");
	else
		envent = environ_find(&s->environ, "PATH");
	if (envent != NULL)
		path = envent->value;

	if (window_pane_spawn(wp, args->argc, args->argv, path, NULL, -1, &env,
	    s->tio, &cause) != 0) {
		cmdq_error(cmdq, "respawn window failed: %s", cause);
		free(cause);
		environ_free(&env);
		server_destroy_pane(wp);
		return (CMD_RETURN_ERROR);
	}
	layout_init(w, wp);
	window_pane_reset_mode(wp);
	screen_reinit(&wp->base);
	input_init(wp);
	window_set_active_pane(w, wp);

	recalculate_sizes();
	server_redraw_window(w);

	environ_free(&env);
	return (CMD_RETURN_NORMAL);
}
