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

static enum args_parse_type	cmd_bind_key_args_parse(struct args *, u_int,
				    char **);
static enum cmd_retval		cmd_bind_key_exec(struct cmd *,
				    struct cmdq_item *);

const struct cmd_entry cmd_bind_key_entry = {
	.name = "bind-key",
	.alias = "bind",

	.args = { "nrN:T:", 1, -1, cmd_bind_key_args_parse },
	.usage = "[-nr] [-T key-table] [-N note] key "
	         "[command [arguments]]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_bind_key_exec
};

static enum args_parse_type
cmd_bind_key_args_parse(__unused struct args *args, __unused u_int idx,
    __unused char **cause)
{
	return (ARGS_PARSE_COMMANDS_OR_STRING);
}

static enum cmd_retval
cmd_bind_key_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		 *args = cmd_get_args(self);
	key_code		  key;
	const char		 *tablename, *note = args_get(args, 'N');
	struct cmd_parse_result	 *pr;
	int			  repeat;
	struct args_value	 *value;
	u_int			  count = args_count(args);

	key = key_string_lookup_string(args_string(args, 0));
	if (key == KEYC_NONE || key == KEYC_UNKNOWN) {
		cmdq_error(item, "unknown key: %s", args_string(args, 0));
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'T'))
		tablename = args_get(args, 'T');
	else if (args_has(args, 'n'))
		tablename = "root";
	else
		tablename = "prefix";
	repeat = args_has(args, 'r');

	if (count == 1) {
		key_bindings_add(tablename, key, note, repeat, NULL);
		return (CMD_RETURN_NORMAL);
	}

	value = args_value(args, 1);
	if (count == 2 && value->type == ARGS_COMMANDS) {
		key_bindings_add(tablename, key, note, repeat, value->cmdlist);
		value->cmdlist->references++;
		return (CMD_RETURN_NORMAL);
	}

	if (count == 2)
		pr = cmd_parse_from_string(args_string(args, 1), NULL);
	else {
		pr = cmd_parse_from_arguments(args_values(args) + 1, count - 1,
		    NULL);
	}
	switch (pr->status) {
	case CMD_PARSE_ERROR:
		cmdq_error(item, "%s", pr->error);
		free(pr->error);
		return (CMD_RETURN_ERROR);
	case CMD_PARSE_SUCCESS:
		break;
	}
	key_bindings_add(tablename, key, note, repeat, pr->cmdlist);
	return (CMD_RETURN_NORMAL);
}
