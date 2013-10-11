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
 * Destroy session, detaching all clients attached to it and destroying any
 * windows linked only to this session.
 *
 * Note this deliberately has no alias to make it hard to hit by accident.
 */

enum cmd_retval	 cmd_kill_session_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_kill_session_entry = {
	"kill-session", NULL,
	"at:", 0, 0,
	"[-a] " CMD_TARGET_SESSION_USAGE,
	0,
	NULL,
	cmd_kill_session_exec
};

enum cmd_retval
cmd_kill_session_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct session	*s, *s2, *s3;

	if ((s = cmd_find_session(cmdq, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

	if (args_has(args, 'a')) {
		RB_FOREACH_SAFE(s2, sessions, &sessions, s3) {
			if (s != s2) {
				server_destroy_session(s2);
				session_destroy(s2);
			}
		}
	} else {
		server_destroy_session(s);
		session_destroy(s);
	}
	return (CMD_RETURN_NORMAL);
}
