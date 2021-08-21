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

#include "tmux.h"

/*
 * Unbind key from command.
 */

static enum cmd_retval	cmd_unbind_key_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_unbind_key_entry = {
	.name = "unbind-key",
	.alias = "unbind",

	.args = { "anqT:", 0, 1, NULL },
	.usage = "[-anq] [-T key-table] key",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_unbind_key_exec
};

static enum cmd_retval
cmd_unbind_key_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = cmd_get_args(self);
	key_code	 key;
	const char	*tablename, *keystr = args_string(args, 0);
	int		 quiet = args_has(args, 'q');

	if (args_has(args, 'a')) {
		if (keystr != NULL) {
			if (!quiet)
				cmdq_error(item, "key given with -a");
			return (CMD_RETURN_ERROR);
		}

		tablename = args_get(args, 'T');
		if (tablename == NULL) {
			if (args_has(args, 'n'))
				tablename = "root";
			else
				tablename = "prefix";
		}
		if (key_bindings_get_table(tablename, 0) == NULL) {
			if (!quiet) {
				cmdq_error(item, "table %s doesn't exist" ,
				    tablename);
			}
			return (CMD_RETURN_ERROR);
		}

		key_bindings_remove_table(tablename);
		return (CMD_RETURN_NORMAL);
	}

	if (keystr == NULL) {
		if (!quiet)
			cmdq_error(item, "missing key");
		return (CMD_RETURN_ERROR);
	}

	key = key_string_lookup_string(keystr);
	if (key == KEYC_NONE || key == KEYC_UNKNOWN) {
		if (!quiet)
			cmdq_error(item, "unknown key: %s", keystr);
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'T')) {
		tablename = args_get(args, 'T');
		if (key_bindings_get_table(tablename, 0) == NULL) {
			if (!quiet) {
				cmdq_error(item, "table %s doesn't exist" ,
				    tablename);
			}
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'n'))
		tablename = "root";
	else
		tablename = "prefix";
	key_bindings_remove(tablename, key);
	return (CMD_RETURN_NORMAL);
}
