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

#include "tmux.h"

/*
 * Detach a client.
 */

int	cmd_detach_client_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_detach_client_entry = {
	"detach-client", "detach",
	"s:t:P", 0, 0,
	"[-P] [-s target-session] " CMD_TARGET_CLIENT_USAGE,
	CMD_READONLY,
	NULL,
	NULL,
	cmd_detach_client_exec
};

int
cmd_detach_client_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct client	*c;
	struct session 	*s;
	enum msgtype     msgtype;
	u_int 		 i;

	if (args_has(args, 'P'))
		msgtype = MSG_DETACHKILL;
	else
		msgtype = MSG_DETACH;

	if (args_has(args, 's')) {
		s = cmd_find_session(ctx, args_get(args, 's'), 0);
		if (s == NULL)
			return (-1);

		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c != NULL && c->session == s)
				server_write_client(c, msgtype, NULL, 0);
		}
	} else {
		c = cmd_find_client(ctx, args_get(args, 't'));
		if (c == NULL)
			return (-1);

		server_write_client(c, msgtype, NULL, 0);
	}

	return (0);
}
