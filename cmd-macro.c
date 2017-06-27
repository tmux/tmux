/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2017 Frank Hebold <frank.hebld@chelnok.de>
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

static enum cmd_retval	cmd_macro_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_macro_entry = {
	.name = "macro",

	.args = { "", 2, -1 },
	.usage = "name command [arguments]",

	.flags = CMD_AFTERHOOK,
	.exec = cmd_macro_exec
};

static enum cmd_retval
cmd_macro_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = self->args;
	char		*cause;
	struct cmd_list	*cmdlist;
	char		*name;

	if (args->argv == NULL) {
		return (CMD_RETURN_ERROR);
	}
	name = xstrdup(args->argv[0]);

	cmdlist = cmd_list_parse(args->argc - 1, args->argv + 1, NULL, 0,
	    &cause);
	if (cmdlist == NULL) {
		cmdq_error(item, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	macro_add(name, cmdlist);
	return (CMD_RETURN_NORMAL);
}
