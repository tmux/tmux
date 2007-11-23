/* $Id: cmd-list-windows.c,v 1.14 2007-11-23 13:11:43 nicm Exp $ */

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
#include <unistd.h>

#include "tmux.h"

/*
 * List windows on given session.
 */

void	cmd_list_windows_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_list_windows_entry = {
	"list-windows", "lsw", NULL,
	CMD_NOCLIENT,
	NULL,
	cmd_list_windows_exec,
	NULL,
	NULL,
	NULL
};

void
cmd_list_windows_exec(unused void *ptr, struct cmd_ctx *ctx)
{
	struct winlink		*wl;
	struct window		*w;
	u_int			 i;
	unsigned long long	 size;

	RB_FOREACH(wl, winlinks, &ctx->session->windows) {
		w = wl->window;

		size = 0;
		for (i = 0; i < w->screen.hsize; i++)
			size += w->screen.grid_size[i] * 3;
		size += w->screen.hsize * (sizeof *w->screen.grid_data);
		size += w->screen.hsize * (sizeof *w->screen.grid_attr);
		size += w->screen.hsize * (sizeof *w->screen.grid_colr);
		size += w->screen.hsize * (sizeof *w->screen.grid_size);

		ctx->print(ctx,
		    "%d: %s \"%s\" (%s) [%ux%u] [history %u/%u, %llu bytes]",
		    wl->idx, w->name, w->screen.title, ttyname(w->fd),
		    screen_size_x(&w->screen), screen_size_y(&w->screen),
		    w->screen.hsize, w->screen.hlimit, size);
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
