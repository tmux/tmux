/* $Id: environ.c,v 1.3 2009-08-09 17:57:39 tcunha Exp $ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

/*
 * Environment - manipulate a set of environment variables.
 */

RB_GENERATE(environ, environ_entry, entry, environ_cmp);

int
environ_cmp(struct environ_entry *envent1, struct environ_entry *envent2)
{
	return (strcmp(envent1->name, envent2->name));
}

void
environ_init(struct environ *env)
{
	RB_INIT(env);
}

void
environ_free(struct environ *env)
{
	struct environ_entry	*envent;

	while (!RB_EMPTY(env)) {
		envent = RB_ROOT(env);
		RB_REMOVE(environ, env, envent);
		xfree(envent->name);
		if (envent->value != NULL)
			xfree(envent->value);
		xfree(envent);
	}
}

void
environ_copy(struct environ *srcenv, struct environ *dstenv)
{
	struct environ_entry	*envent;

	RB_FOREACH(envent, environ, srcenv)
		environ_set(dstenv, envent->name, envent->value);
}

struct environ_entry *
environ_find(struct environ *env, const char *name)
{
	struct environ_entry	envent;

	envent.name = (char *) name;
	return (RB_FIND(environ, env, &envent));
}

void
environ_set(struct environ *env, const char *name, const char *value)
{
	struct environ_entry	*envent;

	if ((envent = environ_find(env, name)) != NULL) {
		if (envent->value != NULL)
			xfree(envent->value);
		if (value != NULL)
			envent->value = xstrdup(value);
		else
			envent->value = NULL;
	} else {
		envent = xmalloc(sizeof *envent);
		envent->name = xstrdup(name);
		if (value != NULL)
			envent->value = xstrdup(value);
		else
			envent->value = NULL;
		RB_INSERT(environ, env, envent);
	}
}

void
environ_put(struct environ *env, const char *var)
{
	char		*name, *value;

	value = strchr(var, '=');
	if (value == NULL)
		return;
	value++;

	name = xstrdup(var);
	name[strcspn(name, "=")] = '\0';

	environ_set(env, name, value);
	xfree(name);
}

void
environ_unset(struct environ *env, const char *name)
{
	struct environ_entry	*envent;

	if ((envent = environ_find(env, name)) == NULL)
		return;
	RB_REMOVE(environ, env, envent);
	xfree(envent->name);
	if (envent->value != NULL)
		xfree(envent->value);
	xfree(envent);
}

void
environ_update(const char *vars, struct environ *srcenv, struct environ *dstenv)
{
	struct environ_entry	*envent;
	char			*copyvars, *var, *next;

	copyvars = next = xstrdup(vars);
	while ((var = strsep(&next, " ")) != NULL) {
		if ((envent = environ_find(srcenv, var)) == NULL)
			environ_set(dstenv, var, NULL);
		else
			environ_set(dstenv, envent->name, envent->value);
	}
	xfree(copyvars);
}
