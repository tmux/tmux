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

#include <ctype.h>
#include <string.h>

#include "tmux.h"

/*
 * Asks for confirmation before executing a command.
 */

int	cmd_confirm_before_exec(struct cmd *, struct cmd_ctx *);
void	cmd_confirm_before_init(struct cmd *, int);

int	cmd_confirm_before_callback(void *, const char *);

struct cmd_confirm_before_data {
	struct client	*c;
	char		*cmd;
};

const struct cmd_entry cmd_confirm_before_entry = {
	"confirm-before", "confirm",
	CMD_TARGET_CLIENT_USAGE " command",
	CMD_ARG1,
	cmd_confirm_before_init,
	cmd_target_parse,
	cmd_confirm_before_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_confirm_before_init(struct cmd *self, int key)
{
	struct cmd_target_data	*data;

	cmd_target_init(self, key);
	data = self->data;

	switch (key) {
	case '&':
		data->arg = xstrdup("kill-window");
		break;
	case 'x':
		data->arg = xstrdup("kill-pane");
		break;
	}
}

int
cmd_confirm_before_exec(unused struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct cmd_confirm_before_data	*cdata;
	struct client			*c;
	char				*buf, *cmd, *ptr;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}

	if ((c = cmd_find_client(ctx, data->target)) == NULL)
		return (-1);

	ptr = xstrdup(data->arg);
	if ((cmd = strtok(ptr, " \t")) == NULL)
		cmd = ptr;
	xasprintf(&buf, "Confirm '%s'? (y/n) ", cmd);
	xfree(ptr);

	cdata = xmalloc(sizeof *cdata);
	cdata->cmd = xstrdup(data->arg);
	cdata->c = c;
	status_prompt_set(
	    cdata->c, buf, cmd_confirm_before_callback, cdata, PROMPT_SINGLE);

	xfree(buf);
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

	if (s == NULL || tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		goto out;

	if (cmd_string_parse(cdata->cmd, &cmdlist, &cause) != 0) {
		if (cause != NULL) {
			*cause = toupper((u_char) *cause);
			status_message_set(c, cause);
			xfree(cause);
		}
		goto out;
	}

	ctx.msgdata = NULL;
	ctx.cursession = c->session;
	ctx.curclient = c;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_list_exec(cmdlist, &ctx);
	cmd_list_free(cmdlist);

out:
	if (cdata->cmd != NULL)
		xfree(cdata->cmd);
	xfree(cdata);

	return (0);
}
