/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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

RB_HEAD(environ, environ_entry);
int	environ_cmp(struct environ_entry *, struct environ_entry *);
RB_PROTOTYPE(environ, environ_entry, entry, environ_cmp);
RB_GENERATE(environ, environ_entry, entry, environ_cmp);

int
environ_cmp(struct environ_entry *envent1, struct environ_entry *envent2)
{
	return (strcmp(envent1->name, envent2->name));
}

/* Initialise the environment. */
struct environ *
environ_create(void)
{
	struct environ	*env;

	env = xcalloc(1, sizeof *env);
	RB_INIT(env);

	return (env);
}

/* Free an environment. */
void
environ_free(struct environ *env)
{
	struct environ_entry	*envent, *envent1;

	RB_FOREACH_SAFE(envent, environ, env, envent1) {
		RB_REMOVE(environ, env, envent);
		free(envent->name);
		free(envent->value);
		free(envent);
	}
	free(env);
}

struct environ_entry *
environ_first(struct environ *env)
{
	return (RB_MIN(environ, env));
}

struct environ_entry *
environ_next(struct environ_entry *envent)
{
	return (RB_NEXT(environ, env, envent));
}

/* Copy one environment into another. */
void
environ_copy(struct environ *srcenv, struct environ *dstenv)
{
	struct environ_entry	*envent;

	RB_FOREACH(envent, environ, srcenv) {
		if (envent->value == NULL)
			environ_clear(dstenv, envent->name);
		else
			environ_set(dstenv, envent->name, "%s", envent->value);
	}
}

/* Find an environment variable. */
struct environ_entry *
environ_find(struct environ *env, const char *name)
{
	struct environ_entry	envent;

	envent.name = (char *) name;
	return (RB_FIND(environ, env, &envent));
}

/* Set an environment variable. */
void
environ_set(struct environ *env, const char *name, const char *fmt, ...)
{
	struct environ_entry	*envent;
	va_list			 ap;

	va_start(ap, fmt);
	if ((envent = environ_find(env, name)) != NULL) {
		free(envent->value);
		xvasprintf(&envent->value, fmt, ap);
	} else {
		envent = xmalloc(sizeof *envent);
		envent->name = xstrdup(name);
		xvasprintf(&envent->value, fmt, ap);
		RB_INSERT(environ, env, envent);
	}
	va_end(ap);
}

/* Clear an environment variable. */
void
environ_clear(struct environ *env, const char *name)
{
	struct environ_entry	*envent;

	if ((envent = environ_find(env, name)) != NULL) {
		free(envent->value);
		envent->value = NULL;
	} else {
		envent = xmalloc(sizeof *envent);
		envent->name = xstrdup(name);
		envent->value = NULL;
		RB_INSERT(environ, env, envent);
	}
}

/* Set an environment variable from a NAME=VALUE string. */
void
environ_put(struct environ *env, const char *var)
{
	char	*name, *value;

	value = strchr(var, '=');
	if (value == NULL)
		return;
	value++;

	name = xstrdup(var);
	name[strcspn(name, "=")] = '\0';

	environ_set(env, name, "%s", value);
	free(name);
}

/* Unset an environment variable. */
void
environ_unset(struct environ *env, const char *name)
{
	struct environ_entry	*envent;

	if ((envent = environ_find(env, name)) == NULL)
		return;
	RB_REMOVE(environ, env, envent);
	free(envent->name);
	free(envent->value);
	free(envent);
}

/*
 * Copy a space-separated list of variables from a destination into a source
 * environment.
 */
void
environ_update(const char *vars, struct environ *srcenv,
    struct environ *dstenv)
{
	struct environ_entry	*envent;
	char			*copyvars, *var, *next;

	copyvars = next = xstrdup(vars);
	while ((var = strsep(&next, " ")) != NULL) {
		if ((envent = environ_find(srcenv, var)) == NULL)
			environ_clear(dstenv, var);
		else
			environ_set(dstenv, envent->name, "%s", envent->value);
	}
	free(copyvars);
}

/* Push environment into the real environment - use after fork(). */
void
environ_push(struct environ *env)
{
	struct environ_entry	 *envent;
	char			*v;

	while (*environ != NULL) {
		v = xstrdup(*environ);
		v[strcspn(v, "=")] = '\0';

		unsetenv(v);
		free(v);
	}

	RB_FOREACH(envent, environ, env) {
		if (envent->value != NULL)
			setenv(envent->name, envent->value, 1);
	}
}
