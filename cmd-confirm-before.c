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

static enum args_parse_type	cmd_confirm_before_args_parse(struct args *,
				    u_int, char **);
static enum cmd_retval		cmd_confirm_before_exec(struct cmd *,
				    struct cmdq_item *);

static int	cmd_confirm_before_callback(struct client *, void *,
		    const char *, int);
static void	cmd_confirm_before_free(void *);

const struct cmd_entry cmd_confirm_before_entry = {
	.name = "confirm-before",
	.alias = "confirm",

	.args = { "bc:p:t:y", 1, 1, cmd_confirm_before_args_parse },
	.usage = "[-by] [-c confirm_key] [-p prompt] " CMD_TARGET_CLIENT_USAGE
		 " command",

	.flags = CMD_CLIENT_TFLAG,
	.exec = cmd_confirm_before_exec
};

struct cmd_confirm_before_data {
	struct cmdq_item	*item;
	struct cmd_list		*cmdlist;
	u_char			 confirm_key;
	int			 default_yes;
};

static enum args_parse_type
cmd_confirm_before_args_parse(__unused struct args *args, __unused u_int idx,
    __unused char **cause)
{
	return (ARGS_PARSE_COMMANDS_OR_STRING);
}

static enum cmd_retval
cmd_confirm_before_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_confirm_before_data	*cdata;
	struct client			*tc = cmdq_get_target_client(item);
	struct cmd_find_state		*target = cmdq_get_target(item);
	char				*new_prompt;
	const char			*confirm_key, *prompt, *cmd;
	int				 wait = !args_has(args, 'b');

	cdata = xcalloc(1, sizeof *cdata);
	cdata->cmdlist = args_make_commands_now(self, item, 0, 1);
	if (cdata->cmdlist == NULL) {
		free(cdata);
		return (CMD_RETURN_ERROR);
	}

	if (wait)
		cdata->item = item;

	cdata->default_yes = args_has(args, 'y');
	if ((confirm_key = args_get(args, 'c')) != NULL) {
		if (confirm_key[1] == '\0' &&
		    confirm_key[0] > 31 &&
		    confirm_key[0] < 127)
			cdata->confirm_key = confirm_key[0];
		else {
			cmdq_error(item, "invalid confirm key");
			free(cdata);
			return (CMD_RETURN_ERROR);
		}
	}
	else
		cdata->confirm_key = 'y';

	if ((prompt = args_get(args, 'p')) != NULL)
		xasprintf(&new_prompt, "%s ", prompt);
	else {
		cmd = cmd_get_entry(cmd_list_first(cdata->cmdlist))->name;
		xasprintf(&new_prompt, "Confirm '%s'? (%c/n) ", cmd,
		    cdata->confirm_key);
	}

	status_prompt_set(tc, target, new_prompt, NULL,
	    cmd_confirm_before_callback, cmd_confirm_before_free, cdata,
	    PROMPT_SINGLE, PROMPT_TYPE_COMMAND);
	free(new_prompt);

	if (!wait)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

static int
cmd_confirm_before_callback(struct client *c, void *data, const char *s,
    __unused int done)
{
	struct cmd_confirm_before_data	*cdata = data;
	struct cmdq_item		*item = cdata->item, *new_item;
	int				 retcode = 1;

	if (c->flags & CLIENT_DEAD)
		goto out;

	if (s == NULL)
		goto out;
	if (s[0] != cdata->confirm_key && (s[0] != '\r' || !cdata->default_yes))
		goto out;
	retcode = 0;

	if (item == NULL) {
		new_item = cmdq_get_command(cdata->cmdlist, NULL);
		cmdq_append(c, new_item);
	} else {
		new_item = cmdq_get_command(cdata->cmdlist,
		    cmdq_get_state(item));
		cmdq_insert_after(item, new_item);
	}

out:
	if (item != NULL) {
		if (cmdq_get_client(item) != NULL &&
		    cmdq_get_client(item)->session == NULL)
			cmdq_get_client(item)->retval = retcode;
		cmdq_continue(item);
	}
	return (0);
}

static void
cmd_confirm_before_free(void *data)
{
	struct cmd_confirm_before_data	*cdata = data;

	cmd_list_free(cdata->cmdlist);
	free(cdata);
}
