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
 * Break pane off into a window.
 */

int	cmd_break_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_break_pane_entry = {
	"break-pane", "breakp",
	CMD_PANE_WINDOW_USAGE " [-d]",
	CMD_DFLAG,
	cmd_pane_init,
	cmd_pane_parse,
	cmd_break_pane_exec,
       	cmd_pane_send,
	cmd_pane_recv,
	cmd_pane_free,
	cmd_pane_print
};

int
cmd_break_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_pane_data	*data = self->data;
	struct winlink		*wl;
	struct session		*s;
	struct window_pane	*wp;
	struct window		*w;
	char			*cause;

	if ((wl = cmd_find_window(ctx, data->target, &s)) == NULL)
		return (-1);
	if (data->pane == -1)
		wp = wl->window->active;
	else {
		wp = window_pane_at_index(wl->window, data->pane);
		if (wp == NULL) {
			ctx->error(ctx, "no pane: %d", data->pane);
			return (-1);
		}
	}

	if (window_count_panes(wl->window) == 1) {
		ctx->error(ctx, "can't break pane: %d", data->pane);
		return (-1);
	}

	TAILQ_REMOVE(&wl->window->panes, wp, entry);
	if (wl->window->active == wp) {
		wl->window->active = TAILQ_PREV(wp, window_panes, entry);
		if (wl->window->active == NULL)
			wl->window->active = TAILQ_NEXT(wp, entry);
	}
 	layout_refresh(wl->window, 0);

 	w = wp->window = window_create1(s->sx, s->sy);
 	TAILQ_INSERT_HEAD(&w->panes, wp, entry);
 	w->active = wp;
 	w->name = default_window_name(w);

 	wl = session_attach(s, w, -1, &cause); /* can't fail */
 	if (!(data->flags & CMD_DFLAG))
 		session_select(s, wl->idx);
 	layout_refresh(w, 0);

	server_redraw_session(s);

	return (0);
}
