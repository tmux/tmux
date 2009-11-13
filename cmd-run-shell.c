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
 * Runs a command without a window.
 */

int	cmd_run_shell_exec(struct cmd *, struct cmd_ctx *);

void	cmd_run_shell_callback(struct job *);
void	cmd_run_shell_free(void *);

const struct cmd_entry cmd_run_shell_entry = {
	"run-shell", "run",
	"command",
	CMD_ARG1, "",
	cmd_target_init,
	cmd_target_parse,
	cmd_run_shell_exec,
	cmd_target_free,
	cmd_target_print
};

struct cmd_run_shell_data {
	char		*cmd;
	struct cmd_ctx	 ctx;
};

int
cmd_run_shell_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct cmd_run_shell_data	*cdata;
	struct job			*job;

	cdata = xmalloc(sizeof *cdata);
	cdata->cmd = xstrdup(data->arg);
	memcpy(&cdata->ctx, ctx, sizeof cdata->ctx);

	if (ctx->cmdclient != NULL)
		ctx->cmdclient->references++;
	if (ctx->curclient != NULL)
		ctx->curclient->references++;

	job = job_add(NULL, 0, NULL,
	    data->arg, cmd_run_shell_callback, cmd_run_shell_free, cdata);
	job_run(job);

	return (1);	/* don't let client exit */
}

void
cmd_run_shell_callback(struct job *job)
{
	struct cmd_run_shell_data	*cdata = job->data;
	struct cmd_ctx			*ctx = &cdata->ctx;
	char				*cmd, *msg, *line;
	size_t				 size;
	int				 retcode;
	u_int				 lines;

	lines = 0;
	do {
		if ((line = evbuffer_readline(job->event->input)) != NULL) {
			ctx->print(ctx, "%s", line);
			lines++;
		}
	} while (line != NULL);

	size = EVBUFFER_LENGTH(job->event->input);
	if (size != 0) {
		line = xmalloc(size + 1);
		memcpy(line, EVBUFFER_DATA(job->event->input), size);
		line[size] = '\0';

		ctx->print(ctx, "%s", line);
		lines++;

		xfree(line);
	}

	cmd = cdata->cmd;

	msg = NULL;
	if (WIFEXITED(job->status)) {
		if ((retcode = WEXITSTATUS(job->status)) != 0)
			xasprintf(&msg, "'%s' returned %d", cmd, retcode);
	} else if (WIFSIGNALED(job->status)) {
		retcode = WTERMSIG(job->status);
		xasprintf(&msg, "'%s' terminated by signal %d", cmd, retcode);
	}
	if (msg != NULL) {
		if (lines != 0)
			ctx->print(ctx, "%s", msg);
		else
			ctx->info(ctx, "%s", msg);
		xfree(msg);
	}
}

void
cmd_run_shell_free(void *data)
{
	struct cmd_run_shell_data	*cdata = data;
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
