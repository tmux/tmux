/* $Id: cmd-send-keys.c,v 1.7 2008-06-03 16:55:09 nicm Exp $ */

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

/*
 * Send keys to client.
 */

int	cmd_send_keys_parse(struct cmd *, void **, int, char **, char **);
void	cmd_send_keys_exec(void *, struct cmd_ctx *);
void	cmd_send_keys_send(void *, struct buffer *);
void	cmd_send_keys_recv(void **, struct buffer *);
void	cmd_send_keys_free(void *);

struct cmd_send_keys_data {
	char	*cname;
	char	*sname;
	int	 idx;
  	u_int	 nkeys;
	int	*keys;
};

const struct cmd_entry cmd_send_keys_entry = {
	"send-keys", "send",
	"[-c client-tty|-s session-name] [-i index] key ...",
	0,
	cmd_send_keys_parse,
	cmd_send_keys_exec,
	cmd_send_keys_send,
	cmd_send_keys_recv,
	cmd_send_keys_free,
	NULL
};

int
cmd_send_keys_parse(
    struct cmd *self, void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_send_keys_data	*data;
	int				 opt, key;
	const char			*errstr;
	char				*s;

	*ptr = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;
	data->idx = -1;
	data->nkeys = 0;
	data->keys = NULL;

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

error:
	cmd_send_keys_free(data);
	return (-1);
}

void
cmd_send_keys_exec(void *ptr, struct cmd_ctx *ctx)
{
	struct cmd_send_keys_data	*data = ptr;
	struct winlink			*wl;
	u_int				 i;

	if (data == NULL)
		return;

	wl = cmd_find_window(ctx, data->cname, data->sname, data->idx, NULL);
	if (wl == NULL)
		return;

	for (i = 0; i < data->nkeys; i++)
		window_key(wl->window, data->keys[i]);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_send_keys_send(void *ptr, struct buffer *b)
{
	struct cmd_send_keys_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->sname);
	buffer_write(b, data->keys, data->nkeys * sizeof *data->keys);
}

void
cmd_send_keys_recv(void **ptr, struct buffer *b)
{
	struct cmd_send_keys_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->sname = cmd_recv_string(b);
	data->keys = xcalloc(data->nkeys, sizeof *data->keys);
	buffer_read(b, data->keys, data->nkeys * sizeof *data->keys);
}

void
cmd_send_keys_free(void *ptr)
{
	struct cmd_send_keys_data	*data = ptr;

	if (data->cname != NULL)
		xfree(data->cname);
	if (data->sname != NULL)
		xfree(data->sname);
	xfree(data);
}
