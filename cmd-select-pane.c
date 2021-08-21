/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Select pane.
 */

static enum cmd_retval	cmd_select_pane_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_select_pane_entry = {
	.name = "select-pane",
	.alias = "selectp",

	.args = { "DdegLlMmP:RT:t:UZ", 0, 0, NULL }, /* -P and -g deprecated */
	.usage = "[-DdeLlMmRUZ] [-T title] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_select_pane_exec
};

const struct cmd_entry cmd_last_pane_entry = {
	.name = "last-pane",
	.alias = "lastp",

	.args = { "det:Z", 0, 0, NULL },
	.usage = "[-deZ] " CMD_TARGET_WINDOW_USAGE,

	.target = { 't', CMD_FIND_WINDOW, 0 },

	.flags = 0,
	.exec = cmd_select_pane_exec
};

static void
cmd_select_pane_redraw(struct window *w)
{
	struct client	*c;

	/*
	 * Redraw entire window if it is bigger than the client (the
	 * offset may change), otherwise just draw borders.
	 */

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL || (c->flags & CLIENT_CONTROL))
			continue;
		if (c->session->curw->window == w && tty_window_bigger(&c->tty))
			server_redraw_client(c);
		else {
			if (c->session->curw->window == w)
				c->flags |= CLIENT_REDRAWBORDERS;
			if (session_has(c->session, w))
				c->flags |= CLIENT_REDRAWSTATUS;
		}

	}
}

static enum cmd_retval
cmd_select_pane_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	const struct cmd_entry	*entry = cmd_get_entry(self);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct client		*c = cmdq_get_client(item);
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct session		*s = target->s;
	struct window_pane	*wp = target->wp, *activewp, *lastwp, *markedwp;
	struct options		*oo = wp->options;
	char			*title;
	const char		*style;
	struct options_entry	*o;

	if (entry == &cmd_last_pane_entry || args_has(args, 'l')) {
		lastwp = w->last;
		if (lastwp == NULL && window_count_panes(w) == 2) {
			lastwp = TAILQ_PREV(w->active, window_panes, entry);
			if (lastwp == NULL)
				lastwp = TAILQ_NEXT(w->active, entry);
		}
		if (lastwp == NULL) {
			cmdq_error(item, "no last pane");
			return (CMD_RETURN_ERROR);
		}
		if (args_has(args, 'e')) {
			lastwp->flags &= ~PANE_INPUTOFF;
			server_redraw_window_borders(lastwp->window);
			server_status_window(lastwp->window);
		} else if (args_has(args, 'd')) {
			lastwp->flags |= PANE_INPUTOFF;
			server_redraw_window_borders(lastwp->window);
			server_status_window(lastwp->window);
		} else {
			if (window_push_zoom(w, 0, args_has(args, 'Z')))
				server_redraw_window(w);
			window_redraw_active_switch(w, lastwp);
			if (window_set_active_pane(w, lastwp, 1)) {
				cmd_find_from_winlink(current, wl, 0);
				cmd_select_pane_redraw(w);
			}
			if (window_pop_zoom(w))
				server_redraw_window(w);
		}
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'm') || args_has(args, 'M')) {
		if (args_has(args, 'm') && !window_pane_visible(wp))
			return (CMD_RETURN_NORMAL);
		if (server_check_marked())
			lastwp = marked_pane.wp;
		else
			lastwp = NULL;

		if (args_has(args, 'M') || server_is_marked(s, wl, wp))
			server_clear_marked();
		else
			server_set_marked(s, wl, wp);
		markedwp = marked_pane.wp;

		if (lastwp != NULL) {
			lastwp->flags |= (PANE_REDRAW|PANE_STYLECHANGED);
			server_redraw_window_borders(lastwp->window);
			server_status_window(lastwp->window);
		}
		if (markedwp != NULL) {
			markedwp->flags |= (PANE_REDRAW|PANE_STYLECHANGED);
			server_redraw_window_borders(markedwp->window);
			server_status_window(markedwp->window);
		}
		return (CMD_RETURN_NORMAL);
	}

	style = args_get(args, 'P');
	if (style != NULL) {
		o = options_set_string(oo, "window-style", 0, "%s", style);
		if (o == NULL) {
			cmdq_error(item, "bad style: %s", style);
			return (CMD_RETURN_ERROR);
		}
		options_set_string(oo, "window-active-style", 0, "%s", style);
		wp->flags |= (PANE_REDRAW|PANE_STYLECHANGED);
	}
	if (args_has(args, 'g')) {
		cmdq_print(item, "%s", options_get_string(oo, "window-style"));
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'L')) {
		window_push_zoom(w, 0, 1);
		wp = window_pane_find_left(wp);
		window_pop_zoom(w);
	} else if (args_has(args, 'R')) {
		window_push_zoom(w, 0, 1);
		wp = window_pane_find_right(wp);
		window_pop_zoom(w);
	} else if (args_has(args, 'U')) {
		window_push_zoom(w, 0, 1);
		wp = window_pane_find_up(wp);
		window_pop_zoom(w);
	} else if (args_has(args, 'D')) {
		window_push_zoom(w, 0, 1);
		wp = window_pane_find_down(wp);
		window_pop_zoom(w);
	}
	if (wp == NULL)
		return (CMD_RETURN_NORMAL);

	if (args_has(args, 'e')) {
		wp->flags &= ~PANE_INPUTOFF;
		server_redraw_window_borders(wp->window);
		server_status_window(wp->window);
		return (CMD_RETURN_NORMAL);
	}
	if (args_has(args, 'd')) {
		wp->flags |= PANE_INPUTOFF;
		server_redraw_window_borders(wp->window);
		server_status_window(wp->window);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'T')) {
		title = format_single_from_target(item, args_get(args, 'T'));
		if (screen_set_title(&wp->base, title)) {
			notify_pane("pane-title-changed", wp);
			server_redraw_window_borders(wp->window);
			server_status_window(wp->window);
		}
		free(title);
		return (CMD_RETURN_NORMAL);
	}

	if (c != NULL && c->session != NULL && (c->flags & CLIENT_ACTIVEPANE))
		activewp = server_client_get_pane(c);
	else
		activewp = w->active;
	if (wp == activewp)
		return (CMD_RETURN_NORMAL);
	if (window_push_zoom(w, 0, args_has(args, 'Z')))
		server_redraw_window(w);
	window_redraw_active_switch(w, wp);
	if (c != NULL && c->session != NULL && (c->flags & CLIENT_ACTIVEPANE))
		server_client_set_pane(c, wp);
	else if (window_set_active_pane(w, wp, 1))
		cmd_find_from_winlink_pane(current, wl, wp, 0);
	cmdq_insert_hook(s, item, current, "after-select-pane");
	cmd_select_pane_redraw(w);
	if (window_pop_zoom(w))
		server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}
