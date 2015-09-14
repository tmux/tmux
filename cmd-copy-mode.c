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

#include "tmux.h"

/*
 * Enter copy or clock mode.
 */

enum cmd_retval	 cmd_copy_mode_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_copy_mode_entry = {
	"copy-mode", NULL,
	"Met:u", 0, 0,
	"[-Meu] " CMD_TARGET_PANE_USAGE,
	0,
	cmd_copy_mode_exec
};

const struct cmd_entry cmd_clock_mode_entry = {
	"clock-mode", NULL,
	"t:", 0, 0,
	CMD_TARGET_PANE_USAGE,
	0,
	cmd_copy_mode_exec
};

enum cmd_retval
cmd_copy_mode_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c = cmdq->client;
	struct session		*s;
	struct window_pane	*wp;

	if (args_has(args, 'M')) {
		if ((wp = cmd_mouse_pane(&cmdq->item->mouse, &s, NULL)) == NULL)
			return (CMD_RETURN_NORMAL);
		if (c == NULL || c->session != s)
			return (CMD_RETURN_NORMAL);
	} else if (cmd_find_pane(cmdq, args_get(args, 't'), NULL, &wp) == NULL)
		return (CMD_RETURN_ERROR);

	if (self->entry == &cmd_clock_mode_entry) {
		window_pane_set_mode(wp, &window_clock_mode);
		return (CMD_RETURN_NORMAL);
	}

	if (wp->mode != &window_copy_mode) {
		if (window_pane_set_mode(wp, &window_copy_mode) != 0)
			return (CMD_RETURN_NORMAL);
		window_copy_init_from_pane(wp, args_has(self->args, 'e'));
	}
	if (args_has(args, 'M')) {
		if (wp->mode != NULL && wp->mode != &window_copy_mode)
			return (CMD_RETURN_NORMAL);
		window_copy_start_drag(c, &cmdq->item->mouse);
	}
	if (wp->mode == &window_copy_mode && args_has(self->args, 'u'))
		window_copy_pageup(wp);

	return (CMD_RETURN_NORMAL);
}
