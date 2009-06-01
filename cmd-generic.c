/* $OpenBSD$ */

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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

#define CMD_FLAGS "adDgkuU"
#define CMD_FLAGMASK (CMD_AFLAG|CMD_DFLAG|CMD_BIGDFLAG|CMD_GFLAG|CMD_KFLAG| \
    CMD_UFLAG|CMD_BIGUFLAG)

int	cmd_do_flags(int, int, int *);
size_t	cmd_print_flags(char *, size_t, size_t, int);
int	cmd_fill_argument(int, char **, int, char **);

size_t
cmd_prarg(char *buf, size_t len, const char *prefix, char *arg)
{
	if (strchr(arg, ' ') != NULL)
		return (xsnprintf(buf, len, "%s\"%s\"", prefix, arg));
	return (xsnprintf(buf, len, "%s%s", prefix, arg));
}

int
cmd_do_flags(int opt, int iflags, int *oflags)
{
	switch (opt) {
	case 'a':
		if (iflags & CMD_AFLAG) {
			(*oflags) |= CMD_AFLAG;
			return (0);
		}
		return (-1);
	case 'd':
		if (iflags & CMD_DFLAG) {
			(*oflags) |= CMD_DFLAG;
			return (0);
		}
		return (-1);
	case 'D':
		if (iflags & CMD_BIGDFLAG) {
			(*oflags) |= CMD_BIGDFLAG;
			return (0);
		}
		return (-1);
	case 'g':
		if (iflags & CMD_GFLAG) {
			(*oflags) |= CMD_GFLAG;
			return (0);
		}
		return (-1);
	case 'k':
		if (iflags & CMD_KFLAG) {
			(*oflags) |= CMD_KFLAG;
			return (0);
		}
		return (-1);
	case 'u':
		if (iflags & CMD_UFLAG) {
			(*oflags) |= CMD_UFLAG;
			return (0);
		}
		return (-1);
	case 'U':
		if (iflags & CMD_BIGUFLAG) {
			(*oflags) |= CMD_BIGUFLAG;
			return (0);
		}
		return (-1);
	}
	return (1);
}

size_t
cmd_print_flags(char *buf, size_t len, size_t off, int flags)
{
	size_t	boff = off;

	if ((flags & CMD_FLAGMASK) == 0)
		return (0);
	off += xsnprintf(buf + off, len - off, " -");
	if (off < len && flags & CMD_AFLAG)
		off += xsnprintf(buf + off, len - off, "a");
	if (off < len && flags & CMD_BIGDFLAG)
		off += xsnprintf(buf + off, len - off, "D");
	if (off < len && flags & CMD_DFLAG)
		off += xsnprintf(buf + off, len - off, "d");
	if (off < len && flags & CMD_GFLAG)
		off += xsnprintf(buf + off, len - off, "g");
	if (off < len && flags & CMD_KFLAG)
		off += xsnprintf(buf + off, len - off, "k");
	if (off < len && flags & CMD_UFLAG)
		off += xsnprintf(buf + off, len - off, "u");
	if (off < len && flags & CMD_BIGUFLAG)
		off += xsnprintf(buf + off, len - off, "U");
	return (off - boff);
}

int
cmd_fill_argument(int flags, char **arg, int argc, char **argv)
{
	*arg = NULL;

	if (flags & CMD_ARG1) {
		if (argc != 1)
			return (-1);
		*arg = xstrdup(argv[0]);
		return (0);
	}

	if (flags & CMD_ARG01) {
		if (argc != 0 && argc != 1)
			return (-1);
		if (argc == 1)
			*arg = xstrdup(argv[0]);
		return (0);
	}

	if (argc != 0)
		return (-1);
	return (0);
}

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

	while ((opt = getopt(argc, argv, CMD_FLAGS "t:")) != -1) {
		switch (cmd_do_flags(opt, self->entry->flags, &data->flags)) {
		case -1:
			goto usage;
		case 0:
			continue;
		}
		switch (opt) {
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

	if (cmd_fill_argument(self->entry->flags, &data->arg, argc, argv) != 0)
		goto usage;
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

size_t
cmd_target_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_target_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	off += cmd_print_flags(buf, len, off, data->flags);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
 	if (off < len && data->arg != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg);
	return (off);
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

	while ((opt = getopt(argc, argv, CMD_FLAGS "s:t:")) != -1) {
		switch (cmd_do_flags(opt, self->entry->flags, &data->flags)) {
		case -1:
			goto usage;
		case 0:
			continue;
		}
		switch (opt) {
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

	if (cmd_fill_argument(self->entry->flags, &data->arg, argc, argv) != 0)
		goto usage;
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

size_t
cmd_srcdst_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_srcdst_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	off += cmd_print_flags(buf, len, off, data->flags);
	if (off < len && data->src != NULL)
		off += xsnprintf(buf + off, len - off, " -s %s", data->src);
	if (off < len && data->dst != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->dst);
 	if (off < len && data->arg != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg);
	return (off);
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

	while ((opt = getopt(argc, argv, CMD_FLAGS "b:t:")) != -1) {
		switch (cmd_do_flags(opt, self->entry->flags, &data->flags)) {
		case -1:
			goto usage;
		case 0:
			continue;
		}
		switch (opt) {
		case 'b':
			if (data->buffer == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "buffer %s", errstr);
					goto error;
				}
				data->buffer = n;
			}
			break;
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

	if (cmd_fill_argument(self->entry->flags, &data->arg, argc, argv) != 0)
		goto usage;
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

size_t
cmd_buffer_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_buffer_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	off += cmd_print_flags(buf, len, off, data->flags);
	if (off < len && data->buffer != -1)
		off += xsnprintf(buf + off, len - off, " -b %d", data->buffer);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
 	if (off < len && data->arg != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg);
	return (off);
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

	while ((opt = getopt(argc, argv, CMD_FLAGS "t:")) != -1) {
		switch (cmd_do_flags(opt, self->entry->flags, &data->flags)) {
		case -1:
			goto usage;
		case 0:
			continue;
		}
		switch (opt) {
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

size_t
cmd_option_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_option_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	off += cmd_print_flags(buf, len, off, data->flags);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
 	if (off < len && data->option != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->option);
 	if (off < len && data->value != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->value);
	return (off);
}

void
cmd_pane_init(struct cmd *self, unused int key)
{
	struct cmd_pane_data	*data;

	self->data = data = xmalloc(sizeof *data);
	data->flags = 0;
	data->target = NULL;
	data->arg = NULL;
	data->pane = -1;
}

int
cmd_pane_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_pane_data	*data;
	int			 opt, n;
	const char		*errstr;

	/* Don't use the entry version since it may be dependent on key. */
	cmd_pane_init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, CMD_FLAGS "p:t:")) != -1) {
		switch (cmd_do_flags(opt, self->entry->flags, &data->flags)) {
		case -1:
			goto usage;
		case 0:
			continue;
		}
		switch (opt) {
		case 'p':
			if (data->pane == -1) {
				n = strtonum(optarg, 0, INT_MAX, &errstr);
				if (errstr != NULL) {
					xasprintf(cause, "pane %s", errstr);
					goto error;
				}
				data->pane = n;
			}
			break;
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

	if (cmd_fill_argument(self->entry->flags, &data->arg, argc, argv) != 0)
		goto usage;
	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

void
cmd_pane_send(struct cmd *self, struct buffer *b)
{
	struct cmd_pane_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
	cmd_send_string(b, data->arg);
}

void
cmd_pane_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_pane_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
	data->arg = cmd_recv_string(b);
}

void
cmd_pane_free(struct cmd *self)
{
	struct cmd_pane_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->arg != NULL)
		xfree(data->arg);
	xfree(data);
}

size_t
cmd_pane_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_pane_data	*data = self->data;
	size_t			 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	off += cmd_print_flags(buf, len, off, data->flags);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
 	if (off < len && data->arg != NULL)
		off += cmd_prarg(buf + off, len - off, " ", data->arg);
	return (off);
}
