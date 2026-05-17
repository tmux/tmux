/* $OpenBSD$ */

/*
 * Copyright (c) 2026 Michael Grant <mgrant@gmail.com>
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
 * Hide or show panes.
 */

static enum cmd_retval cmd_hide_pane_hide_exec(struct cmd *, struct cmdq_item *);
static enum cmd_retval cmd_hide_pane_show_exec(struct cmd *, struct cmdq_item *);

static enum cmd_retval cmd_hide_pane_hide(struct window *, struct window_pane *,
    struct cmdq_item *);
static enum cmd_retval cmd_hide_pane_show(struct window *, struct window_pane *);

const struct cmd_entry cmd_hide_pane_entry = {
	.name = "hide-pane",
	.alias = "hidep",

	.args = { "at:", 0, 1, NULL },
	.usage = "[-a] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_hide_pane_hide_exec
};

const struct cmd_entry cmd_show_pane_entry = {
	.name = "show-pane",
	.alias = "showp",

	.args = { "at:", 0, 1, NULL },
	.usage = "[-a] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_hide_pane_show_exec
};


static enum cmd_retval
cmd_hide_pane_hide_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct window_pane	*wp, *active_pane = w->active;
	u_int			 id;
	char			*cause = NULL;
	enum cmd_retval		 rv;

	if (args_has(args, 'a')) {
		TAILQ_FOREACH(wp, &w->z_index, zentry) {
			if (!window_pane_visible(wp) || wp == active_pane)
				continue;
			rv = cmd_hide_pane_hide(w, wp, item);
			if (rv != CMD_RETURN_NORMAL)
				return (rv);
		}
		return (CMD_RETURN_NORMAL);
	} else {
		wp = target->wp;
		if (wp == NULL) {
			id = args_strtonum_and_expand(args, 't', 0, INT_MAX,
			    item, &cause);
			if (cause != NULL) {
				cmdq_error(item, "%s target pane", cause);
				return (CMD_RETURN_ERROR);
			}
			wp = window_pane_find_by_id(id);
		}
		if (wp == NULL) {
			cmdq_error(item, "No target pane to hide.");
			return (CMD_RETURN_ERROR);
		}
		return (cmd_hide_pane_hide(w, wp, item));
	}
}

static enum cmd_retval
cmd_hide_pane_show_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct window_pane	*wp;
	u_int			 id;
	char			*cause = NULL;
	enum cmd_retval		 rv;

	if (args_has(args, 'a')) {
		TAILQ_FOREACH(wp, &w->z_index, zentry) {
			if (!window_pane_visible(wp))
				continue;
			rv = cmd_hide_pane_show(w, wp);
			if (rv != CMD_RETURN_NORMAL)
				return (rv);
		}
		return (CMD_RETURN_NORMAL);
	} else {
		wp = target->wp;
		if (wp == NULL) {
			id = args_strtonum_and_expand(args, 't', 0, INT_MAX,
			    item, &cause);
			if (cause != NULL) {
				cmdq_error(item, "%s target pane", cause);
				return (CMD_RETURN_ERROR);
			}
			wp = window_pane_find_by_id(id);
		}
		if (wp == NULL) {
			cmdq_error(item, "No target pane to show.");
			return (CMD_RETURN_ERROR);
		}
		return (cmd_hide_pane_show(w, wp));
	}
}

static enum cmd_retval
cmd_hide_pane_hide(struct window *w, struct window_pane *wp,
    __unused struct cmdq_item *item)
{
	struct window_pane	*pwp = NULL;

	if (wp->flags & PANE_HIDDEN)
		return (CMD_RETURN_NORMAL);

	if (wp == w->active) {
		/*
		 * Unzoom before searching: under zoom, window_pane_visible
		 * returns false for every non-active pane.
		 */
		if (w->flags & WINDOW_ZOOMED)
			window_unzoom(w, 1);
		/* Find previous active pane. */
		TAILQ_FOREACH(pwp, &w->last_panes, sentry) {
			if (pwp != wp && window_pane_visible(pwp))
				break;
		}
		if (pwp == NULL) {
			TAILQ_FOREACH(pwp, &w->z_index, zentry) {
				if (pwp != wp &&
				    window_pane_visible(pwp))
					break;
			}
		}
	}

	wp->flags |= PANE_HIDDEN;

	if (w->layout_root != NULL) {
		wp->saved_layout_cell = wp->layout_cell;
		layout_hide_cell(w, wp->layout_cell);
		layout_fix_offsets(w);
		layout_fix_panes(w, NULL);
	}

	window_pane_stack_remove(&w->last_panes, wp);
	if (pwp != NULL) {
		window_set_active_pane(w, pwp, 1);
	} else if (wp == w->active) {
		/* No visible previous active pane; null active pane
		 * to show dots background. */
		w->active = NULL;
		if (options_get_number(global_options, "focus-events"))
			window_pane_update_focus(wp);
		notify_window("window-pane-changed", w);
		notify_window("window-layout-changed", w);
		server_redraw_window(w);
	} else {
		notify_window("window-layout-changed", w);
		server_redraw_window(w);
	}

	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_hide_pane_show(struct window *w, struct window_pane *wp)
{
	wp->flags &= ~PANE_HIDDEN;

	/* Fix pane offsets and sizes. */
	if (w->layout_root != NULL && wp->saved_layout_cell != NULL) {
		wp->layout_cell = wp->saved_layout_cell;
		wp->saved_layout_cell = NULL;
		layout_show_cell(w, wp->layout_cell);
		layout_fix_offsets(w);
		layout_fix_panes(w, NULL);
	}

	window_set_active_pane(w, wp, 1);

	notify_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}
