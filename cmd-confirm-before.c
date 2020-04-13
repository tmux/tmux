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

static enum cmd_retval	cmd_confirm_before_exec(struct cmd *,
			    struct cmdq_item *);

static int	cmd_confirm_before_callback(struct client *, void *,
		    const char *, int);
static void	cmd_confirm_before_free(void *);

const struct cmd_entry cmd_confirm_before_entry = {
	.name = "confirm-before",
	.alias = "confirm",

	.args = { "p:t:", 1, 1 },
	.usage = "[-p prompt] " CMD_TARGET_CLIENT_USAGE " command",

	.flags = 0,
	.exec = cmd_confirm_before_exec
};

struct cmd_confirm_before_data {
	char	*cmd;
};

static enum cmd_retval
cmd_confirm_before_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_confirm_before_data	*cdata;
	struct client			*c;
	char				*cmd, *copy, *new_prompt, *ptr;
	const char			*prompt;

	if ((c = cmd_find_client(item, args_get(args, 't'), 0)) == NULL)
		return (CMD_RETURN_ERROR);

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

	status_prompt_set(c, new_prompt, NULL,
	    cmd_confirm_before_callback, cmd_confirm_before_free, cdata,
	    PROMPT_SINGLE);

	free(new_prompt);
	return (CMD_RETURN_NORMAL);
}

static int
cmd_confirm_before_callback(struct client *c, void *data, const char *s,
    __unused int done)
{
	struct cmd_confirm_before_data	*cdata = data;
	struct cmdq_item		*new_item;
	struct cmd_parse_result		*pr;

	if (c->flags & CLIENT_DEAD)
		return (0);

	if (s == NULL || *s == '\0')
		return (0);
	if (tolower((u_char)s[0]) != 'y' || s[1] != '\0')
		return (0);

	pr = cmd_parse_from_string(cdata->cmd, NULL);
	switch (pr->status) {
	case CMD_PARSE_EMPTY:
		new_item = NULL;
		break;
	case CMD_PARSE_ERROR:
		new_item = cmdq_get_error(pr->error);
		free(pr->error);
		cmdq_append(c, new_item);
		break;
	case CMD_PARSE_SUCCESS:
		new_item = cmdq_get_command(pr->cmdlist, NULL);
		cmd_list_free(pr->cmdlist);
		cmdq_append(c, new_item);
		break;
	}

	return (0);
}

static void
cmd_confirm_before_free(void *data)
{
	struct cmd_confirm_before_data	*cdata = data;

	free(cdata->cmd);
	free(cdata);
}
