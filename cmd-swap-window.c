/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

enum cmd_retval	cmd_swap_window_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_swap_window_entry = {
	"swap-window", "swapw",
	"ds:t:", 0, 0,
	"[-d] " CMD_SRCDST_WINDOW_USAGE,
	0,
	cmd_swap_window_exec
};

enum cmd_retval
cmd_swap_window_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	const char		*target_src, *target_dst;
	struct session		*src, *dst;
	struct session_group	*sg_src, *sg_dst;
	struct winlink		*wl_src, *wl_dst;
	struct window		*w;

	target_src = args_get(args, 's');
	if ((wl_src = cmd_find_window_marked(cmdq, target_src, &src)) == NULL)
		return (CMD_RETURN_ERROR);
	target_dst = args_get(args, 't');
	if ((wl_dst = cmd_find_window(cmdq, target_dst, &dst)) == NULL)
		return (CMD_RETURN_ERROR);

	sg_src = session_group_find(src);
	sg_dst = session_group_find(dst);
	if (src != dst &&
	    sg_src != NULL && sg_dst != NULL && sg_src == sg_dst) {
		cmdq_error(cmdq, "can't move window, sessions are grouped");
		return (CMD_RETURN_ERROR);
	}

	if (wl_dst->window == wl_src->window)
		return (CMD_RETURN_NORMAL);

	w = wl_dst->window;
	wl_dst->window = wl_src->window;
	wl_src->window = w;

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
