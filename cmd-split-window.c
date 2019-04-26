/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Split a window (add a new pane).
 */

#define SPLIT_WINDOW_TEMPLATE "#{session_name}:#{window_index}.#{pane_index}"

static enum cmd_retval	cmd_split_window_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_split_window_entry = {
	.name = "split-window",
	.alias = "splitw",

	.args = { "bc:de:fF:l:hp:Pt:v", 0, -1, "e" },
	.usage = "[-bdefhvP] [-c start-directory] [-e env-setting] [-F format] "
		 "[-p percentage|-l size] " CMD_TARGET_PANE_USAGE " [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_split_window_exec
};

static enum cmd_retval
cmd_split_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct cmd_find_state	*current = &item->shared->current;
	struct spawn_context	 sc;
	struct client		*c = cmd_find_client(item, NULL, 1);
	struct session		*s = item->target.s;
	struct winlink		*wl = item->target.wl;
	struct window_pane	*wp = item->target.wp, *new_wp;
	enum layout_type	 type;
	struct layout_cell	*lc;
	struct cmd_find_state	 fs;
	int			 size, percentage, flags;
	const char		*template;
	char			*cause, *cp;
	const char             **env_defs, **env_def;

	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;
	else
		type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'l')) {
		size = args_strtonum(args, 'l', 0, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "create pane failed: -l %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'p')) {
		percentage = args_strtonum(args, 'p', 0, INT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "create pane failed: -p %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (type == LAYOUT_TOPBOTTOM)
			size = (wp->sy * percentage) / 100;
		else
			size = (wp->sx * percentage) / 100;
	} else
		size = -1;

	server_unzoom_window(wp->window);

	flags = 0;
	if (args_has(args, 'b'))
		flags |= SPAWN_BEFORE;
	if (args_has(args, 'f'))
		flags |= SPAWN_FULLSIZE;

	lc = layout_split_pane(wp, type, size, flags);
	if (lc == NULL) {
		cmdq_error(item, "no space for new pane");
		return (CMD_RETURN_ERROR);
	}

	memset(&sc, 0, sizeof sc);
	sc.item = item;
	sc.s = s;
	sc.wl = wl;

	sc.wp0 = wp;
	sc.lc = lc;

	sc.name = NULL;
	sc.argc = args->argc;
	sc.argv = args->argv;
	sc.env = environ_create();

	env_defs = args_getall(args, 'e');
	for (env_def = env_defs; *env_def != NULL; env_def++) {
		environ_put(sc.env, *env_def);
	}
	free(env_defs);

	sc.idx = -1;
	sc.cwd = args_get(args, 'c');

	sc.flags = flags;
	if (args_has(args, 'd'))
		sc.flags |= SPAWN_DETACHED;

	if ((new_wp = spawn_pane(&sc, &cause)) == NULL) {
		cmdq_error(item, "create pane failed: %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}
	if (!args_has(args, 'd'))
		cmd_find_from_winlink_pane(current, wl, new_wp, 0);
	server_redraw_window(wp->window);
	server_status_session(s);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = SPLIT_WINDOW_TEMPLATE;
		cp = format_single(item, template, c, s, wl, new_wp);
		cmdq_print(item, "%s", cp);
		free(cp);
	}

	cmd_find_from_winlink_pane(&fs, wl, new_wp, 0);
	hooks_insert(s->hooks, item, &fs, "after-split-window");

	environ_free(sc.env);

	return (CMD_RETURN_NORMAL);
}
