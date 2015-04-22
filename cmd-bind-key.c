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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Bind a key to a command, this recurses through cmd_*.
 */

enum cmd_retval	 cmd_bind_key_exec(struct cmd *, struct cmd_q *);

enum cmd_retval	 cmd_bind_key_mode_table(struct cmd *, struct cmd_q *, int);

const struct cmd_entry cmd_bind_key_entry = {
	"bind-key", "bind",
	"cnrt:T:", 1, -1,
	"[-cnr] [-t mode-table] [-T key-table] key command [arguments]",
	0,
	cmd_bind_key_exec
};

enum cmd_retval
cmd_bind_key_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	char		*cause;
	struct cmd_list	*cmdlist;
	int		 key;
	const char	*tablename;

	if (args_has(args, 't')) {
		if (args->argc != 2 && args->argc != 3) {
			cmdq_error(cmdq, "not enough arguments");
			return (CMD_RETURN_ERROR);
		}
	} else {
		if (args->argc < 2) {
			cmdq_error(cmdq, "not enough arguments");
			return (CMD_RETURN_ERROR);
		}
	}

	key = key_string_lookup_string(args->argv[0]);
	if (key == KEYC_NONE) {
		cmdq_error(cmdq, "unknown key: %s", args->argv[0]);
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 't'))
		return (cmd_bind_key_mode_table(self, cmdq, key));

	if (args_has(args, 'T'))
		tablename = args_get(args, 'T');
	else if (args_has(args, 'n'))
		tablename = "root";
	else
		tablename = "prefix";

	cmdlist = cmd_list_parse(args->argc - 1, args->argv + 1, NULL, 0,
	    &cause);
	if (cmdlist == NULL) {
		cmdq_error(cmdq, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	key_bindings_add(tablename, key, args_has(args, 'r'), cmdlist);
	return (CMD_RETURN_NORMAL);
}

enum cmd_retval
cmd_bind_key_mode_table(struct cmd *self, struct cmd_q *cmdq, int key)
{
	struct args			*args = self->args;
	const char			*tablename;
	const struct mode_key_table	*mtab;
	struct mode_key_binding		*mbind, mtmp;
	enum mode_key_cmd		 cmd;
	const char			*arg;

	tablename = args_get(args, 't');
	if ((mtab = mode_key_findtable(tablename)) == NULL) {
		cmdq_error(cmdq, "unknown key table: %s", tablename);
		return (CMD_RETURN_ERROR);
	}

	cmd = mode_key_fromstring(mtab->cmdstr, args->argv[1]);
	if (cmd == MODEKEY_NONE) {
		cmdq_error(cmdq, "unknown command: %s", args->argv[1]);
		return (CMD_RETURN_ERROR);
	}

	switch (cmd) {
	case MODEKEYCOPY_APPENDSELECTION:
	case MODEKEYCOPY_COPYSELECTION:
	case MODEKEYCOPY_STARTNAMEDBUFFER:
		if (args->argc == 2)
			arg = NULL;
		else {
			arg = args->argv[2];
			if (strcmp(arg, "-x") != 0) {
				cmdq_error(cmdq, "unknown argument");
				return (CMD_RETURN_ERROR);
			}
		}
		break;
	case MODEKEYCOPY_COPYPIPE:
		if (args->argc != 3) {
			cmdq_error(cmdq, "no argument given");
			return (CMD_RETURN_ERROR);
		}
		arg = args->argv[2];
		break;
	default:
		if (args->argc != 2) {
			cmdq_error(cmdq, "no argument allowed");
			return (CMD_RETURN_ERROR);
		}
		arg = NULL;
		break;
	}

	mtmp.key = key;
	mtmp.mode = !!args_has(args, 'c');
	if ((mbind = RB_FIND(mode_key_tree, mtab->tree, &mtmp)) == NULL) {
		mbind = xmalloc(sizeof *mbind);
		mbind->key = mtmp.key;
		mbind->mode = mtmp.mode;
		RB_INSERT(mode_key_tree, mtab->tree, mbind);
	}
	mbind->cmd = cmd;
	mbind->arg = arg != NULL ? xstrdup(arg) : NULL;
	return (CMD_RETURN_NORMAL);
}
