/* $Id: cmd-scroll-mode.c,v 1.4 2007-11-27 19:23:33 nicm Exp $ */

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
 * Enter scroll mode. Only valid when bound to a key.
 */

void	cmd_scroll_mode_exec(void *, struct cmd_ctx *);

const struct cmd_entry cmd_scroll_mode_entry = {
	"scroll-mode", NULL, "",
	CMD_NOCLIENT,
	NULL,
	cmd_scroll_mode_exec, 
	NULL,
	NULL,
	NULL
};

void
cmd_scroll_mode_exec(unused void *ptr, struct cmd_ctx *ctx)
{
	struct window	*w = ctx->session->curw->window;

	if (w->mode == NULL && ctx->flags & CMD_KEY) {
		w->mode = &window_scroll_mode;
		w->mode->init(w);
		server_redraw_window(w);
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}
