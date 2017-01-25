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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Bind a key to a command.
 */

static enum cmd_retval	cmd_bind_key_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_bind_key_entry = {
	.name = "bind-key",
	.alias = "bind",

	.args = { "cnrT:", 2, -1 },
	.usage = "[-cnr] [-T key-table] key "
	         "command [arguments]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_bind_key_exec
};

static enum cmd_retval
cmd_bind_key_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = self->args;
	char		*cause;
	struct cmd_list	*cmdlist;
	key_code	 key;
	const char	*tablename;

	key = key_string_lookup_string(args->argv[0]);
	if (key == KEYC_NONE || key == KEYC_UNKNOWN) {
		cmdq_error(item, "unknown key: %s", args->argv[0]);
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'T'))
		tablename = args_get(args, 'T');
	else if (args_has(args, 'n'))
		tablename = "root";
	else
		tablename = "prefix";

	cmdlist = cmd_list_parse(args->argc - 1, args->argv + 1, NULL, 0,
	    &cause);
	if (cmdlist == NULL) {
		cmdq_error(item, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	key_bindings_add(tablename, key, args_has(args, 'r'), cmdlist);
	return (CMD_RETURN_NORMAL);
}
