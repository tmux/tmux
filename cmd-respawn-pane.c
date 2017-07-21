/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Respawn a pane (restart the command). Kill existing if -k given.
 */

static enum cmd_retval	cmd_respawn_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_respawn_pane_entry = {
	.name = "respawn-pane",
	.alias = "respawnp",

	.args = { "c:kt:", 0, -1 },
	.usage = "[-c start-directory] [-k] " CMD_TARGET_PANE_USAGE
	         " [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_respawn_pane_exec
};

static enum cmd_retval
cmd_respawn_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct winlink		*wl = item->target.wl;
	struct window		*w = wl->window;
	struct window_pane	*wp = item->target.wp;
	struct client           *c = cmd_find_client(item, NULL, 1);
	struct session		*s = item->target.s;
	struct environ		*env;
	const char		*path = NULL, *cp;
	char			*cause, *cwd = NULL;
	u_int			 idx;
	struct environ_entry	*envent;

	if (!args_has(self->args, 'k') && wp->fd != -1) {
		if (window_pane_index(wp, &idx) != 0)
			fatalx("index not found");
		cmdq_error(item, "pane still active: %s:%d.%u",
		    s->name, wl->idx, idx);
		return (CMD_RETURN_ERROR);
	}

	window_pane_reset_mode(wp);
	screen_reinit(&wp->base);
	input_init(wp);

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
		cmdq_error(item, "respawn pane failed: %s", cause);
		free(cause);
		environ_free(env);
		free(cwd);
		return (CMD_RETURN_ERROR);
	}
	environ_free(env);
	free(cwd);

	wp->flags |= PANE_REDRAW;
	server_status_window(w);

	return (CMD_RETURN_NORMAL);
}
