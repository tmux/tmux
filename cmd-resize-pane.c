/* $Id: cmd-resize-pane.c,v 1.12 2009-07-30 20:45:20 tcunha Exp $ */

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
 * Increase or decrease pane size.
 */

void	cmd_resize_pane_init(struct cmd *, int);
int	cmd_resize_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_resize_pane_entry = {
	"resize-pane", "resizep",
	"[-DU] " CMD_TARGET_PANE_USAGE " [adjustment]",
	CMD_ARG01,
	CMD_CHFLAG('D')|CMD_CHFLAG('L')|CMD_CHFLAG('R')|CMD_CHFLAG('U'),
	cmd_resize_pane_init,
	cmd_target_parse,
	cmd_resize_pane_exec,
	cmd_target_free,
	cmd_target_print
};

void
cmd_resize_pane_init(struct cmd *self, int key)
{
	struct cmd_target_data	*data;

	cmd_target_init(self, key);
	data = self->data;

	if (key == (KEYC_UP | KEYC_CTRL))
		data->chflags |= CMD_CHFLAG('U');
	if (key == (KEYC_DOWN | KEYC_CTRL))
		data->chflags |= CMD_CHFLAG('D');
	if (key == (KEYC_LEFT | KEYC_CTRL))
		data->chflags |= CMD_CHFLAG('L');
	if (key == (KEYC_RIGHT | KEYC_CTRL))
		data->chflags |= CMD_CHFLAG('R');
	
	if (key == (KEYC_UP | KEYC_ESCAPE)) {
		data->chflags |= CMD_CHFLAG('U');
		data->arg = xstrdup("5");
	}
	if (key == (KEYC_DOWN | KEYC_ESCAPE)) {
		data->chflags |= CMD_CHFLAG('D');
		data->arg = xstrdup("5");
	}
	if (key == (KEYC_LEFT | KEYC_ESCAPE)) {
		data->chflags |= CMD_CHFLAG('L');
		data->arg = xstrdup("5");
	}
	if (key == (KEYC_RIGHT | KEYC_ESCAPE)) {
		data->chflags |= CMD_CHFLAG('R');
		data->arg = xstrdup("5");
	}
}

int
cmd_resize_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct winlink		*wl;
	const char	       	*errstr;
	struct window_pane	*wp;
	u_int			 adjust;

	if ((wl = cmd_find_pane(ctx, data->target, NULL, &wp)) == NULL)
		return (-1);

	if (data->arg == NULL)
		adjust = 1;
	else {
		adjust = strtonum(data->arg, 1, INT_MAX, &errstr);
		if (errstr != NULL) {
			ctx->error(ctx, "adjustment %s: %s", errstr, data->arg);
			return (-1);
		}
	}

	if (data->chflags & (CMD_CHFLAG('L')|CMD_CHFLAG('R'))) {
		if (data->chflags & CMD_CHFLAG('L'))
			adjust = -adjust;
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, adjust);
	} else {
		if (data->chflags & CMD_CHFLAG('U'))
			adjust = -adjust;
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, adjust);
	}
	server_redraw_window(wl->window);

	return (0);
}
