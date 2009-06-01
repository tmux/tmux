/* $OpenBSD$ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Switch client to a different session.
 */

int	cmd_switch_client_parse(struct cmd *, int, char **, char **);
int	cmd_switch_client_exec(struct cmd *, struct cmd_ctx *);
void	cmd_switch_client_send(struct cmd *, struct buffer *);
void	cmd_switch_client_recv(struct cmd *, struct buffer *);
void	cmd_switch_client_free(struct cmd *);
size_t	cmd_switch_client_print(struct cmd *, char *, size_t);

struct cmd_switch_client_data {
	char	*name;
	char	*target;
};

const struct cmd_entry cmd_switch_client_entry = {
	"switch-client", "switchc",
	"[-c target-client] [-t target-session]",
	0,
	NULL,
	cmd_switch_client_parse,
	cmd_switch_client_exec,
	cmd_switch_client_send,
	cmd_switch_client_recv,
	cmd_switch_client_free,
	cmd_switch_client_print
};

int
cmd_switch_client_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_switch_client_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->name = NULL;
	data->target = NULL;

	while ((opt = getopt(argc, argv, "c:t:")) != -1) {
		switch (opt) {
		case 'c':
			data->name = xstrdup(optarg);
			break;
		case 't':
			data->target = xstrdup(optarg);
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

	self->entry->free(self);
	return (-1);
}

int
cmd_switch_client_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_switch_client_data	*data = self->data;
	struct client			*c;
	struct session			*s;

	if (data == NULL)
		return (0);

	if ((c = cmd_find_client(ctx, data->name)) == NULL)
		return (-1);
	if ((s = cmd_find_session(ctx, data->target)) == NULL)
		return (-1);

	c->session = s;

	recalculate_sizes();
	server_redraw_client(c);

	return (0);
}

void
cmd_switch_client_send(struct cmd *self, struct buffer *b)
{
	struct cmd_switch_client_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->name);
	cmd_send_string(b, data->target);
}

void
cmd_switch_client_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_switch_client_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->name = cmd_recv_string(b);
	data->target = cmd_recv_string(b);
}

void
cmd_switch_client_free(struct cmd *self)
{
	struct cmd_switch_client_data	*data = self->data;

	if (data->name != NULL)
		xfree(data->name);
	if (data->target != NULL)
		xfree(data->target);
	xfree(data);
}

size_t
cmd_switch_client_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_switch_client_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->name != NULL)
		off += cmd_prarg(buf + off, len - off, " -c ", data->name);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
	return (off);
}
