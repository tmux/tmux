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

#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Find window containing text.
 */

enum cmd_retval	 cmd_find_window_exec(struct cmd *, struct cmd_ctx *);

void	cmd_find_window_callback(struct window_choose_data *);
void	cmd_find_window_free(struct window_choose_data *);

/* Flags for determining matching behavior. */
#define CMD_FIND_WINDOW_BY_TITLE   0x1
#define CMD_FIND_WINDOW_BY_CONTENT 0x2
#define CMD_FIND_WINDOW_BY_NAME    0x4

#define CMD_FIND_WINDOW_ALL		\
	(CMD_FIND_WINDOW_BY_TITLE |	\
	 CMD_FIND_WINDOW_BY_CONTENT |	\
	 CMD_FIND_WINDOW_BY_NAME)

const struct cmd_entry cmd_find_window_entry = {
	"find-window", "findw",
	"F:CNt:T", 1, 4,
	"[-CNT] [-F format] " CMD_TARGET_WINDOW_USAGE " match-string",
	0,
	NULL,
	NULL,
	cmd_find_window_exec
};

struct cmd_find_window_data {
	struct winlink	*wl;
	char		*list_ctx;
	u_int		 pane_id;
};
ARRAY_DECL(cmd_find_window_data_list, struct cmd_find_window_data);

u_int	cmd_find_window_match_flags(struct args *);
void	cmd_find_window_match(struct cmd_find_window_data_list *, int,
	    struct winlink *, const char *, const char *);

u_int
cmd_find_window_match_flags(struct args *args)
{
	u_int	match_flags = 0;

	/* Turn on flags based on the options. */
	if (args_has(args, 'T'))
		match_flags |= CMD_FIND_WINDOW_BY_TITLE;
	if (args_has(args, 'C'))
		match_flags |= CMD_FIND_WINDOW_BY_CONTENT;
	if (args_has(args, 'N'))
		match_flags |= CMD_FIND_WINDOW_BY_NAME;

	/* If none of the flags were set, default to matching anything. */
	if (match_flags == 0)
		match_flags = CMD_FIND_WINDOW_ALL;

	return (match_flags);
}

void
cmd_find_window_match(struct cmd_find_window_data_list *find_list,
    int match_flags, struct winlink *wl, const char *str, const char *searchstr)
{
	struct cmd_find_window_data	 find_data;
	struct window_pane		*wp;
	u_int				 i, line;
	char				*sres;

	memset(&find_data, 0, sizeof find_data);

	i = 0;
	TAILQ_FOREACH(wp, &wl->window->panes, entry) {
		i++;

		if ((match_flags & CMD_FIND_WINDOW_BY_NAME) &&
		    fnmatch(searchstr, wl->window->name, 0) == 0) {
			find_data.list_ctx = xstrdup("");
			break;
		}

		if ((match_flags & CMD_FIND_WINDOW_BY_TITLE) &&
		    fnmatch(searchstr, wp->base.title, 0) == 0) {
			xasprintf(&find_data.list_ctx,
			    "pane %u title: \"%s\"", i - 1, wp->base.title);
			break;
		}

		if (match_flags & CMD_FIND_WINDOW_BY_CONTENT &&
		    (sres = window_pane_search(wp, str, &line)) != NULL) {
			xasprintf(&find_data.list_ctx,
			    "pane %u line %u: \"%s\"", i - 1, line + 1, sres);
			free(sres);
			break;
		}
	}
	if (find_data.list_ctx != NULL) {
		find_data.wl = wl;
		find_data.pane_id = i - 1;
		ARRAY_ADD(find_list, find_data);
	}
}

enum cmd_retval
cmd_find_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct window_choose_data	*cdata;
	struct session			*s;
	struct winlink			*wl, *wm;
	struct cmd_find_window_data_list find_list;
	char				*str, *searchstr;
	const char			*template;
	u_int				 i, match_flags;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (CMD_RETURN_ERROR);
	}
	s = ctx->curclient->session;

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (CMD_RETURN_ERROR);

	if ((template = args_get(args, 'F')) == NULL)
		template = FIND_WINDOW_TEMPLATE;

	match_flags = cmd_find_window_match_flags(args);
	str = args->argv[0];

	ARRAY_INIT(&find_list);

	xasprintf(&searchstr, "*%s*", str);
	RB_FOREACH(wm, winlinks, &s->windows)
	    cmd_find_window_match (&find_list, match_flags, wm, str, searchstr);
	free(searchstr);

	if (ARRAY_LENGTH(&find_list) == 0) {
		ctx->error(ctx, "no windows matching: %s", str);
		ARRAY_FREE(&find_list);
		return (CMD_RETURN_ERROR);
	}

	if (ARRAY_LENGTH(&find_list) == 1) {
		if (session_select(s, ARRAY_FIRST(&find_list).wl->idx) == 0)
			server_redraw_session(s);
		recalculate_sizes();
		goto out;
	}

	if (window_pane_set_mode(wl->window->active, &window_choose_mode) != 0)
		goto out;

	for (i = 0; i < ARRAY_LENGTH(&find_list); i++) {
		wm = ARRAY_ITEM(&find_list, i).wl;

		cdata = window_choose_data_create(ctx);
		cdata->idx = wm->idx;
		cdata->client->references++;
		cdata->wl = wm;

		cdata->ft_template = xstrdup(template);
		cdata->pane_id = ARRAY_ITEM(&find_list, i).pane_id;

		format_add(cdata->ft, "line", "%u", i);
		format_add(cdata->ft, "window_find_matches", "%s",
		    ARRAY_ITEM(&find_list, i).list_ctx);
		format_session(cdata->ft, s);
		format_winlink(cdata->ft, s, wm);
		format_window_pane(cdata->ft, wm->window->active);

		window_choose_add(wl->window->active, cdata);
	}

	window_choose_ready(wl->window->active,
	    0, cmd_find_window_callback, cmd_find_window_free);

out:
	ARRAY_FREE(&find_list);
	return (CMD_RETURN_NORMAL);
}

void
cmd_find_window_callback(struct window_choose_data *cdata)
{
	struct session		*s;
	struct window_pane	*wp;

	if (cdata == NULL)
		return;

	s = cdata->session;
	if (!session_alive(s))
		return;

	wp = window_pane_at_index(cdata->wl->window, cdata->pane_id);
	if (wp != NULL && window_pane_visible(wp))
		window_set_active_pane(cdata->wl->window, wp);

	if (session_select(s, cdata->idx) == 0) {
		server_redraw_session(s);
		recalculate_sizes();
	}
}

void
cmd_find_window_free(struct window_choose_data *cdata)
{
	if (cdata == NULL)
		return;

	cdata->session->references--;

	free(cdata->ft_template);
	format_free(cdata->ft);
	free(cdata);
}
