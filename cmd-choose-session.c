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
 * Enter choice mode to choose a session.
 */

int	cmd_choose_session_exec(struct cmd *, struct cmd_ctx *);

void	cmd_choose_session_callback(struct window_choose_data *);
void	cmd_choose_session_free(struct window_choose_data *);

const struct cmd_entry cmd_choose_session_entry = {
	"choose-session", NULL,
	"F:t:", 0, 1,
	CMD_TARGET_WINDOW_USAGE " [-F format] [template]",
	0,
	NULL,
	NULL,
	cmd_choose_session_exec
};

int
cmd_choose_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct window_choose_data	*cdata;
	struct winlink			*wl;
	struct session			*s;
	const char			*template;
	u_int			 	 idx, cur;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (-1);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (0);

	if ((template = args_get(args, 'F')) == NULL)
		template = DEFAULT_SESSION_TEMPLATE;

	cur = idx = 0;
	RB_FOREACH(s, sessions, &sessions) {
		if (s == ctx->curclient->session)
			cur = idx;
		idx++;

		cdata = window_choose_data_create(ctx);
		if (args->argc != 0)
			cdata->action = xstrdup(args->argv[0]);
		else
			cdata->action = xstrdup("switch-client -t '%%'");
		cdata->idx = s->idx;

		cdata->client->references++;
		cdata->session->references++;

		cdata->ft_template = xstrdup(template);
		format_add(cdata->ft, "line", "%u", idx);
		format_session(cdata->ft, s);

		window_choose_add(wl->window->active, cdata);
	}

	window_choose_ready(wl->window->active,
	    cur, cmd_choose_session_callback, cmd_choose_session_free);

	return (0);
}

void
cmd_choose_session_callback(struct window_choose_data *cdata)
{
	struct session	*s;

	if (cdata == NULL)
		return;
	if (cdata->client->flags & CLIENT_DEAD)
		return;

	s = session_find_by_index(cdata->idx);
	if (s == NULL)
		return;

	cdata->raw_format = xstrdup(s->name);
	window_choose_ctx(cdata);
}

void
cmd_choose_session_free(struct window_choose_data *cdata)
{
	if (cdata == NULL)
		return;

	cdata->client->references--;
	cdata->session->references--;

	xfree(cdata->ft_template);
	xfree(cdata->action);
	format_free(cdata->ft);
	xfree(cdata);
}
