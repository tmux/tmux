/* $OpenBSD$ */

/*
 * Copyright (c) 2018 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Increase or decrease window size.
 */

static enum cmd_retval	cmd_resize_window_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_resize_window_entry = {
	.name = "resize-window",
	.alias = "resizew",

	.args = { "aADLRt:Ux:y:", 0, 1, NULL },
	.usage = "[-aADLRU] [-x width] [-y height] " CMD_TARGET_WINDOW_USAGE " "
		 "[adjustment]",

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_resize_window_exec
};

static enum cmd_retval
cmd_resize_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct session		*s = target->s;
	const char	       	*errstr;
	char			*cause;
	u_int			 adjust, sx, sy, xpixel = 0, ypixel = 0;

	if (args_count(args) == 0)
		adjust = 1;
	else {
		adjust = strtonum(args_string(args, 0), 1, INT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(item, "adjustment %s", errstr);
			return (CMD_RETURN_ERROR);
		}
	}

	sx = w->sx;
	sy = w->sy;

	if (args_has(args, 'x')) {
		sx = args_strtonum(args, 'x', WINDOW_MINIMUM, WINDOW_MAXIMUM,
		    &cause);
		if (cause != NULL) {
			cmdq_error(item, "width %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}
	if (args_has(args, 'y')) {
		sy = args_strtonum(args, 'y', WINDOW_MINIMUM, WINDOW_MAXIMUM,
		    &cause);
		if (cause != NULL) {
			cmdq_error(item, "height %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	if (args_has(args, 'L')) {
		if (sx >= adjust)
			sx -= adjust;
	} else if (args_has(args, 'R'))
		sx += adjust;
	else if (args_has(args, 'U')) {
		if (sy >= adjust)
			sy -= adjust;
	} else if (args_has(args, 'D'))
		sy += adjust;

	if (args_has(args, 'A')) {
		default_window_size(NULL, s, w, &sx, &sy, &xpixel, &ypixel,
		    WINDOW_SIZE_LARGEST);
	} else if (args_has(args, 'a')) {
		default_window_size(NULL, s, w, &sx, &sy, &xpixel, &ypixel,
		    WINDOW_SIZE_SMALLEST);
	}

	options_set_number(w->options, "window-size", WINDOW_SIZE_MANUAL);
	w->manual_sx = sx;
	w->manual_sy = sy;
	recalculate_size(w, 1);

	return (CMD_RETURN_NORMAL);
}
