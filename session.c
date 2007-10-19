/* $Id: session.c,v 1.23 2007-10-19 22:16:53 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "tmux.h"

/* Global session list. */
struct sessions	sessions;	

void
session_cancelbell(struct session *s, struct window *w)
{
	u_int	i;

	if (window_index(&s->bells, w, &i) == 0)
		window_remove(&s->bells, w);
}

void
session_addbell(struct session *s, struct window *w)
{
	u_int	i;

	/* Never bell in the current window. */
	if (w == s->window || !session_has(s, w))
		return;

	if (window_index(&s->bells, w, &i) != 0)
		window_add(&s->bells, w);
}

int
session_hasbell(struct session *s, struct window *w)
{
	u_int	i;

	return (window_index(&s->bells, w, &i) == 0);
}

/* Find session by name. */
struct session *
session_find(const char *name)
{
	struct session	*s;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s != NULL && strcmp(s->name, name) == 0)
			return (s);
	}
	
	return (NULL);
}

/* Create a new session. */
struct session *
session_create(const char *name, const char *cmd, u_int sx, u_int sy)
{
	struct session	*s;
	u_int		 i;

	s = xmalloc(sizeof *s);
	s->tim = time(NULL);
	s->window = s->last = NULL;
	ARRAY_INIT(&s->windows);
	ARRAY_INIT(&s->bells);

	s->sx = sx;
	s->sy = sy;

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if (ARRAY_ITEM(&sessions, i) == NULL) {
			ARRAY_SET(&sessions, i, s);
			break;
		}
	}
	if (i == ARRAY_LENGTH(&sessions))
		ARRAY_ADD(&sessions, s);

	if (name != NULL)
		s->name = xstrdup(name);
	else
		xasprintf(&s->name, "%u", i);
	if (session_new(s, NULL, cmd, &i) != 0) {
		session_destroy(s);
		return (NULL);
	}
	session_select(s, i);

	return (s);
}

/* Destroy a session. */
void
session_destroy(struct session *s)
{
	u_int	i;

	if (session_index(s, &i) != 0)
		fatalx("session not found");
	ARRAY_SET(&sessions, i, NULL);
	while (!ARRAY_EMPTY(&sessions) && ARRAY_LAST(&sessions) == NULL)
		ARRAY_TRUNC(&sessions, 1);
	
	while (!ARRAY_EMPTY(&s->windows))
		window_remove(&s->windows, ARRAY_FIRST(&s->windows));

	xfree(s->name);
	xfree(s);
}

/* Find session index. */
int
session_index(struct session *s, u_int *i)
{
	for (*i = 0; *i < ARRAY_LENGTH(&sessions); (*i)++) {
		if (s == ARRAY_ITEM(&sessions, *i))
			return (0);
	}
	return (-1);
}

/* Create a new window on a session. */
int
session_new(struct session *s, const char *name, const char *cmd, u_int *i)
{
	struct window	*w;
	const char	*environ[] = { NULL, "TERM=screen", NULL };
	char		 buf[256];

	if (session_index(s, i) != 0)
		fatalx("session not found");
	xsnprintf(buf, sizeof buf, "TMUX=%ld,%u", (long) getpid(), *i);
	environ[0] = buf;
	
	if ((w = window_create(name, cmd, environ, s->sx, s->sy)) == NULL)
		return (-1);
	session_attach(s, w);
	
	window_index(&s->windows, w, i);
	return (0);
}

/* Attach a window to a session. */
void
session_attach(struct session *s, struct window *w)
{
	window_add(&s->windows, w);
}

/* Detach a window from a session. */
int
session_detach(struct session *s, struct window *w)
{
	if (s->window == w && session_last(s) != 0 && session_previous(s) != 0)
		session_next(s);
	if (s->last == w)
		s->last = NULL;

	window_remove(&s->windows, w);
	if (ARRAY_EMPTY(&s->windows)) {
		session_destroy(s);
		return (1);
	}
	return (0);
}

/* Return if session has window. */
int
session_has(struct session *s, struct window *w)
{
	u_int	i;

	return (window_index(&s->windows, w, &i) == 0);
}

/* Move session to next window. */
int
session_next(struct session *s)
{
	struct window	*w;
	u_int            n;

	if (s->window == NULL)
		return (-1);

	w = window_next(&s->windows, s->window);
	if (w == NULL) {
		n = 0;
		while ((w = ARRAY_ITEM(&s->windows, n)) == NULL)
			n++;
		if (w == s->window)
			return (1);
	}
	if (w == s->window)
		return (0);
	s->last = s->window;
	s->window = w;
	session_cancelbell(s, w);
	return (0);
}

/* Move session to previous window. */ 
int
session_previous(struct session *s)
{
	struct window	*w;

	if (s->window == NULL)
		return (-1);

	w = window_previous(&s->windows, s->window);
	if (w == NULL) {
		w = ARRAY_LAST(&s->windows);
		if (w == s->window)
			return (1);
	}
	if (w == s->window)
		return (0);
	s->last = s->window;
	s->window = w;
	session_cancelbell(s, w);
	return (0);
}

/* Move session to specific window. */ 
int
session_select(struct session *s, u_int i)
{
	struct window	*w;

	w = window_at(&s->windows, i);
	if (w == NULL)
		return (-1);
	if (w == s->window)
		return (0);
	s->last = s->window;
	s->window = w;
	session_cancelbell(s, w);
	return (0);
}

/* Move session to last used window. */ 
int
session_last(struct session *s)
{
	struct window	*w;

	w = s->last;
	if (w == NULL)
		return (-1);
	if (w == s->window) 
		return (1);

	s->last = s->window;
	s->window = w;
	session_cancelbell(s, w);
	return (0);
}
