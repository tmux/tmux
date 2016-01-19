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

#include <string.h>

#include "tmux.h"

/*
 * Detach a client.
 */

enum cmd_retval	 cmd_detach_client_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_detach_client_entry = {
	.name = "detach-client",
	.alias = "detach",

	.args = { "as:t:P", 0, 0 },
	.usage = "[-P] [-a] [-s target-session] " CMD_TARGET_CLIENT_USAGE,

	.sflag = CMD_SESSION,
	.tflag = CMD_CLIENT,

	.flags = CMD_READONLY,
	.exec = cmd_detach_client_exec
};

const struct cmd_entry cmd_suspend_client_entry = {
	.name = "suspend-client",
	.alias = "suspendc",

	.args = { "t:", 0, 0 },
	.usage = CMD_TARGET_CLIENT_USAGE,

	.tflag = CMD_CLIENT,

	.flags = 0,
	.exec = cmd_detach_client_exec
};

enum cmd_retval
cmd_detach_client_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct client	*c = cmdq->state.c, *cloop;
	struct session	*s;
	enum msgtype	 msgtype;

	if (self->entry == &cmd_suspend_client_entry) {
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
		s = cmdq->state.sflag.s;
		TAILQ_FOREACH(cloop, &clients, entry) {
			if (cloop->session == s)
				server_client_detach(cloop, msgtype);
		}
		return (CMD_RETURN_STOP);
	}

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
