/* $OpenBSD$ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include "tmux.h"

struct client	*arg_lookup_client(const char *);
struct session	*arg_lookup_session(const char *);

struct client *
arg_lookup_client(const char *name)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && strcmp(name, c->tty.path) == 0)
			return (c);
	}

	return (NULL);
}

struct session *
arg_lookup_session(const char *name)
{
	struct session	*s, *newest = NULL;
	struct timeval	*tv;
	u_int		 i;

	tv = NULL;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL || fnmatch(name, s->name, 0) != 0)
			continue;

		if (tv == NULL || timercmp(&s->tv, tv, >)) {
			newest = s;
			tv = &s->tv;
		}
	}

	return (newest);
}

struct client *
arg_parse_client(const char *arg)
{
	struct client	*c;
	char		*arg2;
	size_t		 n;

	if (arg != NULL && (arg[0] != ':' || arg[1] != '\0')) {
		arg2 = xstrdup(arg);

		/* Trim a trailing : if any from the argument. */
		n = strlen(arg2);
		if (arg2[n - 1] == ':')
			arg2[n - 1] = '\0';

		/* Try and look up the client name. */
		c = arg_lookup_client(arg2);
		xfree(arg2);
		return (c);
	}

	return (NULL);
}

struct session *
arg_parse_session(const char *arg)
{
	struct session	*s;
	struct client	*c;
	char		*arg2;
	size_t		 n;

	if (arg != NULL && (arg[0] != ':' || arg[1] != '\0')) {
		arg2 = xstrdup(arg);

		/* Trim a trailing : if any from the argument. */
		n = strlen(arg2);
		if (arg2[n - 1] == ':')
			arg2[n - 1] = '\0';

		/* See if the argument matches a session. */
		if ((s = arg_lookup_session(arg2)) != NULL) {
			xfree(arg2);
			return (s);
		}

		/* If not try a client. */
		if ((c = arg_lookup_client(arg2)) != NULL) {
			xfree(arg2);
			return (c->session);
		}

		xfree(arg2);
	}

	return (NULL);
}

int
arg_parse_window(const char *arg, struct session **s, int *idx)
{
	char		*arg2, *ptr;
	const char	*errstr;

	*idx = -1;

	/* Handle no argument or a single :. */
	if (arg == NULL || (arg[0] == ':' && arg[1] == '\0')) {
		*s = arg_parse_session(NULL);
		return (0);
	}

	/* Find the separator if any. */
	arg2 = xstrdup(arg);
	ptr = strrchr(arg2, ':');

	/*
	 * If it is first, this means no session name, so use current session
	 * and try to convert the rest as index.
	 */
	if (ptr == arg2) {
		*idx = strtonum(ptr + 1, 0, INT_MAX, &errstr);
		if (errstr != NULL) {
			xfree(arg2);
			return (1);
		}

		xfree(arg2);
		*s = arg_parse_session(NULL);
		return (0);
	}

	/* If missing, try as an index, else look up immediately. */
	if (ptr == NULL) {
		*idx = strtonum(arg2, 0, INT_MAX, &errstr);
		if (errstr == NULL) {
			/* This is good as an index; use current session. */
			xfree(arg2);
			*s = arg_parse_session(NULL);
			return (0);
		}

		*idx = -1;
		goto lookup;
	}

	/* If last, strip it and look up as a session. */
	if (ptr[1] == '\0') {
		*ptr = '\0';
		goto lookup;
	}

	/* Present but not first and not last. Break and convert both. */
	*ptr = '\0';
	*idx = strtonum(ptr + 1, 0, INT_MAX, &errstr);
	if (errstr != NULL) {
		xfree(arg2);
		return (1);
	}

lookup:
	/* Look up as session. */
	*s = arg_parse_session(arg2);
	xfree(arg2);
	if (*s == NULL)
		return (1);
	return (0);
}
