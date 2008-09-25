/* $Id: cmd-show-options.c,v 1.7 2008-09-25 23:28:15 nicm Exp $ */

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
 * Show options.
 */

int	cmd_show_options_parse(struct cmd *, int, char **, char **);
void	cmd_show_options_exec(struct cmd *, struct cmd_ctx *);
void	cmd_show_options_send(struct cmd *, struct buffer *);
void	cmd_show_options_recv(struct cmd *, struct buffer *);
void	cmd_show_options_free(struct cmd *);
void	cmd_show_options_print(struct cmd *, char *, size_t);

struct cmd_show_options_data {
	char	*target;
	int	 flag_global;
};

/*
 * XXX Can't use cmd_target because we want -t not to use current if missing
 * (this could be a flag??).
 */
const struct cmd_entry cmd_show_options_entry = {
	"show-options", "show",
	"[-t target-session]",
	0,
	NULL,
	cmd_show_options_parse,
	cmd_show_options_exec,
	cmd_show_options_send,
	cmd_show_options_recv,
	cmd_show_options_free,
	cmd_show_options_print
};

int
cmd_show_options_parse(struct cmd *self, int argc, char **argv, char **cause)
{
	struct cmd_show_options_data	*data;
	int				 opt;

	self->data = data = xmalloc(sizeof *data);
	data->target = NULL;
	data->flag_global = 1;

	while ((opt = getopt(argc, argv, GETOPT_PREFIX "t:s:")) != EOF) {
		switch (opt) {
		case 't':
			if (data->target == NULL)
				data->target = xstrdup(optarg);
			data->flag_global = 0;
			break;
		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc != 0)
		goto usage;

	return (0);

usage:
	xasprintf(cause, "usage: %s %s", self->entry->name, self->entry->usage);

	self->entry->free(self);
	return (-1);
}

void
cmd_show_options_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_show_options_data	*data = self->data;
	struct session			*s;
	struct options			*oo;
	const struct set_option_entry   *entry;
	u_int				 i;
	char				*vs;
	long long			 vn;

	if (data == NULL)
		return;

	if (data->flag_global ||
	    ((s = cmd_find_session(ctx, data->target))) == NULL)
		oo = &global_options;
	else
		oo = &s->options;

	for (i = 0; i < NSETOPTION; i++) {
		entry = &set_option_table[i];

		if (options_find1(oo, entry->name) == NULL)
			continue;

		switch (entry->type) {
		case SET_OPTION_STRING:
			vs = options_get_string(oo, entry->name);
			ctx->print(ctx, "%s \"%s\"", entry->name, vs);
			break;
		case SET_OPTION_NUMBER:
			vn = options_get_number(oo, entry->name);
			ctx->print(ctx, "%s %lld", entry->name, vn);
			break;
		case SET_OPTION_KEY:
			vn = options_get_number(oo, entry->name);
 			ctx->print(ctx, "%s %s",
			    entry->name, key_string_lookup_key(vn));
			break;
		case SET_OPTION_COLOUR:
			vn = options_get_number(oo, entry->name);
 			ctx->print(ctx, "%s %s",
			    entry->name, colour_tostring(vn));
			break;
		case SET_OPTION_FLAG:
			vn = options_get_number(oo, entry->name);
			if (vn)
				ctx->print(ctx, "%s on", entry->name);
			else
				ctx->print(ctx, "%s off", entry->name);
			break;
		case SET_OPTION_CHOICE:
			vn = options_get_number(oo, entry->name);
			ctx->print(ctx, "%s %s",
			    entry->name, entry->choices[vn]);
			break;
		}
	}

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_show_options_send(struct cmd *self, struct buffer *b)
{
	struct cmd_show_options_data	*data = self->data;

	buffer_write(b, data, sizeof *data);
	cmd_send_string(b, data->target);
}

void
cmd_show_options_recv(struct cmd *self, struct buffer *b)
{
	struct cmd_show_options_data	*data;

	self->data = data = xmalloc(sizeof *data);
	buffer_read(b, data, sizeof *data);
	data->target = cmd_recv_string(b);
}

void
cmd_show_options_free(struct cmd *self)
{
	struct cmd_show_options_data	*data = self->data;

	if (data->target != NULL)
		xfree(data->target);
	xfree(data);
}

void
cmd_show_options_print(struct cmd *self, char *buf, size_t len)
{
	xsnprintf(buf, len, "%s", self->entry->name);
}
