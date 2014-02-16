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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Switch client to a different session.
 */

void		 cmd_switch_client_key_binding(struct cmd *, int);
enum cmd_retval	 cmd_switch_client_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_switch_client_entry = {
	"switch-client", "switchc",
	"lc:npt:r", 0, 0,
	"[-lnpr] [-c target-client] [-t target-session]",
	CMD_READONLY,
	cmd_switch_client_key_binding,
	cmd_switch_client_exec
};

void
cmd_switch_client_key_binding(struct cmd *self, int key)
{
	self->args = args_create(0);
	switch (key) {
	case '(':
		args_set(self->args, 'p', NULL);
		break;
	case ')':
		args_set(self->args, 'n', NULL);
		break;
	case 'L':
		args_set(self->args, 'l', NULL);
		break;
	}
}

enum cmd_retval
cmd_switch_client_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c;
	struct session		*s = NULL;
	struct winlink		*wl = NULL;
	struct window 		*w = NULL;
	struct window_pane	*wp = NULL;
	const char		*tflag;

	if ((c = cmd_find_client(cmdq, args_get(args, 'c'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

	if (args_has(args, 'r')) {
		if (c->flags & CLIENT_READONLY) {
			c->flags &= ~CLIENT_READONLY;
			cmdq_info(cmdq, "made client writable");
		} else {
			c->flags |= CLIENT_READONLY;
			cmdq_info(cmdq, "made client read-only");
		}
	}

	tflag = args_get(args, 't');
	if (args_has(args, 'n')) {
		if ((s = session_next_session(c->session)) == NULL) {
			cmdq_error(cmdq, "can't find next session");
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'p')) {
		if ((s = session_previous_session(c->session)) == NULL) {
			cmdq_error(cmdq, "can't find previous session");
			return (CMD_RETURN_ERROR);
		}
	} else if (args_has(args, 'l')) {
		if (c->last_session != NULL && session_alive(c->last_session))
			s = c->last_session;
		if (s == NULL) {
			cmdq_error(cmdq, "can't find last session");
			return (CMD_RETURN_ERROR);
		}
	} else {
		if (tflag == NULL) {
			if ((s = cmd_find_session(cmdq, tflag, 1)) == NULL)
				return (CMD_RETURN_ERROR);
		} else if (tflag[strcspn(tflag, ":.")] != '\0') {
			if ((wl = cmd_find_pane(cmdq, tflag, &s, &wp)) == NULL)
				return (CMD_RETURN_ERROR);
		} else {
			if ((s = cmd_find_session(cmdq, tflag, 1)) == NULL)
				return (CMD_RETURN_ERROR);
			w = cmd_lookup_windowid(tflag);
			if (w == NULL &&
			    (wp = cmd_lookup_paneid(tflag)) != NULL)
				w = wp->window;
			if (w != NULL)
				wl = winlink_find_by_window(&s->windows, w);
		}

		if (cmdq->client == NULL)
			return (CMD_RETURN_NORMAL);

		if (wl != NULL) {
			if (wp != NULL)
				window_set_active_pane(wp->window, wp);
			session_set_current(s, wl);
		}
	}

	if (c->session != NULL)
		c->last_session = c->session;
	c->session = s;
	session_update_activity(s);

	recalculate_sizes();
	server_check_unattached();
	server_redraw_client(c);
	s->curw->flags &= ~WINLINK_ALERTFLAGS;

	return (CMD_RETURN_NORMAL);
}
