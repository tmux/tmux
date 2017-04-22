/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Swap one window with another.
 */

static enum cmd_retval	cmd_swap_window_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_swap_window_entry = {
	.name = "swap-window",
	.alias = "swapw",

	.args = { "ds:t:", 0, 0 },
	.usage = "[-d] " CMD_SRCDST_WINDOW_USAGE,

	.source = { 's', CMD_FIND_WINDOW, CMD_FIND_DEFAULT_MARKED },
	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_swap_window_exec
};

static enum cmd_retval
cmd_swap_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct session		*src, *dst;
	struct session_group	*sg_src, *sg_dst;
	struct winlink		*wl_src, *wl_dst;
	struct window		*w_src, *w_dst;

	wl_src = item->source.wl;
	src = item->source.s;
	sg_src = session_group_contains(src);

	wl_dst = item->target.wl;
	dst = item->target.s;
	sg_dst = session_group_contains(dst);

	if (src != dst && sg_src != NULL && sg_dst != NULL &&
	    sg_src == sg_dst) {
		cmdq_error(item, "can't move window, sessions are grouped");
		return (CMD_RETURN_ERROR);
	}

	if (wl_dst->window == wl_src->window)
		return (CMD_RETURN_NORMAL);

	w_dst = wl_dst->window;
	TAILQ_REMOVE(&w_dst->winlinks, wl_dst, wentry);
	w_src = wl_src->window;
	TAILQ_REMOVE(&w_src->winlinks, wl_src, wentry);

	wl_dst->window = w_src;
	TAILQ_INSERT_TAIL(&w_src->winlinks, wl_dst, wentry);
	wl_src->window = w_dst;
	TAILQ_INSERT_TAIL(&w_dst->winlinks, wl_src, wentry);

	if (!args_has(self->args, 'd')) {
		session_select(dst, wl_dst->idx);
		if (src != dst)
			session_select(src, wl_src->idx);
	}
	session_group_synchronize_from(src);
	server_redraw_session_group(src);
	if (src != dst) {
		session_group_synchronize_from(dst);
		server_redraw_session_group(dst);
	}
	recalculate_sizes();

	return (CMD_RETURN_NORMAL);
}
