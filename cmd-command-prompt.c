/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <time.h>

#include "tmux.h"

/*
 * Prompt for command in client.
 */

static enum cmd_retval	cmd_command_prompt_exec(struct cmd *,
			    struct cmdq_item *);

static int	cmd_command_prompt_callback(void *, const char *);
static void	cmd_command_prompt_free(void *);

const struct cmd_entry cmd_command_prompt_entry = {
	.name = "command-prompt",
	.alias = NULL,

	.args = { "1I:Np:t:", 0, 1 },
	.usage = "[-1N] [-I inputs] [-p prompts] " CMD_TARGET_CLIENT_USAGE " "
		 "[template]",

	.tflag = CMD_CLIENT,

	.flags = 0,
	.exec = cmd_command_prompt_exec
};

struct cmd_command_prompt_cdata {
	struct client	*c;
	char		*inputs;
	char		*next_input;
	char		*next_prompt;
	char		*prompts;
	char		*template;
	int		 idx;
};

static enum cmd_retval
cmd_command_prompt_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = self->args;
	const char			*inputs, *prompts;
	struct cmd_command_prompt_cdata	*cdata;
	struct client			*c = item->state.c;
	char				*prompt, *ptr, *input = NULL;
	size_t				 n;
	int				 flags;

	if (c->prompt_string != NULL)
		return (CMD_RETURN_NORMAL);

	cdata = xmalloc(sizeof *cdata);
	cdata->c = c;
	cdata->idx = 1;
	cdata->inputs = NULL;
	cdata->next_input = NULL;
	cdata->next_prompt = NULL;
	cdata->prompts = NULL;
	cdata->template = NULL;

	if (args->argc != 0)
		cdata->template = xstrdup(args->argv[0]);
	else
		cdata->template = xstrdup("%1");

	if ((prompts = args_get(args, 'p')) != NULL)
		cdata->prompts = xstrdup(prompts);
	else if (args->argc != 0) {
		n = strcspn(cdata->template, " ,");
		xasprintf(&cdata->prompts, "(%.*s) ", (int) n, cdata->template);
	} else
		cdata->prompts = xstrdup(":");

	/* Get first prompt. */
	cdata->next_prompt = cdata->prompts;
	ptr = strsep(&cdata->next_prompt, ",");
	if (prompts == NULL)
		prompt = xstrdup(ptr);
	else
		xasprintf(&prompt, "%s ", ptr);

	/* Get initial prompt input. */
	if ((inputs = args_get(args, 'I')) != NULL) {
		cdata->inputs = xstrdup(inputs);
		cdata->next_input = cdata->inputs;
		input = strsep(&cdata->next_input, ",");
	}

	flags = 0;
	if (args_has(args, '1'))
		flags |= PROMPT_SINGLE;
	else if (args_has(args, 'N'))
		flags |= PROMPT_NUMERIC;
	status_prompt_set(c, prompt, input, cmd_command_prompt_callback,
	    cmd_command_prompt_free, cdata, flags);
	free(prompt);

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_command_prompt_error(struct cmdq_item *item, void *data)
{
	char	*error = data;

	cmdq_error(item, "%s", error);
	free(error);

	return (CMD_RETURN_NORMAL);
}

static int
cmd_command_prompt_callback(void *data, const char *s)
{
	struct cmd_command_prompt_cdata	*cdata = data;
	struct client			*c = cdata->c;
	struct cmd_list			*cmdlist;
	struct cmdq_item		*new_item;
	char				*cause, *new_template, *prompt, *ptr;
	char				*input = NULL;

	if (s == NULL)
		return (0);

	new_template = cmd_template_replace(cdata->template, s, cdata->idx);
	free(cdata->template);
	cdata->template = new_template;

	/*
	 * Check if there are more prompts; if so, get its respective input
	 * and update the prompt data.
	 */
	if ((ptr = strsep(&cdata->next_prompt, ",")) != NULL) {
		xasprintf(&prompt, "%s ", ptr);
		input = strsep(&cdata->next_input, ",");
		status_prompt_update(c, prompt, input);

		free(prompt);
		cdata->idx++;
		return (1);
	}

	if (cmd_string_parse(new_template, &cmdlist, NULL, 0, &cause) != 0) {
		if (cause != NULL) {
			new_item = cmdq_get_callback(cmd_command_prompt_error,
			    cause);
		} else
			new_item = NULL;
	} else {
		new_item = cmdq_get_command(cmdlist, NULL, NULL, 0);
		cmd_list_free(cmdlist);
	}

	if (new_item != NULL)
		cmdq_append(c, new_item);

	if (c->prompt_callbackfn != (void *)&cmd_command_prompt_callback)
		return (1);
	return (0);
}

static void
cmd_command_prompt_free(void *data)
{
	struct cmd_command_prompt_cdata	*cdata = data;

	free(cdata->inputs);
	free(cdata->prompts);
	free(cdata->template);
	free(cdata);
}
