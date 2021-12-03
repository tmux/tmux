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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Executes a tmux command if a shell command returns true or false.
 */

static enum args_parse_type	cmd_if_shell_args_parse(struct args *, u_int,
				    char **);
static enum cmd_retval		cmd_if_shell_exec(struct cmd *,
				    struct cmdq_item *);

static void	cmd_if_shell_callback(struct job *);
static void	cmd_if_shell_free(void *);

const struct cmd_entry cmd_if_shell_entry = {
	.name = "if-shell",
	.alias = "if",

	.args = { "bFt:", 2, 3, cmd_if_shell_args_parse },
	.usage = "[-bF] " CMD_TARGET_PANE_USAGE " shell-command command "
		 "[command]",

	.target = { 't', CMD_FIND_PANE, CMD_FIND_CANFAIL },

	.flags = 0,
	.exec = cmd_if_shell_exec
};

struct cmd_if_shell_data {
	struct args_command_state	*cmd_if;
	struct args_command_state	*cmd_else;

	struct client			*client;
	struct cmdq_item		*item;
};

static enum args_parse_type
cmd_if_shell_args_parse(__unused struct args *args, u_int idx,
    __unused char **cause)
{
	if (idx == 1 || idx == 2)
		return (ARGS_PARSE_COMMANDS_OR_STRING);
	return (ARGS_PARSE_STRING);
}

static enum cmd_retval
cmd_if_shell_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_find_state		*target = cmdq_get_target(item);
	struct cmd_if_shell_data	*cdata;
	struct cmdq_item		*new_item;
	char				*shellcmd;
	struct client			*tc = cmdq_get_target_client(item);
	struct session			*s = target->s;
	struct cmd_list			*cmdlist;
	u_int				 count = args_count(args);
	int				 wait = !args_has(args, 'b');

	shellcmd = format_single_from_target(item, args_string(args, 0));
	if (args_has(args, 'F')) {
		if (*shellcmd != '0' && *shellcmd != '\0')
			cmdlist = args_make_commands_now(self, item, 1, 0);
		else if (count == 3)
			cmdlist = args_make_commands_now(self, item, 2, 0);
		else {
			free(shellcmd);
			return (CMD_RETURN_NORMAL);
		}
		free(shellcmd);
		if (cmdlist == NULL)
			return (CMD_RETURN_ERROR);
		new_item = cmdq_get_command(cmdlist, cmdq_get_state(item));
		cmdq_insert_after(item, new_item);
		return (CMD_RETURN_NORMAL);
	}

	cdata = xcalloc(1, sizeof *cdata);

	cdata->cmd_if = args_make_commands_prepare(self, item, 1, NULL, wait,
	    0);
	if (count == 3) {
		cdata->cmd_else = args_make_commands_prepare(self, item, 2,
		    NULL, wait, 0);
	}

	if (wait) {
		cdata->client = cmdq_get_client(item);
		cdata->item = item;
	} else
		cdata->client = tc;
	if (cdata->client != NULL)
		cdata->client->references++;

	if (job_run(shellcmd, 0, NULL, NULL, s,
	    server_client_get_cwd(cmdq_get_client(item), s), NULL,
	    cmd_if_shell_callback, cmd_if_shell_free, cdata, 0, -1,
	    -1) == NULL) {
		cmdq_error(item, "failed to run command: %s", shellcmd);
		free(shellcmd);
		free(cdata);
		return (CMD_RETURN_ERROR);
	}
	free(shellcmd);

	if (!wait)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

static void
cmd_if_shell_callback(struct job *job)
{
	struct cmd_if_shell_data	*cdata = job_get_data(job);
	struct client			*c = cdata->client;
	struct cmdq_item		*item = cdata->item, *new_item;
	struct args_command_state	*state;
	struct cmd_list			*cmdlist;
	char				*error;
	int				 status;

	status = job_get_status(job);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		state = cdata->cmd_else;
	else
		state = cdata->cmd_if;
	if (state == NULL)
		goto out;

	cmdlist = args_make_commands(state, 0, NULL, &error);
	if (cmdlist == NULL) {
		if (cdata->item == NULL) {
			*error = toupper((u_char)*error);
			status_message_set(c, -1, 1, 0, "%s", error);
		} else
			cmdq_error(cdata->item, "%s", error);
		free(error);
	} else if (item == NULL) {
		new_item = cmdq_get_command(cmdlist, NULL);
		cmdq_append(c, new_item);
	} else {
		new_item = cmdq_get_command(cmdlist, cmdq_get_state(item));
		cmdq_insert_after(item, new_item);
	}

out:
	if (cdata->item != NULL)
		cmdq_continue(cdata->item);
}

static void
cmd_if_shell_free(void *data)
{
	struct cmd_if_shell_data	*cdata = data;

	if (cdata->client != NULL)
		server_client_unref(cdata->client);

	if (cdata->cmd_else != NULL)
		args_make_commands_free(cdata->cmd_else);
	args_make_commands_free(cdata->cmd_if);

	free(cdata);
}
