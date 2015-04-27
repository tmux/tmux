/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

enum cmd_retval	 cmd_split_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_split_window_entry = {
	"split-window", "splitw",
	"bc:dF:l:hp:Pt:v", 0, -1,
	"[-bdhvP] [-c start-directory] [-F format] [-p percentage|-l size] "
	CMD_TARGET_PANE_USAGE " [command]",
	0,
	cmd_split_window_exec
};

enum cmd_retval
cmd_split_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp, *new_wp = NULL;
	struct environ		 env;
	const char		*cmd, *path, *shell, *template;
	char		       **argv, *cause, *new_cause, *cp;
	u_int			 hlimit;
	int			 argc, size, percentage, cwd, fd = -1;
	enum layout_type	 type;
	struct layout_cell	*lc;
	struct format_tree	*ft;
	struct environ_entry	*envent;

	if ((wl = cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp)) == NULL)
		return (CMD_RETURN_ERROR);
	w = wl->window;
	server_unzoom_window(w);

	environ_init(&env);
	environ_copy(&global_environ, &env);
	environ_copy(&s->environ, &env);
	server_fill_environ(s, &env);

	if (args->argc == 0) {
		cmd = options_get_string(&s->options, "default-command");
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

	if (args_has(args, 'c')) {
		ft = format_create();
		format_defaults(ft, cmd_find_client(cmdq, NULL, 1), s, NULL,
		    NULL);
		cp = format_expand(ft, args_get(args, 'c'));
		format_free(ft);

		if (cp != NULL && *cp != '\0') {
			fd = open(cp, O_RDONLY|O_DIRECTORY);
			free(cp);
			if (fd == -1) {
				cmdq_error(cmdq, "bad working directory: %s",
				    strerror(errno));
				return (CMD_RETURN_ERROR);
			}
		} else if (cp != NULL)
			free(cp);
		cwd = fd;
	} else if (cmdq->client != NULL && cmdq->client->session == NULL)
		cwd = cmdq->client->cwd;
	else
		cwd = s->cwd;

	type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;

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
	hlimit = options_get_number(&s->options, "history-limit");

	shell = options_get_string(&s->options, "default-shell");
	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;

	lc = layout_split_pane(wp, type, size, args_has(args, 'b'));
	if (lc == NULL) {
		cause = xstrdup("pane too small");
		goto error;
	}
	new_wp = window_add_pane(w, hlimit);
	layout_assign_pane(lc, new_wp);

	path = NULL;
	if (cmdq->client != NULL && cmdq->client->session == NULL)
		envent = environ_find(&cmdq->client->environ, "PATH");
	else
		envent = environ_find(&s->environ, "PATH");
	if (envent != NULL)
		path = envent->value;

	if (window_pane_spawn(new_wp, argc, argv, path, shell, cwd, &env,
	    s->tio, &cause) != 0)
		goto error;

	server_redraw_window(w);

	if (!args_has(args, 'd')) {
		window_set_active_pane(w, new_wp);
		session_select(s, wl->idx);
		server_redraw_session(s);
	} else
		server_status_session(s);

	environ_free(&env);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = SPLIT_WINDOW_TEMPLATE;

		ft = format_create();
		format_defaults(ft, cmd_find_client(cmdq, NULL, 1), s, wl,
		    new_wp);

		cp = format_expand(ft, template);
		cmdq_print(cmdq, "%s", cp);
		free(cp);

		format_free(ft);
	}
	notify_window_layout_changed(w);

	if (fd != -1)
		close(fd);
	return (CMD_RETURN_NORMAL);

error:
	environ_free(&env);
	if (new_wp != NULL) {
		layout_close_pane(new_wp);
		window_remove_pane(w, new_wp);
	}
	cmdq_error(cmdq, "create pane failed: %s", cause);
	free(cause);
	if (fd != -1)
		close(fd);
	return (CMD_RETURN_ERROR);
}
