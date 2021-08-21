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
 * Destroy window.
 */

static enum cmd_retval	cmd_kill_window_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_kill_window_entry = {
	.name = "kill-window",
	.alias = "killw",

	.args = { "at:", 0, 0, NULL },
	.usage = "[-a] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_kill_window_exec
};

const struct cmd_entry cmd_unlink_window_entry = {
	.name = "unlink-window",
	.alias = "unlinkw",

	.args = { "kt:", 0, 0, NULL },
	.usage = "[-k] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_kill_window_exec
};

static enum cmd_retval
cmd_kill_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct winlink		*wl = target->wl, *loop;
	struct window		*w = wl->window;
	struct session		*s = target->s;
	u_int			 found;

	if (cmd_get_entry(self) == &cmd_unlink_window_entry) {
		if (!args_has(args, 'k') && !session_is_linked(s, w)) {
			cmdq_error(item, "window only linked to one session");
			return (CMD_RETURN_ERROR);
		}
		server_unlink_window(s, wl);
		recalculate_sizes();
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'a')) {
		if (RB_PREV(winlinks, &s->windows, wl) == NULL &&
		    RB_NEXT(winlinks, &s->windows, wl) == NULL)
			return (CMD_RETURN_NORMAL);

		/* Kill all windows except the current one. */
		do {
			found = 0;
			RB_FOREACH(loop, winlinks, &s->windows) {
				if (loop->window != wl->window) {
					server_kill_window(loop->window, 0);
					found++;
					break;
				}
			}
		} while (found != 0);

		/*
		 * If the current window appears in the session more than once,
		 * kill it as well.
		 */
		found = 0;
		RB_FOREACH(loop, winlinks, &s->windows) {
			if (loop->window == wl->window)
				found++;
		}
		if (found > 1)
			server_kill_window(wl->window, 0);

		server_renumber_all();
		return (CMD_RETURN_NORMAL);
	}

	server_kill_window(wl->window, 1);
	return (CMD_RETURN_NORMAL);
}
