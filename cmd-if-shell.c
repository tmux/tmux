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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Executes a tmux command if a shell command returns true.
 */

int	cmd_if_shell_parse(struct cmd *, int, char **, char **);
int	cmd_if_shell_exec(struct cmd *, struct cmd_ctx *);
void	cmd_if_shell_send(struct cmd *, struct buffer *);
void	cmd_if_shell_recv(struct cmd *, struct buffer *);
void	cmd_if_shell_free(struct cmd *);
void	cmd_if_shell_init(struct cmd *, int);
size_t	cmd_if_shell_print(struct cmd *, char *, size_t);

struct cmd_if_shell_data {
	char *cmd;
	char *sh_cmd;
};

const struct cmd_entry cmd_if_shell_entry = {
	"if-shell", "if",
	"shell-command command",
	0,
	cmd_if_shell_init,
	cmd_if_shell_parse,
	cmd_if_shell_exec,
	cmd_if_shell_send,
	cmd_if_shell_recv,
	cmd_if_shell_free,
	cmd_if_shell_print
};

void
cmd_if_shell_init(struct cmd *self, unused int arg)
{
	struct cmd_if_shell_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->cmd = NULL;
	data->sh_cmd = NULL;
}

int
cmd_if_shell_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_if_shell_data	*data;
	int				 opt;

	self->entry->init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 2)
		goto usage;

	data->sh_cmd = xstrdup(argv[0]);
	data->cmd = xstrdup(argv[1]);
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

int
cmd_if_shell_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_if_shell_data	*data = self->data;
	struct cmd_list			*cmdlist;
	char				*cause;
	int				 ret;

	if ((ret = system(data->sh_cmd)) < 0) {
		ctx->error(ctx, "system error: %s", strerror(errno));
		return (-1);
	} else if (ret != 0)
		return (0);

	if (cmd_string_parse(data->cmd, &cmdlist, &cause) != 0) {
		if (cause != NULL) {
			ctx->error(ctx, "%s", cause);
			xfree(cause);
		}
		return (-1);
	}

	if (cmd_list_exec(cmdlist, ctx) < 0) {
		cmd_list_free(cmdlist);
		return (-1);
	}

	cmd_list_free(cmdlist);
	return (0);
}

void
cmd_if_shell_send(struct cmd *self, struct buffer *b)
{
	struct cmd_if_shell_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cmd);
	cmd_send_string(b, data->sh_cmd);
}

void
cmd_if_shell_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_if_shell_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cmd = cmd_recv_string(b);
	data->sh_cmd = cmd_recv_string(b);
}

void
cmd_if_shell_free(struct cmd *self)
{
	struct cmd_if_shell_data	*data = self->data;

	if (data->cmd != NULL)
		xfree(data->cmd);
	if (data->sh_cmd != NULL)
		xfree(data->sh_cmd);
	xfree(data);
}

size_t
cmd_if_shell_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_if_shell_data	*data = self->data;
	size_t				off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->sh_cmd != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->sh_cmd);
	if (off < len && data->cmd != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->cmd);
	return (off);
}
