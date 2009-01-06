/* $Id: cmd-list-keys.c,v 1.11 2009-01-06 14:10:32 nicm Exp $ */

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
 * List key bindings.
 */

void	cmd_list_keys_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_list_keys_entry = {
	"list-keys", "lsk",
	"",
	0,
	NULL,
	NULL,
	cmd_list_keys_exec,
	NULL,
	NULL,
	NULL,
	NULL
};

void
cmd_list_keys_exec(unused struct cmd *self, struct cmd_ctx *ctx)
{
	struct key_binding	*bd;
	const char		*key;
	char			 s[BUFSIZ];

	SPLAY_FOREACH(bd, key_bindings, &key_bindings) {
		if ((key = key_string_lookup_key(bd->key)) == NULL)
			continue;
		if (bd->cmd->entry->print == NULL) {
			ctx->print(ctx, "%11s: %s", key, bd->cmd->entry->name);
			continue;
		}
		bd->cmd->entry->print(bd->cmd, s, sizeof s);
		ctx->print(ctx, "%11s: %s", key, s);
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
