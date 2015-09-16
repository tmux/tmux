/* $OpenBSD$ */

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
	"bFt:", 2, 3,
	"[-bF] " CMD_TARGET_PANE_USAGE " shell-command command [command]",
	0,
	cmd_if_shell_exec
};

struct cmd_if_shell_data {
	char			*cmd_if;
	char			*cmd_else;

	struct cmd_q		*cmdq;
	struct mouse_event	 mouse;

	int			 bflag;
	int			 references;
};

enum cmd_retval
cmd_if_shell_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args			*args = self->args;
	struct cmd_if_shell_data	*cdata;
	char				*shellcmd, *cmd, *cause;
	struct cmd_list			*cmdlist;
	struct client			*c;
	struct session			*s = NULL;
	struct winlink			*wl = NULL;
	struct window_pane		*wp = NULL;
	struct format_tree		*ft;
	int				 cwd;

	if (args_has(args, 't')) {
		wl = cmd_find_pane(cmdq, args_get(args, 't'), &s, &wp);
		cwd = wp->cwd;
	} else {
		c = cmd_find_client(cmdq, NULL, 1);
		if (c != NULL && c->session != NULL) {
			s = c->session;
			wl = s->curw;
			wp = wl->window->active;
		}
		if (cmdq->client != NULL && cmdq->client->session == NULL)
			cwd = cmdq->client->cwd;
		else if (s != NULL)
			cwd = s->cwd;
		else
			cwd = -1;
	}

	ft = format_create();
	format_defaults(ft, NULL, s, wl, wp);
	shellcmd = format_expand(ft, args->argv[0]);
	format_free(ft);

	if (args_has(args, 'F')) {
		cmd = NULL;
		if (*shellcmd != '0' && *shellcmd != '\0')
			cmd = args->argv[1];
		else if (args->argc == 3)
			cmd = args->argv[2];
		if (cmd == NULL)
			return (CMD_RETURN_NORMAL);
		if (cmd_string_parse(cmd, &cmdlist, NULL, 0, &cause) != 0) {
			if (cause != NULL) {
				cmdq_error(cmdq, "%s", cause);
				free(cause);
			}
			return (CMD_RETURN_ERROR);
		}
		cmdq_run(cmdq, cmdlist, &cmdq->item->mouse);
		cmd_list_free(cmdlist);
		return (CMD_RETURN_NORMAL);
	}

	cdata = xmalloc(sizeof *cdata);

	cdata->cmd_if = xstrdup(args->argv[1]);
	if (args->argc == 3)
		cdata->cmd_else = xstrdup(args->argv[2]);
	else
		cdata->cmd_else = NULL;

	cdata->bflag = args_has(args, 'b');

	cdata->cmdq = cmdq;
	memcpy(&cdata->mouse, &cmdq->item->mouse, sizeof cdata->mouse);
	cmdq->references++;

	cdata->references = 1;
	job_run(shellcmd, s, cwd, cmd_if_shell_callback, cmd_if_shell_free,
	    cdata);
	free(shellcmd);

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

	if (cmdq->flags & CMD_Q_DEAD)
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

	cmdq1 = cmdq_new(cmdq->client);
	cmdq1->emptyfn = cmd_if_shell_done;
	cmdq1->data = cdata;

	cdata->references++;
	cmdq_run(cmdq1, cmdlist, &cdata->mouse);
	cmd_list_free(cmdlist);
}

void
cmd_if_shell_done(struct cmd_q *cmdq1)
{
	struct cmd_if_shell_data	*cdata = cmdq1->data;
	struct cmd_q			*cmdq = cdata->cmdq;

	if (cmdq1->client_exit >= 0)
		cmdq->client_exit = cmdq1->client_exit;
	cmdq_free(cmdq1);

	if (--cdata->references != 0)
		return;

	if (!cmdq_free(cmdq) && !cdata->bflag)
		cmdq_continue(cmdq);

	free(cdata->cmd_else);
	free(cdata->cmd_if);
	free(cdata);
}

void
cmd_if_shell_free(void *data)
{
	struct cmd_if_shell_data	*cdata = data;
	struct cmd_q			*cmdq = cdata->cmdq;

	if (--cdata->references != 0)
		return;

	if (!cmdq_free(cmdq) && !cdata->bflag)
		cmdq_continue(cmdq);

	free(cdata->cmd_else);
	free(cdata->cmd_if);
	free(cdata);
}
