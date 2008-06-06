/* $Id: cmd-link-window.c,v 1.21 2008-06-06 17:20:15 nicm Exp $ */

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

#include <getopt.h>
#include <stdlib.h>

#include "tmux.h"

/*
 * Link a window into another session.
 */

void	cmd_link_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_link_window_entry = {
	"link-window", "linkw",
	"[-dk] " CMD_SRCDST_WINDOW_USAGE,
	CMD_DFLAG|CMD_KFLAG,
	cmd_srcdst_init,
	cmd_srcdst_parse,
	cmd_link_window_exec,
	cmd_srcdst_send,
	cmd_srcdst_recv,
	cmd_srcdst_free,
	cmd_srcdst_print
};

void
cmd_link_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_srcdst_data	*data = self->data;
	struct session		*s;
	struct winlink		*wl_src, *wl_dst;
	int			 idx;

	if ((wl_src = cmd_find_window(ctx, data->src, NULL)) == NULL)
		return;

	if (arg_parse_window(data->dst, &s, &idx) != 0) {
		ctx->error(ctx, "bad window: %s", data->dst);
		return;
	}
	if (s == NULL)
		s = ctx->cursession;
	if (s == NULL)
		s = cmd_current_session(ctx);
	if (s == NULL) {
		ctx->error(ctx, "session not found: %s", data->dst);
		return;
	}

	wl_dst = NULL;
	if (idx != -1)
		wl_dst = winlink_find_by_index(&s->windows, idx);
	if (wl_dst != NULL) {
		if (wl_dst->window == wl_src->window)
			goto out;

		if (data->flags & CMD_KFLAG) {
			/*
			 * Can't use session_detach as it will destroy session
			 * if this makes it empty.
			 */
			session_alert_cancel(s, wl_dst);
			winlink_remove(&s->windows, wl_dst);
			
			/* Force select/redraw if current. */
			if (wl_dst == s->curw) {
				data->flags &= ~CMD_DFLAG;
				s->curw = NULL;
			}
			if (wl_dst == s->lastw)
				s->lastw = NULL;
			
			/*
			 * Can't error out after this or there could be an
			 * empty session!
			 */
		}
	}

	wl_dst = session_attach(s, wl_src->window, idx);
	if (wl_dst == NULL) {
		ctx->error(ctx, "index in use: %d", idx);
		return;
	}

	if (data->flags & CMD_DFLAG)
		server_status_session(s);
	else {
		session_select(s, wl_dst->idx);
		server_redraw_session(s);
	}

out:
	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
