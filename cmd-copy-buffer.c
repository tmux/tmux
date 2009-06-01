/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Tiago Cunha <me@tiagocunha.org>
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

#include <stdlib.h>

#include "tmux.h"

/*
 * Copies a session paste buffer to another session.
 */

int	cmd_copy_buffer_parse(struct cmd *, int, char **, char **);
int	cmd_copy_buffer_exec(struct cmd *, struct cmd_ctx *);
void	cmd_copy_buffer_send(struct cmd *, struct buffer *);
void	cmd_copy_buffer_recv(struct cmd *, struct buffer *);
void	cmd_copy_buffer_free(struct cmd *);
void	cmd_copy_buffer_init(struct cmd *, int);
size_t	cmd_copy_buffer_print(struct cmd *, char *, size_t);

struct cmd_copy_buffer_data {
	char	*dst_session;
	char	*src_session;
	int	 dst_idx;
	int	 src_idx;
};

const struct cmd_entry cmd_copy_buffer_entry = {
	"copy-buffer", "copyb",
	"[-a src-index] [-b dst-index] [-s src-session] [-t dst-session]",
	0,
	cmd_copy_buffer_init,
	cmd_copy_buffer_parse,
	cmd_copy_buffer_exec,
	cmd_copy_buffer_send,
	cmd_copy_buffer_recv,
	cmd_copy_buffer_free,
	cmd_copy_buffer_print
};

void
cmd_copy_buffer_init(struct cmd *self, unused int arg)
{
	struct cmd_copy_buffer_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->dst_session = NULL;
	data->src_session = NULL;
	data->dst_idx = -1;
	data->src_idx = -1;
}

int
cmd_copy_buffer_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_copy_buffer_data	*data;
	const char			*errstr;
	int				 n, opt;

	self->entry->init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "a:b:s:t:")) != -1) {
		switch (opt) {
		case 'a':
			if (data->src_idx == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "buffer %s", errstr);
					goto error;
				}
				data->src_idx = n;
			}
			break;
		case 'b':
			if (data->dst_idx == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "buffer %s", errstr);
					goto error;
				}
				data->dst_idx = n;
			}
			break;
		case 's':
			if (data->src_session == NULL)
				data->src_session = xstrdup(optarg);
			break;
		case 't':
			if (data->dst_session == NULL)
				data->dst_session = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

int
cmd_copy_buffer_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_copy_buffer_data	*data = self->data;
	struct paste_buffer		*pb;
	struct session			*dst_session, *src_session;
	u_int				 limit;

	if ((dst_session = cmd_find_session(ctx, data->dst_session)) == NULL ||
	    (src_session = cmd_find_session(ctx, data->src_session)) == NULL)
	    	return (-1);

	if (data->src_idx == -1) {
		if ((pb = paste_get_top(&src_session->buffers)) == NULL) {
			ctx->error(ctx, "no buffers");
			return (-1);
		}
	} else {
		if ((pb = paste_get_index(&src_session->buffers,
		    data->src_idx)) == NULL) {
		    	ctx->error(ctx, "no buffer %d", data->src_idx);
		    	return (-1);
		}
	}

	limit = options_get_number(&dst_session->options, "buffer-limit");
	if (data->dst_idx == -1) {
		paste_add(&dst_session->buffers, xstrdup(pb->data), limit);
		return (0);
	}
	if (paste_replace(&dst_session->buffers, data->dst_idx,
	    xstrdup(pb->data)) != 0) {
		ctx->error(ctx, "no buffer %d", data->dst_idx);
		return (-1);
	}

	return (0);
}

void
cmd_copy_buffer_send(struct cmd *self, struct buffer *b)
{
	struct cmd_copy_buffer_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->dst_session);
	cmd_send_string(b, data->src_session);
}

void
cmd_copy_buffer_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_copy_buffer_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->dst_session = cmd_recv_string(b);
	data->src_session = cmd_recv_string(b);
}

void
cmd_copy_buffer_free(struct cmd *self)
{
	struct cmd_copy_buffer_data	*data = self->data;

	if (data->dst_session != NULL)
		xfree(data->dst_session);
	if (data->src_session != NULL)
		xfree(data->src_session);
	xfree(data);
}

size_t
cmd_copy_buffer_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_copy_buffer_data	*data = self->data;
	size_t				off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->src_idx != -1) {
		off += xsnprintf(buf + off, len - off, " -a %d",
				 data->src_idx);
	}
	if (off < len && data->dst_idx != -1) {
		off += xsnprintf(buf + off, len - off, " -b %d",
				 data->dst_idx);
	}
	if (off < len && data->src_session != NULL) {
		off += cmd_prarg(buf + off, len - off, " -s ",
				 data->src_session);
	}
	if (off < len && data->dst_session != NULL) {
		off += cmd_prarg(buf + off, len - off, " -t ",
				 data->dst_session);
	}
	return (off);
}
