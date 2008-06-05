/* $Id: cmd-new-session.c,v 1.26 2008-06-05 17:12:10 nicm Exp $ */

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

#include <getopt.h>

#include "tmux.h"

/*
 * Create a new session and attach to the current terminal unless -d is given.
 */

int	cmd_new_session_parse(struct cmd *, int, char **, char **);
void	cmd_new_session_exec(struct cmd *, struct cmd_ctx *);
void	cmd_new_session_send(struct cmd *, struct buffer *);
void	cmd_new_session_recv(struct cmd *, struct buffer *);
void	cmd_new_session_free(struct cmd *);
void	cmd_new_session_init(struct cmd *, int);
void	cmd_new_session_print(struct cmd *, char *, size_t);

struct cmd_new_session_data {
	char	*name;
	char	*winname;
	char	*cmd;
	int	 flag_detached;
};

const struct cmd_entry cmd_new_session_entry = {
	"new-session", "new",
	"[-d] [-n window-name] [-s session-name] [command]",
	CMD_STARTSERVER|CMD_CANTNEST,
	cmd_new_session_parse,
	cmd_new_session_exec,
	cmd_new_session_send,
	cmd_new_session_recv,
	cmd_new_session_free,
	cmd_new_session_init,
	cmd_new_session_print
};

void
cmd_new_session_init(struct cmd *self, unused int arg)
{
	struct cmd_new_session_data	 *data;

	self->data = data = xmalloc(sizeof *data);
	data->flag_detached = 0;
	data->name = NULL;
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

	while ((opt = getopt(argc, argv, "ds:n:")) != EOF) {
		switch (opt) {
		case 'd':
			data->flag_detached = 1;
			break;
		case 's':
			data->name = xstrdup(optarg);
			break;
		case 'n':
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

void
cmd_new_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_new_session_data	*data = self->data;
	struct client			*c = ctx->cmdclient;
	struct session			*s;
	char				*cmd, *cause;
	u_int				 sx, sy, slines;

	if (ctx->flags & CMD_KEY)
		return;

	if (!data->flag_detached) {
		if (c == NULL) {
			ctx->error(ctx, "no client to attach to");
			return;
		}
		if (!(c->flags & CLIENT_TERMINAL)) {
			ctx->error(ctx, "not a terminal");
			return;
		}
	}

	if (data->name != NULL && session_find(data->name) != NULL) {
		ctx->error(ctx, "duplicate session: %s", data->name);
		return;
	}

	cmd = data->cmd;
	if (cmd == NULL)
		cmd = options_get_string(&global_options, "default-command");

	sx = 80;
	sy = 25;
	if (!data->flag_detached) {
		sx = c->sx;
		sy = c->sy;
	}

	slines = options_get_number(&global_options, "status-lines");
	if (sy < slines)
		sy = slines + 1;
	sy -= slines;

	if (!data->flag_detached && tty_open(&c->tty, &cause) != 0) {
		ctx->error(ctx, "%s", cause);
		xfree(cause);
		return;
	}


	if ((s = session_create(data->name, cmd, sx, sy)) == NULL)
		fatalx("session_create failed");
	if (data->winname != NULL) {
		xfree(s->curw->window->name);
		s->curw->window->name = xstrdup(data->winname);
	}

	if (data->flag_detached) {
		if (c != NULL)
			server_write_client(c, MSG_EXIT, NULL, 0);
	} else {
		c->session = s;
		server_write_client(c, MSG_READY, NULL, 0);
		server_redraw_client(c);
	}
}

void
cmd_new_session_send(struct cmd *self, struct buffer *b)
{
	struct cmd_new_session_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->name);
	cmd_send_string(b, data->winname);
	cmd_send_string(b, data->cmd);
}

void
cmd_new_session_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_new_session_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->name = cmd_recv_string(b);
	data->winname = cmd_recv_string(b);
	data->cmd = cmd_recv_string(b);
}

void
cmd_new_session_free(struct cmd *self)
{
	struct cmd_new_session_data	*data = self->data;

	if (data->name != NULL)
		xfree(data->name);
	if (data->winname != NULL)
		xfree(data->winname);
	if (data->cmd != NULL)
		xfree(data->cmd);
	xfree(data);
}

void
cmd_new_session_print(struct cmd *cmd, char *buf, size_t len)
{
}
