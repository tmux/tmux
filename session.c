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

struct sessions		sessions;
u_int			next_session_id;
struct session_groups	session_groups = RB_INITIALIZER(&session_groups);

static void	session_free(int, short, void *);
static void	session_lock_timer(int, short, void *);
static struct winlink *session_next_alert(struct winlink *);
static struct winlink *session_previous_alert(struct winlink *);
static void	session_group_remove(struct session *);
static void	session_group_synchronize1(struct session *, struct session *);

int
session_cmp(struct session *s1, struct session *s2)
{
	return (strcmp(s1->name, s2->name));
}
RB_GENERATE(sessions, session, entry, session_cmp);

int
session_group_cmp(struct session_group *s1, struct session_group *s2)
{
	return (strcmp(s1->name, s2->name));
}
RB_GENERATE(session_groups, session_group, entry, session_group_cmp);

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
session_create(const char *prefix, const char *name, const char *cwd,
    struct environ *env, struct options *oo, struct termios *tio)
{
	struct session	*s;

	s = xcalloc(1, sizeof *s);
	s->references = 1;
	s->flags = 0;

	s->cwd = xstrdup(cwd);

	TAILQ_INIT(&s->lastw);
	RB_INIT(&s->windows);

	s->environ = env;
	s->options = oo;

	status_update_cache(s);

	s->tio = NULL;
	if (tio != NULL) {
		s->tio = xmalloc(sizeof *s->tio);
		memcpy(s->tio, tio, sizeof *s->tio);
	}

	if (name != NULL) {
		s->name = xstrdup(name);
		s->id = next_session_id++;
	} else {
		do {
			s->id = next_session_id++;
			free(s->name);
			if (prefix != NULL)
				xasprintf(&s->name, "%s-%u", prefix, s->id);
			else
				xasprintf(&s->name, "%u", s->id);
		} while (RB_FIND(sessions, &sessions, s) != NULL);
	}
	RB_INSERT(sessions, &sessions, s);

	log_debug("new session %s $%u", s->name, s->id);

	if (gettimeofday(&s->creation_time, NULL) != 0)
		fatal("gettimeofday failed");
	session_update_activity(s, &s->creation_time);

	return (s);
}

/* Add a reference to a session. */
void
session_add_ref(struct session *s, const char *from)
{
	s->references++;
	log_debug("%s: %s %s, now %d", __func__, s->name, from, s->references);
}

/* Remove a reference from a session. */
void
session_remove_ref(struct session *s, const char *from)
{
	s->references--;
	log_debug("%s: %s %s, now %d", __func__, s->name, from, s->references);

	if (s->references == 0)
		event_once(-1, EV_TIMEOUT, session_free, s, NULL);
}

/* Free session. */
static void
session_free(__unused int fd, __unused short events, void *arg)
{
	struct session	*s = arg;

	log_debug("session %s freed (%d references)", s->name, s->references);

	if (s->references == 0) {
		environ_free(s->environ);
		options_free(s->options);

		free(s->name);
		free(s);
	}
}

/* Destroy a session. */
void
session_destroy(struct session *s, int notify, const char *from)
{
	struct winlink	*wl;

	log_debug("session %s destroyed (%s)", s->name, from);

	if (s->curw == NULL)
		return;
	s->curw = NULL;

	RB_REMOVE(sessions, &sessions, s);
	if (notify)
		notify_session("session-closed", s);

	free(s->tio);

	if (event_initialized(&s->lock_timer))
		event_del(&s->lock_timer);

	session_group_remove(s);

	while (!TAILQ_EMPTY(&s->lastw))
		winlink_stack_remove(&s->lastw, TAILQ_FIRST(&s->lastw));
	while (!RB_EMPTY(&s->windows)) {
		wl = RB_ROOT(&s->windows);
		notify_session_window("window-unlinked", s, wl->window);
		winlink_remove(&s->windows, wl);
	}

	free((void *)s->cwd);

	session_remove_ref(s, __func__);
}

/* Sanitize session name. */
char *
session_check_name(const char *name)
{
	char	*copy, *cp, *new_name;

	if (*name == '\0')
		return (NULL);
	copy = xstrdup(name);
	for (cp = copy; *cp != '\0'; cp++) {
		if (*cp == ':' || *cp == '.')
			*cp = '_';
	}
	utf8_stravis(&new_name, copy, VIS_OCTAL|VIS_CSTYLE|VIS_TAB|VIS_NL);
	free(copy);
	return (new_name);
}

/* Lock session if it has timed out. */
static void
session_lock_timer(__unused int fd, __unused short events, void *arg)
{
	struct session	*s = arg;

	if (s->attached == 0)
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
	struct timeval	 tv;

	if (from == NULL)
		gettimeofday(&s->activity_time, NULL);
	else
		memcpy(&s->activity_time, from, sizeof s->activity_time);

	log_debug("session $%u %s activity %lld.%06d", s->id,
	    s->name, (long long)s->activity_time.tv_sec,
	    (int)s->activity_time.tv_usec);

	if (evtimer_initialized(&s->lock_timer))
		evtimer_del(&s->lock_timer);
	else
		evtimer_set(&s->lock_timer, session_lock_timer, s);

	if (s->attached != 0) {
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

/* Attach a window to a session. */
struct winlink *
session_attach(struct session *s, struct window *w, int idx, char **cause)
{
	struct winlink	*wl;

	if ((wl = winlink_add(&s->windows, idx)) == NULL) {
		xasprintf(cause, "index in use: %d", idx);
		return (NULL);
	}
	wl->session = s;
	winlink_set_window(wl, w);
	notify_session_window("window-linked", s, w);

	session_group_synchronize_from(s);
	return (wl);
}

/* Detach a window from a session. */
int
session_detach(struct session *s, struct winlink *wl)
{
	if (s->curw == wl &&
	    session_last(s) != 0 &&
	    session_previous(s, 0) != 0)
		session_next(s, 0);

	wl->flags &= ~WINLINK_ALERTFLAGS;
	notify_session_window("window-unlinked", s, wl->window);
	winlink_stack_remove(&s->lastw, wl);
	winlink_remove(&s->windows, wl);

	session_group_synchronize_from(s);

	if (RB_EMPTY(&s->windows))
		return (1);
       	return (0);
}

/* Return if session has window. */
int
session_has(struct session *s, struct window *w)
{
	struct winlink	*wl;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session == s)
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

	if ((sg = session_group_contains(s)) != NULL)
		return (w->references != session_group_count(sg));
	return (w->references != 1);
}

static struct winlink *
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

static struct winlink *
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
	struct winlink	*old = s->curw;

	if (wl == NULL)
		return (-1);
	if (wl == s->curw)
		return (1);

	winlink_stack_remove(&s->lastw, wl);
	winlink_stack_push(&s->lastw, s->curw);
	s->curw = wl;
	if (options_get_number(global_options, "focus-events")) {
		if (old != NULL)
			window_update_focus(old->window);
		window_update_focus(wl->window);
	}
	winlink_clear_flags(wl);
	window_update_activity(wl->window);
	tty_update_window_offset(wl->window);
	notify_session("session-window-changed", s);
	return (0);
}

/* Find the session group containing a session. */
struct session_group *
session_group_contains(struct session *target)
{
	struct session_group	*sg;
	struct session		*s;

	RB_FOREACH(sg, session_groups, &session_groups) {
		TAILQ_FOREACH(s, &sg->sessions, gentry) {
			if (s == target)
				return (sg);
		}
	}
	return (NULL);
}

/* Find session group by name. */
struct session_group *
session_group_find(const char *name)
{
	struct session_group	sg;

	sg.name = name;
	return (RB_FIND(session_groups, &session_groups, &sg));
}

/* Create a new session group. */
struct session_group *
session_group_new(const char *name)
{
	struct session_group	*sg;

	if ((sg = session_group_find(name)) != NULL)
		return (sg);

	sg = xcalloc(1, sizeof *sg);
	sg->name = xstrdup(name);
	TAILQ_INIT(&sg->sessions);

	RB_INSERT(session_groups, &session_groups, sg);
	return (sg);
}

/* Add a session to a session group. */
void
session_group_add(struct session_group *sg, struct session *s)
{
	if (session_group_contains(s) == NULL)
		TAILQ_INSERT_TAIL(&sg->sessions, s, gentry);
}

/* Remove a session from its group and destroy the group if empty. */
static void
session_group_remove(struct session *s)
{
	struct session_group	*sg;

	if ((sg = session_group_contains(s)) == NULL)
		return;
	TAILQ_REMOVE(&sg->sessions, s, gentry);
	if (TAILQ_EMPTY(&sg->sessions)) {
		RB_REMOVE(session_groups, &session_groups, sg);
		free((void *)sg->name);
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

/* Count number of clients attached to sessions in session group. */
u_int
session_group_attached_count(struct session_group *sg)
{
	struct session	*s;
	u_int		 n;

	n = 0;
	TAILQ_FOREACH(s, &sg->sessions, gentry)
		n += s->attached;
	return (n);
}

/* Synchronize a session to its session group. */
void
session_group_synchronize_to(struct session *s)
{
	struct session_group	*sg;
	struct session		*target;

	if ((sg = session_group_contains(s)) == NULL)
		return;

	target = NULL;
	TAILQ_FOREACH(target, &sg->sessions, gentry) {
		if (target != s)
			break;
	}
	if (target != NULL)
		session_group_synchronize1(target, s);
}

/* Synchronize a session group to a session. */
void
session_group_synchronize_from(struct session *target)
{
	struct session_group	*sg;
	struct session		*s;

	if ((sg = session_group_contains(target)) == NULL)
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
static void
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
		wl2->session = s;
		winlink_set_window(wl2, wl->window);
		notify_session_window("window-linked", s, wl2->window);
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
		if (wl2 != NULL) {
			TAILQ_INSERT_TAIL(&s->lastw, wl2, sentry);
			wl2->flags |= WINLINK_VISITED;
		}
	}

	/* Then free the old winlinks list. */
	while (!RB_EMPTY(&old_windows)) {
		wl = RB_ROOT(&old_windows);
		wl2 = winlink_find_by_window_id(&s->windows, wl->window->id);
		if (wl2 == NULL)
			notify_session_window("window-unlinked", s, wl->window);
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
	int			 new_idx, new_curw_idx, marked_idx = -1;

	/* Save and replace old window list. */
	memcpy(&old_wins, &s->windows, sizeof old_wins);
	RB_INIT(&s->windows);

	/* Start renumbering from the base-index if it's set. */
	new_idx = options_get_number(s->options, "base-index");
	new_curw_idx = 0;

	/* Go through the winlinks and assign new indexes. */
	RB_FOREACH(wl, winlinks, &old_wins) {
		wl_new = winlink_add(&s->windows, new_idx);
		wl_new->session = s;
		winlink_set_window(wl_new, wl->window);
		wl_new->flags |= wl->flags & WINLINK_ALERTFLAGS;

		if (wl == marked_pane.wl)
			marked_idx = wl_new->idx;
		if (wl == s->curw)
			new_curw_idx = wl_new->idx;

		new_idx++;
	}

	/* Fix the stack of last windows now. */
	memcpy(&old_lastw, &s->lastw, sizeof old_lastw);
	TAILQ_INIT(&s->lastw);
	TAILQ_FOREACH(wl, &old_lastw, sentry) {
		wl->flags &= ~WINLINK_VISITED;
		wl_new = winlink_find_by_window(&s->windows, wl->window);
		if (wl_new != NULL) {
			TAILQ_INSERT_TAIL(&s->lastw, wl_new, sentry);
			wl_new->flags |= WINLINK_VISITED;
		}
	}

	/* Set the current window. */
	if (marked_idx != -1) {
		marked_pane.wl = winlink_find_by_index(&s->windows, marked_idx);
		if (marked_pane.wl == NULL)
			server_clear_marked();
	}
	s->curw = winlink_find_by_index(&s->windows, new_curw_idx);

	/* Free the old winlinks (reducing window references too). */
	RB_FOREACH_SAFE(wl, winlinks, &old_wins, wl1)
		winlink_remove(&old_wins, wl);
}
