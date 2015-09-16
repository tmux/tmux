/* $OpenBSD$ */

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
 * Attach existing session to the current terminal.
 */

enum cmd_retval	cmd_attach_session_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_attach_session_entry = {
	"attach-session", "attach",
	"c:dErt:", 0, 0,
	"[-dEr] [-c working-directory] " CMD_TARGET_SESSION_USAGE,
	CMD_STARTSERVER,
	cmd_attach_session_exec
};

enum cmd_retval
cmd_attach_session(struct cmd_q *cmdq, const char *tflag, int dflag, int rflag,
    const char *cflag, int Eflag)
{
	struct session		*s;
	struct client		*c = cmdq->client, *c_loop;
	struct winlink		*wl = NULL;
	struct window		*w = NULL;
	struct window_pane	*wp = NULL;
	const char		*update;
	char			*cause;
	int			 fd;
	struct format_tree	*ft;
	char			*cp;

	if (RB_EMPTY(&sessions)) {
		cmdq_error(cmdq, "no sessions");
		return (CMD_RETURN_ERROR);
	}

	if (tflag == NULL) {
		if ((s = cmd_find_session(cmdq, tflag, 1)) == NULL)
			return (CMD_RETURN_ERROR);
	} else if (tflag[strcspn(tflag, ":.")] != '\0') {
		if ((wl = cmd_find_pane(cmdq, tflag, &s, &wp)) == NULL)
			return (CMD_RETURN_ERROR);
	} else {
		if ((s = cmd_find_session(cmdq, tflag, 1)) == NULL)
			return (CMD_RETURN_ERROR);
		w = window_find_by_id_str(tflag);
		if (w == NULL) {
			wp = window_pane_find_by_id_str(tflag);
			if (wp != NULL)
				w = wp->window;
		}
		if (w != NULL)
			wl = winlink_find_by_window(&s->windows, w);
	}

	if (c == NULL)
		return (CMD_RETURN_NORMAL);
	if (server_client_check_nested(c)) {
		cmdq_error(cmdq, "sessions should be nested with care, "
		    "unset $TMUX to force");
		return (CMD_RETURN_ERROR);
	}

	if (wl != NULL) {
		if (wp != NULL)
			window_set_active_pane(wp->window, wp);
		session_set_current(s, wl);
	}

	if (cflag != NULL) {
		ft = format_create();
		format_defaults(ft, cmd_find_client(cmdq, NULL, 1), s,
		    NULL, NULL);
		cp = format_expand(ft, cflag);
		format_free(ft);

		fd = open(cp, O_RDONLY|O_DIRECTORY);
		free(cp);
		if (fd == -1) {
			cmdq_error(cmdq, "bad working directory: %s",
			    strerror(errno));
			return (CMD_RETURN_ERROR);
		}
		close(s->cwd);
		s->cwd = fd;
	}

	if (c->session != NULL) {
		if (dflag) {
			/*
			 * Can't use server_write_session in case attaching to
			 * the same session as currently attached to.
			 */
			TAILQ_FOREACH(c_loop, &clients, entry) {
				if (c_loop->session != s || c == c_loop)
					continue;
				server_write_client(c, MSG_DETACH,
				    c_loop->session->name,
				    strlen(c_loop->session->name) + 1);
			}
		}

		if (!Eflag) {
			update = options_get_string(&s->options,
			    "update-environment");
			environ_update(update, &c->environ, &s->environ);
		}

		c->session = s;
		status_timer_start(c);
		notify_attached_session_changed(c);
		session_update_activity(s, NULL);
		gettimeofday(&s->last_attached_time, NULL);
		server_redraw_client(c);
		s->curw->flags &= ~WINLINK_ALERTFLAGS;
	} else {
		if (server_client_open(c, &cause) != 0) {
			cmdq_error(cmdq, "open terminal failed: %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}

		if (rflag)
			c->flags |= CLIENT_READONLY;

		if (dflag) {
			server_write_session(s, MSG_DETACH, s->name,
			    strlen(s->name) + 1);
		}

		if (!Eflag) {
			update = options_get_string(&s->options,
			    "update-environment");
			environ_update(update, &c->environ, &s->environ);
		}

		c->session = s;
		status_timer_start(c);
		notify_attached_session_changed(c);
		session_update_activity(s, NULL);
		gettimeofday(&s->last_attached_time, NULL);
		server_redraw_client(c);
		s->curw->flags &= ~WINLINK_ALERTFLAGS;

		server_write_ready(c);
		cmdq->client_exit = 0;
	}
	recalculate_sizes();
	server_update_socket();

	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_attach_session_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;

	return (cmd_attach_session(cmdq, args_get(args, 't'),
	    args_has(args, 'd'), args_has(args, 'r'), args_get(args, 'c'),
	    args_has(args, 'E')));
}
