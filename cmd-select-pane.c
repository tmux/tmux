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

#include "tmux.h"

/*
 * Select pane.
 */

enum cmd_retval	 cmd_select_pane_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_select_pane_entry = {
	"select-pane", "selectp",
	"DdegLlMmP:Rt:U", 0, 0,
	"[-DdegLlMmRU] [-P style] " CMD_TARGET_PANE_USAGE,
	0,
	cmd_select_pane_exec
};

const struct cmd_entry cmd_last_pane_entry = {
	"last-pane", "lastp",
	"det:", 0, 0,
	"[-de] " CMD_TARGET_WINDOW_USAGE,
	0,
	cmd_select_pane_exec
};

enum cmd_retval
cmd_select_pane_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	struct window		*w;
	struct session		*s;
	struct window_pane	*wp, *lastwp, *markedwp;
	const char		*style;

	if (self->entry == &cmd_last_pane_entry || args_has(args, 'l')) {
		wl = cmd_find_window(cmdq, args_get(args, 't'), NULL);
		if (wl == NULL)
			return (CMD_RETURN_ERROR);
		w = wl->window;

		if (w->last == NULL) {
			cmdq_error(cmdq, "no last pane");
			return (CMD_RETURN_ERROR);
		}

		if (args_has(self->args, 'e'))
			w->last->flags &= ~PANE_INPUTOFF;
		else if (args_has(self->args, 'd'))
			w->last->flags |= PANE_INPUTOFF;
		else {
			server_unzoom_window(w);
			window_redraw_active_switch(w, w->last);
			if (window_set_active_pane(w, w->last)) {
				server_status_window(w);
				server_redraw_window_borders(w);
			}
		}

		return (CMD_RETURN_NORMAL);
	}

	if ((wl = cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp)) == NULL)
		return (CMD_RETURN_ERROR);
	w = wl->window;

	if (args_has(args, 'm') || args_has(args, 'M')) {
		if (args_has(args, 'm') && !window_pane_visible(wp))
			return (CMD_RETURN_NORMAL);
		lastwp = marked_window_pane;

		if (args_has(args, 'M') || server_is_marked(s, wl, wp))
			server_clear_marked();
		else
			server_set_marked(s, wl, wp);
		markedwp = marked_window_pane;

		if (lastwp != NULL) {
			server_redraw_window_borders(lastwp->window);
			server_status_window(lastwp->window);
		}
		if (markedwp != NULL) {
			server_redraw_window_borders(markedwp->window);
			server_status_window(markedwp->window);
		}
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(self->args, 'P') || args_has(self->args, 'g')) {
		if (args_has(args, 'P')) {
			style = args_get(args, 'P');
			if (style_parse(&grid_default_cell, &wp->colgc,
			    style) == -1) {
				cmdq_error(cmdq, "bad style: %s", style);
				return (CMD_RETURN_ERROR);
			}
			wp->flags |= PANE_REDRAW;
		}
		if (args_has(self->args, 'g'))
			cmdq_print(cmdq, "%s", style_tostring(&wp->colgc));
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(self->args, 'L'))
		wp = window_pane_find_left(wp);
	else if (args_has(self->args, 'R'))
		wp = window_pane_find_right(wp);
	else if (args_has(self->args, 'U'))
		wp = window_pane_find_up(wp);
	else if (args_has(self->args, 'D'))
		wp = window_pane_find_down(wp);
	if (wp == NULL)
		return (CMD_RETURN_NORMAL);

	if (args_has(self->args, 'e')) {
		wp->flags &= ~PANE_INPUTOFF;
		return (CMD_RETURN_NORMAL);
	}
	if (args_has(self->args, 'd')) {
		wp->flags |= PANE_INPUTOFF;
		return (CMD_RETURN_NORMAL);
	}

	if (wp == w->active)
		return (CMD_RETURN_NORMAL);
	server_unzoom_window(wp->window);
	if (!window_pane_visible(wp)) {
		cmdq_error(cmdq, "pane not visible");
		return (CMD_RETURN_ERROR);
	}
	window_redraw_active_switch(w, wp);
	if (window_set_active_pane(w, wp)) {
		server_status_window(w);
		server_redraw_window_borders(w);
	}

	return (CMD_RETURN_NORMAL);
}
