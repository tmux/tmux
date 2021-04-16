/* $OpenBSD$ */

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

static enum cmd_retval	cmd_display_menu_exec(struct cmd *,
			    struct cmdq_item *);
static enum cmd_retval	cmd_display_popup_exec(struct cmd *,
			    struct cmdq_item *);

const struct cmd_entry cmd_display_menu_entry = {
	.name = "display-menu",
	.alias = "menu",

	.args = { "c:t:OT:x:y:", 1, -1 },
	.usage = "[-O] [-c target-client] " CMD_TARGET_PANE_USAGE " [-T title] "
		 "[-x position] [-y position] name key command ...",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK|CMD_CLIENT_CFLAG,
	.exec = cmd_display_menu_exec
};

const struct cmd_entry cmd_display_popup_entry = {
	.name = "display-popup",
	.alias = "popup",

	.args = { "Cc:d:Eh:t:w:x:y:", 0, -1 },
	.usage = "[-CE] [-c target-client] [-d start-directory] [-h height] "
	         CMD_TARGET_PANE_USAGE " [-w width] "
	         "[-x position] [-y position] [command]",

	.target = { 't', CMD_FIND_PANE, 0 },

	.flags = CMD_AFTERHOOK|CMD_CLIENT_CFLAG,
	.exec = cmd_display_popup_exec
};

static int
cmd_display_menu_get_position(struct client *tc, struct cmdq_item *item,
    struct args *args, u_int *px, u_int *py, u_int w, u_int h)
{
	struct tty		*tty = &tc->tty;
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct key_event	*event = cmdq_get_event(item);
	struct session		*s = tc->session;
	struct winlink		*wl = target->wl;
	struct window_pane	*wp = target->wp;
	struct style_ranges	*ranges = NULL;
	struct style_range	*sr = NULL;
	const char		*xp, *yp;
	char			*p;
	int			 top;
	u_int			 line, ox, oy, sx, sy, lines, position;
	long			 n;
	struct format_tree	*ft;

	/*
	 * Work out the position from the -x and -y arguments. This is the
	 * bottom-left position.
	 */

	/* If the popup is too big, stop now. */
	if (w > tty->sx || h > tty->sy)
		return (0);

	/* Create format with mouse position if any. */
	ft = format_create_from_target(item);
	if (event->m.valid) {
		format_add(ft, "popup_mouse_x", "%u", event->m.x);
		format_add(ft, "popup_mouse_y", "%u", event->m.y);
	}

	/*
	 * If there are any status lines, add this window position and the
	 * status line position.
	 */
	top = status_at_line(tc);
	if (top != -1) {
		lines = status_line_size(tc);
		if (top == 0)
			top = lines;
		else
			top = 0;
		position = options_get_number(s->options, "status-position");

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
		if (line == lines)
			ranges = &tc->status.entries[0].ranges;

		if (sr != NULL) {
			format_add(ft, "popup_window_status_line_x", "%u",
			    sr->start);
			if (position == 0) {
				format_add(ft, "popup_window_status_line_y",
				    "%u", line + 1 + h);
			} else {
				format_add(ft, "popup_window_status_line_y",
				    "%u", tty->sy - lines + line);
			}
		}

		if (position == 0)
			format_add(ft, "popup_status_line_y", "%u", lines + h);
		else {
			format_add(ft, "popup_status_line_y", "%u",
			    tty->sy - lines);
		}
	} else
		top = 0;

	/* Popup width and height. */
	format_add(ft, "popup_width", "%u", w);
	format_add(ft, "popup_height", "%u", h);

	/* Position so popup is in the centre. */
	n = (long)(tty->sx - 1) / 2 - w / 2;
	if (n < 0)
		format_add(ft, "popup_centre_x", "%u", 0);
	else
		format_add(ft, "popup_centre_x", "%ld", n);
	n = (tty->sy - 1) / 2 + h / 2;
	if (n >= tty->sy)
		format_add(ft, "popup_centre_y", "%u", tty->sy - h);
	else
		format_add(ft, "popup_centre_y", "%ld", n);

	/* Position of popup relative to mouse. */
	if (event->m.valid) {
		n = (long)event->m.x - w / 2;
		if (n < 0)
			format_add(ft, "popup_mouse_centre_x", "%u", 0);
		else
			format_add(ft, "popup_mouse_centre_x", "%ld", n);
		n = event->m.y - h / 2;
		if (n + h >= tty->sy) {
			format_add(ft, "popup_mouse_centre_y", "%u",
			    tty->sy - h);
		} else
			format_add(ft, "popup_mouse_centre_y", "%ld", n);
		n = (long)event->m.y + h;
		if (n + h >= tty->sy)
			format_add(ft, "popup_mouse_top", "%u", tty->sy - h);
		else
			format_add(ft, "popup_mouse_top", "%ld", n);
		n = event->m.y - h;
		if (n < 0)
			format_add(ft, "popup_mouse_bottom", "%u", 0);
		else
			format_add(ft, "popup_mouse_bottom", "%ld", n);
	}

	/* Position in pane. */
	tty_window_offset(&tc->tty, &ox, &oy, &sx, &sy);
	n = top + wp->yoff - oy + h;
	if (n >= tty->sy)
		format_add(ft, "popup_pane_top", "%u", tty->sy - h);
	else
		format_add(ft, "popup_pane_top", "%ld", n);
	format_add(ft, "popup_pane_bottom", "%u", top + wp->yoff + wp->sy - oy);
	format_add(ft, "popup_pane_left", "%u", wp->xoff - ox);
	n = (long)wp->xoff + wp->sx - ox - w;
	if (n < 0)
		format_add(ft, "popup_pane_right", "%u", 0);
	else
		format_add(ft, "popup_pane_right", "%ld", n);

	/* Expand horizontal position. */
	xp = args_get(args, 'x');
	if (xp == NULL || strcmp(xp, "C") == 0)
		xp = "#{popup_centre_x}";
	else if (strcmp(xp, "R") == 0)
		xp = "#{popup_pane_right}";
	else if (strcmp(xp, "P") == 0)
		xp = "#{popup_pane_left}";
	else if (strcmp(xp, "M") == 0)
		xp = "#{popup_mouse_centre_x}";
	else if (strcmp(xp, "W") == 0)
		xp = "#{popup_window_status_line_x}";
	p = format_expand(ft, xp);
	n = strtol(p, NULL, 10);
	if (n + w >= tty->sx)
		n = tty->sx - w;
	else if (n < 0)
		n = 0;
	*px = n;
	log_debug("%s: -x: %s = %s = %u", __func__, xp, p, *px);
	free(p);

	/* Expand vertical position  */
	yp = args_get(args, 'y');
	if (yp == NULL || strcmp(yp, "C") == 0)
		yp = "#{popup_centre_y}";
	else if (strcmp(yp, "P") == 0)
		yp = "#{popup_pane_bottom}";
	else if (strcmp(yp, "M") == 0)
		yp = "#{popup_mouse_top}";
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
	if (n + h >= tty->sy)
		n = tty->sy - h;
	else if (n < 0)
		n = 0;
	*py = n;
	log_debug("%s: -y: %s = %s = %u", __func__, yp, p, *py);
	free(p);

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
	const char		*key;
	char			*title, *name;
	int			 flags = 0, i;
	u_int			 px, py;

	if (tc->overlay_draw != NULL)
		return (CMD_RETURN_NORMAL);

	if (args_has(args, 'T'))
		title = format_single_from_target(item, args_get(args, 'T'));
	else
		title = xstrdup("");
	menu = menu_create(title);

	for (i = 0; i != args->argc; /* nothing */) {
		name = args->argv[i++];
		if (*name == '\0') {
			menu_add_item(menu, NULL, item, tc, target);
			continue;
		}

		if (args->argc - i < 2) {
			cmdq_error(item, "not enough arguments");
			free(title);
			menu_free(menu);
			return (CMD_RETURN_ERROR);
		}
		key = args->argv[i++];

		menu_item.name = name;
		menu_item.key = key_string_lookup_string(key);
		menu_item.command = args->argv[i++];

		menu_add_item(menu, &menu_item, item, tc, target);
	}
	free(title);
	if (menu == NULL) {
		cmdq_error(item, "invalid menu arguments");
		return (CMD_RETURN_ERROR);
	}
	if (menu->count == 0) {
		menu_free(menu);
		return (CMD_RETURN_NORMAL);
	}
	if (!cmd_display_menu_get_position(tc, item, args, &px, &py,
	    menu->width + 4, menu->count + 2)) {
		menu_free(menu);
		return (CMD_RETURN_NORMAL);
	}

	if (args_has(args, 'O'))
		flags |= MENU_STAYOPEN;
	if (!event->m.valid)
		flags |= MENU_NOMOUSE;
	if (menu_display(menu, flags, item, px, py, tc, target, NULL,
	    NULL) != 0)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}

static enum cmd_retval
cmd_display_popup_exec(struct cmd *self, struct cmdq_item *item)
{
	struct args		*args = cmd_get_args(self);
	struct cmd_find_state	*target = cmdq_get_target(item);
	struct session		*s = target->s;
	struct client		*tc = cmdq_get_target_client(item);
	struct tty		*tty = &tc->tty;
	const char		*value, *shell[] = { NULL, NULL };
	const char		*shellcmd = NULL;
	char			*cwd, *cause, **argv = args->argv;
	int			 flags = 0, argc = args->argc;
	u_int			 px, py, w, h;

	if (args_has(args, 'C')) {
		server_client_clear_overlay(tc);
		return (CMD_RETURN_NORMAL);
	}
	if (tc->overlay_draw != NULL)
		return (CMD_RETURN_NORMAL);

	h = tty->sy / 2;
	if (args_has(args, 'h')) {
		h = args_percentage(args, 'h', 1, tty->sy, tty->sy, &cause);
		if (cause != NULL) {
			cmdq_error(item, "height %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	w = tty->sx / 2;
	if (args_has(args, 'w')) {
		w = args_percentage(args, 'w', 1, tty->sx, tty->sx, &cause);
		if (cause != NULL) {
			cmdq_error(item, "width %s", cause);
			free(cause);
			return (CMD_RETURN_ERROR);
		}
	}

	if (w > tty->sx - 1)
		w = tty->sx - 1;
	if (h > tty->sy - 1)
		h = tty->sy - 1;
	if (!cmd_display_menu_get_position(tc, item, args, &px, &py, w, h))
		return (CMD_RETURN_NORMAL);

	value = args_get(args, 'd');
	if (value != NULL)
		cwd = format_single_from_target(item, value);
	else
		cwd = xstrdup(server_client_get_cwd(tc, s));
	if (argc == 0)
		shellcmd = options_get_string(s->options, "default-command");
	else if (argc == 1)
		shellcmd = argv[0];
	if (argc <= 1 && (shellcmd == NULL || *shellcmd == '\0')) {
		shellcmd = NULL;
		shell[0] = options_get_string(s->options, "default-shell");
		if (!checkshell(shell[0]))
			shell[0] = _PATH_BSHELL;
		argc = 1;
		argv = (char**)shell;
	}

	if (args_has(args, 'E') > 1)
		flags |= POPUP_CLOSEEXITZERO;
	else if (args_has(args, 'E'))
		flags |= POPUP_CLOSEEXIT;
	if (popup_display(flags, item, px, py, w, h, shellcmd, argc, argv, cwd,
	    tc, s, NULL, NULL) != 0)
		return (CMD_RETURN_NORMAL);
	return (CMD_RETURN_WAIT);
}
