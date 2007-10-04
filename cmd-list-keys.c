/* $Id: cmd-list-keys.c,v 1.1 2007-10-04 00:18:59 nicm Exp $ */

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

#include <getopt.h>
#include <string.h>

#include "tmux.h"

/*
 * List key bindings.
 */

void	cmd_list_keys_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_list_keys_entry = {
	CMD_LISTKEYS, "list-keys", "lsk", CMD_NOSESSION,
	NULL,
	NULL,
	cmd_list_keys_exec,
	NULL,
	NULL,
	NULL
};

void
cmd_list_keys_exec(unused void *ptr, struct cmd_ctx *ctx)
{
	struct client	*c = ctx->client;
	struct binding	*bd;
	const char	*key;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&key_bindings); i++) {
		bd = ARRAY_ITEM(&key_bindings, i);
		if ((key = key_string_lookup_key(bd->key)) == NULL)
			continue;
		ctx->print(ctx, "%11s: %s", key, bd->cmd->entry->name);
	}

	if (!(ctx->flags & CMD_KEY))
		server_write_client(c, MSG_EXIT, NULL, 0);
}
