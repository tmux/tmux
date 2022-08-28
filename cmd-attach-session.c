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

static enum cmd_retval	cmd_attach_session_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_attach_session_entry = {
	.name = "attach-session",
	.alias = "attach",

	.args = { "c:dEf:rt:x", 0, 0, NULL },
	.usage = "[-dErx] [-c working-directory] [-f flags] "
	         CMD_TARGET_SESSION_USAGE,

	/* -t is special */

	.flags = CMD_STARTSERVER|CMD_READONLY,
	.exec = cmd_attach_session_exec
};

enum cmd_retval
cmd_attach_session(struct cmdq_item *item, const char *tflag, int dflag,
    int xflag, int rflag, const char *cflag, int Eflag, const char *fflag)
{
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	 target;
	enum cmd_find_type	 type;
	int			 flags;
	struct client		*c = cmdq_get_client(item), *c_loop;
	struct session		*s;
	struct winlink		*wl;
	struct window_pane	*wp;
	char			*cwd, *cause;
	enum msgtype		 msgtype;

	if (RB_EMPTY(&sessions)) {
		cmdq_error(item, "no sessions");
		return (CMD_RETURN_ERROR);
	}

	if (c == NULL)
		return (CMD_RETURN_NORMAL);

	if (server_client_check_nested(c)) {
		cmdq_error(item, "sessions should be nested with care, "
		    "unset $TMUX to force");
		return (CMD_RETURN_ERROR);
	}

	if (tflag != NULL && tflag[strcspn(tflag, ":.")] != '\0') {
		type = CMD_FIND_PANE;
		flags = 0;
	} else {
		type = CMD_FIND_SESSION;
		flags = CMD_FIND_PREFER_UNATTACHED;
	}
	if (cmd_find_target(&target, item, tflag, type, flags) != 0)
		return (CMD_RETURN_ERROR);
	s = target.s;
	wl = target.wl;
	wp = target.wp;

	if (wl != NULL) {
		if (wp != NULL)
			window_set_active_pane(wp->window, wp, 1);
		session_set_current(s, wl);
		if (wp != NULL)
			cmd_find_from_winlink_pane(current, wl, wp, 0);
		else
			cmd_find_from_winlink(current, wl, 0);
	}

	if (cflag != NULL) {
		cwd = format_single(item, cflag, c, s, wl, wp);
		free((void *)s->cwd);
		s->cwd = cwd;
	}
	if (fflag)
		server_client_set_flags(c, fflag);
	if (rflag)
		c->flags |= (CLIENT_READONLY|CLIENT_IGNORESIZE);

	c->last_session = c->session;
	if (c->session != NULL) {
		if (dflag || xflag) {
			if (xflag)
				msgtype = MSG_DETACHKILL;
			else
				msgtype = MSG_DETACH;
			TAILQ_FOREACH(c_loop, &clients, entry) {
				if (c_loop->session != s || c == c_loop)
					continue;
				server_client_detach(c_loop, msgtype);
			}
		}
		if (!Eflag)
			environ_update(s->options, c->environ, s->environ);

		server_client_set_session(c, s);
		if (~cmdq_get_flags(item) & CMDQ_STATE_REPEAT)
			server_client_set_key_table(c, NULL);
	} else {
		if (server_client_open(c, &cause) != 0) {
			cmdq_error(item, "open terminal failed: %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}

		if (dflag || xflag) {
			if (xflag)
				msgtype = MSG_DETACHKILL;
			else
				msgtype = MSG_DETACH;
			TAILQ_FOREACH(c_loop, &clients, entry) {
				if (c_loop->session != s || c == c_loop)
					continue;
				server_client_detach(c_loop, msgtype);
			}
		}
		if (!Eflag)
			environ_update(s->options, c->environ, s->environ);

		server_client_set_session(c, s);
		server_client_set_key_table(c, NULL);

		if (~c->flags & CLIENT_CONTROL)
			proc_send(c->peer, MSG_READY, -1, NULL, 0);
		notify_client("client-attached", c);
		c->flags |= CLIENT_ATTACHED;
	}

	if (cfg_finished)
		cfg_show_causes(s);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_attach_session_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = cmd_get_args(self);

	return (cmd_attach_session(item, args_get(args, 't'),
	    args_has(args, 'd'), args_has(args, 'x'), args_has(args, 'r'),
	    args_get(args, 'c'), args_has(args, 'E'), args_get(args, 'f')));
}
