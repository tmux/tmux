/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <pwd.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Set server password.
 */

int	cmd_set_password_parse(struct cmd *, int, char **, char **);
int	cmd_set_password_exec(struct cmd *, struct cmd_ctx *);
void	cmd_set_password_send(struct cmd *, struct buffer *);
void	cmd_set_password_recv(struct cmd *, struct buffer *);
void	cmd_set_password_free(struct cmd *);
void	cmd_set_password_init(struct cmd *, int);
size_t	cmd_set_password_print(struct cmd *, char *, size_t);

struct cmd_set_password_data {
	char	*password;
	int	 flag_encrypted;
};

const struct cmd_entry cmd_set_password_entry = {
	"set-password", "pass",
	"[-c] password",
	0,
	cmd_set_password_init,
	cmd_set_password_parse,
	cmd_set_password_exec,
	cmd_set_password_send,
	cmd_set_password_recv,
	cmd_set_password_free,
	cmd_set_password_print
};

void
cmd_set_password_init(struct cmd *self, unused int arg)
{
	struct cmd_set_password_data	 *data;

	self->data = data = xmalloc(sizeof *data);
	data->password = NULL;
	data->flag_encrypted = 0;
}

int
cmd_set_password_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_set_password_data	*data;
	int				 opt;
	char				*out;

	self->entry->init(self, 0);
	data = self->data;

	while ((opt = getopt(argc, argv, "c")) != -1) {
		switch (opt) {
		case 'c':
			data->flag_encrypted = 1;
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1)
		goto usage;

	if (!data->flag_encrypted) {
		if ((out = crypt(argv[0], "$1")) != NULL)
			data->password = xstrdup(out);
	} else
		data->password = xstrdup(argv[0]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

int
cmd_set_password_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_set_password_data	*data = self->data;

	if (data->password == NULL) {
		ctx->error(ctx, "failed to encrypt password");
		return (-1);
	}

	if (server_password != NULL)
		xfree(server_password);
	if (*data->password == '\0')
		server_password = NULL;
	else
		server_password = xstrdup(data->password);
	log_debug("pw now %s", server_password);

 	return (0);
}

void
cmd_set_password_send(struct cmd *self, struct buffer *b)
{
	struct cmd_set_password_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->password);
}

void
cmd_set_password_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_set_password_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->password = cmd_recv_string(b);
}

void
cmd_set_password_free(struct cmd *self)
{
	struct cmd_set_password_data	*data = self->data;

	if (data->password != NULL)
		xfree(data->password);
	xfree(data);
}

size_t
cmd_set_password_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_set_password_data	*data = self->data;
	size_t				 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return (off);
	if (off < len && data->flag_encrypted)
		off += xsnprintf(buf + off, len - off, " -c");
	if (off < len && data->password != NULL)
		off += xsnprintf(buf + off, len - off, " password");
	return (off);
}
