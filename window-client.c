/* $OpenBSD$ */

/*
 * Copyright (c) 2017 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/time.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

static struct screen	*window_client_init(struct window_mode_entry *,
			     struct cmd_find_state *, struct args *);
static void		 window_client_free(struct window_mode_entry *);
static void		 window_client_resize(struct window_mode_entry *, u_int,
			     u_int);
static void		 window_client_update(struct window_mode_entry *);
static void		 window_client_key(struct window_mode_entry *,
			     struct client *, struct session *,
			     struct winlink *, key_code, struct mouse_event *);

#define WINDOW_CLIENT_DEFAULT_COMMAND "detach-client -t '%%'"

#define WINDOW_CLIENT_DEFAULT_FORMAT \
	"#{t/p:client_activity}: session #{session_name}"

#define WINDOW_CLIENT_DEFAULT_KEY_FORMAT \
	"#{?#{e|<:#{line},10}," \
		"#{line}" \
	"," \
		"#{?#{e|<:#{line},36},"	\
	        	"M-#{a:#{e|+:97,#{e|-:#{line},10}}}" \
		"," \
	        	"" \
		"}" \
	"}"

static const struct menu_item window_client_menu_items[] = {
	{ "Detach", 'd', NULL },
	{ "Detach Tagged", 'D', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Tag", 't', NULL },
	{ "Tag All", '\024', NULL },
	{ "Tag None", 'T', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Cancel", 'q', NULL },

	{ NULL, KEYC_NONE, NULL }
};

const struct window_mode window_client_mode = {
	.name = "client-mode",
	.default_format = WINDOW_CLIENT_DEFAULT_FORMAT,

	.init = window_client_init,
	.free = window_client_free,
	.resize = window_client_resize,
	.update = window_client_update,
	.key = window_client_key,
};

enum window_client_sort_type {
	WINDOW_CLIENT_BY_NAME,
	WINDOW_CLIENT_BY_SIZE,
	WINDOW_CLIENT_BY_CREATION_TIME,
	WINDOW_CLIENT_BY_ACTIVITY_TIME,
};
static const char *window_client_sort_list[] = {
	"name",
	"size",
	"creation",
	"activity"
};
static struct mode_tree_sort_criteria *window_client_sort;

struct window_client_itemdata {
	struct client	*c;
};

struct window_client_modedata {
	struct window_pane		 *wp;

	struct mode_tree_data		 *data;
	char				 *format;
	char				 *key_format;
	char				 *command;

	struct window_client_itemdata	**item_list;
	u_int				  item_size;
};

static struct window_client_itemdata *
window_client_add_item(struct window_client_modedata *data)
{
	struct window_client_itemdata	*item;

	data->item_list = xreallocarray(data->item_list, data->item_size + 1,
	    sizeof *data->item_list);
	item = data->item_list[data->item_size++] = xcalloc(1, sizeof *item);
	return (item);
}

static void
window_client_free_item(struct window_client_itemdata *item)
{
	server_client_unref(item->c);
	free(item);
}

static int
window_client_cmp(const void *a0, const void *b0)
{
	const struct window_client_itemdata *const	*a = a0;
	const struct window_client_itemdata *const	*b = b0;
	const struct window_client_itemdata		*itema = *a;
	const struct window_client_itemdata		*itemb = *b;
	struct client					*ca = itema->c;
	struct client					*cb = itemb->c;
	int						 result = 0;

	switch (window_client_sort->field) {
	case WINDOW_CLIENT_BY_SIZE:
		result = ca->tty.sx - cb->tty.sx;
		if (result == 0)
			result = ca->tty.sy - cb->tty.sy;
		break;
	case WINDOW_CLIENT_BY_CREATION_TIME:
		if (timercmp(&ca->creation_time, &cb->creation_time, >))
			result = -1;
		else if (timercmp(&ca->creation_time, &cb->creation_time, <))
			result = 1;
		break;
	case WINDOW_CLIENT_BY_ACTIVITY_TIME:
		if (timercmp(&ca->activity_time, &cb->activity_time, >))
			result = -1;
		else if (timercmp(&ca->activity_time, &cb->activity_time, <))
			result = 1;
		break;
	}

	/* Use WINDOW_CLIENT_BY_NAME as default order and tie breaker. */
	if (result == 0)
		result = strcmp(ca->name, cb->name);

	if (window_client_sort->reversed)
		result = -result;
	return (result);
}

static void
window_client_build(void *modedata, struct mode_tree_sort_criteria *sort_crit,
    __unused uint64_t *tag, const char *filter)
{
	struct window_client_modedata	*data = modedata;
	struct window_client_itemdata	*item;
	u_int				 i;
	struct client			*c;
	char				*text, *cp;

	for (i = 0; i < data->item_size; i++)
		window_client_free_item(data->item_list[i]);
	free(data->item_list);
	data->item_list = NULL;
	data->item_size = 0;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == NULL || (c->flags & CLIENT_UNATTACHEDFLAGS))
			continue;

		item = window_client_add_item(data);
		item->c = c;

		c->references++;
	}

	window_client_sort = sort_crit;
	qsort(data->item_list, data->item_size, sizeof *data->item_list,
	    window_client_cmp);

	for (i = 0; i < data->item_size; i++) {
		item = data->item_list[i];
		c = item->c;

		if (filter != NULL) {
			cp = format_single(NULL, filter, c, NULL, NULL, NULL);
			if (!format_true(cp)) {
				free(cp);
				continue;
			}
			free(cp);
		}

		text = format_single(NULL, data->format, c, NULL, NULL, NULL);
		mode_tree_add(data->data, NULL, item, (uint64_t)c, c->name,
		    text, -1);
		free(text);
	}
}

static void
window_client_draw(__unused void *modedata, void *itemdata,
    struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct window_client_itemdata	*item = itemdata;
	struct client			*c = item->c;
	struct screen			*s = ctx->s;
	struct window_pane		*wp;
	u_int				 cx = s->cx, cy = s->cy, lines, at;

	if (c->session == NULL || (c->flags & CLIENT_UNATTACHEDFLAGS))
		return;
	wp = c->session->curw->window->active;

	lines = status_line_size(c);
	if (lines >= sy)
		lines = 0;
	if (status_at_line(c) == 0)
		at = lines;
	else
		at = 0;

	screen_write_cursormove(ctx, cx, cy + at, 0);
	screen_write_preview(ctx, &wp->base, sx, sy - 2 - lines);

	if (at != 0)
		screen_write_cursormove(ctx, cx, cy + 2, 0);
	else
		screen_write_cursormove(ctx, cx, cy + sy - 1 - lines, 0);
	screen_write_hline(ctx, sx, 0, 0, BOX_LINES_DEFAULT, NULL);

	if (at != 0)
		screen_write_cursormove(ctx, cx, cy, 0);
	else
		screen_write_cursormove(ctx, cx, cy + sy - lines, 0);
	screen_write_fast_copy(ctx, &c->status.screen, 0, 0, sx, lines);
}

static void
window_client_menu(void *modedata, struct client *c, key_code key)
{
	struct window_client_modedata	*data = modedata;
	struct window_pane		*wp = data->wp;
	struct window_mode_entry	*wme;

	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL || wme->data != modedata)
		return;
	window_client_key(wme, c, NULL, NULL, key, NULL);
}

static key_code
window_client_get_key(void *modedata, void *itemdata, u_int line)
{
	struct window_client_modedata	*data = modedata;
	struct window_client_itemdata	*item = itemdata;
	struct format_tree		*ft;
	char				*expanded;
	key_code			 key;

	ft = format_create(NULL, NULL, FORMAT_NONE, 0);
	format_defaults(ft, item->c, NULL, 0, NULL);
	format_add(ft, "line", "%u", line);

	expanded = format_expand(ft, data->key_format);
	key = key_string_lookup_string(expanded);
	free(expanded);
	format_free(ft);
	return (key);
}

static struct screen *
window_client_init(struct window_mode_entry *wme,
    __unused struct cmd_find_state *fs, struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window_client_modedata	*data;
	struct screen			*s;

	wme->data = data = xcalloc(1, sizeof *data);
	data->wp = wp;

	if (args == NULL || !args_has(args, 'F'))
		data->format = xstrdup(WINDOW_CLIENT_DEFAULT_FORMAT);
	else
		data->format = xstrdup(args_get(args, 'F'));
	if (args == NULL || !args_has(args, 'K'))
		data->key_format = xstrdup(WINDOW_CLIENT_DEFAULT_KEY_FORMAT);
	else
		data->key_format = xstrdup(args_get(args, 'K'));
	if (args == NULL || args_count(args) == 0)
		data->command = xstrdup(WINDOW_CLIENT_DEFAULT_COMMAND);
	else
		data->command = xstrdup(args_string(args, 0));

	data->data = mode_tree_start(wp, args, window_client_build,
	    window_client_draw, NULL, window_client_menu, NULL,
	    window_client_get_key, data, window_client_menu_items,
	    window_client_sort_list, nitems(window_client_sort_list), &s);
	mode_tree_zoom(data->data, args);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);

	return (s);
}

static void
window_client_free(struct window_mode_entry *wme)
{
	struct window_client_modedata	*data = wme->data;
	u_int				 i;

	if (data == NULL)
		return;

	mode_tree_free(data->data);

	for (i = 0; i < data->item_size; i++)
		window_client_free_item(data->item_list[i]);
	free(data->item_list);

	free(data->format);
	free(data->key_format);
	free(data->command);

	free(data);
}

static void
window_client_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_client_modedata	*data = wme->data;

	mode_tree_resize(data->data, sx, sy);
}

static void
window_client_update(struct window_mode_entry *wme)
{
	struct window_client_modedata	*data = wme->data;

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;
}

static void
window_client_do_detach(void *modedata, void *itemdata,
    __unused struct client *c, key_code key)
{
	struct window_client_modedata	*data = modedata;
	struct window_client_itemdata	*item = itemdata;

	if (item == mode_tree_get_current(data->data))
		mode_tree_down(data->data, 0);
	if (key == 'd' || key == 'D')
		server_client_detach(item->c, MSG_DETACH);
	else if (key == 'x' || key == 'X')
		server_client_detach(item->c, MSG_DETACHKILL);
	else if (key == 'z' || key == 'Z')
		server_client_suspend(item->c);
}

static void
window_client_key(struct window_mode_entry *wme, struct client *c,
    __unused struct session *s, __unused struct winlink *wl, key_code key,
    struct mouse_event *m)
{
	struct window_pane		*wp = wme->wp;
	struct window_client_modedata	*data = wme->data;
	struct mode_tree_data		*mtd = data->data;
	struct window_client_itemdata	*item;
	int				 finished;

	finished = mode_tree_key(mtd, c, &key, m, NULL, NULL);
	switch (key) {
	case 'd':
	case 'x':
	case 'z':
		item = mode_tree_get_current(mtd);
		window_client_do_detach(data, item, c, key);
		mode_tree_build(mtd);
		break;
	case 'D':
	case 'X':
	case 'Z':
		mode_tree_each_tagged(mtd, window_client_do_detach, c, key, 0);
		mode_tree_build(mtd);
		break;
	case '\r':
		item = mode_tree_get_current(mtd);
		mode_tree_run_command(c, NULL, data->command, item->c->ttyname);
		finished = 1;
		break;
	}
	if (finished || server_client_how_many() == 0)
		window_pane_reset_mode(wp);
	else {
		mode_tree_draw(mtd);
		wp->flags |= PANE_REDRAW;
	}
}
