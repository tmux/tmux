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

static enum cmd_retval	cmd_display_message_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_display_message_entry = {
	.name = "display-message",
	.alias = "display",

	.args = { "ac:d:INpt:F:v", 0, 1, NULL },
	.usage = "[-aINpv] [-c target-client] [-d delay] [-F format] "
		 CMD_TARGET_PANE_USAGE " [message]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = CMD_AFTERHOOK|CMD_CLIENT_CFLAG|CMD_CLIENT_CANFAIL,
	.exec = cmd_display_message_exec
};

static void
cmd_display_message_each(const char *key, const char *value, void *arg)
{
	struct cmdq_item	*item = arg;

	cmdq_print(item, "%s=%s", key, value);
}

static enum cmd_retval
cmd_display_message_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct client		*tc = cmdq_get_target_client(item), *c;
	struct session		*s = target->s;
	struct winlink		*wl = target->wl;
	struct window_pane	*wp = target->wp;
	const char		*template;
	char			*msg, *cause;
	int			 delay = -1, flags;
	struct format_tree	*ft;
	u_int			 count = args_count(args);

	if (args_has(args, 'I')) {
		if (wp == NULL)
			return (CMD_RETURN_NORMAL);
		switch (window_pane_start_input(wp, item, &cause)) {
		case -1:
			cmdq_error(item, "%s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		case 1:
			return (CMD_RETURN_NORMAL);
		case 0:
			return (CMD_RETURN_WAIT);
		}
	}

	if (args_has(args, 'F') && count != 0) {
		cmdq_error(item, "only one of -F or argument must be given");
		return (CMD_RETURN_ERROR);
	}

	if (args_has(args, 'd')) {
		delay = args_strtonum(args, 'd', 0, UINT_MAX, &cause);
		if (cause != NULL) {
			cmdq_error(item, "delay %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	if (count != 0)
		template = args_string(args, 0);
	else
		template = args_get(args, 'F');
	if (template == NULL)
		template = DISPLAY_MESSAGE_TEMPLATE;

	/*
	 * -c is intended to be the client where the message should be
	 * displayed if -p is not given. But it makes sense to use it for the
	 * formats too, assuming it matches the session. If it doesn't, use the
	 * best client for the session.
	 */
	if (tc != NULL && tc->session == s)
		c = tc;
	else if (s != NULL)
		c = cmd_find_best_client(s);
	else
		c = NULL;
	if (args_has(args, 'v'))
		flags = FORMAT_VERBOSE;
	else
		flags = 0;
	ft = format_create(cmdq_get_client(item), item, FORMAT_NONE, flags);
	format_defaults(ft, c, s, wl, wp);

	if (args_has(args, 'a')) {
		format_each(ft, cmd_display_message_each, item);
		return (CMD_RETURN_NORMAL);
	}

	msg = format_expand_time(ft, template);
	if (cmdq_get_client(item) == NULL)
		cmdq_error(item, "%s", msg);
	else if (args_has(args, 'p'))
		cmdq_print(item, "%s", msg);
	else if (tc != NULL) {
		status_message_set(tc, delay, 0, args_has(args, 'N'), "%s",
		    msg);
	}
	free(msg);

	format_free(ft);

	return (CMD_RETURN_NORMAL);
}
