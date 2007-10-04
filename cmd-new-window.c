/* $Id: cmd-new-window.c,v 1.2 2007-10-04 00:02:10 nicm Exp $ */

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
 * Create a new window.
 */

int		 cmd_new_window_parse(void **, int, char **, char **);
const char	*cmd_new_window_usage(void);
void		 cmd_new_window_exec(void *, struct cmd_ctx *);
void		 cmd_new_window_send(void *, struct buffer *);
void		 cmd_new_window_recv(void **, struct buffer *);
void		 cmd_new_window_free(void *);

struct cmd_new_window_data {
	char	*name;
	char	*cmd;
	int	 flag_detached;
};

const struct cmd_entry cmd_new_window_entry = {
	CMD_NEWWINDOW, "new-window", "neww", 0,
	cmd_new_window_parse,
	cmd_new_window_usage,
	cmd_new_window_exec, 
	cmd_new_window_send,
	cmd_new_window_recv,
	cmd_new_window_free
};

int
cmd_new_window_parse(void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_new_window_data	*data;
	int				 opt;

	*ptr = data = xmalloc(sizeof *data);
	data->flag_detached = 0;
	data->name = NULL;
	data->cmd = NULL;

	while ((opt = getopt(argc, argv, "dn:")) != EOF) {
		switch (opt) {
		case 'n':
			data->name = xstrdup(optarg);
			break;
		case 'd':
			data->flag_detached = 1;
			break;
		default:
			goto usage;
		}
	}	
	argc -= optind;
	argv += optind;
	if (argc != 0 && argc != 1)
		goto usage;

	data->cmd = NULL;
	if (argc == 1)
		data->cmd = xstrdup(argv[0]);

	return (0);

usage:
	usage(cause, "%s", cmd_new_window_usage());

	if (data->name != NULL)
		xfree(data->name);
	if (data->cmd != NULL)
		xfree(data->cmd);
	xfree(data);
	return (-1);
}

const char *
cmd_new_window_usage(void)
{
	return ("new-window [command]");
}

void
cmd_new_window_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct cmd_new_window_data	*data = ptr, std = { NULL, NULL, 0 };
	struct client			*c = ctx->client;
	struct session			*s = ctx->session;
	char				*cmd;
	u_int				 i;

	if (data == NULL)
		data = &std;
	
	cmd = data->cmd;
	if (cmd == NULL)
		cmd = default_command;

	if (session_new(s, data->name, cmd, &i) != 0) {
		ctx->error(ctx, "command failed: %s", cmd);
		return;
	}
	if (!data->flag_detached) {
		session_select(s, i);
		server_redraw_session(s);
	} else {
		/* XXX */
		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c != NULL && c->session == s)
				server_redraw_status(c); 
		}
	}
	
	if (!(ctx->flags & CMD_KEY))
		server_write_client(c, MSG_EXIT, NULL, 0);
}

void
cmd_new_window_send(void *ptr, struct buffer *b)
{
	struct cmd_new_window_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->name);
	cmd_send_string(b, data->cmd);
}

void
cmd_new_window_recv(void **ptr, struct buffer *b)
{
	struct cmd_new_window_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->name = cmd_recv_string(b);
	data->cmd = cmd_recv_string(b);
}

void
cmd_new_window_free(void *ptr)
{
	struct cmd_new_window_data	*data = ptr;

	if (data->name != NULL)
		xfree(data->name);
	if (data->cmd != NULL)
		xfree(data->cmd);
	xfree(data);
}
