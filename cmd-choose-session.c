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

#include "tmux.h"

/*
 * Enter choice mode to choose a session.
 */

int	cmd_choose_session_exec(struct cmd *, struct cmd_ctx *);

void	cmd_choose_session_callback(void *, int);

const struct cmd_entry cmd_choose_session_entry = {
	"choose-session", NULL,
	CMD_TARGET_WINDOW_USAGE,
	0,
	cmd_target_init,
	cmd_target_parse,
	cmd_choose_session_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

struct cmd_choose_session_data {
	u_int	client;
};

int
cmd_choose_session_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct cmd_choose_session_data	*cdata;
	struct winlink			*wl;
	struct session			*s;
	u_int			 	 i, idx, cur;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return (-1);

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		return (0);

	cur = idx = 0;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL)
			continue;
		if (s == ctx->curclient->session)
			cur = idx;
		idx++;

		window_choose_add(wl->window->active, i,
		    "%s: %u windows [%ux%u]%s", s->name,
		    winlink_count(&s->windows), s->sx, s->sy,
		    s->flags & SESSION_UNATTACHED ? "" : " (attached)");
	}

	cdata = xmalloc(sizeof *cdata);
	cdata->client = server_client_index(ctx->curclient);

	window_choose_ready(
	    wl->window->active, cur, cmd_choose_session_callback, cdata);

	return (0);
}

void
cmd_choose_session_callback(void *data, int idx)
{
	struct cmd_choose_session_data	*cdata = data;
	struct client  			*c;

	if (idx != -1 && cdata->client <= ARRAY_LENGTH(&clients) - 1) {
		c = ARRAY_ITEM(&clients, cdata->client);
		if (c != NULL && (u_int) idx <= ARRAY_LENGTH(&sessions) - 1) {
			c->session = ARRAY_ITEM(&sessions, idx);
			recalculate_sizes();
			server_redraw_client(c);
		}
	}
	xfree(cdata);
}
