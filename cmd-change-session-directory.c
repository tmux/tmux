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
 * Change session working directory.
 */

enum cmd_retval	 cmd_change_session_directory_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_change_session_directory_entry = {
	.name = "change-session-directory",
	.alias = "move-session",

	.args = { "t:", 1, 1 },
	.usage = CMD_TARGET_SESSION_USAGE " new-dir",

	.tflag = CMD_SESSION,

	.flags = 0,
	.exec = cmd_change_session_directory_exec
};

enum cmd_retval
cmd_change_session_directory_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct session	*s = cmdq->state.tflag.s;
	char			*cwd;
	struct format_tree	*ft;
	const char	*newdir;

	newdir = args->argv[0];
	if (newdir != NULL) {
		ft = format_create(cmdq, 0);
		cwd = format_expand(ft, newdir);
		format_free(ft);

		free((void *)s->cwd);
		s->cwd = cwd;
	}

	return (CMD_RETURN_NORMAL);
}
