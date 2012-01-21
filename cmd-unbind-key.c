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
 * Unbind key from command.
 */

int	cmd_unbind_key_check(struct args *);
int	cmd_unbind_key_exec(struct cmd *, struct cmd_ctx *);

int	cmd_unbind_key_table(struct cmd *, struct cmd_ctx *, int);

const struct cmd_entry cmd_unbind_key_entry = {
	"unbind-key", "unbind",
	"acnt:", 0, 1,
	"[-acn] [-t key-table] key",
	0,
	NULL,
	cmd_unbind_key_check,
	cmd_unbind_key_exec
};

int
cmd_unbind_key_check(struct args *args)
{
	if (args_has(args, 'a') && (args->argc != 0 || args_has(args, 't')))
	    return (-1);
	if (!args_has(args, 'a') && args->argc != 1)
		return (-1);
	return (0);
}

int
cmd_unbind_key_exec(struct cmd *self, unused struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct key_binding	*bd;
	int			 key;

	if (args_has(args, 'a')) {
		while (!RB_EMPTY(&key_bindings)) {
			bd = RB_ROOT(&key_bindings);
			key_bindings_remove(bd->key);
		}
		return (0);
	}

	key = key_string_lookup_string(args->argv[0]);
	if (key == KEYC_NONE) {
		ctx->error(ctx, "unknown key: %s", args->argv[0]);
		return (-1);
	}

	if (args_has(args, 't'))
		return (cmd_unbind_key_table(self, ctx, key));

	if (!args_has(args, 'n'))
		key |= KEYC_PREFIX;
	key_bindings_remove(key);
	return (0);
}

int
cmd_unbind_key_table(struct cmd *self, struct cmd_ctx *ctx, int key)
{
	struct args			*args = self->args;
	const char			*tablename;
	const struct mode_key_table	*mtab;
	struct mode_key_binding		*mbind, mtmp;

	tablename = args_get(args, 't');
	if ((mtab = mode_key_findtable(tablename)) == NULL) {
		ctx->error(ctx, "unknown key table: %s", tablename);
		return (-1);
	}

	mtmp.key = key;
	mtmp.mode = !!args_has(args, 'c');
	if ((mbind = RB_FIND(mode_key_tree, mtab->tree, &mtmp)) != NULL) {
		RB_REMOVE(mode_key_tree, mtab->tree, mbind);
		xfree(mbind);
	}
	return (0);
}
