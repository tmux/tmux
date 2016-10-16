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

static enum cmd_retval	 cmd_if_shell_exec(struct cmd *, struct cmd_q *);

static enum cmd_retval	 cmd_if_shell_error(struct cmd_q *, void *);
static void		 cmd_if_shell_callback(struct job *);
static void		 cmd_if_shell_free(void *);

const struct cmd_entry cmd_if_shell_entry = {
	.name = "if-shell",
	.alias = "if",

	.args = { "bFt:", 2, 3 },
	.usage = "[-bF] " CMD_TARGET_PANE_USAGE " shell-command command "
		 "[command]",

	.tflag = CMD_PANE_CANFAIL,

	.flags = 0,
	.exec = cmd_if_shell_exec
};

struct cmd_if_shell_data {
	char			*file;
	u_int			 line;

	char			*cmd_if;
	char			*cmd_else;

	struct client		*client;
	struct cmd_q		*cmdq;
	struct mouse_event	 mouse;
};

static enum cmd_retval
cmd_if_shell_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args			*args = self->args;
	struct cmd_if_shell_data	*cdata;
	char				*shellcmd, *cmd, *cause;
	struct cmd_list			*cmdlist;
	struct cmd_q			*new_cmdq;
	struct session			*s = cmdq->state.tflag.s;
	struct winlink			*wl = cmdq->state.tflag.wl;
	struct window_pane		*wp = cmdq->state.tflag.wp;
	struct format_tree		*ft;
	const char			*cwd;

	if (cmdq->client != NULL && cmdq->client->session == NULL)
		cwd = cmdq->client->cwd;
	else if (s != NULL)
		cwd = s->cwd;
	else
		cwd = NULL;

	ft = format_create(cmdq, 0);
	format_defaults(ft, cmdq->state.c, s, wl, wp);
	shellcmd = format_expand(ft, args->argv[0]);
	format_free(ft);

	if (args_has(args, 'F')) {
		cmd = NULL;
		if (*shellcmd != '0' && *shellcmd != '\0')
			cmd = args->argv[1];
		else if (args->argc == 3)
			cmd = args->argv[2];
		free(shellcmd);
		if (cmd == NULL)
			return (CMD_RETURN_NORMAL);
		if (cmd_string_parse(cmd, &cmdlist, NULL, 0, &cause) != 0) {
			if (cause != NULL) {
				cmdq_error(cmdq, "%s", cause);
				free(cause);
			}
			return (CMD_RETURN_ERROR);
		}
		new_cmdq = cmdq_get_command(cmdlist, NULL, &cmdq->mouse, 0);
		cmdq_insert_after(cmdq, new_cmdq);
		cmd_list_free(cmdlist);
		return (CMD_RETURN_NORMAL);
	}

	cdata = xcalloc(1, sizeof *cdata);
	if (self->file != NULL) {
		cdata->file = xstrdup(self->file);
		cdata->line = self->line;
	}

	cdata->cmd_if = xstrdup(args->argv[1]);
	if (args->argc == 3)
		cdata->cmd_else = xstrdup(args->argv[2]);
	else
		cdata->cmd_else = NULL;

	cdata->client = cmdq->client;
	cdata->client->references++;

	if (!args_has(args, 'b'))
		cdata->cmdq = cmdq;
	else
		cdata->cmdq = NULL;
	memcpy(&cdata->mouse, &cmdq->mouse, sizeof cdata->mouse);

	job_run(shellcmd, s, cwd, cmd_if_shell_callback, cmd_if_shell_free,
	    cdata);
	free(shellcmd);

	if (args_has(args, 'b'))
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

static enum cmd_retval
cmd_if_shell_error(struct cmd_q *cmdq, void *data)
{
	char	*error = data;

	cmdq_error(cmdq, "%s", error);
	free(error);

	return (CMD_RETURN_NORMAL);
}

static void
cmd_if_shell_callback(struct job *job)
{
	struct cmd_if_shell_data	*cdata = job->data;
	struct client			*c = cdata->client;
	struct cmd_list			*cmdlist;
	struct cmd_q			*new_cmdq;
	char				*cause, *cmd, *file = cdata->file;
	u_int				 line = cdata->line;

	if (!WIFEXITED(job->status) || WEXITSTATUS(job->status) != 0)
		cmd = cdata->cmd_else;
	else
		cmd = cdata->cmd_if;
	if (cmd == NULL)
		goto out;

	if (cmd_string_parse(cmd, &cmdlist, file, line, &cause) != 0) {
		if (cause != NULL)
			new_cmdq = cmdq_get_callback(cmd_if_shell_error, cause);
		else
			new_cmdq = NULL;
	} else {
		new_cmdq = cmdq_get_command(cmdlist, NULL, &cdata->mouse, 0);
		cmd_list_free(cmdlist);
	}

	if (new_cmdq != NULL) {
		if (cdata->cmdq == NULL)
			cmdq_append(c, new_cmdq);
		else
			cmdq_insert_after(cdata->cmdq, new_cmdq);
	}

out:
	if (cdata->cmdq != NULL)
		cdata->cmdq->flags &= ~CMD_Q_WAITING;
}

static void
cmd_if_shell_free(void *data)
{
	struct cmd_if_shell_data	*cdata = data;

	server_client_unref(cdata->client);

	free(cdata->cmd_else);
	free(cdata->cmd_if);

	free(cdata->file);
	free(cdata);
}
