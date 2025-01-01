/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <fnmatch.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/*
 * Option handling; each option has a name, type and value and is stored in
 * a red-black tree.
 */

struct options_array_item {
	u_int				 index;
	union options_value		 value;
	RB_ENTRY(options_array_item)	 entry;
};
static int
options_array_cmp(struct options_array_item *a1, struct options_array_item *a2)
{
	if (a1->index < a2->index)
		return (-1);
	if (a1->index > a2->index)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(options_array, options_array_item, entry, options_array_cmp);

struct options_entry {
	struct options				*owner;

	const char				*name;
	const struct options_table_entry	*tableentry;
	union options_value			 value;

	int					 cached;
	struct style				 style;

	RB_ENTRY(options_entry)			 entry;
};

struct options {
	RB_HEAD(options_tree, options_entry)	 tree;
	struct options				*parent;
};

static struct options_entry	*options_add(struct options *, const char *);
static void			 options_remove(struct options_entry *);

#define OPTIONS_IS_STRING(o)						\
	((o)->tableentry == NULL ||					\
	    (o)->tableentry->type == OPTIONS_TABLE_STRING)
#define OPTIONS_IS_NUMBER(o) \
	((o)->tableentry != NULL &&					\
	    ((o)->tableentry->type == OPTIONS_TABLE_NUMBER ||		\
	    (o)->tableentry->type == OPTIONS_TABLE_KEY ||		\
	    (o)->tableentry->type == OPTIONS_TABLE_COLOUR ||		\
	    (o)->tableentry->type == OPTIONS_TABLE_FLAG ||		\
	    (o)->tableentry->type == OPTIONS_TABLE_CHOICE))
#define OPTIONS_IS_COMMAND(o) \
	((o)->tableentry != NULL &&					\
	    (o)->tableentry->type == OPTIONS_TABLE_COMMAND)

#define OPTIONS_IS_ARRAY(o)						\
	((o)->tableentry != NULL &&					\
	    ((o)->tableentry->flags & OPTIONS_TABLE_IS_ARRAY))

static int	options_cmp(struct options_entry *, struct options_entry *);
RB_GENERATE_STATIC(options_tree, options_entry, entry, options_cmp);

static int
options_cmp(struct options_entry *lhs, struct options_entry *rhs)
{
	return (strcmp(lhs->name, rhs->name));
}

static const char *
options_map_name(const char *name)
{
	const struct options_name_map	*map;

	for (map = options_other_names; map->from != NULL; map++) {
		if (strcmp(map->from, name) == 0)
			return (map->to);
	}
	return (name);
}

static const struct options_table_entry *
options_parent_table_entry(struct options *oo, const char *s)
{
	struct options_entry	*o;

	if (oo->parent == NULL)
		fatalx("no parent options for %s", s);
	o = options_get(oo->parent, s);
	if (o == NULL)
		fatalx("%s not in parent options", s);
	return (o->tableentry);
}

static void
options_value_free(struct options_entry *o, union options_value *ov)
{
	if (OPTIONS_IS_STRING(o))
		free(ov->string);
	if (OPTIONS_IS_COMMAND(o) && ov->cmdlist != NULL)
		cmd_list_free(ov->cmdlist);
}

static char *
options_value_to_string(struct options_entry *o, union options_value *ov,
    int numeric)
{
	char	*s;

	if (OPTIONS_IS_COMMAND(o))
		return (cmd_list_print(ov->cmdlist, 0));
	if (OPTIONS_IS_NUMBER(o)) {
		switch (o->tableentry->type) {
		case OPTIONS_TABLE_NUMBER:
			xasprintf(&s, "%lld", ov->number);
			break;
		case OPTIONS_TABLE_KEY:
			s = xstrdup(key_string_lookup_key(ov->number, 0));
			break;
		case OPTIONS_TABLE_COLOUR:
			s = xstrdup(colour_tostring(ov->number));
			break;
		case OPTIONS_TABLE_FLAG:
			if (numeric)
				xasprintf(&s, "%lld", ov->number);
			else
				s = xstrdup(ov->number ? "on" : "off");
			break;
		case OPTIONS_TABLE_CHOICE:
			s = xstrdup(o->tableentry->choices[ov->number]);
			break;
		default:
			fatalx("not a number option type");
		}
		return (s);
	}
	if (OPTIONS_IS_STRING(o))
		return (xstrdup(ov->string));
	return (xstrdup(""));
}

struct options *
options_create(struct options *parent)
{
	struct options	*oo;

	oo = xcalloc(1, sizeof *oo);
	RB_INIT(&oo->tree);
	oo->parent = parent;
	return (oo);
}

void
options_free(struct options *oo)
{
	struct options_entry	*o, *tmp;

	RB_FOREACH_SAFE(o, options_tree, &oo->tree, tmp)
		options_remove(o);
	free(oo);
}

struct options *
options_get_parent(struct options *oo)
{
	return (oo->parent);
}

void
options_set_parent(struct options *oo, struct options *parent)
{
	oo->parent = parent;
}

struct options_entry *
options_first(struct options *oo)
{
	return (RB_MIN(options_tree, &oo->tree));
}

struct options_entry *
options_next(struct options_entry *o)
{
	return (RB_NEXT(options_tree, &oo->tree, o));
}

struct options_entry *
options_get_only(struct options *oo, const char *name)
{
	struct options_entry	o = { .name = name }, *found;

	found = RB_FIND(options_tree, &oo->tree, &o);
	if (found == NULL) {
		o.name = options_map_name(name);
		return (RB_FIND(options_tree, &oo->tree, &o));
	}
	return (found);
}

struct options_entry *
options_get(struct options *oo, const char *name)
{
	struct options_entry	*o;

	o = options_get_only(oo, name);
	while (o == NULL) {
		oo = oo->parent;
		if (oo == NULL)
			break;
		o = options_get_only(oo, name);
	}
	return (o);
}

struct options_entry *
options_empty(struct options *oo, const struct options_table_entry *oe)
{
	struct options_entry	*o;

	o = options_add(oo, oe->name);
	o->tableentry = oe;

	if (oe->flags & OPTIONS_TABLE_IS_ARRAY)
		RB_INIT(&o->value.array);

	return (o);
}

struct options_entry *
options_default(struct options *oo, const struct options_table_entry *oe)
{
	struct options_entry	*o;
	union options_value	*ov;
	u_int			 i;

	o = options_empty(oo, oe);
	ov = &o->value;

	if (oe->flags & OPTIONS_TABLE_IS_ARRAY) {
		if (oe->default_arr == NULL) {
			options_array_assign(o, oe->default_str, NULL);
			return (o);
		}
		for (i = 0; oe->default_arr[i] != NULL; i++)
			options_array_set(o, i, oe->default_arr[i], 0, NULL);
		return (o);
	}

	switch (oe->type) {
	case OPTIONS_TABLE_STRING:
		ov->string = xstrdup(oe->default_str);
		break;
	default:
		ov->number = oe->default_num;
		break;
	}
	return (o);
}

char *
options_default_to_string(const struct options_table_entry *oe)
{
	char	*s;

	switch (oe->type) {
	case OPTIONS_TABLE_STRING:
	case OPTIONS_TABLE_COMMAND:
		s = xstrdup(oe->default_str);
		break;
	case OPTIONS_TABLE_NUMBER:
		xasprintf(&s, "%lld", oe->default_num);
		break;
	case OPTIONS_TABLE_KEY:
		s = xstrdup(key_string_lookup_key(oe->default_num, 0));
		break;
	case OPTIONS_TABLE_COLOUR:
		s = xstrdup(colour_tostring(oe->default_num));
		break;
	case OPTIONS_TABLE_FLAG:
		s = xstrdup(oe->default_num ? "on" : "off");
		break;
	case OPTIONS_TABLE_CHOICE:
		s = xstrdup(oe->choices[oe->default_num]);
		break;
	default:
		fatalx("unknown option type");
	}
	return (s);
}

static struct options_entry *
options_add(struct options *oo, const char *name)
{
	struct options_entry	*o;

	o = options_get_only(oo, name);
	if (o != NULL)
		options_remove(o);

	o = xcalloc(1, sizeof *o);
	o->owner = oo;
	o->name = xstrdup(name);

	RB_INSERT(options_tree, &oo->tree, o);
	return (o);
}

static void
options_remove(struct options_entry *o)
{
	struct options	*oo = o->owner;

	if (OPTIONS_IS_ARRAY(o))
		options_array_clear(o);
	else
		options_value_free(o, &o->value);
	RB_REMOVE(options_tree, &oo->tree, o);
	free((void *)o->name);
	free(o);
}

const char *
options_name(struct options_entry *o)
{
	return (o->name);
}

struct options *
options_owner(struct options_entry *o)
{
	return (o->owner);
}

const struct options_table_entry *
options_table_entry(struct options_entry *o)
{
	return (o->tableentry);
}

static struct options_array_item *
options_array_item(struct options_entry *o, u_int idx)
{
	struct options_array_item	a;

	a.index = idx;
	return (RB_FIND(options_array, &o->value.array, &a));
}

static struct options_array_item *
options_array_new(struct options_entry *o, u_int idx)
{
	struct options_array_item	*a;

	a = xcalloc(1, sizeof *a);
	a->index = idx;
	RB_INSERT(options_array, &o->value.array, a);
	return (a);
}

static void
options_array_free(struct options_entry *o, struct options_array_item *a)
{
	options_value_free(o, &a->value);
	RB_REMOVE(options_array, &o->value.array, a);
	free(a);
}

void
options_array_clear(struct options_entry *o)
{
	struct options_array_item	*a, *a1;

	if (!OPTIONS_IS_ARRAY(o))
		return;

	RB_FOREACH_SAFE(a, options_array, &o->value.array, a1)
		options_array_free(o, a);
}

union options_value *
options_array_get(struct options_entry *o, u_int idx)
{
	struct options_array_item	*a;

	if (!OPTIONS_IS_ARRAY(o))
		return (NULL);
	a = options_array_item(o, idx);
	if (a == NULL)
		return (NULL);
	return (&a->value);
}

int
options_array_set(struct options_entry *o, u_int idx, const char *value,
    int append, char **cause)
{
	struct options_array_item	*a;
	char				*new;
	struct cmd_parse_result		*pr;
	long long		 	 number;

	if (!OPTIONS_IS_ARRAY(o)) {
		if (cause != NULL)
			*cause = xstrdup("not an array");
		return (-1);
	}

	if (value == NULL) {
		a = options_array_item(o, idx);
		if (a != NULL)
			options_array_free(o, a);
		return (0);
	}

	if (OPTIONS_IS_COMMAND(o)) {
		pr = cmd_parse_from_string(value, NULL);
		switch (pr->status) {
		case CMD_PARSE_ERROR:
			if (cause != NULL)
				*cause = pr->error;
			else
				free(pr->error);
			return (-1);
		case CMD_PARSE_SUCCESS:
			break;
		}

		a = options_array_item(o, idx);
		if (a == NULL)
			a = options_array_new(o, idx);
		else
			options_value_free(o, &a->value);
		a->value.cmdlist = pr->cmdlist;
		return (0);
	}

	if (OPTIONS_IS_STRING(o)) {
		a = options_array_item(o, idx);
		if (a != NULL && append)
			xasprintf(&new, "%s%s", a->value.string, value);
		else
			new = xstrdup(value);
		if (a == NULL)
			a = options_array_new(o, idx);
		else
			options_value_free(o, &a->value);
		a->value.string = new;
		return (0);
	}

	if (o->tableentry->type == OPTIONS_TABLE_COLOUR) {
		if ((number = colour_fromstring(value)) == -1) {
			xasprintf(cause, "bad colour: %s", value);
			return (-1);
		}
		a = options_array_item(o, idx);
		if (a == NULL)
			a = options_array_new(o, idx);
		else
			options_value_free(o, &a->value);
		a->value.number = number;
		return (0);
	}

	if (cause != NULL)
		*cause = xstrdup("wrong array type");
	return (-1);
}

int
options_array_assign(struct options_entry *o, const char *s, char **cause)
{
	const char	*separator;
	char		*copy, *next, *string;
	u_int		 i;

	separator = o->tableentry->separator;
	if (separator == NULL)
		separator = " ,";
	if (*separator == '\0') {
		if (*s == '\0')
			return (0);
		for (i = 0; i < UINT_MAX; i++) {
			if (options_array_item(o, i) == NULL)
				break;
		}
		return (options_array_set(o, i, s, 0, cause));
	}

	if (*s == '\0')
		return (0);
	copy = string = xstrdup(s);
	while ((next = strsep(&string, separator)) != NULL) {
		if (*next == '\0')
			continue;
		for (i = 0; i < UINT_MAX; i++) {
			if (options_array_item(o, i) == NULL)
				break;
		}
		if (i == UINT_MAX)
			break;
		if (options_array_set(o, i, next, 0, cause) != 0) {
			free(copy);
			return (-1);
		}
	}
	free(copy);
	return (0);
}

struct options_array_item *
options_array_first(struct options_entry *o)
{
	if (!OPTIONS_IS_ARRAY(o))
		return (NULL);
	return (RB_MIN(options_array, &o->value.array));
}

struct options_array_item *
options_array_next(struct options_array_item *a)
{
	return (RB_NEXT(options_array, &o->value.array, a));
}

u_int
options_array_item_index(struct options_array_item *a)
{
	return (a->index);
}

union options_value *
options_array_item_value(struct options_array_item *a)
{
	return (&a->value);
}

int
options_is_array(struct options_entry *o)
{
	return (OPTIONS_IS_ARRAY(o));
}

int
options_is_string(struct options_entry *o)
{
	return (OPTIONS_IS_STRING(o));
}

char *
options_to_string(struct options_entry *o, int idx, int numeric)
{
	struct options_array_item	*a;
	char				*result = NULL;
	char				*last = NULL;
	char				*next;

	if (OPTIONS_IS_ARRAY(o)) {
		if (idx == -1) {
			RB_FOREACH(a, options_array, &o->value.array) {
				next = options_value_to_string(o, &a->value,
				    numeric);
				if (last == NULL)
					result = next;
				else {
					xasprintf(&result, "%s %s", last, next);
					free(last);
					free(next);
				}
				last = result;
			}
			if (result == NULL)
				return (xstrdup(""));
			return (result);
		}
		a = options_array_item(o, idx);
		if (a == NULL)
			return (xstrdup(""));
		return (options_value_to_string(o, &a->value, numeric));
	}
	return (options_value_to_string(o, &o->value, numeric));
}

char *
options_parse(const char *name, int *idx)
{
	char	*copy, *cp, *end;

	if (*name == '\0')
		return (NULL);
	copy = xstrdup(name);
	if ((cp = strchr(copy, '[')) == NULL) {
		*idx = -1;
		return (copy);
	}
	end = strchr(cp + 1, ']');
	if (end == NULL || end[1] != '\0' || !isdigit((u_char)end[-1])) {
		free(copy);
		return (NULL);
	}
	if (sscanf(cp, "[%d]", idx) != 1 || *idx < 0) {
		free(copy);
		return (NULL);
	}
	*cp = '\0';
	return (copy);
}

struct options_entry *
options_parse_get(struct options *oo, const char *s, int *idx, int only)
{
	struct options_entry	*o;
	char			*name;

	name = options_parse(s, idx);
	if (name == NULL)
		return (NULL);
	if (only)
		o = options_get_only(oo, name);
	else
		o = options_get(oo, name);
	free(name);
	return (o);
}

char *
options_match(const char *s, int *idx, int *ambiguous)
{
	const struct options_table_entry	*oe, *found;
	char					*parsed;
	const char				*name;
	size_t					 namelen;

	parsed = options_parse(s, idx);
	if (parsed == NULL)
		return (NULL);
	if (*parsed == '@') {
		*ambiguous = 0;
		return (parsed);
	}

	name = options_map_name(parsed);
	namelen = strlen(name);

	found = NULL;
	for (oe = options_table; oe->name != NULL; oe++) {
		if (strcmp(oe->name, name) == 0) {
			found = oe;
			break;
		}
		if (strncmp(oe->name, name, namelen) == 0) {
			if (found != NULL) {
				*ambiguous = 1;
				free(parsed);
				return (NULL);
			}
			found = oe;
		}
	}
	free(parsed);
	if (found == NULL) {
		*ambiguous = 0;
		return (NULL);
	}
	return (xstrdup(found->name));
}

struct options_entry *
options_match_get(struct options *oo, const char *s, int *idx, int only,
    int *ambiguous)
{
	char			*name;
	struct options_entry	*o;

	name = options_match(s, idx, ambiguous);
	if (name == NULL)
		return (NULL);
	*ambiguous = 0;
	if (only)
		o = options_get_only(oo, name);
	else
		o = options_get(oo, name);
	free(name);
	return (o);
}

const char *
options_get_string(struct options *oo, const char *name)
{
	struct options_entry	*o;

	o = options_get(oo, name);
	if (o == NULL)
		fatalx("missing option %s", name);
	if (!OPTIONS_IS_STRING(o))
		fatalx("option %s is not a string", name);
	return (o->value.string);
}

long long
options_get_number(struct options *oo, const char *name)
{
	struct options_entry	*o;

	o = options_get(oo, name);
	if (o == NULL)
		fatalx("missing option %s", name);
	if (!OPTIONS_IS_NUMBER(o))
		fatalx("option %s is not a number", name);
	return (o->value.number);
}

struct options_entry *
options_set_string(struct options *oo, const char *name, int append,
    const char *fmt, ...)
{
	struct options_entry	*o;
	va_list			 ap;
	const char		*separator = "";
	char			*s, *value;

	va_start(ap, fmt);
	xvasprintf(&s, fmt, ap);
	va_end(ap);

	o = options_get_only(oo, name);
	if (o != NULL && append && OPTIONS_IS_STRING(o)) {
		if (*name != '@') {
			separator = o->tableentry->separator;
			if (separator == NULL)
				separator = "";
		}
		xasprintf(&value, "%s%s%s", o->value.string, separator, s);
		free(s);
	} else
		value = s;
	if (o == NULL && *name == '@')
		o = options_add(oo, name);
	else if (o == NULL) {
		o = options_default(oo, options_parent_table_entry(oo, name));
		if (o == NULL)
			return (NULL);
	}

	if (!OPTIONS_IS_STRING(o))
		fatalx("option %s is not a string", name);
	free(o->value.string);
	o->value.string = value;
	o->cached = 0;
	return (o);
}

struct options_entry *
options_set_number(struct options *oo, const char *name, long long value)
{
	struct options_entry	*o;

	if (*name == '@')
		fatalx("user option %s must be a string", name);

	o = options_get_only(oo, name);
	if (o == NULL) {
		o = options_default(oo, options_parent_table_entry(oo, name));
		if (o == NULL)
			return (NULL);
	}

	if (!OPTIONS_IS_NUMBER(o))
		fatalx("option %s is not a number", name);
	o->value.number = value;
	return (o);
}

int
options_scope_from_name(struct args *args, int window,
    const char *name, struct cmd_find_state *fs, struct options **oo,
    char **cause)
{
	struct session				*s = fs->s;
	struct winlink				*wl = fs->wl;
	struct window_pane			*wp = fs->wp;
	const char				*target = args_get(args, 't');
	const struct options_table_entry	*oe;
	int					 scope = OPTIONS_TABLE_NONE;

	if (*name == '@')
		return (options_scope_from_flags(args, window, fs, oo, cause));

	for (oe = options_table; oe->name != NULL; oe++) {
		if (strcmp(oe->name, name) == 0)
			break;
	}
	if (oe->name == NULL) {
		xasprintf(cause, "unknown option: %s", name);
		return (OPTIONS_TABLE_NONE);
	}
	switch (oe->scope) {
	case OPTIONS_TABLE_SERVER:
		*oo = global_options;
		scope = OPTIONS_TABLE_SERVER;
		break;
	case OPTIONS_TABLE_SESSION:
		if (args_has(args, 'g')) {
			*oo = global_s_options;
			scope = OPTIONS_TABLE_SESSION;
		} else if (s == NULL && target != NULL)
			xasprintf(cause, "no such session: %s", target);
		else if (s == NULL)
			xasprintf(cause, "no current session");
		else {
			*oo = s->options;
			scope = OPTIONS_TABLE_SESSION;
		}
		break;
	case OPTIONS_TABLE_WINDOW|OPTIONS_TABLE_PANE:
		if (args_has(args, 'p')) {
			if (wp == NULL && target != NULL)
				xasprintf(cause, "no such pane: %s", target);
			else if (wp == NULL)
				xasprintf(cause, "no current pane");
			else {
				*oo = wp->options;
				scope = OPTIONS_TABLE_PANE;
			}
			break;
		}
		/* FALLTHROUGH */
	case OPTIONS_TABLE_WINDOW:
		if (args_has(args, 'g')) {
			*oo = global_w_options;
			scope = OPTIONS_TABLE_WINDOW;
		} else if (wl == NULL && target != NULL)
			xasprintf(cause, "no such window: %s", target);
		else if (wl == NULL)
			xasprintf(cause, "no current window");
		else {
			*oo = wl->window->options;
			scope = OPTIONS_TABLE_WINDOW;
		}
		break;
	}
	return (scope);
}

int
options_scope_from_flags(struct args *args, int window,
    struct cmd_find_state *fs, struct options **oo, char **cause)
{
	struct session		*s = fs->s;
	struct winlink		*wl = fs->wl;
	struct window_pane	*wp = fs->wp;
	const char		*target = args_get(args, 't');

	if (args_has(args, 's')) {
		*oo = global_options;
		return (OPTIONS_TABLE_SERVER);
	}

	if (args_has(args, 'p')) {
		if (wp == NULL) {
			if (target != NULL)
				xasprintf(cause, "no such pane: %s", target);
			else
				xasprintf(cause, "no current pane");
			return (OPTIONS_TABLE_NONE);
		}
		*oo = wp->options;
		return (OPTIONS_TABLE_PANE);
	} else if (window || args_has(args, 'w')) {
		if (args_has(args, 'g')) {
			*oo = global_w_options;
			return (OPTIONS_TABLE_WINDOW);
		}
		if (wl == NULL) {
			if (target != NULL)
				xasprintf(cause, "no such window: %s", target);
			else
				xasprintf(cause, "no current window");
			return (OPTIONS_TABLE_NONE);
		}
		*oo = wl->window->options;
		return (OPTIONS_TABLE_WINDOW);
	} else {
		if (args_has(args, 'g')) {
			*oo = global_s_options;
			return (OPTIONS_TABLE_SESSION);
		}
		if (s == NULL) {
			if (target != NULL)
				xasprintf(cause, "no such session: %s", target);
			else
				xasprintf(cause, "no current session");
			return (OPTIONS_TABLE_NONE);
		}
		*oo = s->options;
		return (OPTIONS_TABLE_SESSION);
	}
}

struct style *
options_string_to_style(struct options *oo, const char *name,
    struct format_tree *ft)
{
	struct options_entry	*o;
	const char		*s;
	char			*expanded;

	o = options_get(oo, name);
	if (o == NULL || !OPTIONS_IS_STRING(o))
		return (NULL);

	if (o->cached)
		return (&o->style);
	s = o->value.string;
	log_debug("%s: %s is '%s'", __func__, name, s);

	style_set(&o->style, &grid_default_cell);
	o->cached = (strstr(s, "#{") == NULL);

	if (ft != NULL && !o->cached) {
		expanded = format_expand(ft, s);
		if (style_parse(&o->style, &grid_default_cell, expanded) != 0) {
			free(expanded);
			return (NULL);
		}
		free(expanded);
	} else {
		if (style_parse(&o->style, &grid_default_cell, s) != 0)
			return (NULL);
	}
	return (&o->style);
}

static int
options_from_string_check(const struct options_table_entry *oe,
    const char *value, char **cause)
{
	struct style	sy;

	if (oe == NULL)
		return (0);
	if (strcmp(oe->name, "default-shell") == 0 && !checkshell(value)) {
		xasprintf(cause, "not a suitable shell: %s", value);
		return (-1);
	}
	if (oe->pattern != NULL && fnmatch(oe->pattern, value, 0) != 0) {
		xasprintf(cause, "value is invalid: %s", value);
		return (-1);
	}
	if ((oe->flags & OPTIONS_TABLE_IS_STYLE) &&
	    strstr(value, "#{") == NULL &&
	    style_parse(&sy, &grid_default_cell, value) != 0) {
		xasprintf(cause, "invalid style: %s", value);
		return (-1);
	}
	return (0);
}

static int
options_from_string_flag(struct options *oo, const char *name,
    const char *value, char **cause)
{
	int	flag;

	if (value == NULL || *value == '\0')
		flag = !options_get_number(oo, name);
	else if (strcmp(value, "1") == 0 ||
	    strcasecmp(value, "on") == 0 ||
	    strcasecmp(value, "yes") == 0)
		flag = 1;
	else if (strcmp(value, "0") == 0 ||
	    strcasecmp(value, "off") == 0 ||
	    strcasecmp(value, "no") == 0)
		flag = 0;
	else {
		xasprintf(cause, "bad value: %s", value);
		return (-1);
	}
	options_set_number(oo, name, flag);
	return (0);
}

int
options_find_choice(const struct options_table_entry *oe, const char *value,
    char **cause)
{
	const char	**cp;
	int		  n = 0, choice = -1;

	for (cp = oe->choices; *cp != NULL; cp++) {
		if (strcmp(*cp, value) == 0)
			choice = n;
		n++;
	}
	if (choice == -1) {
		xasprintf(cause, "unknown value: %s", value);
		return (-1);
	}
	return (choice);
}

static int
options_from_string_choice(const struct options_table_entry *oe,
    struct options *oo, const char *name, const char *value, char **cause)
{
	int	choice = -1;

	if (value == NULL) {
		choice = options_get_number(oo, name);
		if (choice < 2)
			choice = !choice;
	} else {
		choice = options_find_choice(oe, value, cause);
		if (choice < 0)
			return (-1);
	}
	options_set_number(oo, name, choice);
	return (0);
}

int
options_from_string(struct options *oo, const struct options_table_entry *oe,
    const char *name, const char *value, int append, char **cause)
{
	enum options_table_type	 type;
	long long		 number;
	const char		*errstr, *new;
	char			*old;
	key_code		 key;

	if (oe != NULL) {
		if (value == NULL &&
		    oe->type != OPTIONS_TABLE_FLAG &&
		    oe->type != OPTIONS_TABLE_CHOICE) {
			xasprintf(cause, "empty value");
			return (-1);
		}
		type = oe->type;
	} else {
		if (*name != '@') {
			xasprintf(cause, "bad option name");
			return (-1);
		}
		type = OPTIONS_TABLE_STRING;
	}

	switch (type) {
	case OPTIONS_TABLE_STRING:
		old = xstrdup(options_get_string(oo, name));
		options_set_string(oo, name, append, "%s", value);

		new = options_get_string(oo, name);
		if (options_from_string_check(oe, new, cause) != 0) {
			options_set_string(oo, name, 0, "%s", old);
			free(old);
			return (-1);
		}
		free(old);
		return (0);
	case OPTIONS_TABLE_NUMBER:
		number = strtonum(value, oe->minimum, oe->maximum, &errstr);
		if (errstr != NULL) {
			xasprintf(cause, "value is %s: %s", errstr, value);
			return (-1);
		}
		options_set_number(oo, name, number);
		return (0);
	case OPTIONS_TABLE_KEY:
		key = key_string_lookup_string(value);
		if (key == KEYC_UNKNOWN) {
			xasprintf(cause, "bad key: %s", value);
			return (-1);
		}
		options_set_number(oo, name, key);
		return (0);
	case OPTIONS_TABLE_COLOUR:
		if ((number = colour_fromstring(value)) == -1) {
			xasprintf(cause, "bad colour: %s", value);
			return (-1);
		}
		options_set_number(oo, name, number);
		return (0);
	case OPTIONS_TABLE_FLAG:
		return (options_from_string_flag(oo, name, value, cause));
	case OPTIONS_TABLE_CHOICE:
		return (options_from_string_choice(oe, oo, name, value, cause));
	case OPTIONS_TABLE_COMMAND:
		break;
	}
	return (-1);
}

void
options_push_changes(const char *name)
{
	struct client		*loop;
	struct session		*s;
	struct window		*w;
	struct window_pane	*wp;

	log_debug("%s: %s", __func__, name);

	if (strcmp(name, "automatic-rename") == 0) {
		RB_FOREACH(w, windows, &windows) {
			if (w->active == NULL)
				continue;
			if (options_get_number(w->options, name))
				w->active->flags |= PANE_CHANGED;
		}
	}
	if (strcmp(name, "cursor-colour") == 0) {
		RB_FOREACH(wp, window_pane_tree, &all_window_panes)
			window_pane_default_cursor(wp);
	}
	if (strcmp(name, "cursor-style") == 0) {
		RB_FOREACH(wp, window_pane_tree, &all_window_panes)
			window_pane_default_cursor(wp);
	}
	if (strcmp(name, "fill-character") == 0) {
		RB_FOREACH(w, windows, &windows)
			window_set_fill_character(w);
	}
	if (strcmp(name, "key-table") == 0) {
		TAILQ_FOREACH(loop, &clients, entry)
			server_client_set_key_table(loop, NULL);
	}
	if (strcmp(name, "user-keys") == 0) {
		TAILQ_FOREACH(loop, &clients, entry) {
			if (loop->tty.flags & TTY_OPENED)
				tty_keys_build(&loop->tty);
		}
	}
	if (strcmp(name, "status") == 0 ||
	    strcmp(name, "status-interval") == 0)
		status_timer_start_all();
	if (strcmp(name, "monitor-silence") == 0)
		alerts_reset_all();
	if (strcmp(name, "window-style") == 0 ||
	    strcmp(name, "window-active-style") == 0) {
		RB_FOREACH(wp, window_pane_tree, &all_window_panes)
			wp->flags |= PANE_STYLECHANGED;
	}
	if (strcmp(name, "pane-colours") == 0) {
		RB_FOREACH(wp, window_pane_tree, &all_window_panes)
			colour_palette_from_option(&wp->palette, wp->options);
	}
	if (strcmp(name, "pane-border-status") == 0 ||
	    strcmp(name, "pane-scrollbars") == 0 ||
	    strcmp(name, "pane-scrollbars-position") == 0) {
		RB_FOREACH(w, windows, &windows)
			layout_fix_panes(w, NULL);
	}
	if (strcmp(name, "pane-scrollbars-style") == 0) {
		RB_FOREACH(wp, window_pane_tree, &all_window_panes) {
			style_set_scrollbar_style_from_option(
			    &wp->scrollbar_style, wp->options);
		}
		RB_FOREACH(w, windows, &windows)
			layout_fix_panes(w, NULL);
	}
	if (strcmp(name, "codepoint-widths") == 0)
		utf8_update_width_cache();
	if (strcmp(name, "input-buffer-size") == 0)
		input_set_buffer_size(options_get_number(global_options, name));
	RB_FOREACH(s, sessions, &sessions)
		status_update_cache(s);

	recalculate_sizes();
	TAILQ_FOREACH(loop, &clients, entry) {
		if (loop->session != NULL)
			server_redraw_client(loop);
	}
}

int
options_remove_or_default(struct options_entry *o, int idx, char **cause)
{
	struct options	*oo = o->owner;

	if (idx == -1) {
		if (o->tableentry != NULL &&
		    (oo == global_options ||
		    oo == global_s_options ||
		    oo == global_w_options))
			options_default(oo, o->tableentry);
		else
			options_remove(o);
	} else if (options_array_set(o, idx, NULL, 0, cause) != 0)
		return (-1);
	return (0);
}
