/* $OpenBSD: cmd-split-window.c,v 1.146 2026/07/21 12:28:43 nicm Exp $ */

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

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Create a new pane.
 */

#define SPLIT_WINDOW_TEMPLATE "#{session_name}:#{window_index}.#{pane_index}"

static enum cmd_retval	cmd_split_window_exec(struct cmd *, struct cmdq_item *);
static void		cmd_split_window_mouse_resize(struct client *,
			    struct mouse_event *);

const struct cmd_entry cmd_new_pane_entry = {
	.name = "new-pane",
	.alias = "newp",

	.args = { "bB:Cc:de:EfF:hIkl:LMm:Op:PR:s:S:t:T:vWx:X:y:Y:Z", 0, -1, NULL },
	.usage = "[-bCdefhIklMOPvWZ] [-B border-lines] "
		 "[-c start-directory] [-e environment] "
		 "[-F format] [-l size] [-m message] [-p percentage] "
		 "[-s style] [-S active-border-style] "
		 "[-R inactive-border-style] [-T title] [-x width] [-y height] "
		 "[-X x-position] [-Y y-position] " CMD_TARGET_PANE_USAGE " "
		 "[shell-command [argument ...]]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_split_window_exec
};

const struct cmd_entry cmd_split_window_entry = {
	.name = "split-window",
	.alias = "splitw",

	.args = { "bB:c:de:EfF:hIkl:m:p:PR:s:S:t:T:vWZ", 0, -1, NULL },
	.usage = "[-bdefhIklPvWZ] [-B border-lines] [-c start-directory] "
		 "[-e environment] [-F format] [-l size] [-m message] "
		 "[-p percentage] [-s style] [-S active-border-style] "
		 "[-R inactive-border-style] [-T title] "
	         CMD_TARGET_PANE_USAGE " [shell-command [argument ...]]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_split_window_exec
};

static enum cmd_retval
cmd_split_window_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*current = cmdq_get_current(item);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct spawn_context	 sc = { 0 };
	struct client		*tc = cmdq_get_target_client(item);
	struct session		*s = target->s;
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct window_pane	*wp = target->wp, *new_wp = NULL;
	struct layout_cell	*lc = NULL;
	struct event_payload	*ep;
	struct cmd_find_state	 fs;
	struct key_event	*event = cmdq_get_event(item);
	int			 input, empty, is_floating, flags = 0;
	const char		*template, *style, *value;
	char			*cause = NULL, *cp, *title;
	const struct options_table_entry *oe;
	struct args_value	*av;
	enum pane_lines		 lines;
	u_int			 count = args_count(args);

	if (cmd_get_entry(self) == &cmd_new_pane_entry)
		is_floating = !args_has(args, 'L');
	else {
		is_floating = window_pane_is_floating(wp);
		flags |= SPAWN_SPLIT;
	}

	if (args_has(args, 'O')) {
		if (!is_floating) {
			cmdq_error(item, "modal pane must be floating");
			return (CMD_RETURN_ERROR);
		}
		if (w->modal != NULL) {
			cmdq_error(item, "window already has a modal pane");
			return (CMD_RETURN_ERROR);
		}
	}

	if (args_has(args, 'M') && is_floating) {
		if (event == NULL || !event->m.valid || tc == NULL)
			return (CMD_RETURN_NORMAL);
	}

	if (is_floating)
		flags |= SPAWN_FLOATING;
	if (args_has(args, 'h'))
		flags |= SPAWN_HORIZONTAL;
	if (args_has(args, 'b'))
		flags |= SPAWN_BEFORE;
	if (args_has(args, 'f'))
		flags |= SPAWN_FULLSIZE;
	if (args_has(args, 'd'))
		flags |= SPAWN_DETACHED;
	if (args_has(args, 'Z'))
		flags |= SPAWN_ZOOM;
	if (args_has(args, 'O'))
		flags |= SPAWN_MODAL;

	input = args_has(args, 'I');
	if (input || (count == 1 && *args_string(args, 0) == '\0'))
		empty = 1;
	else
		empty = args_has(args, 'E');
	if (empty &&
	    count != 0 &&
	    (count != 1 || *args_string(args, 0) != '\0')) {
		cmdq_error(item, "command cannot be given for empty pane");
		return (CMD_RETURN_ERROR);
	}
	if (empty)
		flags |= SPAWN_EMPTY;

	if ((value = args_get(args, 'B')) == NULL)
		lines = window_get_pane_lines(w);
	else {
		oe = options_search("pane-border-lines");
		lines = options_find_choice(oe, value, &cause);
		if (cause != NULL) {
			cmdq_error(item, "pane-border-lines %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	if (flags & SPAWN_FLOATING) {
		lc = layout_get_floating_cell(item, args, lines, w, wp, flags,
		    &cause);
	} else
		lc = layout_get_tiled_cell(item, args, w, wp, flags, &cause);
	if (cause != NULL) {
		cmdq_error(item, "%s", cause);
		free(cause);
		return (CMD_RETURN_ERROR);
	}

	sc.item = item;
	sc.s = s;
	sc.wl = wl;

	sc.wp0 = wp;
	sc.lc = lc;

	args_to_vector(args, &sc.argc, &sc.argv);
	sc.environ = environ_create();

	av = args_first_value(args, 'e');
	while (av != NULL) {
		environ_put(sc.environ, av->string, 0);
		av = args_next_value(av);
	}

	sc.idx = -1;
	sc.cwd = args_get(args, 'c');
	sc.flags = flags;

	if ((new_wp = spawn_pane(&sc, &cause)) == NULL) {
		cmdq_error(item, "create pane failed: %s", cause);
		free(cause);
		/*
		 * spawn_pane has already torn the half-built pane down (its
		 * fork-failure path removes the pane and destroys the layout
		 * cell), so new_wp is NULL and there is nothing for fail to do.
		 */
		goto fail;
	}
	if (args_has(args, 'C') && args_has(args, 'O'))
		new_wp->flags |= PANE_CLOSEONCLICK;

	style = args_get(args, 's');
	if (style != NULL) {
		if (options_set_string(new_wp->options, "window-style", 0,
		    "%s", style) == NULL) {
			cmdq_error(item, "bad style: %s", style);
			goto fail;
		}
		options_set_string(new_wp->options, "window-active-style", 0,
		    "%s", style);
		new_wp->flags |= (PANE_REDRAW|PANE_STYLECHANGED|
		    PANE_THEMECHANGED);
	}
	style = args_get(args, 'S');
	if (style != NULL) {
		if (options_set_string(new_wp->options,
		    "pane-active-border-style", 0, "%s", style) == NULL) {
			cmdq_error(item, "bad active border style: %s", style);
			goto fail;
		}
	}
	style = args_get(args, 'R');
	if (style != NULL) {
		if (options_set_string(new_wp->options, "pane-border-style", 0,
		    "%s", style) == NULL) {
			cmdq_error(item, "bad inactive border style: %s",
			    style);
			goto fail;
		}
	}
	if (args_has(args, 'B'))
		options_set_number(new_wp->options, "pane-border-lines", lines);
	if (args_has(args, 'k') || args_has(args, 'm')) {
		options_set_number(new_wp->options, "remain-on-exit", 3);
		if (args_has(args, 'm')) {
			options_set_string(new_wp->options,
			    "remain-on-exit-format", 0, "%s",
			    args_get(args, 'm'));
		}
	}
	if (args_has(args, 'T')) {
		title = format_single_from_target(item, args_get(args, 'T'));
		screen_set_title(&new_wp->base, title, 0);
		ep = event_payload_create();
		cmd_find_from_pane(&fs, new_wp, 0);
		event_payload_set_target(ep, &fs);
		event_payload_set_pane(ep, "pane", new_wp);
		event_payload_set_window(ep, "window", new_wp->window);
		event_payload_set_string(ep, "new_title", "%s", title);
		events_fire("pane-title-changed", ep);
		free(title);
	}

	if (input) {
		switch (window_pane_start_input(new_wp, item, &cause)) {
		case -1:
			cmdq_error(item, "%s", cause);
			free(cause);
			goto fail;
		case 1:
			input = 0;
			break;
		}
	}
	if (~flags & SPAWN_DETACHED)
		cmd_find_from_winlink_pane(current, wl, new_wp, 0);

	if ((~flags & SPAWN_FLOATING) && !args_has(args, 'O')) {
		window_pop_zoom(wp->window);
		server_redraw_window(wp->window);
	}
	server_redraw_session(s);

	if (args_has(args, 'M') && is_floating) {
		tc->tty.mouse_last_pane = new_wp->id;
		tc->tty.mouse_drag_update = cmd_split_window_mouse_resize;
		cmd_split_window_mouse_resize(tc, &event->m);
	}

	if (args_has(args, 'P')) {
		if ((template = args_get(args, 'F')) == NULL)
			template = SPLIT_WINDOW_TEMPLATE;
		cp = format_single(item, template, tc, s, wl, new_wp);
		cmdq_print(item, "%s", cp);
		free(cp);
	}

	cmd_find_from_winlink_pane(&fs, wl, new_wp, 0);
	cmdq_insert_hook(s, item, &fs, "after-split-window");

	if (sc.argv != NULL)
		cmd_free_argv(sc.argc, sc.argv);
	environ_free(sc.environ);
	if (input)
		return (CMD_RETURN_WAIT);

	if (args_has(args, 'W')) {
		/*
		 * With -W, block this command queue item until the pane's
		 * command exits; window_pane_wait_finish will be called to
		 * continue it.
		 */
		new_wp->wait_item = item;
		return (CMD_RETURN_WAIT);
	}
	return (CMD_RETURN_NORMAL);

fail:
	/*
	 * If the pane was spawned before we failed, tear it down here; this
	 * also destroys its layout cell. spawn_pane's own failure path has
	 * already done this, so new_wp is NULL in that case.
	 */
	if (new_wp != NULL) {
		server_client_remove_pane(new_wp);
		if (!is_floating)
			layout_close_pane(new_wp);
		window_remove_pane(wp->window, new_wp);
	} else if (args_has(args, 'O'))
		window_pop_modal_zoom(wp->window);
	if (sc.argv != NULL)
		cmd_free_argv(sc.argc, sc.argv);
	environ_free(sc.environ);

	return (CMD_RETURN_ERROR);

}

static void
cmd_split_window_mouse_resize(struct client *c, struct mouse_event *m)
{
	struct window_pane	*wp;
	struct window		*w;
	struct layout_cell	*lc;
	enum pane_lines		 lines;
	u_int			 sx, sy;
	int			 x, y, xoff, yoff, border;

	if (c->tty.mouse_last_pane == -1)
		return;
	wp = window_pane_find_by_id(c->tty.mouse_last_pane);
	if (wp == NULL || !window_pane_is_floating(wp)) {
		c->tty.mouse_drag_update = NULL;
		return;
	}
	w = wp->window;
	lc = wp->layout_cell;

	x = m->x + m->ox;
	y = m->y + m->oy;
	if (m->statusat == 0 && y >= (int)m->statuslines)
		y -= m->statuslines;
	else if (m->statusat > 0 && y >= m->statusat)
		y = m->statusat - 1;

	lines = window_pane_get_pane_lines(wp);
	border = (lines != PANE_LINES_NONE);

	if (x >= (int)c->tty.mouse_drag_x) {
		xoff = c->tty.mouse_drag_x + border;
		sx = x - c->tty.mouse_drag_x + 1;
	} else {
		sx = c->tty.mouse_drag_x - x + 1;
		xoff = c->tty.mouse_drag_x - sx + 1;
		if (border)
			xoff++;
	}
	if (y >= (int)c->tty.mouse_drag_y) {
		yoff = c->tty.mouse_drag_y + border;
		sy = y - c->tty.mouse_drag_y + 1;
	} else {
		sy = c->tty.mouse_drag_y - y + 1;
		yoff = c->tty.mouse_drag_y - sy + 1;
		if (border)
			yoff++;
	}

	if (border) {
		if (sx <= 2)
			sx = PANE_MINIMUM;
		else
			sx -= 2;
		if (sy <= 2)
			sy = PANE_MINIMUM;
		else
			sy -= 2;
	}
	if (sx < PANE_MINIMUM)
		sx = PANE_MINIMUM;
	if (sy < PANE_MINIMUM)
		sy = PANE_MINIMUM;

	layout_set_size(lc, sx, sy, xoff, yoff);
	layout_fix_panes(w, NULL);
	server_redraw_window(w);
	server_redraw_window_borders(w);
}
