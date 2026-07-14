/* $OpenBSD: window-customize.c,v 1.34 2026/07/14 17:17:18 nicm Exp $ */

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
			     struct cmdq_item *, struct cmd_find_state *,
			     struct args *);
static void		 window_customize_free(struct window_mode_entry *);
static void		 window_customize_resize(struct window_mode_entry *,
			      u_int, u_int);
static void		 window_customize_update(struct window_mode_entry *);
static void		 window_customize_key(struct window_mode_entry *,
			     struct client *, struct session *,
			     struct winlink *, key_code, struct mouse_event *);

#define WINDOW_CUSTOMIZE_DEFAULT_FORMAT \
	"#{?is_option," \
		"#{?option_is_global,,#[reverse](#{option_scope})#[default] }" \
		"#[fg=themelightgrey]#[ignore]#{option_value}" \
		"#{?option_unit, #{option_unit},}" \
	"," \
		"#{?is_environment," \
			"#[fg=themelightgrey]#[ignore]#{environment_value}" \
		"," \
			"#{key}" \
		"}" \
	"}"

static const struct menu_item window_customize_menu_items[] = {
	{ "Select", '\r', NULL },
	{ "Edit", 'e', NULL },
	{ "Expand", KEYC_RIGHT, NULL },
	{ "", KEYC_NONE, NULL },
	{ "Tag", 't', NULL },
	{ "Tag All", '\024', NULL },
	{ "Tag None", 'T', NULL },
	{ "", KEYC_NONE, NULL },
	{ "Changed Only", 'C', NULL },
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
	.update = window_customize_update,
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
	WINDOW_CUSTOMIZE_PANE,
	WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT,
	WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT
};

enum window_customize_change {
	WINDOW_CUSTOMIZE_UNSET,
	WINDOW_CUSTOMIZE_RESET,
};

enum window_customize_option_type {
	WINDOW_CUSTOMIZE_OPTIONS,
	WINDOW_CUSTOMIZE_HOOKS
};

enum window_customize_item_type {
	WINDOW_CUSTOMIZE_ITEM_OPTION,
	WINDOW_CUSTOMIZE_ITEM_KEY,
	WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT
};

enum window_customize_edit_type {
	WINDOW_CUSTOMIZE_EDIT_OPTION,
	WINDOW_CUSTOMIZE_EDIT_KEY_COMMAND,
	WINDOW_CUSTOMIZE_EDIT_KEY_NOTE,
	WINDOW_CUSTOMIZE_EDIT_ENVIRONMENT
};

struct window_customize_itemdata {
	struct window_customize_modedata	 *data;
	enum window_customize_item_type	  type;
	enum window_customize_option_type  option_type;
	enum window_customize_scope		  scope;

	char					 *table;
	key_code				  key;

	struct options				 *oo;
	struct environ				 *environ;
	int					  environ_flags;
	char					 *name;
	char					 *array_key;
};

struct window_customize_modedata {
	struct window_pane			 *wp;
	int					  dead;
	int					  references;

	struct mode_tree_data			 *data;
	struct spawn_editor_state		 *editor;
	struct window_customize_editdata	 *edit;
	char					 *format;
	int					  hide_global;
	int					  hide_default;
	int					  prompt_flags;

	struct window_customize_itemdata	**item_list;
	u_int					  item_size;

	struct cmd_find_state			  fs;
	enum window_customize_change		  change;
};

struct window_customize_editdata {
	u_int					  wp_id;
	enum window_customize_edit_type		  edit_type;
	struct window_customize_itemdata	 *item;
	struct spawn_editor_state		 *editor;
};

static uint64_t
window_customize_get_tag(struct options_entry *o, struct options_array_item *a,
    const struct options_table_entry *oe)
{
	uint64_t	offset;

	if (a != NULL)
		return ((uint64_t)(uintptr_t)a);
	if (oe == NULL)
		return ((uint64_t)o);
	offset = ((char *)oe - (char *)options_table) / sizeof *options_table;
	return ((2ULL << 62)|(offset << 32)|1);
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
	case WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT:
	case WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT:
		return (NULL);
	}
	return (NULL);
}

static struct environ *
window_customize_get_environment(enum window_customize_scope scope,
    struct cmd_find_state *fs)
{
	switch (scope) {
	case WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT:
		return (global_environ);
	case WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT:
		return (fs->s->environ);
	default:
		return (NULL);
	}
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
	if (item->type == WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT) {
		return (item->environ ==
		    window_customize_get_environment(item->scope, fsp));
	}
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
	case WINDOW_CUSTOMIZE_PANE:
		window_pane_index(fs->wp, &idx);
		xasprintf(&s, "pane %u", idx);
		break;
	case WINDOW_CUSTOMIZE_SESSION:
	case WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT:
		xasprintf(&s, "session %s", fs->s->name);
		break;
	case WINDOW_CUSTOMIZE_WINDOW:
		xasprintf(&s, "window %u", fs->wl->idx);
		break;
	default:
		s = xstrdup("");
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

static int printflike(7, 8)
window_customize_write_value(struct screen_write_ctx *ctx, u_int cx,
    u_int sx, u_int sy, int more, const char *label, const char *fmt, ...)
{
	struct screen		*s = ctx->s;
	struct grid_cell	 gc;
	va_list			 ap;
	char			*value;
	u_int			 cy = s->cy;
	int			 retval;

	if (sy == 0)
		return (0);
	if (!screen_write_text(ctx, cx, sx, sy, 1, &grid_default_cell, "%s",
	    label))
		return (0);
	if (s->cy - cy >= sy)
		return (0);
	sy -= s->cy - cy;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	gc.fg = COLOUR_THEME_LIGHT_GREY|COLOUR_FLAG_THEME;

	va_start(ap, fmt);
	xvasprintf(&value, fmt, ap);
	va_end(ap);

	retval = screen_write_text(ctx, cx, sx, sy, more, &gc, "%s", value);
	free(value);
	return (retval);
}

static void
window_customize_free_item(struct window_customize_itemdata *item)
{
	free(item->table);
	free(item->name);
	free(item->array_key);
	free(item);
}

static struct window_customize_itemdata *
window_customize_copy_item(struct window_customize_itemdata *item)
{
	struct window_customize_itemdata	*new_item;

	new_item = xcalloc(1, sizeof *new_item);
	new_item->data = item->data;
	new_item->type = item->type;
	new_item->option_type = item->option_type;
	new_item->scope = item->scope;
	new_item->key = item->key;
	new_item->oo = item->oo;
	new_item->environ = item->environ;
	new_item->environ_flags = item->environ_flags;
	if (item->table != NULL)
		new_item->table = xstrdup(item->table);
	if (item->name != NULL)
		new_item->name = xstrdup(item->name);
	if (item->array_key != NULL)
		new_item->array_key = xstrdup(item->array_key);
	return (new_item);
}

static void
window_customize_finish_edit(struct window_customize_editdata *ed)
{
	window_customize_free_item(ed->item);
	free(ed);
}

static void
window_customize_draw_waiting(struct window_customize_modedata *data)
{
	struct screen_write_ctx	 ctx;
	struct screen		*s = data->wp->screen;
	struct grid_cell	 gc;
	char			 text[128];
	u_int			 sx, sy, box_w, box_h, x, y, text_x;
	size_t			 textlen;
	pid_t			 pid;

	if (data->editor == NULL)
		return;
	sx = screen_size_x(s);
	sy = screen_size_y(s);
	if (sx == 0 || sy == 0)
		return;

	pid = spawn_get_editor_pid(data->editor);
	if (pid == -1)
		xsnprintf(text, sizeof text, "WAITING FOR EDITOR");
	else {
		xsnprintf(text, sizeof text, "WAITING FOR EDITOR (PID %ld)",
		    (long)pid);
	}

	textlen = strlen(text);
	box_w = textlen + 4;
	box_h = 3;
	if (sx < box_w || sy < box_h)
		return;
	x = (sx - box_w) / 2;
	y = (sy - box_h) / 2;
	text_x = x + (box_w - textlen) / 2;

	memcpy(&gc, &grid_default_cell, sizeof gc);
	screen_write_start(&ctx, s);
	screen_write_cursormove(&ctx, x, y, 0);
	screen_write_box(&ctx, box_w, box_h, BOX_LINES_DEFAULT, &gc, NULL);
	screen_write_cursormove(&ctx, text_x, y + 1, 0);
	screen_write_nputs(&ctx, box_w - 2, &gc, "%s", text);
	screen_write_stop(&ctx);
}

static int
window_customize_set_option_value(struct window_customize_itemdata *item,
    const char *s, char **cause)
{
	struct options_entry			*o;
	const struct options_table_entry	*oe;
	struct options				*oo = item->oo;
	const char				*name = item->name;
	const char				*array_key = item->array_key;
	u_int					 idx;
	char					 keybuf[32];

	o = options_get(oo, name);
	if (o == NULL)
		return (-1);
	oe = options_table_entry(o);

	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY)) {
		if (array_key == NULL) {
			for (idx = 0; idx < INT_MAX; idx++) {
				if (options_array_getv(o, "%u", idx) == NULL)
					break;
			}
			xsnprintf(keybuf, sizeof keybuf, "%u", idx);
			array_key = keybuf;
		}
		if (options_array_set(o, array_key, s, 0, cause) != 0)
			return (-1);
	} else {
		if (options_from_string(oo, oe, name, s, 0, cause) != 0)
			return (-1);
	}
	if (item->option_type == WINDOW_CUSTOMIZE_HOOKS && *name == '@')
		hooks_add_event(name);

	options_push_changes(item->name);
	return (0);
}

static int
window_customize_option_editable(struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	struct options_entry			*o;
	const struct options_table_entry	*oe;

	if (item->type != WINDOW_CUSTOMIZE_ITEM_OPTION ||
	    !window_customize_check_item(data, item, NULL))
		return (0);

	o = options_get(item->oo, item->name);
	if (o == NULL)
		return (0);
	oe = options_table_entry(o);
	if (oe == NULL)
		return (1);
	if (oe->type == OPTIONS_TABLE_FLAG || oe->type == OPTIONS_TABLE_CHOICE)
		return (0);
	return (1);
}

static int
window_customize_set_command_value(struct window_customize_itemdata *item,
    const char *s, char **cause)
{
	struct key_binding		*bd;
	struct cmd_parse_result		*pr;

	if (!window_customize_get_key(item, NULL, &bd))
		return (-1);

	pr = cmd_parse_from_string(s, NULL);
	switch (pr->status) {
	case CMD_PARSE_ERROR:
		*cause = pr->error;
		return (-1);
	case CMD_PARSE_SUCCESS:
		break;
	}
	cmd_list_free(bd->cmdlist);
	bd->cmdlist = pr->cmdlist;
	return (0);
}

static int
window_customize_set_note_value(struct window_customize_itemdata *item,
    const char *s)
{
	struct key_binding	*bd;

	if (!window_customize_get_key(item, NULL, &bd))
		return (-1);

	free((void *)bd->note);
	if (*s == '\0')
		bd->note = NULL;
	else
		bd->note = xstrdup(s);
	return (0);
}

static void
window_customize_set_environment_value(struct window_customize_itemdata *item,
    const char *s)
{
	struct environ_entry	*envent;
	int			 flags;

	flags = item->environ_flags;
	envent = environ_find(item->environ, item->name);
	if (envent != NULL)
		flags = envent->flags;
	environ_set(item->environ, item->name, flags, "%s", s);
}

static int
window_customize_option_is_changed(struct options_entry *o,
    const char *array_key)
{
	const struct options_table_entry	*oe = options_table_entry(o);
	struct options			*oo;
	struct options_entry		*defaults;
	union options_value		*ov, *default_ov;
	char				*value, *default_value;
	int				 changed;

	if (oe == NULL || options_get_monitor_data(o) != NULL)
		return (1);
	if (*options_name(o) == '@' && hooks_is_event(options_name(o)))
		return (1);

	if (oe->flags & OPTIONS_TABLE_IS_ARRAY) {
		oo = options_create(NULL);
		defaults = options_default(oo, oe);
		if (array_key != NULL) {
			ov = options_array_get(o, array_key);
			default_ov = options_array_get(defaults, array_key);
			if (ov == NULL || default_ov == NULL) {
				changed = (ov != default_ov);
				options_free(oo);
				return (changed);
			}
		}
		value = options_to_string(o, array_key, 0);
		default_value = options_to_string(defaults, array_key, 0);
		changed = (strcmp(value, default_value) != 0);
		free(value);
		free(default_value);
		options_free(oo);
		return (changed);
	}

	value = options_to_string(o, NULL, 0);
	default_value = options_default_to_string(oe);
	changed = (strcmp(value, default_value) != 0);
	free(value);
	free(default_value);
	return (changed);
}

static int
window_customize_key_is_changed(struct key_table *kt, struct key_binding *bd)
{
	struct key_binding	*default_bd;
	char			*cmd, *default_cmd;
	int			 changed;

	default_bd = key_bindings_get_default(kt, bd->key);
	if (default_bd == NULL)
		return (1);
	if (bd->flags != default_bd->flags)
		return (1);
	if ((bd->note == NULL) != (default_bd->note == NULL))
		return (1);
	if (bd->note != NULL && strcmp(bd->note, default_bd->note) != 0)
		return (1);

	cmd = cmd_list_print(bd->cmdlist, 0);
	default_cmd = cmd_list_print(default_bd->cmdlist, 0);
	changed = (strcmp(cmd, default_cmd) != 0);
	free(cmd);
	free(default_cmd);
	return (changed);
}

static u_int
window_customize_build_array(struct window_customize_modedata *data,
    struct mode_tree_item *top, enum window_customize_scope scope,
    struct options_entry *o, struct format_tree *ft)
{
	const struct options_table_entry	*oe = options_table_entry(o);
	struct options				*oo = options_owner(o);
	struct window_customize_itemdata	*item;
	struct options_array_item		*ai;
	char					*name, *value, *text;
	uint64_t				 tag;
	const char				*array_key;
	u_int					 count = 0;

	ai = options_array_first(o);
	while (ai != NULL) {
		array_key = options_array_item_key(ai);
		if (data->hide_default &&
		    !window_customize_option_is_changed(o, array_key)) {
			ai = options_array_next(ai);
			continue;
		}

		xasprintf(&name, "%s[%s]", options_name(o), array_key);
		format_add(ft, "option_name", "%s", name);
		value = options_to_string(o, array_key, 0);
		format_add(ft, "option_value", "%s", value);

		item = window_customize_add_item(data);
		item->type = WINDOW_CUSTOMIZE_ITEM_OPTION;
		if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_HOOK))
			item->option_type = WINDOW_CUSTOMIZE_HOOKS;
		item->scope = scope;
		item->oo = oo;
		item->name = xstrdup(options_name(o));
		item->array_key = xstrdup(array_key);

		text = format_expand(ft, data->format);
		tag = window_customize_get_tag(o, ai, oe);
		mode_tree_add(data->data, top, item, tag, name, text, -1);
		free(text);

		free(name);
		free(value);
		count++;

		ai = options_array_next(ai);
	}
	return (count);
}

static u_int
window_customize_build_option(struct window_customize_modedata *data,
    struct mode_tree_item *top, enum window_customize_scope scope,
    struct options_entry *o, struct format_tree *ft,
    const char *filter, struct cmd_find_state *fs,
    enum window_customize_option_type type)
{
	const struct options_table_entry	*oe = options_table_entry(o);
	struct options				*oo = options_owner(o);
	const char				*name = options_name(o);
	struct window_customize_itemdata	*item;
	char					*text, *expanded, *value;
	int					 global = 0, array = 0;
	int					 is_hook = 0, is_monitor = 0;
	int					 is_user_hook = 0;
	uint64_t				 tag;

	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_HOOK))
		is_hook = 1;
	if (options_get_monitor_data(o) != NULL)
		is_monitor = 1;
	if (*name == '@' && hooks_is_event(name))
		is_user_hook = 1;
	switch (type) {
	case WINDOW_CUSTOMIZE_OPTIONS:
		if (is_hook || is_monitor || is_user_hook)
			return (0);
		break;
	case WINDOW_CUSTOMIZE_HOOKS:
		if (!is_hook && !is_monitor && !is_user_hook)
			return (0);
		break;
	}
	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY))
		array = 1;

	if (scope == WINDOW_CUSTOMIZE_SERVER ||
	    scope == WINDOW_CUSTOMIZE_GLOBAL_SESSION ||
	    scope == WINDOW_CUSTOMIZE_GLOBAL_WINDOW)
		global = 1;
	if (data->hide_global && global)
		return (0);
	if (data->hide_default && !window_customize_option_is_changed(o, NULL))
		return (0);

	format_add(ft, "option_name", "%s", name);
	format_add(ft, "option_is_global", "%d", global);
	format_add(ft, "option_is_array", "%d", array);
	format_add(ft, "option_is_hook", "%d", is_hook);
	format_add(ft, "option_is_monitor", "%d", is_monitor);

	text = window_customize_scope_text(scope, fs);
	format_add(ft, "option_scope", "%s", text);
	free(text);

	if (oe != NULL && oe->unit != NULL)
		format_add(ft, "option_unit", "%s", oe->unit);
	else
		format_add(ft, "option_unit", "%s", "");

	if (is_monitor) {
		value = hooks_monitor_to_string(o);
		if (value != NULL) {
			format_add(ft, "option_monitor", "%s", value);
			free(value);
		} else
			format_add(ft, "option_monitor", "%s", "");
	} else
		format_add(ft, "option_monitor", "%s", "");

	if (!array) {
		value = options_to_string(o, NULL, 0);
		format_add(ft, "option_value", "%s", value);
		free(value);
	}

	if (filter != NULL) {
		expanded = format_expand(ft, filter);
		if (!format_true(expanded)) {
			free(expanded);
			return (0);
		}
		free(expanded);
	}
	item = window_customize_add_item(data);
	item->type = WINDOW_CUSTOMIZE_ITEM_OPTION;
	item->option_type = type;
	item->oo = oo;
	item->scope = scope;
	item->name = xstrdup(name);

	if (array)
		text = NULL;
	else
		text = format_expand(ft, data->format);
	tag = window_customize_get_tag(o, NULL, oe);
	top = mode_tree_add(data->data, top, item, tag, name, text, 0);
	free(text);

	if (!array)
		return (1);
	return (1 + window_customize_build_array(data, top, scope, o, ft));
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
    struct format_tree *ft, const char *filter, struct cmd_find_state *fs,
    enum window_customize_option_type type)
{
	struct mode_tree_item		 *top;
	struct options_entry		 *o = NULL, *loop;
	const char			**list = NULL, *name;
	u_int				  size = 0, i, count = 0;
	enum window_customize_scope	  scope;

	top = mode_tree_add(data->data, NULL, NULL, tag, title, NULL, 0);
	mode_tree_no_tag(top);

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
		o = NULL;
		if (oo2 != NULL)
			o = options_get(oo2, list[i]);
		if (o == NULL && oo1 != NULL)
			o = options_get(oo1, list[i]);
		if (o == NULL)
			o = options_get(oo0, list[i]);
		if (options_owner(o) == oo2)
			scope = scope2;
		else if (options_owner(o) == oo1)
			scope = scope1;
		else
			scope = scope0;
		count += window_customize_build_option(data, top, scope, o, ft,
		    filter, fs, type);
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
		count += window_customize_build_option(data, top, scope, o, ft,
		    filter, fs, type);
		loop = options_next(loop);
	}
	if (data->hide_default && count == 0)
		mode_tree_remove(data->data, top);
}

static uint64_t
window_customize_key_tag(const void *ptr, u_int type)
{
	return ((uint64_t)(uintptr_t)ptr|type);
}

static void
window_customize_build_keys(struct window_customize_modedata *data,
    struct key_table *kt, struct format_tree *ft, const char *filter,
    struct cmd_find_state *fs)
{
	struct mode_tree_item			*top, *child, *mti;
	struct window_customize_itemdata	*item;
	struct key_binding			*bd;
	char					*title, *text, *tmp, *expanded;
	const char				*flag;
	u_int					 count = 0;

	xasprintf(&title, "Key Table - %s", kt->name);
	top = mode_tree_add(data->data, NULL, NULL,
	    window_customize_key_tag(kt, 0), title, NULL, 0);
	mode_tree_no_tag(top);
	free(title);

	ft = format_create_from_state(NULL, NULL, fs);
	format_add(ft, "is_option", "0");
	format_add(ft, "is_key", "1");
	format_add(ft, "is_environment", "0");

	bd = key_bindings_first(kt);
	while (bd != NULL) {
		if (data->hide_default &&
		    !window_customize_key_is_changed(kt, bd)) {
			bd = key_bindings_next(kt, bd);
			continue;
		}

		format_add(ft, "key", "%s", key_string_lookup_key(bd->key, 0));
		if (bd->note != NULL)
			format_add(ft, "key_note", "%s", bd->note);
		if (filter != NULL) {
			expanded = format_expand(ft, filter);
			if (!format_true(expanded)) {
				free(expanded);
				bd = key_bindings_next(kt, bd);
				continue;
			}
			free(expanded);
		}

		item = window_customize_add_item(data);
		item->type = WINDOW_CUSTOMIZE_ITEM_KEY;
		item->scope = WINDOW_CUSTOMIZE_KEY;
		item->table = xstrdup(kt->name);
		item->key = bd->key;
		item->name = xstrdup(key_string_lookup_key(item->key, 0));

		expanded = format_expand(ft, data->format);
		child = mode_tree_add(data->data, top, item,
		    window_customize_key_tag(bd, 0), expanded, NULL, 0);
		free(expanded);

		tmp = cmd_list_print(bd->cmdlist, 0);
		xasprintf(&text, "#[fg=themelightgrey]#[ignore]%s", tmp);
		free(tmp);
		mti = mode_tree_add(data->data, child, item,
		    window_customize_key_tag(bd, 1), "Command", text, -1);
		mode_tree_draw_as_parent(mti);
		mode_tree_no_tag(mti);
		free(text);

		if (bd->note != NULL) {
			xasprintf(&text, "#[fg=themelightgrey]#[ignore]%s",
			    bd->note);
		} else
			text = xstrdup("");
		mti = mode_tree_add(data->data, child, item,
		    window_customize_key_tag(bd, 2), "Note", text, -1);
		mode_tree_draw_as_parent(mti);
		mode_tree_no_tag(mti);
		free(text);

		if (bd->flags & KEY_BINDING_REPEAT)
			flag = "on";
		else
			flag = "off";
		xasprintf(&text, "#[fg=themelightgrey]#[ignore]%s", flag);
		mti = mode_tree_add(data->data, child, item,
		    window_customize_key_tag(bd, 3), "Repeat", text, -1);
		mode_tree_draw_as_parent(mti);
		mode_tree_no_tag(mti);
		free(text);

		count++;
		bd = key_bindings_next(kt, bd);
	}

	format_free(ft);
	if (data->hide_default && count == 0)
		mode_tree_remove(data->data, top);
}

static void
window_customize_build_environment(struct window_customize_modedata *data,
    const char *title, uint64_t tag, enum window_customize_scope scope,
    struct environ *env, struct format_tree *ft, const char *filter,
    struct cmd_find_state *fs)
{
	struct mode_tree_item		*top;
	struct window_customize_itemdata	*item;
	struct environ_entry		*envent;
	char				*name, *text, *expanded, *value;
	uint64_t			 item_tag;
	int				 global;

	if (data->hide_default)
		return;

	top = mode_tree_add(data->data, NULL, NULL, tag, title, NULL, 0);
	mode_tree_no_tag(top);

	global = (scope == WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT);
	format_add(ft, "is_option", "0");
	format_add(ft, "is_key", "0");
	format_add(ft, "is_environment", "1");
	format_add(ft, "environment_is_global", "%d", global);

	text = window_customize_scope_text(scope, fs);
	format_add(ft, "environment_scope", "%s", text);
	free(text);

	envent = environ_first(env);
	while (envent != NULL) {
		format_add(ft, "environment_name", "%s", envent->name);
		format_add(ft, "environment_hidden", "%d",
		    !!(envent->flags & ENVIRON_HIDDEN));
		format_add(ft, "environment_removed", "%d",
		    envent->value == NULL);
		if (envent->value == NULL)
			value = xstrdup("");
		else
			value = xstrdup(envent->value);
		format_add(ft, "environment_value", "%s", value);

		if (filter != NULL) {
			expanded = format_expand(ft, filter);
			if (!format_true(expanded)) {
				free(expanded);
				free(value);
				envent = environ_next(envent);
				continue;
			}
			free(expanded);
		}

		item = window_customize_add_item(data);
		item->type = WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT;
		item->scope = scope;
		item->environ = env;
		item->environ_flags = envent->flags;
		item->name = xstrdup(envent->name);

		if (envent->value == NULL) {
			xasprintf(&name, "-%s", envent->name);
			text = NULL;
		} else {
			name = xstrdup(envent->name);
			text = format_expand(ft, data->format);
		}
		item_tag = (2ULL << 62)|((uint64_t)(uintptr_t)envent);
		mode_tree_add(data->data, top, item, item_tag, name, text, 0);
		free(name);
		free(text);
		free(value);

		envent = environ_next(envent);
	}
}

static void
window_customize_build(void *modedata,
    __unused struct sort_criteria *sort_crit, __unused uint64_t *tag,
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
	format_add(ft, "is_environment", "0");

	window_customize_build_options(data, "Server Options",
	    (3ULL << 62)|(OPTIONS_TABLE_SERVER << 1)|1,
	    WINDOW_CUSTOMIZE_SERVER, global_options,
	    WINDOW_CUSTOMIZE_NONE, NULL,
	    WINDOW_CUSTOMIZE_NONE, NULL,
	    ft, filter, &fs, WINDOW_CUSTOMIZE_OPTIONS);
	window_customize_build_options(data, "Session Options",
	    (3ULL << 62)|(OPTIONS_TABLE_SESSION << 1)|1,
	    WINDOW_CUSTOMIZE_GLOBAL_SESSION, global_s_options,
	    WINDOW_CUSTOMIZE_SESSION, fs.s->options,
	    WINDOW_CUSTOMIZE_NONE, NULL,
	    ft, filter, &fs, WINDOW_CUSTOMIZE_OPTIONS);
	window_customize_build_options(data, "Window & Pane Options",
	    (3ULL << 62)|(OPTIONS_TABLE_WINDOW << 1)|1,
	    WINDOW_CUSTOMIZE_GLOBAL_WINDOW, global_w_options,
	    WINDOW_CUSTOMIZE_WINDOW, fs.w->options,
	    WINDOW_CUSTOMIZE_PANE, fs.wp->options,
	    ft, filter, &fs, WINDOW_CUSTOMIZE_OPTIONS);
	window_customize_build_options(data, "Session Hooks",
	    (3ULL << 62)|(1ULL << 8)|(OPTIONS_TABLE_SESSION << 1)|1,
	    WINDOW_CUSTOMIZE_GLOBAL_SESSION, global_s_options,
	    WINDOW_CUSTOMIZE_SESSION, fs.s->options,
	    WINDOW_CUSTOMIZE_NONE, NULL,
	    ft, filter, &fs, WINDOW_CUSTOMIZE_HOOKS);
	window_customize_build_options(data, "Window & Pane Hooks",
	    (3ULL << 62)|(1ULL << 8)|(OPTIONS_TABLE_WINDOW << 1)|1,
	    WINDOW_CUSTOMIZE_GLOBAL_WINDOW, global_w_options,
	    WINDOW_CUSTOMIZE_WINDOW, fs.w->options,
	    WINDOW_CUSTOMIZE_PANE, fs.wp->options,
	    ft, filter, &fs, WINDOW_CUSTOMIZE_HOOKS);
	window_customize_build_environment(data, "Global Environment",
	    (3ULL << 62)|(2ULL << 8)|1,
	    WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT, global_environ, ft, filter,
	    &fs);
	window_customize_build_environment(data, "Session Environment",
	    (3ULL << 62)|(2ULL << 8)|3,
	    WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT, fs.s->environ, ft, filter,
	    &fs);

	format_free(ft);
	ft = format_create_from_state(NULL, NULL, &fs);
	format_add(ft, "is_environment", "0");

	kt = key_bindings_first_table();
	while (kt != NULL) {
		if (!RB_EMPTY(&kt->key_bindings))
			window_customize_build_keys(data, kt, ft, filter, &fs);
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
	if (!window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy), 0,
	    "Repeat: ", "%s", (bd->flags & KEY_BINDING_REPEAT) ? "on" : "off"))
		return;
	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy >= cy + sy - 1)
		return;

	cmd = cmd_list_print(bd->cmdlist, 0);
	if (!window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy), 0,
	    "Command: ", "%s", cmd)) {
		free(cmd);
		return;
	}
	default_bd = key_bindings_get_default(kt, bd->key);
	if (default_bd != NULL) {
		default_cmd = cmd_list_print(default_bd->cmdlist, 0);
		if (strcmp(cmd, default_cmd) != 0 &&
		    !window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy),
		    0, "The default is: ", "%s", default_cmd)) {
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
	struct options_entry			 *o, *parent;
	struct options				 *go, *wo;
	const struct options_table_entry	 *oe;
	struct grid_cell			  gc;
	const char				**choice, *text, *name;
	const char				 *array_key;
	const char				 *space = "", *unit = "";
	char					 *value = NULL, *expanded;
	char					 *monitor, label[64];
	char					 *default_value = NULL;
	char					  choices[256] = "";
	struct cmd_find_state			  fs;
	struct format_tree			 *ft;
	int					  is_hook, is_monitor, is_user_hook;

	if (!window_customize_check_item(data, item, &fs))
		return;
	name = item->name;
	array_key = item->array_key;

	o = options_get(item->oo, name);
	if (o == NULL)
		return;
	oe = options_table_entry(o);
	is_hook = (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_HOOK));
	is_monitor = (options_get_monitor_data(o) != NULL);
	is_user_hook = (*name == '@' && hooks_is_event(name));

	if (oe != NULL && oe->unit != NULL) {
		space = " ";
		unit = oe->unit;
	}
	ft = format_create_from_state(NULL, NULL, &fs);

	if (oe == NULL || oe->text == NULL) {
		if (is_monitor)
			text = "This hook runs when a monitor changes.";
		else if (is_user_hook)
			text = "This hook doesn't have a description.";
		else
			text = "This option doesn't have a description.";
	} else
		text = oe->text;
	if (!screen_write_text(ctx, cx, sx, sy, 0, &grid_default_cell, "%s",
	    text))
		goto out;
	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy >= cy + sy - 1)
		goto out;

	if (is_monitor) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "This is a monitor hook."))
			goto out;
		goto monitor;
	}
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
	if (is_user_hook) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "This is a user hook."))
			goto out;
	} else if (is_hook) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "This is a %s hook.", text))
			goto out;
	} else {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "This is a %s option.", text))
			goto out;
	}

monitor:
	monitor = hooks_monitor_to_string(o);
	if (monitor != NULL) {
		if (!window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy),
		    0, "Monitor: ", "%s", monitor)) {
			free(monitor);
			goto out;
		}
		free(monitor);
	}
	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY)) {
		if (is_hook) {
			if (array_key == NULL)
				goto out;
		} else if (array_key != NULL) {
			if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy),
			    0, &grid_default_cell,
			    "This is an array option, key %s.", array_key))
				goto out;
		} else {
			if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy),
			    0, &grid_default_cell, "This is an array option."))
				goto out;
		}
		if (array_key == NULL)
			goto out;
	}
	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy >= cy + sy - 1)
		goto out;

	value = options_to_string(o, array_key, 0);
	if (oe != NULL && array_key == NULL) {
		default_value = options_default_to_string(oe);
		if (strcmp(default_value, value) == 0) {
			free(default_value);
			default_value = NULL;
		}
	}
	if (is_hook || is_monitor || is_user_hook) {
		if (!window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy),
		    0, "Hook command: ", "%s%s%s", value, space, unit))
			goto out;
	} else {
		if (!window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy),
		    0, "Option value: ", "%s%s%s", value, space, unit))
			goto out;
	}
	if (oe == NULL || oe->type == OPTIONS_TABLE_STRING) {
		expanded = format_expand(ft, value);
		if (strcmp(expanded, value) != 0) {
			if (!window_customize_write_value(ctx, cx, sx,
			    sy - (s->cy - cy), 0, "This expands to: ", "%s",
			    expanded)) {
				free(expanded);
				goto out;
			}
		}
		free(expanded);
	}
	if (oe != NULL && oe->type == OPTIONS_TABLE_CHOICE) {
		for (choice = oe->choices; *choice != NULL; choice++) {
			strlcat(choices, *choice, sizeof choices);
			strlcat(choices, ", ", sizeof choices);
		}
		choices[strlen(choices) - 2] = '\0';
		if (!window_customize_write_value(ctx, cx, sx,
		    sy - (s->cy - cy), 0, "Available values are: ", "%s",
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
	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_COLOUR)) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 1,
		    &grid_default_cell, "This is a colour option: "))
			goto out;
		style_apply(&gc, item->oo, name, ft);
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
		if (!window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy),
		    0, "The default is: ", "%s%s%s", default_value, space, unit))
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
			value = options_to_string(parent, NULL, 0);
			xsnprintf(label, sizeof label,
			    "Window value (from window %u): ", fs.wl->idx);
			if (!window_customize_write_value(ctx, s->cx, sx,
			    sy - (s->cy - cy), 0, label, "%s%s%s", value, space,
			    unit))
				goto out;
		}
	}
	if (go != NULL && options_owner(o) != go) {
		parent = options_get_only(go, name);
		if (parent != NULL) {
			value = options_to_string(parent, NULL, 0);
			if (!window_customize_write_value(ctx, s->cx, sx,
			    sy - (s->cy - cy), 0, "Global value: ", "%s%s%s",
			    value, space, unit))
				goto out;
		}
	}

out:
	free(value);
	free(default_value);
	format_free(ft);
}

static void
window_customize_draw_environment(struct window_customize_modedata *data,
    struct window_customize_itemdata *item, struct screen_write_ctx *ctx,
    u_int sx, u_int sy)
{
	struct screen		 *s = ctx->s;
	u_int			  cx = s->cx, cy = s->cy;
	struct environ_entry	 *envent, *parent;
	struct cmd_find_state	  fs;
	const char		 *text;

	if (!window_customize_check_item(data, item, &fs))
		return;
	envent = environ_find(item->environ, item->name);
	if (envent == NULL)
		return;

	if (item->scope == WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT)
		text = "global";
	else
		text = "session";
	if (!screen_write_text(ctx, cx, sx, sy, 0, &grid_default_cell,
	    "This is a %s environment variable.", text))
		return;
	if (envent->flags & ENVIRON_HIDDEN) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "This variable is hidden."))
			return;
	}

	screen_write_cursormove(ctx, cx, s->cy + 1, 0); /* skip line */
	if (s->cy >= cy + sy - 1)
		return;

	if (envent->value == NULL) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "Variable is removed."))
			return;
	} else {
		if (!window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy),
		    0, "Variable value: ", "%s", envent->value))
			return;
	}

	if (item->scope != WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT)
		return;

	parent = environ_find(global_environ, item->name);
	if (parent == NULL)
		return;
	if (parent->value == NULL) {
		if (!screen_write_text(ctx, cx, sx, sy - (s->cy - cy), 0,
		    &grid_default_cell, "Global variable is removed."))
			return;
	} else {
		if (!window_customize_write_value(ctx, cx, sx, sy - (s->cy - cy),
		    0, "Global value: ", "%s", parent->value))
			return;
	}
}

static void
window_customize_draw(void *modedata, void *itemdata,
    struct screen_write_ctx *ctx, u_int sx, u_int sy)
{
	struct window_customize_modedata	*data = modedata;
	struct window_customize_itemdata	*item = itemdata;

	if (item == NULL)
		return;

	if (item->type == WINDOW_CUSTOMIZE_ITEM_KEY)
		window_customize_draw_key(data, item, ctx, sx, sy);
	else if (item->type == WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT)
		window_customize_draw_environment(data, item, ctx, sx, sy);
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

static const char* window_customize_help_lines[] = {
	"#[fg=themelightgrey]"
	"   Enter, s #[#{E:tree-mode-border-style},acs]x#[default] Set %1 value",
	"#[fg=themelightgrey]"
	"          S #[#{E:tree-mode-border-style},acs]x#[default] Set global %1 value",
	"#[fg=themelightgrey]"
	"          w #[#{E:tree-mode-border-style},acs]x#[default] Set window %1 value",
	"#[fg=themelightgrey]"
	"          d #[#{E:tree-mode-border-style},acs]x#[default] Set to default value",
	"#[fg=themelightgrey]"
	"          D #[#{E:tree-mode-border-style},acs]x#[default] Set tagged %1s to default value",
	"#[fg=themelightgrey]"
	"          u #[#{E:tree-mode-border-style},acs]x#[default] Unset an %1",
	"#[fg=themelightgrey]"
	"          U #[#{E:tree-mode-border-style},acs]x#[default] Unset tagged %1s",
	"#[fg=themelightgrey]"
	"          a #[#{E:tree-mode-border-style},acs]x#[default] Change array key",
	"#[fg=themelightgrey]"
	"          e #[#{E:tree-mode-border-style},acs]x#[default] Open %1 value in editor",
	"#[fg=themelightgrey]"
	"          f #[#{E:tree-mode-border-style},acs]x#[default] Enter a filter",
	"#[fg=themelightgrey]"
	"          C #[#{E:tree-mode-border-style},acs]x#[default] Toggle only changed items",
	"#[fg=themelightgrey]"
	"          v #[#{E:tree-mode-border-style},acs]x#[default] Toggle information",
	NULL
};

static const char**
window_customize_help(u_int *width, const char **item)
{
	*width = 52;
	*item = "item";
	return (window_customize_help_lines);
}

static struct screen *
window_customize_init(struct window_mode_entry *wme,
    __unused struct cmdq_item *item, struct cmd_find_state *fs,
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
	if (args_has(args, 'y'))
		data->prompt_flags = PROMPT_ACCEPT;

	data->data = mode_tree_start(wp, args, window_customize_build,
	    window_customize_draw, NULL, window_customize_menu,
	    window_customize_height, NULL, NULL, NULL, window_customize_help,
	    data, window_customize_menu_items, &s);
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
	if (data->editor != NULL) {
		spawn_cancel_editor(data->editor);
		window_customize_finish_edit(data->edit);
	}
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
window_customize_update(struct window_mode_entry *wme)
{
	struct window_customize_modedata	*data = wme->data;

	window_customize_draw_waiting(data);
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

static enum prompt_result
window_customize_set_option_callback(struct client *c, void *itemdata,
    const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	struct options_entry			*o;
	const struct options_table_entry	*oe;
	struct options				*oo = item->oo;
	const char				*name = item->name;
	const char				*array_key = item->array_key;
	char					*cause;
	u_int					 idx;
	char					 keybuf[32];

	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);
	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return (PROMPT_CLOSE);
	o = options_get(oo, name);
	if (o == NULL)
		return (PROMPT_CLOSE);
	oe = options_table_entry(o);

	if (oe != NULL && (oe->flags & OPTIONS_TABLE_IS_ARRAY)) {
		if (array_key == NULL) {
			for (idx = 0; idx < INT_MAX; idx++) {
				if (options_array_getv(o, "%u", idx) == NULL)
					break;
			}
			xsnprintf(keybuf, sizeof keybuf, "%u", idx);
			array_key = keybuf;
		}
		if (options_array_set(o, array_key, s, 0, &cause) != 0)
			goto fail;
	} else {
		if (options_from_string(oo, oe, name, s, 0, &cause) != 0)
			goto fail;
	}
	if (item->option_type == WINDOW_CUSTOMIZE_HOOKS && *name == '@')
		hooks_add_event(name);

	options_push_changes(item->name);
	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (PROMPT_CLOSE);

fail:
	*cause = toupper((u_char)*cause);
	status_message_set(c, -1, 1, 0, 0, "%s", cause);
	free(cause);
	return (PROMPT_CLOSE);
}

static enum prompt_result
window_customize_set_environment_callback(__unused struct client *c,
    void *itemdata, const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	struct environ_entry		*envent;
	int				 flags;

	if (s == NULL || data->dead)
		return (PROMPT_CLOSE);
	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return (PROMPT_CLOSE);

	flags = item->environ_flags;
	envent = environ_find(item->environ, item->name);
	if (envent != NULL)
		flags = envent->flags;
	environ_set(item->environ, item->name, flags, "%s", s);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (PROMPT_CLOSE);
}

static void
window_customize_set_environment(struct client *c,
    struct window_customize_modedata *data,
    struct window_customize_itemdata *item, int global)
{
	struct window_customize_itemdata	*new_item;
	struct environ_entry		*envent;
	struct environ			*env;
	enum window_customize_scope	 scope;
	struct cmd_find_state		 fs;
	const char			*space = "";
	char				*prompt, *text;

	if (item == NULL || !window_customize_check_item(data, item, &fs))
		return;
	envent = environ_find(item->environ, item->name);
	if (envent == NULL)
		return;

	if (global) {
		scope = WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT;
		env = global_environ;
	} else {
		scope = item->scope;
		env = item->environ;
	}

	text = window_customize_scope_text(scope, &fs);
	if (*text != '\0')
		space = ", for ";
	else if (scope == WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT)
		space = ", global";
	xasprintf(&prompt, "(%s%s%s) ", item->name, space, text);
	free(text);

	new_item = xcalloc(1, sizeof *new_item);
	new_item->data = data;
	new_item->type = WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT;
	new_item->scope = scope;
	new_item->environ = env;
	new_item->environ_flags = envent->flags;
	new_item->name = xstrdup(item->name);

	data->references++;
	mode_tree_set_prompt(data->data, c, prompt,
	    envent->value == NULL ? "" : envent->value, PROMPT_TYPE_COMMAND,
	    PROMPT_NOFORMAT, window_customize_set_environment_callback,
	    window_customize_free_item_callback, new_item);

	free(prompt);
}

static enum prompt_result
window_customize_add_option_callback(struct client *c, void *itemdata,
    const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	char				*copy, *name, *array_key;
	const char			*value;
	const char			*what;
	int				 ambiguous;
	size_t				 namelen;

	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);
	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return (PROMPT_CLOSE);

	namelen = strcspn(s, " \t");
	if (namelen == 0 || s[namelen] == '\0') {
		status_message_set(c, -1, 1, 0, 0,
		    "User option must be @name value");
		return (PROMPT_CLOSE);
	}

	value = s + namelen;
	while (*value == ' ' || *value == '\t')
		value++;
	if (*value == '\0') {
		status_message_set(c, -1, 1, 0, 0,
		    "User option must be @name value");
		return (PROMPT_CLOSE);
	}

	copy = xstrndup(s, namelen);
	name = options_match(copy, &array_key, &ambiguous);
	free(copy);
	if (name == NULL || *name != '@' || array_key != NULL) {
		what = (item->option_type == WINDOW_CUSTOMIZE_HOOKS) ?
		    "hook" : "option";
		status_message_set(c, -1, 1, 0, 0,
		    "User %s name must start with @", what);
		free(name);
		free(array_key);
		return (PROMPT_CLOSE);
	}

	options_set_string(item->oo, name, 0, "%s", value);
	if (item->option_type == WINDOW_CUSTOMIZE_HOOKS)
		hooks_add_event(name);
	options_push_changes(name);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	free(name);
	free(array_key);
	return (PROMPT_CLOSE);
}

static void
window_customize_add_option(struct client *c,
    struct window_customize_modedata *data, enum window_customize_scope scope,
    struct options *oo, enum window_customize_option_type type)
{
	struct window_customize_itemdata	*new_item;
	char				*prompt;
	const char			*what;

	what = (type == WINDOW_CUSTOMIZE_HOOKS) ? "hook" : "option";
	xasprintf(&prompt, "New user %s: ", what);

	new_item = xcalloc(1, sizeof *new_item);
	new_item->data = data;
	new_item->type = WINDOW_CUSTOMIZE_ITEM_OPTION;
	new_item->option_type = type;
	new_item->scope = scope;
	new_item->oo = oo;

	data->references++;
	mode_tree_set_prompt(data->data, c, prompt, "@", PROMPT_TYPE_COMMAND,
	    PROMPT_NOFORMAT, window_customize_add_option_callback,
	    window_customize_free_item_callback, new_item);

	free(prompt);
}

static enum prompt_result
window_customize_add_environment_callback(struct client *c, void *itemdata,
    const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	char				*name;
	const char			*value;

	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);
	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return (PROMPT_CLOSE);

	if (*s == '-') {
		if (s[1] == '\0' || strchr(s + 1, '=') != NULL) {
			status_message_set(c, -1, 1, 0, 0,
			    "Bad environment variable: %s", s);
			return (PROMPT_CLOSE);
		}
		environ_clear(item->environ, s + 1);
	} else {
		value = strchr(s, '=');
		if (value == NULL || value == s) {
			status_message_set(c, -1, 1, 0, 0,
			    "Environment variable must be NAME=value");
			return (PROMPT_CLOSE);
		}

		name = xstrdup(s);
		name[strcspn(name, "=")] = '\0';
		environ_set(item->environ, name, 0, "%s", value + 1);
		free(name);
	}

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (PROMPT_CLOSE);
}

static void
window_customize_add_environment(struct client *c,
    struct window_customize_modedata *data, enum window_customize_scope scope,
    struct environ *env)
{
	struct window_customize_itemdata	*new_item;

	new_item = xcalloc(1, sizeof *new_item);
	new_item->data = data;
	new_item->type = WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT;
	new_item->scope = scope;
	new_item->environ = env;

	data->references++;
	mode_tree_set_prompt(data->data, c, "New environment: ", "",
	    PROMPT_TYPE_COMMAND, PROMPT_NOFORMAT,
	    window_customize_add_environment_callback,
	    window_customize_free_item_callback, new_item);
}

static void
window_customize_edit_close_cb(char *buf, size_t len, void *arg)
{
	struct window_customize_editdata		*ed = arg;
	struct window_customize_itemdata		*item = ed->item;
	struct window_pane			*wp;
	struct window_mode_entry		*wme;
	struct window_customize_modedata	*data = NULL;
	char					*value, *cause = NULL;

	wp = window_pane_find_by_id(ed->wp_id);
	if (wp != NULL) {
		wme = TAILQ_FIRST(&wp->modes);
		if (wme != NULL && wme->mode == &window_customize_mode) {
			data = wme->data;
			if (data->editor == ed->editor) {
				data->editor = NULL;
				data->edit = NULL;
			}
		}
	}

	if (buf == NULL || len == 0 || data == NULL || data->dead) {
		free(buf);
		window_customize_finish_edit(ed);
		return;
	}
	if (buf[len - 1] == '\n')
		len--;
	value = xmalloc(len + 1);
	memcpy(value, buf, len);
	value[len] = '\0';
	free(buf);

	switch (ed->edit_type) {
	case WINDOW_CUSTOMIZE_EDIT_OPTION:
		if (window_customize_option_editable(data, item) &&
		    window_customize_set_option_value(item, value, &cause) != 0) {
			free(cause);
			goto out;
		}
		break;
	case WINDOW_CUSTOMIZE_EDIT_KEY_COMMAND:
		if (window_customize_set_command_value(item, value, &cause) != 0) {
			free(cause);
			goto out;
		}
		break;
	case WINDOW_CUSTOMIZE_EDIT_KEY_NOTE:
		if (window_customize_set_note_value(item, value) != 0)
			goto out;
		break;
	case WINDOW_CUSTOMIZE_EDIT_ENVIRONMENT:
		if (!window_customize_check_item(data, item, NULL))
			goto out;
		window_customize_set_environment_value(item, value);
		break;
	}

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	wp->flags |= PANE_REDRAW;

out:
	free(value);
	window_customize_finish_edit(ed);
}

static void
window_customize_start_edit(struct window_customize_modedata *data,
    struct window_customize_itemdata *item, struct client *c)
{
	struct window_customize_editdata	*ed;
	struct options_entry		*o;
	struct environ_entry		*envent;
	struct key_binding		*bd;
	const char			*name;
	const char			*buf;
	char				*value;
	size_t				 len;
	enum window_customize_edit_type	 edit_type;

	if (data->editor != NULL || item == NULL)
		return;

	if (item->type == WINDOW_CUSTOMIZE_ITEM_OPTION) {
		if (!window_customize_option_editable(data, item))
			return;
		o = options_get(item->oo, item->name);
		if (o == NULL)
			return;
		value = options_to_string(o, item->array_key, 0);
		edit_type = WINDOW_CUSTOMIZE_EDIT_OPTION;
	} else if (item->type == WINDOW_CUSTOMIZE_ITEM_KEY) {
		name = mode_tree_get_current_name(data->data);
		if (!window_customize_get_key(item, NULL, &bd))
			return;
		if (strcmp(name, "Command") == 0) {
			value = cmd_list_print(bd->cmdlist, 0);
			edit_type = WINDOW_CUSTOMIZE_EDIT_KEY_COMMAND;
		} else if (strcmp(name, "Note") == 0) {
			if (bd->note == NULL)
				value = xstrdup("");
			else
				value = xstrdup(bd->note);
			edit_type = WINDOW_CUSTOMIZE_EDIT_KEY_NOTE;
		} else
			return;
	} else if (item->type == WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT) {
		if (!window_customize_check_item(data, item, NULL))
			return;
		envent = environ_find(item->environ, item->name);
		if (envent == NULL || envent->value == NULL)
			return;
		value = xstrdup(envent->value);
		edit_type = WINDOW_CUSTOMIZE_EDIT_ENVIRONMENT;
	} else
		return;

	ed = xcalloc(1, sizeof *ed);
	ed->wp_id = data->wp->id;
	ed->edit_type = edit_type;
	ed->item = window_customize_copy_item(item);

	buf = value;
	len = strlen(value);
	if (len == 0) {
		buf = "\n";
		len = 1;
	}
	ed->editor = spawn_editor(c, buf, len, window_customize_edit_close_cb,
	    ed);
	free(value);
	if (ed->editor == NULL)
		window_customize_finish_edit(ed);
	else {
		data->editor = ed->editor;
		data->edit = ed;
	}
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
	int					 flag;
	enum window_customize_scope		 scope = WINDOW_CUSTOMIZE_NONE;
	u_int					 choice;
	const char				*name = item->name, *space = "";
	const char				*array_key = item->array_key;
	char					*prompt, *value, *text;
	struct cmd_find_state			 fs;

	if (item == NULL || !window_customize_check_item(data, item, &fs))
		return;
	o = options_get(item->oo, name);
	if (o == NULL)
		return;

	oe = options_table_entry(o);
	if (oe != NULL && ~oe->scope & OPTIONS_TABLE_PANE)
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
			case WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT:
			case WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT:
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
			case WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT:
			case WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT:
				scope = item->scope;
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
			if (array_key == NULL) {
				xasprintf(&prompt, "(%s[+]%s%s) ", name, space,
				    text);
			} else {
				xasprintf(&prompt, "(%s[%s]%s%s) ", name,
				    array_key, space, text);
			}
		} else
			xasprintf(&prompt, "(%s%s%s) ", name, space, text);
		free(text);

		value = options_to_string(o, array_key, 0);

		new_item = xcalloc(1, sizeof *new_item);
		new_item->data = data;
		new_item->type = WINDOW_CUSTOMIZE_ITEM_OPTION;
		new_item->option_type = item->option_type;
		new_item->scope = scope;
		new_item->oo = oo;
		new_item->name = xstrdup(name);
		if (array_key != NULL)
			new_item->array_key = xstrdup(array_key);

		data->references++;
		mode_tree_set_prompt(data->data, c, prompt, value,
		    PROMPT_TYPE_COMMAND, PROMPT_NOFORMAT,
		    window_customize_set_option_callback,
		    window_customize_free_item_callback, new_item);

		free(prompt);
		free(value);
	}
}

static enum prompt_result
window_customize_set_array_key_callback(struct client *c, void *itemdata,
    const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data;
	struct options_entry			*o;
	const char				*name, *array_key;
	char					*value, *cause;

	if (item == NULL)
		return (PROMPT_CLOSE);
	data = item->data;
	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);
	name = item->name;
	array_key = item->array_key;
	if (array_key == NULL || !window_customize_check_item(data, item, NULL))
		return (PROMPT_CLOSE);

	o = options_get(item->oo, name);
	if (o == NULL)
		return (PROMPT_CLOSE);
	if (options_array_get(o, s) != NULL)
		return (PROMPT_CLOSE);

	value = options_to_string(o, array_key, 0);
	if (options_array_set(o, s, value, 0, &cause) != 0)
		goto fail;
	free(value);

	options_array_set(o, array_key, NULL, 0, NULL);
	options_push_changes(item->name);
	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (PROMPT_CLOSE);

fail:
	free(value);
	*cause = toupper((u_char)*cause);
	status_message_set(c, -1, 1, 0, 0, "%s", cause);
	free(cause);
	return (PROMPT_CLOSE);
}

static void
window_customize_set_array_key(struct client *c,
    struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	struct window_customize_itemdata	*new_item;
	char				*prompt;

	if (item == NULL ||
	    item->array_key == NULL ||
	    !window_customize_check_item(data, item, NULL))
		return;

	xasprintf(&prompt, "(%s[%s]) ", item->name, item->array_key);

	new_item = xcalloc(1, sizeof *new_item);
	new_item->data = data;
	new_item->type = WINDOW_CUSTOMIZE_ITEM_OPTION;
	new_item->option_type = item->option_type;
	new_item->scope = item->scope;
	new_item->oo = item->oo;
	new_item->name = xstrdup(item->name);
	new_item->array_key = xstrdup(item->array_key);

	data->references++;
	mode_tree_set_prompt(data->data, c, prompt, item->array_key,
	    PROMPT_TYPE_COMMAND, PROMPT_NOFORMAT,
	    window_customize_set_array_key_callback,
	    window_customize_free_item_callback, new_item);

	free(prompt);
}

static void
window_customize_unset_environment(struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return;
	if (environ_find(item->environ, item->name) == NULL)
		return;
	if (item == mode_tree_get_current(data->data))
		mode_tree_up(data->data, 0);
	environ_unset(item->environ, item->name);
}

static void
window_customize_unset_option(struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	struct options_entry	*o;

	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return;

	o = options_get(item->oo, item->name);
	if (o == NULL)
		return;
	if (item->array_key != NULL &&
	    item == mode_tree_get_current(data->data))
		mode_tree_up(data->data, 0);
	options_remove_or_default(o, item->array_key, NULL);
}

static void
window_customize_reset_option(struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	struct options		*oo;
	struct options_entry	*o;

	if (item == NULL || !window_customize_check_item(data, item, NULL))
		return;
	if (item->array_key != NULL)
		return;

	oo = item->oo;
	while (oo != NULL) {
		o = options_get_only(oo, item->name);
		if (o != NULL)
			options_remove_or_default(o, NULL, NULL);
		oo = options_get_parent(oo);
	}
}

static enum prompt_result
window_customize_set_command_callback(struct client *c, void *itemdata,
    const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	struct key_binding			*bd;
	struct cmd_parse_result			*pr;
	char					*error;

	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);
	if (item == NULL || !window_customize_get_key(item, NULL, &bd))
		return (PROMPT_CLOSE);

	pr = cmd_parse_from_string(s, NULL);
	switch (pr->status) {
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

	return (PROMPT_CLOSE);

fail:
	*error = toupper((u_char)*error);
	status_message_set(c, -1, 1, 0, 0, "%s", error);
	free(error);
	return (PROMPT_CLOSE);
}

static enum prompt_result
window_customize_set_note_callback(__unused struct client *c, void *itemdata,
    const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	struct key_binding			*bd;

	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);
	if (item == NULL || !window_customize_get_key(item, NULL, &bd))
		return (PROMPT_CLOSE);

	free((void *)bd->note);
	bd->note = xstrdup(s);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (PROMPT_CLOSE);
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
		new_item->type = WINDOW_CUSTOMIZE_ITEM_KEY;
		new_item->scope = item->scope;
		new_item->table = xstrdup(item->table);
		new_item->key = key;

		data->references++;
		mode_tree_set_prompt(data->data, c, prompt, value,
		    PROMPT_TYPE_COMMAND, PROMPT_NOFORMAT,
		    window_customize_set_command_callback,
		    window_customize_free_item_callback, new_item);
		free(prompt);
		free(value);
	} else if (strcmp(s, "Note") == 0) {
		xasprintf(&prompt, "(%s) ", key_string_lookup_key(key, 0));

		new_item = xcalloc(1, sizeof *new_item);
		new_item->data = data;
		new_item->type = WINDOW_CUSTOMIZE_ITEM_KEY;
		new_item->scope = item->scope;
		new_item->table = xstrdup(item->table);
		new_item->key = key;

		data->references++;
		mode_tree_set_prompt(data->data, c, prompt,
		    (bd->note == NULL ? "" : bd->note),
		    PROMPT_TYPE_COMMAND, PROMPT_NOFORMAT,
		    window_customize_set_note_callback,
		    window_customize_free_item_callback, new_item);
		free(prompt);
	}
}

static enum prompt_result
window_customize_add_key_callback(struct client *c, void *itemdata,
    const char *s, __unused enum prompt_key_result key0)
{
	struct window_customize_itemdata	*item = itemdata;
	struct window_customize_modedata	*data = item->data;
	key_code			 key;
	struct cmd_parse_result		*pr;
	const char			*command;
	char				*error, *keystr;
	size_t				 keylen;

	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);

	keylen = strcspn(s, " \t");
	if (keylen == 0 || s[keylen] == '\0') {
		status_message_set(c, -1, 1, 0, 0,
		    "Key binding must be key command");
		return (PROMPT_CLOSE);
	}

	command = s + keylen;
	while (*command == ' ' || *command == '\t')
		command++;
	if (*command == '\0') {
		status_message_set(c, -1, 1, 0, 0,
		    "Key binding must be key command");
		return (PROMPT_CLOSE);
	}

	keystr = xstrndup(s, keylen);
	key = key_string_lookup_string(keystr);
	if (key == KEYC_NONE || key == KEYC_UNKNOWN) {
		status_message_set(c, -1, 1, 0, 0, "Unknown key: %s", keystr);
		free(keystr);
		return (PROMPT_CLOSE);
	}
	free(keystr);

	pr = cmd_parse_from_string(command, NULL);
	switch (pr->status) {
	case CMD_PARSE_ERROR:
		error = pr->error;
		goto fail;
	case CMD_PARSE_SUCCESS:
		break;
	}
	key_bindings_add(item->table, key, NULL, 0, pr->cmdlist);

	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (PROMPT_CLOSE);

fail:
	*error = toupper((u_char)*error);
	status_message_set(c, -1, 1, 0, 0, "%s", error);
	free(error);
	return (PROMPT_CLOSE);
}

static void
window_customize_add_key(struct client *c,
    struct window_customize_modedata *data, const char *table)
{
	struct window_customize_itemdata	*new_item;
	char				*prompt;

	xasprintf(&prompt, "New key in %s: ", table);

	new_item = xcalloc(1, sizeof *new_item);
	new_item->data = data;
	new_item->type = WINDOW_CUSTOMIZE_ITEM_KEY;
	new_item->scope = WINDOW_CUSTOMIZE_KEY;
	new_item->table = xstrdup(table);

	data->references++;
	mode_tree_set_prompt(data->data, c, prompt, "", PROMPT_TYPE_COMMAND,
	    PROMPT_NOFORMAT, window_customize_add_key_callback,
	    window_customize_free_item_callback, new_item);

	free(prompt);
}

static void
window_customize_unset_key(struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	struct key_table	*kt;
	struct key_binding	*bd;

	if (item == NULL || !window_customize_get_key(item, &kt, &bd))
		return;

	if (item == mode_tree_get_current(data->data))
		mode_tree_up(data->data, 0);
	key_bindings_remove(kt->name, bd->key);
}

static void
window_customize_reset_key(struct window_customize_modedata *data,
    struct window_customize_itemdata *item)
{
	struct key_table	*kt;
	struct key_binding	*dd, *bd;

	if (item == NULL || !window_customize_get_key(item, &kt, &bd))
		return;

	dd = key_bindings_get_default(kt, bd->key);
	if (dd != NULL && bd->cmdlist == dd->cmdlist)
		return;
	if (dd == NULL && item == mode_tree_get_current(data->data))
		mode_tree_up(data->data, 0);
	key_bindings_reset(kt->name, bd->key);
}

static void
window_customize_change_each(void *modedata, void *itemdata,
    __unused struct client *c, __unused key_code key)
{
	struct window_customize_modedata	*data = modedata;
	struct window_customize_itemdata	*item = itemdata;
	enum window_customize_item_type	 type = item->type;
	char				*name = NULL;

	if (type == WINDOW_CUSTOMIZE_ITEM_OPTION)
		name = xstrdup(item->name);
	switch (data->change) {
	case WINDOW_CUSTOMIZE_UNSET:
		if (type == WINDOW_CUSTOMIZE_ITEM_KEY)
			window_customize_unset_key(data, item);
		else if (type == WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT)
			window_customize_unset_environment(data, item);
		else
			window_customize_unset_option(data, item);
		break;
	case WINDOW_CUSTOMIZE_RESET:
		if (type == WINDOW_CUSTOMIZE_ITEM_KEY)
			window_customize_reset_key(data, item);
		else if (type == WINDOW_CUSTOMIZE_ITEM_OPTION)
			window_customize_reset_option(data, item);
		break;
	}
	if (type == WINDOW_CUSTOMIZE_ITEM_OPTION)
		options_push_changes(name);
	free(name);
}

static enum prompt_result
window_customize_change_current_callback(__unused struct client *c,
    void *modedata, const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_modedata	*data = modedata;
	struct window_customize_itemdata	*item;
	enum window_customize_item_type	 type;
	char				*name = NULL;

	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);
	if (tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		return (PROMPT_CLOSE);

	item = mode_tree_get_current(data->data);
	if (item == NULL)
		return (PROMPT_CLOSE);
	type = item->type;
	if (type == WINDOW_CUSTOMIZE_ITEM_OPTION)
		name = xstrdup(item->name);
	switch (data->change) {
	case WINDOW_CUSTOMIZE_UNSET:
		if (type == WINDOW_CUSTOMIZE_ITEM_KEY)
			window_customize_unset_key(data, item);
		else if (type == WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT)
			window_customize_unset_environment(data, item);
		else
			window_customize_unset_option(data, item);
		break;
	case WINDOW_CUSTOMIZE_RESET:
		if (type == WINDOW_CUSTOMIZE_ITEM_KEY)
			window_customize_reset_key(data, item);
		else if (type == WINDOW_CUSTOMIZE_ITEM_OPTION)
			window_customize_reset_option(data, item);
		break;
	}
	if (type == WINDOW_CUSTOMIZE_ITEM_OPTION)
		options_push_changes(name);
	free(name);
	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (PROMPT_CLOSE);
}

static enum prompt_result
window_customize_change_tagged_callback(struct client *c, void *modedata,
    const char *s, __unused enum prompt_key_result key)
{
	struct window_customize_modedata	*data = modedata;

	if (s == NULL || *s == '\0' || data->dead)
		return (PROMPT_CLOSE);
	if (tolower((u_char) s[0]) != 'y' || s[1] != '\0')
		return (PROMPT_CLOSE);

	mode_tree_each_tagged(data->data, window_customize_change_each, c,
	    KEYC_NONE, 0);
	mode_tree_build(data->data);
	mode_tree_draw(data->data);
	data->wp->flags |= PANE_REDRAW;

	return (PROMPT_CLOSE);
}

static int
window_customize_add_current(struct client *c,
    struct window_customize_modedata *data)
{
	struct cmd_find_state	 fs;
	const char		*name, *table;

	name = mode_tree_get_current_name(data->data);
	if (cmd_find_valid_state(&data->fs))
		cmd_find_copy_state(&fs, &data->fs);
	else
		cmd_find_from_pane(&fs, data->wp, 0);

	if (strcmp(name, "Server Options") == 0) {
		window_customize_add_option(c, data, WINDOW_CUSTOMIZE_SERVER,
		    global_options, WINDOW_CUSTOMIZE_OPTIONS);
		return (1);
	}
	if (strcmp(name, "Session Options") == 0) {
		window_customize_add_option(c, data, WINDOW_CUSTOMIZE_SESSION,
		    fs.s->options, WINDOW_CUSTOMIZE_OPTIONS);
		return (1);
	}
	if (strcmp(name, "Window & Pane Options") == 0) {
		window_customize_add_option(c, data, WINDOW_CUSTOMIZE_PANE,
		    fs.wp->options, WINDOW_CUSTOMIZE_OPTIONS);
		return (1);
	}
	if (strcmp(name, "Session Hooks") == 0) {
		window_customize_add_option(c, data, WINDOW_CUSTOMIZE_SESSION,
		    fs.s->options, WINDOW_CUSTOMIZE_HOOKS);
		return (1);
	}
	if (strcmp(name, "Window & Pane Hooks") == 0) {
		window_customize_add_option(c, data, WINDOW_CUSTOMIZE_PANE,
		    fs.wp->options, WINDOW_CUSTOMIZE_HOOKS);
		return (1);
	}
	if (strcmp(name, "Global Environment") == 0) {
		window_customize_add_environment(c, data,
		    WINDOW_CUSTOMIZE_GLOBAL_ENVIRONMENT, global_environ);
		return (1);
	}
	if (strcmp(name, "Session Environment") == 0) {
		window_customize_add_environment(c, data,
		    WINDOW_CUSTOMIZE_SESSION_ENVIRONMENT, fs.s->environ);
		return (1);
	}

	if (strncmp(name, "Key Table - ", 12) == 0) {
		table = name + 12;
		window_customize_add_key(c, data, table);
		return (1);
	}
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
	if (data->editor != NULL) {
		if (key == 'q' || key == '\033' || key == '\003')
			finished = 1;
		else
			finished = 0;
		goto out;
	}

	finished = mode_tree_key(data->data, c, &key, m, NULL, NULL);
	if (item != (new_item = mode_tree_get_current(data->data)))
		item = new_item;

	switch (key) {
	case 'e':
		window_customize_start_edit(data, item, c);
		break;
	case 'a':
		if (item == NULL || item->type != WINDOW_CUSTOMIZE_ITEM_OPTION)
			break;
		window_customize_set_array_key(c, data, item);
		break;
	case '\r':
	case 's':
		if (item == NULL) {
			if (window_customize_add_current(c, data))
				mode_tree_build(data->data);
			break;
		}
		if (item->type == WINDOW_CUSTOMIZE_ITEM_KEY)
			window_customize_set_key(c, data, item);
		else if (item->type == WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT)
			window_customize_set_environment(c, data, item, 0);
		else {
			window_customize_set_option(c, data, item, 0, 1);
			options_push_changes(item->name);
		}
		mode_tree_build(data->data);
		break;
	case 'w':
		if (item == NULL || item->type != WINDOW_CUSTOMIZE_ITEM_OPTION)
			break;
		window_customize_set_option(c, data, item, 0, 0);
		options_push_changes(item->name);
		mode_tree_build(data->data);
		break;
	case 'S':
	case 'W':
		if (item == NULL || item->type == WINDOW_CUSTOMIZE_ITEM_KEY)
			break;
		if (item->type == WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT)
			window_customize_set_environment(c, data, item, 1);
		else {
			window_customize_set_option(c, data, item, 1, 0);
			options_push_changes(item->name);
		}
		mode_tree_build(data->data);
		break;
	case 'd':
		if (item == NULL ||
		    (item->type == WINDOW_CUSTOMIZE_ITEM_OPTION &&
		    item->array_key != NULL) ||
		    item->type == WINDOW_CUSTOMIZE_ITEM_ENVIRONMENT)
			break;
		xasprintf(&prompt, "Reset %s to default? ", item->name);
		data->references++;
		data->change = WINDOW_CUSTOMIZE_RESET;
		mode_tree_set_prompt(data->data, c, prompt, "",
		    PROMPT_TYPE_COMMAND,
		    PROMPT_SINGLE|PROMPT_NOFORMAT|data->prompt_flags,
		    window_customize_change_current_callback,
		    window_customize_free_callback, data);
		free(prompt);
		break;
	case 'D':
		tagged = mode_tree_count_tagged(data->data);
		if (tagged == 0)
			break;
		xasprintf(&prompt, "Reset %u tagged to default? ", tagged);
		data->references++;
		data->change = WINDOW_CUSTOMIZE_RESET;
		mode_tree_set_prompt(data->data, c, prompt, "",
		    PROMPT_TYPE_COMMAND,
		    PROMPT_SINGLE|PROMPT_NOFORMAT|data->prompt_flags,
		    window_customize_change_tagged_callback,
		    window_customize_free_callback, data);
		free(prompt);
		break;
	case 'u':
		if (item == NULL)
			break;
		if (item->array_key != NULL) {
			xasprintf(&prompt, "Unset %s[%s]? ", item->name,
			    item->array_key);
		} else
			xasprintf(&prompt, "Unset %s? ", item->name);
		data->references++;
		data->change = WINDOW_CUSTOMIZE_UNSET;
		mode_tree_set_prompt(data->data, c, prompt, "",
		    PROMPT_TYPE_COMMAND,
		    PROMPT_SINGLE|PROMPT_NOFORMAT|data->prompt_flags,
		    window_customize_change_current_callback,
		    window_customize_free_callback, data);
		free(prompt);
		break;
	case 'U':
		tagged = mode_tree_count_tagged(data->data);
		if (tagged == 0)
			break;
		xasprintf(&prompt, "Unset %u tagged? ", tagged);
		data->references++;
		data->change = WINDOW_CUSTOMIZE_UNSET;
		mode_tree_set_prompt(data->data, c, prompt, "",
		    PROMPT_TYPE_COMMAND,
		    PROMPT_SINGLE|PROMPT_NOFORMAT|data->prompt_flags,
		    window_customize_change_tagged_callback,
		    window_customize_free_callback, data);
		free(prompt);
		break;
	case 'H':
		data->hide_global = !data->hide_global;
		mode_tree_build(data->data);
		break;
	case 'C':
		data->hide_default = !data->hide_default;
		mode_tree_build(data->data);
		break;
	}

out:
	if (finished)
		window_pane_reset_mode(wp);
	else {
		mode_tree_draw(data->data);
		window_customize_draw_waiting(data);
		wp->flags |= PANE_REDRAW;
	}
}
