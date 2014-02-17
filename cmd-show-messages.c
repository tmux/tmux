/* $Id$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

enum cmd_retval	 cmd_show_messages_exec(struct cmd *, struct cmd_q *);

const struct cmd_entry cmd_show_messages_entry = {
	"show-messages", "showmsgs",
	"IJTt:", 0, 0,
	"[-IJT] " CMD_TARGET_CLIENT_USAGE,
	0,
	NULL,
	cmd_show_messages_exec
};

const struct cmd_entry cmd_server_info_entry = {
	"server-info", "info",
	"", 0, 0,
	"",
	0,
	NULL,
	cmd_show_messages_exec
};

void	cmd_show_messages_server(struct cmd_q *);
void	cmd_show_messages_terminals(struct cmd_q *);
void	cmd_show_messages_jobs(struct cmd_q *);

void
cmd_show_messages_server(struct cmd_q *cmdq)
{
	char	*tim;

	tim = ctime(&start_time);
	*strchr(tim, '\n') = '\0';

	cmdq_print(cmdq, "started %s", tim);
	cmdq_print(cmdq, "socket path %s", socket_path);
	cmdq_print(cmdq, "debug level %d", debug_level);
	cmdq_print(cmdq, "protocol version %d", PROTOCOL_VERSION);
}

void
cmd_show_messages_terminals(struct cmd_q *cmdq)
{
	struct tty_term				*term;
	const struct tty_term_code_entry	*ent;
	struct tty_code				*code;
	u_int					 i, n;
	char					 out[80];

	n = 0;
	LIST_FOREACH(term, &tty_terms, entry) {
		cmdq_print(cmdq,
		    "Terminal %u: %s [references=%u, flags=0x%x]:",
		    n, term->name, term->references, term->flags);
		n++;
		for (i = 0; i < NTTYCODE; i++) {
			ent = &tty_term_codes[i];
			code = &term->codes[ent->code];
			switch (code->type) {
			case TTYCODE_NONE:
				cmdq_print(cmdq, "%4u: %s: [missing]",
				    ent->code, ent->name);
				break;
			case TTYCODE_STRING:
				strnvis(out, code->value.string, sizeof out,
				    VIS_OCTAL|VIS_TAB|VIS_NL);
				cmdq_print(cmdq, "%4u: %s: (string) %s",
				    ent->code, ent->name, out);
				break;
			case TTYCODE_NUMBER:
				cmdq_print(cmdq, "%4u: %s: (number) %d",
				    ent->code, ent->name, code->value.number);
				break;
			case TTYCODE_FLAG:
				cmdq_print(cmdq, "%4u: %s: (flag) %s",
				    ent->code, ent->name,
				    code->value.flag ? "true" : "false");
				break;
			}
		}
	}
}

void
cmd_show_messages_jobs(struct cmd_q *cmdq)
{
	struct job	*job;
	u_int		 n;

	n = 0;
	LIST_FOREACH(job, &all_jobs, lentry) {
		cmdq_print(cmdq,
		    "Job %u: %s [fd=%d, pid=%d, status=%d]",
		    n, job->cmd, job->fd, job->pid, job->status);
		n++;
	}
}

enum cmd_retval
cmd_show_messages_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args		*args = self->args;
	struct client		*c;
	struct message_entry	*msg;
	char			*tim;
	u_int			 i;
	int			 done;

	done = 0;
	if (args_has(args, 'I') || self->entry == &cmd_server_info_entry) {
		cmd_show_messages_server(cmdq);
		done = 1;
	}
	if (args_has(args, 'T') || self->entry == &cmd_server_info_entry) {
		if (done)
			cmdq_print(cmdq, "%s", "");
		cmd_show_messages_terminals(cmdq);
		done = 1;
	}
	if (args_has(args, 'J') || self->entry == &cmd_server_info_entry) {
		if (done)
			cmdq_print(cmdq, "%s", "");
		cmd_show_messages_jobs(cmdq);
		done = 1;
	}
	if (done)
		return (CMD_RETURN_NORMAL);

	if ((c = cmd_find_client(cmdq, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

	for (i = 0; i < ARRAY_LENGTH(&c->message_log); i++) {
		msg = &ARRAY_ITEM(&c->message_log, i);

		tim = ctime(&msg->msg_time);
		*strchr(tim, '\n') = '\0';

		cmdq_print(cmdq, "%s %s", tim, msg->msg);
	}

	return (CMD_RETURN_NORMAL);
}
