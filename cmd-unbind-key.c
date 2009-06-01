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
 * Unbind key from command.
 */

int	cmd_unbind_key_parse(struct cmd *, int, char **, char **);
int	cmd_unbind_key_exec(struct cmd *, struct cmd_ctx *);
void	cmd_unbind_key_send(struct cmd *, struct buffer *);
void	cmd_unbind_key_recv(struct cmd *, struct buffer *);
void	cmd_unbind_key_free(struct cmd *);

struct cmd_unbind_key_data {
	int	key;
};

const struct cmd_entry cmd_unbind_key_entry = {
	"unbind-key", "unbind",
	"key",
	0,
	NULL,
	cmd_unbind_key_parse,
	cmd_unbind_key_exec,
	cmd_unbind_key_send,
	cmd_unbind_key_recv,
	cmd_unbind_key_free,
	NULL
};

int
cmd_unbind_key_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_unbind_key_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);

	while ((opt = getopt(argc, argv, "")) != -1) {
		switch (opt) {
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		goto usage;

	if ((data->key = key_string_lookup_string(argv[0])) == KEYC_NONE) {
		xasprintf(cause, "unknown key: %s", argv[0]);
		goto error;
	}

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	xfree(data);
	return (-1);
}

int
cmd_unbind_key_exec(struct cmd *self, unused struct cmd_ctx *ctx)
{
	struct cmd_unbind_key_data	*data = self->data;

	if (data == NULL)
		return (0);

	key_bindings_remove(data->key);

	return (0);
}

void
cmd_unbind_key_send(struct cmd *self, struct buffer *b)
{
	struct cmd_unbind_key_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
}

void
cmd_unbind_key_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_unbind_key_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
}

void
cmd_unbind_key_free(struct cmd *self)
{
	struct cmd_unbind_key_data	*data = self->data;

	xfree(data);
}
