/* $Id$ */

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

#include <ctype.h>
#include <string.h>

#include "tmux.h"

/*
 * Asks for confirmation before executing a command.
 */

void	cmd_confirm_before_key_binding(struct cmd *, int);
int	cmd_confirm_before_exec(struct cmd *, struct cmd_ctx *);

int	cmd_confirm_before_callback(void *, const char *);
void	cmd_confirm_before_free(void *);

const struct cmd_entry cmd_confirm_before_entry = {
	"confirm-before", "confirm",
	"p:t:", 1, 1,
	"[-p prompt] " CMD_TARGET_CLIENT_USAGE " command",
	0,
	cmd_confirm_before_key_binding,
	NULL,
	cmd_confirm_before_exec
};

struct cmd_confirm_before_data {
	struct client	*c;
	char		*cmd;
};

void
cmd_confirm_before_key_binding(struct cmd *self, int key)
{
	switch (key) {
	case '&':
		self->args = args_create(1, "kill-window");
		args_set(self->args, 'p', "kill-window #W? (y/n)");
		break;
	case 'x':
		self->args = args_create(1, "kill-pane");
		args_set(self->args, 'p', "kill-pane #P? (y/n)");
		break;
	default:
		self->args = args_create(0);
		break;
	}
}

int
cmd_confirm_before_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct cmd_confirm_before_data	*cdata;
	struct client			*c;
	char				*cmd, *copy, *new_prompt, *ptr;
	const char			*prompt;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}

	if ((c = cmd_find_client(ctx, args_get(args, 't'))) == NULL)
		return (-1);

	if ((prompt = args_get(args, 'p')) != NULL)
		xasprintf(&new_prompt, "%s ", prompt);
	else {
		ptr = copy = xstrdup(args->argv[0]);
		cmd = strsep(&ptr, " \t");
		xasprintf(&new_prompt, "Confirm '%s'? (y/n) ", cmd);
		xfree(copy);
	}

	cdata = xmalloc(sizeof *cdata);
	cdata->cmd = xstrdup(args->argv[0]);
	cdata->c = c;
	status_prompt_set(cdata->c, new_prompt, NULL,
	    cmd_confirm_before_callback, cmd_confirm_before_free, cdata,
	    PROMPT_SINGLE);

	xfree(new_prompt);
	return (1);
}

int
cmd_confirm_before_callback(void *data, const char *s)
{
	struct cmd_confirm_before_data	*cdata = data;
	struct client			*c = cdata->c;
	struct cmd_list			*cmdlist;
	struct cmd_ctx	 	 	 ctx;
	char				*cause;

	if (s == NULL || *s == '\0')
		return (0);
	if (tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		return (0);

	if (cmd_string_parse(cdata->cmd, &cmdlist, &cause) != 0) {
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

	return (0);
}

void
cmd_confirm_before_free(void *data)
{
	struct cmd_confirm_before_data	*cdata = data;

	if (cdata->cmd != NULL)
		xfree(cdata->cmd);
	xfree(cdata);
}
