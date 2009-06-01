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

#include "tmux.h"

/*
 * Rotate the panes in a window.
 */

void	cmd_rotate_window_init(struct cmd *, int);
int	cmd_rotate_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_rotate_window_entry = {
	"rotate-window", "rotatew",
	"[-DU] " CMD_TARGET_WINDOW_USAGE,
	CMD_BIGUFLAG|CMD_BIGDFLAG,
	cmd_rotate_window_init,
	cmd_target_parse,
	cmd_rotate_window_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_rotate_window_init(struct cmd *self, int key)
{
	struct cmd_target_data	*data;

	cmd_target_init(self, key);
	data = self->data;

	if (key == KEYC_ADDESC('o'))
		data->flags |= CMD_BIGDFLAG;
}

int
cmd_rotate_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp, *wp2;
	u_int			 sx, sy, xoff, yoff;

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return (-1);
	w = wl->window;

	if (data->flags & CMD_BIGDFLAG) {
		wp = TAILQ_LAST(&w->panes, window_panes);
		TAILQ_REMOVE(&w->panes, wp, entry);
		TAILQ_INSERT_HEAD(&w->panes, wp, entry);

		xoff = wp->xoff; yoff = wp->yoff;
		sx = wp->sx; sy = wp->sy;
		TAILQ_FOREACH(wp, &w->panes, entry) {
			if ((wp2 = TAILQ_NEXT(wp, entry)) == NULL)
				break;
			wp->xoff = wp2->xoff; wp->yoff = wp2->yoff;
			window_pane_resize(wp, wp2->sx, wp2->sy);
		}
		wp->xoff = xoff; wp->yoff = yoff;
		window_pane_resize(wp, sx, sy);

		if ((wp = TAILQ_PREV(w->active, window_panes, entry)) == NULL)
			wp = TAILQ_LAST(&w->panes, window_panes);
		window_set_active_pane(w, wp);
	} else {
		wp = TAILQ_FIRST(&w->panes);
		TAILQ_REMOVE(&w->panes, wp, entry);
		TAILQ_INSERT_TAIL(&w->panes, wp, entry);

		xoff = wp->xoff; yoff = wp->yoff;
		sx = wp->sx; sy = wp->sy;
		TAILQ_FOREACH_REVERSE(wp, &w->panes, window_panes, entry) {
			if ((wp2 = TAILQ_PREV(wp, window_panes, entry)) == NULL)
				break;
			wp->xoff = wp2->xoff; wp->yoff = wp2->yoff;
			window_pane_resize(wp, wp2->sx, wp2->sy);
		}
		wp->xoff = xoff; wp->yoff = yoff;
		window_pane_resize(wp, sx, sy);

		if ((wp = TAILQ_NEXT(w->active, entry)) == NULL)
			wp = TAILQ_FIRST(&w->panes);
		window_set_active_pane(w, wp);
	}

	layout_refresh(w, 0);

	return (0);
}
