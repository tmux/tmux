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
#include <time.h>

#include "tmux.h"

/*
 * Get Session CWD
 */


static enum cmd_retval	cmd_get_cwd_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_get_cwd_entry = {
	.name = "get-cwd",
	.alias = "gcwd",

	.args = { "F:t:", 0, 0 },
	.usage = "[-F format] " CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = CMD_READONLY|CMD_AFTERHOOK,

	.exec = cmd_get_cwd_exec
};

static enum cmd_retval
cmd_get_cwd_exec(__unused struct cmd *self, struct cmdq_item *item)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s;

	if (target == NULL)
		return (CMD_RETURN_ERROR);

	s = target->s;

	if (s == NULL)
		return (CMD_RETURN_ERROR);

	cmdq_print(item, "%s", s->cwd);

	return (CMD_RETURN_NORMAL);
}
