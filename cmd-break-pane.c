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
 * Break pane off into a window.
 */

#define BREAK_PANE_TEMPLATE "#{session_name}:#{window_index}.#{pane_index}"

enum cmd_retval	 cmd_break_pane_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_break_pane_entry = {
	"break-pane", "breakp",
	"dPF:s:t:", 0, 0,
	"[-dP] [-F format] " CMD_SRCDST_PANE_USAGE,
	0,
	cmd_break_pane_exec
};

enum cmd_retval
cmd_break_pane_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct winlink		*wl;
	struct session		*src_s;
	struct session		*dst_s;
	struct window_pane	*wp;
	struct window		*w;
	char			*name;
	char			*cause;
	int			 idx;
	struct format_tree	*ft;
	const char		*template;
	char			*cp;

	wl = cmd_find_pane(cmdq, args_get(args, 's'), &src_s, &wp);
	if (wl == NULL)
		return (CMD_RETURN_ERROR);
	if ((idx = cmd_find_index(cmdq, args_get(args, 't'), &dst_s)) == -2)
		return (CMD_RETURN_ERROR);
	if (idx != -1 && winlink_find_by_index(&dst_s->windows, idx) != NULL) {
		cmdq_error(cmdq, "index %d already in use", idx);
		return (CMD_RETURN_ERROR);
	}
	w = wl->window;

	if (window_count_panes(w) == 1) {
		cmdq_error(cmdq, "can't break with only one pane");
		return (CMD_RETURN_ERROR);
	}
	server_unzoom_window(w);

	TAILQ_REMOVE(&w->panes, wp, entry);
	window_lost_pane(w, wp);
	layout_close_pane(wp);

	w = wp->window = window_create1(dst_s->sx, dst_s->sy);
	TAILQ_INSERT_HEAD(&w->panes, wp, entry);
	w->active = wp;
	name = default_window_name(w);
	window_set_name(w, name);
	free(name);
	layout_init(w, wp);

	if (idx == -1)
		idx = -1 - options_get_number(&dst_s->options, "base-index");
	wl = session_attach(dst_s, w, idx, &cause); /* can't fail */
	if (!args_has(self->args, 'd'))
		session_select(dst_s, wl->idx);

	server_redraw_session(src_s);
	if (src_s != dst_s)
		server_redraw_session(dst_s);
	server_status_session_group(src_s);
	if (src_s != dst_s)
		server_status_session_group(dst_s);

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = BREAK_PANE_TEMPLATE;

		ft = format_create();
		format_defaults(ft, cmd_find_client(cmdq, NULL, 1), dst_s, wl,
		    wp);

		cp = format_expand(ft, template);
		cmdq_print(cmdq, "%s", cp);
		free(cp);

		format_free(ft);
	}
	return (CMD_RETURN_NORMAL);
}
