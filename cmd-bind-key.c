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

	.args = { "nrN:T:", 1, -1 },
	.usage = "[-nr] [-T key-table] [-N note] key "
	         "[command [arguments]]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_bind_key_exec
};

static enum cmd_retval
cmd_bind_key_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		 *args = cmd_get_args(self);
	key_code		  key;
	const char		 *tablename, *note = args_get(args, 'N');
	struct cmd_parse_result	 *pr;
	char			**argv = args->argv;
	int			  argc = args->argc, repeat;

	key = key_string_lookup_string(argv[0]);
	if (key == KEYC_NONE || key == KEYC_UNKNOWN) {
		cmdq_error(item, "unknown key: %s", argv[0]);
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'T'))
		tablename = args_get(args, 'T');
	else if (args_has(args, 'n'))
		tablename = "root";
	else
		tablename = "prefix";
	repeat = args_has(args, 'r');

	if (argc != 1) {
		if (argc == 2)
			pr = cmd_parse_from_string(argv[1], NULL);
		else
			pr = cmd_parse_from_arguments(argc - 1, argv + 1, NULL);
		switch (pr->status) {
		case CMD_PARSE_EMPTY:
			cmdq_error(item, "empty command");
			return (CMD_RETURN_ERROR);
		case CMD_PARSE_ERROR:
			cmdq_error(item, "%s", pr->error);
			free(pr->error);
			return (CMD_RETURN_ERROR);
		case CMD_PARSE_SUCCESS:
			break;
		}
		key_bindings_add(tablename, key, note, repeat, pr->cmdlist);
	} else
		key_bindings_add(tablename, key, note, repeat, NULL);
	return (CMD_RETURN_NORMAL);
}
