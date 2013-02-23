/* $Id$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
 * Copyright (c) 2009 Nicholas Marriott <nicm@openbsd.org>
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
#include <sys/wait.h>

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Executes a tmux command if a shell command returns true or false.
 */

enum cmd_retval	 cmd_if_shell_exec(struct cmd *, struct cmd_q *);

void	cmd_if_shell_callback(struct job *);
void	cmd_if_shell_done(struct cmd_q *);
void	cmd_if_shell_free(void *);

const struct cmd_entry cmd_if_shell_entry = {
	"if-shell", "if",
	"b", 2, 3,
	"[-b] shell-command command [command]",
	0,
	NULL,
	NULL,
	cmd_if_shell_exec
};

struct cmd_if_shell_data {
	char		*cmd_if;
	char		*cmd_else;
	struct cmd_q	*cmdq;
	int		 bflag;
	int		 started;
};

enum cmd_retval
cmd_if_shell_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args			*args = self->args;
	struct cmd_if_shell_data	*cdata;
	const char			*shellcmd = args->argv[0];

	cdata = xmalloc(sizeof *cdata);
	cdata->cmd_if = xstrdup(args->argv[1]);
	if (args->argc == 3)
		cdata->cmd_else = xstrdup(args->argv[2]);
	else
		cdata->cmd_else = NULL;
	cdata->bflag = args_has(args, 'b');

	cdata->started = 0;
	cdata->cmdq = cmdq;
	cmdq->references++;

	job_run(shellcmd, cmd_if_shell_callback, cmd_if_shell_free, cdata);

	if (cdata->bflag)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

void
cmd_if_shell_callback(struct job *job)
{
	struct cmd_if_shell_data	*cdata = job->data;
	struct cmd_q			*cmdq = cdata->cmdq, *cmdq1;
	struct cmd_list			*cmdlist;
	char				*cause, *cmd;

	if (cmdq->dead)
		return;

	if (!WIFEXITED(job->status) || WEXITSTATUS(job->status) != 0)
		cmd = cdata->cmd_else;
	else
		cmd = cdata->cmd_if;
	if (cmd == NULL)
		return;

	if (cmd_string_parse(cmd, &cmdlist, NULL, 0, &cause) != 0) {
		if (cause != NULL) {
			cmdq_error(cmdq, "%s", cause);
			free(cause);
		}
		return;
	}

	cdata->started = 1;

	cmdq1 = cmdq_new(cmdq->client);
	cmdq1->emptyfn = cmd_if_shell_done;
	cmdq1->data = cdata;

	cmdq_run(cmdq1, cmdlist);
	cmd_list_free(cmdlist);
}

void
cmd_if_shell_done(struct cmd_q *cmdq1)
{
	struct cmd_if_shell_data	*cdata = cmdq1->data;
	struct cmd_q			*cmdq = cdata->cmdq;

	if (!cmdq_free(cmdq) && !cdata->bflag)
		cmdq_continue(cmdq);

	cmdq_free(cmdq1);

	free(cdata->cmd_else);
	free(cdata->cmd_if);
	free(cdata);
}

void
cmd_if_shell_free(void *data)
{
	struct cmd_if_shell_data	*cdata = data;
	struct cmd_q			*cmdq = cdata->cmdq;

	if (cdata->started)
		return;

	if (!cmdq_free(cmdq) && !cdata->bflag)
		cmdq_continue(cmdq);

	free(cdata->cmd_else);
	free(cdata->cmd_if);
	free(cdata);
}
