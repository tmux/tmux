/* $Id: session.c,v 1.48 2009-01-10 14:43:43 nicm Exp $ */

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
#include <sys/time.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

/* Global session list. */
struct sessions	sessions;

void
session_alert_cancel(struct session *s, struct winlink *wl)
{
	struct session_alert	*sa, *sb;

	sa = SLIST_FIRST(&s->alerts);
	while (sa != NULL) {
		sb = sa;
		sa = SLIST_NEXT(sa, entry);

		if (wl == NULL || sb->wl == wl) {
			SLIST_REMOVE(&s->alerts, sb, session_alert, entry);
			xfree(sb);
		}
	}
}

void
session_alert_add(struct session *s, struct window *w, int type)
{
	struct session_alert	*sa;
	struct winlink		*wl;

	RB_FOREACH(wl, winlinks, &s->windows) {
		if (wl == s->curw)
			continue;

		if (wl->window == w &&
		    !session_alert_has(s, wl, type)) {
			sa = xmalloc(sizeof *sa);
			sa->wl = wl;
			sa->type = type;
			SLIST_INSERT_HEAD(&s->alerts, sa, entry);
		}
	}
}

int
session_alert_has(struct session *s, struct winlink *wl, int type)
{
	struct session_alert	*sa;

	SLIST_FOREACH(sa, &s->alerts, entry) {
		if (sa->wl == wl && sa->type == type)
			return (1);
	}

	return (0);
}

int
session_alert_has_window(struct session *s, struct window *w, int type)
{
	struct session_alert	*sa;

	SLIST_FOREACH(sa, &s->alerts, entry) {
		if (sa->wl->window == w && sa->type == type)
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
	s->flags = 0;
	if (gettimeofday(&s->tv, NULL) != 0)
		fatal("gettimeofday");
	s->curw = NULL;
	SLIST_INIT(&s->lastw);
	RB_INIT(&s->windows);
	SLIST_INIT(&s->alerts);
	paste_init_stack(&s->buffers);
	options_init(&s->options, &global_options);

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
	session_select(s, 0);

	log_debug("session %s created", s->name);

	return (s);
}

/* Destroy a session. */
void
session_destroy(struct session *s)
{
	u_int	i;

	log_debug("session %s destroyed", s->name);

	if (session_index(s, &i) != 0)
		fatalx("session not found");
	ARRAY_SET(&sessions, i, NULL);
	while (!ARRAY_EMPTY(&sessions) && ARRAY_LAST(&sessions) == NULL)
		ARRAY_TRUNC(&sessions, 1);

	session_alert_cancel(s, NULL);
	options_free(&s->options);
	paste_free_stack(&s->buffers);

	while (!SLIST_EMPTY(&s->lastw))
		winlink_stack_remove(&s->lastw, SLIST_FIRST(&s->lastw));
	while (!RB_EMPTY(&s->windows))
		winlink_remove(&s->windows, RB_ROOT(&s->windows));

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
	const char	*env[] = { NULL /* TMUX= */, "TERM=screen", NULL };
	char		 buf[256];
	u_int		 i, hlimit;

	if (session_index(s, &i) != 0)
		fatalx("session not found");
	xsnprintf(buf, sizeof buf, "TMUX=%ld,%u", (long) getpid(), i);
	env[0] = buf;

	hlimit = options_get_number(&s->options, "history-limit");
	if ((w = window_create(name, cmd, env, s->sx, s->sy, hlimit)) == NULL)
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

	session_alert_cancel(s, wl);
	winlink_stack_remove(&s->lastw, wl);
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
	winlink_stack_remove(&s->lastw, wl);
	winlink_stack_push(&s->lastw, s->curw);
	s->curw = wl;
	session_alert_cancel(s, wl);
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
	winlink_stack_remove(&s->lastw, wl);
	winlink_stack_push(&s->lastw, s->curw);
	s->curw = wl;
	session_alert_cancel(s, wl);
	return (0);
}

/* Move session to specific window. */
int
session_select(struct session *s, int idx)
{
	struct winlink	*wl;

	wl = winlink_find_by_index(&s->windows, idx);
	if (wl == NULL)
		return (-1);
	if (wl == s->curw)
		return (1);
	winlink_stack_remove(&s->lastw, wl);
	winlink_stack_push(&s->lastw, s->curw);
	s->curw = wl;
	session_alert_cancel(s, wl);
	return (0);
}

/* Move session to last used window. */
int
session_last(struct session *s)
{
	struct winlink	*wl;

	wl = SLIST_FIRST(&s->lastw);
	if (wl == NULL)
		return (-1);
	if (wl == s->curw)
		return (1);

	winlink_stack_remove(&s->lastw, wl);
	winlink_stack_push(&s->lastw, s->curw);
	s->curw = wl;
	session_alert_cancel(s, wl);
	return (0);
}
