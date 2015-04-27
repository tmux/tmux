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
	"display-message", "display",
	"c:pt:F:", 0, 1,
	"[-p] [-c target-client] [-F format] " CMD_TARGET_PANE_USAGE
	" [message]",
	0,
	cmd_display_message_exec
};

enum cmd_retval
cmd_display_message_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c;
	struct session		*s;
	struct winlink		*wl;
	struct window_pane	*wp;
	const char		*template;
	char			*msg;
	struct format_tree	*ft;
	char			 out[BUFSIZ];
	time_t			 t;
	size_t			 len;

	if (args_has(args, 't')) {
		wl = cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp);
		if (wl == NULL)
			return (CMD_RETURN_ERROR);
	} else {
		wl = cmd_find_pane(cmdq, NULL, &s, &wp);
		if (wl == NULL)
			return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'F') && args->argc != 0) {
		cmdq_error(cmdq, "only one of -F or argument must be given");
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'c')) {
		c = cmd_find_client(cmdq, args_get(args, 'c'), 0);
		if (c == NULL)
			return (CMD_RETURN_ERROR);
	} else {
		c = cmd_find_client(cmdq, NULL, 1);
		if (c == NULL && !args_has(self->args, 'p')) {
			cmdq_error(cmdq, "no client available");
			return (CMD_RETURN_ERROR);
		}
	}

	template = args_get(args, 'F');
	if (args->argc != 0)
		template = args->argv[0];
	if (template == NULL)
		template = DISPLAY_MESSAGE_TEMPLATE;

	ft = format_create();
	format_defaults(ft, c, s, wl, wp);

	t = time(NULL);
	len = strftime(out, sizeof out, template, localtime(&t));
	out[len] = '\0';

	msg = format_expand(ft, out);
	if (args_has(self->args, 'p'))
		cmdq_print(cmdq, "%s", msg);
	else
		status_message_set(c, "%s", msg);
	free(msg);
	format_free(ft);

	return (CMD_RETURN_NORMAL);
}
