/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
 * Copyright (c) 2009 Nicholas Marriott <nicm@openbsd.org>
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
#include <sys/wait.h>

#include <string.h>

#include "tmux.h"

/*
 * Executes a tmux command if a shell command returns true.
 */

int	cmd_if_shell_exec(struct cmd *, struct cmd_ctx *);

void	cmd_if_shell_callback(struct job *);
void	cmd_if_shell_free(void *);

const struct cmd_entry cmd_if_shell_entry = {
	"if-shell", "if",
	"shell-command command",
	CMD_ARG2, 0,
	cmd_target_init,
	cmd_target_parse,
	cmd_if_shell_exec,
	cmd_target_free,
	cmd_target_print
};

struct cmd_if_shell_data {
	char		*cmd;
	struct cmd_ctx	 ctx;
};

int
cmd_if_shell_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct cmd_if_shell_data	*cdata;
	struct job			*job;

	cdata = xmalloc(sizeof *cdata);
	cdata->cmd = xstrdup(data->arg2);
	memcpy(&cdata->ctx, ctx, sizeof cdata->ctx);

	if (ctx->cmdclient != NULL)
		ctx->cmdclient->references++;
	if (ctx->curclient != NULL)
		ctx->curclient->references++;

	job = job_add(NULL, 0, NULL,
	    data->arg, cmd_if_shell_callback, cmd_if_shell_free, cdata);
	job_run(job);

	return (1);	/* don't let client exit */
}

void
cmd_if_shell_callback(struct job *job)
{
	struct cmd_if_shell_data	*cdata = job->data;
	struct cmd_ctx			*ctx = &cdata->ctx;
	struct cmd_list			*cmdlist;
	char				*cause;

	if (!WIFEXITED(job->status) || WEXITSTATUS(job->status) != 0)
		return;

	if (cmd_string_parse(cdata->cmd, &cmdlist, &cause) != 0) {
		if (cause != NULL) {
			ctx->error(ctx, "%s", cause);
			xfree(cause);
		}
		return;
	}

	if (cmd_list_exec(cmdlist, ctx) < 0) {
		cmd_list_free(cmdlist);
		return;
	}

	cmd_list_free(cmdlist);
}

void
cmd_if_shell_free(void *data)
{
	struct cmd_if_shell_data	*cdata = data;
	struct cmd_ctx			*ctx = &cdata->ctx;

	if (ctx->cmdclient != NULL) {
		ctx->cmdclient->references--;
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
	}
	if (ctx->curclient != NULL)
		ctx->curclient->references--;

	xfree(cdata->cmd);
	xfree(cdata);
}
