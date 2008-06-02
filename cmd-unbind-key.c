/* $Id: cmd-unbind-key.c,v 1.9 2008-06-02 18:08:16 nicm Exp $ */

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
 * Unbind key from command.
 */

int	cmd_unbind_key_parse(struct cmd *, void **, int, char **, char **);
void	cmd_unbind_key_exec(void *, struct cmd_ctx *);
void	cmd_unbind_key_send(void *, struct buffer *);
void	cmd_unbind_key_recv(void **, struct buffer *);
void	cmd_unbind_key_free(void *);

struct cmd_unbind_key_data {
	int		 key;
};

const struct cmd_entry cmd_unbind_key_entry = {
	"unbind-key", "unbind",
	"key",
	0,
	cmd_unbind_key_parse,
	cmd_unbind_key_exec,
	cmd_unbind_key_send,
	cmd_unbind_key_recv,
	cmd_unbind_key_free
};

int
cmd_unbind_key_parse(
    struct cmd *self, void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_unbind_key_data	*data;
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

	if ((data->key = key_string_lookup_string(argv[0])) == KEYC_NONE) {
		xasprintf(cause, "unknown key: %s", argv[0]);
		goto error;
	}

	return (0);

usage:
	usage(cause, "%s %s", self->entry->name, self->entry->usage);

error:
	xfree(data);
	return (-1);
}

void
cmd_unbind_key_exec(void *ptr, unused struct cmd_ctx *ctx)
{
	struct cmd_unbind_key_data	*data = ptr;

	if (data == NULL)
		return;

	key_bindings_remove(data->key);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_unbind_key_send(void *ptr, struct buffer *b)
{
	struct cmd_unbind_key_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
}

void
cmd_unbind_key_recv(void **ptr, struct buffer *b)
{
	struct cmd_unbind_key_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
}

void
cmd_unbind_key_free(void *ptr)
{
	struct cmd_unbind_key_data	*data = ptr;

	xfree(data);
}
