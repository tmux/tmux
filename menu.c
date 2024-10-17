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

struct menu_data {
	struct cmdq_item	*item;
	int			 flags;

	struct grid_cell	 style;
	struct grid_cell	 border_style;
	struct grid_cell	 selected_style;
	enum box_lines		 border_lines;

	struct cmd_find_state	 fs;
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

	for (i = 0; i < menu->count; i++) {
		free((void *)menu->items[i].name);
		free((void *)menu->items[i].command);
	}
	free(menu->items);

	free((void *)menu->title);
	free(menu);
}

struct screen *
menu_mode_cb(__unused struct client *c, void *data, u_int *cx, u_int *cy)
{
	struct menu_data	*md = data;

	*cx = md->px + 2;
	if (md->choice == -1)
		*cy = md->py;
	else
		*cy = md->py + 1 + md->choice;

	return (&md->s);
}

/* Return parts of the input range which are not obstructed by the menu. */
void
menu_check_cb(__unused struct client *c, void *data, u_int px, u_int py,
    u_int nx, struct overlay_ranges *r)
{
	struct menu_data	*md = data;
	struct menu		*menu = md->menu;

	server_client_overlay_range(md->px, md->py, menu->width + 4,
	    menu->count + 2, px, py, nx, r);
}

void
menu_draw_cb(struct client *c, void *data,
    __unused struct screen_redraw_ctx *rctx)
{
	struct menu_data	*md = data;
	struct tty		*tty = &c->tty;
	struct screen		*s = &md->s;
	struct menu		*menu = md->menu;
	struct screen_write_ctx	 ctx;
	u_int			 i, px = md->px, py = md->py;

	screen_write_start(&ctx, s);
	screen_write_clearscreen(&ctx, 8);

	if (md->border_lines != BOX_LINES_NONE) {
		screen_write_box(&ctx, menu->width + 4, menu->count + 2,
		    md->border_lines, &md->border_style, menu->title);
	}

	screen_write_menu(&ctx, menu, md->choice, md->border_lines,
	    &md->style, &md->border_style, &md->selected_style);
	screen_write_stop(&ctx);

	for (i = 0; i < screen_size_y(&md->s); i++) {
		tty_draw_line(tty, s, 0, i, menu->width + 4, px, py + i,
		    &grid_default_cell, NULL);
	}
}

void
menu_free_cb(__unused struct client *c, void *data)
{
	struct menu_data	*md = data;

	if (md->item != NULL)
		cmdq_continue(md->item);

	if (md->cb != NULL)
		md->cb(md->menu, UINT_MAX, KEYC_NONE, md->data);

	screen_free(&md->s);
	menu_free(md->menu);
	free(md);
}

int
menu_key_cb(struct client *c, void *data, struct key_event *event)
{
	struct menu_data		*md = data;
	struct menu			*menu = md->menu;
	struct mouse_event		*m = &event->m;
	u_int				 i;
	int				 count = menu->count, old = md->choice;
	const char			*name = NULL;
	const struct menu_item		*item;
	struct cmdq_state		*state;
	enum cmd_parse_status		 status;
	char				*error;

	if (KEYC_IS_MOUSE(event->key)) {
		if (md->flags & MENU_NOMOUSE) {
			if (MOUSE_BUTTONS(m->b) != MOUSE_BUTTON_1)
				return (1);
			return (0);
		}
		if (m->x < md->px ||
		    m->x > md->px + 4 + menu->width ||
		    m->y < md->py + 1 ||
		    m->y > md->py + 1 + count - 1) {
			if (~md->flags & MENU_STAYOPEN) {
				if (MOUSE_RELEASE(m->b))
					return (1);
			} else {
				if (!MOUSE_RELEASE(m->b) &&
				    !MOUSE_WHEEL(m->b) &&
				    !MOUSE_DRAG(m->b))
					return (1);
			}
			if (md->choice != -1) {
				md->choice = -1;
				c->flags |= CLIENT_REDRAWOVERLAY;
			}
			return (0);
		}
		if (~md->flags & MENU_STAYOPEN) {
			if (MOUSE_RELEASE(m->b))
				goto chosen;
		} else {
			if (!MOUSE_WHEEL(m->b) && !MOUSE_DRAG(m->b))
				goto chosen;
		}
		md->choice = m->y - (md->py + 1);
		if (md->choice != old)
			c->flags |= CLIENT_REDRAWOVERLAY;
		return (0);
	}
	for (i = 0; i < (u_int)count; i++) {
		name = menu->items[i].name;
		if (name == NULL || *name == '-')
			continue;
		if (event->key == menu->items[i].key) {
			md->choice = i;
			goto chosen;
		}
	}
	switch (event->key & ~KEYC_MASK_FLAGS) {
	case KEYC_UP:
	case 'k':
		if (old == -1)
			old = 0;
		do {
			if (md->choice == -1 || md->choice == 0)
				md->choice = count - 1;
			else
				md->choice--;
			name = menu->items[md->choice].name;
		} while ((name == NULL || *name == '-') && md->choice != old);
		c->flags |= CLIENT_REDRAWOVERLAY;
		return (0);
	case KEYC_BSPACE:
		if (~md->flags & MENU_TAB)
			break;
		return (1);
	case '\011': /* Tab */
		if (~md->flags & MENU_TAB)
			break;
		if (md->choice == count - 1)
			return (1);
		/* FALLTHROUGH */
	case KEYC_DOWN:
	case 'j':
		if (old == -1)
			old = 0;
		do {
			if (md->choice == -1 || md->choice == count - 1)
				md->choice = 0;
			else
				md->choice++;
			name = menu->items[md->choice].name;
		} while ((name == NULL || *name == '-') && md->choice != old);
		c->flags |= CLIENT_REDRAWOVERLAY;
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
		c->flags |= CLIENT_REDRAWOVERLAY;
		break;
	case KEYC_NPAGE:
		if (md->choice > count - 6) {
			md->choice = count - 1;
			name = menu->items[md->choice].name;
		} else {
			i = 5;
			while (i > 0) {
				md->choice++;
				name = menu->items[md->choice].name;
				if (md->choice != count - 1 &&
				    (name != NULL && *name != '-'))
					i++;
				else if (md->choice == count - 1)
					break;
			}
		}
		while (name == NULL || *name == '-') {
			md->choice--;
			name = menu->items[md->choice].name;
		}
		c->flags |= CLIENT_REDRAWOVERLAY;
		break;
	case 'g':
	case KEYC_HOME:
		md->choice = 0;
		name = menu->items[md->choice].name;
		while (name == NULL || *name == '-') {
			md->choice++;
			name = menu->items[md->choice].name;
		}
		c->flags |= CLIENT_REDRAWOVERLAY;
		break;
	case 'G':
	case KEYC_END:
		md->choice = count - 1;
		name = menu->items[md->choice].name;
		while (name == NULL || *name == '-') {
			md->choice--;
			name = menu->items[md->choice].name;
		}
		c->flags |= CLIENT_REDRAWOVERLAY;
		break;
	case 'f'|KEYC_CTRL:
		break;
	case '\r':
		goto chosen;
	case '\033': /* Escape */
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

	if (md->item != NULL)
		event = cmdq_get_event(md->item);
	else
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

static void
menu_set_style(struct client *c, struct grid_cell *gc, const char *style,
    const char *option)
{
	struct style	 sytmp;
	struct options	*o = c->session->curw->window->options;

	memcpy(gc, &grid_default_cell, sizeof *gc);
	style_apply(gc, o, option, NULL);
	if (style != NULL) {
		style_set(&sytmp, &grid_default_cell);
		if (style_parse(&sytmp, gc, style) == 0) {
			gc->fg = sytmp.gc.fg;
			gc->bg = sytmp.gc.bg;
		}
	}
}

struct menu_data *
menu_prepare(struct menu *menu, int flags, int starting_choice,
    struct cmdq_item *item, u_int px, u_int py, struct client *c,
    enum box_lines lines, const char *style, const char *selected_style,
    const char *border_style, struct cmd_find_state *fs, menu_choice_cb cb,
    void *data)
{
	struct menu_data	*md;
	int			 choice;
	const char		*name;
	struct options		*o = c->session->curw->window->options;

	if (c->tty.sx < menu->width + 4 || c->tty.sy < menu->count + 2)
		return (NULL);
	if (px + menu->width + 4 > c->tty.sx)
		px = c->tty.sx - menu->width - 4;
	if (py + menu->count + 2 > c->tty.sy)
		py = c->tty.sy - menu->count - 2;

	if (lines == BOX_LINES_DEFAULT)
		lines = options_get_number(o, "menu-border-lines");

	md = xcalloc(1, sizeof *md);
	md->item = item;
	md->flags = flags;
	md->border_lines = lines;

	menu_set_style(c, &md->style, style, "menu-style");
	menu_set_style(c, &md->selected_style, selected_style,
	    "menu-selected-style");
	menu_set_style(c, &md->border_style, border_style, "menu-border-style");

	if (fs != NULL)
		cmd_find_copy_state(&md->fs, fs);
	screen_init(&md->s, menu->width + 4, menu->count + 2, 0);
	if (~md->flags & MENU_NOMOUSE)
		md->s.mode |= (MODE_MOUSE_ALL|MODE_MOUSE_BUTTON);
	md->s.mode &= ~MODE_CURSOR;

	md->px = px;
	md->py = py;

	md->menu = menu;
	md->choice = -1;

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

	md->cb = cb;
	md->data = data;
	return (md);
}

int
menu_display(struct menu *menu, int flags, int starting_choice,
    struct cmdq_item *item, u_int px, u_int py, struct client *c,
    enum box_lines lines, const char *style, const char *selected_style,
    const char *border_style, struct cmd_find_state *fs, menu_choice_cb cb,
    void *data)
{
	struct menu_data	*md;

	md = menu_prepare(menu, flags, starting_choice, item, px, py, c, lines,
	    style, selected_style, border_style, fs, cb, data);
	if (md == NULL)
		return (-1);
	server_client_set_overlay(c, 0, NULL, menu_mode_cb, menu_draw_cb,
	    menu_key_cb, menu_free_cb, NULL, md);
	return (0);
}
