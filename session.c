/* $Id: session.c,v 1.25 2007-10-26 12:29:07 nicm Exp $ */

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
session_cancelbell(struct session *s, struct winlink *wl)
{
	u_int	i;

	for (i = 0; i < ARRAY_LENGTH(&s->bells); i++) {
		if (ARRAY_ITEM(&s->bells, i) == wl) {
			ARRAY_REMOVE(&s->bells, i);
			break;
		}
	}
}

void
session_addbell(struct session *s, struct window *w)
{
	struct winlink	*wl;

	RB_FOREACH(wl, winlinks, &s->windows) {
		if (wl == s->curw)
			continue;
		if (wl->window == w && !session_hasbell(s, wl))
			ARRAY_ADD(&s->bells, wl);
	}
}

int
session_hasbell(struct session *s, struct winlink *wl)
{
	u_int	i;

	for (i = 0; i < ARRAY_LENGTH(&s->bells); i++) {
		if (ARRAY_ITEM(&s->bells, i) == wl)
			return (1);
	}
	return (0);
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
	s->curw = s->lastw = NULL;
	RB_INIT(&s->windows);
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
	if (session_new(s, NULL, cmd, -1) == NULL) {
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
	struct winlink	*wl;
	u_int		 i;

	if (session_index(s, &i) != 0)
		fatalx("session not found");
	ARRAY_SET(&sessions, i, NULL);
	while (!ARRAY_EMPTY(&sessions) && ARRAY_LAST(&sessions) == NULL)
		ARRAY_TRUNC(&sessions, 1);
	
	while (!RB_EMPTY(&s->windows)) {
		wl = RB_ROOT(&s->windows);
		RB_REMOVE(winlinks, &s->windows, wl);
		winlink_remove(&s->windows, wl);
	}

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
struct winlink *
session_new(struct session *s, const char *name, const char *cmd, int idx)
{
	struct window	*w;
	const char	*environ[] = { NULL, "TERM=screen", NULL };
	char		 buf[256];
	u_int		 i;

	if (session_index(s, &i) != 0)
		fatalx("session not found");
	xsnprintf(buf, sizeof buf, "TMUX=%ld,%u", (long) getpid(), i);
	environ[0] = buf;
	
	if ((w = window_create(name, cmd, environ, s->sx, s->sy)) == NULL)
		return (NULL);
	return (session_attach(s, w, idx));
}

/* Attach a window to a session. */
struct winlink *
session_attach(struct session *s, struct window *w, int idx)
{
	return (winlink_add(&s->windows, w, idx));
}

/* Detach a window from a session. */
int
session_detach(struct session *s, struct winlink *wl)
{
	if (s->curw == wl && session_last(s) != 0 && session_previous(s) != 0)
		session_next(s);
	if (s->lastw == wl)
		s->lastw = NULL;

	session_cancelbell(s, wl);
	winlink_remove(&s->windows, wl);
	if (RB_EMPTY(&s->windows)) {
		session_destroy(s);
		return (1);
	}
	return (0);
}

/* Return if session has window. */
int
session_has(struct session *s, struct window *w)
{
	struct winlink	*wl;

	RB_FOREACH(wl, winlinks, &s->windows) {
		if (wl->window == w)
			return (1);
	}
	return (0);
}

/* Move session to next window. */
int
session_next(struct session *s)
{
	struct winlink	*wl;

	if (s->curw == NULL)
		return (-1);

	wl = winlink_next(&s->windows, s->curw);
	if (wl == NULL)
		wl = RB_MIN(winlinks, &s->windows);
	if (wl == s->curw)
		return (1);
	s->lastw = s->curw;
	s->curw = wl;
	session_cancelbell(s, wl);
	return (0);
}

/* Move session to previous window. */ 
int
session_previous(struct session *s)
{
	struct winlink	*wl;

	if (s->curw == NULL)
		return (-1);

	wl = winlink_previous(&s->windows, s->curw);
	if (wl == NULL)
		wl = RB_MAX(winlinks, &s->windows);
	if (wl == s->curw)
		return (1);
	s->lastw = s->curw;
	s->curw = wl;
	session_cancelbell(s, wl);
	return (0);
}

/* Move session to specific window. */ 
int
session_select(struct session *s, u_int i)
{
	struct winlink	*wl;

	wl = winlink_find_by_index(&s->windows, i);
	if (wl == NULL)
		return (-1);
	if (wl == s->curw)
		return (1);
	s->lastw = s->curw;
	s->curw = wl;
	session_cancelbell(s, wl);
	return (0);
}

/* Move session to last used window. */ 
int
session_last(struct session *s)
{
	struct winlink	*wl;

	wl = s->lastw;
	if (wl == NULL)
		return (-1);
	if (wl == s->curw) 
		return (1);

	s->lastw = s->curw;
	s->curw = wl;
	session_cancelbell(s, wl);
	return (0);
}
