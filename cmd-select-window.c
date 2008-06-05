/* $Id: cmd-select-window.c,v 1.17 2008-06-05 16:35:32 nicm Exp $ */

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
 * Select window by index.
 */

void	cmd_select_window_init(struct cmd *, int);
void	cmd_select_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_select_window_entry = {
	"select-window", "selectw",
	CMD_WINDOWONLY_USAGE,
	0,
	cmd_windowonly_parse,
	cmd_select_window_exec,
	cmd_windowonly_send,
	cmd_windowonly_recv,
	cmd_windowonly_free,
	cmd_select_window_init,
	cmd_windowonly_print
};

void
cmd_select_window_init(struct cmd *self, int arg)
{
	struct cmd_windowonly_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;
	data->idx = arg - '0';
}

void
cmd_select_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct winlink	*wl;
	struct session	*s;

	if ((wl = cmd_windowonly_get(self, ctx, &s)) == NULL)
		return;

	if (session_select(s, wl->idx) == 0)
		server_redraw_session(s);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
