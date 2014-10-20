/* $OpenBSD$ */

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

#include "tmux.h"

/*
 * Select window by index.
 */

enum cmd_retval	 cmd_select_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_select_window_entry = {
	"select-window", "selectw",
	"lnpTt:", 0, 0,
	"[-lnpT] " CMD_TARGET_WINDOW_USAGE,
	0,
	cmd_select_window_exec
};

const struct cmd_entry cmd_next_window_entry = {
	"next-window", "next",
	"at:", 0, 0,
	"[-a] " CMD_TARGET_SESSION_USAGE,
	0,
	cmd_select_window_exec
};

const struct cmd_entry cmd_previous_window_entry = {
	"previous-window", "prev",
	"at:", 0, 0,
	"[-a] " CMD_TARGET_SESSION_USAGE,
	0,
	cmd_select_window_exec
};

const struct cmd_entry cmd_last_window_entry = {
	"last-window", "last",
	"t:", 0, 0,
	CMD_TARGET_SESSION_USAGE,
	0,
	cmd_select_window_exec
};

enum cmd_retval
cmd_select_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args	*args = self->args;
	struct winlink	*wl;
	struct session	*s;
	int		 next, previous, last, activity;

	next = self->entry == &cmd_next_window_entry;
	if (args_has(self->args, 'n'))
		next = 1;
	previous = self->entry == &cmd_previous_window_entry;
	if (args_has(self->args, 'p'))
		previous = 1;
	last = self->entry == &cmd_last_window_entry;
	if (args_has(self->args, 'l'))
		last = 1;

	if (next || previous || last) {
		s = cmd_find_session(cmdq, args_get(args, 't'), 0);
		if (s == NULL)
			return (CMD_RETURN_ERROR);

		activity = args_has(self->args, 'a');
		if (next) {
			if (session_next(s, activity) != 0) {
				cmdq_error(cmdq, "no next window");
				return (CMD_RETURN_ERROR);
			}
		} else if (previous) {
			if (session_previous(s, activity) != 0) {
				cmdq_error(cmdq, "no previous window");
				return (CMD_RETURN_ERROR);
			}
		} else {
			if (session_last(s) != 0) {
				cmdq_error(cmdq, "no last window");
				return (CMD_RETURN_ERROR);
			}
		}

		server_redraw_session(s);
	} else {
		wl = cmd_find_window(cmdq, args_get(args, 't'), &s);
		if (wl == NULL)
			return (CMD_RETURN_ERROR);

		/*
		 * If -T and select-window is invoked on same window as
		 * current, switch to previous window.
		 */
		if (args_has(self->args, 'T') && wl == s->curw) {
			if (session_last(s) != 0) {
				cmdq_error(cmdq, "no last window");
				return (-1);
			}
			server_redraw_session(s);
		} else if (session_select(s, wl->idx) == 0)
			server_redraw_session(s);
	}
	recalculate_sizes();

	return (CMD_RETURN_NORMAL);
}
