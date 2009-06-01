/* $OpenBSD$ */

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

#include "tmux.h"

/*
 * Bind a key to a command, this recurses through cmd_*.
 */

int	cmd_bind_key_parse(struct cmd *, int, char **, char **);
int	cmd_bind_key_exec(struct cmd *, struct cmd_ctx *);
void	cmd_bind_key_send(struct cmd *, struct buffer *);
void	cmd_bind_key_recv(struct cmd *, struct buffer *);
void	cmd_bind_key_free(struct cmd *);
size_t	cmd_bind_key_print(struct cmd *, char *, size_t);

struct cmd_bind_key_data {
	int		 key;
	int		 can_repeat;
	struct cmd_list	*cmdlist;
};

const struct cmd_entry cmd_bind_key_entry = {
	"bind-key", "bind",
	"[-r] key command [arguments]",
	0,
	NULL,
	cmd_bind_key_parse,
	cmd_bind_key_exec,
	cmd_bind_key_send,
	cmd_bind_key_recv,
	cmd_bind_key_free,
	cmd_bind_key_print
};

int
cmd_bind_key_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_bind_key_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->can_repeat = 0;
	data->cmdlist = NULL;

	while ((opt = getopt(argc, argv, "r")) != -1) {
		switch (opt) {
		case 'r':
			data->can_repeat = 1;
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		goto usage;

	if ((data->key = key_string_lookup_string(argv[0])) == KEYC_NONE) {
		xasprintf(cause, "unknown key: %s", argv[0]);
		goto error;
	}

	argc--;
	argv++;
	if ((data->cmdlist = cmd_list_parse(argc, argv, cause)) == NULL)
		goto error;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	self->entry->free(self);
	return (-1);
}

int
cmd_bind_key_exec(struct cmd *self, unused struct cmd_ctx *ctx)
{
	struct cmd_bind_key_data	*data = self->data;

	if (data == NULL)
		return (0);

	key_bindings_add(data->key, data->can_repeat, data->cmdlist);
	data->cmdlist = NULL;	/* avoid free */

	return (0);
}

void
cmd_bind_key_send(struct cmd *self, struct buffer *b)
{
	struct cmd_bind_key_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_list_send(data->cmdlist, b);
}

void
cmd_bind_key_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_bind_key_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cmdlist = cmd_list_recv(b);
}

void
cmd_bind_key_free(struct cmd *self)
{
	struct cmd_bind_key_data	*data = self->data;

	if (data->cmdlist != NULL)
		cmd_list_free(data->cmdlist);
	xfree(data);
}

size_t
cmd_bind_key_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_bind_key_data	*data = self->data;
	size_t				 off = 0;
	const char			*skey;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len) {
		skey = key_string_lookup_key(data->key);
		off += xsnprintf(buf + off, len - off, " %s ", skey);
	}
	if (off < len)
		off += cmd_list_print(data->cmdlist, buf + off, len - off);
	return (off);
}
