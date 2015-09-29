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

#include "tmux.h"

/*
 * Enter copy or clock mode.
 */

enum cmd_retval	 cmd_copy_mode_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_copy_mode_entry = {
	.name = "copy-mode",
	.alias = NULL,

	.args = { "Met:u", 0, 0 },
	.usage = "[-Mu] " CMD_TARGET_PANE_USAGE,

	.tflag = CMD_PANE,

	.flags = 0,
	.exec = cmd_copy_mode_exec
};

const struct cmd_entry cmd_clock_mode_entry = {
	.name = "clock-mode",
	.alias = NULL,

	.args = { "t:", 0, 0 },
	.usage = CMD_TARGET_PANE_USAGE,

	.tflag = CMD_PANE,

	.flags = 0,
	.exec = cmd_copy_mode_exec
};

enum cmd_retval
cmd_copy_mode_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c = cmdq->client;
	struct session		*s;
	struct window_pane	*wp = cmdq->state.tflag.wp;
	u_int			 b;
	int			 scroll_exit;

	if (args_has(args, 'M')) {
		if ((wp = cmd_mouse_pane(&cmdq->item->mouse, &s, NULL)) == NULL)
			return (CMD_RETURN_NORMAL);
		if (c == NULL || c->session != s)
			return (CMD_RETURN_NORMAL);
	}

	if (self->entry == &cmd_clock_mode_entry) {
		window_pane_set_mode(wp, &window_clock_mode);
		return (CMD_RETURN_NORMAL);
	}

	scroll_exit = args_has(self->args, 'e');
	if (args_has(args, 'M')) {
		b = cmdq->item->mouse.b;
		if (MOUSE_DRAG(b))
			window_copy_start_drag(c, &cmdq->item->mouse, scroll_exit);
		else if (!MOUSE_WHEEL(b) && !MOUSE_RELEASE(b))
			window_copy_mouse_down(c, &cmdq->item->mouse, scroll_exit);
		else
			return (CMD_RETURN_ERROR);
	} else {
		if (window_enter_copy_mode(wp, scroll_exit) != 0)
			return (CMD_RETURN_NORMAL);
		if (args_has(self->args, 'u'))
			window_copy_pageup(wp, 0);
	}

	return (CMD_RETURN_NORMAL);
}
