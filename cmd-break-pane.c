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

	.args = { "abdPF:n:s:t:", 0, 0, NULL },
	.usage = "[-abdP] [-F format] [-n window-name] [-s src-pane] "
		 "[-t dst-window]",

	.source = { 's', CMD_FIND_PANE, 0 },
	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_WINDOW_INDEX },

	.flags = 0,
	.exec = cmd_break_pane_exec
};

static enum cmd_retval
cmd_break_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct client		*tc = cmdq_get_target_client(item);
	struct winlink		*wl = source->wl;
	struct session		*src_s = source->s;
	struct session		*dst_s = target->s;
	struct window_pane	*wp = source->wp;
	struct window		*w = wl->window;
	char			*name, *cause, *cp;
	int			 idx = target->idx, before;
	const char		*template;

	before = args_has(args, 'b');
	if (args_has(args, 'a') || before) {
		if (target->wl != NULL)
			idx = winlink_shuffle_up(dst_s, target->wl, before);
		else
			idx = winlink_shuffle_up(dst_s, dst_s->curw, before);
		if (idx == -1)
			return (CMD_RETURN_ERROR);
	}
	server_unzoom_window(w);

	if (window_count_panes(w) == 1) {
		if (server_link_window(src_s, wl, dst_s, idx, 0,
		    !args_has(args, 'd'), &cause) != 0) {
			cmdq_error(item, "%s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
		if (args_has(args, 'n')) {
			window_set_name(w, args_get(args, 'n'));
			options_set_number(w->options, "automatic-rename", 0);
		}
		server_unlink_window(src_s, wl);
		return (CMD_RETURN_NORMAL);
	}
	if (idx != -1 && winlink_find_by_index(&dst_s->windows, idx) != NULL) {
		cmdq_error(item, "index in use: %d", idx);
		return (CMD_RETURN_ERROR);
	}

	TAILQ_REMOVE(&w->panes, wp, entry);
	server_client_remove_pane(wp);
	window_lost_pane(w, wp);
	layout_close_pane(wp);

	w = wp->window = window_create(w->sx, w->sy, w->xpixel, w->ypixel);
	options_set_parent(wp->options, w->options);
	wp->flags |= PANE_STYLECHANGED;
	TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	w->active = wp;
	w->latest = tc;

	if (!args_has(args, 'n')) {
		name = default_window_name(w);
		window_set_name(w, name);
		free(name);
	} else {
		window_set_name(w, args_get(args, 'n'));
		options_set_number(w->options, "automatic-rename", 0);
	}

	layout_init(w, wp);
	wp->flags |= PANE_CHANGED;
	colour_palette_from_option(&wp->palette, wp->options);

	if (idx == -1)
		idx = -1 - options_get_number(dst_s->options, "base-index");
	wl = session_attach(dst_s, w, idx, &cause); /* can't fail */
	if (!args_has(args, 'd')) {
		session_select(dst_s, wl->idx);
		cmd_find_from_session(current, dst_s, 0);
	}

	server_redraw_session(src_s);
	if (src_s != dst_s)
		server_redraw_session(dst_s);
	server_status_session_group(src_s);
	if (src_s != dst_s)
		server_status_session_group(dst_s);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = BREAK_PANE_TEMPLATE;
		cp = format_single(item, template, tc, dst_s, wl, wp);
		cmdq_print(item, "%s", cp);
		free(cp);
	}
	return (CMD_RETURN_NORMAL);
}
