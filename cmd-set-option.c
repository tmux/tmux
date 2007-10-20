/* $Id: cmd-set-option.c,v 1.10 2007-10-20 09:57:08 nicm Exp $ */

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

int	cmd_set_option_parse(void **, int, char **, char **);
void	cmd_set_option_exec(void *, struct cmd_ctx *);
void	cmd_set_option_send(void *, struct buffer *);
void	cmd_set_option_recv(void **, struct buffer *);
void	cmd_set_option_free(void *);

struct cmd_set_option_data {
	char	*option;
	char	*value;
};

const struct cmd_entry cmd_set_option_entry = {
	"set-option", "set", "option value",
	CMD_NOSESSION,
	cmd_set_option_parse,
	cmd_set_option_exec, 
	cmd_set_option_send,
	cmd_set_option_recv,
	cmd_set_option_free
};

int
cmd_set_option_parse(void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_set_option_data	*data;
	int				 opt;

	*ptr = data = xmalloc(sizeof *data);
	data->option = NULL;
	data->value = NULL;

	while ((opt = getopt(argc, argv, "")) != EOF) {
		switch (opt) {
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
	usage(cause, "%s %s",
	    cmd_set_option_entry.name, cmd_set_option_entry.usage);

	cmd_set_option_free(data);
	return (-1);
}

void
cmd_set_option_exec(void *ptr, unused struct cmd_ctx *ctx)
{
	struct cmd_set_option_data	*data = ptr;
	struct client			*c = ctx->client;
	const char			*errstr;
	u_int				 number, i;
	int				 bool, key;

	if (data == NULL)
		return;

	if (*data->option == '\0') {
		ctx->error(ctx, "invalid option");
		return;
	}

	number = -1;
	if (data->value != NULL) {
		number = strtonum(data->value, 0, UINT_MAX, &errstr);
		
		bool = -1;
		if (number == 1 || strcasecmp(data->value, "on") == 0 ||
		    strcasecmp(data->value, "yes") == 0)
			bool = 1;
		else if (number == 0 || strcasecmp(data->value, "off") == 0 ||
		    strcasecmp(data->value, "no") == 0)
			bool = 0;
	} else
		bool = 1;

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
		prefix_key = key;
	} else if (strcmp(data->option, "status") == 0) {
		if (bool == -1) {
			ctx->error(ctx, "bad value: %s", data->value);
			return;
		}
		status_lines = bool;
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
		status_colour &= 0x0f;
		status_colour |= number << 4;
		if (status_lines > 0) {
			for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
				c = ARRAY_ITEM(&clients, i);
				if (c != NULL && c->session != NULL)
					server_redraw_client(c);
			}
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
		status_colour &= 0xf0;
		status_colour |= number;
		if (status_lines > 0) {
			for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
				c = ARRAY_ITEM(&clients, i);
				if (c != NULL && c->session != NULL)
					server_redraw_client(c);
			}
		}
	} else if (strcmp(data->option, "bell-action") == 0) { 
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;			
		}
		if (strcmp(data->value, "any") == 0)
			bell_action = BELL_ANY;
		else if (strcmp(data->value, "none") == 0)
			bell_action = BELL_NONE;
		else if (strcmp(data->value, "current") == 0)
			bell_action = BELL_CURRENT;
		else {
			ctx->error(ctx, "unknown bell-action: %s", data->value);
			return;
		}
	} else if (strcmp(data->option, "default-command") == 0) { 
		if (data->value == NULL) {
			ctx->error(ctx, "invalid value");
			return;			
		}
		xfree(default_command);
		default_command = xstrdup(data->value);
	} else {
		ctx->error(ctx, "unknown option: %s", data->option);
		return;
	}

	if (!(ctx->flags & CMD_KEY))
		server_write_client(c, MSG_EXIT, NULL, 0);
}

void
cmd_set_option_send(void *ptr, struct buffer *b)
{
	struct cmd_set_option_data	*data = ptr;

	cmd_send_string(b, data->option);
	cmd_send_string(b, data->value);
}

void
cmd_set_option_recv(void **ptr, struct buffer *b)
{
	struct cmd_set_option_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	data->option = cmd_recv_string(b);
	data->value = cmd_recv_string(b);
}

void
cmd_set_option_free(void *ptr)
{
	struct cmd_set_option_data	*data = ptr;

	if (data->option != NULL)
		xfree(data->option);
	if (data->value != NULL)
		xfree(data->value);
	xfree(data);
}
