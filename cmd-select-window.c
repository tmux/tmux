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
 * Select window by index.
 */

static enum cmd_retval	cmd_select_window_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_select_window_entry = {
	.name = "select-window",
	.alias = "selectw",

	.args = { "lnpTt:", 0, 0 },
	.usage = "[-lnpT] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_select_window_exec
};

const struct cmd_entry cmd_next_window_entry = {
	.name = "next-window",
	.alias = "next",

	.args = { "at:", 0, 0 },
	.usage = "[-a] " CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_select_window_exec
};

const struct cmd_entry cmd_previous_window_entry = {
	.name = "previous-window",
	.alias = "prev",

	.args = { "at:", 0, 0 },
	.usage = "[-a] " CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_select_window_exec
};

const struct cmd_entry cmd_last_window_entry = {
	.name = "last-window",
	.alias = "last",

	.args = { "t:", 0, 0 },
	.usage = CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_select_window_exec
};

static enum cmd_retval
cmd_select_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct cmd_find_state	*current = &item->shared->current;
	struct winlink		*wl = item->target.wl;
	struct session		*s = item->target.s;
	int			 next, previous, last, activity;

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
		activity = args_has(self->args, 'a');
		if (next) {
			if (session_next(s, activity) != 0) {
				cmdq_error(item, "no next window");
				return (CMD_RETURN_ERROR);
			}
		} else if (previous) {
			if (session_previous(s, activity) != 0) {
				cmdq_error(item, "no previous window");
				return (CMD_RETURN_ERROR);
			}
		} else {
			if (session_last(s) != 0) {
				cmdq_error(item, "no last window");
				return (CMD_RETURN_ERROR);
			}
		}
		cmd_find_from_session(current, s, 0);
		server_redraw_session(s);
		hooks_insert(s->hooks, item, current, "after-select-window");
	} else {
		/*
		 * If -T and select-window is invoked on same window as
		 * current, switch to previous window.
		 */
		if (args_has(self->args, 'T') && wl == s->curw) {
			if (session_last(s) != 0) {
				cmdq_error(item, "no last window");
				return (-1);
			}
			if (current->s == s)
				cmd_find_from_session(current, s, 0);
			server_redraw_session(s);
		} else if (session_select(s, wl->idx) == 0) {
			cmd_find_from_session(current, s, 0);
			server_redraw_session(s);
		}
		hooks_insert(s->hooks, item, current, "after-select-window");
	}
	recalculate_sizes();

	return (CMD_RETURN_NORMAL);
}
