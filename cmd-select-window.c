/* $OpenBSD: cmd-select-window.c,v 1.30 2021/08/21 10:22:39 nicm Exp $ */

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

	.args = { "LlSnpTt:", 0, 0, NULL },
	.usage = "[-LlnpST] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_select_window_exec
};

const struct cmd_entry cmd_next_window_entry = {
	.name = "next-window",
	.alias = "next",

	.args = { "at:", 0, 0, NULL },
	.usage = "[-a] " CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_select_window_exec
};

const struct cmd_entry cmd_previous_window_entry = {
	.name = "previous-window",
	.alias = "prev",

	.args = { "at:", 0, 0, NULL },
	.usage = "[-a] " CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_select_window_exec
};

const struct cmd_entry cmd_last_window_entry = {
	.name = "last-window",
	.alias = "last",

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_SESSION_USAGE,

	.target = { 't', CMD_FIND_SESSION, 0 },

	.flags = 0,
	.exec = cmd_select_window_exec
};

static enum cmd_retval
cmd_select_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct client		*c = cmdq_get_client(item);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct winlink		*wl = target->wl, *current_wl;
	struct session		*s = target->s;
	enum active_window_mode	 mode;
	int			 next, previous, last, activity;

	next = (cmd_get_entry(self) == &cmd_next_window_entry);
	if (args_has(args, 'n'))
		next = 1;
	previous = (cmd_get_entry(self) == &cmd_previous_window_entry);
	if (args_has(args, 'p'))
		previous = 1;
	last = (cmd_get_entry(self) == &cmd_last_window_entry);
	if (args_has(args, 'l'))
		last = 1;

	if (args_has(args, 'L') || args_has(args, 'S')) {
		if (c == NULL || c->session == NULL) {
			cmdq_error(item, "no current client");
			return (CMD_RETURN_ERROR);
		}
		if (args_has(args, 'S'))
			mode = ACTIVE_SHARED;
		else
			mode = ACTIVE_LOCAL;
		active_set_local_window(c, s, mode);
		if (!next &&
		    !previous &&
		    !last &&
		    !args_has(args, 't') &&
		    !args_has(args, 'T')) {
			if (current->s == s) {
				if (c->session == s)
					cmd_find_from_client(current, c, 0);
				else
					cmd_find_from_session(current, s, 0);
			}
			if (c->session == s)
				server_redraw_client(c);
			else
				server_redraw_session(s);
			cmdq_insert_hook(s, item, current, "after-select-window");
			recalculate_sizes();
			return (CMD_RETURN_NORMAL);
		}
	}

	if (active_is_local_window(c, s))
		mode = ACTIVE_LOCAL;
	else
		mode = ACTIVE_SHARED;
	if (next || previous || last) {
		activity = args_has(args, 'a');
		if (next) {
			if (active_next_window(c, s, activity) != 0) {
				cmdq_error(item, "no next window");
				return (CMD_RETURN_ERROR);
			}
		} else if (previous) {
			if (active_previous_window(c, s, activity) != 0) {
				cmdq_error(item, "no previous window");
				return (CMD_RETURN_ERROR);
			}
		} else {
			if (active_last_window(c, s) != 0) {
				cmdq_error(item, "no last window");
				return (CMD_RETURN_ERROR);
			}
		}
		if (mode == ACTIVE_LOCAL && c != NULL && c->session == s)
			cmd_find_from_client(current, c, 0);
		else
			cmd_find_from_session(current, s, 0);
		if (mode == ACTIVE_LOCAL && c != NULL && c->session == s)
			server_redraw_client(c);
		else
			server_redraw_session(s);
		cmdq_insert_hook(s, item, current, "after-select-window");
	} else {
		/*
		 * If -T and select-window is invoked on same window as
		 * current, switch to previous window.
		 */
		current_wl = active_get_effective_winlink(c, s);
		if (args_has(args, 'T') && wl == current_wl) {
			if (active_last_window(c, s) != 0) {
				cmdq_error(item, "no last window");
				return (-1);
			}
			if (current->s == s) {
				if (mode == ACTIVE_LOCAL && c != NULL &&
				    c->session == s)
					cmd_find_from_client(current, c, 0);
				else
					cmd_find_from_session(current, s, 0);
			}
			if (mode == ACTIVE_LOCAL && c != NULL && c->session == s)
				server_redraw_client(c);
			else
				server_redraw_session(s);
		} else if (active_select_window(c, s, wl) == 0) {
			if (mode == ACTIVE_LOCAL && c != NULL && c->session == s)
				cmd_find_from_client(current, c, 0);
			else
				cmd_find_from_session(current, s, 0);
			if (mode == ACTIVE_LOCAL && c != NULL && c->session == s)
				server_redraw_client(c);
			else
				server_redraw_session(s);
		}
		cmdq_insert_hook(s, item, current, "after-select-window");
	}
	if (c != NULL && c->session != NULL)
		active_get_effective_window(c, s)->latest = c;
	recalculate_sizes();

	return (CMD_RETURN_NORMAL);
}
