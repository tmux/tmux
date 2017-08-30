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

	.args = { "dPF:n:s:t:", 0, 0 },
	.usage = "[-dP] [-F format] [-n window-name] [-s src-pane] "
		 "[-t dst-window]",

	.source = { 's', CMD_FIND_PANE, 0 },
	.target = { 't', CMD_FIND_WINDOW, CMD_FIND_WINDOW_INDEX },

	.flags = 0,
	.exec = cmd_break_pane_exec
};

static enum cmd_retval
cmd_break_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct cmd_find_state	*current = &item->shared->current;
	struct client		*c = cmd_find_client(item, NULL, 1);
	struct winlink		*wl = item->source.wl;
	struct session		*src_s = item->source.s;
	struct session		*dst_s = item->target.s;
	struct window_pane	*wp = item->source.wp;
	struct window		*w = wl->window;
	char			*name, *cause;
	int			 idx = item->target.idx;
	const char		*template;
	char			*cp;

	if (idx != -1 && winlink_find_by_index(&dst_s->windows, idx) != NULL) {
		cmdq_error(item, "index %d already in use", idx);
		return (CMD_RETURN_ERROR);
	}

	if (window_count_panes(w) == 1) {
		cmdq_error(item, "can't break with only one pane");
		return (CMD_RETURN_ERROR);
	}
	server_unzoom_window(w);

	TAILQ_REMOVE(&w->panes, wp, entry);
	window_lost_pane(w, wp);
	layout_close_pane(wp);

	w = wp->window = window_create(dst_s->sx, dst_s->sy);
	TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	w->active = wp;

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

	if (idx == -1)
		idx = -1 - options_get_number(dst_s->options, "base-index");
	wl = session_attach(dst_s, w, idx, &cause); /* can't fail */
	if (!args_has(self->args, 'd')) {
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
		cp = format_single(item, template, c, dst_s, wl, wp);
		cmdq_print(item, "%s", cp);
		free(cp);
	}
	return (CMD_RETURN_NORMAL);
}
