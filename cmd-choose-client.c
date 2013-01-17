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
#include <stdlib.h>

#include "tmux.h"

/*
 * Enter choice mode to choose a client.
 */

enum cmd_retval	 cmd_choose_client_exec(struct cmd *, struct cmd_ctx *);

void	cmd_choose_client_callback(struct window_choose_data *);
void	cmd_choose_client_free(struct window_choose_data *);

const struct cmd_entry cmd_choose_client_entry = {
	"choose-client", NULL,
	"F:t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE " [-F format] [template]",
	0,
	NULL,
	NULL,
	cmd_choose_client_exec
};

struct cmd_choose_client_data {
	struct client	*client;
};

enum cmd_retval
cmd_choose_client_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct window_choose_data	*cdata;
	struct winlink			*wl;
	struct client			*c;
	const char			*template;
	char				*action;
	u_int			 	 i, idx, cur;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (CMD_RETURN_ERROR);
	}

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (CMD_RETURN_ERROR);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (CMD_RETURN_NORMAL);

	if ((template = args_get(args, 'F')) == NULL)
		template = CHOOSE_CLIENT_TEMPLATE;

	if (args->argc != 0)
		action = xstrdup(args->argv[0]);
	else
		action = xstrdup("detach-client -t '%%'");

	cur = idx = 0;
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c == ctx->curclient)
			cur = idx;
		idx++;

		cdata = window_choose_data_create(ctx);
		cdata->idx = i;
		cdata->client->references++;

		cdata->ft_template = xstrdup(template);
		format_add(cdata->ft, "line", "%u", i);
		format_session(cdata->ft, c->session);
		format_client(cdata->ft, c);

		cdata->command = cmd_template_replace(action, c->tty.path, 1);

		window_choose_add(wl->window->active, cdata);
	}
	free(action);

	window_choose_ready(wl->window->active,
	    cur, cmd_choose_client_callback, cmd_choose_client_free);

	return (CMD_RETURN_NORMAL);
}

void
cmd_choose_client_callback(struct window_choose_data *cdata)
{
	struct client  	*c;

	if (cdata == NULL)
		return;
	if (cdata->client->flags & CLIENT_DEAD)
		return;

	if (cdata->idx > ARRAY_LENGTH(&clients) - 1)
		return;
	c = ARRAY_ITEM(&clients, cdata->idx);
	if (c == NULL || c->session == NULL)
		return;

	window_choose_ctx(cdata);
}

void
cmd_choose_client_free(struct window_choose_data *cdata)
{
	if (cdata == NULL)
		return;

	cdata->client->references--;

	free(cdata->ft_template);
	free(cdata->command);
	format_free(cdata->ft);
	free(cdata);
}
