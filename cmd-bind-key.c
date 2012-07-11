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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Bind a key to a command, this recurses through cmd_*.
 */

enum cmd_retval	 cmd_bind_key_check(struct args *);
enum cmd_retval	 cmd_bind_key_exec(struct cmd *, struct cmd_ctx *);

enum cmd_retval	 cmd_bind_key_table(struct cmd *, struct cmd_ctx *, int);

const struct cmd_entry cmd_bind_key_entry = {
	"bind-key", "bind",
	"cnrt:", 1, -1,
	"[-cnr] [-t key-table] key command [arguments]",
	0,
	NULL,
	cmd_bind_key_check,
	cmd_bind_key_exec
};

enum cmd_retval
cmd_bind_key_check(struct args *args)
{
	if (args_has(args, 't')) {
		if (args->argc != 2)
			return (CMD_RETURN_ERROR);
	} else {
		if (args->argc < 2)
			return (CMD_RETURN_ERROR);
	}
	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_bind_key_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	char		*cause;
	struct cmd_list	*cmdlist;
	int		 key;

	key = key_string_lookup_string(args->argv[0]);
	if (key == KEYC_NONE) {
		ctx->error(ctx, "unknown key: %s", args->argv[0]);
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 't'))
		return (cmd_bind_key_table(self, ctx, key));

	cmdlist = cmd_list_parse(args->argc - 1, args->argv + 1, &cause);
	if (cmdlist == NULL) {
		ctx->error(ctx, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	if (!args_has(args, 'n'))
	    key |= KEYC_PREFIX;
	key_bindings_add(key, args_has(args, 'r'), cmdlist);
	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_bind_key_table(struct cmd *self, struct cmd_ctx *ctx, int key)
{
	struct args			*args = self->args;
	const char			*tablename;
	const struct mode_key_table	*mtab;
	struct mode_key_binding		*mbind, mtmp;
	enum mode_key_cmd		 cmd;

	tablename = args_get(args, 't');
	if ((mtab = mode_key_findtable(tablename)) == NULL) {
		ctx->error(ctx, "unknown key table: %s", tablename);
		return (CMD_RETURN_ERROR);
	}

	cmd = mode_key_fromstring(mtab->cmdstr, args->argv[1]);
	if (cmd == MODEKEY_NONE) {
		ctx->error(ctx, "unknown command: %s", args->argv[1]);
		return (CMD_RETURN_ERROR);
	}

	mtmp.key = key;
	mtmp.mode = !!args_has(args, 'c');
	if ((mbind = RB_FIND(mode_key_tree, mtab->tree, &mtmp)) != NULL) {
		mbind->cmd = cmd;
		return (CMD_RETURN_NORMAL);
	}
	mbind = xmalloc(sizeof *mbind);
	mbind->key = mtmp.key;
	mbind->mode = mtmp.mode;
	mbind->cmd = cmd;
	RB_INSERT(mode_key_tree, mtab->tree, mbind);
	return (CMD_RETURN_NORMAL);
}
