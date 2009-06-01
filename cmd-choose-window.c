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
 * Enter choice mode to choose a window.
 */

int	cmd_choose_window_exec(struct cmd *, struct cmd_ctx *);

void	cmd_choose_window_callback(void *, int);

const struct cmd_entry cmd_choose_window_entry = {
	"choose-window", NULL,
	CMD_TARGET_WINDOW_USAGE,
	0,
	cmd_target_init,
	cmd_target_parse,
	cmd_choose_window_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

struct cmd_choose_window_data {
	u_int	session;
};

int
cmd_choose_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct cmd_choose_window_data	*cdata;
	struct session			*s;
	struct winlink			*wl, *wm;
	struct window			*w;
	u_int			 	 idx, cur;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}
	s = ctx->curclient->session;

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return (-1);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (0);

	cur = idx = 0;
	RB_FOREACH(wm, winlinks, &s->windows) {
		w = wm->window;

		if (wm == s->curw)
			cur = idx;
		idx++;

		window_choose_add(wl->window->active,
		    wm->idx, "%3d: %s [%ux%u %s] (%u panes)", wm->idx, w->name,
		    w->sx, w->sy, layout_name(w), window_count_panes(w));
	}

	cdata = xmalloc(sizeof *cdata);
	if (session_index(s, &cdata->session) != 0)
		fatalx("session not found");

	window_choose_ready(
	    wl->window->active, cur, cmd_choose_window_callback, cdata);

 	return (0);
}

void
cmd_choose_window_callback(void *data, int idx)
{
	struct cmd_choose_window_data	*cdata = data;
	struct session			*s;

	if (idx != -1 && cdata->session <= ARRAY_LENGTH(&sessions) - 1) {
		s = ARRAY_ITEM(&sessions, cdata->session);
		if (s != NULL && session_select(s, idx) == 0)
			server_redraw_session(s);
		recalculate_sizes();
	}
	xfree(cdata);
}
