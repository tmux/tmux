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

#include "tmux.h"

/*
 * Send keys to client.
 */

int	cmd_send_keys_parse(struct cmd *, int, char **, char **);
int	cmd_send_keys_exec(struct cmd *, struct cmd_ctx *);
void	cmd_send_keys_send(struct cmd *, struct buffer *);
void	cmd_send_keys_recv(struct cmd *, struct buffer *);
void	cmd_send_keys_free(struct cmd *);
size_t	cmd_send_keys_print(struct cmd *, char *, size_t);

struct cmd_send_keys_data {
	char	*target;
	int	 idx;
  	u_int	 nkeys;
	int	*keys;
};

const struct cmd_entry cmd_send_keys_entry = {
	"send-keys", "send",
	"[-t target-window] key ...",
	0,
	NULL,
	cmd_send_keys_parse,
	cmd_send_keys_exec,
	cmd_send_keys_send,
	cmd_send_keys_recv,
	cmd_send_keys_free,
	cmd_send_keys_print
};

int
cmd_send_keys_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_send_keys_data	*data;
	int				 opt, key;
	char				*s;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->idx = -1;
	data->nkeys = 0;
	data->keys = NULL;

	while ((opt = getopt(argc, argv, "t:")) != -1) {
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
	if (argc == 0)
		goto usage;

	while (argc-- != 0) {
		if ((key = key_string_lookup_string(*argv)) != KEYC_NONE) {
			data->keys = xrealloc(
			    data->keys, data->nkeys + 1, sizeof *data->keys);
			data->keys[data->nkeys++] = key;
		} else {
			for (s = *argv; *s != '\0'; s++) {
				data->keys = xrealloc(data->keys,
				    data->nkeys + 1, sizeof *data->keys);
				data->keys[data->nkeys++] = *s;
			}
		}

		argv++;
	}

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

int
cmd_send_keys_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_send_keys_data	*data = self->data;
	struct winlink			*wl;
	u_int				 i;

	if (data == NULL)
		return (-1);

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return (-1);

	for (i = 0; i < data->nkeys; i++) {
		window_pane_key(
		    wl->window->active, ctx->curclient, data->keys[i]);
	}

	return (0);
}

void
cmd_send_keys_send(struct cmd *self, struct buffer *b)
{
	struct cmd_send_keys_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
	buffer_write(b, data->keys, data->nkeys * sizeof *data->keys);
}

void
cmd_send_keys_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_send_keys_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
	data->keys = xcalloc(data->nkeys, sizeof *data->keys);
	buffer_read(b, data->keys, data->nkeys * sizeof *data->keys);
}

void
cmd_send_keys_free(struct cmd *self)
{
	struct cmd_send_keys_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	xfree(data);
}

size_t
cmd_send_keys_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_send_keys_data	*data = self->data;
	size_t				 off = 0;
	u_int				 i;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->target != NULL)
		off += cmd_prarg(buf + off, len - off, " -t ", data->target);
	if (off < len && data->idx != -1)
		off += xsnprintf(buf + off, len - off, " -i %d", data->idx);

	for (i = 0; i < data->nkeys; i++) {
		if (off >= len)
			break;
		off += xsnprintf(buf + off,
		    len - off, " %s", key_string_lookup_key(data->keys[i]));
	}
	return (off);
}
