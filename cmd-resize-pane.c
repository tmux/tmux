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
#include <string.h>

#include "tmux.h"

/*
 * Increase or decrease pane size.
 */

static enum cmd_retval	cmd_resize_pane_exec(struct cmd *, struct cmdq_item *);

static void	cmd_resize_pane_mouse_update(struct client *,
		    struct mouse_event *);

const struct cmd_entry cmd_resize_pane_entry = {
	.name = "resize-pane",
	.alias = "resizep",

	.args = { "DLMRTt:Ux:y:Z", 0, 1, NULL },
	.usage = "[-DLMRTUZ] [-x width] [-y height] " CMD_TARGET_PANE_USAGE " "
		 "[adjustment]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_resize_pane_exec
};

static enum cmd_retval
cmd_resize_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct key_event	*event = cmdq_get_event(item);
	struct window_pane	*wp = target->wp;
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct client		*c = cmdq_get_client(item);
	struct session		*s = target->s;
	const char	       	*errstr;
	char			*cause;
	u_int			 adjust;
	int			 x, y, status;
	struct grid		*gd = wp->base.grid;

	if (args_has(args, 'T')) {
		if (!TAILQ_EMPTY(&wp->modes))
			return (CMD_RETURN_NORMAL);
		adjust = screen_size_y(&wp->base) - 1 - wp->base.cy;
		if (adjust > gd->hsize)
			adjust = gd->hsize;
		grid_remove_history(gd, adjust);
		wp->base.cy += adjust;
		wp->flags |= PANE_REDRAW;
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'M')) {
		if (!event->m.valid || cmd_mouse_window(&event->m, &s) == NULL)
			return (CMD_RETURN_NORMAL);
		if (c == NULL || c->session != s)
			return (CMD_RETURN_NORMAL);
		c->tty.mouse_drag_update = cmd_resize_pane_mouse_update;
		cmd_resize_pane_mouse_update(c, &event->m);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'Z')) {
		if (w->flags & WINDOW_ZOOMED)
			window_unzoom(w, 1);
		else
			window_zoom(wp);
		server_redraw_window(w);
		return (CMD_RETURN_NORMAL);
	}
	server_unzoom_window(w);

	if (args_count(args) == 0)
		adjust = 1;
	else {
		adjust = strtonum(args_string(args, 0), 1, INT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(item, "adjustment %s", errstr);
			return (CMD_RETURN_ERROR);
		}
	}

	if (args_has(args, 'x')) {
		x = args_percentage(args, 'x', 0, INT_MAX, w->sx, &cause);
		if (cause != NULL) {
			cmdq_error(item, "width %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		layout_resize_pane_to(wp, LAYOUT_LEFTRIGHT, x);
	}
	if (args_has(args, 'y')) {
		y = args_percentage(args, 'y', 0, INT_MAX, w->sy, &cause);
		if (cause != NULL) {
			cmdq_error(item, "height %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		status = options_get_number(w->options, "pane-border-status");
		switch (status) {
		case PANE_STATUS_TOP:
			if (y != INT_MAX && wp->yoff == 1)
				y++;
			break;
		case PANE_STATUS_BOTTOM:
			if (y != INT_MAX && wp->yoff + wp->sy == w->sy - 1)
				y++;
			break;
		}
		layout_resize_pane_to(wp, LAYOUT_TOPBOTTOM, y);
	}

	if (args_has(args, 'L'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, -adjust, 1);
	else if (args_has(args, 'R'))
		layout_resize_pane(wp, LAYOUT_LEFTRIGHT, adjust, 1);
	else if (args_has(args, 'U'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, -adjust, 1);
	else if (args_has(args, 'D'))
		layout_resize_pane(wp, LAYOUT_TOPBOTTOM, adjust, 1);
	server_redraw_window(wl->window);

	return (CMD_RETURN_NORMAL);
}

static void
cmd_resize_pane_mouse_update(struct client *c, struct mouse_event *m)
{
	struct winlink		*wl;
	struct window		*w;
	u_int			 y, ly, x, lx;
	static const int         offsets[][2] = {
	    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 },
	};
	struct layout_cell	*cells[nitems(offsets)], *lc;
	u_int			 ncells = 0, i, j, resizes = 0;
	enum layout_type	 type;

	wl = cmd_mouse_window(m, NULL);
	if (wl == NULL) {
		c->tty.mouse_drag_update = NULL;
		return;
	}
	w = wl->window;

	y = m->y + m->oy; x = m->x + m->ox;
	if (m->statusat == 0 && y >= m->statuslines)
		y -= m->statuslines;
	else if (m->statusat > 0 && y >= (u_int)m->statusat)
		y = m->statusat - 1;
	ly = m->ly + m->oy; lx = m->lx + m->ox;
	if (m->statusat == 0 && ly >= m->statuslines)
		ly -= m->statuslines;
	else if (m->statusat > 0 && ly >= (u_int)m->statusat)
		ly = m->statusat - 1;

	for (i = 0; i < nitems(cells); i++) {
		lc = layout_search_by_border(w->layout_root, lx + offsets[i][0],
		    ly + offsets[i][1]);
		if (lc == NULL)
			continue;

		for (j = 0; j < ncells; j++) {
			if (cells[j] == lc) {
				lc = NULL;
				break;
			}
		}
		if (lc == NULL)
			continue;

		cells[ncells] = lc;
		ncells++;
	}
	if (ncells == 0)
		return;

	for (i = 0; i < ncells; i++) {
		type = cells[i]->parent->type;
		if (y != ly && type == LAYOUT_TOPBOTTOM) {
			layout_resize_layout(w, cells[i], type, y - ly, 0);
			resizes++;
		} else if (x != lx && type == LAYOUT_LEFTRIGHT) {
			layout_resize_layout(w, cells[i], type, x - lx, 0);
			resizes++;
		}
	}
	if (resizes != 0)
		server_redraw_window(w);
}
