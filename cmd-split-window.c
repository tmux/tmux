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

	.args = { "bc:de:fF:hH:Ikl:m:M:p:PR:s:S:t:w:x:y:vZ", 0, -1, NULL },
	.usage = "[-bdefhIklPvZ] [-c start-directory] [-e environment] "
		 "[-F format] [-H height] [-l size] [-m message] "
		 "[-M mode] [-R inactive-border-style] [-s style] "
		 "[-S active-border-style] [-w width] [-x x-position] "
		 "[-y y-position]" CMD_TARGET_PANE_USAGE
		 "[shell-command [argument ...]]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_split_window_exec
};

const struct cmd_entry cmd_split_window_entry = {
	.name = "split-window",
	.alias = "splitw",

	.args = { "bc:de:fF:hH:Ikl:m:M:p:PR:s:S:t:w:x:y:vZ", 0, -1, NULL },
	.usage = "[-bdefhIklPvZ] [-c start-directory] [-e environment] "
		 "[-F format] [-H height] [-l size] [-m message] "
		 "[-M mode] [-R inactive-border-style] [-s style] "
		 "[-S active-border-style] [-w width] [-x x-position] "
		 "[-y y-position]" CMD_TARGET_PANE_USAGE
		 "[shell-command [argument ...]]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = 0,
	.exec = cmd_split_window_exec
};

enum new_pane_mode {
	FLOATING,
	TILED,
	NONE,
};

static struct layout_cell *
cmd_split_window_get_floating_layout_cell(struct cmdq_item *item,
    struct args *args, struct window *w)
{
	struct layout_cell	*lc = NULL;
	char			*cause = NULL;
	int			 x, y;
	u_int			 sx = w->sx / 2, sy = w->sy / 2;
	static int		 last_x = 0, last_y = 0;

	if (last_x == 0) {
		x = 4;
	} else {
		x = (last_x += 4);
		if (last_x > (int)w->sx)
			x = 4;
	}
	if (last_y == 0) {
		y = 2;
	} else {
		y = (last_y += 2);
		if (last_y > (int)w->sy)
			y = 2;
	}
	if (args_has(args, 'w')) {
		sx = args_percentage_and_expand(args, 'w', 0, USHRT_MAX, w->sx,
			item, &cause);
		if (cause != NULL) {
			cmdq_error(item, "size %s", cause);
			free(cause);
			return (NULL);
		}
	}
	if (args_has(args, 'H')) {
		sy = args_percentage_and_expand(args, 'H', 0, USHRT_MAX, w->sy,
		    item, &cause);
		if (cause != NULL) {
			cmdq_error(item, "size %s", cause);
			free(cause);
			return (NULL);
		}
	}
	if (args_has(args, 'x')) {
		x = args_percentage_and_expand(args, 'x', 0, USHRT_MAX, w->sx,
		    item, &cause);
		if (cause != NULL) {
			cmdq_error(item, "size %s", cause);
			free(cause);
			return (NULL);
		}
	}
	if (args_has(args, 'y')) {
		y = args_percentage_and_expand(args, 'y', 0, USHRT_MAX, w->sy,
		    item, &cause);
		if (cause != NULL) {
			cmdq_error(item, "size %s", cause);
			free(cause);
			return (NULL);
		}
	}

	/* Floating panes sit in layout cells which are not in the layout_root
	 * tree so we call it with parent == NULL.
	 */
	lc = layout_create_cell(NULL);
	lc->xoff = x;
	lc->yoff = y;
	lc->sx = sx;
	lc->sy = sy;
	last_x = x;	/* Statically save last xoff & yoff so that new */
	last_y = y;	/* floating panes offset so they don't overlap. */

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
	u_int			 curval = 0;

	if (wp->flags & PANE_FLOATING) {
		cmdq_error(item, "can't split a floating pane");
		return (NULL);
	}

	type = LAYOUT_TOPBOTTOM;
	if (args_has(args, 'h'))
		type = LAYOUT_LEFTRIGHT;

	/* If the 'p' flag is dropped then this bit can be moved into 'l'. */
	if (args_has(args, 'l') || args_has(args, 'p')) {
		if (args_has(args, 'f')) {
			if (type == LAYOUT_TOPBOTTOM)
				curval = w->sy;
			else
				curval = w->sx;
		} else {
			if (type == LAYOUT_TOPBOTTOM)
				curval = wp->sy;
			else
				curval = wp->sx;
		}
	}

	size = -1;
	if (args_has(args, 'l')) {
		size = args_percentage_and_expand(args, 'l', 0, INT_MAX, curval,
		    item, &cause);
	} else if (args_has(args, 'p')) {
		size = args_strtonum_and_expand(args, 'p', 0, 100, item,
		    &cause);
		if (cause == NULL)
			size = curval * size / 100;
	}
	if (cause != NULL) {
		cmdq_error(item, "size %s", cause);
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
	int			 flags, input;
	const char		*template, *style;
	char			*cause = NULL, *cp;
	struct args_value	*av;
	u_int			 count = args_count(args);
	enum new_pane_mode	 pane_mode = NONE;

	if (args_has(args, 'M')) {
		if (strcasecmp(args_get(args, 'M'), "f") == 0)
			pane_mode = FLOATING;
		else if (strcasecmp(args_get(args, 'M'), "t") == 0)
			pane_mode = TILED;
		else
			pane_mode = NONE;
	} else {
		if (cmd_get_entry(self) == &cmd_new_pane_entry)
			pane_mode = FLOATING;
		else
			pane_mode = TILED;
	}

	input = (args_has(args, 'I') && count == 0);

	flags = pane_mode == FLOATING ? SPAWN_FLOATING : 0;
	if (args_has(args, 'b'))
		flags |= SPAWN_BEFORE;
	if (args_has(args, 'f'))
		flags |= SPAWN_FULLSIZE;
	if (input || (count == 1 && *args_string(args, 0) == '\0'))
		flags |= SPAWN_EMPTY;


	if (pane_mode == FLOATING)
		lc = cmd_split_window_get_floating_layout_cell(item, args, w);
	else if (pane_mode == TILED)
		lc = cmd_split_window_get_tiled_layout_cell(item, args, w, wp,
			flags);
	else {
		cmdq_error(item, "unrecognized pane mode '%s'",
		    args_get(args, 'M'));
		return (CMD_RETURN_ERROR);
	}
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
			if (pane_mode == TILED)
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
