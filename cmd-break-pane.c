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
 * Break pane off into a window.
 */

#define BREAK_PANE_TEMPLATE "#{session_name}:#{window_index}.#{pane_index}"

static enum cmd_retval	cmd_break_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_break_pane_entry = {
	.name = "break-pane",
	.alias = "breakp",

	.args = { "abdF:Ln:Ps:t:Wx:X:y:Y:", 0, 0, NULL },
	.usage = "[-abdP] [-F format] [-n window-name] [-s src-pane] "
		 "[-x width] [-y height] [-X x-position] [-Y y-position] "
		 "[-t dst-window]",

	.source = { 's', CMD_FIND_PANE, 0 },
	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_WINDOW_INDEX },

	.flags = 0,
	.exec = cmd_break_pane_exec
};

static enum cmd_retval
cmd_break_pane_float_pane(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct window		*w = source->wl->window;
	struct window_pane	*wp = source->wp;
	struct layout_cell	*lc = NULL, *lcph = wp->placeholder_layout_cell;
	char			*cause = NULL;

	if (window_pane_is_floating(wp)) {
		cmdq_error(item, "pane is already floating");
		return (CMD_RETURN_ERROR);
	}
	if (window_pane_is_hidden(wp)) {
		cmdq_error(item, "can't float a hidden pane");
		return (CMD_RETURN_ERROR);
	}
	if (w->flags & WINDOW_ZOOMED) {
		cmdq_error(item, "can't float a pane while window is zoomed");
		return (CMD_RETURN_ERROR);
	}

	lcph = layout_get_floating_cell(item, args, w, wp, lcph, &cause);
	if (lcph == NULL) {
		cmdq_error(item, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	wp->placeholder_layout_cell = lcph;
	lc = layout_make_placeholder(w, wp, wp->layout_cell);
	layout_make_leaf(lc, wp);
	lc->flags |= LAYOUT_CELL_FLOATING;

	TAILQ_REMOVE(&w->z_index, wp, zentry);
	TAILQ_INSERT_HEAD(&w->z_index, wp, zentry);

	if (w->layout_root != NULL)
		layout_fix_offsets(w);
	layout_fix_panes(w, NULL);
	window_set_active_pane(w, wp, 1);
	notify_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_break_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct client		*tc = cmdq_get_target_client(item);
	struct winlink		*dwl, *swl = source->wl;
	struct session		*ss = source->s;
	struct session		*ds = target->s;
	struct window_pane	*wp = source->wp;
	struct window		*dw, *sw = swl->window;
	struct layout_cell	*lc;
	char			*name, *cp, *cause = NULL;
	int			 before, floating, idx = target->idx;
	const char		*template;

	floating = window_pane_is_floating(wp);
	if (!floating && args_has(args, 'W'))
		return cmd_break_pane_float_pane(self, item);

	before = args_has(args, 'b');
	if (args_has(args, 'a') || before) {
		if (target->wl != NULL)
			idx = winlink_shuffle_up(ds, target->wl, before);
		else
			idx = winlink_shuffle_up(ds, ds->curw, before);
		if (idx == -1)
			return (CMD_RETURN_ERROR);
	}
	server_unzoom_window(sw);

	if (window_count_panes(sw, 1) == 1) {
		if (server_link_window(ss, swl, ds, idx, 0,
		    !args_has(args, 'd'), &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (args_has(args, 'n')) {
			window_set_name(sw, args_get(args, 'n'));
			options_set_number(sw->options, "automatic-rename", 0);
		}
		server_unlink_window(ss, swl);
		dwl = winlink_find_by_window(&ds->windows, sw);
		if (dwl == NULL)
			return (CMD_RETURN_ERROR);
		goto out;
	}
	if (idx != -1 && winlink_find_by_index(&ds->windows, idx) != NULL) {
		cmdq_error(item, "index in use: %d", idx);
		return (CMD_RETURN_ERROR);
	}

	TAILQ_REMOVE(&sw->panes, wp, entry);
	TAILQ_REMOVE(&sw->z_index, wp, zentry);
	server_client_remove_pane(wp);
	window_lost_pane(sw, wp);
	if (floating) {
		/* Floating panes always have a concealed, tiled placeholder */
		layout_destroy_cell(sw, wp->placeholder_layout_cell,
			&sw->layout_root);
		wp->placeholder_layout_cell = NULL;
		if (args_has(args, 'L'))
			layout_make_placeholder(sw, wp, wp->layout_cell);
	} else
		layout_close_pane(wp);

	dw = wp->window = window_create(sw->sx, sw->sy, sw->xpixel, sw->ypixel);
	options_set_parent(wp->options, dw->options);
	wp->flags |= (PANE_STYLECHANGED|PANE_THEMECHANGED);
	TAILQ_INSERT_HEAD(&dw->panes, wp, entry);
	TAILQ_INSERT_HEAD(&dw->z_index, wp, zentry);
	dw->active = wp;
	dw->latest = tc;

	if (!args_has(args, 'n')) {
		name = default_window_name(dw);
		window_set_name(dw, name);
		free(name);
	} else {
		window_set_name(dw, args_get(args, 'n'));
		options_set_number(dw->options, "automatic-rename", 0);
	}

	if (floating & !args_has(args, 'L')) {
		lc = layout_get_floating_cell(item, args, dw, wp,
		    wp->layout_cell, &cause);
		if (cause != NULL) {
			cmdq_error(item, "failed to break pane: %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		layout_make_leaf(lc, wp);
		layout_make_placeholder(dw, wp, NULL);
	} else
		layout_init(dw, wp);
	wp->flags |= PANE_CHANGED;
	colour_palette_from_option(&wp->palette, wp->options);

	if (idx == -1)
		idx = -1 - options_get_number(ds->options, "base-index");
	dwl = session_attach(ds, dw, idx, &cause); /* can't fail */
	if (!args_has(args, 'd')) {
		session_select(ds, dwl->idx);
		cmd_find_from_session(current, ds, 0);
	}

	server_redraw_session(ss);
	if (ss != ds)
		server_redraw_session(ds);
	server_status_session_group(ss);
	if (ss != ds)
		server_status_session_group(ds);

out:
	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = BREAK_PANE_TEMPLATE;
		cp = format_single(item, template, tc, ds, dwl, wp);
		cmdq_print(item, "%s", cp);
		free(cp);
	}
	return (CMD_RETURN_NORMAL);
}
