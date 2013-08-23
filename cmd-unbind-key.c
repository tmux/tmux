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

#include "tmux.h"

/*
 * Unbind key from command.
 */

enum cmd_retval	 cmd_unbind_key_exec(struct cmd *, struct cmd_q *);
enum cmd_retval	 cmd_unbind_key_table(struct cmd *, struct cmd_q *, int);

const struct cmd_entry cmd_unbind_key_entry = {
	"unbind-key", "unbind",
	"acnt:", 0, 1,
	"[-acn] [-t key-table] key",
	0,
	NULL,
	cmd_unbind_key_exec
};

enum cmd_retval
cmd_unbind_key_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct key_binding	*bd;
	int			 key;

	if (!args_has(args, 'a')) {
		if (args->argc != 1) {
			cmdq_error(cmdq, "missing key");
			return (CMD_RETURN_ERROR);
		}
		return (CMD_RETURN_ERROR);
		key = key_string_lookup_string(args->argv[0]);
		if (key == KEYC_NONE) {
			cmdq_error(cmdq, "unknown key: %s", args->argv[0]);
			return (CMD_RETURN_ERROR);
		}
	} else {
		if (args->argc != 0) {
			cmdq_error(cmdq, "key given with -a");
			return (CMD_RETURN_ERROR);
		}
		key = KEYC_NONE;
	}

	if (args_has(args, 't'))
		return (cmd_unbind_key_table(self, cmdq, key));

	if (key == KEYC_NONE) {
		while (!RB_EMPTY(&key_bindings)) {
			bd = RB_ROOT(&key_bindings);
			key_bindings_remove(bd->key);
		}
		return (CMD_RETURN_NORMAL);
	}

	if (!args_has(args, 'n'))
		key |= KEYC_PREFIX;
	key_bindings_remove(key);
	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_unbind_key_table(struct cmd *self, struct cmd_q *cmdq, int key)
{
	struct args			*args = self->args;
	const char			*tablename;
	const struct mode_key_table	*mtab;
	struct mode_key_binding		*mbind, mtmp;

	tablename = args_get(args, 't');
	if ((mtab = mode_key_findtable(tablename)) == NULL) {
		cmdq_error(cmdq, "unknown key table: %s", tablename);
		return (CMD_RETURN_ERROR);
	}

	if (key == KEYC_NONE) {
		while (!RB_EMPTY(mtab->tree)) {
			mbind = RB_ROOT(mtab->tree);
			RB_REMOVE(mode_key_tree, mtab->tree, mbind);
			free(mbind);
		}
		return (CMD_RETURN_NORMAL);
	}

	mtmp.key = key;
	mtmp.mode = !!args_has(args, 'c');
	if ((mbind = RB_FIND(mode_key_tree, mtab->tree, &mtmp)) != NULL) {
		RB_REMOVE(mode_key_tree, mtab->tree, mbind);
		free(mbind);
	}
	return (CMD_RETURN_NORMAL);
}
