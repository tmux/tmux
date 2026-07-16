/* $OpenBSD: cmd-display-menu.c,v 1.52 2026/07/14 19:07:03 nicm Exp $ */

/*
 * Copyright (c) 2019 Nicholas Marriott <nicholas.marriott@gmail.com>
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
 * Display a menu on a client.
 */

static enum args_parse_type	cmd_display_menu_args_parse(struct args *,
				    u_int, char **);
static enum cmd_retval		cmd_display_menu_exec(struct cmd *,
				    struct cmdq_item *);
static enum cmd_retval		cmd_display_popup_exec(struct cmd *,
				    struct cmdq_item *);

const struct cmd_entry cmd_display_menu_entry = {
	.name = "display-menu",
	.alias = "menu",

	.args = { "b:c:C:H:s:S:MOt:T:x:y:", 1, -1, cmd_display_menu_args_parse },
	.usage = "[-MO] [-b border-lines] [-c target-client] "
		 "[-C starting-choice] [-H selected-style] [-s style] "
		 "[-S border-style] " CMD_TARGET_PANE_USAGE " [-T title] "
		 "[-x position] [-y position] name [key] [command] ...",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK|CMD_CLIENT_CFLAG,
	.exec = cmd_display_menu_exec
};

const struct cmd_entry cmd_display_popup_entry = {
	.name = "display-popup",
	.alias = "popup",

	.args = { "Bb:Cc:d:e:Eh:kNs:S:t:T:w:x:y:", 0, -1, NULL },
	.usage = "[-BCEkN] [-b border-lines] [-c target-client] "
		 "[-d start-directory] [-e environment] [-h height] "
		 "[-s style] [-S border-style] " CMD_TARGET_PANE_USAGE
		 " [-T title] [-w width] [-x position] [-y position] "
		 "[shell-command [argument ...]]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK|CMD_CLIENT_CFLAG,
	.exec = cmd_display_popup_exec
};

static enum args_parse_type
cmd_display_menu_args_parse(struct args *args, u_int idx, __unused char **cause)
{
	u_int			 i = 0;
	enum args_parse_type	 type = ARGS_PARSE_STRING;

	for (;;) {
		type = ARGS_PARSE_STRING;
		if (i == idx)
			break;
		if (*args_string(args, i++) == '\0')
			continue;

		type = ARGS_PARSE_STRING;
		if (i++ == idx)
			break;

		type = ARGS_PARSE_COMMANDS_OR_STRING;
		if (i++ == idx)
			break;
	}
	return (type);
}

static int
cmd_display_menu_get_menu_pos(struct client *tc, struct cmdq_item *item,
    struct args *args, u_int *px, u_int *py, u_int w, u_int h)
{
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct key_event	*event = cmdq_get_event(item);
	struct session		*s = tc->session;
	struct winlink		*wl = target->wl;
	struct window		*window = target->w;
	struct window_pane	*wp = target->wp;
	struct style_ranges	*ranges = NULL;
	struct style_range	*sr = NULL;
	const char		*xp, *yp;
	char			*p;
	u_int			 line, ox, oy, sx, sy, lines, position;
	long			 n, max_x, max_y, mouse_x = 0, mouse_y = 0;
	struct format_tree	*ft;

	max_x = window->sx > w ? window->sx - w : 0;
	max_y = window->sy > h ? window->sy - h : 0;

	tty_window_offset(&tc->tty, &ox, &oy, &sx, &sy);

	ft = format_create_from_target(item);
	if (event->m.valid) {
		mouse_x = event->m.x + ox;
		if (event->m.statusat == 0) {
			if (event->m.y >= event->m.statuslines)
				mouse_y = event->m.y - event->m.statuslines + oy;
			else
				mouse_y = oy;
		} else if (event->m.statusat > 0 &&
		    event->m.y >= (u_int)event->m.statusat) {
			mouse_y = oy + sy - 1;
		} else
			mouse_y = event->m.y + oy;
		format_add(ft, "popup_mouse_x", "%ld", mouse_x);
		format_add(ft, "popup_mouse_y", "%ld", mouse_y);
	}

	format_add(ft, "popup_last_x", "%u", window->menu_last_px);
	format_add(ft, "popup_last_y", "%u", window->menu_last_py + h);

	lines = status_line_size(tc);
	position = options_get_number(s->options, "status-position");
	if (status_at_line(tc) != -1 && lines != 0) {
		for (line = 0; line < lines; line++) {
			ranges = &tc->status.entries[line].ranges;
			TAILQ_FOREACH(sr, ranges, entry) {
				if (sr->type != STYLE_RANGE_WINDOW)
					continue;
				if (sr->argument == (u_int)wl->idx)
					break;
			}
			if (sr != NULL)
				break;
		}
		if (sr != NULL) {
			format_add(ft, "popup_window_status_line_x", "%u",
			    sr->start + ox);
			if (position == 0) {
				format_add(ft, "popup_window_status_line_y",
				    "%u", h);
			} else {
				format_add(ft, "popup_window_status_line_y",
				    "%u", window->sy);
			}
		}
		if (position == 0)
			format_add(ft, "popup_status_line_y", "%u", h);
		else
			format_add(ft, "popup_status_line_y", "%u", window->sy);
	}

	format_add(ft, "popup_width", "%u", w);
	format_add(ft, "popup_height", "%u", h);

	n = ((long)window->sx - 1) / 2 - w / 2;
	if (n < 0)
		format_add(ft, "popup_centre_x", "%u", 0);
	else
		format_add(ft, "popup_centre_x", "%ld", n);
	n = ((long)window->sy - 1) / 2 + h / 2;
	if (n >= window->sy)
		format_add(ft, "popup_centre_y", "%ld", max_y);
	else
		format_add(ft, "popup_centre_y", "%ld", n);

	if (event->m.valid) {
		n = mouse_x - w / 2;
		if (n < 0)
			format_add(ft, "popup_mouse_centre_x", "%u", 0);
		else
			format_add(ft, "popup_mouse_centre_x", "%ld", n);
		n = mouse_y - h / 2;
		if (n + h >= window->sy)
			format_add(ft, "popup_mouse_centre_y", "%ld", max_y);
		else
			format_add(ft, "popup_mouse_centre_y", "%ld", n);
		n = mouse_y + h;
		if (n >= window->sy)
			format_add(ft, "popup_mouse_top", "%u", window->sy - 1);
		else
			format_add(ft, "popup_mouse_top", "%ld", n);
		n = mouse_y - h;
		if (n < 0)
			format_add(ft, "popup_mouse_bottom", "%u", 0);
		else
			format_add(ft, "popup_mouse_bottom", "%ld", n);
	}

	n = wp->yoff + h;
	if (n >= window->sy)
		format_add(ft, "popup_pane_top", "%ld", max_y);
	else
		format_add(ft, "popup_pane_top", "%ld", n);
	format_add(ft, "popup_pane_bottom", "%u", wp->yoff + wp->sy);
	format_add(ft, "popup_pane_left", "%u", wp->xoff);
	n = (long)wp->xoff + wp->sx - w;
	if (n < 0)
		format_add(ft, "popup_pane_right", "%u", 0);
	else
		format_add(ft, "popup_pane_right", "%ld", n);

	xp = args_get(args, 'x');
	if (xp == NULL || strcmp(xp, "C") == 0)
		xp = "#{popup_centre_x}";
	else if (strcmp(xp, "R") == 0)
		xp = "#{popup_pane_right}";
	else if (strcmp(xp, "P") == 0)
		xp = "#{popup_pane_left}";
	else if (strcmp(xp, "M") == 0)
		xp = "#{popup_mouse_centre_x}";
	else if (strcmp(xp, "L") == 0)
		xp = "#{popup_last_x}";
	else if (strcmp(xp, "W") == 0)
		xp = "#{popup_window_status_line_x}";
	p = format_expand(ft, xp);
	n = strtol(p, NULL, 10);
	if (n < 0)
		n = 0;
	if (n > max_x)
		n = max_x;
	*px = n;
	log_debug("%s: -x: %s = %s = %u (-w %u)", __func__, xp, p, *px, w);
	free(p);

	yp = args_get(args, 'y');
	if (yp == NULL || strcmp(yp, "C") == 0)
		yp = "#{popup_centre_y}";
	else if (strcmp(yp, "P") == 0)
		yp = "#{popup_pane_bottom}";
	else if (strcmp(yp, "M") == 0)
		yp = "#{popup_mouse_top}";
	else if (strcmp(yp, "L") == 0)
		yp = "#{popup_last_y}";
	else if (strcmp(yp, "S") == 0)
		yp = "#{popup_status_line_y}";
	else if (strcmp(yp, "W") == 0)
		yp = "#{popup_window_status_line_y}";
	p = format_expand(ft, yp);
	n = strtol(p, NULL, 10);
	if (n < h)
		n = 0;
	else
		n -= h;
	if (n < 0)
		n = 0;
	if (n > max_y)
		n = max_y;
	*py = n;
	log_debug("%s: -y: %s = %s = %u (-h %u)", __func__, yp, p, *py, h);
	free(p);

	format_free(ft);
	return (1);
}

static enum cmd_retval
cmd_display_menu_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct key_event	*event = cmdq_get_event(item);
	struct client		*tc = cmdq_get_target_client(item);
	struct menu		*menu = NULL;
	struct menu_item	 menu_item;
	const char		*key, *name, *value;
	const char		*style = args_get(args, 's');
	const char		*border_style = args_get(args, 'S');
	const char		*selected_style = args_get(args, 'H');
	enum box_lines		 lines = BOX_LINES_DEFAULT;
	char			*title, *cause = NULL;
	int			 flags = 0, starting_choice = 0;
	u_int			 px, py, i, count = args_count(args);
	struct options		*o = target->s->curw->window->options;
	struct options_entry	*oe;

	if (args_has(args, 'C')) {
		if (strcmp(args_get(args, 'C'), "-") == 0)
			starting_choice = -1;
		else {
			starting_choice = args_strtonum(args, 'C', 0, UINT_MAX,
			    &cause);
			if (cause != NULL) {
				cmdq_error(item, "starting choice %s", cause);
				goto fail;
			}
		}
	}

	if (args_has(args, 'T'))
		title = format_single_from_target(item, args_get(args, 'T'));
	else
		title = xstrdup("");
	menu = menu_create(title);
	free(title);

	for (i = 0; i != count; /* nothing */) {
		name = args_string(args, i++);
		if (*name == '\0') {
			menu_add_item(menu, NULL, item, tc, target);
			continue;
		}

		if (count - i < 2) {
			cmdq_error(item, "not enough arguments");
			goto fail;
		}
		key = args_string(args, i++);

		menu_item.name = name;
		menu_item.key = key_string_lookup_string(key);
		menu_item.command = args_string(args, i++);

		menu_add_item(menu, &menu_item, item, tc, target);
	}
	if (menu == NULL) {
		cmdq_error(item, "invalid menu arguments");
		goto fail;
	}
	if (menu->count == 0)
		goto out;
	if (!cmd_display_menu_get_menu_pos(tc, item, args, &px, &py,
	    menu->width + 4, menu->count + 2))
		goto out;

	value = args_get(args, 'b');
	if (value != NULL) {
		oe = options_get(o, "menu-border-lines");
		lines = options_find_choice(options_table_entry(oe), value,
		    &cause);
		if (lines == -1) {
			cmdq_error(item, "menu-border-lines %s", cause);
			goto fail;
		}
	}

	if (args_has(args, 'O'))
		flags |= MENU_STAYOPEN;
	if (!event->m.valid && !args_has(args, 'M'))
		flags |= MENU_NOMOUSE;
	if (menu_display(menu, flags, starting_choice, item, px, py, tc, lines,
	    style, selected_style, border_style, target, NULL, NULL) != 0)
		goto out;
	return (CMD_RETURN_NORMAL);

out:
	menu_free(menu);
	return (CMD_RETURN_NORMAL);

fail:
	free(cause);
	menu_free(menu);
	return (CMD_RETURN_ERROR);
}

static enum pane_lines
cmd_display_popup_get_lines(const char *value, char **cause)
{
	const struct options_table_entry	*oe;

	if (value == NULL)
		value = "single";
	else if (strcmp(value, "rounded") == 0)
		value = "single";
	else if (strcmp(value, "padded") == 0)
		value = "spaces";

	oe = options_search("pane-border-lines");
	return (options_find_choice(oe, value, cause));
}

static enum cmd_retval
cmd_display_popup_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s = target->s;
	struct client		*tc = cmdq_get_target_client(item);
	struct winlink		*wl = target->wl;
	struct window		*w = wl->window;
	struct window_pane	*wp = target->wp, *new_wp = NULL;
	struct spawn_context	 sc = { 0 };
	struct layout_cell	*lc = NULL;
	struct layout_geometry	 lg;
	struct event_payload	*ep;
	struct cmd_find_state	 fs;
	const char		*value, *style = args_get(args, 's');
	const char		*border_style = args_get(args, 'S');
	char			*cause = NULL, *title = NULL;
	int			 flags = SPAWN_FLOATING|SPAWN_MODAL;
	enum pane_lines		 lines = PANE_LINES_SINGLE;
	u_int			 px, py, sx, sy, count = args_count(args);
	struct args_value	*av;
	long long		 ll;

	if (args_has(args, 'C')) {
		if (w->modal != NULL)
			server_kill_pane(w->modal);
		return (CMD_RETURN_NORMAL);
	}

	if (w->modal != NULL)
		return (CMD_RETURN_NORMAL);

	value = args_get(args, 'b');
	if (args_has(args, 'B'))
		lines = PANE_LINES_NONE;
	else if (value != NULL) {
		lines = cmd_display_popup_get_lines(value, &cause);
		if (cause != NULL) {
			cmdq_error(item, "pane-border-lines %s", cause);
			goto fail;
		}
	}

	sy = w->sy / 2;
	if (args_has(args, 'h')) {
		ll = args_percentage(args, 'h', 1, w->sy, w->sy, &cause);
		if (cause != NULL) {
			cmdq_error(item, "height %s", cause);
			goto fail;
		}
		sy = ll;
	}

	sx = w->sx / 2;
	if (args_has(args, 'w')) {
		ll = args_percentage(args, 'w', 1, w->sx, w->sx, &cause);
		if (cause != NULL) {
			cmdq_error(item, "width %s", cause);
			goto fail;
		}
		sx = ll;
	}

	if (sx > w->sx)
		sx = w->sx;
	if (sy > w->sy)
		sy = w->sy;
	if (lines != PANE_LINES_NONE && (sx < 3 || sy < 3))
		goto out;
	if (!cmd_display_menu_get_menu_pos(tc, item, args, &px, &py, sx, sy))
		goto out;

	lg.sx = sx;
	lg.sy = sy;
	lg.xoff = px;
	lg.yoff = py;
	if (lines != PANE_LINES_NONE) {
		lg.sx -= 2;
		lg.sy -= 2;
		lg.xoff++;
		lg.yoff++;
	}

	window_push_modal_zoom(w);
	lc = layout_floating_pane(w, wp, &lg);
	if (lc == NULL) {
		window_pop_modal_zoom(w);
		goto out;
	}

	sc.item = item;
	sc.s = s;
	sc.wl = wl;
	sc.tc = tc;
	sc.wp0 = wp;
	sc.lc = lc;
	sc.idx = -1;
	sc.cwd = args_get(args, 'd');
	sc.flags = flags;

	if (count != 1 || *args_string(args, 0) != '\0')
		args_to_vector(args, &sc.argc, &sc.argv);
	sc.environ = environ_create();
	av = args_first_value(args, 'e');
	while (av != NULL) {
		environ_put(sc.environ, av->string, 0);
		av = args_next_value(av);
	}

	new_wp = spawn_pane(&sc, &cause);
	if (new_wp == NULL) {
		cmdq_error(item, "create pane failed: %s", cause);
		free(cause);
		cause = NULL;
		window_pop_modal_zoom(w);
		goto fail;
	}

	options_set_number(new_wp->options, "pane-border-lines", lines);
	if (args_has(args, 'E') > 1)
		options_set_number(new_wp->options, "remain-on-exit", 2);
	else if (args_has(args, 'E'))
		options_set_number(new_wp->options, "remain-on-exit", 0);
	else
		options_set_number(new_wp->options, "remain-on-exit", 3);

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
	if (border_style != NULL) {
		if (options_set_string(new_wp->options, "pane-border-style", 0,
		    "%s", border_style) == NULL) {
			cmdq_error(item, "bad border style: %s", border_style);
			goto fail;
		}
		options_set_string(new_wp->options, "pane-active-border-style",
		    0, "%s", border_style);
	}
	if (args_has(args, 'T'))
		title = format_single_from_target(item, args_get(args, 'T'));
	if (title != NULL) {
		screen_set_title(&new_wp->base, title, 0);
		ep = event_payload_create();
		cmd_find_from_pane(&fs, new_wp, 0);
		event_payload_set_target(ep, &fs);
		event_payload_set_pane(ep, "pane", new_wp);
		event_payload_set_window(ep, "window", new_wp->window);
		event_payload_set_string(ep, "new_title", "%s", title);
		events_fire("pane-title-changed", ep);
	}

	cmd_find_from_winlink_pane(&fs, wl, new_wp, 0);
	cmdq_insert_hook(s, item, &fs, "after-split-window");

	new_wp->wait_item = item;
	server_redraw_session(s);
	if (sc.argv != NULL)
		cmd_free_argv(sc.argc, sc.argv);
	environ_free(sc.environ);
	free(title);
	return (CMD_RETURN_WAIT);

out:
	if (sc.argv != NULL)
		cmd_free_argv(sc.argc, sc.argv);
	environ_free(sc.environ);
	free(title);
	return (CMD_RETURN_NORMAL);

fail:
	free(cause);
	if (new_wp != NULL) {
		server_client_remove_pane(new_wp);
		layout_close_pane(new_wp);
		window_remove_pane(new_wp->window, new_wp);
	}
	if (sc.argv != NULL)
		cmd_free_argv(sc.argc, sc.argv);
	environ_free(sc.environ);
	free(title);
	return (CMD_RETURN_ERROR);
}
