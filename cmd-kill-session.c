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

#include "tmux.h"

/*
 * Destroy session, detaching all clients attached to it and destroying any
 * windows linked only to this session.
 *
 * Note this deliberately has no alias to make it hard to hit by accident.
 */

static enum cmd_retval	cmd_kill_session_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_kill_session_entry = {
	.name = "kill-session",
	.alias = NULL,

	.args = { "aCt:", 0, 0 },
	.usage = "[-aC] " CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_kill_session_exec
};

static enum cmd_retval
cmd_kill_session_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = self->args;
	struct session	*s, *sloop, *stmp;
	struct winlink	*wl;

	s = item->target.s;

	if (args_has(args, 'C')) {
		RB_FOREACH(wl, winlinks, &s->windows) {
			wl->window->flags &= ~WINDOW_ALERTFLAGS;
			wl->flags &= ~WINLINK_ALERTFLAGS;
		}
		server_redraw_session(s);
	} else if (args_has(args, 'a')) {
		RB_FOREACH_SAFE(sloop, sessions, &sessions, stmp) {
			if (sloop != s) {
				server_destroy_session(sloop);
				session_destroy(sloop, __func__);
			}
		}
	} else {
		server_destroy_session(s);
		session_destroy(s, __func__);
	}
	return (CMD_RETURN_NORMAL);
}
