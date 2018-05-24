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

	.args = { "bc:dfF:l:hp:Pt:v", 0, -1 },
	.usage = "[-bdfhvP] [-c start-directory] [-F format] "
		 "[-p percentage|-l size] " CMD_TARGET_PANE_USAGE " [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_split_window_exec
};

static enum cmd_retval
cmd_split_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct cmd_find_state	*current = &item->shared->current;
	struct args		*args = self->args;
	struct client		*c = cmd_find_client(item, NULL, 1);
	struct session		*s = item->target.s;
	struct winlink		*wl = item->target.wl;
	struct window		*w = wl->window;
	struct window_pane	*wp = item->target.wp, *new_wp = NULL;
	struct environ		*env;
	const char		*cmd, *path, *shell, *template, *tmp;
	char		       **argv, *cause, *new_cause, *cp, *cwd;
	u_int			 hlimit;
	int			 argc, size, percentage, before;
	enum layout_type	 type;
	struct layout_cell	*lc;
	struct environ_entry	*envent;
	struct cmd_find_state    fs;

	server_unzoom_window(w);

	if (args->argc == 0) {
		cmd = options_get_string(s->options, "default-command");
		if (cmd != NULL && *cmd != '\0') {
			argc = 1;
			argv = (char **)&cmd;
		} else {
			argc = 0;
			argv = NULL;
		}
	} else {
		argc = args->argc;
		argv = args->argv;
	}

	if ((tmp = args_get(args, 'c')) != NULL)
		cwd = format_single(item, tmp, c, s, NULL, NULL);
	else
		cwd = xstrdup(server_client_get_cwd(item->client, s));

	type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;
	before = args_has(args, 'b');

	size = -1;
	if (args_has(args, 'l')) {
		size = args_strtonum(args, 'l', 0, INT_MAX, &cause);
		if (cause != NULL) {
			xasprintf(&new_cause, "size %s", cause);
			free(cause);
			cause = new_cause;
			goto error;
		}
	} else if (args_has(args, 'p')) {
		percentage = args_strtonum(args, 'p', 0, INT_MAX, &cause);
		if (cause != NULL) {
			xasprintf(&new_cause, "percentage %s", cause);
			free(cause);
			cause = new_cause;
			goto error;
		}
		if (type == LAYOUT_TOPBOTTOM)
			size = (wp->sy * percentage) / 100;
		else
			size = (wp->sx * percentage) / 100;
	}
	hlimit = options_get_number(s->options, "history-limit");

	shell = options_get_string(s->options, "default-shell");
	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;

	lc = layout_split_pane(wp, type, size, before, args_has(args, 'f'));
	if (lc == NULL) {
		cause = xstrdup("pane too small");
		goto error;
	}
	new_wp = window_add_pane(w, wp, before, args_has(args, 'f'), hlimit);
	layout_make_leaf(lc, new_wp);

	path = NULL;
	if (item->client != NULL && item->client->session == NULL)
		envent = environ_find(item->client->environ, "PATH");
	else
		envent = environ_find(s->environ, "PATH");
	if (envent != NULL)
		path = envent->value;

	env = environ_for_session(s, 0);
	if (window_pane_spawn(new_wp, argc, argv, path, shell, cwd, env,
	    s->tio, &cause) != 0) {
		environ_free(env);
		goto error;
	}
	environ_free(env);

	layout_fix_panes(w, w->sx, w->sy);
	server_redraw_window(w);

	if (!args_has(args, 'd')) {
		window_set_active_pane(w, new_wp);
		session_select(s, wl->idx);
		cmd_find_from_session(current, s, 0);
		server_redraw_session(s);
	} else
		server_status_session(s);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = SPLIT_WINDOW_TEMPLATE;
		cp = format_single(item, template, c, s, wl, new_wp);
		cmdq_print(item, "%s", cp);
		free(cp);
	}
	notify_window("window-layout-changed", w);

	cmd_find_from_winlink_pane(&fs, wl, new_wp, 0);
	hooks_insert(s->hooks, item, &fs, "after-split-window");

	free(cwd);
	return (CMD_RETURN_NORMAL);

error:
	if (new_wp != NULL) {
		layout_close_pane(new_wp);
		window_remove_pane(w, new_wp);
	}
	cmdq_error(item, "create pane failed: %s", cause);
	free(cause);

	free(cwd);
	return (CMD_RETURN_ERROR);
}
