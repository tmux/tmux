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
#include <string.h>

#include "tmux.h"

/*
 * Find window containing text.
 */

int	cmd_find_window_exec(struct cmd *, struct cmd_ctx *);

u_int	cmd_find_window_match_flags(struct args *);
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

int
cmd_find_window_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args			*args = self->args;
	struct window_choose_data	*cdata;
	struct session			*s;
	struct winlink			*wl, *wm;
	struct window_pane		*wp;
	ARRAY_DECL(, u_int)	 	 list_idx;
	ARRAY_DECL(, char *)	 	 list_ctx;
	char				*str, *sres, *sctx, *searchstr;
	const char			*template;
	u_int				 i, line, match_flags;

	if (ctx->curclient == NULL) {
		ctx->error(ctx, "must be run interactively");
		return (-1);
	}
	s = ctx->curclient->session;

	if ((wl = cmd_find_window(ctx, args_get(args, 't'), NULL)) == NULL)
		return (-1);

	if ((template = args_get(args, 'F')) == NULL)
		template = DEFAULT_FIND_WINDOW_TEMPLATE;

	match_flags = cmd_find_window_match_flags(args);
	str = args->argv[0];

	ARRAY_INIT(&list_idx);
	ARRAY_INIT(&list_ctx);

	xasprintf(&searchstr, "*%s*", str);
	RB_FOREACH(wm, winlinks, &s->windows) {
		i = 0;
		TAILQ_FOREACH(wp, &wm->window->panes, entry) {
			i++;

			if ((match_flags & CMD_FIND_WINDOW_BY_NAME) &&
			    fnmatch(searchstr, wm->window->name, 0) == 0)
				sctx = xstrdup("");
			else {
				sres = NULL;
				if (match_flags & CMD_FIND_WINDOW_BY_CONTENT) {
					sres = window_pane_search(
					    wp, str, &line);
				}

				if (sres == NULL &&
				    (!(match_flags & CMD_FIND_WINDOW_BY_TITLE) ||
				     fnmatch(searchstr, wp->base.title, 0) != 0))
					continue;

				if (sres == NULL) {
					xasprintf(&sctx,
					    "pane %u title: \"%s\"", i - 1,
					    wp->base.title);
				} else {
					xasprintf(&sctx,
					    "pane %u line %u: \"%s\"", i - 1,
					    line + 1, sres);
					xfree(sres);
				}
			}

			ARRAY_ADD(&list_idx, wm->idx);
			ARRAY_ADD(&list_ctx, sctx);
			break;
		}
	}
	xfree(searchstr);

	if (ARRAY_LENGTH(&list_idx) == 0) {
		ctx->error(ctx, "no windows matching: %s", str);
		ARRAY_FREE(&list_idx);
		ARRAY_FREE(&list_ctx);
		return (-1);
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

		cdata = window_choose_data_create(ctx);
		cdata->idx = wm->idx;
		cdata->client->references++;

		cdata->ft_template = xstrdup(template);
		format_add(cdata->ft, "line", "%u", i);
		format_add(cdata->ft, "window_find_matches", "%s",
			ARRAY_ITEM(&list_ctx, i));
		format_session(cdata->ft, s);
		format_winlink(cdata->ft, s, wm);

		window_choose_add(wl->window->active, cdata);
	}

	window_choose_ready(wl->window->active,
	    0, cmd_find_window_callback, cmd_find_window_free);

out:

	ARRAY_FREE(&list_idx);
	ARRAY_FREE(&list_ctx);

	return (0);
}

void
cmd_find_window_callback(struct window_choose_data *cdata)
{
	struct session	*s;

	if (cdata == NULL)
		return;

	s = cdata->session;
	if (!session_alive(s))
		return;

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

	xfree(cdata->ft_template);
	format_free(cdata->ft);
	xfree(cdata);
}
