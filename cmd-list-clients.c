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
#include <time.h>

#include "tmux.h"

/*
 * List all clients.
 */

int	cmd_list_clients_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_list_clients_entry = {
	"list-clients", "lsc",
	"",
	0,
	NULL,
	NULL,
	cmd_list_clients_exec,
	NULL,
	NULL,
	NULL,
	NULL
};

int
cmd_list_clients_exec(unused struct cmd *self, struct cmd_ctx *ctx)
{
	struct client	*c;
	u_int		 i;
	const char	*s_utf8;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;

		if (c->tty.flags & TTY_UTF8)
			s_utf8 = " (utf8)";
		else
			s_utf8 = "";
		ctx->print(ctx, "%s: %s [%ux%u %s]%s", c->tty.path,
		    c->session->name, c->tty.sx, c->tty.sy,
		    c->tty.termname, s_utf8);
	}

	return (0);
}
