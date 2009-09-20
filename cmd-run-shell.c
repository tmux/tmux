/* $Id: cmd-run-shell.c,v 1.1 2009-09-20 22:20:10 tcunha Exp $ */

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
#include <sys/wait.h>

#include <string.h>

#include "tmux.h"

/*
 * Runs a command without a window.
 */

int	cmd_run_shell_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_run_shell_entry = {
	"run-shell", "run",
	"command",
	CMD_ARG1, 0,
	cmd_target_init,
	cmd_target_parse,
	cmd_run_shell_exec,
	cmd_target_free,
	cmd_target_print
};

int
cmd_run_shell_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data	*data = self->data;
	FILE			*fp;
	char			*buf, *lbuf, *msg;
	size_t			 len;
	int			 has_output, ret, status;

	if ((fp = popen(data->arg, "r")) == NULL) {
		ctx->error(ctx, "popen error");
		return (-1);
	}

	has_output = 0;
	lbuf = NULL;
	while ((buf = fgetln(fp, &len)) != NULL) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			lbuf = xmalloc(len + 1);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}
		ctx->print(ctx, "%s", buf);
		has_output = 1;
	}
	if (lbuf != NULL)
		xfree(lbuf);

	msg = NULL;
	status = pclose(fp);

	if (WIFEXITED(status)) {
		if ((ret = WEXITSTATUS(status)) == 0)
			return (0);
		xasprintf(&msg, "'%s' returned %d", data->arg, ret);
	} else if (WIFSIGNALED(status)) {
		xasprintf(
		    &msg, "'%s' terminated by signal %d", data->arg,
		    WTERMSIG(status));
	}

	if (msg != NULL) {
		if (has_output)
			ctx->print(ctx, "%s", msg);
		else
			ctx->info(ctx, "%s", msg);
		xfree(msg);
	}

	return (0);
}
