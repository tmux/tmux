/* $OpenBSD$ */

/*
 * Copyright (c) 2020 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

static struct screen	*window_customize_init(struct window_mode_entry *,
			     struct cmd_find_state *, struct args *);
static void		 window_customize_free(struct window_mode_entry *);
static void		 window_customize_resize(struct window_mode_entry *,
			      u_int, u_int);
static void		 window_customize_key(struct window_mode_entry *,
			     struct client *, struct session *,
			     struct winlink *, key_code, struct mouse_event *);

#define WINDOW_CUSTOMIZE_DEFAULT_FORMAT \
	"#{?is_option," \
		"#{?option_is_global,,#[reverse](#{option_scope})#[default] }" \
		"#[ignore]" \
		"#{option_value}#{?option_unit, #{option_unit},}" \
	"," \
		"#{key}" \
	"}"

static const struct menu_item window_customize_menu_items[] = {
	{ "Select", '\r', NULL },
	{ "Expand", KEYC_RIGHT, NULL },
	{ "", KEYC_NONE, NULL },
	{ "Tag", 't', NULL },
	{ "Tag All", '\024', NULL },
	{ "Tag None", 'T', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Cancel", 'q', NULL },

	{ NULL, KEYC_NONE, NULL }
};

const struct window_mode window_customize_mode = {
	.name = "options-mode",
	.default_format = WINDOW_CUSTOMIZE_DEFAULT_FORMAT,

	.init = window_customize_init,
	.free = window_customize_free,
	.resize = window_customize_resize,
	.key = window_customize_key,
};

enum window_customize_scope {
	WINDOW_CUSTOMIZE_NONE,
	WINDOW_CUSTOMIZE_KEY,
	WINDOW_CUSTOMIZE_SERVER,
	WINDOW_CUSTOMIZE_GLOBAL_SESSION,
	WINDOW_CUSTOMIZE_SESSION,
	WINDOW_CUSTOMIZE_GLOBAL_WINDOW,
	WINDOW_CUSTOMIZE_WINDOW,
	WINDOW_CUSTOMIZE_PANE
};

struct window_customize_itemdata {
	struct window_customize_modedata	*data;
	enum window_customize_scope		 scope;

	char					*table;
	key_code				 key;

	struct options				*oo;
	char					*name;
	int					 idx;
};

struct window_customize_modedata {
	struct window_pane			 *wp;
	int					  dead;
	int					  references;

	struct mode_tree_data			 *data;
	char					 *format;
	int					  hide_global;

	struct window_customize_itemdata	**item_list;
	u_int					  item_size;

	struct cmd_find_state			  fs;
};

static uint64_t
window_customize_get_tag(struct options_entry *o, int idx,
    const struct options_table_entry *oe)
{
	uint64_t	offset;

	if (oe == NULL)
		return ((uint64_t)o);
	offset = ((char *)oe - (char *)options_table) / sizeof *options_table;
	return ((2ULL << 62)|(offset << 32)|((idx + 1) << 1)|1);
}

static struct options *
window_customize_get_tree(enum window_customize_scope scope,
    struct cmd_find_state *fs)
{
	switch (scope) {
	case WINDOW_CUSTOMIZE_NONE:
	case WINDOW_CUSTOMIZE_KEY:
		return (NULL);
	case WINDOW_CUSTOMIZE_SERVER:
		return (global_options);
	case WINDOW_CUSTOMIZE_GLOBAL_SESSION:
		return (global_s_options);
	case WINDOW_CUSTOMIZE_SESSION:
		return (fs->s->options);
	case WINDOW_CUSTOMIZE_GLOBAL_WINDOW:
		return (global_w_options);
	case WINDOW_CUSTOMIZE_WINDOW:
		return (fs->w->options);
	case WINDOW_CUSTOMIZE_PANE:
		return (fs->wp->options);
	}
	return (NULL);
}

static int
window_customize_check_item(struct window_customize_modedata *data,
    struct window_customize_itemdata *item, struct cmd_find_state *fsp)
{
	struct cmd_find_state	fs;

	if (fsp == NULL)
		fsp = &fs;

	if (cmd_find_valid_state(&data->fs))
		cmd_find_copy_state(fsp, &data->fs);
	else
		cmd_find_from_pane(fsp, data->wp, 0);
	return (item->oo == window_customize_get_tree(item->scope, fsp));
}

static int
window_customize_get_key(struct window_customize_itemdata *item,
    struct key_table **ktp, struct key_binding **bdp)
{
	struct key_table	*kt;
	struct key_binding	*bd;

	kt = key_bindings_get_table(item->table, 0);
	if (kt == NULL)
		return (0);
	bd = key_bindings_get(kt, item->key);
	if (bd == NULL)
		return (0);

	if (ktp != NULL)
		*ktp = kt;
	if (bdp != NULL)
		*bdp = bd;
	return (1);
}

static char *
window_customize_scope_text(enum window_customize_scope scope,
    struct cmd_find_state *fs)
{
	char	*s;
	u_int	 idx;

	switch (scope) {
	case WINDOW_CUSTOMIZE_NONE:
	case WINDOW_CUSTOMIZE_KEY:
	case WINDOW_CUSTOMIZE_SERVER:
	case WINDOW_CUSTOMIZE_GLOBAL_SESSION:
	case WINDOW_CUSTOMIZE_GLOBAL_WINDOW:
		s = xstrdup("");
		break;
	case WINDOW_CUSTOMIZE_PANE:
		window_pane_index(fs->wp, &idx);
		xasprintf(&s, "pane %u", idx);
		break;
	case WINDOW_CUSTOMIZE_SESSION:
		xasprintf(&s, "session %s", fs->s->name);
		break;
	case WINDOW_CUSTOMIZE_WINDOW:
		xasprintf(&s, "window %u", fs->wl->idx);
		break;
	}
	return (s);
}

static struct window_customize_itemdata *
window_customize_add_item(struct window_customize_modedata *data)
{
	struct window_customize_itemdata	*item;

	data->item_list = xreallocarray(data->item_list, data->item_size + 1,
	    sizeof *data->item_list);
	item = data->item_list[data->item_size++] = xcalloc(1, sizeof *item);
	return (item);
}

static void
window_customize_free_item(struct window_customize_itemdata *item)
{
	free(item->table);
	free(item->name);
	free(item);
}

static void
window_customize_build_array(struct window_customize_modedata *data,
    struct mode_tree_item *top, enum window_customize_scope scope,
    struct options_entry *o, struct format_tree *ft)
{
	const struct options_table_entry	*oe = options_table_entry(o);
	struct options				*oo = options_owner(o);
	struct window_customize_itemdata	*item;
	struct options_array_item		*ai;
	char					*name, *value, *text;
	u_int					 idx;
	uint64_t				 tag;

	ai = options_array_first(o);
	while (ai != NULL) {
		idx = options_array_item_index(ai);

		xasprintf(&name, "%s[%u]", options_name(o), idx);
		format_add(ft, "option_name", "%s", name);
		value = options_to_string(o, idx, 0);
		format_add(ft, "option_value", "%s", value);

		item = window_customize_add_item(data);
		item->scope = scope;
		item->oo = oo;
		item->name = xstrdup(options_name(o));
		item->idx = idx;

		text = format_expand(ft, data->format);
		tag = window_customize_get_tag(o, idx, oe);
		mode_tree_add(data->data, top, item, tag, name, text, -1);
		free(text);

		free(name);
		free(value);

		ai = options_array_next(ai);
	}
}

static void
window_customize_build_option(struct window_customize_modedata *data,
    struct mode_tree_item *top, enum window_customize_scope scope,
    struct options_entry *o, struct format_tree *ft,
    const char *filter, struct cmd_find_state *fs)
{
	const struct options_table_entry	*oe = options_table_entry(o);
	struct options				*oo = options_owner(o);
	const char				*name = options_name(o);
	struct window_customize_itemdata	*item;
	char					*text, *expanded, *value;
	int					 global = 0, array = 0;
	uint64_t				 tag;

	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_HOOK))
		return;
	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY))
		array = 1;

	if (scope == WINDOW_CUSTOMIZE_SERVER ||
	    scope == WINDOW_CUSTOMIZE_GLOBAL_SESSION ||
	    scope == WINDOW_CUSTOMIZE_GLOBAL_WINDOW)
		global = 1;
	if (data->hide_global && global)
		return;

	format_add(ft, "option_name", "%s", name);
	format_add(ft, "option_is_global", "%d", global);
	format_add(ft, "option_is_array", "%d", array);

	text = window_customize_scope_text(scope, fs);
	format_add(ft, "option_scope", "%s", text);
	free(text);

	if (oe != NULL && oe->unit != NULL)
		format_add(ft, "option_unit", "%s", oe->unit);
	else
		format_add(ft, "option_unit", "%s", "");

	if (!array) {
		value = options_to_string(o, -1, 0);
		format_add(ft, "option_value", "%s", value);
		free(value);
	}

	if (filter != NULL) {
		expanded = format_expand(ft, filter);
		if (!format_true(expanded)) {
			free(expanded);
			return;
		}
		free(expanded);
	}
	item = window_customize_add_item(data);
	item->oo = oo;
	item->scope = scope;
	item->name = xstrdup(name);
	item->idx = -1;

	if (array)
		text = NULL;
	else
		text = format_expand(ft, data->format);
	tag = window_customize_get_tag(o, -1, oe);
	top = mode_tree_add(data->data, top, item, tag, name, text, 0);
	free(text);

	if (array)
		window_customize_build_array(data, top, scope, o, ft);
}

static void
window_customize_find_user_options(struct options *oo, const char ***list,
    u_int *size)
{
	struct options_entry	*o;
	const char		*name;
	u_int			 i;

	o = options_first(oo);
	while (o != NULL) {
		name = options_name(o);
		if (*name != '@') {
			o = options_next(o);
			continue;
		}
		for (i = 0; i < *size; i++) {
			if (strcmp((*list)[i], name) == 0)
				break;
		}
		if (i != *size) {
			o = options_next(o);
			continue;
		}
		*list = xreallocarray(*list, (*size) + 1, sizeof **list);
		(*list)[(*size)++] = name;

		o = options_next(o);
	}
}

static void
window_customize_build_options(struct window_customize_modedata *data,
    const char *title, uint64_t tag,
    enum window_customize_scope scope0, struct options *oo0,
    enum window_customize_scope scope1, struct options *oo1,
    enum window_customize_scope scope2, struct options *oo2,
    struct format_tree *ft, const char *filter, struct cmd_find_state *fs)
{
	struct mode_tree_item		 *top;
	struct options_entry		 *o, *loop;
	const char			**list = NULL, *name;
	u_int				  size = 0, i;
	enum window_customize_scope	  scope;

	top = mode_tree_add(data->data, NULL, NULL, tag, title, NULL, 0);

	/*
	 * We get the options from the first tree, but build it using the
	 * values from the other two. Any tree can have user options so we need
	 * to build a separate list of them.
	 */

	window_customize_find_user_options(oo0, &list, &size);
	if (oo1 != NULL)
		window_customize_find_user_options(oo1, &list, &size);
	if (oo2 != NULL)
		window_customize_find_user_options(oo2, &list, &size);

	for (i = 0; i < size; i++) {
		if (oo2 != NULL)
			o = options_get(oo0, list[i]);
		else if (oo1 != NULL)
			o = options_get(oo1, list[i]);
		else
			o = options_get(oo2, list[i]);
		if (options_owner(o) == oo2)
			scope = scope2;
		else if (options_owner(o) == oo1)
			scope = scope1;
		else
			scope = scope0;
		window_customize_build_option(data, top, scope, o, ft, filter,
		    fs);
	}
	free(list);

	loop = options_first(oo0);
	while (loop != NULL) {
		name = options_name(loop);
		if (*name == '@') {
			loop = options_next(loop);
			continue;
		}
		if (oo2 != NULL)
			o = options_get(oo2, name);
		else if (oo1 != NULL)
			o = options_get(oo1, name);
		else
			o = loop;
		if (options_owner(o) == oo2)
			scope = scope2;
		else if (options_owner(o) == oo1)
			scope = scope1;
		else
			scope = scope0;
		window_customize_build_option(data, top, scope, o, ft, filter,
		    fs);
		loop = options_next(loop);
	}
}

static void
window_customize_build_keys(struct window_customize_modedata *data,
    struct key_table *kt, struct format_tree *ft, const char *filter,
    struct cmd_find_state *fs, u_int number)
{
	struct mode_tree_item			*top, *child, *mti;
	struct window_customize_itemdata	*item;
	struct key_binding			*bd;
	char					*title, *text, *tmp, *expanded;
	const char				*flag;
	uint64_t				 tag;

	tag = (1ULL << 62)|((uint64_t)number << 54)|1;

	xasprintf(&title, "Key Table - %s", kt->name);
	top = mode_tree_add(data->data, NULL, NULL, tag, title, NULL, 0);
	free(title);

	ft = format_create_from_state(NULL, NULL, fs);
	format_add(ft, "is_option", "0");
	format_add(ft, "is_key", "1");

	bd = key_bindings_first(kt);
	while (bd != NULL) {
		format_add(ft, "key", "%s", key_string_lookup_key(bd->key, 0));
		if (bd->note != NULL)
			format_add(ft, "key_note", "%s", bd->note);
		if (filter != NULL) {
			expanded = format_expand(ft, filter);
			if (!format_true(expanded)) {
				free(expanded);
				continue;
			}
			free(expanded);
		}

		item = window_customize_add_item(data);
		item->scope = WINDOW_CUSTOMIZE_KEY;
		item->table = xstrdup(kt->name);
		item->key = bd->key;

		expanded = format_expand(ft, data->format);
		child = mode_tree_add(data->data, top, item, (uint64_t)bd,
		    expanded, NULL, 0);
		free(expanded);

		tmp = cmd_list_print(bd->cmdlist, 0);
		xasprintf(&text, "#[ignore]%s", tmp);
		free(tmp);
		mti = mode_tree_add(data->data, child, item,
		    tag|(bd->key << 3)|(0 << 1)|1, "Command", text, -1);
		mode_tree_draw_as_parent(mti);
		free(text);

		if (bd->note != NULL)
			xasprintf(&text, "#[ignore]%s", bd->note);
		else
			text = xstrdup("");
		mti = mode_tree_add(data->data, child, item,
		    tag|(bd->key << 3)|(1 << 1)|1, "Note", text, -1);
		mode_tree_draw_as_parent(mti);
		free(text);

		if (bd->flags & KEY_BINDING_REPEAT)
			flag = "on";
		else
			flag = "off";
		mti = mode_tree_add(data->data, child, item,
		    tag|(bd->key << 3)|(2 << 1)|1, "Repeat", flag, -1);
		mode_tree_draw_as_parent(mti);

		bd = key_bindings_next(kt, bd);
	}

	format_free(ft);
}

static void
window_customize_build(void *modedata,
    __unused struct mode_tree_sort_criteria *sort_crit, __unused uint64_t *tag,
    const char *filter)
{
	struct window_customize_modedata	*data = modedata;
	struct cmd_find_state			 fs;
	struct format_tree			*ft;
	u_int					 i;
	struct key_table			*kt;

	for (i = 0; i < data->item_size; i++)
		window_customize_free_item(data->item_list[i]);
	free(data->item_list);
	data->item_list = NULL;
	data->item_size = 0;

	if (cmd_find_valid_state(&data->fs))
		cmd_find_copy_state(&fs, &data->fs);
	else
		cmd_find_from_pane(&fs, data->wp, 0);

	ft = format_create_from_state(NULL, NULL, &fs);
	format_add(ft, "is_option", "1");
	format_add(ft, "is_key", "0");

	window_customize_build_options(data, "Server Options",
	    (3ULL << 62)|(OPTIONS_TABLE_SERVER << 1)|1,
	    WINDOW_CUSTOMIZE_SERVER, global_options,
	    WINDOW_CUSTOMIZE_NONE, NULL,
	    WINDOW_CUSTOMIZE_NONE, NULL,
	    ft, filter, &fs);
	window_customize_build_options(data, "Session Options",
	    (3ULL << 62)|(OPTIONS_TABLE_SESSION << 1)|1,
	    WINDOW_CUSTOMIZE_GLOBAL_SESSION, global_s_options,
	    WINDOW_CUSTOMIZE_SESSION, fs.s->options,
	    WINDOW_CUSTOMIZE_NONE, NULL,
	    ft, filter, &fs);
	window_customize_build_options(data, "Window & Pane Options",
	    (3ULL << 62)|(OPTIONS_TABLE_WINDOW << 1)|1,
	    WINDOW_CUSTOMIZE_GLOBAL_WINDOW, global_w_options,
	    WINDOW_CUSTOMIZE_WINDOW, fs.w->options,
	    WINDOW_CUSTOMIZE_PANE, fs.wp->options,
	    ft, filter, &fs);

	format_free(ft);
	ft = format_create_from_state(NULL, NULL, &fs);

	i = 0;
	kt = key_bindings_first_table();
	while (kt != NULL) {
		if (!RB_EMPTY(&kt->key_bindings)) {
			window_customize_build_keys(data, kt, ft, filter, &fs,
			    i);
			if (++i == 256)
				break;
		}
		kt = key_bindings_next_table(kt);
	}

	format_free(ft);
}

static void
window_customize_draw_key(__unused struct window_customize_modedata *data,
    struct window_customize_itemdata *item, struct screen_write_ctx *ctx,
    u_int sx, u_int sy)
{
	struct screen		*s = ctx->s;
	u_int			 cx = s->cx, cy = s->cy;
	struct key_table	*kt;
	struct key_binding	*bd, *default_bd;
	const char		*note, *period = "";
	char			*cmd, *default_cmd;

	if (item == NULL || !window_customize_get_key(item, &kt, &bd))
		return;

	note = bd->note;
	if (note == NULL)
		note = "There is no note for this key.";
	if (*note != '\0' && note[strlen (note) - 1] != '.')
		period = ".";
	if (!screen_write_text(ctx, cx, sx, sy, 0, &grid_default_cell, "%s%s",
	    note, period))
		return;
	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy >= cy + sy - 1)
		return;

	if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
	    &grid_default_cell, "This key is in the %s table.", kt->name))
		return;
	if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
	    &grid_default_cell, "This key %s repeat.",
	    (bd->flags & KEY_BINDING_REPEAT) ? "does" : "does not"))
		return;
	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy >= cy + sy - 1)
		return;

	cmd = cmd_list_print(bd->cmdlist, 0);
	if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
	    &grid_default_cell, "Command: %s", cmd)) {
		free(cmd);
		return;
	}
	default_bd = key_bindings_get_default(kt, bd->key);
	if (default_bd != NULL) {
		default_cmd = cmd_list_print(default_bd->cmdlist, 0);
		if (strcmp(cmd, default_cmd) != 0 &&
		    !screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "The default is: %s", default_cmd)) {
			free(default_cmd);
			free(cmd);
			return;
		}
		free(default_cmd);
	}
	free(cmd);
}

static void
window_customize_draw_option(struct window_customize_modedata *data,
    struct window_customize_itemdata *item, struct screen_write_ctx *ctx,
    u_int sx, u_int sy)
{
	struct screen				 *s = ctx->s;
	u_int					  cx = s->cx, cy = s->cy;
	int					  idx;
	struct options_entry			 *o, *parent;
	struct options				 *go, *wo;
	const struct options_table_entry	 *oe;
	struct grid_cell			  gc;
	const char				**choice, *text, *name;
	const char				 *space = "", *unit = "";
	char					 *value = NULL, *expanded;
	char					 *default_value = NULL;
	char					  choices[256] = "";
	struct cmd_find_state			  fs;
	struct format_tree			 *ft;

	if (!window_customize_check_item(data, item, &fs))
		return;
	name = item->name;
	idx = item->idx;

	o = options_get(item->oo, name);
	if (o == NULL)
		return;
	oe = options_table_entry(o);

	if (oe != NULL && oe->unit != NULL) {
		space = " ";
		unit = oe->unit;
	}
	ft = format_create_from_state(NULL, NULL, &fs);

	if (oe == NULL)
		text = "This is a user option.";
	else if (oe->text == NULL)
		text = "This option doesn't have a description.";
	else
		text = oe->text;
	if (!screen_write_text(ctx, cx, sx, sy, 0, &grid_default_cell, "%s",
	    text))
		goto out;
	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy >= cy + sy - 1)
		goto out;

	if (oe == NULL)
		text = "user";
	else if ((oe->scope & (OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE)) ==
	    (OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE))
		text = "window and pane";
	else if (oe->scope & OPTIONS_TABLE_WINDOW)
		text = "window";
	else if (oe->scope & OPTIONS_TABLE_SESSION)
		text = "session";
	else
		text = "server";
	if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
	    &grid_default_cell, "This is a %s option.", text))
		goto out;
	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY)) {
		if (idx != -1) {
			if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy),
			    0, &grid_default_cell,
			    "This is an array option, index %u.", idx))
				goto out;
		} else {
			if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy),
			    0, &grid_default_cell, "This is an array option."))
				goto out;
		}
		if (idx == -1)
			goto out;
	}
	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy >= cy + sy - 1)
		goto out;

	value = options_to_string(o, idx, 0);
	if (oe != NULL && idx == -1) {
		default_value = options_default_to_string(oe);
		if (strcmp(default_value, value) == 0) {
			free(default_value);
			default_value = NULL;
		}
	}
	if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
	    &grid_default_cell, "Option value: %s%s%s", value, space, unit))
		goto out;
	if (oe == NULL || oe->type == OPTIONS_TABLE_STRING) {
		expanded = format_expand(ft, value);
		if (strcmp(expanded, value) != 0) {
			if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy),
			    0, &grid_default_cell, "This expands to: %s",
			    expanded))
				goto out;
		}
		free(expanded);
	}
	if (oe != NULL && oe->type == OPTIONS_TABLE_CHOICE) {
		for (choice = oe->choices; *choice != NULL; choice++) {
			strlcat(choices, *choice, sizeof choices);
			strlcat(choices, ", ", sizeof choices);
		}
		choices[strlen(choices) - 2] = '\0';
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "Available values are: %s",
		    choices))
			goto out;
	}
	if (oe != NULL && oe->type == OPTIONS_TABLE_COLOUR) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 1,
		    &grid_default_cell, "This is a colour option: "))
			goto out;
		memcpy(&gc, &grid_default_cell, sizeof gc);
		gc.fg = options_get_number(item->oo, name);
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0, &gc,
		    "EXAMPLE"))
			goto out;
	}
	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_STYLE)) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 1,
		    &grid_default_cell, "This is a style option: "))
			goto out;
		style_apply(&gc, item->oo, name, ft);
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0, &gc,
		    "EXAMPLE"))
			goto out;
	}
	if (default_value != NULL) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "The default is: %s%s%s", default_value,
		    space, unit))
			goto out;
	}

	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy > cy + sy - 1)
		goto out;
	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY)) {
		wo = NULL;
		go = NULL;
	} else {
		switch (item->scope) {
		case WINDOW_CUSTOMIZE_PANE:
			wo = options_get_parent(item->oo);
			go = options_get_parent(wo);
			break;
		case WINDOW_CUSTOMIZE_WINDOW:
		case WINDOW_CUSTOMIZE_SESSION:
			wo = NULL;
			go = options_get_parent(item->oo);
			break;
		default:
			wo = NULL;
			go = NULL;
			break;
		}
	}
	if (wo != NULL && options_owner(o) != wo) {
		parent = options_get_only(wo, name);
		if (parent != NULL) {
			value = options_to_string(parent, -1 , 0);
			if (!screen_write_text(ctx, s->cx, sx,
			    sy - (s->cy - cy), 0, &grid_default_cell,
			    "Window value (from window %u): %s%s%s", fs.wl->idx,
			    value, space, unit))
				goto out;
		}
	}
	if (go != NULL && options_owner(o) != go) {
		parent = options_get_only(go, name);
		if (parent != NULL) {
			value = options_to_string(parent, -1 , 0);
			if (!screen_write_text(ctx, s->cx, sx,
			    sy - (s->cy - cy), 0, &grid_default_cell,
			    "Global value: %s%s%s", value, space, unit))
				goto out;
		}
	}

out:
	free(value);
	free(default_value);
	format_free(ft);
}

static void
window_customize_draw(void *modedata, void *itemdata,
    struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct window_customize_modedata	*data = modedata;
	struct window_customize_itemdata	*item = itemdata;

	if (item == NULL)
		return;

	if (item->scope == WINDOW_CUSTOMIZE_KEY)
		window_customize_draw_key(data, item, ctx, sx, sy);
	else
		window_customize_draw_option(data, item, ctx, sx, sy);
}

static void
window_customize_menu(void *modedata, struct client *c, key_code key)
{
	struct window_customize_modedata	*data = modedata;
	struct window_pane			*wp = data->wp;
	struct window_mode_entry		*wme;

	wme = TAILQ_FIRST(&wp->modes);
	if (wme == NULL || wme->data != modedata)
		return;
	window_customize_key(wme, c, NULL, NULL, key, NULL);
}

static u_int
window_customize_height(__unused void *modedata, __unused u_int height)
{
	return (12);
}

static struct screen *
window_customize_init(struct window_mode_entry *wme, struct cmd_find_state *fs,
    struct args *args)
{
	struct window_pane			*wp = wme->wp;
	struct window_customize_modedata	*data;
	struct screen				*s;

	wme->data = data = xcalloc(1, sizeof *data);
	data->wp = wp;
	data->references = 1;

	memcpy(&data->fs, fs, sizeof data->fs);

	if (args == NULL || !args_has(args, 'F'))
		data->format = xstrdup(WINDOW_CUSTOMIZE_DEFAULT_FORMAT);
	else
		data->format = xstrdup(args_get(args, 'F'));

	data->data = mode_tree_start(wp, args, window_customize_build,
	    window_customize_draw, NULL, window_customize_menu,
	    window_customize_height, data, window_customize_menu_items, NULL, 0,
	    &s);
	mode_tree_zoom(data->data, args);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);

	return (s);
}

static void
window_customize_destroy(struct window_customize_modedata *data)
{
	u_int	i;

	if (--data->references != 0)
		return;

	for (i = 0; i < data->item_size; i++)
		window_customize_free_item(data->item_list[i]);
	free(data->item_list);

	free(data->format);

	free(data);
}

static void
window_customize_free(struct window_mode_entry *wme)
{
	struct window_customize_modedata *data = wme->data;

	if (data == NULL)
		return;

	data->dead = 1;
	mode_tree_free(data->data);
	window_customize_destroy(data);
}

static void
window_customize_resize(struct window_mode_entry *wme, u_int sx, u_int sy)
{
	struct window_customize_modedata	*data = wme->data;

	mode_tree_resize(data->data, sx, sy);
}

static void
window_customize_free_callback(void *modedata)
{
	window_customize_destroy(modedata);
}

static void
window_customize_free_item_callback(void *itemdata)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;

	window_customize_free_item(item);
	window_customize_destroy(data);
}

static int
window_customize_set_option_callback(struct client *c, void *itemdata,
    const char *s, __unused int done)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	struct options_entry			*o;
	const struct options_table_entry	*oe;
	struct options				*oo = item->oo;
	const char				*name = item->name;
	char					*cause;
	int					 idx = item->idx;

	if (s == NULL || *s == '\0' || data->dead)
		return (0);
	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return (0);
	o = options_get(oo, name);
	if (o == NULL)
		return (0);
	oe = options_table_entry(o);

	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY)) {
		if (idx == -1) {
			for (idx = 0; idx < INT_MAX; idx++) {
				if (options_array_get(o, idx) == NULL)
					break;
			}
		}
		if (options_array_set(o, idx, s, 0, &cause) != 0)
			goto fail;
	} else {
		if (options_from_string(oo, oe, name, s, 0, &cause) != 0)
			goto fail;
	}

	options_push_changes(item->name);
	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (0);

fail:
	*cause = toupper((u_char)*cause);
	status_message_set(c, 1, "%s", cause);
	free(cause);
	return (0);
}

static void
window_customize_set_option(struct client *c,
    struct window_customize_modedata *data,
    struct window_customize_itemdata *item, int global, int pane)
{
	struct options_entry			*o;
	const struct options_table_entry	*oe;
	struct options				*oo;
	struct window_customize_itemdata	*new_item;
	int					 flag, idx = item->idx;
	enum window_customize_scope		 scope;
	u_int					 choice;
	const char				*name = item->name, *space = "";
	char					*prompt, *value, *text;
	struct cmd_find_state			 fs;

	if (item == NULL || !window_customize_check_item(data, item, &fs))
		return;
	o = options_get(item->oo, name);
	if (o == NULL)
		return;

	oe = options_table_entry(o);
	if (~oe->scope & OPTIONS_TABLE_PANE)
		pane = 0;
	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY)) {
		scope = item->scope;
		oo = item->oo;
	} else {
		if (global) {
			switch (item->scope) {
			case WINDOW_CUSTOMIZE_NONE:
			case WINDOW_CUSTOMIZE_KEY:
			case WINDOW_CUSTOMIZE_SERVER:
			case WINDOW_CUSTOMIZE_GLOBAL_SESSION:
			case WINDOW_CUSTOMIZE_GLOBAL_WINDOW:
				scope = item->scope;
				break;
			case WINDOW_CUSTOMIZE_SESSION:
				scope = WINDOW_CUSTOMIZE_GLOBAL_SESSION;
				break;
			case WINDOW_CUSTOMIZE_WINDOW:
			case WINDOW_CUSTOMIZE_PANE:
				scope = WINDOW_CUSTOMIZE_GLOBAL_WINDOW;
				break;
			}
		} else {
			switch (item->scope) {
			case WINDOW_CUSTOMIZE_NONE:
			case WINDOW_CUSTOMIZE_KEY:
			case WINDOW_CUSTOMIZE_SERVER:
			case WINDOW_CUSTOMIZE_SESSION:
				scope = item->scope;
				break;
			case WINDOW_CUSTOMIZE_WINDOW:
			case WINDOW_CUSTOMIZE_PANE:
				if (pane)
					scope = WINDOW_CUSTOMIZE_PANE;
				else
					scope = WINDOW_CUSTOMIZE_WINDOW;
				break;
			case WINDOW_CUSTOMIZE_GLOBAL_SESSION:
				scope = WINDOW_CUSTOMIZE_SESSION;
				break;
			case WINDOW_CUSTOMIZE_GLOBAL_WINDOW:
				if (pane)
					scope = WINDOW_CUSTOMIZE_PANE;
				else
					scope = WINDOW_CUSTOMIZE_WINDOW;
				break;
			}
		}
		if (scope == item->scope)
			oo = item->oo;
		else
			oo = window_customize_get_tree(scope, &fs);
	}

	if (oe != NULL && oe->type == OPTIONS_TABLE_FLAG) {
		flag = options_get_number(oo, name);
		options_set_number(oo, name, !flag);
	} else if (oe != NULL && oe->type == OPTIONS_TABLE_CHOICE) {
		choice = options_get_number(oo, name);
		if (oe->choices[choice + 1] == NULL)
			choice = 0;
		else
			choice++;
		options_set_number(oo, name, choice);
	} else {
		text = window_customize_scope_text(scope, &fs);
		if (*text != '\0')
			space = ", for ";
		else if (scope != WINDOW_CUSTOMIZE_SERVER)
			space = ", global";
		if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY)) {
			if (idx == -1) {
				xasprintf(&prompt, "(%s[+]%s%s) ", name, space,
				    text);
			} else {
				xasprintf(&prompt, "(%s[%d]%s%s) ", name, idx,
				    space, text);
			}
		} else
			xasprintf(&prompt, "(%s%s%s) ", name, space, text);
		free(text);

		value = options_to_string(o, idx, 0);

		new_item = xcalloc(1, sizeof *new_item);
		new_item->data = data;
		new_item->scope = scope;
		new_item->oo = oo;
		new_item->name = xstrdup(name);
		new_item->idx = idx;

		data->references++;
		status_prompt_set(c, NULL, prompt, value,
		    window_customize_set_option_callback,
		    window_customize_free_item_callback, new_item,
		    PROMPT_NOFORMAT);

		free(prompt);
		free(value);
	}
}

static void
window_customize_unset_option(struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	struct options_entry			*o;
	const struct options_table_entry	*oe;

	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return;

	o = options_get(item->oo, item->name);
	if (o == NULL)
		return;
	if (item->idx != -1) {
		if (item == mode_tree_get_current(data->data))
			mode_tree_up(data->data, 0);
		options_array_set(o, item->idx, NULL, 0, NULL);
		return;
	}
	oe = options_table_entry(o);
	if (oe != NULL &&
	    options_owner(o) != global_options &&
	    options_owner(o) != global_s_options &&
	    options_owner(o) != global_w_options)
		options_remove(o);
	else
		options_default(options_owner(o), oe);
}

static int
window_customize_set_command_callback(struct client *c, void *itemdata,
    const char *s, __unused int done)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	struct key_binding			*bd;
	struct cmd_parse_result			*pr;
	char					*error;

	if (s == NULL || *s == '\0' || data->dead)
		return (0);
	if (item == NULL || !window_customize_get_key(item, NULL, &bd))
		return (0);

	pr = cmd_parse_from_string(s, NULL);
	switch (pr->status) {
	case CMD_PARSE_EMPTY:
		error = xstrdup("empty command");
		goto fail;
	case CMD_PARSE_ERROR:
		error = pr->error;
		goto fail;
	case CMD_PARSE_SUCCESS:
		break;
	}
	cmd_list_free(bd->cmdlist);
	bd->cmdlist = pr->cmdlist;

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (0);

fail:
	*error = toupper((u_char)*error);
	status_message_set(c, 1, "%s", error);
	free(error);
	return (0);
}

static int
window_customize_set_note_callback(__unused struct client *c, void *itemdata,
    const char *s, __unused int done)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	struct key_binding			*bd;

	if (s == NULL || *s == '\0' || data->dead)
		return (0);
	if (item == NULL || !window_customize_get_key(item, NULL, &bd))
		return (0);

	free((void *)bd->note);
	bd->note = xstrdup(s);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (0);
}

static void
window_customize_set_key(struct client *c,
    struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	key_code				 key = item->key;
	struct key_binding			*bd;
	const char				*s;
	char					*prompt, *value;
	struct window_customize_itemdata	*new_item;

	if (item == NULL || !window_customize_get_key(item, NULL, &bd))
		return;

	s = mode_tree_get_current_name(data->data);
	if (strcmp(s, "Repeat") == 0)
		bd->flags ^= KEY_BINDING_REPEAT;
	else if (strcmp(s, "Command") == 0) {
		xasprintf(&prompt, "(%s) ", key_string_lookup_key(key, 0));
		value = cmd_list_print(bd->cmdlist, 0);

		new_item = xcalloc(1, sizeof *new_item);
		new_item->data = data;
		new_item->scope = item->scope;
		new_item->table = xstrdup(item->table);
		new_item->key = key;

		data->references++;
		status_prompt_set(c, NULL, prompt, value,
		    window_customize_set_command_callback,
		    window_customize_free_item_callback, new_item,
		    PROMPT_NOFORMAT);
		free(prompt);
		free(value);
	} else if (strcmp(s, "Note") == 0) {
		xasprintf(&prompt, "(%s) ", key_string_lookup_key(key, 0));

		new_item = xcalloc(1, sizeof *new_item);
		new_item->data = data;
		new_item->scope = item->scope;
		new_item->table = xstrdup(item->table);
		new_item->key = key;

		data->references++;
		status_prompt_set(c, NULL, prompt,
		    (bd->note == NULL ? "" : bd->note),
		    window_customize_set_note_callback,
		    window_customize_free_item_callback, new_item,
		    PROMPT_NOFORMAT);
		free(prompt);
	}
}

static void
window_customize_unset_key(struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	struct key_table	*kt;
	struct key_binding	*bd;

	if (item == NULL || !window_customize_get_key(item, &kt, &bd))
		return;

	if (item == mode_tree_get_current(data->data)) {
		mode_tree_collapse_current(data->data);
		mode_tree_up(data->data, 0);
	}
	key_bindings_remove(kt->name, bd->key);
}

static void
window_customize_unset_each(void *modedata, void *itemdata,
    __unused struct client *c, __unused key_code key)
{
	struct window_customize_itemdata	*item = itemdata;

	if (item->scope == WINDOW_CUSTOMIZE_KEY)
		window_customize_unset_key(modedata, item);
	else {
		window_customize_unset_option(modedata, item);
		options_push_changes(item->name);
	}
}

static int
window_customize_unset_current_callback(__unused struct client *c,
    void *modedata, const char *s, __unused int done)
{
	struct window_customize_modedata	*data = modedata;
	struct window_customize_itemdata	*item;

	if (s == NULL || *s == '\0' || data->dead)
		return (0);
	if (tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		return (0);

	item = mode_tree_get_current(data->data);
	if (item->scope == WINDOW_CUSTOMIZE_KEY)
		window_customize_unset_key(data, item);
	else {
		window_customize_unset_option(data, item);
		options_push_changes(item->name);
	}
	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (0);
}

static int
window_customize_unset_tagged_callback(struct client *c, void *modedata,
    const char *s, __unused int done)
{
	struct window_customize_modedata	*data = modedata;

	if (s == NULL || *s == '\0' || data->dead)
		return (0);
	if (tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		return (0);

	mode_tree_each_tagged(data->data, window_customize_unset_each, c,
	    KEYC_NONE, 0);
	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (0);
}

static void
window_customize_key(struct window_mode_entry *wme, struct client *c,
    __unused struct session *s, __unused struct winlink *wl, key_code key,
    struct mouse_event *m)
{
	struct window_pane			*wp = wme->wp;
	struct window_customize_modedata	*data = wme->data;
	struct window_customize_itemdata	*item, *new_item;
	int					 finished;
	char					*prompt;
	u_int					 tagged;

	item = mode_tree_get_current(data->data);
	finished = mode_tree_key(data->data, c, &key, m, NULL, NULL);
	if (item != (new_item = mode_tree_get_current(data->data)))
		item = new_item;

	switch (key) {
	case '\r':
	case 's':
		if (item == NULL)
			break;
		if (item->scope == WINDOW_CUSTOMIZE_KEY)
			window_customize_set_key(c, data, item);
		else {
			window_customize_set_option(c, data, item, 0, 1);
			options_push_changes(item->name);
		}
		mode_tree_build(data->data);
		break;
	case 'w':
		if (item == NULL || item->scope == WINDOW_CUSTOMIZE_KEY)
			break;
		window_customize_set_option(c, data, item, 0, 0);
		options_push_changes(item->name);
		mode_tree_build(data->data);
		break;
	case 'S':
	case 'W':
		if (item == NULL || item->scope == WINDOW_CUSTOMIZE_KEY)
			break;
		window_customize_set_option(c, data, item, 1, 0);
		options_push_changes(item->name);
		mode_tree_build(data->data);
		break;
	case 'u':
		if (item == NULL)
			break;
		if (item->scope == WINDOW_CUSTOMIZE_KEY) {
			xasprintf(&prompt, "Unbind key %s? ",
			    key_string_lookup_key(item->key, 0));
		} else
			xasprintf(&prompt, "Unset option %s? ", item->name);
		data->references++;
		status_prompt_set(c, NULL, prompt, "",
		    window_customize_unset_current_callback,
		    window_customize_free_callback, data,
		    PROMPT_SINGLE|PROMPT_NOFORMAT);
		free(prompt);
		break;
	case 'U':
		tagged = mode_tree_count_tagged(data->data);
		if (tagged == 0)
			break;
		xasprintf(&prompt, "Unset or unbind %u tagged? ", tagged);
		data->references++;
		status_prompt_set(c, NULL, prompt, "",
		    window_customize_unset_tagged_callback,
		    window_customize_free_callback, data,
		    PROMPT_SINGLE|PROMPT_NOFORMAT);
		free(prompt);
		break;
	case 'H':
		data->hide_global = !data->hide_global;
		mode_tree_build(data->data);
		break;
	}
	if (finished)
		window_pane_reset_mode(wp);
	else {
		mode_tree_draw(data->data);
		wp->flags |= PANE_REDRAW;
	}
}
