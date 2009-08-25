/* $Id: cmd-choose-window.c,v 1.15 2009-08-25 13:53:39 tcunha Exp $ */

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

#include "tmux.h"

/*
 * Enter choice mode to choose a window.
 */

int	cmd_choose_window_exec(struct cmd *, struct cmd_ctx *);

void	cmd_choose_window_callback(void *, int);
void	cmd_choose_window_free(void *);

const struct cmd_entry cmd_choose_window_entry = {
	"choose-window", NULL,
	CMD_TARGET_WINDOW_USAGE " [template]",
	CMD_ARG01, 0,
	cmd_target_init,
	cmd_target_parse,
	cmd_choose_window_exec,
	cmd_target_free,
	cmd_target_print
};

struct cmd_choose_window_data {
	u_int		 client;
	u_int		 session;
	char   		*template;
};

int
cmd_choose_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct cmd_choose_window_data	*cdata;
	struct session			*s;
	struct winlink			*wl, *wm;
	struct window			*w;
	u_int			 	 idx, cur;
	char				 flag, *title;
	const char			*left, *right;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}
	s = ctx->curclient->session;

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return (-1);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (0);

	cur = idx = 0;
	RB_FOREACH(wm, winlinks, &s->windows) {
		w = wm->window;

		if (wm == s->curw)
			cur = idx;
		idx++;

		flag = ' ';
		if (session_alert_has(s, wm, WINDOW_ACTIVITY))
			flag = '#';
		else if (session_alert_has(s, wm, WINDOW_BELL))
			flag = '!';
		else if (session_alert_has(s, wm, WINDOW_CONTENT))
			flag = '+';
		else if (wm == s->curw)
			flag = '*';
		else if (wm == SLIST_FIRST(&s->lastw))
			flag = '-';

		title = w->active->screen->title;
		if (wm == wl)
			title = w->active->base.title;
		left = " \"";
		right = "\"";
		if (*title == '\0')
			left = right = "";

		window_choose_add(wl->window->active,
		    wm->idx, "%3d: %s%c [%ux%u] (%u panes)%s%s%s",
		    wm->idx, w->name, flag, w->sx, w->sy, window_count_panes(w),
		    left, title, right);
	}

	cdata = xmalloc(sizeof *cdata);
	if (session_index(s, &cdata->session) != 0)
		fatalx("session not found");
	if (data->arg != NULL)
		cdata->template = xstrdup(data->arg);
	else
		cdata->template = xstrdup("select-window -t '%%'");
	cdata->client = server_client_index(ctx->curclient);

	window_choose_ready(wl->window->active, 
	    cur, cmd_choose_window_callback, cmd_choose_window_free, cdata);

 	return (0);
}

void
cmd_choose_window_callback(void *data, int idx)
{
	struct cmd_choose_window_data	*cdata = data;
	struct client			*c;
	struct session			*s;
	struct cmd_list			*cmdlist;
	struct cmd_ctx			 ctx;
	char				*target, *template, *cause;

	if (idx == -1)
		return;
	if (cdata->client > ARRAY_LENGTH(&clients) - 1)
		return;
	c = ARRAY_ITEM(&clients, cdata->client);
	if (cdata->session > ARRAY_LENGTH(&sessions) - 1)
		return;
	s = ARRAY_ITEM(&sessions, cdata->session);
	if (c->session != s)
		return;

	xasprintf(&target, "%s:%d", s->name, idx);
	template = cmd_template_replace(cdata->template, target, 1);
	xfree(target);

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
	ctx.curclient = c;

	ctx.error = key_bindings_error;
	ctx.print = key_bindings_print;
	ctx.info = key_bindings_info;

	ctx.cmdclient = NULL;

	cmd_list_exec(cmdlist, &ctx);
	cmd_list_free(cmdlist);
}

void
cmd_choose_window_free(void *data)
{
	struct cmd_choose_window_data	*cdata = data;

	xfree(cdata->template);
	xfree(cdata);
}
