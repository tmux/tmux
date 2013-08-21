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

void		 cmd_resize_pane_key_binding(struct cmd *, int);
enum cmd_retval	 cmd_resize_pane_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_resize_pane_entry = {
	"resize-pane", "resizep",
	"DLRt:Ux:y:Z", 0, 1,
	"[-DLRUZ] [-x width] [-y height] " CMD_TARGET_PANE_USAGE " [adjustment]",
	0,
	cmd_resize_pane_key_binding,
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
	case 'z':
		self->args = args_create(0);
		args_set(self->args, 'Z', NULL);
		break;
	default:
		self->args = args_create(0);
		break;
	}
}

enum cmd_retval
cmd_resize_pane_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	struct window		*w;
	const char	       	*errstr;
	char			*cause;
	struct window_pane	*wp;
	u_int			 adjust;
	int			 x, y;

	if ((wl = cmd_find_pane(cmdq, args_get(args, 't'), NULL, &wp)) == NULL)
		return (CMD_RETURN_ERROR);
	w = wl->window;

	if (args_has(args, 'Z')) {
		if (w->flags & WINDOW_ZOOMED)
			window_unzoom(w);
		else
			window_zoom(wp);
		server_redraw_window(w);
		server_status_window(w);
		return (CMD_RETURN_NORMAL);
	}
	server_unzoom_window(w);

	if (args->argc == 0)
		adjust = 1;
	else {
		adjust = strtonum(args->argv[0], 1, INT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(cmdq, "adjustment %s", errstr);
			return (CMD_RETURN_ERROR);
		}
	}

	if (args_has(self->args, 'x')) {
		x = args_strtonum(self->args, 'x', PANE_MINIMUM, INT_MAX,
		    &cause);
		if (cause != NULL) {
			cmdq_error(cmdq, "width %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		layout_resize_pane_to(wp, LAYOUT_LEFTRIGHT, x);
	}
	if (args_has(self->args, 'y')) {
		y = args_strtonum(self->args, 'y', PANE_MINIMUM, INT_MAX,
		    &cause);
		if (cause != NULL) {
			cmdq_error(cmdq, "height %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		layout_resize_pane_to(wp, LAYOUT_TOPBOTTOM, y);
	}

	if (args_has(self->args, 'L'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, -adjust);
	else if (args_has(self->args, 'R'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, adjust);
	else if (args_has(self->args, 'U'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, -adjust);
	else if (args_has(self->args, 'D'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, adjust);
	server_redraw_window(wl->window);

	return (CMD_RETURN_NORMAL);
}
