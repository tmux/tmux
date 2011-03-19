/* $Id: cmd-detach-client.c,v 1.13 2011-03-19 23:27:35 tcunha Exp $ */

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
	"t:P", 0, 0,
	"[-P] " CMD_TARGET_CLIENT_USAGE,
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

	if ((c = cmd_find_client(ctx, args_get(args, 't'))) == NULL)
		return (-1);

	if (args_has(args, 'P'))
		server_write_client(c, MSG_DETACHKILL, NULL, 0);
	else
		server_write_client(c, MSG_DETACH, NULL, 0);

	return (0);
}
