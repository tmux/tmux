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
 * List all commands with usages.
 */

enum cmd_retval	 cmd_list_commands_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_list_commands_entry = {
	"list-commands", "lscm",
	"", 0, 0,
	"",
	0,
	NULL,
	cmd_list_commands_exec
};

enum cmd_retval
cmd_list_commands_exec(unused struct cmd *self, struct cmd_q *cmdq)
{
	const struct cmd_entry 	      **entryp;

	for (entryp = cmd_table; *entryp != NULL; entryp++) {
		if ((*entryp)->alias != NULL) {
			cmdq_print(cmdq, "%s (%s) %s", (*entryp)->name,
			    (*entryp)->alias, (*entryp)->usage);
		} else {
			cmdq_print(cmdq, "%s %s", (*entryp)->name,
			    (*entryp)->usage);
		}
	}

	return (CMD_RETURN_NORMAL);
}
