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
#include <string.h>

#include "tmux.h"

/*
 * Respawn a pane (restart the command). Kill existing if -k given.
 */

static enum cmd_retval	cmd_respawn_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_respawn_pane_entry = {
	.name = "respawn-pane",
	.alias = "respawnp",

	.args = { "c:e:kt:", 0, -1 },
	.usage = "[-k] [-c start-directory] [-e environment] "
		 CMD_TARGET_PANE_USAGE " [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_respawn_pane_exec
};

static enum cmd_retval
cmd_respawn_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct spawn_context	 sc;
	struct session		*s = target->s;
	struct winlink		*wl = target->wl;
	struct window_pane	*wp = target->wp;
	char			*cause = NULL;
	const char		*add;
	struct args_value	*value;

	memset(&sc, 0, sizeof sc);
	sc.item = item;
	sc.s = s;
	sc.wl = wl;

	sc.wp0 = wp;
	sc.lc = NULL;

	sc.name = NULL;
	sc.argc = args->argc;
	sc.argv = args->argv;
	sc.environ = environ_create();

	add = args_first_value(args, 'e', &value);
	while (add != NULL) {
		environ_put(sc.environ, add, 0);
		add = args_next_value(&value);
	}

	sc.idx = -1;
	sc.cwd = args_get(args, 'c');

	sc.flags = SPAWN_RESPAWN;
	if (args_has(args, 'k'))
		sc.flags |= SPAWN_KILL;

	if (spawn_pane(&sc, &cause) == NULL) {
		cmdq_error(item, "respawn pane failed: %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	wp->flags |= PANE_REDRAW;
	server_redraw_window_borders(wp->window);
	server_status_window(wp->window);

	environ_free(sc.environ);
	return (CMD_RETURN_NORMAL);
}
