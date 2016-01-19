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
 * Attach existing session to the current terminal.
 */

enum cmd_retval	cmd_attach_session_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_attach_session_entry = {
	.name = "attach-session",
	.alias = "attach",

	.args = { "c:dErt:", 0, 0 },
	.usage = "[-dEr] [-c working-directory] " CMD_TARGET_SESSION_USAGE,

	.tflag = CMD_SESSION_WITHPANE,

	.flags = CMD_STARTSERVER,
	.exec = cmd_attach_session_exec
};

enum cmd_retval
cmd_attach_session(struct cmd_q *cmdq, int dflag, int rflag, const char *cflag,
    int Eflag)
{
	struct session		*s = cmdq->state.tflag.s;
	struct client		*c = cmdq->client, *c_loop;
	struct winlink		*wl = cmdq->state.tflag.wl;
	struct window_pane	*wp = cmdq->state.tflag.wp;
	const char		*update;
	char			*cause, *cwd;
	struct format_tree	*ft;

	if (RB_EMPTY(&sessions)) {
		cmdq_error(cmdq, "no sessions");
		return (CMD_RETURN_ERROR);
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
		ft = format_create(cmdq, 0);
		format_defaults(ft, c, s, wl, wp);
		cwd = format_expand(ft, cflag);
		format_free(ft);

		free((void *)s->cwd);
		s->cwd = cwd;
	}

	if (c->session != NULL) {
		if (dflag) {
			TAILQ_FOREACH(c_loop, &clients, entry) {
				if (c_loop->session != s || c == c_loop)
					continue;
				server_client_detach(c_loop, MSG_DETACH);
			}
		}

		if (!Eflag) {
			update = options_get_string(s->options,
			    "update-environment");
			environ_update(update, c->environ, s->environ);
		}

		c->session = s;
		server_client_set_key_table(c, NULL);
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
			TAILQ_FOREACH(c_loop, &clients, entry) {
				if (c_loop->session != s || c == c_loop)
					continue;
				server_client_detach(c_loop, MSG_DETACH);
			}
		}

		if (!Eflag) {
			update = options_get_string(s->options,
			    "update-environment");
			environ_update(update, c->environ, s->environ);
		}

		c->session = s;
		server_client_set_key_table(c, NULL);
		status_timer_start(c);
		notify_attached_session_changed(c);
		session_update_activity(s, NULL);
		gettimeofday(&s->last_attached_time, NULL);
		server_redraw_client(c);
		s->curw->flags &= ~WINLINK_ALERTFLAGS;

		if (~c->flags & CLIENT_CONTROL)
			proc_send(c->peer, MSG_READY, -1, NULL, 0);
		hooks_run(c->session->hooks, c, NULL, "client-attached");
		cmdq->client_exit = 0;
	}
	recalculate_sizes();
	alerts_check_session(s);
	server_update_socket();

	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_attach_session_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;

	return (cmd_attach_session(cmdq, args_has(args, 'd'),
	    args_has(args, 'r'), args_get(args, 'c'), args_has(args, 'E')));
}
