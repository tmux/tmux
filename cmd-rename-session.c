/* $Id: cmd-rename-session.c,v 1.9 2008-06-05 16:35:32 nicm Exp $ */

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

#include "tmux.h"

/*
 * Change session name.
 */

int	cmd_rename_session_parse(struct cmd *, int, char **, char **);
void	cmd_rename_session_exec(struct cmd *, struct cmd_ctx *);
void	cmd_rename_session_send(struct cmd *, struct buffer *);
void	cmd_rename_session_recv(struct cmd *, struct buffer *);
void	cmd_rename_session_free(struct cmd *);

struct cmd_rename_session_data {
	char	*cname;
	char	*sname;
	char	*newname;
};

const struct cmd_entry cmd_rename_session_entry = {
	"rename-session", "rename",
	"[-c client-tty|-s session-name] new-name",
	0,
	cmd_rename_session_parse,
	cmd_rename_session_exec,
	cmd_rename_session_send,
	cmd_rename_session_recv,
	cmd_rename_session_free,
	NULL,
	NULL
};

int
cmd_rename_session_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_rename_session_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;
	data->newname = NULL;

	while ((opt = getopt(argc, argv, "c:s:")) != EOF) {
		switch (opt) {
		case 'c':
			if (data->sname != NULL)
				goto usage;
			if (data->cname == NULL)
				data->cname = xstrdup(optarg);
			break;
		case 's':
			if (data->cname != NULL)
				goto usage;
			if (data->sname == NULL)
				data->sname = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		goto usage;

	data->newname = xstrdup(argv[0]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_rename_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_rename_session_data	*data = self->data;
	struct session			*s;

	if (data == NULL)
		return;

	if ((s = cmd_find_session(ctx, data->cname, data->sname)) == NULL)
		return;

	xfree(s->name);
	s->name = xstrdup(data->newname);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_rename_session_send(struct cmd *self, struct buffer *b)
{
	struct cmd_rename_session_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->sname);
	cmd_send_string(b, data->newname);
}

void
cmd_rename_session_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_rename_session_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->sname = cmd_recv_string(b);
	data->newname = cmd_recv_string(b);
}

void
cmd_rename_session_free(struct cmd *self)
{
	struct cmd_rename_session_data	*data = self->data;

	if (data->cname != NULL)
		xfree(data->cname);
	if (data->sname != NULL)
		xfree(data->sname);
	if (data->newname != NULL)
		xfree(data->newname);
	xfree(data);
}
