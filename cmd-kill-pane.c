/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Kill pane.
 */

static enum cmd_retval	cmd_kill_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_kill_pane_entry = {
	.name = "kill-pane",
	.alias = "killp",

	.args = { "at:", 0, 0 },
	.usage = "[-a] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_kill_pane_exec
};

static enum cmd_retval
cmd_kill_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct winlink		*wl = item->target.wl;
	struct window_pane	*loopwp, *tmpwp, *wp = item->target.wp;

	server_unzoom_window(wl->window);

	if (args_has(self->args, 'a')) {
		TAILQ_FOREACH_SAFE(loopwp, &wl->window->panes, entry, tmpwp) {
			if (loopwp == wp)
				continue;
			layout_close_pane(loopwp);
			window_remove_pane(wl->window, loopwp);
		}
		server_redraw_window(wl->window);
		return (CMD_RETURN_NORMAL);
	}

	if (window_count_panes(wl->window) == 1) {
		server_kill_window(wl->window);
		recalculate_sizes();
	} else {
		layout_close_pane(wp);
		window_remove_pane(wl->window, wp);
		server_redraw_window(wl->window);
	}
	return (CMD_RETURN_NORMAL);
}
