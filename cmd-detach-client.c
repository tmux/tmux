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

static enum cmd_retval	cmd_detach_client_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_detach_client_entry = {
	.name = "detach-client",
	.alias = "detach",

	.args = { "aE:s:t:P", 0, 0, NULL },
	.usage = "[-aP] [-E shell-command] "
	         "[-s target-session] " CMD_TARGET_CLIENT_USAGE,

	.source = { 's', CMD_FIND_SESSION, CMD_FIND_CANFAIL },

	.flags = CMD_READONLY|CMD_CLIENT_TFLAG,
	.exec = cmd_detach_client_exec
};

const struct cmd_entry cmd_suspend_client_entry = {
	.name = "suspend-client",
	.alias = "suspendc",

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_CLIENT_USAGE,

	.flags = CMD_CLIENT_TFLAG,
	.exec = cmd_detach_client_exec
};

static enum cmd_retval
cmd_detach_client_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct client		*tc = cmdq_get_target_client(item), *loop;
	struct session		*s;
	enum msgtype		 msgtype;
	const char		*cmd = args_get(args, 'E');

	if (cmd_get_entry(self) == &cmd_suspend_client_entry) {
		server_client_suspend(tc);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'P'))
		msgtype = MSG_DETACHKILL;
	else
		msgtype = MSG_DETACH;

	if (args_has(args, 's')) {
		s = source->s;
		if (s == NULL)
			return (CMD_RETURN_NORMAL);
		TAILQ_FOREACH(loop, &clients, entry) {
			if (loop->session == s) {
				if (cmd != NULL)
					server_client_exec(loop, cmd);
				else
					server_client_detach(loop, msgtype);
			}
		}
		return (CMD_RETURN_STOP);
	}

	if (args_has(args, 'a')) {
		TAILQ_FOREACH(loop, &clients, entry) {
			if (loop->session != NULL && loop != tc) {
				if (cmd != NULL)
					server_client_exec(loop, cmd);
				else
					server_client_detach(loop, msgtype);
			}
		}
		return (CMD_RETURN_NORMAL);
	}

	if (cmd != NULL)
		server_client_exec(tc, cmd);
	else
		server_client_detach(tc, msgtype);
	return (CMD_RETURN_STOP);
}
