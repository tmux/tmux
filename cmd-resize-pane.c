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

enum cmd_retval	 cmd_resize_pane_exec(struct cmd *, struct cmd_q *);

void	cmd_resize_pane_mouse_update(struct client *, struct mouse_event *);

const struct cmd_entry cmd_resize_pane_entry = {
	"resize-pane", "resizep",
	"DLMRt:Ux:y:Z", 0, 1,
	"[-DLMRUZ] [-x width] [-y height] " CMD_TARGET_PANE_USAGE
	" [adjustment]",
	0,
	cmd_resize_pane_exec
};

enum cmd_retval
cmd_resize_pane_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c = cmdq->client;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w;
	const char	       	*errstr;
	char			*cause;
	struct window_pane	*wp;
	u_int			 adjust;
	int			 x, y;

	if (args_has(args, 'M')) {
		if (cmd_mouse_window(&cmdq->item->mouse, &s) == NULL)
			return (CMD_RETURN_NORMAL);
		if (c == NULL || c->session != s)
			return (CMD_RETURN_NORMAL);
		c->tty.mouse_drag_update = cmd_resize_pane_mouse_update;
		cmd_resize_pane_mouse_update(c, &cmdq->item->mouse);
		return (CMD_RETURN_NORMAL);
	}

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

void
cmd_resize_pane_mouse_update(struct client *c, struct mouse_event *m)
{
	struct winlink		*wl;
	struct window_pane	*wp;
	int			 found;
	u_int			 y, ly;

	wl = cmd_mouse_window(m, NULL);
	if (wl == NULL) {
		c->tty.mouse_drag_update = NULL;
		return;
	}

	y = m->y;
	if (m->statusat == 0 && y > 0)
		y--;
	else if (m->statusat > 0 && y >= (u_int)m->statusat)
		y = m->statusat - 1;
	ly = m->ly;
	if (m->statusat == 0 && ly > 0)
		ly--;
	else if (m->statusat > 0 && ly >= (u_int)m->statusat)
		ly = m->statusat - 1;

	found = 0;
	TAILQ_FOREACH(wp, &wl->window->panes, entry) {
		if (!window_pane_visible(wp))
			continue;

		if (wp->xoff + wp->sx == m->lx &&
		    wp->yoff <= 1 + ly && wp->yoff + wp->sy >= ly) {
			layout_resize_pane(wp, LAYOUT_LEFTRIGHT, m->x - m->lx);
			found = 1;
		}
		if (wp->yoff + wp->sy == ly &&
		    wp->xoff <= 1 + m->lx && wp->xoff + wp->sx >= m->lx) {
			layout_resize_pane(wp, LAYOUT_TOPBOTTOM, y - ly);
			found = 1;
		}
	}
	if (found)
		server_redraw_window(wl->window);
	else
		c->tty.mouse_drag_update = NULL;
}
