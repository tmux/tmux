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

#include <signal.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Kill the server and do nothing else.
 */

static enum cmd_retval	cmd_kill_server_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_kill_server_entry = {
	.name = "kill-server",
	.alias = NULL,

	.args = { "", 0, 0, NULL },
	.usage = "",

	.flags = 0,
	.exec = cmd_kill_server_exec
};

const struct cmd_entry cmd_start_server_entry = {
	.name = "start-server",
	.alias = "start",

	.args = { "", 0, 0, NULL },
	.usage = "",

	.flags = CMD_STARTSERVER,
	.exec = cmd_kill_server_exec
};

static enum cmd_retval
cmd_kill_server_exec(struct cmd *self, __unused struct cmdq_item *item)
{
	if (cmd_get_entry(self) == &cmd_kill_server_entry)
		kill(getpid(), SIGTERM);

	return (CMD_RETURN_NORMAL);
}
