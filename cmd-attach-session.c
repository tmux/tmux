/* $Id: cmd-attach-session.c,v 1.13 2008-06-02 21:08:36 nicm Exp $ */

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
 * Attach existing session to the current terminal.
 */

int	cmd_attach_session_parse(struct cmd *, void **, int, char **, char **);
void	cmd_attach_session_exec(void *, struct cmd_ctx *);
void	cmd_attach_session_send(void *, struct buffer *);
void	cmd_attach_session_recv(void **, struct buffer *);
void	cmd_attach_session_free(void *);

struct cmd_attach_session_data {
	char	*sname;
	int	 flag_detach;
};

const struct cmd_entry cmd_attach_session_entry = {
	"attach-session", "attach",
	"[-d] [-s session-name]",
	CMD_CANTNEST,
	cmd_attach_session_parse,
	cmd_attach_session_exec,
	cmd_attach_session_send,
	cmd_attach_session_recv,
	cmd_attach_session_free
};

int
cmd_attach_session_parse(
    struct cmd *self, void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_attach_session_data	*data;
	int				 opt;

	*ptr = data = xmalloc(sizeof *data);
	data->sname = NULL;
	data->flag_detach = 0;

	while ((opt = getopt(argc, argv, "ds:")) != EOF) {
		switch (opt) {
		case 'd':
			data->flag_detach = 1;
			break;
		case 's':
			data->sname = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		goto usage;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	cmd_attach_session_free(data);
	return (-1);
}

void
cmd_attach_session_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct cmd_attach_session_data	*data = ptr;
	struct session			*s;
	char				*cause;

	if (ctx->flags & CMD_KEY)
		return;

	if ((s = cmd_find_session(ctx, data->sname)) == NULL)
		return;

	if (!(ctx->cmdclient->flags & CLIENT_TERMINAL)) {
		ctx->error(ctx, "not a terminal");
		return;
	}

	if (tty_open(&ctx->cmdclient->tty, &cause) != 0) {
		ctx->error(ctx, "%s", cause);
		xfree(cause);
		return;
	}

	if (data->flag_detach)
		server_write_session(s, MSG_DETACH, NULL, 0);
	ctx->cmdclient->session = s;

	server_write_client(ctx->cmdclient, MSG_READY, NULL, 0);
	recalculate_sizes();
	server_redraw_client(ctx->cmdclient);
}

void
cmd_attach_session_send(void *ptr, struct buffer *b)
{
	struct cmd_attach_session_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->sname);
}

void
cmd_attach_session_recv(void **ptr, struct buffer *b)
{
	struct cmd_attach_session_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->sname = cmd_recv_string(b);
}

void
cmd_attach_session_free(void *ptr)
{
	struct cmd_attach_session_data	*data = ptr;

	if (data->sname != NULL)
		xfree(data->sname);
	xfree(data);
}
