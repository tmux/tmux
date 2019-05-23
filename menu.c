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

static void
menu_add_item(struct menu *menu, struct menu_item *item, struct client *c,
    struct cmd_find_state *fs)
{
	struct menu_item	*new_item;
	const char		*key;
	char			*name;
	u_int			 width;

	menu->items = xreallocarray(menu->items, menu->count + 1,
	    sizeof *menu->items);
	new_item = &menu->items[menu->count++];
	memset(new_item, 0, sizeof *new_item);

	if (item == NULL || *item->name == '\0') /* horizontal line */
		return;
	if (fs != NULL) {
		name = format_single(NULL, item->name, c, fs->s, fs->wl,
		    fs->wp);
	} else
		name = xstrdup(item->name);
	if (*name == '\0') { /* no item if empty after format expanded */
		menu->count--;
		return;
	}
	if (item->key != KEYC_UNKNOWN) {
		key = key_string_lookup_key(item->key);
		xasprintf(&new_item->name, "%s #[align=right](%s)", name, key);
	} else
		xasprintf(&new_item->name, "%s", name);
	free(name);

	if (item->command != NULL)
		new_item->command = xstrdup(item->command);
	else
		new_item->command = NULL;
	new_item->key = item->key;

	width = format_width(new_item->name);
	if (width > menu->width)
		menu->width = width;
}

static void
menu_parse_item(struct menu *menu, const char *s, struct client *c,
    struct cmd_find_state *fs)
{
	char			*copy, *first;
	const char		*second, *third;
	struct menu_item	 item;

	first = copy = xstrdup(s);
	if ((second = format_skip(first, ",")) != NULL) {
		*(char *)second++ = '\0';
		if ((third = format_skip(second, ",")) != NULL) {
			*(char *)third++ = '\0';

			item.name = first;
			item.command = (char *)third;
			item.key = key_string_lookup_string(second);
			menu_add_item(menu, &item, c, fs);
		}
	}
	free(copy);
}

struct menu *
menu_create(const char *s, struct client *c, struct cmd_find_state *fs,
    const char *title)
{
	struct menu	*menu;
	char		*copy, *string, *next;

	if (*s == '\0')
		return (NULL);

	menu = xcalloc(1, sizeof *menu);
	menu->title = xstrdup(title);

	copy = string = xstrdup(s);
	do {
		next = (char *)format_skip(string, "|");
		if (next != NULL)
			*next++ = '\0';
		if (*string == '\0')
			menu_add_item(menu, NULL, c, fs);
		else
			menu_parse_item(menu, string, c, fs);
		string = next;
	} while (next != NULL);
	free(copy);

	return (menu);
}

void
menu_free(struct menu *menu)
{
	u_int	i;

	for (i = 0; i < menu->count; i++) {
		free(menu->items[i].name);
		free(menu->items[i].command);
	}
	free(menu->items);

	free(menu->title);
	free(menu);
}

static void
menu_draw_cb(struct client *c, __unused struct screen_redraw_ctx *ctx0)
{
	struct menu_data	*md = c->overlay_data;
	struct tty		*tty = &c->tty;
	struct screen		*s = &md->s;
	struct menu		*menu = md->menu;
	struct screen_write_ctx	 ctx;
	u_int			 i, px, py;

	screen_write_start(&ctx, NULL, s);
	screen_write_clearscreen(&ctx, 8);
	screen_write_menu(&ctx, menu, md->choice);
	screen_write_stop(&ctx);

	px = md->px;
	py = md->py;

	for (i = 0; i < screen_size_y(&md->s); i++)
		tty_draw_line(tty, NULL, s, 0, i, menu->width + 4, px, py + i);

	if (~md->flags & MENU_NOMOUSE)
		tty_update_mode(tty, MODE_MOUSE_ALL, NULL);
}

static void
menu_free_cb(struct client *c)
{
	struct menu_data	*md = c->overlay_data;

	if (md->item != NULL)
		md->item->flags &= ~CMDQ_WAITING;

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
	const struct menu_item		*item;
	struct cmdq_item		*new_item;
	struct cmd_parse_result		*pr;

	if (KEYC_IS_MOUSE(event->key)) {
		if (md->flags & MENU_NOMOUSE)
			return (0);
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
		md->choice = m->y - (md->py + 1);
		if (MOUSE_RELEASE(m->b))
			goto chosen;
		if (md->choice != old)
			c->flags |= CLIENT_REDRAWOVERLAY;
		return (0);
	}
	switch (event->key) {
	case KEYC_UP:
		do {
			if (md->choice == -1 || md->choice == 0)
				md->choice = count - 1;
			else
				md->choice--;
		} while (menu->items[md->choice].name == NULL);
		c->flags |= CLIENT_REDRAWOVERLAY;
		return (0);
	case KEYC_DOWN:
		do {
			if (md->choice == -1 || md->choice == count - 1)
				md->choice = 0;
		else
			md->choice++;
		} while (menu->items[md->choice].name == NULL);
		c->flags |= CLIENT_REDRAWOVERLAY;
		return (0);
	case '\r':
		goto chosen;
	case '\033': /* Escape */
	case '\003': /* C-c */
	case '\007': /* C-g */
	case 'q':
		return (1);
	}
	for (i = 0; i < (u_int)count; i++) {
		if (event->key == menu->items[i].key) {
			md->choice = i;
			goto chosen;
		}
	}
	return (0);

chosen:
	if (md->choice == -1)
		return (1);
	item = &menu->items[md->choice];
	if (item->name == NULL)
		return (1);
	if (md->cb != NULL) {
	    md->cb(md->menu, md->choice, item->key, md->data);
	    md->cb = NULL;
	    return (1);
	}

	pr = cmd_parse_from_string(item->command, NULL);
	switch (pr->status) {
	case CMD_PARSE_EMPTY:
		new_item = NULL;
		break;
	case CMD_PARSE_ERROR:
		new_item = cmdq_get_error(pr->error);
		free(pr->error);
		cmdq_append(c, new_item);
		break;
	case CMD_PARSE_SUCCESS:
		new_item = cmdq_get_command(pr->cmdlist, NULL, NULL, 0);
		cmd_list_free(pr->cmdlist);
		cmdq_append(c, new_item);
		break;
	}
	return (1);
}

int
menu_display(struct menu *menu, int flags, struct cmdq_item *item, u_int px,
    u_int py, struct client *c, struct cmd_find_state *fs, menu_choice_cb cb,
    void *data)
{
	struct menu_data	*md;

	if (c->tty.sx < menu->width + 4 || c->tty.sy < menu->count + 2)
		return (-1);

	md = xcalloc(1, sizeof *md);
	md->item = item;
	md->flags = flags;

	if (fs != NULL)
		cmd_find_copy_state(&md->fs, fs);
	screen_init(&md->s, menu->width + 4, menu->count + 2, 0);

	md->px = px;
	md->py = py;

	md->menu = menu;
	md->choice = -1;

	md->cb = cb;
	md->data = data;

	server_client_set_overlay(c, 0, menu_draw_cb, menu_key_cb, menu_free_cb,
	    md);
	return (0);
}
