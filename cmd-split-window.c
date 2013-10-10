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
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Split a window (add a new pane).
 */

void		 cmd_split_window_key_binding(struct cmd *, int);
enum cmd_retval	 cmd_split_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_split_window_entry = {
	"split-window", "splitw",
	"c:dF:l:hp:Pt:v", 0, 1,
	"[-dhvP] [-c start-directory] [-F format] [-p percentage|-l size] "
	CMD_TARGET_PANE_USAGE " [command]",
	0,
	cmd_split_window_key_binding,
	cmd_split_window_exec
};

void
cmd_split_window_key_binding(struct cmd *self, int key)
{
	self->args = args_create(0);
	if (key == '%')
		args_set(self->args, 'h', NULL);
}

enum cmd_retval
cmd_split_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp, *new_wp = NULL;
	struct environ		 env;
	const char		*cmd, *shell, *template;
	char			*cause, *new_cause, *cp;
	u_int			 hlimit;
	int			 size, percentage, cwd, fd = -1;
	enum layout_type	 type;
	struct layout_cell	*lc;
	struct client		*c;
	struct format_tree	*ft;

	if ((wl = cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp)) == NULL)
		return (CMD_RETURN_ERROR);
	w = wl->window;
	server_unzoom_window(w);

	environ_init(&env);
	environ_copy(&global_environ, &env);
	environ_copy(&s->environ, &env);
	server_fill_environ(s, &env);

	if (args->argc == 0)
		cmd = options_get_string(&s->options, "default-command");
	else
		cmd = args->argv[0];

	if (args_has(args, 'c')) {
		ft = format_create();
		if ((c = cmd_find_client(cmdq, NULL, 1)) != NULL)
			format_client(ft, c);
		format_session(ft, s);
		format_winlink(ft, s, s->curw);
		format_window_pane(ft, s->curw->window->active);
		cp = format_expand(ft, args_get(args, 'c'));
		format_free(ft);

		fd = open(cp, O_RDONLY|O_DIRECTORY);
		free(cp);
		if (fd == -1) {
			cmdq_error(cmdq, "bad working directory: %s",
			    strerror(errno));
			return (CMD_RETURN_ERROR);
		}
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

	if ((lc = layout_split_pane(wp, type, size, 0)) == NULL) {
		cause = xstrdup("pane too small");
		goto error;
	}
	new_wp = window_add_pane(w, hlimit);
	if (window_pane_spawn(
	    new_wp, cmd, shell, cwd, &env, s->tio, &cause) != 0)
		goto error;
	layout_assign_pane(lc, new_wp);

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
		if ((c = cmd_find_client(cmdq, NULL, 1)) != NULL)
			format_client(ft, c);
		format_session(ft, s);
		format_winlink(ft, s, wl);
		format_window_pane(ft, new_wp);

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
	if (new_wp != NULL)
		window_remove_pane(w, new_wp);
	cmdq_error(cmdq, "create pane failed: %s", cause);
	free(cause);
	if (fd != -1)
		close(fd);
	return (CMD_RETURN_ERROR);
}
