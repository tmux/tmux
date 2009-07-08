/* $OpenBSD: cmd-new-session.c,v 1.2 2009/07/07 19:49:19 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include "tmux.h"

/*
 * Create a new session and attach to the current terminal unless -d is given.
 */

int	cmd_new_session_parse(struct cmd *, int, char **, char **);
int	cmd_new_session_exec(struct cmd *, struct cmd_ctx *);
void	cmd_new_session_send(struct cmd *, struct buffer *);
void	cmd_new_session_recv(struct cmd *, struct buffer *);
void	cmd_new_session_free(struct cmd *);
void	cmd_new_session_init(struct cmd *, int);
size_t	cmd_new_session_print(struct cmd *, char *, size_t);

struct cmd_new_session_data {
	char	*newname;
	char	*winname;
	char	*cmd;
	int	 flag_detached;
};

const struct cmd_entry cmd_new_session_entry = {
	"new-session", "new",
	"[-d] [-n window-name] [-s session-name] [command]",
	CMD_STARTSERVER|CMD_CANTNEST,
	cmd_new_session_init,
	cmd_new_session_parse,
	cmd_new_session_exec,
	cmd_new_session_send,
	cmd_new_session_recv,
	cmd_new_session_free,
	cmd_new_session_print
};

void
cmd_new_session_init(struct cmd *self, unused int arg)
{
	struct cmd_new_session_data	 *data;

	self->data = data = xmalloc(sizeof *data);
	data->flag_detached = 0;
	data->newname = NULL;
	data->winname = NULL;
	data->cmd = NULL;
}

int
cmd_new_session_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_new_session_data	*data;
	int				 opt;

	self->entry->init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "ds:n:")) != -1) {
		switch (opt) {
		case 'd':
			data->flag_detached = 1;
			break;
		case 's':
			if (data->newname == NULL)
				data->newname = xstrdup(optarg);
			break;
		case 'n':
			if (data->winname == NULL)
				data->winname = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0 && argc != 1)
		goto usage;

	if (argc == 1)
		data->cmd = xstrdup(argv[0]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

int
cmd_new_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_new_session_data	*data = self->data;
	struct client			*c = ctx->cmdclient;
	struct session			*s;
	char				*cmd, *cwd, *cause;
	u_int				 sx, sy;

	if (ctx->curclient != NULL)
		return (0);

	if (!data->flag_detached) {
		if (c == NULL) {
			ctx->error(ctx, "no client to attach to");
			return (-1);
		}
		if (!(c->flags & CLIENT_TERMINAL)) {
			ctx->error(ctx, "not a terminal");
			return (-1);
		}
	}

	if (data->newname != NULL && session_find(data->newname) != NULL) {
		ctx->error(ctx, "duplicate session: %s", data->newname);
		return (-1);
	}

	cmd = data->cmd;
	if (cmd == NULL)
		cmd = options_get_string(&global_s_options, "default-command");
	if (c == NULL || c->cwd == NULL)
		cwd = options_get_string(&global_s_options, "default-path");
	else
		cwd = c->cwd;

	sx = 80;
	sy = 25;
	if (!data->flag_detached) {
		sx = c->tty.sx;
		sy = c->tty.sy;
	}

	if (options_get_number(&global_s_options, "status")) {
		if (sy == 0)
			sy = 1;
		else
			sy--;
	}

	if (!data->flag_detached && tty_open(&c->tty, &cause) != 0) {
		ctx->error(ctx, "open terminal failed: %s", cause);
		xfree(cause);
		return (-1);
	}


	s = session_create(data->newname, cmd, cwd, sx, sy, &cause);
	if (s == NULL) {
		ctx->error(ctx, "create session failed: %s", cause);
		xfree(cause);
		return (-1);
	}
	if (data->winname != NULL) {
		xfree(s->curw->window->name);
		s->curw->window->name = xstrdup(data->winname);
		options_set_number(
		    &s->curw->window->options, "automatic-rename", 0);
	}

	if (data->flag_detached) {
		if (c != NULL)
			server_write_client(c, MSG_EXIT, NULL, 0);
	} else {
		c->session = s;
		server_write_client(c, MSG_READY, NULL, 0);
		server_redraw_client(c);
	}
	recalculate_sizes();

	return (1);
}

void
cmd_new_session_send(struct cmd *self, struct buffer *b)
{
	struct cmd_new_session_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->newname);
	cmd_send_string(b, data->winname);
	cmd_send_string(b, data->cmd);
}

void
cmd_new_session_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_new_session_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->newname = cmd_recv_string(b);
	data->winname = cmd_recv_string(b);
	data->cmd = cmd_recv_string(b);
}

void
cmd_new_session_free(struct cmd *self)
{
	struct cmd_new_session_data	*data = self->data;

	if (data->newname != NULL)
		xfree(data->newname);
	if (data->winname != NULL)
		xfree(data->winname);
	if (data->cmd != NULL)
		xfree(data->cmd);
	xfree(data);
}

size_t
cmd_new_session_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_new_session_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->flag_detached)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->newname != NULL)
		off += cmd_prarg(buf + off, len - off, " -s ", data->newname);
	if (off < len && data->winname != NULL)
		off += cmd_prarg(buf + off, len - off, " -n ", data->winname);
	if (off < len && data->cmd != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->cmd);
	return (off);
}
