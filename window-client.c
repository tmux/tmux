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

static struct screen	*window_client_init(struct window_pane *,
			     struct cmd_find_state *, struct args *);
static void		 window_client_free(struct window_pane *);
static void		 window_client_resize(struct window_pane *, u_int,
			     u_int);
static void		 window_client_key(struct window_pane *,
			     struct client *, struct session *, key_code,
			     struct mouse_event *);

#define WINDOW_CLIENT_DEFAULT_COMMAND "detach-client -t '%%'"

#define WINDOW_CLIENT_DEFAULT_FORMAT \
	"session #{session_name} " \
	"(#{client_width}x#{client_height}, #{t:client_activity})"

const struct window_mode window_client_mode = {
	.name = "client-mode",

	.init = window_client_init,
	.free = window_client_free,
	.resize = window_client_resize,
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

struct window_client_itemdata {
	struct client	*c;
};

struct window_client_modedata {
	struct mode_tree_data		 *data;
	char				 *format;
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
window_client_cmp_name(const void *a0, const void *b0)
{
	const struct window_client_itemdata *const *a = a0;
	const struct window_client_itemdata *const *b = b0;

	return (strcmp((*a)->c->name, (*b)->c->name));
}

static int
window_client_cmp_size(const void *a0, const void *b0)
{
	const struct window_client_itemdata *const *a = a0;
	const struct window_client_itemdata *const *b = b0;

	if ((*a)->c->tty.sx < (*b)->c->tty.sx)
		return (-1);
	if ((*a)->c->tty.sx > (*b)->c->tty.sx)
		return (1);
	if ((*a)->c->tty.sy < (*b)->c->tty.sy)
		return (-1);
	if ((*a)->c->tty.sy > (*b)->c->tty.sy)
		return (1);
	return (strcmp((*a)->c->name, (*b)->c->name));
}

static int
window_client_cmp_creation_time(const void *a0, const void *b0)
{
	const struct window_client_itemdata *const *a = a0;
	const struct window_client_itemdata *const *b = b0;

	if (timercmp(&(*a)->c->creation_time, &(*b)->c->creation_time, >))
		return (-1);
	if (timercmp(&(*a)->c->creation_time, &(*b)->c->creation_time, <))
		return (1);
	return (strcmp((*a)->c->name, (*b)->c->name));
}

static int
window_client_cmp_activity_time(const void *a0, const void *b0)
{
	const struct window_client_itemdata *const *a = a0;
	const struct window_client_itemdata *const *b = b0;

	if (timercmp(&(*a)->c->activity_time, &(*b)->c->activity_time, >))
		return (-1);
	if (timercmp(&(*a)->c->activity_time, &(*b)->c->activity_time, <))
		return (1);
	return (strcmp((*a)->c->name, (*b)->c->name));
}

static void
window_client_build(void *modedata, u_int sort_type, __unused uint64_t *tag,
    const char *filter)
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
		if (c->session == NULL || (c->flags & (CLIENT_DETACHING)))
			continue;

		item = window_client_add_item(data);
		item->c = c;

		c->references++;
	}

	switch (sort_type) {
	case WINDOW_CLIENT_BY_NAME:
		qsort(data->item_list, data->item_size, sizeof *data->item_list,
		    window_client_cmp_name);
		break;
	case WINDOW_CLIENT_BY_SIZE:
		qsort(data->item_list, data->item_size, sizeof *data->item_list,
		    window_client_cmp_size);
		break;
	case WINDOW_CLIENT_BY_CREATION_TIME:
		qsort(data->item_list, data->item_size, sizeof *data->item_list,
		    window_client_cmp_creation_time);
		break;
	case WINDOW_CLIENT_BY_ACTIVITY_TIME:
		qsort(data->item_list, data->item_size, sizeof *data->item_list,
		    window_client_cmp_activity_time);
		break;
	}

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
	struct window_pane		*wp;
	u_int				 cx = ctx->s->cx, cy = ctx->s->cy;

	if (c->session == NULL || (c->flags & (CLIENT_DEAD|CLIENT_DETACHING)))
		return;
	wp = c->session->curw->window->active;

	screen_write_preview(ctx, &wp->base, sx, sy - 3);

	screen_write_cursormove(ctx, cx, cy + sy - 2);
	screen_write_hline(ctx, sx, 0, 0);

	screen_write_cursormove(ctx, cx, cy + sy - 1);
	if (c->status.old_status != NULL)
		screen_write_fast_copy(ctx, c->status.old_status, 0, 0, sx, 1);
	else
		screen_write_fast_copy(ctx, &c->status.status, 0, 0, sx, 1);
}

static struct screen *
window_client_init(struct window_pane *wp, __unused struct cmd_find_state *fs,
    struct args *args)
{
	struct window_client_modedata	*data;
	struct screen			*s;

	wp->modedata = data = xcalloc(1, sizeof *data);

	if (args == NULL || !args_has(args, 'F'))
		data->format = xstrdup(WINDOW_CLIENT_DEFAULT_FORMAT);
	else
		data->format = xstrdup(args_get(args, 'F'));
	if (args == NULL || args->argc == 0)
		data->command = xstrdup(WINDOW_CLIENT_DEFAULT_COMMAND);
	else
		data->command = xstrdup(args->argv[0]);

	data->data = mode_tree_start(wp, args, window_client_build,
	    window_client_draw, NULL, data, window_client_sort_list,
	    nitems(window_client_sort_list), &s);
	mode_tree_zoom(data->data, args);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);

	return (s);
}

static void
window_client_free(struct window_pane *wp)
{
	struct window_client_modedata	*data = wp->modedata;
	u_int				 i;

	if (data == NULL)
		return;

	mode_tree_free(data->data);

	for (i = 0; i < data->item_size; i++)
		window_client_free_item(data->item_list[i]);
	free(data->item_list);

	free(data->format);
	free(data->command);

	free(data);
}

static void
window_client_resize(struct window_pane *wp, u_int sx, u_int sy)
{
	struct window_client_modedata	*data = wp->modedata;

	mode_tree_resize(data->data, sx, sy);
}

static void
window_client_do_detach(void* modedata, void *itemdata,
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
window_client_key(struct window_pane *wp, struct client *c,
    __unused struct session *s, key_code key, struct mouse_event *m)
{
	struct window_client_modedata	*data = wp->modedata;
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
