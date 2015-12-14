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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Asks for confirmation before executing a command.
 */

enum cmd_retval	 cmd_confirm_before_exec(struct cmd *, struct cmd_q *);

int		 cmd_confirm_before_callback(void *, const char *);
void		 cmd_confirm_before_free(void *);

const struct cmd_entry cmd_confirm_before_entry = {
	.name = "confirm-before",
	.alias = "confirm",

	.args = { "p:t:", 1, 1 },
	.usage = "[-p prompt] " CMD_TARGET_CLIENT_USAGE " command",

	.tflag = CMD_CLIENT,

	.flags = 0,
	.exec = cmd_confirm_before_exec
};

struct cmd_confirm_before_data {
	char		*cmd;
	struct client	*client;
};

enum cmd_retval
cmd_confirm_before_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args			*args = self->args;
	struct cmd_confirm_before_data	*cdata;
	struct client			*c = cmdq->state.c;
	char				*cmd, *copy, *new_prompt, *ptr;
	const char			*prompt;

	if ((prompt = args_get(args, 'p')) != NULL)
		xasprintf(&new_prompt, "%s ", prompt);
	else {
		ptr = copy = xstrdup(args->argv[0]);
		cmd = strsep(&ptr, " \t");
		xasprintf(&new_prompt, "Confirm '%s'? (y/n) ", cmd);
		free(copy);
	}

	cdata = xmalloc(sizeof *cdata);
	cdata->cmd = xstrdup(args->argv[0]);

	cdata->client = c;
	cdata->client->references++;

	status_prompt_set(c, new_prompt, NULL,
	    cmd_confirm_before_callback, cmd_confirm_before_free, cdata,
	    PROMPT_SINGLE);

	free(new_prompt);
	return (CMD_RETURN_NORMAL);
}

int
cmd_confirm_before_callback(void *data, const char *s)
{
	struct cmd_confirm_before_data	*cdata = data;
	struct client			*c = cdata->client;
	struct cmd_list			*cmdlist;
	char				*cause;

	if (c->flags & CLIENT_DEAD)
		return (0);

	if (s == NULL || *s == '\0')
		return (0);
	if (tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		return (0);

	if (cmd_string_parse(cdata->cmd, &cmdlist, NULL, 0, &cause) != 0) {
		if (cause != NULL) {
			cmdq_error(c->cmdq, "%s", cause);
			free(cause);
		}
		return (0);
	}

	cmdq_run(c->cmdq, cmdlist, NULL);
	cmd_list_free(cmdlist);

	return (0);
}

void
cmd_confirm_before_free(void *data)
{
	struct cmd_confirm_before_data	*cdata = data;
	struct client			*c = cdata->client;

	server_client_unref(c);

	free(cdata->cmd);
	free(cdata);
}
