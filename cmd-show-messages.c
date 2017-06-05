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

#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Show client message log.
 */

static enum cmd_retval	cmd_show_messages_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_show_messages_entry = {
	.name = "show-messages",
	.alias = "showmsgs",

	.args = { "JTt:", 0, 0 },
	.usage = "[-JT] " CMD_TARGET_CLIENT_USAGE,

	.flags = CMD_AFTERHOOK,
	.exec = cmd_show_messages_exec
};

static int	cmd_show_messages_terminals(struct cmdq_item *, int);
static int	cmd_show_messages_jobs(struct cmdq_item *, int);

static int
cmd_show_messages_terminals(struct cmdq_item *item, int blank)
{
	struct tty_term	*term;
	u_int		 i, n;

	n = 0;
	LIST_FOREACH(term, &tty_terms, entry) {
		if (blank) {
			cmdq_print(item, "%s", "");
			blank = 0;
		}
		cmdq_print(item, "Terminal %u: %s [references=%u, flags=0x%x]:",
		    n, term->name, term->references, term->flags);
		n++;
		for (i = 0; i < tty_term_ncodes(); i++)
			cmdq_print(item, "%s", tty_term_describe(term, i));
	}
	return (n != 0);
}

static int
cmd_show_messages_jobs(struct cmdq_item *item, int blank)
{
	struct job	*job;
	u_int		 n;

	n = 0;
	LIST_FOREACH(job, &all_jobs, entry) {
		if (blank) {
			cmdq_print(item, "%s", "");
			blank = 0;
		}
		cmdq_print(item, "Job %u: %s [fd=%d, pid=%ld, status=%d]",
		    n, job->cmd, job->fd, (long)job->pid, job->status);
		n++;
	}
	return (n != 0);
}

static enum cmd_retval
cmd_show_messages_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = self->args;
	struct client		*c;
	struct message_entry	*msg;
	char			*tim;
	int			 done, blank;

	if ((c = cmd_find_client(item, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

	done = blank = 0;
	if (args_has(args, 'T')) {
		blank = cmd_show_messages_terminals(item, blank);
		done = 1;
	}
	if (args_has(args, 'J')) {
		cmd_show_messages_jobs(item, blank);
		done = 1;
	}
	if (done)
		return (CMD_RETURN_NORMAL);

	TAILQ_FOREACH(msg, &c->message_log, entry) {
		tim = ctime(&msg->msg_time);
		*strchr(tim, '\n') = '\0';

		cmdq_print(item, "%s %s", tim, msg->msg);
	}

	return (CMD_RETURN_NORMAL);
}
