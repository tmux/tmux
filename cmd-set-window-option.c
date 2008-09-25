/* $Id: cmd-set-window-option.c,v 1.12 2008-09-25 23:28:15 nicm Exp $ */

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

int	cmd_set_window_option_parse(struct cmd *, int, char **, char **);
void	cmd_set_window_option_exec(struct cmd *, struct cmd_ctx *);
void	cmd_set_window_option_send(struct cmd *, struct buffer *);
void	cmd_set_window_option_recv(struct cmd *, struct buffer *);
void	cmd_set_window_option_free(struct cmd *);
void	cmd_set_window_option_print(struct cmd *, char *, size_t);

struct cmd_set_window_option_data {
	char	*target;
	char	*option;
	char	*value;
};

const struct cmd_entry cmd_set_window_option_entry = {
	"set-window-option", "setw",
	"[-t target-window] option value",
	0,
	NULL,
	cmd_set_window_option_parse,
	cmd_set_window_option_exec,
	cmd_set_window_option_send,
	cmd_set_window_option_recv,
	cmd_set_window_option_free,
	cmd_set_window_option_print
};

int
cmd_set_window_option_parse(
    struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_set_window_option_data	*data;
	int				 	 opt;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->option = NULL;
	data->value = NULL;

	while ((opt = getopt(argc, argv, GETOPT_PREFIX "t:")) != EOF) {
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
cmd_set_window_option_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_set_window_option_data	*data = self->data;
	struct winlink				*wl;
	struct session				*s;
	const char				*errstr;
	int				 	 number, flag;
	u_int					 i;

	if (data == NULL)
		return;

	wl = cmd_find_window(ctx, data->target, &s);
	if (wl == NULL)
		return;

	if (*data->option == '\0') {
		ctx->error(ctx, "invalid option");
		return;
	}

	number = -1;
	if (data->value != NULL) {
		number = strtonum(data->value, 0, INT_MAX, &errstr);

		flag = -1;
		if (number == 1 || strcasecmp(data->value, "on") == 0 ||
		    strcasecmp(data->value, "yes") == 0)
			flag = 1;
		else if (number == 0 || strcasecmp(data->value, "off") == 0 ||
		    strcasecmp(data->value, "no") == 0)
			flag = 0;
	} else
		flag = -2;

	if (strcmp(data->option, "monitor-activity") == 0) {
		if (flag == -1) {
			ctx->error(ctx, "bad value: %s", data->value);
			return;
		}

		if (flag == -2)
			wl->window->flags ^= WINDOW_MONITOR;
		else {
			if (flag)
				wl->window->flags |= WINDOW_MONITOR;
			else
				wl->window->flags &= ~WINDOW_MONITOR;
		}

		if (wl->window->flags & WINDOW_MONITOR) {
			ctx->info(ctx, "window %s:%d: set %s",
			    s->name, wl->idx, data->option);
		} else {
			ctx->info(ctx, "window %s:%d: cleared %s",
			    s->name, wl->idx, data->option);
		}

		for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
			s = ARRAY_ITEM(&sessions, i);
			if (s != NULL)
				session_alert_cancel(s, wl);
		}
	} else if (strcmp(data->option, "aggressive-resize") == 0) {
		if (flag == -1) {
			ctx->error(ctx, "bad value: %s", data->value);
			return;
		}

		if (flag == -2)
			wl->window->flags ^= WINDOW_AGGRESSIVE;
		else {
			if (flag)
				wl->window->flags |= WINDOW_AGGRESSIVE;
			else
				wl->window->flags &= ~WINDOW_AGGRESSIVE;
		}

		if (wl->window->flags & WINDOW_AGGRESSIVE) {
			ctx->info(ctx, "window %s:%d: set %s",
			    s->name, wl->idx, data->option);
		} else {
			ctx->info(ctx, "window %s:%d: cleared %s",
			    s->name, wl->idx, data->option);
		}

		recalculate_sizes();
	} else if (strcmp(data->option, "force-width") == 0) {
		if (data->value == NULL || number == -1) {
			ctx->error(ctx, "invalid value");
			return;
		}
		if (errstr != NULL) {
			ctx->error(ctx, "force-width %s", errstr);
			return;
		}
		if (number == 0)
			wl->window->limitx = UINT_MAX;
		else
			wl->window->limitx = number;

		ctx->info(ctx, "window %s:%d: set force-width %u",
		    s->name, wl->idx, number);

		recalculate_sizes();
	} else if (strcmp(data->option, "force-height") == 0) {
		if (data->value == NULL || number == -1) {
			ctx->error(ctx, "invalid value");
			return;
		}
		if (errstr != NULL) {
			ctx->error(ctx, "force-height %s", errstr);
			return;
		}
		if (number == 0)
			wl->window->limity = UINT_MAX;
		else
			wl->window->limity = number;

		ctx->info(ctx, "window %s:%d: set force-height %u",
		    s->name, wl->idx, number);

		recalculate_sizes();
	} else if (strcmp(data->option, "remain-on-exit") == 0) {
		if (flag == -1) {
			ctx->error(ctx, "bad value: %s", data->value);
			return;
		}

		if (flag == -2)
			wl->window->flags ^= WINDOW_ZOMBIFY;
		else {
			if (flag)
				wl->window->flags |= WINDOW_ZOMBIFY;
			else
				wl->window->flags &= ~WINDOW_ZOMBIFY;
		}
	} else {
		ctx->error(ctx, "unknown option: %s", data->option);
		return;
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_set_window_option_send(struct cmd *self, struct buffer *b)
{
	struct cmd_set_window_option_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
	cmd_send_string(b, data->option);
	cmd_send_string(b, data->value);
}

void
cmd_set_window_option_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_set_window_option_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
	data->option = cmd_recv_string(b);
	data->value = cmd_recv_string(b);
}

void
cmd_set_window_option_free(struct cmd *self)
{
	struct cmd_set_window_option_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	if (data->option != NULL)
		xfree(data->option);
	if (data->value != NULL)
		xfree(data->value);
	xfree(data);
}

void
cmd_set_window_option_print(struct cmd *self, char *buf, size_t len)
{
	struct cmd_set_window_option_data	*data = self->data;
	size_t					 off = 0;

	off += xsnprintf(buf, len, "%s", self->entry->name);
	if (data == NULL)
		return;
	if (off < len && data->target != NULL)
		off += xsnprintf(buf + off, len - off, " -t %s", data->target);
	if (off < len && data->option != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->option);
	if (off < len && data->value != NULL)
		off += xsnprintf(buf + off, len - off, " %s", data->value);
}
