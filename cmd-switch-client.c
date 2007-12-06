/* $Id: cmd-switch-client.c,v 1.2 2007-12-06 09:46:22 nicm Exp $ */

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

int	cmd_switch_client_parse(void **, int, char **, char **);
void	cmd_switch_client_exec(void *, struct cmd_ctx *);
void	cmd_switch_client_send(void *, struct buffer *);
void	cmd_switch_client_recv(void **, struct buffer *);
void	cmd_switch_client_free(void *);

struct cmd_switch_client_data {
	char	*name;
};

const struct cmd_entry cmd_switch_client_entry = {
	"switch-client", "switchc", "session-name",
	CMD_NOSESSION,
	cmd_switch_client_parse,
	cmd_switch_client_exec,
	cmd_switch_client_send,
	cmd_switch_client_recv,
	cmd_switch_client_free
};

int
cmd_switch_client_parse(void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_switch_client_data	*data;
	int				 opt;

	*ptr = data = xmalloc(sizeof *data);
	data->name = NULL;

	while ((opt = getopt(argc, argv, "")) != EOF) {
		switch (opt) {
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
	usage(cause, "%s %s",
	    cmd_switch_client_entry.name, cmd_switch_client_entry.usage);

	cmd_switch_client_free(data);
	return (-1);
}

void
cmd_switch_client_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct cmd_switch_client_data	*data = ptr;
	struct session			*s;

	if (data == NULL)
		return;

	if ((s = session_find(data->name)) == NULL) {
		ctx->error(ctx, "session not found: %s", data->name);
		return;
	}

	ctx->client->session = s;

	recalculate_sizes();
	server_redraw_client(ctx->client);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_switch_client_send(void *ptr, struct buffer *b)
{
	struct cmd_switch_client_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->name);
}

void
cmd_switch_client_recv(void **ptr, struct buffer *b)
{
	struct cmd_switch_client_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->name = cmd_recv_string(b);
}

void
cmd_switch_client_free(void *ptr)
{
	struct cmd_switch_client_data	*data = ptr;

	if (data->name != NULL)
		xfree(data->name);
	xfree(data);
}
