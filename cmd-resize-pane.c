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

#include <stdlib.h>

#include "tmux.h"

/*
 * Increase or decrease pane size.
 */

void	cmd_resize_pane_init(struct cmd *, int);
int	cmd_resize_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_resize_pane_entry = {
	"resize-pane", "resizep",
	"[-DLRU] " CMD_TARGET_PANE_USAGE " [adjustment]",
	CMD_ARG01, "DLRU",
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
		cmd_set_flag(&data->chflags, 'U');
	if (key == (KEYC_DOWN | KEYC_CTRL))
		cmd_set_flag(&data->chflags, 'D');
	if (key == (KEYC_LEFT | KEYC_CTRL))
		cmd_set_flag(&data->chflags, 'L');
	if (key == (KEYC_RIGHT | KEYC_CTRL))
		cmd_set_flag(&data->chflags, 'R');

	if (key == (KEYC_UP | KEYC_ESCAPE)) {
		cmd_set_flag(&data->chflags, 'U');
		data->arg = xstrdup("5");
	}
	if (key == (KEYC_DOWN | KEYC_ESCAPE)) {
		cmd_set_flag(&data->chflags, 'D');
		data->arg = xstrdup("5");
	}
	if (key == (KEYC_LEFT | KEYC_ESCAPE)) {
		cmd_set_flag(&data->chflags, 'L');
		data->arg = xstrdup("5");
	}
	if (key == (KEYC_RIGHT | KEYC_ESCAPE)) {
		cmd_set_flag(&data->chflags, 'R');
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

	if (cmd_check_flag(data->chflags, 'L'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, -adjust);
	else if (cmd_check_flag(data->chflags, 'R'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, adjust);
	else if (cmd_check_flag(data->chflags, 'U'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, -adjust);
	else if (cmd_check_flag(data->chflags, 'D'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, adjust);
	server_redraw_window(wl->window);

	return (0);
}
