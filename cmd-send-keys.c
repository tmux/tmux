/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <string.h>

#include "tmux.h"

/*
 * Send keys to client.
 */

enum cmd_retval	 cmd_send_keys_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_send_keys_entry = {
	.name = "send-keys",
	.alias = "send",

	.args = { "lRMt:", 0, -1 },
	.usage = "[-lRM] " CMD_TARGET_PANE_USAGE " key ...",

	.tflag = CMD_PANE,

	.flags = 0,
	.exec = cmd_send_keys_exec
};

const struct cmd_entry cmd_send_prefix_entry = {
	.name = "send-prefix",
	.alias = NULL,

	.args = { "2t:", 0, 0 },
	.usage = "[-2] " CMD_TARGET_PANE_USAGE,

	.tflag = CMD_PANE,

	.flags = 0,
	.exec = cmd_send_keys_exec
};

enum cmd_retval
cmd_send_keys_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct window_pane	*wp = cmdq->state.tflag.wp;
	struct session		*s = cmdq->state.tflag.s;
	struct mouse_event	*m = &cmdq->item->mouse;
	const u_char		*keystr;
	int			 i, literal;
	key_code		 key;

	if (args_has(args, 'M')) {
		wp = cmd_mouse_pane(m, &s, NULL);
		if (wp == NULL) {
			cmdq_error(cmdq, "no mouse target");
			return (CMD_RETURN_ERROR);
		}
		window_pane_key(wp, NULL, s, m->key, m);
		return (CMD_RETURN_NORMAL);
	}

	if (self->entry == &cmd_send_prefix_entry) {
		if (args_has(args, '2'))
			key = options_get_number(s->options, "prefix2");
		else
			key = options_get_number(s->options, "prefix");
		window_pane_key(wp, NULL, s, key, NULL);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'R'))
		input_reset(wp, 1);

	for (i = 0; i < args->argc; i++) {
		literal = args_has(args, 'l');
		if (!literal) {
			key = key_string_lookup_string(args->argv[i]);
			if (key != KEYC_NONE && key != KEYC_UNKNOWN)
				window_pane_key(wp, NULL, s, key, NULL);
			else
				literal = 1;
		}
		if (literal) {
			for (keystr = args->argv[i]; *keystr != '\0'; keystr++)
				window_pane_key(wp, NULL, s, *keystr, NULL);
		}
	}

	return (CMD_RETURN_NORMAL);
}
