/* $Id: cmd-generic.c,v 1.6 2008-06-05 16:35:31 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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

int
cmd_clientonly_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_clientonly_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;

	while ((opt = getopt(argc, argv, "c:")) != EOF) {
		switch (opt) {
		case 'c':
			if (data->cname == NULL)
				data->cname = xstrdup(optarg);
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

void
cmd_clientonly_send(struct cmd *self, struct buffer *b)
{
	struct cmd_clientonly_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
}

void
cmd_clientonly_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_clientonly_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
}

void
cmd_clientonly_free(struct cmd *self)
{
	struct cmd_clientonly_data	*data = self->data;

	if (data->cname != NULL)
		xfree(data->cname);
	xfree(data);
}

struct client *
cmd_clientonly_get(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_clientonly_data	*data = self->data;

  	if (data != NULL)
		return (cmd_find_client(ctx, data->cname));
	return (cmd_find_client(ctx, NULL));
}

void
cmd_clientonly_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_clientonly_data	*data = self->data;
	size_t				 off = 0;
	
	off += xsnprintf(buf, len, "%s ", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->cname != NULL)
		off += xsnprintf(buf + off, len - off, "-c %s ", data->cname);
}

int
cmd_sessiononly_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_sessiononly_data	*data;
	int				 opt;
	
	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;

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
	if (argc != 0)
		goto usage;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_sessiononly_send(struct cmd *self, struct buffer *b)
{
	struct cmd_sessiononly_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->sname);
}

void
cmd_sessiononly_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_sessiononly_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->sname = cmd_recv_string(b);
}

void
cmd_sessiononly_free(struct cmd *self)
{
	struct cmd_sessiononly_data	*data = self->data;

	if (data->cname != NULL)
		xfree(data->cname);
	if (data->sname != NULL)
		xfree(data->sname);
	xfree(data);
}

struct session *
cmd_sessiononly_get(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_sessiononly_data	*data = self->data;

  	if (data != NULL)
		return (cmd_find_session(ctx, data->cname, data->sname));
	return (cmd_find_session(ctx, NULL, NULL));
}

void
cmd_sessiononly_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_sessiononly_data	*data = self->data;
	size_t				 off = 0;
	
	off += xsnprintf(buf, len, "%s ", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->cname != NULL)
		off += xsnprintf(buf + off, len - off, "-c %s ", data->cname);
	if (off < len && data->sname != NULL)
		off += xsnprintf(buf + off, len - off, "-s %s ", data->sname);
}

int
cmd_windowonly_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_windowonly_data	*data;
	int				 opt;
	const char			*errstr;

	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;
	data->idx = -1;

	while ((opt = getopt(argc, argv, "c:i:s:")) != EOF) {
		switch (opt) {
		case 'c':
			if (data->sname != NULL)
				goto usage;
			if (data->cname == NULL)
				data->cname = xstrdup(optarg);
			break;
		case 'i':
			data->idx = strtonum(optarg, 0, INT_MAX, &errstr);
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
	if (argc != 0)
		goto usage;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

void
cmd_windowonly_send(struct cmd *self, struct buffer *b)
{
	struct cmd_windowonly_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->sname);
}

void
cmd_windowonly_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_windowonly_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->sname = cmd_recv_string(b);
}

void
cmd_windowonly_free(struct cmd *self)
{
	struct cmd_windowonly_data	*data = self->data;

	if (data->cname != NULL)
		xfree(data->cname);
	if (data->sname != NULL)
		xfree(data->sname);
	xfree(data);
}

struct winlink *
cmd_windowonly_get(struct cmd *self, struct cmd_ctx *ctx, struct session **sp)
{
	struct cmd_windowonly_data	*data = self->data;
	struct winlink			*wl;

  	if (data == NULL) {
		wl = cmd_find_window(ctx, NULL, NULL, -1, sp);
		return (wl);
	}
	
	return (cmd_find_window(ctx, data->cname, data->sname, data->idx, sp));
}

void
cmd_windowonly_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_windowonly_data	*data = self->data;
	size_t				 off = 0;
	
	off += xsnprintf(buf, len, "%s ", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->cname != NULL)
		off += xsnprintf(buf + off, len - off, "-c %s ", data->cname);
	if (off < len && data->sname != NULL)
		off += xsnprintf(buf + off, len - off, "-s %s ", data->sname);
	if (off < len && data->idx != -1)
		off += xsnprintf(buf + off, len - off, "-i %d ", data->idx);
}
