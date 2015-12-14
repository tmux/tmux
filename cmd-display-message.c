/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
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
#include <time.h>

#include "tmux.h"

/*
 * Displays a message in the status line.
 */

#define DISPLAY_MESSAGE_TEMPLATE			\
	"[#{session_name}] #{window_index}:"		\
	"#{window_name}, current pane #{pane_index} "	\
	"- (%H:%M %d-%b-%y)"

enum cmd_retval	 cmd_display_message_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_display_message_entry = {
	.name = "display-message",
	.alias = "display",

	.args = { "c:pt:F:", 0, 1 },
	.usage = "[-p] [-c target-client] [-F format] "
		 CMD_TARGET_PANE_USAGE " [message]",

	.cflag = CMD_CLIENT_CANFAIL,
	.tflag = CMD_PANE,

	.flags = 0,
	.exec = cmd_display_message_exec
};

enum cmd_retval
cmd_display_message_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c = cmdq->state.c;
	struct session		*s = cmdq->state.tflag.s;
	struct winlink		*wl = cmdq->state.tflag.wl;
	struct window_pane	*wp = cmdq->state.tflag.wp;
	const char		*template;
	char			*msg;
	struct format_tree	*ft;

	if (args_has(args, 'F') && args->argc != 0) {
		cmdq_error(cmdq, "only one of -F or argument must be given");
		return (CMD_RETURN_ERROR);
	}

	template = args_get(args, 'F');
	if (args->argc != 0)
		template = args->argv[0];
	if (template == NULL)
		template = DISPLAY_MESSAGE_TEMPLATE;

	ft = format_create(cmdq, 0);
	format_defaults(ft, c, s, wl, wp);

	msg = format_expand_time(ft, template, time(NULL));
	if (args_has(self->args, 'p'))
		cmdq_print(cmdq, "%s", msg);
	else
		status_message_set(c, "%s", msg);
	free(msg);
	format_free(ft);

	return (CMD_RETURN_NORMAL);
}
