/* $Id$ */

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

void	cmd_resize_pane_key_binding(struct cmd *, int);
int	cmd_resize_pane_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_resize_pane_entry = {
	"resize-pane", "resizep",
	"DLRt:U", 0, 1,
	"[-DLRU] " CMD_TARGET_PANE_USAGE " [adjustment]",
	0,
	cmd_resize_pane_key_binding,
	NULL,
	cmd_resize_pane_exec
};

void
cmd_resize_pane_key_binding(struct cmd *self, int key)
{
	switch (key) {
	case KEYC_UP | KEYC_CTRL:
		self->args = args_create(0);
		args_set(self->args, 'U', NULL);
		break;
	case KEYC_DOWN | KEYC_CTRL:
		self->args = args_create(0);
		args_set(self->args, 'D', NULL);
		break;
	case KEYC_LEFT | KEYC_CTRL:
		self->args = args_create(0);
		args_set(self->args, 'L', NULL);
		break;
	case KEYC_RIGHT | KEYC_CTRL:
		self->args = args_create(0);
		args_set(self->args, 'R', NULL);
		break;
	case KEYC_UP | KEYC_ESCAPE:
		self->args = args_create(1, "5");
		args_set(self->args, 'U', NULL);
		break;
	case KEYC_DOWN | KEYC_ESCAPE:
		self->args = args_create(1, "5");
		args_set(self->args, 'D', NULL);
		break;
	case KEYC_LEFT | KEYC_ESCAPE:
		self->args = args_create(1, "5");
		args_set(self->args, 'L', NULL);
		break;
	case KEYC_RIGHT | KEYC_ESCAPE:
		self->args = args_create(1, "5");
		args_set(self->args, 'R', NULL);
		break;
	default:
		self->args = args_create(0);
		break;
	}
}

int
cmd_resize_pane_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	const char	       	*errstr;
	struct window_pane	*wp;
	u_int			 adjust;

	if ((wl = cmd_find_pane(ctx, args_get(args, 't'), NULL, &wp)) == NULL)
		return (-1);

	if (args->argc == 0)
		adjust = 1;
	else {
		adjust = strtonum(args->argv[0], 1, INT_MAX, &errstr);
		if (errstr != NULL) {
			ctx->error(ctx, "adjustment %s", errstr);
			return (-1);
		}
	}

	layout_list_add(wp->window);
	if (args_has(self->args, 'L'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, -adjust);
	else if (args_has(self->args, 'R'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, adjust);
	else if (args_has(self->args, 'U'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, -adjust);
	else if (args_has(self->args, 'D'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, adjust);
	server_redraw_window(wl->window);

	return (0);
}
