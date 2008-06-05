/* $Id: cmd-switch-client.c,v 1.7 2008-06-05 16:35:32 nicm Exp $ */

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
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Switch client to a different session.
 */

int	cmd_switch_client_parse(struct cmd *, int, char **, char **);
void	cmd_switch_client_exec(struct cmd *, struct cmd_ctx *);
void	cmd_switch_client_send(struct cmd *, struct buffer *);
void	cmd_switch_client_recv(struct cmd *, struct buffer *);
void	cmd_switch_client_free(struct cmd *);

struct cmd_switch_client_data {
	char	*cname;
	char	*name;
};

const struct cmd_entry cmd_switch_client_entry = {
	"switch-client", "switchc",
	"[-c client-tty] session-name",
	0,
	cmd_switch_client_parse,
	cmd_switch_client_exec,
	cmd_switch_client_send,
	cmd_switch_client_recv,
	cmd_switch_client_free,
	NULL,
	NULL
};

int
cmd_switch_client_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_switch_client_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->name = NULL;

	while ((opt = getopt(argc, argv, "c:")) != EOF) {
		switch (opt) {
		case 'c':
			data->cname = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		goto usage;

	data->name = xstrdup(argv[0]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_switch_client_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_switch_client_data	*data = self->data;
	struct client			*c;
	struct session			*s;

	if (data == NULL)
		return;

	if ((c = cmd_find_client(ctx, data->cname)) == NULL)
		return;

	if ((s = session_find(data->name)) == NULL) {
		ctx->error(ctx, "session not found: %s", data->name);
		return;
	}
	c->session = s;

	recalculate_sizes();
	server_redraw_client(c);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_switch_client_send(struct cmd *self, struct buffer *b)
{
	struct cmd_switch_client_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->name);
}

void
cmd_switch_client_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_switch_client_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->name = cmd_recv_string(b);
}

void
cmd_switch_client_free(struct cmd *self)
{
	struct cmd_switch_client_data	*data = self->data;

	if (data->cname != NULL)
		xfree(data->cname);
	if (data->name != NULL)
		xfree(data->name);
	xfree(data);
}
