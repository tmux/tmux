/* $OpenBSD: cmd-display-message.c,v 1.68 2026/10/09 13:12:14 nicm Exp $ */

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
	.description = "Display a message or expand formats.",
	.alias = "display",

	.args = { "aCc:d:jlINpt:F:v", 0, 1, NULL },
	.usage = "[-aCIjlNpv] [-c target-client] [-d delay] [-F format] "
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
	char			*msg, *cause = NULL;
	int			 delay = -1, flags, Nflag = args_has(args, 'N');
	int			 Cflag = args_has(args, 'C');
	struct format_tree	*ft;
	u_int			 count = args_count(args);
	struct evbuffer		*evb;
	struct json_node	*jn;

	if (args_has(args, 'I') && !args_has(args, 'j')) {
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
	if (args_has(args, 'j') && template == NULL)
		template = "";
	else if (template == NULL)
		template = DISPLAY_MESSAGE_TEMPLATE;

	/*
	 * -c is also used for the client formats. If it was not given, tc is
	 * only the current client, so use it only if it matches the session.
	 */
	if (tc != NULL && (args_has(args, 'c') || tc->session == s))
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

	if (args_has(args, 'a') && !args_has(args, 'j')) {
		format_each(ft, cmd_display_message_each, item);
		format_free(ft);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'l'))
		msg = xstrdup(template);
	else
		msg = format_expand_time(ft, template);
	if (args_has(args, 'j')) {
		jn = json_parse(msg, &cause);
		if (jn == NULL) {
			cmdq_error(item, "%s", cause);
			free(cause);
			free(msg);
			format_free(ft);
			return (CMD_RETURN_ERROR);
		}
		free(msg);
		msg = json_to_string(jn);
		json_destroy_node(jn);
	}

	if (cmdq_get_client(item) == NULL)
		cmdq_error(item, "%s", msg);
	else if (args_has(args, 'p'))
		cmdq_print(item, "%s", msg);
	else if (tc != NULL && (tc->flags & CLIENT_CONTROL)) {
		evb = evbuffer_new();
		if (evb == NULL)
			fatalx("out of memory");
		evbuffer_add_printf(evb, "%%message %s", msg);
		server_client_print(tc, 0, evb);
		evbuffer_free(evb);
	} else if (tc != NULL)
		status_message_set(tc, delay, 0, Nflag, Cflag, "%s", msg);
	free(msg);

	format_free(ft);
	return (CMD_RETURN_NORMAL);
}
