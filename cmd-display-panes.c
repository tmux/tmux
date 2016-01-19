/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Display panes on a client.
 */

enum cmd_retval	 cmd_display_panes_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_display_panes_entry = {
	.name = "display-panes",
	.alias = "displayp",

	.args = { "t:", 0, 0 },
	.usage = CMD_TARGET_CLIENT_USAGE,

	.tflag = CMD_CLIENT,

	.flags = 0,
	.exec = cmd_display_panes_exec
};

enum cmd_retval
cmd_display_panes_exec(__unused struct cmd *self, struct cmd_q *cmdq)
{
	server_set_identify(cmdq->state.c);

	return (CMD_RETURN_NORMAL);
}
