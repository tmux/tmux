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

#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

/*
 * Environment - manipulate a set of environment variables.
 */

RB_HEAD(environ, environ_entry);
static int environ_cmp(struct environ_entry *, struct environ_entry *);
RB_GENERATE_STATIC(environ, environ_entry, entry, environ_cmp);

static int
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
		else {
			environ_set(dstenv, envent->name, envent->flags,
			    "%s", envent->value);
		}
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
environ_set(struct environ *env, const char *name, int flags, const char *fmt,
    ...)
{
	struct environ_entry	*envent;
	va_list			 ap;

	va_start(ap, fmt);
	if ((envent = environ_find(env, name)) != NULL) {
		envent->flags = flags;
		free(envent->value);
		xvasprintf(&envent->value, fmt, ap);
	} else {
		envent = xmalloc(sizeof *envent);
		envent->name = xstrdup(name);
		envent->flags = flags;
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
		envent->flags = 0;
		envent->value = NULL;
		RB_INSERT(environ, env, envent);
	}
}

/* Set an environment variable from a NAME=VALUE string. */
void
environ_put(struct environ *env, const char *var, int flags)
{
	char	*name, *value;

	value = strchr(var, '=');
	if (value == NULL)
		return;
	value++;

	name = xstrdup(var);
	name[strcspn(name, "=")] = '\0';

	environ_set(env, name, flags, "%s", value);
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

/* Copy variables from a destination into a source environment. */
void
environ_update(struct options *oo, struct environ *src, struct environ *dst)
{
	struct environ_entry		*envent;
	struct environ_entry		*envent1;
	struct options_entry		*o;
	struct options_array_item	*a;
	union options_value		*ov;
	int				 found;

	o = options_get(oo, "update-environment");
	if (o == NULL)
		return;
	a = options_array_first(o);
	while (a != NULL) {
		ov = options_array_item_value(a);
		found = 0;
		RB_FOREACH_SAFE(envent, environ, src, envent1) {
			if (fnmatch(ov->string, envent->name, 0) == 0) {
				environ_set(dst, envent->name, 0, "%s", envent->value);
				found = 1;
			}
		}
		if (!found)
			environ_clear(dst, ov->string);
		a = options_array_next(a);
	}
}

/* Push environment into the real environment - use after fork(). */
void
environ_push(struct environ *env)
{
	struct environ_entry	*envent;

	environ = xcalloc(1, sizeof *environ);
	RB_FOREACH(envent, environ, env) {
		if (envent->value != NULL &&
		    *envent->name != '\0' &&
		    (~envent->flags & ENVIRON_HIDDEN))
			setenv(envent->name, envent->value, 1);
	}
}

/* Log the environment. */
void
environ_log(struct environ *env, const char *fmt, ...)
{
	struct environ_entry	*envent;
	va_list			 ap;
	char			*prefix;

	va_start(ap, fmt);
	vasprintf(&prefix, fmt, ap);
	va_end(ap);

	RB_FOREACH(envent, environ, env) {
		if (envent->value != NULL && *envent->name != '\0') {
			log_debug("%s%s=%s", prefix, envent->name,
			    envent->value);
		}
	}

	free(prefix);
}

/* Create initial environment for new child. */
struct environ *
environ_for_session(struct session *s, int no_TERM)
{
	struct environ	*env;
	const char	*value;
	int		 idx;

	env = environ_create();
	environ_copy(global_environ, env);
	if (s != NULL)
		environ_copy(s->environ, env);

	if (!no_TERM) {
		value = options_get_string(global_options, "default-terminal");
		environ_set(env, "TERM", 0, "%s", value);
		environ_set(env, "TERM_PROGRAM", 0, "%s", "tmux");
		environ_set(env, "TERM_PROGRAM_VERSION", 0, "%s", getversion());
	}

#ifdef HAVE_SYSTEMD
	environ_clear(env, "LISTEN_PID");
	environ_clear(env, "LISTEN_FDS");
	environ_clear(env, "LISTEN_FDNAMES");
#endif

	if (s != NULL)
		idx = s->id;
	else
		idx = -1;
	environ_set(env, "TMUX", 0, "%s,%ld,%d", socket_path, (long)getpid(),
	    idx);

	return (env);
}
