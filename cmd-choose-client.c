/* $Id$ */

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

#include <ctype.h>

#include "tmux.h"

/*
 * Enter choice mode to choose a client.
 */

int	cmd_choose_client_exec(struct cmd *, struct cmd_ctx *);

void	cmd_choose_client_callback(void *, int);
void	cmd_choose_client_free(void *);

const struct cmd_entry cmd_choose_client_entry = {
	"choose-client", NULL,
	"t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE " [template]",
	0,
	NULL,
	NULL,
	cmd_choose_client_exec
};

struct cmd_choose_client_data {
	struct client	*client;
	char   		*template;
};

int
cmd_choose_client_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct cmd_choose_client_data	*cdata;
	struct winlink			*wl;
	struct client			*c;
	u_int			 	 i, idx, cur;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (-1);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (0);

	cur = idx = 0;
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c == ctx->curclient)
			cur = idx;
		idx++;

		window_choose_add(wl->window->active, i,
		    "%s: %s [%ux%u %s]%s%s", c->tty.path,
		    c->session->name, c->tty.sx, c->tty.sy,
		    c->tty.termname,
		    c->tty.flags & TTY_UTF8 ? " (utf8)" : "",
		    c->flags & CLIENT_READONLY ? " (ro)" : "");
	}

	cdata = xmalloc(sizeof *cdata);
	if (args->argc != 0)
		cdata->template = xstrdup(args->argv[0]);
	else
		cdata->template = xstrdup("detach-client -t '%%'");
	cdata->client = ctx->curclient;
	cdata->client->references++;

	window_choose_ready(wl->window->active,
	    cur, cmd_choose_client_callback, cmd_choose_client_free, cdata);

	return (0);
}

void
cmd_choose_client_callback(void *data, int idx)
{
	struct cmd_choose_client_data	*cdata = data;
	struct client  			*c;
	struct cmd_list			*cmdlist;
	struct cmd_ctx			 ctx;
	char				*template, *cause;

	if (idx == -1)
		return;
	if (cdata->client->flags & CLIENT_DEAD)
		return;

	if ((u_int) idx > ARRAY_LENGTH(&clients) - 1)
		return;
	c = ARRAY_ITEM(&clients, idx);
	if (c == NULL || c->session == NULL)
		return;
	template = cmd_template_replace(cdata->template, c->tty.path, 1);

	if (cmd_string_parse(template, &cmdlist, &cause) != 0) {
		if (cause != NULL) {
			*cause = toupper((u_char) *cause);
			status_message_set(c, "%s", cause);
			xfree(cause);
		}
		xfree(template);
		return;
	}
	xfree(template);

	ctx.msgdata = NULL;
	ctx.curclient = cdata->client;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_list_exec(cmdlist, &ctx);
	cmd_list_free(cmdlist);
}

void
cmd_choose_client_free(void *data)
{
	struct cmd_choose_client_data	*cdata = data;

	cdata->client->references--;
	xfree(cdata->template);
	xfree(cdata);
}
