/* $Id: cmd-list-windows.c,v 1.22 2008-09-08 17:40:50 nicm Exp $ */

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

#include <unistd.h>

#include "tmux.h"

/*
 * List windows on given session.
 */

void	cmd_list_windows_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_list_windows_entry = {
	"list-windows", "lsw",
	CMD_TARGET_SESSION_USAGE,
	0,
	cmd_target_init,
	cmd_target_parse,
	cmd_list_windows_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_list_windows_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	u_int			 i;
	unsigned long long	 size;
	const char		*name;

	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return;

	RB_FOREACH(wl, winlinks, &s->windows) {
		w = wl->window;

		size = 0;
		for (i = 0; i < w->base.hsize; i++)
			size += w->base.grid_size[i] * 3;
		size += w->base.hsize * (sizeof *w->base.grid_data);
		size += w->base.hsize * (sizeof *w->base.grid_attr);
		size += w->base.hsize * (sizeof *w->base.grid_fg);
		size += w->base.hsize * (sizeof *w->base.grid_bg);
		size += w->base.hsize * (sizeof *w->base.grid_size);

		if (w->fd != -1)
			name = ttyname(w->fd);
		else
			name = "";
		ctx->print(ctx,
		    "%d: %s \"%s\" (%s) [%ux%u] [history %u/%u, %llu bytes]",
		    wl->idx, w->name, w->base.title, name,
		    screen_size_x(&w->base), screen_size_y(&w->base),
		    w->base.hsize, w->base.hlimit, size);
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
