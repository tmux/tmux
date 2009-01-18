/* $Id: cmd-find-window.c,v 1.1 2009-01-18 17:20:52 nicm Exp $ */

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

#include <string.h>

#include "tmux.h"

/*
 * Find window containing text.
 */

void	cmd_find_window_exec(struct cmd *, struct cmd_ctx *);

void	cmd_find_window_callback(void *, int);
char   *cmd_find_window_search(struct window_pane *, const char *);

const struct cmd_entry cmd_find_window_entry = {
	"find-window", NULL,
	CMD_TARGET_WINDOW_USAGE,
	CMD_ARG1,
	cmd_target_init,
	cmd_target_parse,
	cmd_find_window_exec,
	cmd_target_send,
	cmd_target_recv,
	cmd_target_free,
	cmd_target_print
};

struct cmd_find_window_data {
	u_int	session;
};

void
cmd_find_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct cmd_target_data		*data = self->data;
	struct cmd_find_window_data	*cdata;
	struct session			*s = ctx->curclient->session;
	struct winlink			*wl, *wm;
	struct window			*w;
	struct window_pane		*wp;
	ARRAY_DECL(, u_int)	 	 list_idx;
	ARRAY_DECL(, char *)	 	 list_ctx;
	char				*sres, *sctx;
	u_int				 i;

	if (ctx->curclient == NULL)
		return;

	if ((wl = cmd_find_window(ctx, data->target, NULL)) == NULL)
		return;
	
	ARRAY_INIT(&list_idx);
	ARRAY_INIT(&list_ctx);

	RB_FOREACH(wm, winlinks, &s->windows) {
		i = 0;
		TAILQ_FOREACH(wp, &wm->window->panes, entry) {
			i++;

			if (strstr(wm->window->name, data->arg) != NULL)
				sctx = xstrdup("");
			else {
				sres = cmd_find_window_search(wp, data->arg);
				if (sres == NULL &&
				    strstr(wp->base.title, data->arg) == NULL)
					continue;
				
				if (sres == NULL) {
					xasprintf(&sctx,
					    "pane %u title: \"%s\"", i - 1,
					    wp->base.title);
				} else {
					xasprintf(&sctx, "\"%s\"", sres);
					xfree(sres);
				}
			}
				
			ARRAY_ADD(&list_idx, wm->idx);
			ARRAY_ADD(&list_ctx, sctx);			
		}
	}

	if (ARRAY_LENGTH(&list_idx) == 0) {
		ctx->error(ctx, "no windows matching: %s", data->arg);
		ARRAY_FREE(&list_idx);
		ARRAY_FREE(&list_ctx);
		return;
	}

	if (ARRAY_LENGTH(&list_idx) == 1) {
		if (session_select(s, ARRAY_FIRST(&list_idx)) == 0)
			server_redraw_session(s);
		recalculate_sizes();
		goto out;
	}

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		goto out;

	for (i = 0; i < ARRAY_LENGTH(&list_idx); i++) {
		wm = winlink_find_by_index(
		    &s->windows, ARRAY_ITEM(&list_idx, i));
		w = wm->window;
		
		sctx = ARRAY_ITEM(&list_ctx, i);
		window_choose_add(wl->window->active,
		    wm->idx, "%3d: %s [%ux%u] (%u panes) %s", wm->idx, w->name,
		    w->sx, w->sy, window_count_panes(w), sctx);
		xfree(sctx);
	}

	cdata = xmalloc(sizeof *cdata);
	if (session_index(s, &cdata->session) != 0)
		fatalx("session not found");

	window_choose_ready(
	    wl->window->active, 0, cmd_find_window_callback, cdata);

out:
	ARRAY_FREE(&list_idx);
	ARRAY_FREE(&list_ctx);

	if (ctx->cmdclient != NULL)
		server_write_client(ctx->cmdclient, MSG_EXIT, NULL, 0);
}

void
cmd_find_window_callback(void *data, int idx)
{
	struct cmd_find_window_data	*cdata = data;
	struct session			*s;

	if (idx != -1 && cdata->session <= ARRAY_LENGTH(&sessions) - 1) {
		s = ARRAY_ITEM(&sessions, cdata->session);
		if (s != NULL && session_select(s, idx) == 0)
			server_redraw_session(s);
		recalculate_sizes();
	}
	xfree(cdata);
}

char *
cmd_find_window_search(struct window_pane *wp, const char *searchstr)
{
	char			*buf, *s;
	size_t			 off;
	const struct grid_cell	*gc;
	u_int			 i, j, k;
	u_char			 data[4];

	buf = xmalloc(1);
				
	for (j = 0; j < screen_size_y(&wp->base); j++) {
		off = 0;
		for (i = 0; i < screen_size_x(&wp->base); i++) {
			gc = grid_view_peek_cell(wp->base.grid, i, j);
			utf8_split(gc->data, data);
			
			buf = xrealloc(buf, 1, off + 4);
			for (k = 0; k < sizeof data; k++) {
				if (data[k] == 0xff)
					break;
				buf[off++] = data[k];
			}
		}
		while (off > 0 && buf[off - 1] == ' ')
			off--;
		buf[off] = '\0';
		
		if ((s = strstr(buf, searchstr)) != NULL) {
			s = section_string(buf, off, s - buf, 40);
			xfree(buf);
			return (s);
		}
	}

	xfree(buf);
	return (NULL);
}
