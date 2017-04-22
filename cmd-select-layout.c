/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Switch window to selected layout.
 */

static enum cmd_retval	cmd_select_layout_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_select_layout_entry = {
	.name = "select-layout",
	.alias = "selectl",

	.args = { "nopt:", 0, 1 },
	.usage = "[-nop] " CMD_TARGET_WINDOW_USAGE " [layout-name]",

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_select_layout_exec
};

const struct cmd_entry cmd_next_layout_entry = {
	.name = "next-layout",
	.alias = "nextl",

	.args = { "t:", 0, 0 },
	.usage = CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_select_layout_exec
};

const struct cmd_entry cmd_previous_layout_entry = {
	.name = "previous-layout",
	.alias = "prevl",

	.args = { "t:", 0, 0 },
	.usage = CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_select_layout_exec
};

static enum cmd_retval
cmd_select_layout_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args	*args = self->args;
	struct winlink	*wl = item->target.wl;
	struct window	*w;
	const char	*layoutname;
	char		*oldlayout;
	int		 next, previous, layout;

	w = wl->window;
	server_unzoom_window(w);

	next = self->entry == &cmd_next_layout_entry;
	if (args_has(args, 'n'))
		next = 1;
	previous = self->entry == &cmd_previous_layout_entry;
	if (args_has(args, 'p'))
		previous = 1;

	oldlayout = w->old_layout;
	w->old_layout = layout_dump(w->layout_root);

	if (next || previous) {
		if (next)
			layout_set_next(w);
		else
			layout_set_previous(w);
		goto changed;
	}

	if (!args_has(args, 'o')) {
		if (args->argc == 0)
			layout = w->lastlayout;
		else
			layout = layout_set_lookup(args->argv[0]);
		if (layout != -1) {
			layout_set_select(w, layout);
			goto changed;
		}
	}

	if (args->argc != 0)
		layoutname = args->argv[0];
	else if (args_has(args, 'o'))
		layoutname = oldlayout;
	else
		layoutname = NULL;

	if (layoutname != NULL) {
		if (layout_parse(w, layoutname) == -1) {
			cmdq_error(item, "can't set layout: %s", layoutname);
			goto error;
		}
		goto changed;
	}

	free(oldlayout);
	return (CMD_RETURN_NORMAL);

changed:
	free(oldlayout);
	server_redraw_window(w);
	return (CMD_RETURN_NORMAL);

error:
	free(w->old_layout);
	w->old_layout = oldlayout;
	return (CMD_RETURN_ERROR);
}
