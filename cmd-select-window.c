/* $Id: cmd-select-window.c,v 1.2 2007-10-04 21:48:11 nicm Exp $ */

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
 * Select window by index.
 */

int		 cmd_select_window_parse(void **, int, char **, char **);
void		 cmd_select_window_exec(void *, struct cmd_ctx *);
void		 cmd_select_window_send(void *, struct buffer *);
void		 cmd_select_window_recv(void **, struct buffer *);
void		 cmd_select_window_free(void *);

struct cmd_select_window_data {
	u_int	idx;
};

const struct cmd_entry cmd_select_window_entry = {
	CMD_SELECTWINDOW, "select-window", "selectw", "[command]",
	0,
	cmd_select_window_parse,
	cmd_select_window_exec, 
	cmd_select_window_send,
	cmd_select_window_recv,
	cmd_select_window_free
};

/*
 * select-window requires different defaults depending on the key, so this
 * fills in the right data. XXX should this be extended to them all and get
 * rid of std/NULL rubbish?
 */
void
cmd_select_window_default(void **ptr, int key)
{
	struct cmd_select_window_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	data->idx = key - '0';
}

int
cmd_select_window_parse(void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_select_window_data	*data;
	const char			*errstr;
	int				 opt;

	*ptr = data = xmalloc(sizeof *data);

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

	data->idx = strtonum(argv[0], 0, UINT_MAX, &errstr);
	if (errstr != NULL) {
		xasprintf(cause, "index %s", errstr);
		goto error;
	}
	
	return (0);

usage:
	usage(cause, "%s %s",
	    cmd_select_window_entry.name, cmd_select_window_entry.usage);

error:
	cmd_select_window_free(data);
	return (-1);
}

void
cmd_select_window_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct cmd_select_window_data	*data = ptr;
	struct client			*c = ctx->client;
	struct session			*s = ctx->session;

	if (data == NULL)
		return;

	if (session_select(s, data->idx) == 0)
		server_redraw_session(s);
	else
		ctx->error(ctx, "no window %u", data->idx);
	
	if (!(ctx->flags & CMD_KEY))
		server_write_client(c, MSG_EXIT, NULL, 0);
}

void
cmd_select_window_send(void *ptr, struct buffer *b)
{
	struct cmd_select_window_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
}

void
cmd_select_window_recv(void **ptr, struct buffer *b)
{
	struct cmd_select_window_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
}

void
cmd_select_window_free(void *ptr)
{
	struct cmd_select_window_data	*data = ptr;

	xfree(data);
}
