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

	.args = { "bp:t:", 1, 1 },
	.usage = "[-b] [-p prompt] " CMD_TARGET_CLIENT_USAGE " command",

	.flags = CMD_CLIENT_TFLAG,
	.exec = cmd_confirm_before_exec
};

struct cmd_confirm_before_data {
	char			*cmd;
	struct cmdq_item	*item;
	struct cmd_parse_input	 pi;
};

static enum cmd_retval
cmd_confirm_before_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct cmd_confirm_before_data	*cdata;
	struct client			*tc = cmdq_get_target_client(item);
	struct cmd_find_state		*target = cmdq_get_target(item);
	char				*cmd, *copy, *new_prompt, *ptr;
	const char			*prompt;
	int				 wait = !args_has(args, 'b');

	if ((prompt = args_get(args, 'p')) != NULL)
		xasprintf(&new_prompt, "%s ", prompt);
	else {
		ptr = copy = xstrdup(args->argv[0]);
		cmd = strsep(&ptr, " \t");
		xasprintf(&new_prompt, "Confirm '%s'? (y/n) ", cmd);
		free(copy);
	}

	cdata = xcalloc(1, sizeof *cdata);
	cdata->cmd = xstrdup(args->argv[0]);

	cmd_get_source(self, &cdata->pi.file, &cdata->pi.line);
	if (wait)
		cdata->pi.item = item;
	cdata->pi.c = tc;
	cmd_find_copy_state(&cdata->pi.fs, target);

	if (wait)
		cdata->item = item;

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
	const char			*cmd = cdata->cmd;
	char				*error;
	struct cmdq_item		*item = cdata->item;
	enum cmd_parse_status		 status;
	int				 retcode = 1;

	if (c->flags & CLIENT_DEAD)
		goto out;

	if (s == NULL || *s == '\0')
		goto out;
	if (tolower((u_char)s[0]) != 'y' || s[1] != '\0')
		goto out;
	retcode = 0;

	if (item != NULL) {
		status = cmd_parse_and_insert(cmd, &cdata->pi, item,
		    cmdq_get_state(item), &error);
	} else
		status = cmd_parse_and_append(cmd, &cdata->pi, c, NULL, &error);
	if (status == CMD_PARSE_ERROR) {
		cmdq_append(c, cmdq_get_error(error));
		free(error);
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

	free(cdata->cmd);
	free(cdata);
}
