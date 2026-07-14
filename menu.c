/* $OpenBSD: menu.c,v 1.69 2026/07/14 19:07:03 nicm Exp $ */

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

struct menu_data {
	struct window		*w;
	int			 flags;

	char			*style;
	char			*border_style;
	char			*selected_style;

	struct grid_cell	 style_gc;
	struct grid_cell	 border_style_gc;
	struct grid_cell	 selected_style_gc;
	enum box_lines		 border_lines;

	struct cmd_find_state	 fs;
	key_code		 key;
	struct mouse_event	 m;
	struct screen		 s;

	u_int			 px;
	u_int			 py;

	struct menu		*menu;
	int			 choice;

	menu_choice_cb		 cb;
	void			*data;
};

void
menu_add_items(struct menu *menu, const struct menu_item *items,
    struct cmdq_item *qitem, struct client *c, struct cmd_find_state *fs)
{
	const struct menu_item	*loop;

	for (loop = items; loop->name != NULL; loop++)
		menu_add_item(menu, loop, qitem, c, fs);
}

void
menu_add_item(struct menu *menu, const struct menu_item *item,
    struct cmdq_item *qitem, struct client *c, struct cmd_find_state *fs)
{
	struct menu_item	*new_item;
	const char		*key = NULL, *cmd, *suffix = "";
	char			*s, *trimmed, *name;
	u_int			 width, max_width;
	int			 line;
	size_t			 keylen, slen;

	line = (item == NULL || item->name == NULL || *item->name == '\0');
	if (line && menu->count == 0)
		return;
	if (line && menu->items[menu->count - 1].name == NULL)
		return;

	menu->items = xreallocarray(menu->items, menu->count + 1,
	    sizeof *menu->items);
	new_item = &menu->items[menu->count++];
	memset(new_item, 0, sizeof *new_item);

	if (line)
		return;

	if (fs != NULL)
		s = format_single_from_state(qitem, item->name, c, fs);
	else
		s = format_single(qitem, item->name, c, NULL, NULL, NULL);
	if (*s == '\0') { /* no item if empty after format expanded */
		free(s);
		menu->count--;
		return;
	}
	max_width = c->tty.sx - 4;

	slen = strlen(s);
	if (*s != '-' && item->key != KEYC_UNKNOWN && item->key != KEYC_NONE) {
		key = key_string_lookup_key(item->key, 0);
		keylen = strlen(key) + 3; /* 3 = space and two brackets */

		/*
		 * Add the key if it is shorter than a quarter of the available
		 * space or there is space for the entire item text and the
		 * key.
		 */
		if (keylen <= max_width / 4)
			max_width -= keylen;
		else if (keylen >= max_width || slen >= max_width - keylen)
			key = NULL;
	}

	if (slen > max_width) {
		max_width--;
		suffix = ">";
	}
	trimmed = format_trim_right(s, max_width);
	if (key != NULL) {
		xasprintf(&name, "%s%s#[default] #[align=right](%s)",
		    trimmed, suffix, key);
	} else
		xasprintf(&name, "%s%s", trimmed, suffix);
	free(trimmed);

	new_item->name = name;
	free(s);

	cmd = item->command;
	if (cmd != NULL) {
		if (fs != NULL)
			s = format_single_from_state(qitem, cmd, c, fs);
		else
			s = format_single(qitem, cmd, c, NULL, NULL, NULL);
	} else
		s = NULL;
	new_item->command = s;
	new_item->key = item->key;

	width = format_width(new_item->name);
	if (*new_item->name == '-')
		width--;
	if (width > menu->width)
		menu->width = width;
}

struct menu *
menu_create(const char *title)
{
	struct menu	*menu;

	menu = xcalloc(1, sizeof *menu);
	menu->title = xstrdup(title);
	menu->width = format_width(title);

	return (menu);
}

void
menu_free(struct menu *menu)
{
	u_int	i;

	if (menu == NULL)
		return;

	for (i = 0; i < menu->count; i++) {
		free((void *)menu->items[i].name);
		free((void *)menu->items[i].command);
	}
	free(menu->items);

	free((void *)menu->title);
	free(menu);
}

static void
menu_reapply_styles(struct menu_data *md)
{
	struct options		*o = md->w->options;
	struct format_tree	*ft;
	struct style		 sytmp;

	ft = format_create_defaults(NULL, NULL, md->fs.s, md->fs.wl, md->fs.wp);

	/* Reapply menu style from options. */
	memcpy(&md->style_gc, &grid_default_cell, sizeof md->style_gc);
	style_apply(&md->style_gc, o, "menu-style", ft);
	if (md->style != NULL) {
		style_set(&sytmp, &grid_default_cell);
		if (style_parse(&sytmp, &md->style_gc, md->style) == 0) {
			md->style_gc.fg = sytmp.gc.fg;
			md->style_gc.bg = sytmp.gc.bg;
		}
	}

	/* Reapply selected style from options. */
	memcpy(&md->selected_style_gc, &grid_default_cell,
	    sizeof md->selected_style_gc);
	style_apply(&md->selected_style_gc, o, "menu-selected-style", ft);
	if (md->selected_style != NULL) {
		style_set(&sytmp, &grid_default_cell);
		if (style_parse(&sytmp, &md->selected_style_gc,
		    md->selected_style) == 0) {
			md->selected_style_gc.fg = sytmp.gc.fg;
			md->selected_style_gc.bg = sytmp.gc.bg;
		}
	}

	/* Reapply border style from options. */
	memcpy(&md->border_style_gc, &grid_default_cell,
	    sizeof md->border_style_gc);
	style_apply(&md->border_style_gc, o, "menu-border-style", ft);
	if (md->border_style != NULL) {
		style_set(&sytmp, &grid_default_cell);
		if (style_parse(&sytmp, &md->border_style_gc,
		    md->border_style) == 0) {
			md->border_style_gc.fg = sytmp.gc.fg;
			md->border_style_gc.bg = sytmp.gc.bg;
		}
	}

	format_free(ft);
}

void
menu_update(struct menu_data *md)
{
	struct screen		*s = &md->s;
	struct menu		*menu = md->menu;
	struct screen_write_ctx	 ctx;

	menu_reapply_styles(md);

	screen_write_start(&ctx, s);
	screen_write_clearscreen(&ctx, 8);

	if (md->border_lines != BOX_LINES_NONE) {
		screen_write_box(&ctx, menu->width + 4, menu->count + 2,
		    md->border_lines, &md->border_style_gc, menu->title);
	}

	screen_write_menu(&ctx, menu, md->choice, md->border_lines,
	    &md->style_gc, &md->border_style_gc, &md->selected_style_gc);
	screen_write_stop(&ctx);
}

static void
menu_free_data(struct menu_data *md)
{
	if (md != NULL) {
		if (md->cb != NULL)
			md->cb(md->menu, UINT_MAX, KEYC_NONE, md->data);

		screen_free(&md->s);
		menu_free(md->menu);
		free(md->style);
		free(md->selected_style);
		free(md->border_style);
		free(md);
	}
}

void
menu_close(struct window *w)
{
	if (w->menu != NULL) {
		menu_free_data(w->menu);
		w->menu = NULL;

		redraw_invalidate_scene(w);
		window_update_focus(w);
		server_redraw_window(w);
	}
}

void
menu_destroy(struct window *w)
{
	menu_free_data(w->menu);
	w->menu = NULL;
}

void
menu_get_cursor(struct menu_data *md, u_int *cx, u_int *cy)
{
	*cx = md->px + 2;
	if (md->choice == -1)
		*cy = md->py;
	else
		*cy = md->py + 1 + md->choice;
}

struct screen *
menu_screen(struct menu_data *md)
{
	return (&md->s);
}

u_int
menu_width(struct menu_data *md)
{
	return (md->menu->width + 4);
}

u_int
menu_height(struct menu_data *md)
{
	return (md->menu->count + 2);
}

u_int
menu_x(struct menu_data *md)
{
	return (md->px);
}

u_int
menu_y(struct menu_data *md)
{
	return (md->py);
}

int
menu_key(struct client *c, struct menu_data *md, struct key_event *event)
{
	struct menu			*menu = md->menu;
	struct mouse_event		*m = &event->m;
	u_int				 i;
	int				 n = menu->count, old = md->choice;
	int				 move;
	const char			*name = NULL;
	const struct menu_item		*item;
	struct key_event		 saved_event;
	struct cmdq_state		*state;
	enum cmd_parse_status		 status;
	char				*error;

	if (KEYC_IS_MOUSE(event->key)) {
		/*
		 * A mouse move with no button held reports as a release, so
		 * treat it as highlight-only: it must never select or close the
		 * menu, otherwise a menu opened without a button already down
		 * (such as a submenu opened from another menu) would vanish as
		 * soon as the mouse moved over it.
		 */
		move = MOUSE_DRAG(m->b) && MOUSE_RELEASE(m->b);
		if (md->flags & MENU_NOMOUSE) {
			if (MOUSE_BUTTONS(m->b) != MOUSE_BUTTON_1)
				return (1);
			return (0);
		}
		if (m->x < md->px ||
		    m->x > md->px + 4 + menu->width ||
		    m->y < md->py + 1 ||
		    m->y > md->py + 1 + n - 1) {
			if (~md->flags & MENU_STAYOPEN) {
				if (!move && MOUSE_RELEASE(m->b))
					return (1);
			} else {
				if (!MOUSE_RELEASE(m->b) &&
				    !MOUSE_WHEEL(m->b) &&
				    !MOUSE_DRAG(m->b))
					return (1);
			}
			if (md->choice != -1) {
				md->choice = -1;
				server_redraw_window_menu(md->w);
			}
			return (0);
		}
		if (~md->flags & MENU_STAYOPEN) {
			if (!move && MOUSE_RELEASE(m->b))
				goto chosen;
		} else {
			if (!MOUSE_WHEEL(m->b) && !MOUSE_DRAG(m->b))
				goto chosen;
		}
		md->choice = m->y - (md->py + 1);
		if (md->choice != old)
			server_redraw_window_menu(md->w);
		return (0);
	}
	for (i = 0; i < (u_int)n; i++) {
		name = menu->items[i].name;
		if (name == NULL || *name == '-')
			continue;
		if ((event->key & ~KEYC_MASK_FLAGS) == menu->items[i].key) {
			md->choice = i;
			goto chosen;
		}
	}
	switch (event->key & ~KEYC_MASK_FLAGS) {
	case KEYC_BTAB:
	case KEYC_UP:
	case 'k':
		if (old == -1)
			old = 0;
		do {
			if (md->choice == -1 || md->choice == 0)
				md->choice = n - 1;
			else
				md->choice--;
			name = menu->items[md->choice].name;
		} while ((name == NULL || *name == '-') && md->choice != old);
		server_redraw_window_menu(md->w);
		return (0);
	case KEYC_BSPACE:
		if (~md->flags & MENU_TAB)
			break;
		return (1);
	case '\011': /* Tab */
		if (~md->flags & MENU_TAB)
			break;
		if (md->choice == n - 1)
			return (1);
		/* FALLTHROUGH */
	case KEYC_DOWN:
	case 'j':
		if (old == -1)
			old = 0;
		do {
			if (md->choice == -1 || md->choice == n - 1)
				md->choice = 0;
			else
				md->choice++;
			name = menu->items[md->choice].name;
		} while ((name == NULL || *name == '-') && md->choice != old);
		server_redraw_window_menu(md->w);
		return (0);
	case KEYC_PPAGE:
	case 'b'|KEYC_CTRL:
		if (md->choice < 6)
			md->choice = 0;
		else {
			i = 5;
			while (i > 0) {
				md->choice--;
				name = menu->items[md->choice].name;
				if (md->choice != 0 &&
				    (name != NULL && *name != '-'))
					i--;
				else if (md->choice == 0)
					break;
			}
		}
		server_redraw_window_menu(md->w);
		break;
	case KEYC_NPAGE:
		if (md->choice > n - 6) {
			md->choice = n - 1;
			name = menu->items[md->choice].name;
		} else {
			i = 5;
			while (i > 0) {
				md->choice++;
				name = menu->items[md->choice].name;
				if (md->choice != n - 1 &&
				    (name != NULL && *name != '-'))
					i--;
				else if (md->choice == n - 1)
					break;
			}
		}
		while ((name == NULL || *name == '-') && md->choice != 0) {
			md->choice--;
			name = menu->items[md->choice].name;
		}
		server_redraw_window_menu(md->w);
		break;
	case 'g':
	case KEYC_HOME:
		md->choice = 0;
		name = menu->items[md->choice].name;
		while ((name == NULL || *name == '-') && md->choice != n - 1) {
			md->choice++;
			name = menu->items[md->choice].name;
		}
		server_redraw_window_menu(md->w);
		break;
	case 'G':
	case KEYC_END:
		md->choice = n - 1;
		name = menu->items[md->choice].name;
		while ((name == NULL || *name == '-') && md->choice != 0) {
			md->choice--;
			name = menu->items[md->choice].name;
		}
		server_redraw_window_menu(md->w);
		break;
	case 'f'|KEYC_CTRL:
		break;
	case '\r':
		goto chosen;
	case '\033': /* Escape */
	case '['|KEYC_CTRL:
	case 'c'|KEYC_CTRL:
	case 'g'|KEYC_CTRL:
	case 'q':
		return (1);
	}
	return (0);

chosen:
	if (md->choice == -1)
		return (1);
	item = &menu->items[md->choice];
	if (item->name == NULL || *item->name == '-') {
		if (md->flags & MENU_STAYOPEN)
			return (0);
		return (1);
	}
	if (md->cb != NULL) {
		md->cb(md->menu, md->choice, item->key, md->data);
		md->cb = NULL;
		return (1);
	}

	if (md->key != KEYC_NONE) {
		memset(&saved_event, 0, sizeof saved_event);
		saved_event.key = md->key;
		memcpy(&saved_event.m, &md->m, sizeof saved_event.m);
		event = &saved_event;
	} else
		event = NULL;
	state = cmdq_new_state(&md->fs, event, 0);

	status = cmd_parse_and_append(item->command, NULL, c, state, &error);
	if (status == CMD_PARSE_ERROR) {
		cmdq_append(c, cmdq_get_error(error));
		free(error);
	}
	cmdq_free_state(state);

	return (1);
}

void
menu_resize(struct menu_data *md, struct window *w)
{
	u_int	nx, ny, sx, sy;

	if (md == NULL)
		return;

	nx = md->px;
	ny = md->py;

	sx = md->menu->width + 4;
	sy = md->menu->count + 2;

	if (nx + sx > w->sx) {
		if (w->sx <= sx)
			nx = 0;
		else
			nx = w->sx - sx;
	}

	if (ny + sy > w->sy) {
		if (w->sy <= sy)
			ny = 0;
		else
			ny = w->sy - sy;
	}
	md->px = nx;
	md->py = ny;
}

int
menu_display(struct menu *menu, int flags, int starting_choice,
    struct cmdq_item *item, u_int px, u_int py, struct client *c,
    enum box_lines lines, const char *style, const char *selected_style,
    const char *border_style, struct cmd_find_state *fs, menu_choice_cb cb,
    void *data)
{
	struct menu_data	*md;
	struct key_event	*event;
	int			 choice;
	const char		*name;
	u_int			 sx, sy;
	struct window		*w;
	struct options		*o;

	if (fs == NULL)
		w = c->session->curw->window;
	else
		w = fs->w;
	o = w->options;

	sx = menu->width + 4;
	sy = menu->count + 2;
	if (sx >= w->sx)
		px = 0;
	else if (px + sx > w->sx)
		px = w->sx - sx;
	if (sy >= w->sy)
		py = 0;
	else if (py + sy > w->sy)
		py = w->sy - sy;
	w->menu_last_px = px;
	w->menu_last_py = py;

	if (lines == BOX_LINES_DEFAULT)
		lines = options_get_number(o, "menu-border-lines");

	md = xcalloc(1, sizeof *md);
	md->w = w;
	md->flags = flags;
	md->border_lines = lines;
	md->key = KEYC_NONE;

	if (item != NULL) {
		event = cmdq_get_event(item);
		md->key = event->key;
		memcpy(&md->m, &event->m, sizeof md->m);
	}

	if (style != NULL)
		md->style = xstrdup(style);
	if (selected_style != NULL)
		md->selected_style = xstrdup(selected_style);
	if (border_style != NULL)
		md->border_style = xstrdup(border_style);

	if (fs != NULL)
		cmd_find_copy_state(&md->fs, fs);
	else if (cmd_find_from_window(&md->fs, w, 0) != 0)
		cmd_find_clear_state(&md->fs, 0);
	screen_init(&md->s, sx, sy, 0);
	if (~md->flags & MENU_NOMOUSE)
		md->s.mode |= (MODE_MOUSE_ALL|MODE_MOUSE_BUTTON);
	md->s.mode &= ~MODE_CURSOR;

	md->px = px;
	md->py = py;

	md->menu = menu;
	md->choice = -1;

	md->cb = cb;
	md->data = data;

	if (md->flags & MENU_NOMOUSE) {
		if (starting_choice >= (int)menu->count) {
			starting_choice = menu->count - 1;
			choice = starting_choice + 1;
			for (;;) {
				name = menu->items[choice - 1].name;
				if (name != NULL && *name != '-') {
					md->choice = choice - 1;
					break;
				}
				if (--choice == 0)
					choice = menu->count;
				if (choice == starting_choice + 1)
					break;
			}
		} else if (starting_choice >= 0) {
			choice = starting_choice;
			for (;;) {
				name = menu->items[choice].name;
				if (name != NULL && *name != '-') {
					md->choice = choice;
					break;
				}
				if (++choice == (int)menu->count)
					choice = 0;
				if (choice == starting_choice)
					break;
			}
		}
	}

	menu_close(md->w);
	md->w->menu = md;

	redraw_invalidate_scene(md->w);
	window_update_focus(md->w);
	server_redraw_window(md->w);
	return (0);
}
