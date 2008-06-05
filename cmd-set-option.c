/* $Id: cmd-set-option.c,v 1.23 2008-06-05 16:35:32 nicm Exp $ */

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
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Set an option.
 */

int	cmd_set_option_parse(struct cmd *, int, char **, char **);
void	cmd_set_option_exec(struct cmd *, struct cmd_ctx *);
void	cmd_set_option_send(struct cmd *, struct buffer *);
void	cmd_set_option_recv(struct cmd *, struct buffer *);
void	cmd_set_option_free(struct cmd *);

struct cmd_set_option_data {
	char	*cname;
	char	*sname;
	int	 flag_global;
	char	*option;
	char	*value;
};

const struct cmd_entry cmd_set_option_entry = {
	"set-option", "set",
	"[-c client-tty|-s session-name] option value",
	0,
	cmd_set_option_parse,
	cmd_set_option_exec,
	cmd_set_option_send,
	cmd_set_option_recv,
	cmd_set_option_free,
	NULL,
	NULL
};

int
cmd_set_option_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_set_option_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;
	data->flag_global = 1;
	data->option = NULL;
	data->value = NULL;

	while ((opt = getopt(argc, argv, "c:s:")) != EOF) {
		switch (opt) {
		case 'c':
			if (data->sname != NULL)
				goto usage;
			if (data->cname == NULL)
				data->cname = xstrdup(optarg);
			data->flag_global = 0;
			break;
		case 's':
			if (data->cname != NULL)
				goto usage;
			if (data->sname == NULL)
				data->sname = xstrdup(optarg);
			data->flag_global = 0;
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 1 && argc != 2)
		goto usage;

	data->option = xstrdup(argv[0]);
	if (argc == 2)
		data->value = xstrdup(argv[1]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_set_option_exec(struct cmd *self, unused struct cmd_ctx *ctx)
{
	struct cmd_set_option_data	*data = self->data;
	struct client			*c;
	struct session			*s;
	struct options			*oo;
	const char			*errstr;
	u_int				 i;
	int				 number, bool, key;
	u_char				 colour;

	if (data == NULL)
		return;

	if (data->flag_global ||
	    ((s = cmd_find_session(ctx, data->cname, data->sname))) == NULL)
		oo = &global_options;
	else
		oo = &s->options;

	if (*data->option == '\0') {
		ctx->error(ctx, "invalid option");
		return;
	}

	number = -1;
	if (data->value != NULL) {
		number = strtonum(data->value, 0, INT_MAX, &errstr);

		bool = -1;
		if (number == 1 || strcasecmp(data->value, "on") == 0 ||
		    strcasecmp(data->value, "yes") == 0)
			bool = 1;
		else if (number == 0 || strcasecmp(data->value, "off") == 0 ||
		    strcasecmp(data->value, "no") == 0)
			bool = 0;
	} else
		bool = -2;

	if (strcmp(data->option, "prefix") == 0) {
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;
		}
		key = key_string_lookup_string(data->value);
		if (key == KEYC_NONE) {
			ctx->error(ctx, "unknown key: %s", data->value);
			return;
		}
		options_set_number(oo, "prefix-key", key);
	} else if (strcmp(data->option, "status") == 0) {
		if (bool == -1) {
			ctx->error(ctx, "bad value: %s", data->value);
			return;
		}
		if (bool == -2)
			bool = !options_get_number(oo, "status-lines");
		options_set_number(oo, "status-lines", bool);
		recalculate_sizes();
	} else if (strcmp(data->option, "status-fg") == 0) {
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;
		}
		number = screen_stringcolour(data->value);
		if (number > 8) {
			ctx->error(ctx, "bad colour: %s", data->value);
			return;
		}

		colour = options_get_number(oo, "status-colour");
		colour &= 0x0f;
		colour |= number << 4;
		options_set_number(oo, "status-colour", colour);

		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c != NULL && c->session != NULL)
				server_redraw_client(c);
		}
	} else if (strcmp(data->option, "status-bg") == 0) {
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;
		}
		number = screen_stringcolour(data->value);
		if (number > 8) {
			ctx->error(ctx, "bad colour: %s", data->value);
			return;
		}

		colour = options_get_number(oo, "status-colour");
		colour &= 0xf0;
		colour |= number;
		options_set_number(oo, "status-colour", colour);

		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c != NULL && c->session != NULL)
				server_redraw_client(c);
		}
	} else if (strcmp(data->option, "bell-action") == 0) {
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;
		}
		if (strcmp(data->value, "any") == 0)
			number = BELL_ANY;
		else if (strcmp(data->value, "none") == 0)
			number = BELL_NONE;
		else if (strcmp(data->value, "current") == 0)
			number = BELL_CURRENT;
		else {
			ctx->error(ctx, "unknown bell-action: %s", data->value);
			return;
		}
		options_set_number(oo, "bell-action", number);
	} else if (strcmp(data->option, "default-command") == 0) {
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;
		}
		options_set_string(oo, "default-command", "%s", data->value);
	} else if (strcmp(data->option, "history-limit") == 0) {
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;
		}
		if (number > SHRT_MAX) {
			ctx->error(ctx, "history-limit too big: %u", number);
			return;
		}
		options_set_number(oo, "history-limit", number);
	} else if (strcmp(data->option, "status-left") == 0) {
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;
		}

		options_set_string(oo, "status-left", "%s", data->value);

		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c != NULL && c->session != NULL)
				server_redraw_client(c);
		}
	} else if (strcmp(data->option, "status-right") == 0) {
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;
		}

		options_set_string(oo, "status-right", "%s", data->value);

		for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
			c = ARRAY_ITEM(&clients, i);
			if (c != NULL && c->session != NULL)
				server_redraw_client(c);
		}
	} else {
		ctx->error(ctx, "unknown option: %s", data->option);
		return;
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_set_option_send(struct cmd *self, struct buffer *b)
{
	struct cmd_set_option_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->sname);
	cmd_send_string(b, data->option);
	cmd_send_string(b, data->value);
}

void
cmd_set_option_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_set_option_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->sname = cmd_recv_string(b);
	data->option = cmd_recv_string(b);
	data->value = cmd_recv_string(b);
}

void
cmd_set_option_free(struct cmd *self)
{
	struct cmd_set_option_data	*data = self->data;

	if (data->cname != NULL)
		xfree(data->cname);
	if (data->sname != NULL)
		xfree(data->sname);
	if (data->option != NULL)
		xfree(data->option);
	if (data->value != NULL)
		xfree(data->value);
	xfree(data);
}
