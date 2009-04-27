/* $Id: cmd-confirm-before.c,v 1.2 2009-04-27 17:27:36 nicm Exp $ */

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

int	cmd_confirm_before_parse(struct cmd *, int, char **, char **);
int	cmd_confirm_before_exec(struct cmd *, struct cmd_ctx *);
void	cmd_confirm_before_send(struct cmd *, struct buffer *);
void	cmd_confirm_before_recv(struct cmd *, struct buffer *);
void	cmd_confirm_before_free(struct cmd *);
void	cmd_confirm_before_init(struct cmd *, int);
size_t	cmd_confirm_before_print(struct cmd *, char *, size_t);
int	cmd_confirm_before_callback(void *, const char *);

struct cmd_confirm_before_data {
	char *cmd;
};

struct cmd_confirm_before_cdata {
	struct client			*c;
	struct cmd_confirm_before_data	 data;
};

const struct cmd_entry cmd_confirm_before_entry = {
	"confirm-before", "confirm",
	"command",
	0,
	cmd_confirm_before_init,
	cmd_confirm_before_parse,
	cmd_confirm_before_exec,
	cmd_confirm_before_send,
	cmd_confirm_before_recv,
	cmd_confirm_before_free,
	cmd_confirm_before_print
};

void
cmd_confirm_before_init(struct cmd *self, int key)
{
	struct cmd_confirm_before_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->cmd = NULL;

	switch (key) {
	case '&':
		data->cmd = xstrdup("kill-window");
		break;
	case 'x':
		data->cmd = xstrdup("kill-pane");
		break;
	}
}

int
cmd_confirm_before_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_confirm_before_data	*data;
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
	if (argc != 1)
		goto usage;

	data->cmd = xstrdup(argv[0]);
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

int
cmd_confirm_before_exec(unused struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_confirm_before_cdata	*cdata;
	struct cmd_confirm_before_data	*data = self->data;
	char				*buf, *cmd, *ptr;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}

	ptr = xstrdup(data->cmd);
	if ((cmd = strtok(ptr, " \t")) == NULL)
		cmd = ptr;
	xasprintf(&buf, "Confirm '%s'? (y/n) ", cmd);
	xfree(ptr);

	cdata = xmalloc(sizeof *cdata);
	cdata->data.cmd = xstrdup(data->cmd);
	cdata->c = ctx->curclient;
	status_prompt_set(
	    cdata->c, buf, cmd_confirm_before_callback, cdata, PROMPT_SINGLE);

	xfree(buf);
	return (1);
}

void
cmd_confirm_before_send(struct cmd *self, struct buffer *b)
{
	struct cmd_confirm_before_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cmd);
}

void
cmd_confirm_before_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_confirm_before_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cmd = cmd_recv_string(b);
}

void
cmd_confirm_before_free(struct cmd *self)
{
	struct cmd_confirm_before_data	*data = self->data;

	if (data->cmd != NULL)
		xfree(data->cmd);
	xfree(data);
}

size_t
cmd_confirm_before_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_confirm_before_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s ", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->cmd != NULL)
		off += xsnprintf(buf + off, len - off, "%s", data->cmd);
	return (off);
}

int
cmd_confirm_before_callback(void *data, const char *s)
{
	struct cmd_confirm_before_cdata	*cdata = data;
	struct client			*c = cdata->c;
	struct cmd_list			*cmdlist;
	struct cmd_ctx	 	 	 ctx;
	char				*cause;

	if (s == NULL || tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		goto out;

	if (cmd_string_parse(cdata->data.cmd, &cmdlist, &cause) != 0) {
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
	if (cdata->data.cmd != NULL)
		xfree(cdata->data.cmd);
	xfree(cdata);

	return (0);
}
