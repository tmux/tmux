/* $Id: cmd-resize-pane-down.c,v 1.1 2009-01-12 19:23:14 nicm Exp $ */

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
 * Increase pane size.
 */

void	cmd_resize_pane_down_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_resize_pane_down_entry = {
	"resize-pane-down", "resizep-dn",
	CMD_TARGET_WINDOW_USAGE " [adjustment]",
	CMD_ZEROONEARG,
	cmd_target_init,
	cmd_target_parse,
	cmd_resize_pane_down_exec,
       	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_resize_pane_down_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	int			 adjust;
	const char	       	*errstr;
	u_int			 y0, y1;
	
	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return;
#ifdef notyet
	if (data->pane == -1)
		wp = wl->window->active;
	else {
		if (data->pane > 1 || wl->window->panes[data->pane] == NULL) {
			ctx->error(ctx, "no pane: %d", data->pane);
			return;
		}
		wp = wl->window->panes[data->pane];
	}
#endif

	if (data->arg == NULL)
		adjust = 1;
	else {
		adjust = strtonum(data->arg, 0, INT_MAX, &errstr);
		if (errstr != NULL) {
			ctx->error(ctx, "adjustment %s: %s", errstr, data->arg);
			return;
		}
	}

	if (wl->window->panes[1] == NULL)
		goto out;

	y0 = wl->window->panes[0]->sy;
	y1 = wl->window->panes[1]->sy;
	if (adjust >= y1)
		adjust = y1 - 1;
	y0 += adjust;
	y1 -= adjust;		
	window_pane_resize(wl->window->panes[0], wl->window->sx, y0);
	window_pane_resize(wl->window->panes[1], wl->window->sx, y1);
	wl->window->panes[1]->yoff = y0 + 1;		

	server_redraw_window(wl->window);

out:
	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
