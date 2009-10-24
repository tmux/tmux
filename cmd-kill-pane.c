/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

int	cmd_kill_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_kill_pane_entry = {
	"kill-pane", "killp",
	"[-a] " CMD_TARGET_PANE_USAGE,
	0, CMD_CHFLAG('a'),
	cmd_target_init,
	cmd_target_parse,
	cmd_kill_pane_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_kill_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	struct window_pane	*loopwp, *nextwp, *wp;

	if ((wl = cmd_find_pane(ctx, data->target, NULL, &wp)) == NULL)
		return (-1);

	if (window_count_panes(wl->window) == 1) {
		/* Only one pane, kill the window. */
		server_kill_window(wl->window);
		recalculate_sizes();
		return (0);
	}

	if (data->chflags & CMD_CHFLAG('a')) {
		loopwp = TAILQ_FIRST(&wl->window->panes);
		while (loopwp != NULL) {
			nextwp = TAILQ_NEXT(loopwp, entry);
			if (loopwp != wp) {
				layout_close_pane(loopwp);
				window_remove_pane(wl->window, loopwp);
			}
			loopwp = nextwp;
		}
	} else {
		layout_close_pane(wp);
		window_remove_pane(wl->window, wp);
	}
	server_redraw_window(wl->window);

	return (0);
}
