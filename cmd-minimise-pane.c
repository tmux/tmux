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
 * Increase or decrease pane size.
 */

static enum cmd_retval cmd_minimise_pane_minimise_exec(struct cmd *, struct cmdq_item *);
static enum cmd_retval cmd_minimise_pane_unminimise_exec(struct cmd *, struct cmdq_item *);

static enum cmd_retval cmd_minimise_pane_minimise(struct window *, struct window_pane *);
static enum cmd_retval cmd_minimise_pane_unminimise(struct window *, struct window_pane *);

const struct cmd_entry cmd_minimise_pane_entry = {
	.name = "minimise-pane",
	.alias = "minimize-pane",

	.args = { "at:", 0, 1, NULL },
	.usage = "[-a] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_minimise_pane_minimise_exec
};

const struct cmd_entry cmd_unminimise_pane_entry = {
	.name = "unminimise-pane",
	.alias = "unminimize-pane",

	.args = { "at:", 0, 1, NULL },
	.usage = "[-a] " CMD_TARGET_PANE_USAGE,

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK,
	.exec = cmd_minimise_pane_unminimise_exec
};


static enum cmd_retval
cmd_minimise_pane_minimise_exec(struct cmd *self, struct cmdq_item *item)
{
	__attribute((unused)) struct args	*args = cmd_get_args(self);
	struct cmd_find_state			*target = cmdq_get_target(item);
	struct winlink				*wl = target->wl;
	struct window				*w = wl->window;
	struct window_pane			*wp;
	u_int					 id;
	char					*cause = NULL;
	enum cmd_retval				 rv;

	if (args_has(args, 'a')) {
		TAILQ_FOREACH(wp, &w->z_index, zentry) {
			if (!window_pane_visible(wp))
				continue;
			rv = cmd_minimise_pane_minimise(w, wp);
			if (rv != CMD_RETURN_NORMAL)
				return(rv);
		}
		return (CMD_RETURN_NORMAL);
	} else {
		wp = target->wp;
		if (wp == NULL) {
			id = args_strtonum_and_expand(args, 't', 0, INT_MAX, item, &cause);
			if (cause != NULL) {
				cmdq_error(item, "%s target pane", cause);
				return (CMD_RETURN_ERROR);
			}
			wp = window_pane_find_by_id(id);
		}
		if (wp == NULL) {
			cmdq_error(item, "No target pane to miminise.");
			return (CMD_RETURN_ERROR);
		}
		return(cmd_minimise_pane_minimise(w, wp));
	}
}

static enum cmd_retval
cmd_minimise_pane_unminimise_exec(struct cmd *self, struct cmdq_item *item)
{
	__attribute((unused)) struct args	*args = cmd_get_args(self);
	struct cmd_find_state			*target = cmdq_get_target(item);
	struct winlink				*wl = target->wl;
	struct window				*w = wl->window;
	struct window_pane			*wp;
	u_int					 id;
	char					*cause = NULL;
	enum cmd_retval				 rv;

	if (args_has(args, 'a')) {
		TAILQ_FOREACH(wp, &w->z_index, zentry) {
			if (!window_pane_visible(wp))
				continue;
			rv = cmd_minimise_pane_unminimise(w, wp);
			if (rv != CMD_RETURN_NORMAL)
				return(rv);
		}
		return (CMD_RETURN_NORMAL);
	} else {
		wp = target->wp;
		if (wp == NULL) {
			id = args_strtonum_and_expand(args, 't', 0, INT_MAX, item, &cause);
			if (cause != NULL) {
				cmdq_error(item, "%s target pane", cause);
				return (CMD_RETURN_ERROR);
			}
			wp = window_pane_find_by_id(id);
		}
		if (wp == NULL) {
			cmdq_error(item, "No target pane to unmiminise.");
			return (CMD_RETURN_ERROR);
		}
		return(cmd_minimise_pane_unminimise(w, wp));
	}
}

static enum cmd_retval
cmd_minimise_pane_minimise(struct window *w, struct window_pane *wp)
{
	struct window_pane	*wp2;

	wp->flags |= PANE_MINIMISED;
	window_deactivate_pane(w, wp, 1);

	/* Fix pane offsets and sizes. */
	if (w->layout_root != NULL) {
		wp->saved_layout_cell = wp->layout_cell;
		layout_minimise_cell(w, wp->layout_cell);
		layout_fix_offsets(w);
		layout_fix_panes(w, NULL);
	}

	/* Find next visible window in z-index. */
	TAILQ_FOREACH(wp2, &w->z_index, zentry) {
		if (!window_pane_visible(wp2))
			continue;
		break;
	}
	if (wp2 != NULL)
		window_set_active_pane(w, wp2, 1);

	notify_window("window-layout-changed", w);
	server_redraw_window(w);
	
	return (CMD_RETURN_NORMAL);
}

static enum cmd_retval
cmd_minimise_pane_unminimise(struct window *w, struct window_pane *wp)
{
	wp->flags &= ~PANE_MINIMISED;

	/* Fix pane offsets and sizes. */
	if (w->layout_root != NULL && wp->saved_layout_cell != NULL) {
		wp->layout_cell = wp->saved_layout_cell;
		wp->saved_layout_cell = NULL;
		layout_unminimise_cell(w, wp->layout_cell);
		layout_fix_offsets(w);
		layout_fix_panes(w, NULL);
	}

	window_set_active_pane(w, wp, 1);

	notify_window("window-layout-changed", w);
	server_redraw_window(w);

	return (CMD_RETURN_NORMAL);
}
