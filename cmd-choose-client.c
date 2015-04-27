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

#include <ctype.h>
#include <stdlib.h>

#include "tmux.h"

/*
 * Enter choice mode to choose a client.
 */

#define CHOOSE_CLIENT_TEMPLATE					\
	"#{client_tty}: #{session_name} "			\
	"[#{client_width}x#{client_height} #{client_termname}]"	\
	"#{?client_utf8, (utf8),}#{?client_readonly, (ro),} "	\
	"(last used #{client_activity_string})"

enum cmd_retval	 cmd_choose_client_exec(struct cmd *, struct cmd_q *);

void	cmd_choose_client_callback(struct window_choose_data *);

const struct cmd_entry cmd_choose_client_entry = {
	"choose-client", NULL,
	"F:t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE " [-F format] [template]",
	0,
	cmd_choose_client_exec
};

struct cmd_choose_client_data {
	struct client	*client;
};

enum cmd_retval
cmd_choose_client_exec(struct cmd *self, struct cmd_q *cmdq)
{
	struct args			*args = self->args;
	struct client			*c;
	struct client			*c1;
	struct window_choose_data	*cdata;
	struct winlink			*wl;
	const char			*template;
	char				*action;
	u_int			 	 idx, cur;

	if ((c = cmd_find_client(cmdq, NULL, 1)) == NULL) {
		cmdq_error(cmdq, "no client available");
		return (CMD_RETURN_ERROR);
	}

	if ((wl = cmd_find_window(cmdq, args_get(args, 't'), NULL)) == NULL)
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
	TAILQ_FOREACH(c1, &clients, entry) {
		if (c1->session == NULL || c1->tty.path == NULL)
			continue;
		if (c1 == cmdq->client)
			cur = idx;

		cdata = window_choose_data_create(TREE_OTHER, c, c->session);
		cdata->idx = idx;

		cdata->ft_template = xstrdup(template);
		format_add(cdata->ft, "line", "%u", idx);
		format_defaults(cdata->ft, c1, NULL, NULL, NULL);

		cdata->command = cmd_template_replace(action, c1->tty.path, 1);

		window_choose_add(wl->window->active, cdata);

		idx++;
	}
	free(action);

	window_choose_ready(wl->window->active, cur,
	    cmd_choose_client_callback);

	return (CMD_RETURN_NORMAL);
}

void
cmd_choose_client_callback(struct window_choose_data *cdata)
{
	struct client  	*c;
	u_int		 idx;

	if (cdata == NULL)
		return;
	if (cdata->start_client->flags & CLIENT_DEAD)
		return;

	idx = 0;
	TAILQ_FOREACH(c, &clients, entry) {
		if (idx == cdata->idx)
			break;
		idx++;
	}
	if (c == NULL || c->session == NULL)
		return;

	window_choose_data_run(cdata);
}
