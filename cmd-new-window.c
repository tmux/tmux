/* $Id$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

enum cmd_retval	cmd_new_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_new_window_entry = {
	"new-window", "neww",
	"ac:dF:kn:Pt:", 0, 1,
	"[-adkP] [-c start-directory] [-F format] [-n window-name] "
	CMD_TARGET_WINDOW_USAGE " [command]",
	0,
	NULL,
	cmd_new_window_exec
};

enum cmd_retval
cmd_new_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct session		*s;
	struct winlink		*wl;
	struct client		*c;
	const char		*cmd, *template;
	char			*cause, *cp;
	int			 idx, last, detached, cwd, fd = -1;
	struct format_tree	*ft;

	if (args_has(args, 'a')) {
		wl = cmd_find_window(cmdq, args_get(args, 't'), &s);
		if (wl == NULL)
			return (CMD_RETURN_ERROR);
		idx = wl->idx + 1;

		/* Find the next free index. */
		for (last = idx; last < INT_MAX; last++) {
			if (winlink_find_by_index(&s->windows, last) == NULL)
				break;
		}
		if (last == INT_MAX) {
			cmdq_error(cmdq, "no free window indexes");
			return (CMD_RETURN_ERROR);
		}

		/* Move everything from last - 1 to idx up a bit. */
		for (; last > idx; last--) {
			wl = winlink_find_by_index(&s->windows, last - 1);
			server_link_window(s, wl, s, last, 0, 0, NULL);
			server_unlink_window(s, wl);
		}
	} else {
		if ((idx = cmd_find_index(cmdq, args_get(args, 't'), &s)) == -2)
			return (CMD_RETURN_ERROR);
	}
	detached = args_has(args, 'd');

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
		idx = -1 - options_get_number(&s->options, "base-index");
	wl = session_new(s, args_get(args, 'n'), cmd, cwd, idx, &cause);
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

		ft = format_create();
		if ((c = cmd_find_client(cmdq, NULL, 1)) != NULL)
			format_client(ft, c);
		format_session(ft, s);
		format_winlink(ft, s, wl);
		format_window_pane(ft, wl->window->active);

		cp = format_expand(ft, template);
		cmdq_print(cmdq, "%s", cp);
		free(cp);

		format_free(ft);
	}

	if (fd != -1)
		close(fd);
	return (CMD_RETURN_NORMAL);

error:
	if (fd != -1)
		close(fd);
	return (CMD_RETURN_ERROR);
}
