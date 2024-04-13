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

static enum args_parse_type	cmd_command_prompt_args_parse(struct args *,
				    u_int, char **);
static enum cmd_retval		cmd_command_prompt_exec(struct cmd *,
				    struct cmdq_item *);

static int	cmd_command_prompt_callback(struct client *, void *,
		    const char *, int);
static void	cmd_command_prompt_free(void *);

const struct cmd_entry cmd_command_prompt_entry = {
	.name = "command-prompt",
	.alias = NULL,

	.args = { "1bFkiI:Np:t:T:", 0, 1, cmd_command_prompt_args_parse },
	.usage = "[-1bFkiN] [-I inputs] [-p prompts] " CMD_TARGET_CLIENT_USAGE
		 " [-T type] [template]",

	.flags = CMD_CLIENT_TFLAG,
	.exec = cmd_command_prompt_exec
};

struct cmd_command_prompt_prompt {
	char	*input;
	char	*prompt;
};

struct cmd_command_prompt_cdata {
	struct cmdq_item		 *item;
	struct args_command_state	 *state;

	int				  flags;
	enum prompt_type		  prompt_type;

	struct cmd_command_prompt_prompt *prompts;
	u_int				  count;
	u_int				  current;

	int				  argc;
	char				**argv;
};

static enum args_parse_type
cmd_command_prompt_args_parse(__unused struct args *args, __unused u_int idx,
    __unused char **cause)
{
	return (ARGS_PARSE_COMMANDS_OR_STRING);
}

static enum cmd_retval
cmd_command_prompt_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args			*args = cmd_get_args(self);
	struct client			*tc = cmdq_get_target_client(item);
	struct cmd_find_state		*target = cmdq_get_target(item);
	const char			*type, *s, *input;
	struct cmd_command_prompt_cdata	*cdata;
	char				*tmp, *prompts, *prompt, *next_prompt;
	char				*inputs = NULL, *next_input;
	u_int				 count = args_count(args);
	int				 wait = !args_has(args, 'b'), space = 1;

	if (tc->prompt_string != NULL)
		return (CMD_RETURN_NORMAL);
	if (args_has(args, 'i'))
		wait = 0;

	cdata = xcalloc(1, sizeof *cdata);
	if (wait)
		cdata->item = item;
	cdata->state = args_make_commands_prepare(self, item, 0, "%1", wait,
	    args_has(args, 'F'));

	if ((s = args_get(args, 'p')) == NULL) {
		if (count != 0) {
			tmp = args_make_commands_get_command(cdata->state);
			xasprintf(&prompts, "(%s)", tmp);
			free(tmp);
		} else {
			prompts = xstrdup(":");
			space = 0;
		}
		next_prompt = prompts;
	} else
		next_prompt = prompts = xstrdup(s);
	if ((s = args_get(args, 'I')) != NULL)
		next_input = inputs = xstrdup(s);
	else
		next_input = NULL;
	while ((prompt = strsep(&next_prompt, ",")) != NULL) {
		cdata->prompts = xreallocarray(cdata->prompts, cdata->count + 1,
		    sizeof *cdata->prompts);
		if (!space)
			tmp = xstrdup(prompt);
		else
			xasprintf(&tmp, "%s ", prompt);
		cdata->prompts[cdata->count].prompt = tmp;

		if (next_input != NULL) {
			input = strsep(&next_input, ",");
			if (input == NULL)
				input = "";
		} else
			input = "";
		cdata->prompts[cdata->count].input = xstrdup(input);

		cdata->count++;
	}
	free(inputs);
	free(prompts);

	if ((type = args_get(args, 'T')) != NULL) {
		cdata->prompt_type = status_prompt_type(type);
		if (cdata->prompt_type == PROMPT_TYPE_INVALID) {
			cmdq_error(item, "unknown type: %s", type);
			cmd_command_prompt_free(cdata);
			return (CMD_RETURN_ERROR);
		}
	} else
		cdata->prompt_type = PROMPT_TYPE_COMMAND;

	if (args_has(args, '1'))
		cdata->flags |= PROMPT_SINGLE;
	else if (args_has(args, 'N'))
		cdata->flags |= PROMPT_NUMERIC;
	else if (args_has(args, 'i'))
		cdata->flags |= PROMPT_INCREMENTAL;
	else if (args_has(args, 'k'))
		cdata->flags |= PROMPT_KEY;
	status_prompt_set(tc, target, cdata->prompts[0].prompt,
	    cdata->prompts[0].input, cmd_command_prompt_callback,
	    cmd_command_prompt_free, cdata, cdata->flags, cdata->prompt_type);

	if (!wait)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

static int
cmd_command_prompt_callback(struct client *c, void *data, const char *s,
    int done)
{
	struct cmd_command_prompt_cdata		 *cdata = data;
	char					 *error;
	struct cmdq_item			 *item = cdata->item, *new_item;
	struct cmd_list				 *cmdlist;
	struct cmd_command_prompt_prompt	 *prompt;
	int					  argc = 0;
	char					**argv = NULL;

	if (s == NULL)
		goto out;

	if (done) {
		if (cdata->flags & PROMPT_INCREMENTAL)
			goto out;
		cmd_append_argv(&cdata->argc, &cdata->argv, s);
		if (++cdata->current != cdata->count) {
			prompt = &cdata->prompts[cdata->current];
			status_prompt_update(c, prompt->prompt, prompt->input);
			return (1);
		}
	}

	argc = cdata->argc;
	argv = cmd_copy_argv(cdata->argc, cdata->argv);
	if (!done)
		cmd_append_argv(&argc, &argv, s);

	if (done) {
		cmd_free_argv(cdata->argc, cdata->argv);
		cdata->argc = argc;
		cdata->argv = cmd_copy_argv(argc, argv);
	}

	cmdlist = args_make_commands(cdata->state, argc, argv, &error);
	if (cmdlist == NULL) {
		cmdq_append(c, cmdq_get_error(error));
		free(error);
	} else if (item == NULL) {
		new_item = cmdq_get_command(cmdlist, NULL);
		cmdq_append(c, new_item);
	} else {
		new_item = cmdq_get_command(cmdlist, cmdq_get_state(item));
		cmdq_insert_after(item, new_item);
	}
	cmd_free_argv(argc, argv);

	if (c->prompt_inputcb != cmd_command_prompt_callback)
		return (1);

out:
	if (item != NULL)
		cmdq_continue(item);
	return (0);
}

static void
cmd_command_prompt_free(void *data)
{
	struct cmd_command_prompt_cdata	*cdata = data;
	u_int				 i;

	for (i = 0; i < cdata->count; i++) {
		free(cdata->prompts[i].prompt);
		free(cdata->prompts[i].input);
	}
	free(cdata->prompts);
	cmd_free_argv(cdata->argc, cdata->argv);
	args_make_commands_free(cdata->state);
	free(cdata);
}
