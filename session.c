/* $Id: session.c,v 1.10 2007-09-21 18:16:31 nicm Exp $ */

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

#include "tmux.h"

/* Global session list. */
struct sessions	sessions;	

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

	if (session_new(s, cmd, sx, sy) != 0) {
		xfree(s);
		return (NULL);
	}

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if (ARRAY_ITEM(&sessions, i) == NULL) {
			ARRAY_SET(&sessions, i, s);
			break;
		}
	}
	if (i == ARRAY_LENGTH(&sessions))
		ARRAY_ADD(&sessions, s);

	if (*name != '\0')
		strlcpy(s->name, name, sizeof s->name);
	else
		xsnprintf(s->name, sizeof s->name, "%u", i);

	return (s);
}

/* Destroy a session. */
void
session_destroy(struct session *s)
{
	u_int	i;

	if (session_index(s, &i) != 0)
		fatalx("session not found");
	ARRAY_REMOVE(&sessions, i);

	while (!ARRAY_EMPTY(&s->windows))
		window_remove(&s->windows, ARRAY_FIRST(&s->windows));

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
session_new(struct session *s, const char *cmd, u_int sx, u_int sy)
{
	struct window	*w;

	if ((w = window_create(cmd, sx, sy)) == NULL)
		return (-1);
	session_attach(s, w);

	s->last = s->window;
	s->window = w;
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
	/* Move to last, previous of next window if possible. */
	if (s->window == w && session_last(s) != 0 && session_previous(s) != 0)
		session_next(s);

	/* Remove the window from the list. */
	window_remove(&s->windows, w);

	/* Destroy session if it is empty. */
	if (!ARRAY_EMPTY(&s->windows))
		return (0);
	session_destroy(s);
	return (1);
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
	s->last = s->window;
	s->window = w;
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
	s->last = s->window;
	s->window = w;
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
	s->last = s->window;
	s->window = w;
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
	return (0);
}
