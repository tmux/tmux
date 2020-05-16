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
	const char		*key, *cmd;
	char			*s, *name;
	u_int			 width;
	int			 line;

	line = (item == NULL || item->name == NULL || *item->name == '\0');
	if (line && menu->count == 0)
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
	if (*s != '-' && item->key != KEYC_UNKNOWN && item->key != KEYC_NONE) {
		key = key_string_lookup_key(item->key);
		xasprintf(&name, "%s#[default] #[align=right](%s)", s, key);
	} else
		xasprintf(&name, "%s", s);
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
	if (width > menu->width)
		menu->width = width;
}

struct menu *
menu_create(const char *title)
{
	struct menu	*menu;

	menu = xcalloc(1, sizeof *menu);
	menu->title = xstrdup(title);

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

static struct screen *
menu_mode_cb(struct client *c, __unused u_int *cx, __unused u_int *cy)
{
	struct menu_data	*md = c->overlay_data;

	return (&md->s);
}

static void
menu_draw_cb(struct client *c, __unused struct screen_redraw_ctx *ctx0)
{
	struct menu_data	*md = c->overlay_data;
	struct tty		*tty = &c->tty;
	struct screen		*s = &md->s;
	struct menu		*menu = md->menu;
	struct screen_write_ctx	 ctx;
	u_int			 i, px = md->px, py = md->py;
	struct grid_cell	 gc;

	style_apply(&gc, c->session->curw->window->options, "mode-style", NULL);

	screen_write_start(&ctx, s);
	screen_write_clearscreen(&ctx, 8);
	screen_write_menu(&ctx, menu, md->choice, &gc);
	screen_write_stop(&ctx);

	for (i = 0; i < screen_size_y(&md->s); i++) {
		tty_draw_line(tty, s, 0, i, menu->width + 4, px, py + i,
		    &grid_default_cell, NULL);
	}
}

static void
menu_free_cb(struct client *c)
{
	struct menu_data	*md = c->overlay_data;

	if (md->item != NULL)
		cmdq_continue(md->item);

	if (md->cb != NULL)
		md->cb(md->menu, UINT_MAX, KEYC_NONE, md->data);

	screen_free(&md->s);
	menu_free(md->menu);
	free(md);
}

static int
menu_key_cb(struct client *c, struct key_event *event)
{
	struct menu_data		*md = c->overlay_data;
	struct menu			*menu = md->menu;
	struct mouse_event		*m = &event->m;
	u_int				 i;
	int				 count = menu->count, old = md->choice;
	const char			*name;
	const struct menu_item		*item;
	struct cmdq_state		*state;
	enum cmd_parse_status		 status;
	char				*error;

	if (KEYC_IS_MOUSE(event->key)) {
		if (md->flags & MENU_NOMOUSE) {
			if (MOUSE_BUTTONS(m->b) != 0)
				return (1);
			return (0);
		}
		if (m->x < md->px ||
		    m->x > md->px + 4 + menu->width ||
		    m->y < md->py + 1 ||
		    m->y > md->py + 1 + count - 1) {
			if (MOUSE_RELEASE(m->b))
				return (1);
			if (md->choice != -1) {
				md->choice = -1;
				c->flags |= CLIENT_REDRAWOVERLAY;
			}
			return (0);
		}
		if (MOUSE_RELEASE(m->b))
			goto chosen;
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
	switch (event->key) {
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
	case 'g':
	case KEYC_PPAGE:
	case '\002': /* C-b */
		if (md->choice > 5)
			md->choice -= 5;
		else
			md->choice = 0;
		while (md->choice != count && (name == NULL || *name == '-'))
			md->choice++;
		if (md->choice == count)
			md->choice = -1;
		c->flags |= CLIENT_REDRAWOVERLAY;
		break;
	case 'G':
	case KEYC_NPAGE:
		if (md->choice > count - 6)
			md->choice = count - 1;
		else
			md->choice += 5;
		while (md->choice != -1 && (name == NULL || *name == '-'))
			md->choice--;
		c->flags |= CLIENT_REDRAWOVERLAY;
		break;
	case '\006': /* C-f */
		break;
	case '\r':
		goto chosen;
	case '\033': /* Escape */
	case '\003': /* C-c */
	case '\007': /* C-g */
	case 'q':
		return (1);
	}
	return (0);

chosen:
	if (md->choice == -1)
		return (1);
	item = &menu->items[md->choice];
	if (item->name == NULL || *item->name == '-')
		return (1);
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

int
menu_display(struct menu *menu, int flags, struct cmdq_item *item, u_int px,
    u_int py, struct client *c, struct cmd_find_state *fs, menu_choice_cb cb,
    void *data)
{
	struct menu_data	*md;
	u_int			 i;
	const char		*name;

	if (c->tty.sx < menu->width + 4 || c->tty.sy < menu->count + 2)
		return (-1);
	if (px + menu->width + 4 > c->tty.sx)
		px = c->tty.sx - menu->width - 4;
	if (py + menu->count + 2 > c->tty.sy)
		py = c->tty.sy - menu->count - 2;

	md = xcalloc(1, sizeof *md);
	md->item = item;
	md->flags = flags;

	if (fs != NULL)
		cmd_find_copy_state(&md->fs, fs);
	screen_init(&md->s, menu->width + 4, menu->count + 2, 0);
	if (~md->flags & MENU_NOMOUSE)
		md->s.mode |= MODE_MOUSE_ALL;

	md->px = px;
	md->py = py;

	md->menu = menu;
	if (md->flags & MENU_NOMOUSE) {
		for (i = 0; i < menu->count; i++) {
			name = menu->items[i].name;
			if (name != NULL && *name != '-')
				break;
		}
		if (i != menu->count)
			md->choice = i;
		else
			md->choice = -1;
	} else
		md->choice = -1;

	md->cb = cb;
	md->data = data;

	server_client_set_overlay(c, 0, NULL, menu_mode_cb, menu_draw_cb,
	    menu_key_cb, menu_free_cb, md);
	return (0);
}
