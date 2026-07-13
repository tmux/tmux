/* $OpenBSD: cmd-join-pane.c,v 1.70 2026/07/13 10:03:27 nicm Exp $ */

/*
 * Copyright (c) 2011 George Nachman <tmux@georgester.com>
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

#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Join or move a pane into another (like split/swap/kill).
 */

static enum cmd_retval	cmd_join_pane_exec(struct cmd *, struct cmdq_item *);
static enum cmd_retval	cmd_join_pane_mouse_update(struct cmdq_item *);
static void		cmd_join_pane_mouse_move(struct client *,
			    struct mouse_event *);

const struct cmd_entry cmd_join_pane_entry = {
	.name = "join-pane",
	.alias = "joinp",

	.args = { "bdfhvp:l:s:t:", 0, 0, NULL },
	.usage = "[-bdfhv] [-l size] " CMD_SRCDST_PANE_USAGE,

	.source = { 's', CMD_FIND_PANE, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_join_pane_exec
};

const struct cmd_entry cmd_move_pane_entry = {
	.name = "move-pane",
	.alias = "movep",

	.args = { "bdD::fhMvl:L::P:R::s:t:U::X:Y:z:", 0, 0, NULL },
	.usage = "[-bdfhMv] [-D lines] [-l size] [-L columns] [-P position] "
	         "[-R columns] " CMD_SRCDST_PANE_USAGE " [-U lines] "
	         "[-X x-position] [-Y y-position] [-z z-index]",

	.source = { 's', CMD_FIND_PANE, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_join_pane_exec
};

static enum cmd_retval
cmd_join_pane_place(struct cmdq_item *item, struct winlink *wl,
    struct window_pane *wp, const char *position)
{
	struct window		*w = wl->window;
	struct layout_cell	*lc = wp->layout_cell;
	struct window_pane	*owp;
	int			 wx = w->sx, wy = w->sy;
	int			 px = lc->g.sx, py = lc->g.sy;
	int			 xoff = lc->g.xoff, yoff = lc->g.yoff;
	int			 border = 1;

	if (window_pane_get_pane_lines(wp) == PANE_LINES_NONE)
		border = 0;

	if (strcmp(position, "top-left") == 0) {
		xoff = border;
		yoff = border;
	} else if (strcmp(position, "top-centre") == 0 ||
	    strcmp(position, "top-center") == 0) {
		xoff = (wx - px) / 2;
		yoff = border;
	} else if (strcmp(position, "top-right") == 0) {
		xoff = wx - px - border;
		yoff = border;
	} else if (strcmp(position, "centre-left") == 0 ||
	    strcmp(position, "center-left") == 0) {
		xoff = border;
		yoff = (wy - py) / 2;
	} else if (strcmp(position, "centre") == 0 ||
	    strcmp(position, "center") == 0) {
		xoff = (wx - px) / 2;
		yoff = (wy - py) / 2;
	} else if (strcmp(position, "centre-right") == 0 ||
	    strcmp(position, "center-right") == 0) {
		xoff = wx - px - border;
		yoff = (wy - py) / 2;
	} else if (strcmp(position, "bottom-left") == 0) {
		xoff = border;
		yoff = wy - py - border;
	} else if (strcmp(position, "bottom-centre") == 0 ||
	    strcmp(position, "bottom-center") == 0) {
		xoff = (wx - px) / 2;
		yoff = wy - py - border;
	} else if (strcmp(position, "bottom-right") == 0) {
		xoff = wx - px - border;
		yoff = wy - py - border;
	} else if (strcmp(position, "top-left-centre") == 0 ||
	    strcmp(position, "top-left-center") == 0) {
		xoff = wx / 4 - px / 2;
		yoff = wy / 4 - py / 2;
	} else if (strcmp(position, "top-right-centre") == 0 ||
	    strcmp(position, "top-right-center") == 0) {
		xoff = (3 * wx) / 4 - px / 2;
		yoff = wy / 4 - py / 2;
	} else if (strcmp(position, "bottom-left-centre") == 0 ||
	    strcmp(position, "bottom-left-center") == 0) {
		xoff = wx / 4 - px / 2;
		yoff = (3 * wy) / 4 - py / 2;
	} else if (strcmp(position, "bottom-right-centre") == 0 ||
	    strcmp(position, "bottom-right-center") == 0) {
		xoff = (3 * wx) / 4 - px / 2;
		yoff = (3 * wy) / 4 - py / 2;
	} else if (strcmp(position, "front") == 0) {
		TAILQ_REMOVE(&w->z_index, wp, zentry);
		TAILQ_INSERT_HEAD(&w->z_index, wp, zentry);
	} else if (strcmp(position, "back") == 0) {
		TAILQ_REMOVE(&w->z_index, wp, zentry);
		TAILQ_FOREACH(owp, &w->z_index, zentry) {
			if (!window_pane_is_floating(owp))
				break;
		}
		if (owp != NULL)
			TAILQ_INSERT_BEFORE(owp, wp, zentry);
		else
			TAILQ_INSERT_TAIL(&w->z_index, wp, zentry);
	} else if (strcmp(position, "forward") == 0) {
		owp = TAILQ_PREV(wp, window_panes_zindex, zentry);
		if (owp != NULL) {
			TAILQ_REMOVE(&w->z_index, wp, zentry);
			TAILQ_INSERT_BEFORE(owp, wp, zentry);
		}
	} else if (strcmp(position, "backward") == 0) {
		owp = TAILQ_NEXT(wp, zentry);
		if (owp != NULL && window_pane_is_floating(owp)) {
			TAILQ_REMOVE(&w->z_index, wp, zentry);
			TAILQ_INSERT_AFTER(&w->z_index, owp, wp, zentry);
		}
	} else if (strcmp(position, "forward-loop") == 0) {
		owp = TAILQ_PREV(wp, window_panes_zindex, zentry);
		TAILQ_REMOVE(&w->z_index, wp, zentry);
		if (owp != NULL)
			TAILQ_INSERT_BEFORE(owp, wp, zentry);
		else {
			TAILQ_FOREACH(owp, &w->z_index, zentry) {
				if (!window_pane_is_floating(owp))
					break;
			}
			if (owp != NULL)
				TAILQ_INSERT_BEFORE(owp, wp, zentry);
			else
				TAILQ_INSERT_TAIL(&w->z_index, wp, zentry);
		}
	} else if (strcmp(position, "backward-loop") == 0) {
		owp = TAILQ_NEXT(wp, zentry);
		if (owp != NULL && window_pane_is_floating(owp)) {
			TAILQ_REMOVE(&w->z_index, wp, zentry);
			TAILQ_INSERT_AFTER(&w->z_index, owp, wp, zentry);
		} else {
			TAILQ_REMOVE(&w->z_index, wp, zentry);
			TAILQ_INSERT_HEAD(&w->z_index, wp, zentry);
		}
	} else {
		cmdq_error(item, "unknown position: %s", position);
		return (CMD_RETURN_ERROR);
	}

	if (xoff != lc->g.xoff || yoff != lc->g.yoff) {
		lc->g.xoff = xoff;
		lc->g.yoff = yoff;
		layout_fix_panes(w, NULL);
	}
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_join_pane_move(struct cmdq_item *item, struct args *args,
    struct winlink *wl, struct window_pane *wp)
{
	struct window		*w = wl->window;
	struct layout_cell	*lc = wp->layout_cell;
	const char		*errstr, *argval;
	const char		 flags[] = { 'U', 'D', 'L', 'R' };
	char			*cause = NULL, flag;
	int			 xoff = lc->g.xoff, yoff = lc->g.yoff, adjust;
	u_int			 i;
	enum pane_lines		 lines = window_pane_get_pane_lines(wp);

	if (args_has(args, 'X')) {
		xoff = args_percentage_and_expand(args, 'X', -(int)w->sx,
		    w->sx, w->sx, item, &cause);
		if (cause != NULL) {
			cmdq_error(item, "position %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (lines != PANE_LINES_NONE)
			xoff += 1;
	}
	if (args_has(args, 'Y')) {
		yoff = args_percentage_and_expand(args, 'Y', -(int)w->sy,
		    w->sy, w->sy, item, &cause);
		if (cause != NULL) {
			cmdq_error(item, "position %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (lines != PANE_LINES_NONE)
			yoff += 1;
	}

	for (i = 0; i < nitems(flags); i++) {
		flag = flags[i];
		if (!args_has(args, flag))
			continue;

		argval = args_get(args, flag);
		if (argval == NULL)
			argval = "1";
		adjust = strtonum(argval, INT_MIN, INT_MAX, &errstr);
		if (errstr != NULL) {
			cmdq_error(item, "offset %s", errstr);
			return (CMD_RETURN_ERROR);
		}

		if (flag == 'U')
			yoff -= adjust;
		else if (flag == 'D')
			yoff += adjust;
		else if (flag == 'L')
			xoff -= adjust;
		else
			xoff += adjust;
	}

	if (xoff != lc->g.xoff || yoff != lc->g.yoff) {
		lc->g.xoff = xoff;
		lc->g.yoff = yoff;
		layout_fix_panes(w, NULL);
		events_fire_window("window-layout-changed", w);
		server_redraw_window(w);
	}

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_join_pane_mouse_update(struct cmdq_item *item)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct key_event	*event = cmdq_get_event(item);
	struct client		*c = cmdq_get_client(item);
	struct session		*s = target->s;
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;

	if (!event->m.valid)
		return (CMD_RETURN_NORMAL);
	wp = cmd_mouse_pane(&event->m, &s, &wl);
	if (wp == NULL || c == NULL || c->session != s)
		return (CMD_RETURN_NORMAL);
	if (!window_pane_is_floating(wp))
		return (CMD_RETURN_NORMAL);

	w = wl->window;
	window_redraw_active_switch(w, wp);
	window_set_active_pane(w, wp, 1);

	c->tty.mouse_drag_update = cmd_join_pane_mouse_move;
	cmd_join_pane_mouse_move(c, &event->m);
	return (CMD_RETURN_NORMAL);
}

static void
cmd_join_pane_mouse_move(struct client *c, struct mouse_event *m)
{
	struct winlink		*wl;
	struct window		*w;
	struct window_pane	*wp;
	struct layout_cell	*lc;
	int			 y, ly, x, lx;

	wp = cmd_mouse_pane(m, NULL, &wl);
	if (wp == NULL) {
		c->tty.mouse_drag_update = NULL;
		return;
	}
	w = wl->window;
	lc = wp->layout_cell;

	y = m->y + m->oy; x = m->x + m->ox;
	if (m->statusat == 0 && y >= (int)m->statuslines)
		y -= m->statuslines;
	else if (m->statusat > 0 && y >= m->statusat)
		y = m->statusat - 1;
	ly = m->ly + m->oy; lx = m->lx + m->ox;
	if (m->statusat == 0 && ly >= (int)m->statuslines)
		ly -= m->statuslines;
	else if (m->statusat > 0 && ly >= m->statusat)
		ly = m->statusat - 1;

	if (x != lx || y != ly) {
		lc->g.xoff += x - lx;
		lc->g.yoff += y - ly;
		layout_fix_panes(w, NULL);
		server_redraw_window(w);
		server_redraw_window_borders(w);
	}
}

static enum cmd_retval
cmd_join_pane_zindex(struct cmdq_item *item, struct winlink *wl,
    struct window_pane *wp, const char *s)
{
	struct window		*w = wl->window;
	struct window_pane	*owp;
	const char		*errstr;
	u_int			 n, z;

	z = strtonum(s, 0, UINT_MAX, &errstr);
	if (errstr != NULL) {
		cmdq_error(item, "z-index %s", errstr);
		return (CMD_RETURN_ERROR);
	}
	TAILQ_REMOVE(&w->z_index, wp, zentry);

	n = 0;
	TAILQ_FOREACH(owp, &w->z_index, zentry) {
		if (!window_pane_is_floating(owp))
			break;
		if (n >= z)
			break;
		n++;
	}

	if (owp != NULL)
		TAILQ_INSERT_BEFORE(owp, wp, zentry);
	else
		TAILQ_INSERT_TAIL(&w->z_index, wp, zentry);

	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_join_pane_tile(struct cmdq_item *item, struct args *args, struct window *w,
    struct window_pane *wp)
{
	struct layout_cell	*lc = wp->layout_cell;

	if (!window_pane_is_floating(wp)) {
		cmdq_error(item, "pane is not floating");
		return (CMD_RETURN_ERROR);
	}
	if (w->flags & WINDOW_ZOOMED) {
		cmdq_error(item, "can't tile a pane while window is zoomed");
		return (CMD_RETURN_ERROR);
	}

	lc->fg.sx = lc->g.sx;
	lc->fg.sy = lc->g.sy;
	lc->fg.xoff = lc->g.xoff;
	lc->fg.yoff = lc->g.yoff;

	if (layout_insert_tile(w, lc) != 0) {
		cmdq_error(item, "no space for a new pane");
		return (CMD_RETURN_ERROR);
	}
	lc->flags &= ~LAYOUT_CELL_FLOATING;

	TAILQ_REMOVE(&w->z_index, wp, zentry);
	TAILQ_INSERT_TAIL(&w->z_index, wp, zentry);

	if (!args_has(args, 'd'))
		window_set_active_pane(w, wp, 1);
	layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	events_fire_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_join_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct session		*dst_s;
	struct winlink		*src_wl, *dst_wl;
	struct window		*src_w, *dst_w;
	struct window_pane	*src_wp, *dst_wp;
	const char		*s;
	char			*cause = NULL;
	int			 flags = 0, dst_idx;
	struct layout_cell	*lc;

	dst_s = target->s;
	dst_wl = target->wl;
	dst_wp = target->wp;
	dst_w = dst_wl->window;
	dst_idx = dst_wl->idx;
	server_unzoom_window(dst_w);

	if (cmd_get_entry(self) == &cmd_move_pane_entry) {
		if (args_has(args, 'M'))
			return (cmd_join_pane_mouse_update(item));
		if (!window_pane_is_floating(dst_wp)) {
			cmdq_error(item, "pane is not floating");
			return (CMD_RETURN_ERROR);
		}
		if ((s = args_get(args, 'P')) != NULL)
			return (cmd_join_pane_place(item, dst_wl, dst_wp, s));
		if ((s = args_get(args, 'z')) != NULL)
			return (cmd_join_pane_zindex(item, dst_wl, dst_wp, s));
		if (args_has(args, 'X') ||
		    args_has(args, 'Y') ||
		    args_has(args, 'U') ||
		    args_has(args, 'D') ||
		    args_has(args, 'L') ||
		    args_has(args, 'R'))
			return (cmd_join_pane_move(item, args, dst_wl, dst_wp));
	}

	src_wl = source->wl;
	src_wp = source->wp;
	src_w = src_wl->window;
	server_unzoom_window(src_w);

	if (src_wp == dst_wp) {
		if (window_pane_is_floating(src_wp))
			return (cmd_join_pane_tile(item, args, src_w, src_wp));
		cmdq_error(item, "source and target panes must be different");
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'h'))
		flags |= SPAWN_HORIZONTAL;
	if (args_has(args, 'b'))
		flags |= SPAWN_BEFORE;
	if (args_has(args, 'f'))
		flags |= SPAWN_FULLSIZE;

	lc = layout_get_tiled_cell(item, args, dst_w, dst_wp, flags, &cause);
	if (cause != NULL) {
		cmdq_error(item, "size or position %s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	layout_close_pane(src_wp);

	server_client_remove_pane(src_wp);
	window_lost_pane(src_w, src_wp);
	TAILQ_REMOVE(&src_w->panes, src_wp, entry);
	TAILQ_REMOVE(&src_w->z_index, src_wp, zentry);

	src_wp->window = dst_w;
	options_set_parent(src_wp->options, dst_w->options);
	src_wp->flags |= (PANE_STYLECHANGED|PANE_THEMECHANGED);
	if (flags & SPAWN_BEFORE) {
		TAILQ_INSERT_BEFORE(dst_wp, src_wp, entry);
		TAILQ_INSERT_BEFORE(dst_wp, src_wp, zentry);
	} else {
		TAILQ_INSERT_AFTER(&dst_w->panes, dst_wp, src_wp, entry);
		TAILQ_INSERT_AFTER(&dst_w->z_index, dst_wp, src_wp, zentry);
	}
	layout_assign_pane(lc, src_wp, 0);
	colour_palette_from_option(&src_wp->palette, src_wp->options);

	recalculate_sizes();

	server_redraw_window(src_w);
	server_redraw_window(dst_w);

	if (!args_has(args, 'd')) {
		window_set_active_pane(dst_w, src_wp, 1);
		session_select(dst_s, dst_idx);
		cmd_find_from_session(current, dst_s, 0);
		server_redraw_session(dst_s);
	} else
		server_status_session(dst_s);

	if (window_count_panes(src_w, 1) == 0)
		server_kill_window(src_w, 1);
	else
		events_fire_window("window-layout-changed", src_w);
	events_fire_window("window-layout-changed", dst_w);

	return (CMD_RETURN_NORMAL);
}
