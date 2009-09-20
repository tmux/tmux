/* $OpenBSD$ */

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

#include <stdlib.h>

#include "tmux.h"

/*
 * Link a window into another session.
 */

int	cmd_link_window_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_link_window_entry = {
	"link-window", "linkw",
	"[-dk] " CMD_SRCDST_WINDOW_USAGE,
	0, CMD_CHFLAG('d')|CMD_CHFLAG('k'),
	cmd_srcdst_init,
	cmd_srcdst_parse,
	cmd_link_window_exec,
	cmd_srcdst_free,
	cmd_srcdst_print
};

int
cmd_link_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_srcdst_data	*data = self->data;
	struct session		*dst;
	struct winlink		*wl;
	char			*cause;
	int			 idx, kflag, dflag;

	if ((wl = cmd_find_window(ctx, data->src, NULL)) == NULL)
		return (-1);
	if ((idx = cmd_find_index(ctx, data->dst, &dst)) == -2)
		return (-1);

	kflag = data->chflags & CMD_CHFLAG('k');
	dflag = data->chflags & CMD_CHFLAG('d');
	if (server_link_window(wl, dst, idx, kflag, !dflag, &cause) != 0) {
		ctx->error(ctx, "can't create session: %s", cause);
		xfree(cause);
		return (-1);
	}
	recalculate_sizes();

	return (0);
}
