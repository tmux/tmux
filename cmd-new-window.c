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

enum cmd_retval	cmd_new_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_new_window_entry = {
	.name = "new-window",
	.alias = "neww",

	.args = { "ac:dF:kn:Pt:", 0, -1 },
	.usage = "[-adkP] [-c start-directory] [-F format] [-n window-name] "
		 CMD_TARGET_WINDOW_USAGE " [command]",

	.tflag = CMD_WINDOW_INDEX,

	.flags = 0,
	.exec = cmd_new_window_exec
};

enum cmd_retval
cmd_new_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct session		*s = cmdq->state.tflag.s;
	struct winlink		*wl = cmdq->state.tflag.wl;
	struct client		*c = cmdq->state.c;
	int			 idx = cmdq->state.tflag.idx;
	const char		*cmd, *path, *template, *cwd, *to_free;
	char		       **argv, *cause, *cp;
	int			 argc, detached;
	struct format_tree	*ft;
	struct environ_entry	*envent;

	if (args_has(args, 'a')) {
		if ((idx = winlink_shuffle_up(s, wl)) == -1) {
			cmdq_error(cmdq, "no free window indexes");
			return (CMD_RETURN_ERROR);
		}
	}
	detached = args_has(args, 'd');

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

	path = NULL;
	if (cmdq->client != NULL && cmdq->client->session == NULL)
		envent = environ_find(cmdq->client->environ, "PATH");
	else
		envent = environ_find(s->environ, "PATH");
	if (envent != NULL)
		path = envent->value;

	to_free = NULL;
	if (args_has(args, 'c')) {
		ft = format_create(cmdq, 0);
		format_defaults(ft, c, s, NULL, NULL);
		cwd = to_free = format_expand(ft, args_get(args, 'c'));
		format_free(ft);
	} else if (cmdq->client != NULL && cmdq->client->session == NULL)
		cwd = cmdq->client->cwd;
	else
		cwd = s->cwd;

	wl = NULL;
	if (idx != -1)
		wl = winlink_find_by_index(&s->windows, idx);
	if (wl != NULL && args_has(args, 'k')) {
		/*
		 * Can't use session_detach as it will destroy session if this
		 * makes it empty.
		 */
		notify_window_unlinked(s, wl->window);
		wl->flags &= ~WINLINK_ALERTFLAGS;
		winlink_stack_remove(&s->lastw, wl);
		winlink_remove(&s->windows, wl);

		/* Force select/redraw if current. */
		if (wl == s->curw) {
			detached = 0;
			s->curw = NULL;
		}
	}

	if (idx == -1)
		idx = -1 - options_get_number(s->options, "base-index");
	wl = session_new(s, args_get(args, 'n'), argc, argv, path, cwd, idx,
		&cause);
	if (wl == NULL) {
		cmdq_error(cmdq, "create window failed: %s", cause);
		free(cause);
		goto error;
	}
	if (!detached) {
		session_select(s, wl->idx);
		server_redraw_session_group(s);
	} else
		server_status_session_group(s);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = NEW_WINDOW_TEMPLATE;

		ft = format_create(cmdq, 0);
		format_defaults(ft, c, s, wl, NULL);

		cp = format_expand(ft, template);
		cmdq_print(cmdq, "%s", cp);
		free(cp);

		format_free(ft);
	}

	if (to_free != NULL)
		free((void *)to_free);
	return (CMD_RETURN_NORMAL);

error:
	if (to_free != NULL)
		free((void *)to_free);
	return (CMD_RETURN_ERROR);
}
