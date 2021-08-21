/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <unistd.h>

#include "tmux.h"

/*
 * Show message log.
 */

#define SHOW_MESSAGES_TEMPLATE \
	"#{t/p:message_time}: #{message_text}"

static enum cmd_retval	cmd_show_messages_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_show_messages_entry = {
	.name = "show-messages",
	.alias = "showmsgs",

	.args = { "JTt:", 0, 0, NULL },
	.usage = "[-JT] " CMD_TARGET_CLIENT_USAGE,

	.flags = CMD_AFTERHOOK|CMD_CLIENT_TFLAG,
	.exec = cmd_show_messages_exec
};

static int
cmd_show_messages_terminals(struct cmd *self, struct cmdq_item *item, int blank)
{
	struct args	*args = cmd_get_args(self);
	struct client	*tc = cmdq_get_target_client(item);
	struct tty_term	*term;
	u_int		 i, n;

	n = 0;
	LIST_FOREACH(term, &tty_terms, entry) {
		if (args_has(args, 't') && term != tc->tty.term)
			continue;
		if (blank) {
			cmdq_print(item, "%s", "");
			blank = 0;
		}
		cmdq_print(item, "Terminal %u: %s for %s, flags=0x%x:", n,
		    term->name, term->tty->client->name, term->flags);
		n++;
		for (i = 0; i < tty_term_ncodes(); i++)
			cmdq_print(item, "%s", tty_term_describe(term, i));
	}
	return (n != 0);
}

static enum cmd_retval
cmd_show_messages_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct message_entry	*msg;
	char			*s;
	int			 done, blank;
	struct format_tree	*ft;

	done = blank = 0;
	if (args_has(args, 'T')) {
		blank = cmd_show_messages_terminals(self, item, blank);
		done = 1;
	}
	if (args_has(args, 'J')) {
		job_print_summary(item, blank);
		done = 1;
	}
	if (done)
		return (CMD_RETURN_NORMAL);

	ft = format_create_from_target(item);
	TAILQ_FOREACH_REVERSE(msg, &message_log, message_list, entry) {
		format_add(ft, "message_text", "%s", msg->msg);
		format_add(ft, "message_number", "%u", msg->msg_num);
		format_add_tv(ft, "message_time", &msg->msg_time);

		s = format_expand(ft, SHOW_MESSAGES_TEMPLATE);
		cmdq_print(item, "%s", s);
		free(s);
	}
	format_free(ft);

	return (CMD_RETURN_NORMAL);
}
