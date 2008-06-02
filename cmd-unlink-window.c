/* $Id: cmd-unlink-window.c,v 1.6 2008-06-02 18:08:16 nicm Exp $ */

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
 * Unlink a window, unless it would be destroyed by doing so (only one link).
 */

int	cmd_unlink_window_parse(struct cmd *, void **, int, char **, char **);
void	cmd_unlink_window_exec(void *, struct cmd_ctx *);
void	cmd_unlink_window_send(void *, struct buffer *);
void	cmd_unlink_window_recv(void **, struct buffer *);
void	cmd_unlink_window_free(void *);

struct cmd_unlink_window_data {
	char	*sname;
	int	 idx;
};

const struct cmd_entry cmd_unlink_window_entry = {
	"unlink-window", "unlinkw",
	"[-i index] [-s session-name]",
	0,
	cmd_unlink_window_parse,
	cmd_unlink_window_exec,
	cmd_unlink_window_send,
	cmd_unlink_window_recv,
	cmd_unlink_window_free
};

int
cmd_unlink_window_parse(
    struct cmd *self, void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_unlink_window_data	*data;
	const char			*errstr;
	int				 opt;

	*ptr = data = xmalloc(sizeof *data);
	data->sname = NULL;
	data->idx = -1;

	while ((opt = getopt(argc, argv, "i:s:")) != EOF) {
		switch (opt) {
		case 'i':
			data->idx = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				xasprintf(cause, "index %s", errstr);
				goto error;
			}
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
	usage(cause, "%s %s", self->entry->name, self->entry->usage);

error:
	cmd_unlink_window_free(data);
	return (-1);
}

void
cmd_unlink_window_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct cmd_unlink_window_data	*data = ptr;
	struct session			*s;
	struct client			*c;
	struct winlink			*wl;
	u_int		 		 i;
	int		 		 destroyed;

	if (data == NULL)
		return;

	if ((s = cmd_find_session(ctx, data->sname)) == NULL)
		return;

	if (data->idx < 0)
		data->idx = -1;
	if (data->idx == -1)
		wl = s->curw;
	else {
		wl = winlink_find_by_index(&s->windows, data->idx);
		if (wl == NULL) {
			ctx->error(ctx, "no window %d", data->idx);
			return;
		}
	}

	if (wl->window->references == 1) {
		ctx->error(ctx, "window is only linked to one session");
		return;
	}

 	destroyed = session_detach(s, wl);
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session != s)
			continue;
		if (destroyed) {
			c->session = NULL;
			server_write_client(c, MSG_EXIT, NULL, 0);
		} else
			server_redraw_client(c);
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_unlink_window_send(void *ptr, struct buffer *b)
{
	struct cmd_unlink_window_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->sname);
}

void
cmd_unlink_window_recv(void **ptr, struct buffer *b)
{
	struct cmd_unlink_window_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
}

void
cmd_unlink_window_free(void *ptr)
{
	struct cmd_unlink_window_data	*data = ptr;

	if (data->sname != NULL)
		xfree(data->sname);
	xfree(data);
}
