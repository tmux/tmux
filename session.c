/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <time.h>

#include "tmux.h"

struct sessions	sessions;
u_int		next_session_id;
struct session_groups session_groups;

void	session_free(int, short, void *);

void	session_lock_timer(int, short, void *);

struct winlink *session_next_alert(struct winlink *);
struct winlink *session_previous_alert(struct winlink *);

RB_GENERATE(sessions, session, entry, session_cmp);

int
session_cmp(struct session *s1, struct session *s2)
{
	return (strcmp(s1->name, s2->name));
}

/*
 * Find if session is still alive. This is true if it is still on the global
 * sessions list.
 */
int
session_alive(struct session *s)
{
	struct session *s_loop;

	RB_FOREACH(s_loop, sessions, &sessions) {
		if (s_loop == s)
			return (1);
	}
	return (0);
}

/* Find session by name. */
struct session *
session_find(const char *name)
{
	struct session	s;

	s.name = (char *) name;
	return (RB_FIND(sessions, &sessions, &s));
}

/* Find session by id parsed from a string. */
struct session *
session_find_by_id_str(const char *s)
{
	const char	*errstr;
	u_int		 id;

	if (*s != '$')
		return (NULL);

	id = strtonum(s + 1, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return (NULL);
	return (session_find_by_id(id));
}

/* Find session by id. */
struct session *
session_find_by_id(u_int id)
{
	struct session	*s;

	RB_FOREACH(s, sessions, &sessions) {
		if (s->id == id)
			return (s);
	}
	return (NULL);
}

/* Create a new session. */
struct session *
session_create(const char *name, int argc, char **argv, const char *path,
    const char *cwd, struct environ *env, struct termios *tio, int idx,
    u_int sx, u_int sy, char **cause)
{
	struct session	*s;
	struct winlink	*wl;

	s = xcalloc(1, sizeof *s);
	s->references = 1;
	s->flags = 0;

	s->cwd = xstrdup(cwd);

	s->curw = NULL;
	TAILQ_INIT(&s->lastw);
	RB_INIT(&s->windows);

	s->environ = environ_create();
	if (env != NULL)
		environ_copy(env, s->environ);

	s->options = options_create(global_s_options);
	s->hooks = hooks_create(global_hooks);

	s->tio = NULL;
	if (tio != NULL) {
		s->tio = xmalloc(sizeof *s->tio);
		memcpy(s->tio, tio, sizeof *s->tio);
	}

	s->sx = sx;
	s->sy = sy;

	if (name != NULL) {
		s->name = xstrdup(name);
		s->id = next_session_id++;
	} else {
		s->name = NULL;
		do {
			s->id = next_session_id++;
			free(s->name);
			xasprintf(&s->name, "%u", s->id);
		} while (RB_FIND(sessions, &sessions, s) != NULL);
	}
	RB_INSERT(sessions, &sessions, s);

	log_debug("new session %s $%u", s->name, s->id);

	if (gettimeofday(&s->creation_time, NULL) != 0)
		fatal("gettimeofday failed");
	session_update_activity(s, &s->creation_time);

	if (argc >= 0) {
		wl = session_new(s, NULL, argc, argv, path, cwd, idx, cause);
		if (wl == NULL) {
			session_destroy(s);
			return (NULL);
		}
		session_select(s, RB_ROOT(&s->windows)->idx);
	}

	log_debug("session %s created", s->name);
	notify_session_created(s);

	return (s);
}

/* Remove a reference from a session. */
void
session_unref(struct session *s)
{
	log_debug("session %s has %d references", s->name, s->references);

	s->references--;
	if (s->references == 0)
		event_once(-1, EV_TIMEOUT, session_free, s, NULL);
}

/* Free session. */
void
session_free(__unused int fd, __unused short events, void *arg)
{
	struct session	*s = arg;

	log_debug("session %s freed (%d references)", s->name, s->references);

	if (s->references == 0) {
		environ_free(s->environ);

		options_free(s->options);
		hooks_free(s->hooks);

		free(s->name);
		free(s);
	}
}

/* Destroy a session. */
void
session_destroy(struct session *s)
{
	struct winlink	*wl;

	log_debug("session %s destroyed", s->name);

	RB_REMOVE(sessions, &sessions, s);
	notify_session_closed(s);

	free(s->tio);

	if (event_initialized(&s->lock_timer))
		event_del(&s->lock_timer);

	session_group_remove(s);

	while (!TAILQ_EMPTY(&s->lastw))
		winlink_stack_remove(&s->lastw, TAILQ_FIRST(&s->lastw));
	while (!RB_EMPTY(&s->windows)) {
		wl = RB_ROOT(&s->windows);
		notify_window_unlinked(s, wl->window);
		winlink_remove(&s->windows, wl);
	}

	free((void *)s->cwd);

	session_unref(s);
}

/* Check a session name is valid: not empty and no colons or periods. */
int
session_check_name(const char *name)
{
	return (*name != '\0' && name[strcspn(name, ":.")] == '\0');
}

/* Lock session if it has timed out. */
void
session_lock_timer(__unused int fd, __unused short events, void *arg)
{
	struct session	*s = arg;

	if (s->flags & SESSION_UNATTACHED)
		return;

	log_debug("session %s locked, activity time %lld", s->name,
	    (long long)s->activity_time.tv_sec);

	server_lock_session(s);
	recalculate_sizes();
}

/* Update activity time. */
void
session_update_activity(struct session *s, struct timeval *from)
{
	struct timeval	*last = &s->last_activity_time;
	struct timeval	 tv;

	memcpy(last, &s->activity_time, sizeof *last);
	if (from == NULL)
		gettimeofday(&s->activity_time, NULL);
	else
		memcpy(&s->activity_time, from, sizeof s->activity_time);

	log_debug("session %s activity %lld.%06d (last %lld.%06d)", s->name,
	    (long long)s->activity_time.tv_sec, (int)s->activity_time.tv_usec,
	    (long long)last->tv_sec, (int)last->tv_usec);

	if (evtimer_initialized(&s->lock_timer))
		evtimer_del(&s->lock_timer);
	else
		evtimer_set(&s->lock_timer, session_lock_timer, s);

	if (~s->flags & SESSION_UNATTACHED) {
		timerclear(&tv);
		tv.tv_sec = options_get_number(s->options, "lock-after-time");
		if (tv.tv_sec != 0)
			evtimer_add(&s->lock_timer, &tv);
	}
}

/* Find the next usable session. */
struct session *
session_next_session(struct session *s)
{
	struct session *s2;

	if (RB_EMPTY(&sessions) || !session_alive(s))
		return (NULL);

	s2 = RB_NEXT(sessions, &sessions, s);
	if (s2 == NULL)
		s2 = RB_MIN(sessions, &sessions);
	if (s2 == s)
		return (NULL);
	return (s2);
}

/* Find the previous usable session. */
struct session *
session_previous_session(struct session *s)
{
	struct session *s2;

	if (RB_EMPTY(&sessions) || !session_alive(s))
		return (NULL);

	s2 = RB_PREV(sessions, &sessions, s);
	if (s2 == NULL)
		s2 = RB_MAX(sessions, &sessions);
	if (s2 == s)
		return (NULL);
	return (s2);
}

/* Create a new window on a session. */
struct winlink *
session_new(struct session *s, const char *name, int argc, char **argv,
    const char *path, const char *cwd, int idx, char **cause)
{
	struct window	*w;
	struct winlink	*wl;
	struct environ	*env;
	const char	*shell;
	u_int		 hlimit;

	if ((wl = winlink_add(&s->windows, idx)) == NULL) {
		xasprintf(cause, "index in use: %d", idx);
		return (NULL);
	}

	env = environ_create();
	environ_copy(global_environ, env);
	environ_copy(s->environ, env);
	server_fill_environ(s, env);

	shell = options_get_string(s->options, "default-shell");
	if (*shell == '\0' || areshell(shell))
		shell = _PATH_BSHELL;

	hlimit = options_get_number(s->options, "history-limit");
	w = window_create(name, argc, argv, path, shell, cwd, env, s->tio,
	    s->sx, s->sy, hlimit, cause);
	if (w == NULL) {
		winlink_remove(&s->windows, wl);
		environ_free(env);
		return (NULL);
	}
	winlink_set_window(wl, w);
	notify_window_linked(s, w);
	environ_free(env);

	if (options_get_number(s->options, "set-remain-on-exit"))
		options_set_number(w->options, "remain-on-exit", 1);

	session_group_synchronize_from(s);
	return (wl);
}

/* Attach a window to a session. */
struct winlink *
session_attach(struct session *s, struct window *w, int idx, char **cause)
{
	struct winlink	*wl;

	if ((wl = winlink_add(&s->windows, idx)) == NULL) {
		xasprintf(cause, "index in use: %d", idx);
		return (NULL);
	}
	winlink_set_window(wl, w);
	notify_window_linked(s, w);

	session_group_synchronize_from(s);
	return (wl);
}

/* Detach a window from a session. */
int
session_detach(struct session *s, struct winlink *wl)
{
	if (s->curw == wl &&
	    session_last(s) != 0 && session_previous(s, 0) != 0)
		session_next(s, 0);

	wl->flags &= ~WINLINK_ALERTFLAGS;
	notify_window_unlinked(s, wl->window);
	winlink_stack_remove(&s->lastw, wl);
	winlink_remove(&s->windows, wl);
	session_group_synchronize_from(s);
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

/*
 * Return 1 if a window is linked outside this session (not including session
 * groups). The window must be in this session!
 */
int
session_is_linked(struct session *s, struct window *w)
{
	struct session_group	*sg;

	if ((sg = session_group_find(s)) != NULL)
		return (w->references != session_group_count(sg));
	return (w->references != 1);
}

struct winlink *
session_next_alert(struct winlink *wl)
{
	while (wl != NULL) {
		if (wl->flags & WINLINK_ALERTFLAGS)
			break;
		wl = winlink_next(wl);
	}
	return (wl);
}

/* Move session to next window. */
int
session_next(struct session *s, int alert)
{
	struct winlink	*wl;

	if (s->curw == NULL)
		return (-1);

	wl = winlink_next(s->curw);
	if (alert)
		wl = session_next_alert(wl);
	if (wl == NULL) {
		wl = RB_MIN(winlinks, &s->windows);
		if (alert && ((wl = session_next_alert(wl)) == NULL))
			return (-1);
	}
	return (session_set_current(s, wl));
}

struct winlink *
session_previous_alert(struct winlink *wl)
{
	while (wl != NULL) {
		if (wl->flags & WINLINK_ALERTFLAGS)
			break;
		wl = winlink_previous(wl);
	}
	return (wl);
}

/* Move session to previous window. */
int
session_previous(struct session *s, int alert)
{
	struct winlink	*wl;

	if (s->curw == NULL)
		return (-1);

	wl = winlink_previous(s->curw);
	if (alert)
		wl = session_previous_alert(wl);
	if (wl == NULL) {
		wl = RB_MAX(winlinks, &s->windows);
		if (alert && (wl = session_previous_alert(wl)) == NULL)
			return (-1);
	}
	return (session_set_current(s, wl));
}

/* Move session to specific window. */
int
session_select(struct session *s, int idx)
{
	struct winlink	*wl;

	wl = winlink_find_by_index(&s->windows, idx);
	return (session_set_current(s, wl));
}

/* Move session to last used window. */
int
session_last(struct session *s)
{
	struct winlink	*wl;

	wl = TAILQ_FIRST(&s->lastw);
	if (wl == NULL)
		return (-1);
	if (wl == s->curw)
		return (1);

	return (session_set_current(s, wl));
}

/* Set current winlink to wl .*/
int
session_set_current(struct session *s, struct winlink *wl)
{
	if (wl == NULL)
		return (-1);
	if (wl == s->curw)
		return (1);

	winlink_stack_remove(&s->lastw, wl);
	winlink_stack_push(&s->lastw, s->curw);
	s->curw = wl;
	winlink_clear_flags(wl);
	window_update_activity(wl->window);
	return (0);
}

/* Find the session group containing a session. */
struct session_group *
session_group_find(struct session *target)
{
	struct session_group	*sg;
	struct session		*s;

	TAILQ_FOREACH(sg, &session_groups, entry) {
		TAILQ_FOREACH(s, &sg->sessions, gentry) {
			if (s == target)
				return (sg);
		}
	}
	return (NULL);
}

/* Find session group index. */
u_int
session_group_index(struct session_group *sg)
{
	struct session_group   *sg2;
	u_int			i;

	i = 0;
	TAILQ_FOREACH(sg2, &session_groups, entry) {
		if (sg == sg2)
			return (i);
		i++;
	}

	fatalx("session group not found");
}

/*
 * Add a session to the session group containing target, creating it if
 * necessary.
 */
void
session_group_add(struct session *target, struct session *s)
{
	struct session_group	*sg;

	if ((sg = session_group_find(target)) == NULL) {
		sg = xmalloc(sizeof *sg);
		TAILQ_INSERT_TAIL(&session_groups, sg, entry);
		TAILQ_INIT(&sg->sessions);
		TAILQ_INSERT_TAIL(&sg->sessions, target, gentry);
	}
	TAILQ_INSERT_TAIL(&sg->sessions, s, gentry);
}

/* Remove a session from its group and destroy the group if empty. */
void
session_group_remove(struct session *s)
{
	struct session_group	*sg;

	if ((sg = session_group_find(s)) == NULL)
		return;
	TAILQ_REMOVE(&sg->sessions, s, gentry);
	if (TAILQ_NEXT(TAILQ_FIRST(&sg->sessions), gentry) == NULL)
		TAILQ_REMOVE(&sg->sessions, TAILQ_FIRST(&sg->sessions), gentry);
	if (TAILQ_EMPTY(&sg->sessions)) {
		TAILQ_REMOVE(&session_groups, sg, entry);
		free(sg);
	}
}

/* Count number of sessions in session group. */
u_int
session_group_count(struct session_group *sg)
{
	struct session	*s;
	u_int		 n;

	n = 0;
	TAILQ_FOREACH(s, &sg->sessions, gentry)
	    n++;
	return (n);
}

/* Synchronize a session to its session group. */
void
session_group_synchronize_to(struct session *s)
{
	struct session_group	*sg;
	struct session		*target;

	if ((sg = session_group_find(s)) == NULL)
		return;

	target = NULL;
	TAILQ_FOREACH(target, &sg->sessions, gentry) {
		if (target != s)
			break;
	}
	session_group_synchronize1(target, s);
}

/* Synchronize a session group to a session. */
void
session_group_synchronize_from(struct session *target)
{
	struct session_group	*sg;
	struct session		*s;

	if ((sg = session_group_find(target)) == NULL)
		return;

	TAILQ_FOREACH(s, &sg->sessions, gentry) {
		if (s != target)
			session_group_synchronize1(target, s);
	}
}

/*
 * Synchronize a session with a target session. This means destroying all
 * winlinks then recreating them, then updating the current window, last window
 * stack and alerts.
 */
void
session_group_synchronize1(struct session *target, struct session *s)
{
	struct winlinks		 old_windows, *ww;
	struct winlink_stack	 old_lastw;
	struct winlink		*wl, *wl2;

	/* Don't do anything if the session is empty (it'll be destroyed). */
	ww = &target->windows;
	if (RB_EMPTY(ww))
		return;

	/* If the current window has vanished, move to the next now. */
	if (s->curw != NULL &&
	    winlink_find_by_index(ww, s->curw->idx) == NULL &&
	    session_last(s) != 0 && session_previous(s, 0) != 0)
		session_next(s, 0);

	/* Save the old pointer and reset it. */
	memcpy(&old_windows, &s->windows, sizeof old_windows);
	RB_INIT(&s->windows);

	/* Link all the windows from the target. */
	RB_FOREACH(wl, winlinks, ww) {
		wl2 = winlink_add(&s->windows, wl->idx);
		winlink_set_window(wl2, wl->window);
		notify_window_linked(s, wl2->window);
		wl2->flags |= wl->flags & WINLINK_ALERTFLAGS;
	}

	/* Fix up the current window. */
	if (s->curw != NULL)
		s->curw = winlink_find_by_index(&s->windows, s->curw->idx);
	else
		s->curw = winlink_find_by_index(&s->windows, target->curw->idx);

	/* Fix up the last window stack. */
	memcpy(&old_lastw, &s->lastw, sizeof old_lastw);
	TAILQ_INIT(&s->lastw);
	TAILQ_FOREACH(wl, &old_lastw, sentry) {
		wl2 = winlink_find_by_index(&s->windows, wl->idx);
		if (wl2 != NULL)
			TAILQ_INSERT_TAIL(&s->lastw, wl2, sentry);
	}

	/* Then free the old winlinks list. */
	while (!RB_EMPTY(&old_windows)) {
		wl = RB_ROOT(&old_windows);
		wl2 = winlink_find_by_window_id(&s->windows, wl->window->id);
		if (wl2 == NULL)
			notify_window_unlinked(s, wl->window);
		winlink_remove(&old_windows, wl);
	}
}

/* Renumber the windows across winlinks attached to a specific session. */
void
session_renumber_windows(struct session *s)
{
	struct winlink		*wl, *wl1, *wl_new;
	struct winlinks		 old_wins;
	struct winlink_stack	 old_lastw;
	int			 new_idx, new_curw_idx;

	/* Save and replace old window list. */
	memcpy(&old_wins, &s->windows, sizeof old_wins);
	RB_INIT(&s->windows);

	/* Start renumbering from the base-index if it's set. */
	new_idx = options_get_number(s->options, "base-index");
	new_curw_idx = 0;

	/* Go through the winlinks and assign new indexes. */
	RB_FOREACH(wl, winlinks, &old_wins) {
		wl_new = winlink_add(&s->windows, new_idx);
		winlink_set_window(wl_new, wl->window);
		wl_new->flags |= wl->flags & WINLINK_ALERTFLAGS;

		if (wl == s->curw)
			new_curw_idx = wl_new->idx;

		new_idx++;
	}

	/* Fix the stack of last windows now. */
	memcpy(&old_lastw, &s->lastw, sizeof old_lastw);
	TAILQ_INIT(&s->lastw);
	TAILQ_FOREACH(wl, &old_lastw, sentry) {
		wl_new = winlink_find_by_window(&s->windows, wl->window);
		if (wl_new != NULL)
			TAILQ_INSERT_TAIL(&s->lastw, wl_new, sentry);
	}

	/* Set the current window. */
	s->curw = winlink_find_by_index(&s->windows, new_curw_idx);

	/* Free the old winlinks (reducing window references too). */
	RB_FOREACH_SAFE(wl, winlinks, &old_wins, wl1)
		winlink_remove(&old_wins, wl);
}
