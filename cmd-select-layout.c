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

#include "tmux.h"

/*
 * Switch window to selected layout.
 */

void		 cmd_select_layout_key_binding(struct cmd *, int);
enum cmd_retval	 cmd_select_layout_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_select_layout_entry = {
	"select-layout", "selectl",
	"npt:", 0, 1,
	"[-np] " CMD_TARGET_WINDOW_USAGE " [layout-name]",
	0,
	cmd_select_layout_key_binding,
	NULL,
	cmd_select_layout_exec
};

const struct cmd_entry cmd_next_layout_entry = {
	"next-layout", "nextl",
	"t:", 0, 0,
	CMD_TARGET_WINDOW_USAGE,
	0,
	NULL,
	NULL,
	cmd_select_layout_exec
};

const struct cmd_entry cmd_previous_layout_entry = {
	"previous-layout", "prevl",
	"t:", 0, 0,
	CMD_TARGET_WINDOW_USAGE,
	0,
	NULL,
	NULL,
	cmd_select_layout_exec
};

void
cmd_select_layout_key_binding(struct cmd *self, int key)
{
	switch (key) {
	case '1' | KEYC_ESCAPE:
		self->args = args_create(1, "even-horizontal");
		break;
	case '2' | KEYC_ESCAPE:
		self->args = args_create(1, "even-vertical");
		break;
	case '3' | KEYC_ESCAPE:
		self->args = args_create(1, "main-horizontal");
		break;
	case '4' | KEYC_ESCAPE:
		self->args = args_create(1, "main-vertical");
		break;
	case '5' | KEYC_ESCAPE:
		self->args = args_create(1, "tiled");
		break;
	default:
		self->args = args_create(0);
		break;
	}
}

enum cmd_retval
cmd_select_layout_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct winlink	*wl;
	struct window	*w;
	const char	*layoutname;
	int		 next, previous, layout;

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (CMD_RETURN_ERROR);
	w = wl->window;

	next = self->entry == &cmd_next_layout_entry;
	if (args_has(self->args, 'n'))
		next = 1;
	previous = self->entry == &cmd_previous_layout_entry;
	if (args_has(self->args, 'p'))
		previous = 1;

	if (next || previous) {
		if (next)
			layout = layout_set_next(wl->window);
		else
			layout = layout_set_previous(wl->window);
		server_redraw_window(wl->window);
		ctx->info(ctx, "arranging in: %s", layout_set_name(layout));
		return (CMD_RETURN_NORMAL);
	}

	if (args->argc == 0)
		layout = wl->window->lastlayout;
	else
		layout = layout_set_lookup(args->argv[0]);
	if (layout != -1) {
		layout = layout_set_select(wl->window, layout);
		server_redraw_window(wl->window);
		ctx->info(ctx, "arranging in: %s", layout_set_name(layout));
		return (CMD_RETURN_NORMAL);
	}

	if (args->argc != 0) {
		layoutname = args->argv[0];
		if (layout_parse(wl->window, layoutname) == -1) {
			ctx->error(ctx, "can't set layout: %s", layoutname);
			return (CMD_RETURN_ERROR);
		}
		server_redraw_window(wl->window);
		ctx->info(ctx, "arranging in: %s", layoutname);
	}
	return (CMD_RETURN_NORMAL);
}
