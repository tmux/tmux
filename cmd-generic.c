/* $Id: cmd-generic.c,v 1.14 2008-12-08 16:19:51 nicm Exp $ */

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

void
cmd_target_init(struct cmd *self, unused int key)
{
	struct cmd_target_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->flags = 0;
	data->target = NULL;
	data->arg = NULL;
}

int
cmd_target_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_target_data	*data;
	int			 opt;

	/* Don't use the entry version since it may be dependent on key. */
	cmd_target_init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, GETOPT_PREFIX "dgkt:")) != EOF) {
		switch (opt) {
		case 'd':
			if (self->entry->flags & CMD_DFLAG) {
				data->flags |= CMD_DFLAG;
				break;
			}
			goto usage;
		case 'g':
			if (self->entry->flags & CMD_GFLAG) {
				data->flags |= CMD_GFLAG;
				break;
			}
			goto usage;
		case 'k':
			if (self->entry->flags & CMD_KFLAG) {
				data->flags |= CMD_KFLAG;
				break;
			}
			goto usage;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (self->entry->flags & CMD_ONEARG) {
		if (argc != 1)
			goto usage;
		data->arg = xstrdup(argv[0]);
	} else if (self->entry->flags & CMD_ZEROONEARG) {
		if (argc == 0)
			data->arg = NULL;
		else if (argc == 1)
			data->arg = xstrdup(argv[0]);
		else
			goto usage;
	} else {
		if (argc != 0)
			goto usage;
	}
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_target_send(struct cmd *self, struct buffer *b)
{
	struct cmd_target_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
	cmd_send_string(b, data->arg);
}

void
cmd_target_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_target_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
	data->arg = cmd_recv_string(b);
}

void
cmd_target_free(struct cmd *self)
{
	struct cmd_target_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->arg != NULL)
		xfree(data->arg);
	xfree(data);
}

void
cmd_target_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_target_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->flags & CMD_DFLAG)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->flags & CMD_GFLAG)
		off += xsnprintf(buf + off, len - off, " -g");
	if (off < len && data->flags & CMD_KFLAG)
		off += xsnprintf(buf + off, len - off, " -k");
	if (off < len && data->target != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->target);
 	if (off < len && data->arg != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->arg);
}

void
cmd_srcdst_init(struct cmd *self, unused int key)
{
	struct cmd_srcdst_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->flags = 0;
	data->src = NULL;
	data->dst = NULL;
	data->arg = NULL;
}

int
cmd_srcdst_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_srcdst_data	*data;
	int			 opt;

	cmd_srcdst_init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, GETOPT_PREFIX "dgks:t:")) != EOF) {
		switch (opt) {
		case 'd':
			if (self->entry->flags & CMD_DFLAG) {
				data->flags |= CMD_DFLAG;
				break;
			}
			goto usage;
		case 'g':
			if (self->entry->flags & CMD_GFLAG) {
				data->flags |= CMD_GFLAG;
				break;
			}
			goto usage;
		case 'k':
			if (self->entry->flags & CMD_KFLAG) {
				data->flags |= CMD_KFLAG;
				break;
			}
			goto usage;
		case 's':
			if (data->src == NULL)
				data->src = xstrdup(optarg);
			break;
		case 't':
			if (data->dst == NULL)
				data->dst = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (self->entry->flags & CMD_ONEARG) {
		if (argc != 1)
			goto usage;
		data->arg = xstrdup(argv[0]);
	} else if (self->entry->flags & CMD_ZEROONEARG) {
		if (argc == 0)
			data->arg = NULL;
		else if (argc == 1)
			data->arg = xstrdup(argv[0]);
		else
			goto usage;
	} else {
		if (argc != 0)
			goto usage;
	}
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_srcdst_send(struct cmd *self, struct buffer *b)
{
	struct cmd_srcdst_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->src);
	cmd_send_string(b, data->dst);
	cmd_send_string(b, data->arg);
}

void
cmd_srcdst_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_srcdst_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->src = cmd_recv_string(b);
	data->dst = cmd_recv_string(b);
	data->arg = cmd_recv_string(b);
}

void
cmd_srcdst_free(struct cmd *self)
{
	struct cmd_srcdst_data	*data = self->data;

	if (data->src != NULL)
		xfree(data->src);
	if (data->dst != NULL)
		xfree(data->dst);
	if (data->arg != NULL)
		xfree(data->arg);
	xfree(data);
}

void
cmd_srcdst_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_srcdst_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->flags & CMD_DFLAG)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->flags & CMD_GFLAG)
		off += xsnprintf(buf + off, len - off, " -g");
	if (off < len && data->flags & CMD_KFLAG)
		off += xsnprintf(buf + off, len - off, " -k");
	if (off < len && data->src != NULL)
		off += xsnprintf(buf + off, len - off, " -s %s", data->src);
	if (off < len && data->dst != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->dst);
 	if (off < len && data->arg != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->arg);
}

void
cmd_buffer_init(struct cmd *self, unused int key)
{
	struct cmd_buffer_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->flags = 0;
	data->target = NULL;
	data->buffer = -1;
	data->arg = NULL;
}

int
cmd_buffer_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_buffer_data	*data;
	int			 opt, n;
	const char		*errstr;

	cmd_buffer_init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, GETOPT_PREFIX "b:dgkt:")) != EOF) {
		switch (opt) {
		case 'b':
			if (data->buffer == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(
					    cause, "buffer index %s", errstr);
					goto error;
				}
				data->buffer = n;
			}
			break;
		case 'd':
			if (self->entry->flags & CMD_DFLAG) {
				data->flags |= CMD_DFLAG;
				break;
			}
			goto usage;
		case 'g':
			if (self->entry->flags & CMD_GFLAG) {
				data->flags |= CMD_GFLAG;
				break;
			}
			goto usage;
		case 'k':
			if (self->entry->flags & CMD_KFLAG) {
				data->flags |= CMD_KFLAG;
				break;
			}
			goto usage;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (self->entry->flags & CMD_ONEARG) {
		if (argc != 1)
			goto usage;
		data->arg = xstrdup(argv[0]);
	} else if (self->entry->flags & CMD_ZEROONEARG) {
		if (argc == 0)
			data->arg = NULL;
		else if (argc == 1)
			data->arg = xstrdup(argv[0]);
		else
			goto usage;
	} else {
		if (argc != 0)
			goto usage;
	}
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

void
cmd_buffer_send(struct cmd *self, struct buffer *b)
{
	struct cmd_buffer_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
	cmd_send_string(b, data->arg);
}

void
cmd_buffer_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_buffer_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
	data->arg = cmd_recv_string(b);
}

void
cmd_buffer_free(struct cmd *self)
{
	struct cmd_buffer_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->arg != NULL)
		xfree(data->arg);
	xfree(data);
}

void
cmd_buffer_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_buffer_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->flags & CMD_DFLAG)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->flags & CMD_GFLAG)
		off += xsnprintf(buf + off, len - off, " -g");
	if (off < len && data->flags & CMD_KFLAG)
		off += xsnprintf(buf + off, len - off, " -k");
	if (off < len && data->buffer != -1)
		off += xsnprintf(buf + off, len - off, " -b %d", data->buffer);
	if (off < len && data->target != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->target);
 	if (off < len && data->arg != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->arg);
}

void
cmd_option_init(struct cmd *self, unused int key)
{
	struct cmd_option_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->flags = 0;
	data->target = NULL;
	data->option = NULL;
	data->value = NULL;
}

int
cmd_option_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_option_data	*data;
	int			 opt;

	/* Don't use the entry version since it may be dependent on key. */
	cmd_option_init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, GETOPT_PREFIX "dgkt:")) != EOF) {
		switch (opt) {
		case 'd':
			if (self->entry->flags & CMD_DFLAG) {
				data->flags |= CMD_DFLAG;
				break;
			}
			goto usage;
		case 'g':
			if (self->entry->flags & CMD_GFLAG) {
				data->flags |= CMD_GFLAG;
				break;
			}
			goto usage;
		case 'k':
			if (self->entry->flags & CMD_KFLAG) {
				data->flags |= CMD_KFLAG;
				break;
			}
			goto usage;
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 2) {
		data->option = xstrdup(argv[0]);
		data->value = xstrdup(argv[1]);
	} else if (argc == 1)
		data->option = xstrdup(argv[0]);
	else
		goto usage;
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_option_send(struct cmd *self, struct buffer *b)
{
	struct cmd_option_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
	cmd_send_string(b, data->option);
	cmd_send_string(b, data->value);
}

void
cmd_option_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_option_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
	data->option = cmd_recv_string(b);
	data->value = cmd_recv_string(b);
}

void
cmd_option_free(struct cmd *self)
{
	struct cmd_option_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->option != NULL)
		xfree(data->option);
	if (data->value != NULL)
		xfree(data->value);
	xfree(data);
}

void
cmd_option_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_option_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->flags & CMD_DFLAG)
		off += xsnprintf(buf + off, len - off, " -d");
	if (off < len && data->flags & CMD_GFLAG)
		off += xsnprintf(buf + off, len - off, " -g");
	if (off < len && data->flags & CMD_KFLAG)
		off += xsnprintf(buf + off, len - off, " -k");
	if (off < len && data->target != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->target);
 	if (off < len && data->option != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->option);
 	if (off < len && data->value != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->value);
}
