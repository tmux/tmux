/* $Id: cmd-command-prompt.c,v 1.7 2009-01-11 00:48:42 nicm Exp $ */

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

#include "tmux.h"

/*
 * Prompt for command in client.
 */

void	cmd_command_prompt_exec(struct cmd *, struct cmd_ctx *);

int	cmd_command_prompt_callback(void *, const char *);

const struct cmd_entry cmd_command_prompt_entry = {
	"command-prompt", NULL,
	CMD_TARGET_CLIENT_USAGE,
	0,
	cmd_target_init,
	cmd_target_parse,
	cmd_command_prompt_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

void
cmd_command_prompt_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	struct client		*c;

	if ((c = cmd_find_client(ctx, data->target)) == NULL)
		return;

	if (c->prompt_string != NULL)
		return;

	server_set_client_prompt(c, ":", cmd_command_prompt_callback, c, 0);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

int
cmd_command_prompt_callback(void *data, const char *s)
{
	struct client	*c = data;
	struct cmd	*cmd;
	struct cmd_ctx	 ctx;
	char		*cause;

	if (s == NULL)
		return (0);

	if (cmd_string_parse(s, &cmd, &cause) != 0) {
		if (cause == NULL)
			return (0);
		*cause = toupper((u_char) *cause);
		server_set_client_message(c, cause);
		xfree(cause);
		return (0);
	}
	if (cmd == NULL)
		return (0);

	ctx.msgdata = NULL;
	ctx.cursession = c->session;
	ctx.curclient = c;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_exec(cmd, &ctx);

	if (c->prompt_callback != (void *) &cmd_command_prompt_callback)
		return (1);
	return (0);
}
