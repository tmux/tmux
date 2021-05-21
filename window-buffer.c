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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

static struct screen	*window_buffer_init(struct window_mode_entry *,
			     struct cmd_find_state *, struct args *);
static void		 window_buffer_free(struct window_mode_entry *);
static void		 window_buffer_resize(struct window_mode_entry *, u_int,
			     u_int);
static void		 window_buffer_update(struct window_mode_entry *);
static void		 window_buffer_key(struct window_mode_entry *,
			     struct client *, struct session *,
			     struct winlink *, key_code, struct mouse_event *);

#define WINDOW_BUFFER_DEFAULT_COMMAND "paste-buffer -b '%%'"

#define WINDOW_BUFFER_DEFAULT_FORMAT \
	"#{t/p:buffer_created}: #{buffer_sample}"

#define WINDOW_BUFFER_DEFAULT_KEY_FORMAT \
	"#{?#{e|<:#{line},10}," \
		"#{line}" \
	"," \
		"#{?#{e|<:#{line},36},"	\
	        	"M-#{a:#{e|+:97,#{e|-:#{line},10}}}" \
		"," \
	        	"" \
		"}" \
	"}"

static const struct menu_item window_buffer_menu_items[] = {
	{ "Paste", 'p', NULL },
	{ "Paste Tagged", 'P', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Tag", 't', NULL },
	{ "Tag All", '\024', NULL },
	{ "Tag None", 'T', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Delete", 'd', NULL },
	{ "Delete Tagged", 'D', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Cancel", 'q', NULL },

	{ NULL, KEYC_NONE, NULL }
};

const struct window_mode window_buffer_mode = {
	.name = "buffer-mode",
	.default_format = WINDOW_BUFFER_DEFAULT_FORMAT,

	.init = window_buffer_init,
	.free = window_buffer_free,
	.resize = window_buffer_resize,
	.update = window_buffer_update,
	.key = window_buffer_key,
};

enum window_buffer_sort_type {
	WINDOW_BUFFER_BY_TIME,
	WINDOW_BUFFER_BY_NAME,
	WINDOW_BUFFER_BY_SIZE,
};
static const char *window_buffer_sort_list[] = {
	"time",
	"name",
	"size"
};
static struct mode_tree_sort_criteria *window_buffer_sort;

struct window_buffer_itemdata {
	const char	*name;
	u_int		 order;
	size_t		 size;
};

struct window_buffer_modedata {
	struct window_pane		 *wp;
	struct cmd_find_state		  fs;

	struct mode_tree_data		 *data;
	char				 *command;
	char				 *format;
	char				 *key_format;

	struct window_buffer_itemdata	**item_list;
	u_int				  item_size;
};

struct window_buffer_editdata {
	u_int			 wp_id;
	char			*name;
	struct paste_buffer	*pb;
};

static struct window_buffer_itemdata *
window_buffer_add_item(struct window_buffer_modedata *data)
{
	struct window_buffer_itemdata	*item;

	data->item_list = xreallocarray(data->item_list, data->item_size + 1,
	    sizeof *data->item_list);
	item = data->item_list[data->item_size++] = xcalloc(1, sizeof *item);
	return (item);
}

static void
window_buffer_free_item(struct window_buffer_itemdata *item)
{
	free((void *)item->name);
	free(item);
}

static int
window_buffer_cmp(const void *a0, const void *b0)
{
	const struct window_buffer_itemdata *const	*a = a0;
	const struct window_buffer_itemdata *const	*b = b0;
	int						 result = 0;

	if (window_buffer_sort->field == WINDOW_BUFFER_BY_TIME)
		result = (*b)->order - (*a)->order;
	else if (window_buffer_sort->field == WINDOW_BUFFER_BY_SIZE)
		result = (*b)->size - (*a)->size;

	/* Use WINDOW_BUFFER_BY_NAME as default order and tie breaker. */
	if (result == 0)
		result = strcmp((*a)->name, (*b)->name);

	if (window_buffer_sort->reversed)
		result = -result;
	return (result);
}

static void
window_buffer_build(void *modedata, struct mode_tree_sort_criteria *sort_crit,
    __unused uint64_t *tag, const char *filter)
{
	struct window_buffer_modedata	*data = modedata;
	struct window_buffer_itemdata	*item;
	u_int				 i;
	struct paste_buffer		*pb;
	char				*text, *cp;
	struct format_tree		*ft;
	struct session			*s = NULL;
	struct winlink			*wl = NULL;
	struct window_pane		*wp = NULL;

	for (i = 0; i < data->item_size; i++)
		window_buffer_free_item(data->item_list[i]);
	free(data->item_list);
	data->item_list = NULL;
	data->item_size = 0;

	pb = NULL;
	while ((pb = paste_walk(pb)) != NULL) {
		item = window_buffer_add_item(data);
		item->name = xstrdup(paste_buffer_name(pb));
		paste_buffer_data(pb, &item->size);
		item->order = paste_buffer_order(pb);
	}

	window_buffer_sort = sort_crit;
	qsort(data->item_list, data->item_size, sizeof *data->item_list,
	    window_buffer_cmp);

	if (cmd_find_valid_state(&data->fs)) {
		s = data->fs.s;
		wl = data->fs.wl;
		wp = data->fs.wp;
	}

	for (i = 0; i < data->item_size; i++) {
		item = data->item_list[i];

		pb = paste_get_name(item->name);
		if (pb == NULL)
			continue;
		ft = format_create(NULL, NULL, FORMAT_NONE, 0);
		format_defaults(ft, NULL, s, wl, wp);
		format_defaults_paste_buffer(ft, pb);

		if (filter != NULL) {
			cp = format_expand(ft, filter);
			if (!format_true(cp)) {
				free(cp);
				format_free(ft);
				continue;
			}
			free(cp);
		}

		text = format_expand(ft, data->format);
		mode_tree_add(data->data, NULL, item, item->order, item->name,
		    text, -1);
		free(text);

		format_free(ft);
	}

}

static void
window_buffer_draw(__unused void *modedata, void *itemdata,
    struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct window_buffer_itemdata	*item = itemdata;
	struct paste_buffer		*pb;
	const char			*pdata, *start, *end;
	char				*buf = NULL;
	size_t				 psize;
	u_int				 i, cx = ctx->s->cx, cy = ctx->s->cy;

	pb = paste_get_name(item->name);
	if (pb == NULL)
		return;

	pdata = end = paste_buffer_data(pb, &psize);
	for (i = 0; i < sy; i++) {
		start = end;
		while (end != pdata + psize && *end != '\n')
			end++;
		buf = xreallocarray(buf, 4, end - start + 1);
		utf8_strvis(buf, start, end - start,
		    VIS_OCTAL|VIS_CSTYLE|VIS_TAB);
		if (*buf != '\0') {
			screen_write_cursormove(ctx, cx, cy + i, 0);
			screen_write_nputs(ctx, sx, &grid_default_cell, "%s",
			    buf);
		}

		if (end == pdata + psize)
			break;
		end++;
	}
	free(buf);
}

static int
window_buffer_search(__unused void *modedata, void *itemdata, const char *ss)
{
	struct window_buffer_itemdata	*item = itemdata;
	struct paste_buffer		*pb;
	const char			*bufdata;
	size_t				 bufsize;

	if ((pb = paste_get_name(item->name)) == NULL)
		return (0);
	if (strstr(item->name, ss) != NULL)
		return (1);
	bufdata = paste_buffer_data(pb, &bufsize);
	return (memmem(bufdata, bufsize, ss, strlen(ss)) != NULL);
}

static void
window_buffer_menu(void *modedata, struct client *c, key_code key)
{
	struct window_buffer_modedata	*data = modedata;
	struct window_pane		*wp = data->wp;
	struct window_mode_entry	*wme;

	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL || wme->data != modedata)
		return;
	window_buffer_key(wme, c, NULL, NULL, key, NULL);
}

static key_code
window_buffer_get_key(void *modedata, void *itemdata, u_int line)
{
	struct window_buffer_modedata	*data = modedata;
	struct window_buffer_itemdata	*item = itemdata;
	struct format_tree		*ft;
	struct session			*s = NULL;
	struct winlink			*wl = NULL;
	struct window_pane		*wp = NULL;
	struct paste_buffer		*pb;
	char				*expanded;
	key_code			 key;

	if (cmd_find_valid_state(&data->fs)) {
		s = data->fs.s;
		wl = data->fs.wl;
		wp = data->fs.wp;
	}
	pb = paste_get_name(item->name);
	if (pb == NULL)
		return KEYC_NONE;

	ft = format_create(NULL, NULL, FORMAT_NONE, 0);
	format_defaults(ft, NULL, NULL, 0, NULL);
	format_defaults(ft, NULL, s, wl, wp);
	format_defaults_paste_buffer(ft, pb);
	format_add(ft, "line", "%u", line);

	expanded = format_expand(ft, data->key_format);
	key = key_string_lookup_string(expanded);
	free(expanded);
	format_free(ft);
	return key;
}

static struct screen *
window_buffer_init(struct window_mode_entry *wme, struct cmd_find_state *fs,
    struct args *args)
{
	struct window_pane		*wp = wme->wp;
	struct window_buffer_modedata	*data;
	struct screen			*s;

	wme->data = data = xcalloc(1, sizeof *data);
	data->wp = wp;
	cmd_find_copy_state(&data->fs, fs);

	if (args == NULL || !args_has(args, 'F'))
		data->format = xstrdup(WINDOW_BUFFER_DEFAULT_FORMAT);
	else
		data->format = xstrdup(args_get(args, 'F'));
	if (args == NULL || !args_has(args, 'K'))
		data->key_format = xstrdup(WINDOW_BUFFER_DEFAULT_KEY_FORMAT);
	else
		data->key_format = xstrdup(args_get(args, 'K'));
	if (args == NULL || args->argc == 0)
		data->command = xstrdup(WINDOW_BUFFER_DEFAULT_COMMAND);
	else
		data->command = xstrdup(args->argv[0]);

	data->data = mode_tree_start(wp, args, window_buffer_build,
	    window_buffer_draw, window_buffer_search, window_buffer_menu, NULL,
	    window_buffer_get_key, data, window_buffer_menu_items,
	    window_buffer_sort_list, nitems(window_buffer_sort_list), &s);
	mode_tree_zoom(data->data, args);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);

	return (s);
}

static void
window_buffer_free(struct window_mode_entry *wme)
{
	struct window_buffer_modedata	*data = wme->data;
	u_int				 i;

	if (data == NULL)
		return;

	mode_tree_free(data->data);

	for (i = 0; i < data->item_size; i++)
		window_buffer_free_item(data->item_list[i]);
	free(data->item_list);

	free(data->format);
	free(data->key_format);
	free(data->command);

	free(data);
}

static void
window_buffer_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_buffer_modedata	*data = wme->data;

	mode_tree_resize(data->data, sx, sy);
}

static void
window_buffer_update(struct window_mode_entry *wme)
{
	struct window_buffer_modedata	*data = wme->data;

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;
}

static void
window_buffer_do_delete(void *modedata, void *itemdata,
    __unused struct client *c, __unused key_code key)
{
	struct window_buffer_modedata	*data = modedata;
	struct window_buffer_itemdata	*item = itemdata;
	struct paste_buffer		*pb;

	if (item == mode_tree_get_current(data->data))
		mode_tree_down(data->data, 0);
	if ((pb = paste_get_name(item->name)) != NULL)
		paste_free(pb);
}

static void
window_buffer_do_paste(void *modedata, void *itemdata, struct client *c,
    __unused key_code key)
{
	struct window_buffer_modedata	*data = modedata;
	struct window_buffer_itemdata	*item = itemdata;

	if (paste_get_name(item->name) != NULL)
		mode_tree_run_command(c, NULL, data->command, item->name);
}

static void
window_buffer_finish_edit(struct window_buffer_editdata *ed)
{
	free(ed->name);
	free(ed);
}

static void
window_buffer_edit_close_cb(char *buf, size_t len, void *arg)
{
	struct window_buffer_editdata	*ed = arg;
	size_t				 oldlen;
	const char			*oldbuf;
	struct paste_buffer		*pb;
	struct window_pane		*wp;
	struct window_buffer_modedata	*data;
	struct window_mode_entry	*wme;

	if (buf == NULL || len == 0) {
		window_buffer_finish_edit(ed);
		return;
	}

	pb = paste_get_name(ed->name);
	if (pb == NULL || pb != ed->pb) {
		window_buffer_finish_edit(ed);
		return;
	}

	oldbuf = paste_buffer_data(pb, &oldlen);
	if (oldlen != '\0' &&
	    oldbuf[oldlen - 1] != '\n' &&
	    buf[len - 1] == '\n')
		len--;
	if (len != 0)
		paste_replace(pb, buf, len);

	wp = window_pane_find_by_id(ed->wp_id);
	if (wp != NULL) {
		wme = TAILQ_FIRST(&wp->modes);
		if (wme->mode == &window_buffer_mode) {
			data = wme->data;
			mode_tree_build(data->data);
			mode_tree_draw(data->data);
		}
		wp->flags |= PANE_REDRAW;
	}
	window_buffer_finish_edit(ed);
}

static void
window_buffer_start_edit(struct window_buffer_modedata *data,
    struct window_buffer_itemdata *item, struct client *c)
{
	struct paste_buffer		*pb;
	const char			*buf;
	size_t				 len;
	struct window_buffer_editdata	*ed;

	if ((pb = paste_get_name(item->name)) == NULL)
		return;
	buf = paste_buffer_data(pb, &len);

	ed = xcalloc(1, sizeof *ed);
	ed->wp_id = data->wp->id;
	ed->name = xstrdup(paste_buffer_name(pb));
	ed->pb = pb;

	if (popup_editor(c, buf, len, window_buffer_edit_close_cb, ed) != 0)
		window_buffer_finish_edit(ed);
}

static void
window_buffer_key(struct window_mode_entry *wme, struct client *c,
    __unused struct session *s, __unused struct winlink *wl, key_code key,
    struct mouse_event *m)
{
	struct window_pane		*wp = wme->wp;
	struct window_buffer_modedata	*data = wme->data;
	struct mode_tree_data		*mtd = data->data;
	struct window_buffer_itemdata	*item;
	int				 finished;

	finished = mode_tree_key(mtd, c, &key, m, NULL, NULL);
	switch (key) {
	case 'e':
		item = mode_tree_get_current(mtd);
		window_buffer_start_edit(data, item, c);
		break;
	case 'd':
		item = mode_tree_get_current(mtd);
		window_buffer_do_delete(data, item, c, key);
		mode_tree_build(mtd);
		break;
	case 'D':
		mode_tree_each_tagged(mtd, window_buffer_do_delete, c, key, 0);
		mode_tree_build(mtd);
		break;
	case 'P':
		mode_tree_each_tagged(mtd, window_buffer_do_paste, c, key, 0);
		finished = 1;
		break;
	case 'p':
	case '\r':
		item = mode_tree_get_current(mtd);
		window_buffer_do_paste(data, item, c, key);
		finished = 1;
		break;
	}
	if (finished || paste_get_top(NULL) == NULL)
		window_pane_reset_mode(wp);
	else {
		mode_tree_draw(mtd);
		wp->flags |= PANE_REDRAW;
	}
}
