/* $Id$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * Prompt for command in client.
 */

void	cmd_command_prompt_key_binding(struct cmd *, int);
int	cmd_command_prompt_check(struct args *);
int	cmd_command_prompt_exec(struct cmd *, struct cmd_ctx *);

int	cmd_command_prompt_callback(void *, const char *);
void	cmd_command_prompt_free(void *);

const struct cmd_entry cmd_command_prompt_entry = {
	"command-prompt", NULL,
	"I:p:t:", 0, 1,
	"[-I inputs] [-p prompts] " CMD_TARGET_CLIENT_USAGE " [template]",
	0,
	cmd_command_prompt_key_binding,
	NULL,
	cmd_command_prompt_exec
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

void
cmd_command_prompt_key_binding(struct cmd *self, int key)
{
	switch (key) {
	case '$':
		self->args = args_create(1, "rename-session '%%'");
		args_set(self->args, 'I', "#S");
		break;
	case ',':
		self->args = args_create(1, "rename-window '%%'");
		args_set(self->args, 'I', "#W");
		break;
	case '.':
		self->args = args_create(1, "move-window -t '%%'");
		break;
	case 'f':
		self->args = args_create(1, "find-window '%%'");
		break;
	case '\'':
		self->args = args_create(1, "select-window -t ':%%'");
		args_set(self->args, 'p', "index");
		break;
	default:
		self->args = args_create(0);
		break;
	}
}

int
cmd_command_prompt_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	const char			*inputs, *prompts;
	struct cmd_command_prompt_cdata	*cdata;
	struct client			*c;
	char				*prompt, *ptr, *input = NULL;
	size_t				 n;

	if ((c = cmd_find_client(ctx, args_get(args, 't'))) == NULL)
		return (-1);

	if (c->prompt_string != NULL)
		return (0);

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

	status_prompt_set(c, prompt, input, cmd_command_prompt_callback,
	    cmd_command_prompt_free, cdata, 0);
	xfree(prompt);

	return (0);
}

int
cmd_command_prompt_callback(void *data, const char *s)
{
	struct cmd_command_prompt_cdata	*cdata = data;
	struct client			*c = cdata->c;
	struct cmd_list			*cmdlist;
	struct cmd_ctx			 ctx;
	char				*cause, *new_template, *prompt, *ptr;
	char				*input = NULL;

	if (s == NULL)
		return (0);

	new_template = cmd_template_replace(cdata->template, s, cdata->idx);
	xfree(cdata->template);
	cdata->template = new_template;

	/*
	 * Check if there are more prompts; if so, get its respective input
	 * and update the prompt data.
	 */
	if ((ptr = strsep(&cdata->next_prompt, ",")) != NULL) {
		xasprintf(&prompt, "%s ", ptr);
		input = strsep(&cdata->next_input, ",");
		status_prompt_update(c, prompt, input);

		xfree(prompt);
		cdata->idx++;
		return (1);
	}

	if (cmd_string_parse(new_template, &cmdlist, &cause) != 0) {
		if (cause != NULL) {
			*cause = toupper((u_char) *cause);
			status_message_set(c, "%s", cause);
			xfree(cause);
		}
		return (0);
	}

	ctx.msgdata = NULL;
	ctx.curclient = c;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_list_exec(cmdlist, &ctx);
	cmd_list_free(cmdlist);

	if (c->prompt_callbackfn != (void *) &cmd_command_prompt_callback)
		return (1);
	return (0);
}

void
cmd_command_prompt_free(void *data)
{
	struct cmd_command_prompt_cdata	*cdata = data;

	if (cdata->inputs != NULL)
		xfree(cdata->inputs);
	if (cdata->prompts != NULL)
		xfree(cdata->prompts);
	if (cdata->template != NULL)
		xfree(cdata->template);
	xfree(cdata);
}
