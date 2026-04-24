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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Create a new pane.
 */

#define SPLIT_WINDOW_TEMPLATE "#{session_name}:#{window_index}.#{pane_index}"

static enum cmd_retval	cmd_split_window_exec(struct cmd *, struct cmdq_item *);

const struct cmd_entry cmd_new_pane_entry = {
	.name = "new-pane",
	.alias = "newp",

	.args = { "bc:de:fF:hIkl:m:M:p:PR:s:S:t:x:X:y:Y:vZ", 0, -1, NULL },
	.usage = "[-bdefhIklPvZ] [-c start-directory] [-e environment] "
		 "[-F format] [-l size] [-m message] [-M mode] "
		 "[-R inactive-border-style] [-s style] "
		 "[-S active-border-style] [-x width] [-X x-position]"
		 "[-y height] [-Y y-position]" CMD_TARGET_PANE_USAGE
		 "[shell-command [argument ...]]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_split_window_exec
};

const struct cmd_entry cmd_split_window_entry = {
	.name = "split-window",
	.alias = "splitw",

	.args = { "bc:de:fF:hIkl:m:M:p:PR:s:S:t:x:X:y:Y:vZ", 0, -1, NULL },
	.usage = "[-bdefhIklPvZ] [-c start-directory] [-e environment] "
		 "[-F format] [-l size] [-m message] [-M mode] "
		 "[-R inactive-border-style] [-s style] "
		 "[-S active-border-style] [-x width] [-X x-position]"
		 "[-y height] [-Y y-position]" CMD_TARGET_PANE_USAGE
		 "[shell-command [argument ...]]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_split_window_exec
};

static struct layout_cell *
cmd_split_window_get_floating_layout_cell(struct cmdq_item *item,
    struct args *args, struct window *w, struct window_pane *wp)
{
	struct layout_cell	*lc = NULL;
	char			*cause = NULL;
	u_int			 x, y, sx, sy;

	if (window_pane_float_geometry(w, wp, &x, &y, &sx, &sy, item, args, &cause)
	    != 0) {
		cmdq_error(item, "invalid float geometry %s", cause);
		free(cause);
		return (NULL);
	}

	/* Floating panes sit in layout cells which are not in the layout_root
	 * tree so we call it with parent == NULL.
	 */
	lc = layout_create_cell(NULL);
	lc->xoff = x;
	lc->yoff = y;
	lc->sx = sx;
	lc->sy = sy;

	return (lc);
}

static struct layout_cell *
cmd_split_window_get_tiled_layout_cell(struct cmdq_item *item,
    struct args *args, struct window *w, struct window_pane *wp, int flags)
{
	enum layout_type	 type;
	struct layout_cell	*lc = NULL;
	char			*cause = NULL;
	int			 size;

	if (wp->flags & PANE_FLOATING) {
		cmdq_error(item, "can't split a floating pane");
		return (NULL);
	}

	if (window_pane_tile_geometry(w, wp, &size, &flags, &type, item, args,
	    &cause) != 0) {
		cmdq_error(item, "invalid tiled geometry %s", cause);
		free(cause);
		return (NULL);
	}

	window_push_zoom(wp->window, 1, args_has(args, 'Z'));
	lc = layout_split_pane(wp, type, size, flags);
	if (lc == NULL) 
		cmdq_error(item, "no space for new pane");

	return (lc);
}

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
	struct window_pane	*wp = target->wp, *new_wp;
	struct layout_cell	*lc = NULL;
	struct cmd_find_state	 fs;
	int			 input, flags = 0, is_floating = 0;
	const char		*template, *style;
	char			*cause = NULL, *cp;
	struct args_value	*av;
	u_int			 count = args_count(args);

	if (args_has(args, 'M')) {
		is_floating = strcasecmp(args_get(args, 'M'), "f") == 0;
	} else {
		if (cmd_get_entry(self) == &cmd_new_pane_entry)
			is_floating = 1; /* default new-pane */
	}

	input = (args_has(args, 'I') && count == 0);

	flags = is_floating ? SPAWN_FLOATING : 0;
	if (args_has(args, 'b'))
		flags |= SPAWN_BEFORE;
	if (args_has(args, 'f'))
		flags |= SPAWN_FULLSIZE;
	if (input || (count == 1 && *args_string(args, 0) == '\0'))
		flags |= SPAWN_EMPTY;

	if (is_floating)
		lc = cmd_split_window_get_floating_layout_cell(item, args, w,
			wp);
	else
		lc = cmd_split_window_get_tiled_layout_cell(item, args, w, wp,
			flags);
	if (lc == NULL)
		return (CMD_RETURN_ERROR);

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
	if (args_has(args, 'd'))
		sc.flags |= SPAWN_DETACHED;
	if (args_has(args, 'Z'))
		sc.flags |= SPAWN_ZOOM;

	if ((new_wp = spawn_pane(&sc, &cause)) == NULL) {
		cmdq_error(item, "create pane failed: %s", cause);
		free(cause);
		if (sc.argv != NULL)
			cmd_free_argv(sc.argc, sc.argv);
		environ_free(sc.environ);
		return (CMD_RETURN_ERROR);
	}

	style = args_get(args, 's');
	if (style != NULL) {
		if (options_set_string(new_wp->options, "window-style", 0,
		    "%s", style) == NULL) {
			cmdq_error(item, "bad style: %s", style);
			return (CMD_RETURN_ERROR);
		}
		options_set_string(new_wp->options, "window-active-style", 0,
		    "%s", style);
		new_wp->flags |= (PANE_REDRAW|PANE_STYLECHANGED|PANE_THEMECHANGED);
	}
	style = args_get(args, 'S');
	if (style != NULL) {
		if (options_set_string(new_wp->options,
		    "pane-active-border-style", 0, "%s", style) == NULL) {
			cmdq_error(item, "bad active border style: %s", style);
			return (CMD_RETURN_ERROR);
		}
	}
	style = args_get(args, 'R');
	if (style != NULL) {
		if (options_set_string(new_wp->options, "pane-border-style", 0,
		    "%s", style) == NULL) {
			cmdq_error(item, "bad inactive border style: %s", style);
			return (CMD_RETURN_ERROR);
		}
	}
	if (args_has(args, 'k') || args_has(args, 'm')) {
		options_set_number(new_wp->options, "remain-on-exit", 3);
		if (args_has(args, 'm'))
			options_set_string(new_wp->options,
				"remain-on-exit-format",
				0, "%s", args_get(args, 'm'));
	}

	if (input) {
		switch (window_pane_start_input(new_wp, item, &cause)) {
		case -1:
			server_client_remove_pane(new_wp);
			if (!is_floating)
				layout_close_pane(new_wp);
			window_remove_pane(wp->window, new_wp);
			cmdq_error(item, "%s", cause);
			free(cause);
			if (sc.argv != NULL)
				cmd_free_argv(sc.argc, sc.argv);
			environ_free(sc.environ);
			return (CMD_RETURN_ERROR);
		case 1:
			input = 0;
			break;
		}
	}
	if (!args_has(args, 'd'))
		cmd_find_from_winlink_pane(current, wl, new_wp, 0);
	window_pop_zoom(wp->window);
	server_redraw_window(wp->window);
	server_status_session(s);

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
	return (CMD_RETURN_NORMAL);
}
