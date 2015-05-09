/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
	"send-keys", "send",
	"lRMt:", 0, -1,
	"[-lRM] " CMD_TARGET_PANE_USAGE " key ...",
	0,
	cmd_send_keys_exec
};

const struct cmd_entry cmd_send_prefix_entry = {
	"send-prefix", NULL,
	"2t:", 0, 0,
	"[-2] " CMD_TARGET_PANE_USAGE,
	0,
	cmd_send_keys_exec
};

enum cmd_retval
cmd_send_keys_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct mouse_event	*m = &cmdq->item->mouse;
	struct window_pane	*wp;
	struct session		*s;
	const u_char		*str;
	int			 i, key;

	if (args_has(args, 'M')) {
		wp = cmd_mouse_pane(m, &s, NULL);
		if (wp == NULL) {
			cmdq_error(cmdq, "no mouse target");
			return (CMD_RETURN_ERROR);
		}
		window_pane_key(wp, NULL, s, m->key, m);
		return (CMD_RETURN_NORMAL);
	}

	if (cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp) == NULL)
		return (CMD_RETURN_ERROR);

	if (self->entry == &cmd_send_prefix_entry) {
		if (args_has(args, '2'))
			key = options_get_number(&s->options, "prefix2");
		else
			key = options_get_number(&s->options, "prefix");
		window_pane_key(wp, NULL, s, key, NULL);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'R'))
		input_reset(wp);

	for (i = 0; i < args->argc; i++) {
		str = args->argv[i];

		if (!args_has(args, 'l') &&
		    (key = key_string_lookup_string(str)) != KEYC_NONE) {
			window_pane_key(wp, NULL, s, key, NULL);
		} else {
			for (; *str != '\0'; str++)
				window_pane_key(wp, NULL, s, *str, NULL);
		}
	}

	return (CMD_RETURN_NORMAL);
}
