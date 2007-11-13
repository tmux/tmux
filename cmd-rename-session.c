/* $Id: cmd-rename-session.c,v 1.2 2007-11-13 09:53:47 nicm Exp $ */

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

int	cmd_rename_session_parse(void **, int, char **, char **);
void	cmd_rename_session_exec(void *, struct cmd_ctx *);
void	cmd_rename_session_send(void *, struct buffer *);
void	cmd_rename_session_recv(void **, struct buffer *);
void	cmd_rename_session_free(void *);

struct cmd_rename_session_data {
	char	*newname;
};

const struct cmd_entry cmd_rename_session_entry = {
	"rename-session", "rename", "new-name",
	0,
	cmd_rename_session_parse,
	cmd_rename_session_exec, 
	cmd_rename_session_send,
	cmd_rename_session_recv,
	cmd_rename_session_free
};

int
cmd_rename_session_parse(void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_rename_session_data	*data;
	int				 opt;

	*ptr = data = xmalloc(sizeof *data);
	data->newname = NULL;

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

	data->newname = xstrdup(argv[0]);

	return (0);

usage:
	usage(cause, "%s %s",
	    cmd_rename_session_entry.name, cmd_rename_session_entry.usage);

	cmd_rename_session_free(data);
	return (-1);
}

void
cmd_rename_session_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct cmd_rename_session_data	*data = ptr;

	if (data == NULL)
		return;

	xfree(ctx->session->name);
	ctx->session->name = xstrdup(data->newname);
	
	if (!(ctx->flags & CMD_KEY))
		server_write_client(ctx->client, MSG_EXIT, NULL, 0);
}

void
cmd_rename_session_send(void *ptr, struct buffer *b)
{
	struct cmd_rename_session_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->newname);
}

void
cmd_rename_session_recv(void **ptr, struct buffer *b)
{
	struct cmd_rename_session_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->newname = cmd_recv_string(b);
}

void
cmd_rename_session_free(void *ptr)
{
	struct cmd_rename_session_data	*data = ptr;

	if (data->newname != NULL)
		xfree(data->newname);
	xfree(data);
}
