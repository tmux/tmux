/* $Id: cmd-set-window-option.c,v 1.1 2008-06-04 17:54:26 nicm Exp $ */

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
 * Set a window option.
 */

int	cmd_set_window_option_parse(
	    struct cmd *, void **, int, char **, char **);
void	cmd_set_window_option_exec(void *, struct cmd_ctx *);
void	cmd_set_window_option_send(void *, struct buffer *);
void	cmd_set_window_option_recv(void **, struct buffer *);
void	cmd_set_window_option_free(void *);

struct cmd_set_window_option_data {
	char	*cname;
	char	*sname;
	int	 idx;
	char	*option;
	char	*value;
};

const struct cmd_entry cmd_set_window_option_entry = {
	"set-window-option", "setw",
	"[-c client-tty|-s session-name] [-i index] option value",
	0,
	cmd_set_window_option_parse,
	cmd_set_window_option_exec,
	cmd_set_window_option_send,
	cmd_set_window_option_recv,
	cmd_set_window_option_free,
	NULL
};

int
cmd_set_window_option_parse(
    struct cmd *self, void **ptr, int argc, char **argv, char **cause)
{
	struct cmd_set_window_option_data	*data;
	int				 	 opt;
	const char   				*errstr;

	*ptr = data = xmalloc(sizeof *data);
	data->cname = NULL;
	data->sname = NULL;
	data->idx = -1;
	data->option = NULL;
	data->value = NULL;

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
	if (argc != 1 && argc != 2)
		goto usage;

	data->option = xstrdup(argv[0]);
	if (argc == 2)
		data->value = xstrdup(argv[1]);

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

error:
	cmd_set_window_option_free(data);
	return (-1);
}

void
cmd_set_window_option_exec(void *ptr, unused struct cmd_ctx *ctx)
{
	struct cmd_set_window_option_data	*data = ptr;
	struct winlink				*wl;
	struct session				*s;
	const char				*errstr;
	int				 	 number, bool;
	u_int					 i;

	if (data == NULL)
		return;

	if (data == NULL)
		return;

	wl = cmd_find_window(ctx, data->cname, data->sname, data->idx, &s);
	if (wl == NULL)
		return;

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
		bool = 1;

	if (strcmp(data->option, "monitor-activity") == 0) {
		if (bool == -1) {
			ctx->error(ctx, "bad value: %s", data->value);
			return;
		}

		if (bool)
			wl->window->flags |= WINDOW_MONITOR;
		else 
			wl->window->flags &= ~WINDOW_MONITOR;

		for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
			s = ARRAY_ITEM(&sessions, i); 
			if (s != NULL)
				session_alert_cancel(s, wl);
		}
	} else {
		ctx->error(ctx, "unknown option: %s", data->option);
		return;
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_set_window_option_send(void *ptr, struct buffer *b)
{
	struct cmd_set_window_option_data	*data = ptr;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->cname);
	cmd_send_string(b, data->sname);
	cmd_send_string(b, data->option);
	cmd_send_string(b, data->value);
}

void
cmd_set_window_option_recv(void **ptr, struct buffer *b)
{
	struct cmd_set_window_option_data	*data;

	*ptr = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->cname = cmd_recv_string(b);
	data->sname = cmd_recv_string(b);
	data->option = cmd_recv_string(b);
	data->value = cmd_recv_string(b);
}

void
cmd_set_window_option_free(void *ptr)
{
	struct cmd_set_window_option_data	*data = ptr;

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
