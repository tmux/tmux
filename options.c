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
	char				*value;
	RB_ENTRY(options_array_item)	 entry;
};
RB_HEAD(options_array, options_array_item);
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
	struct options				 *owner;

	const char				 *name;
	const struct options_table_entry	 *tableentry;

	union {
		char				 *string;
		long long			  number;
		struct style			  style;
		struct options_array		  array;
	};

	RB_ENTRY(options_entry)			  entry;
};

struct options {
	RB_HEAD(options_tree, options_entry)	 tree;
	struct options				*parent;
};

static struct options_entry	*options_add(struct options *, const char *);

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
#define OPTIONS_IS_STYLE(o) \
	((o)->tableentry != NULL &&					\
	    (o)->tableentry->type == OPTIONS_TABLE_STYLE)
#define OPTIONS_IS_ARRAY(o) \
	((o)->tableentry != NULL &&					\
	    (o)->tableentry->type == OPTIONS_TABLE_ARRAY)

static int	options_cmp(struct options_entry *, struct options_entry *);
RB_GENERATE_STATIC(options_tree, options_entry, entry, options_cmp);

static int
options_cmp(struct options_entry *lhs, struct options_entry *rhs)
{
	return (strcmp(lhs->name, rhs->name));
}

static const struct options_table_entry *
options_parent_table_entry(struct options *oo, const char *s)
{
	struct options_entry	*o;

	if (oo->parent == NULL)
		fatalx("no parent options for %s", s);
	o = options_get_only(oo->parent, s);
	if (o == NULL)
		fatalx("%s not in parent options", s);
	return (o->tableentry);
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
	struct options_entry	o;

	o.name = name;
	return (RB_FIND(options_tree, &oo->tree, &o));
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

	if (oe->type == OPTIONS_TABLE_ARRAY)
		RB_INIT(&o->array);

	return (o);
}

struct options_entry *
options_default(struct options *oo, const struct options_table_entry *oe)
{
	struct options_entry	 *o;
	u_int			  i;

	o = options_empty(oo, oe);
	if (oe->type == OPTIONS_TABLE_ARRAY) {
		if (oe->default_arr != NULL) {
			for (i = 0; oe->default_arr[i] != NULL; i++)
				options_array_set(o, i, oe->default_arr[i], 0);
		} else
			options_array_assign(o, oe->default_str);
	} else if (oe->type == OPTIONS_TABLE_STRING)
		o->string = xstrdup(oe->default_str);
	else if (oe->type == OPTIONS_TABLE_STYLE) {
		style_set(&o->style, &grid_default_cell);
		style_parse(&o->style, &grid_default_cell, oe->default_str);
	} else
		o->number = oe->default_num;
	return (o);
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

void
options_remove(struct options_entry *o)
{
	struct options	*oo = o->owner;

	if (OPTIONS_IS_STRING(o))
		free(o->string);
	else if (OPTIONS_IS_ARRAY(o))
		options_array_clear(o);

	RB_REMOVE(options_tree, &oo->tree, o);
	free(o);
}

const char *
options_name(struct options_entry *o)
{
	return (o->name);
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
	return (RB_FIND(options_array, &o->array, &a));
}

static void
options_array_free(struct options_entry *o, struct options_array_item *a)
{
	free(a->value);
	RB_REMOVE(options_array, &o->array, a);
	free(a);
}

void
options_array_clear(struct options_entry *o)
{
	struct options_array_item	*a, *a1;

	if (!OPTIONS_IS_ARRAY(o))
		return;

	RB_FOREACH_SAFE(a, options_array, &o->array, a1)
	    options_array_free(o, a);
}

const char *
options_array_get(struct options_entry *o, u_int idx)
{
	struct options_array_item	*a;

	if (!OPTIONS_IS_ARRAY(o))
		return (NULL);
	a = options_array_item(o, idx);
	if (a == NULL)
		return (NULL);
	return (a->value);
}

int
options_array_set(struct options_entry *o, u_int idx, const char *value,
    int append)
{
	struct options_array_item	*a;
	char				*new;

	if (!OPTIONS_IS_ARRAY(o))
		return (-1);

	a = options_array_item(o, idx);
	if (value == NULL) {
		if (a != NULL)
			options_array_free(o, a);
		return (0);
	}

	if (a == NULL) {
		a = xcalloc(1, sizeof *a);
		a->index = idx;
		a->value = xstrdup(value);
		RB_INSERT(options_array, &o->array, a);
	} else {
		free(a->value);
		if (a != NULL && append)
			xasprintf(&new, "%s%s", a->value, value);
		else
			new = xstrdup(value);
		a->value = new;
	}

	return (0);
}

void
options_array_assign(struct options_entry *o, const char *s)
{
	const char	*separator;
	char		*copy, *next, *string;
	u_int		 i;

	separator = o->tableentry->separator;
	if (separator == NULL)
		separator = " ,";

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
		options_array_set(o, i, next, 0);
	}
	free(copy);
}

struct options_array_item *
options_array_first(struct options_entry *o)
{
	if (!OPTIONS_IS_ARRAY(o))
		return (NULL);
	return (RB_MIN(options_array, &o->array));
}

struct options_array_item *
options_array_next(struct options_array_item *a)
{
	return (RB_NEXT(options_array, &o->array, a));
}

u_int
options_array_item_index(struct options_array_item *a)
{
	return (a->index);
}

const char *
options_array_item_value(struct options_array_item *a)
{
	return (a->value);
}

int
options_isarray(struct options_entry *o)
{
	return (OPTIONS_IS_ARRAY(o));
}

int
options_isstring(struct options_entry *o)
{
	return (OPTIONS_IS_STRING(o) || OPTIONS_IS_ARRAY(o));
}

const char *
options_tostring(struct options_entry *o, int idx, int numeric)
{
	static char			 s[1024];
	const char			*tmp;
	struct options_array_item	*a;

	if (OPTIONS_IS_ARRAY(o)) {
		if (idx == -1)
			return (NULL);
		a = options_array_item(o, idx);
		if (a == NULL)
			return ("");
		return (a->value);
	}
	if (OPTIONS_IS_STYLE(o))
		return (style_tostring(&o->style));
	if (OPTIONS_IS_NUMBER(o)) {
		tmp = NULL;
		switch (o->tableentry->type) {
		case OPTIONS_TABLE_NUMBER:
			xsnprintf(s, sizeof s, "%lld", o->number);
			break;
		case OPTIONS_TABLE_KEY:
			tmp = key_string_lookup_key(o->number);
			break;
		case OPTIONS_TABLE_COLOUR:
			tmp = colour_tostring(o->number);
			break;
		case OPTIONS_TABLE_FLAG:
			if (numeric)
				xsnprintf(s, sizeof s, "%lld", o->number);
			else
				tmp = (o->number ? "on" : "off");
			break;
		case OPTIONS_TABLE_CHOICE:
			tmp = o->tableentry->choices[o->number];
			break;
		case OPTIONS_TABLE_STRING:
		case OPTIONS_TABLE_STYLE:
		case OPTIONS_TABLE_ARRAY:
			break;
		}
		if (tmp != NULL)
			xsnprintf(s, sizeof s, "%s", tmp);
		return (s);
	}
	if (OPTIONS_IS_STRING(o))
		return (o->string);
	return (NULL);
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
options_match(const char *s, int *idx, int* ambiguous)
{
	const struct options_table_entry	*oe, *found;
	char					*name;
	size_t					 namelen;

	name = options_parse(s, idx);
	if (name == NULL)
		return (NULL);
	namelen = strlen(name);

	if (*name == '@') {
		*ambiguous = 0;
		return (name);
	}

	found = NULL;
	for (oe = options_table; oe->name != NULL; oe++) {
		if (strcmp(oe->name, name) == 0) {
			found = oe;
			break;
		}
		if (strncmp(oe->name, name, namelen) == 0) {
			if (found != NULL) {
				*ambiguous = 1;
				free(name);
				return (NULL);
			}
			found = oe;
		}
	}
	free(name);
	if (found == NULL) {
		*ambiguous = 0;
		return (NULL);
	}
	return (xstrdup(found->name));
}

struct options_entry *
options_match_get(struct options *oo, const char *s, int *idx, int only,
    int* ambiguous)
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
	return (o->string);
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
	return (o->number);
}

struct style *
options_get_style(struct options *oo, const char *name)
{
	struct options_entry	*o;

	o = options_get(oo, name);
	if (o == NULL)
		fatalx("missing option %s", name);
	if (!OPTIONS_IS_STYLE(o))
		fatalx("option %s is not a style", name);
	return (&o->style);
}

struct options_entry *
options_set_string(struct options *oo, const char *name, int append,
    const char *fmt, ...)
{
	struct options_entry	*o;
	va_list			 ap;
	char			*s, *value;

	va_start(ap, fmt);
	xvasprintf(&s, fmt, ap);
	va_end(ap);

	o = options_get_only(oo, name);
	if (o != NULL && append && OPTIONS_IS_STRING(o)) {
		xasprintf(&value, "%s%s", o->string, s);
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
	free(o->string);
	o->string = value;
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
	o->number = value;
	return (o);
}

struct options_entry *
options_set_style(struct options *oo, const char *name, int append,
    const char *value)
{
	struct options_entry	*o;
	struct style		 sy;

	if (*name == '@')
		fatalx("user option %s must be a string", name);

	o = options_get_only(oo, name);
	if (o != NULL && append && OPTIONS_IS_STYLE(o))
		style_copy(&sy, &o->style);
	else
		style_set(&sy, &grid_default_cell);
	if (style_parse(&sy, &grid_default_cell, value) == -1)
		return (NULL);
	if (o == NULL) {
		o = options_default(oo, options_parent_table_entry(oo, name));
		if (o == NULL)
			return (NULL);
	}

	if (!OPTIONS_IS_STYLE(o))
		fatalx("option %s is not a style", name);
	style_copy(&o->style, &sy);
	return (o);
}

enum options_table_scope
options_scope_from_flags(struct args *args, int window,
    struct cmd_find_state *fs, struct options **oo, char **cause)
{
	struct session	*s = fs->s;
	struct winlink	*wl = fs->wl;
	const char	*target= args_get(args, 't');

	if (args_has(args, 's')) {
		*oo = global_options;
		return (OPTIONS_TABLE_SERVER);
	}

	if (window || args_has(args, 'w')) {
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
