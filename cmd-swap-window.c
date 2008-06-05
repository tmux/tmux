/* $Id: cmd-swap-window.c,v 1.9 2008-06-05 16:35:32 nicm Exp $ */

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
 * Swap one window with another.
 */

int	cmd_swap_window_parse(struct cmd *, int, char **, char **);
void	cmd_swap_window_exec(struct cmd *, struct cmd_ctx *);
void	cmd_swap_window_send(struct cmd *, struct buffer *);
void	cmd_swap_window_recv(struct cmd *, struct buffer *);
void	cmd_swap_window_free(struct cmd *);

struct cmd_swap_window_data {
	char	*cname;
	char	*sname;
	int	 dstidx;
	int	 srcidx;
	char	*srcname;
	int	 flag_detached;
};

const struct cmd_entry cmd_swap_window_entry = {
	"swap-window", "swapw",
	"[-c client-tty|-s session-name] [-i index] session-name index",
	0,
	cmd_swap_window_parse,
	cmd_swap_window_exec,
	cmd_swap_window_send,
	cmd_swap_window_recv,
	cmd_swap_window_free,
	NULL,
	NULL
};

int
cmd_swap_window_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_swap_window_data	*data;
	const char			*errstr;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;
	data->flag_detached = 0;
	data->dstidx = -1;
	data->srcidx = -1;
	data->srcname = NULL;

	while ((opt = getopt(argc, argv, "c:di:s:")) != EOF) {
		switch (opt) {
		case 'c':
			if (data->sname != NULL)
				goto usage;
			if (data->cname == NULL)
				data->cname = xstrdup(optarg);
			break;
		case 'd':
			data->flag_detached = 1;
			break;
		case 'i':
			data->dstidx = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				xasprintf(cause, "index %s", errstr);
				goto error;
			}
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
	if (argc != 2)
		goto usage;

	data->srcname = xstrdup(argv[0]);
	data->srcidx = strtonum(argv[1], 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		xasprintf(cause, "index %s", errstr);
		goto error;
	}

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

void
cmd_swap_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_swap_window_data	*data = self->data;
	struct session			*s, *src;
	struct winlink			*srcwl, *dstwl;
	struct window			*w;

	if (data == NULL)
		return;

	if ((s = cmd_find_session(ctx, data->cname, data->sname)) == NULL)
		return;

	if ((src = session_find(data->srcname)) == NULL) {
		ctx->error(ctx, "session not found: %s", data->srcname);
		return;
	}

	if (data->srcidx < 0)
		data->srcidx = -1;
	if (data->srcidx == -1)
		srcwl = src->curw;
	else {
		srcwl = winlink_find_by_index(&src->windows, data->srcidx);
		if (srcwl == NULL) {
			ctx->error(ctx, "no window %d", data->srcidx);
			return;
		}
	}

	if (data->dstidx < 0)
		data->dstidx = -1;
	if (data->dstidx == -1)
		dstwl = s->curw;
	else {
		dstwl = winlink_find_by_index(&s->windows, data->dstidx);
		if (dstwl == NULL) {
			ctx->error(ctx, "no window %d", data->dstidx);
			return;
		}
	}

	w = dstwl->window;
	dstwl->window = srcwl->window;
	srcwl->window = w;

	if (!data->flag_detached) {
		session_select(s, dstwl->idx);
		if (src != s)
			session_select(src, srcwl->idx);
	}
	server_redraw_session(src);
	if (src != s)
		server_redraw_session(s);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_swap_window_send(struct cmd *self, struct buffer *b)
{
	struct cmd_swap_window_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->sname);
	cmd_send_string(b, data->srcname);
}

void
cmd_swap_window_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_swap_window_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->sname = cmd_recv_string(b);
	data->srcname = cmd_recv_string(b);
}

void
cmd_swap_window_free(struct cmd *self)
{
	struct cmd_swap_window_data	*data = self->data;

	if (data->cname != NULL)
		xfree(data->cname);
	if (data->sname != NULL)
		xfree(data->sname);
	if (data->srcname != NULL)
		xfree(data->srcname);
	xfree(data);
}
