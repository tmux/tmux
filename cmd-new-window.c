/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Create a new window.
 */

#define NEW_WINDOW_TEMPLATE "#{session_name}:#{window_index}.#{pane_index}"

static enum cmd_retval	cmd_new_window_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_new_window_entry = {
	.name = "new-window",
	.alias = "neww",

	.args = { "abc:de:F:kn:PSt:", 0, -1, NULL },
	.usage = "[-abdkPS] [-c start-directory] [-e environment] [-F format] "
		 "[-n window-name] " CMD_TARGET_WINDOW_USAGE " [shell-command]",

	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_WINDOW_INDEX },

	.flags = 0,
	.exec = cmd_new_window_exec
};

static enum cmd_retval
cmd_new_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*c = cmdq_get_client(item);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct spawn_context	 sc = { 0 };
	struct client		*tc = cmdq_get_target_client(item);
	struct session		*s = target->s;
	struct winlink		*wl = target->wl, *new_wl = NULL;
	int			 idx = target->idx, before;
	char			*cause = NULL, *cp;
	const char		*template, *name;
	struct cmd_find_state	 fs;
	struct args_value	*av;

	/*
	 * If -S and -n are given and -t is not and a single window with this
	 * name already exists, select it.
	 */
	name = args_get(args, 'n');
	if (args_has(args, 'S') && name != NULL && target->idx == -1) {
		RB_FOREACH(wl, winlinks, &s->windows) {
			if (strcmp(wl->window->name, name) != 0)
				continue;
			if (new_wl == NULL) {
				new_wl = wl;
				continue;
			}
			cmdq_error(item, "multiple windows named %s", name);
			return (CMD_RETURN_ERROR);
		}
		if (new_wl != NULL) {
			if (args_has(args, 'd'))
				return (CMD_RETURN_NORMAL);
			if (session_set_current(s, new_wl) == 0)
				server_redraw_session(s);
			if (c != NULL && c->session != NULL)
				s->curw->window->latest = c;
			recalculate_sizes();
			return (CMD_RETURN_NORMAL);
		}
	}

	before = args_has(args, 'b');
	if (args_has(args, 'a') || before) {
		idx = winlink_shuffle_up(s, wl, before);
		if (idx == -1)
			idx = target->idx;
	}

	sc.item = item;
	sc.s = s;
	sc.tc = tc;

	sc.name = args_get(args, 'n');
	args_to_vector(args, &sc.argc, &sc.argv);
	sc.environ = environ_create();

	av = args_first_value(args, 'e');
	while (av != NULL) {
		environ_put(sc.environ, av->string, 0);
		av = args_next_value(av);
	}

	sc.idx = idx;
	sc.cwd = args_get(args, 'c');

	sc.flags = 0;
	if (args_has(args, 'd'))
		sc.flags |= SPAWN_DETACHED;
	if (args_has(args, 'k'))
		sc.flags |= SPAWN_KILL;

	if ((new_wl = spawn_window(&sc, &cause)) == NULL) {
		cmdq_error(item, "create window failed: %s", cause);
		free(cause);
		if (sc.argv != NULL)
			cmd_free_argv(sc.argc, sc.argv);
		environ_free(sc.environ);
		return (CMD_RETURN_ERROR);
	}
	if (!args_has(args, 'd') || new_wl == s->curw) {
		cmd_find_from_winlink(current, new_wl, 0);
		server_redraw_session_group(s);
	} else
		server_status_session_group(s);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = NEW_WINDOW_TEMPLATE;
		cp = format_single(item, template, tc, s, new_wl,
			new_wl->window->active);
		cmdq_print(item, "%s", cp);
		free(cp);
	}

	cmd_find_from_winlink(&fs, new_wl, 0);
	cmdq_insert_hook(s, item, &fs, "after-new-window");

	if (sc.argv != NULL)
		cmd_free_argv(sc.argc, sc.argv);
	environ_free(sc.environ);
	return (CMD_RETURN_NORMAL);
}
