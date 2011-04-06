/* $Id: cmd-list-panes.c,v 1.9 2011-04-06 22:20:16 nicm Exp $ */

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

#include <unistd.h>

#include "tmux.h"

/*
 * List panes on given window.
 */

int	cmd_list_panes_exec(struct cmd *, struct cmd_ctx *);

void	cmd_list_panes_server(struct cmd_ctx *);
void	cmd_list_panes_session(struct session *, struct cmd_ctx *);
void	cmd_list_panes_window(struct winlink *, struct cmd_ctx *);

const struct cmd_entry cmd_list_panes_entry = {
	"list-panes", "lsp",
	"ast:", 0, 0,
	"[-as] [-t target]",
	0,
	NULL,
	NULL,
	cmd_list_panes_exec
};

int
cmd_list_panes_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct session	*s;
	struct winlink	*wl;

	if (args_has(args, 'a'))
		cmd_list_panes_server(ctx);
	else if (args_has(args, 's')) {
		s = cmd_find_session(ctx, args_get(args, 't'));
		if (s == NULL)
			return (-1);
		cmd_list_panes_session(s, ctx);
	} else {
		wl = cmd_find_window(ctx, args_get(args, 't'), NULL);
		if (wl == NULL)
			return (-1);
		cmd_list_panes_window(wl, ctx);
	}

	return (0);
}

void
cmd_list_panes_server(struct cmd_ctx *ctx)
{
	struct session	*s;

	RB_FOREACH(s, sessions, &sessions)
		cmd_list_panes_session(s, ctx);
}

void
cmd_list_panes_session(struct session *s, struct cmd_ctx *ctx)
{
	struct winlink	*wl;

	RB_FOREACH(wl, winlinks, &s->windows)
		cmd_list_panes_window(wl, ctx);
}

void
cmd_list_panes_window(struct winlink *wl, struct cmd_ctx *ctx)
{
	struct window_pane	*wp;
	struct grid		*gd;
	struct grid_line	*gl;
	u_int			 i, n;
	unsigned long long	 size;

	n = 0;
	TAILQ_FOREACH(wp, &wl->window->panes, entry) {
		gd = wp->base.grid;

		size = 0;
		for (i = 0; i < gd->hsize; i++) {
			gl = &gd->linedata[i];
			size += gl->cellsize * sizeof *gl->celldata;
			size += gl->utf8size * sizeof *gl->utf8data;
		}
		size += gd->hsize * sizeof *gd->linedata;

		ctx->print(ctx,
		    "%u: [%ux%u] [history %u/%u, %llu bytes] %%%u%s%s",
		    n, wp->sx, wp->sy, gd->hsize, gd->hlimit, size, wp->id,
		    wp == wp->window->active ? " (active)" : "",
		    wp->fd == -1 ? " (dead)" : "");
		n++;
	}
}
