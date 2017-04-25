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

static enum cmd_retval	cmd_if_shell_exec(struct cmd *, struct cmdq_item *);

static void		cmd_if_shell_callback(struct job *);
static void		cmd_if_shell_free(void *);

const struct cmd_entry cmd_if_shell_entry = {
	.name = "if-shell",
	.alias = "if",

	.args = { "bFt:", 2, 3 },
	.usage = "[-bF] " CMD_TARGET_PANE_USAGE " shell-command command "
		 "[command]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = 0,
	.exec = cmd_if_shell_exec
};

struct cmd_if_shell_data {
	char			*file;
	u_int			 line;

	char			*cmd_if;
	char			*cmd_else;

	struct client		*client;
	struct cmdq_item	*item;
	struct mouse_event	 mouse;
};

static enum cmd_retval
cmd_if_shell_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = self->args;
	struct cmdq_shared		*shared = item->shared;
	struct cmd_if_shell_data	*cdata;
	char				*shellcmd, *cmd, *cause;
	struct cmd_list			*cmdlist;
	struct cmdq_item		*new_item;
	struct client			*c = cmd_find_client(item, NULL, 1);
	struct session			*s = item->target.s;
	struct winlink			*wl = item->target.wl;
	struct window_pane		*wp = item->target.wp;
	const char			*cwd;

	if (item->client != NULL && item->client->session == NULL)
		cwd = item->client->cwd;
	else if (s != NULL)
		cwd = s->cwd;
	else
		cwd = NULL;

	shellcmd = format_single(item, args->argv[0], c, s, wl, wp);
	if (args_has(args, 'F')) {
		cmd = NULL;
		if (*shellcmd != '0' && *shellcmd != '\0')
			cmd = args->argv[1];
		else if (args->argc == 3)
			cmd = args->argv[2];
		free(shellcmd);
		if (cmd == NULL)
			return (CMD_RETURN_NORMAL);
		cmdlist = cmd_string_parse(cmd, NULL, 0, &cause);
		if (cmdlist == NULL) {
			if (cause != NULL) {
				cmdq_error(item, "%s", cause);
				free(cause);
			}
			return (CMD_RETURN_ERROR);
		}
		new_item = cmdq_get_command(cmdlist, NULL, &shared->mouse, 0);
		cmdq_insert_after(item, new_item);
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

	cdata->client = item->client;
	if (cdata->client != NULL)
		cdata->client->references++;

	if (!args_has(args, 'b'))
		cdata->item = item;
	else
		cdata->item = NULL;
	memcpy(&cdata->mouse, &shared->mouse, sizeof cdata->mouse);

	job_run(shellcmd, s, cwd, NULL, cmd_if_shell_callback,
	    cmd_if_shell_free, cdata);
	free(shellcmd);

	if (args_has(args, 'b'))
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

static void
cmd_if_shell_callback(struct job *job)
{
	struct cmd_if_shell_data	*cdata = job->data;
	struct client			*c = cdata->client;
	struct cmd_list			*cmdlist;
	struct cmdq_item		*new_item;
	char				*cause, *cmd, *file = cdata->file;
	u_int				 line = cdata->line;

	if (!WIFEXITED(job->status) || WEXITSTATUS(job->status) != 0)
		cmd = cdata->cmd_else;
	else
		cmd = cdata->cmd_if;
	if (cmd == NULL)
		goto out;

	cmdlist = cmd_string_parse(cmd, file, line, &cause);
	if (cmdlist == NULL) {
		if (cause != NULL && cdata->item != NULL)
			cmdq_error(cdata->item, "%s", cause);
		free(cause);
		new_item = NULL;
	} else {
		new_item = cmdq_get_command(cmdlist, NULL, &cdata->mouse, 0);
		cmd_list_free(cmdlist);
	}

	if (new_item != NULL) {
		if (cdata->item == NULL)
			cmdq_append(c, new_item);
		else
			cmdq_insert_after(cdata->item, new_item);
	}

out:
	if (cdata->item != NULL)
		cdata->item->flags &= ~CMDQ_WAITING;
}

static void
cmd_if_shell_free(void *data)
{
	struct cmd_if_shell_data	*cdata = data;

	if (cdata->client != NULL)
		server_client_unref(cdata->client);

	free(cdata->cmd_else);
	free(cdata->cmd_if);

	free(cdata->file);
	free(cdata);
}
