/* $Id: cmd-show-options.c,v 1.2 2008-06-16 06:33:50 nicm Exp $ */

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

	while ((opt = getopt(argc, argv, "t:s:")) != EOF) {
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
	struct options_entry		*o;

	if (data == NULL)
		return;

	if (data->flag_global ||
	    ((s = cmd_find_session(ctx, data->target))) == NULL)
		oo = &global_options;
	else
		oo = &s->options;

	SPLAY_FOREACH(o, options_tree, &oo->tree) {
		switch (o->type) {
		case OPTIONS_STRING:
			ctx->print(
			    ctx, "%s \"%s\"", o->name, o->value.string);
			break;
		case OPTIONS_NUMBER:
			ctx->print(ctx, "%s %lld", o->name, o->value.number);
			break;
		case OPTIONS_KEY:
			ctx->print(ctx, "%s %s", o->name, 
			    key_string_lookup_key(o->value.key));
			break;
		case OPTIONS_COLOURS:
			ctx->print(ctx, "%s fg=%s, bg=%s", o->name,
			    screen_colourstring(o->value.colours >> 4),
			    screen_colourstring(o->value.colours & 0x0f));
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
