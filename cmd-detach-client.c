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

#include <string.h>

#include "tmux.h"

/*
 * Detach a client.
 */

enum cmd_retval	 cmd_detach_client_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_detach_client_entry = {
	"detach-client", "detach",
	"as:t:P", 0, 0,
	"[-aP] [-s target-session] " CMD_TARGET_CLIENT_USAGE,
	CMD_READONLY,
	cmd_detach_client_exec
};

const struct cmd_entry cmd_suspend_client_entry = {
	"suspend-client", "suspendc",
	"t:", 0, 0,
	CMD_TARGET_CLIENT_USAGE,
	0,
	cmd_detach_client_exec
};

enum cmd_retval
cmd_detach_client_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct client	*c, *cloop;
	struct session	*s;
	enum msgtype	 msgtype;

	if (self->entry == &cmd_suspend_client_entry) {
		if ((c = cmd_find_client(cmdq, args_get(args, 't'), 0)) == NULL)
			return (CMD_RETURN_ERROR);
		tty_stop_tty(&c->tty);
		c->flags |= CLIENT_SUSPENDED;
		proc_send(c->peer, MSG_SUSPEND, -1, NULL, 0);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'P'))
		msgtype = MSG_DETACHKILL;
	else
		msgtype = MSG_DETACH;

	if (args_has(args, 's')) {
		s = cmd_find_session(cmdq, args_get(args, 's'), 0);
		if (s == NULL)
			return (CMD_RETURN_ERROR);

		TAILQ_FOREACH(cloop, &clients, entry) {
			if (cloop->session == s)
				server_client_detach(cloop, msgtype);
		}
		return (CMD_RETURN_STOP);
	}

	c = cmd_find_client(cmdq, args_get(args, 't'), 0);
	if (c == NULL)
		return (CMD_RETURN_ERROR);

	if (args_has(args, 'a')) {
		TAILQ_FOREACH(cloop, &clients, entry) {
			if (cloop->session != NULL && cloop != c)
				server_client_detach(cloop, msgtype);
		}
		return (CMD_RETURN_NORMAL);
	}

	server_client_detach(c, msgtype);
	return (CMD_RETURN_STOP);
}
