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
 * Swap two panes.
 */

static enum cmd_retval	cmd_swap_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_swap_pane_entry = {
	.name = "swap-pane",
	.alias = "swapp",

	.args = { "dDs:t:UZ", 0, 0, NULL },
	.usage = "[-dDUZ] " CMD_SRCDST_PANE_USAGE,

	.source = { 's', CMD_FIND_PANE, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_swap_pane_exec
};

static enum cmd_retval
cmd_swap_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct window		*src_w, *dst_w;
	struct window_pane	*tmp_wp, *src_wp, *dst_wp;
	struct layout_cell	*src_lc, *dst_lc;
	u_int			 sx, sy, xoff, yoff;

	dst_w = target->wl->window;
	dst_wp = target->wp;
	src_w = source->wl->window;
	src_wp = source->wp;

	if (window_push_zoom(dst_w, 0, args_has(args, 'Z')))
		server_redraw_window(dst_w);

	if (args_has(args, 'D')) {
		src_w = dst_w;
		src_wp = TAILQ_NEXT(dst_wp, entry);
		if (src_wp == NULL)
			src_wp = TAILQ_FIRST(&dst_w->panes);
	} else if (args_has(args, 'U')) {
		src_w = dst_w;
		src_wp = TAILQ_PREV(dst_wp, window_panes, entry);
		if (src_wp == NULL)
			src_wp = TAILQ_LAST(&dst_w->panes, window_panes);
	}

	if (src_w != dst_w && window_push_zoom(src_w, 0, args_has(args, 'Z')))
		server_redraw_window(src_w);

	if (src_wp == dst_wp)
		goto out;

	server_client_remove_pane(src_wp);
	server_client_remove_pane(dst_wp);

	tmp_wp = TAILQ_PREV(dst_wp, window_panes, entry);
	TAILQ_REMOVE(&dst_w->panes, dst_wp, entry);
	TAILQ_REPLACE(&src_w->panes, src_wp, dst_wp, entry);
	if (tmp_wp == src_wp)
		tmp_wp = dst_wp;
	if (tmp_wp == NULL)
		TAILQ_INSERT_HEAD(&dst_w->panes, src_wp, entry);
	else
		TAILQ_INSERT_AFTER(&dst_w->panes, tmp_wp, src_wp, entry);

	src_lc = src_wp->layout_cell;
	dst_lc = dst_wp->layout_cell;
	src_lc->wp = dst_wp;
	dst_wp->layout_cell = src_lc;
	dst_lc->wp = src_wp;
	src_wp->layout_cell = dst_lc;

	src_wp->window = dst_w;
	options_set_parent(src_wp->options, dst_w->options);
	src_wp->flags |= PANE_STYLECHANGED;
	dst_wp->window = src_w;
	options_set_parent(dst_wp->options, src_w->options);
	dst_wp->flags |= PANE_STYLECHANGED;

	sx = src_wp->sx; sy = src_wp->sy;
	xoff = src_wp->xoff; yoff = src_wp->yoff;
	src_wp->xoff = dst_wp->xoff; src_wp->yoff = dst_wp->yoff;
	window_pane_resize(src_wp, dst_wp->sx, dst_wp->sy);
	dst_wp->xoff = xoff; dst_wp->yoff = yoff;
	window_pane_resize(dst_wp, sx, sy);

	if (!args_has(args, 'd')) {
		if (src_w != dst_w) {
			window_set_active_pane(src_w, dst_wp, 1);
			window_set_active_pane(dst_w, src_wp, 1);
		} else {
			tmp_wp = dst_wp;
			window_set_active_pane(src_w, tmp_wp, 1);
		}
	} else {
		if (src_w->active == src_wp)
			window_set_active_pane(src_w, dst_wp, 1);
		if (dst_w->active == dst_wp)
			window_set_active_pane(dst_w, src_wp, 1);
	}
	if (src_w != dst_w) {
		window_pane_stack_remove(&src_w->last_panes, src_wp);
		window_pane_stack_remove(&dst_w->last_panes, dst_wp);
		colour_palette_from_option(&src_wp->palette, src_wp->options);
		colour_palette_from_option(&dst_wp->palette, dst_wp->options);
	}
	server_redraw_window(src_w);
	server_redraw_window(dst_w);
	notify_window("window-layout-changed", src_w);
	if (src_w != dst_w)
		notify_window("window-layout-changed", dst_w);

out:
	if (window_pop_zoom(src_w))
		server_redraw_window(src_w);
	if (src_w != dst_w && window_pop_zoom(dst_w))
		server_redraw_window(dst_w);
	return (CMD_RETURN_NORMAL);
}
