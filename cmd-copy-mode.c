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

static enum cmd_retval	cmd_copy_mode_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_copy_mode_entry = {
	.name = "copy-mode",
	.alias = NULL,

	.args = { "deHMqSs:t:u", 0, 0, NULL },
	.usage = "[-deHMqSu] [-s src-pane] " CMD_TARGET_PANE_USAGE,

	.source =  { 's', CMD_FIND_PANE, 0 },
	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_copy_mode_exec
};

const struct cmd_entry cmd_clock_mode_entry = {
	.name = "clock-mode",
	.alias = NULL,

	.args = { "t:", 0, 0, NULL },
	.usage = CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_copy_mode_exec
};

static enum cmd_retval
cmd_copy_mode_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct key_event	*event = cmdq_get_event(item);
	struct cmd_find_state	*source = cmdq_get_source(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct client		*c = cmdq_get_client(item);
	struct session		*s;
	struct window_pane	*wp = target->wp, *swp;

	if (args_has(args, 'q')) {
		window_pane_reset_mode_all(wp);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'M')) {
		if ((wp = cmd_mouse_pane(&event->m, &s, NULL)) == NULL)
			return (CMD_RETURN_NORMAL);
		if (c == NULL || c->session != s)
			return (CMD_RETURN_NORMAL);
	}

	if (cmd_get_entry(self) == &cmd_clock_mode_entry) {
		window_pane_set_mode(wp, NULL, &window_clock_mode, NULL, NULL);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 's'))
		swp = source->wp;
	else
		swp = wp;
	if (!window_pane_set_mode(wp, swp, &window_copy_mode, NULL, args)) {
		if (args_has(args, 'M'))
			window_copy_start_drag(c, &event->m);
	}
	if (args_has(args, 'u'))
		window_copy_pageup(wp, 0);
	if (args_has(args, 'd'))
		window_copy_pagedown(wp, 0, args_has(args, 'e'));
	if (args_has(args, 'S')) {
		window_copy_scroll(wp, c->tty.mouse_slider_mpos, event->m.y,
		    args_has(args, 'e'));
		return (CMD_RETURN_NORMAL);
	}

	return (CMD_RETURN_NORMAL);
}
