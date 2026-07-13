/* $OpenBSD: monitor.c,v 1.6 2026/07/10 15:20:06 nicm Exp $ */

/*
 * Copyright (c) 2026 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <event.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

/* Subscription pane. */
struct monitor_pane {
	u_int				 pane;
	u_int				 idx;
	char				*last;
	u_int				 generation;

	RB_ENTRY(monitor_pane)		 entry;
};
RB_HEAD(monitor_panes, monitor_pane);

/* Subscription window. */
struct monitor_window {
	u_int				 window;
	u_int				 idx;
	char				*last;
	u_int				 generation;

	RB_ENTRY(monitor_window)	 entry;
};
RB_HEAD(monitor_windows, monitor_window);

/* Subscription. */
struct monitor_item {
	char				*name;
	char				*format;

	enum monitor_type		 type;
	u_int				 id;
	int				 flags;

	char				*last;
	struct monitor_panes		 panes;
	struct monitor_windows		 windows;

	RB_ENTRY(monitor_item)		 entry;
};
RB_HEAD(monitor_items, monitor_item);

/* Monitored subscription set. */
struct monitor_set {
	struct client			*client;
	struct session			*session;
	monitor_cb			 cb;
	void				*data;

	struct monitor_items		 items;
	struct event			 timer;
	u_int				 generation;
};

static void	monitor_timer(__unused int, __unused short, void *);

/* Get the session for this monitor set. */
static struct session *
monitor_get_session(struct monitor_set *ms)
{
	struct session	*s;

	if (ms->client != NULL)
		return (ms->client->session);
	s = ms->session;
	if (s == NULL)
		return (RB_MIN(sessions, &sessions));
	if (session_find_by_id(s->id) != s)
		return (NULL);
	return (s);
}

/* Create a format tree for a subscription. */
static struct format_tree *
monitor_create_formats(struct client *c, struct session *s, struct winlink *wl,
    struct window_pane *wp)
{
	struct format_tree	*ft;

	ft = format_create(NULL, NULL, 0, FORMAT_NOJOBS);
	format_defaults(ft, c, s, wl, wp);
	return (ft);
}

/* Compare subscriptions. */
static int
monitor_item_cmp(struct monitor_item *m1, struct monitor_item *m2)
{
	return (strcmp(m1->name, m2->name));
}
RB_GENERATE_STATIC(monitor_items, monitor_item, entry, monitor_item_cmp);

/* Compare subscription panes. */
static int
monitor_pane_cmp(struct monitor_pane *mp1, struct monitor_pane *mp2)
{
	if (mp1->pane < mp2->pane)
		return (-1);
	if (mp1->pane > mp2->pane)
		return (1);
	if (mp1->idx < mp2->idx)
		return (-1);
	if (mp1->idx > mp2->idx)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(monitor_panes, monitor_pane, entry, monitor_pane_cmp);

/* Compare subscription windows. */
static int
monitor_window_cmp(struct monitor_window *mw1, struct monitor_window *mw2)
{
	if (mw1->window < mw2->window)
		return (-1);
	if (mw1->window > mw2->window)
		return (1);
	if (mw1->idx < mw2->idx)
		return (-1);
	if (mw1->idx > mw2->idx)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(monitor_windows, monitor_window, entry, monitor_window_cmp);

/* Free a subscription. */
static void
monitor_free_item(struct monitor_set *ms, struct monitor_item *me)
{
	struct monitor_pane	*mp, *mp1;
	struct monitor_window	*mw, *mw1;

	RB_FOREACH_SAFE(mp, monitor_panes, &me->panes, mp1) {
		RB_REMOVE(monitor_panes, &me->panes, mp);
		free(mp->last);
		free(mp);
	}
	RB_FOREACH_SAFE(mw, monitor_windows, &me->windows, mw1) {
		RB_REMOVE(monitor_windows, &me->windows, mw);
		free(mw->last);
		free(mw);
	}
	free(me->last);

	RB_REMOVE(monitor_items, &ms->items, me);
	free(me->name);
	free(me->format);
	free(me);
}

/* Report a changed value. */
static void
monitor_report(struct monitor_set *ms, struct monitor_item *me,
    struct session *s, struct winlink *wl, struct window_pane *wp,
    const char *value, const char *last)
{
	struct monitor_change	change = { 0 };

	log_debug("%s: %s changed to %s", __func__, me->name, value);

	change.name = me->name;
	change.value = value;
	change.last = last;
	change.c = ms->client;
	change.s = s;
	change.wl = wl;
	change.wp = wp;
	ms->cb(&change, ms->data);
}

/* Check a value against its last value and report if changed. */
static void
monitor_check_value(struct monitor_set *ms, struct monitor_item *me,
    struct session *s, struct winlink *wl, struct window_pane *wp,
    char *value, char **last)
{
	if (*last == NULL) {
		*last = value;
		if ((me->flags & MONITOR_NOTIFY_INITIAL) &&
		    ((~me->flags & MONITOR_NOTIFY_TRUE) ||
		    format_true(value)))
			monitor_report(ms, me, s, wl, wp, value, NULL);
		return;
	}

	if (strcmp(value, *last) == 0) {
		free(value);
		return;
	}

	if ((~me->flags & MONITOR_NOTIFY_TRUE) || format_true(value))
		monitor_report(ms, me, s, wl, wp, value, *last);
	free(*last);
	*last = value;
}

/* Check session subscription. */
static void
monitor_check_session(struct monitor_set *ms, struct monitor_item *me,
    struct format_tree *ft)
{
	struct session	*s = monitor_get_session(ms);
	char		*value;

	value = format_expand(ft, me->format);

	monitor_check_value(ms, me, s, NULL, NULL, value, &me->last);
}

/* Check pane subscription. */
static void
monitor_check_pane(struct monitor_set *ms, struct monitor_item *me)
{
	struct client		*c = ms->client;
	struct session		*s = monitor_get_session(ms);
	struct window_pane	*wp;
	struct window		*w;
	struct winlink		*wl;
	struct format_tree	*ft;
	char			*value;
	struct monitor_pane	*mp, find;

	wp = window_pane_find_by_id(me->id);
	if (wp == NULL || wp->fd == -1)
		return;
	w = wp->window;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session != s)
			continue;

		ft = monitor_create_formats(c, s, wl, wp);
		value = format_expand(ft, me->format);
		format_free(ft);

		find.pane = wp->id;
		find.idx = wl->idx;
		mp = RB_FIND(monitor_panes, &me->panes, &find);
		if (mp == NULL) {
			mp = xcalloc(1, sizeof *mp);
			mp->pane = wp->id;
			mp->idx = wl->idx;
			RB_INSERT(monitor_panes, &me->panes, mp);
		}

		monitor_check_value(ms, me, s, wl, wp, value, &mp->last);
	}
}

/* Check one all-panes subscription. */
static void
monitor_check_all_panes_one(struct monitor_set *ms, struct monitor_item *me,
    struct format_tree *ft, struct winlink *wl, struct window_pane *wp)
{
	struct session		*s = monitor_get_session(ms);
	char			*value;
	struct monitor_pane	*mp, find;

	value = format_expand(ft, me->format);

	find.pane = wp->id;
	find.idx = wl->idx;
	mp = RB_FIND(monitor_panes, &me->panes, &find);
	if (mp == NULL) {
		mp = xcalloc(1, sizeof *mp);
		mp->pane = wp->id;
		mp->idx = wl->idx;
		RB_INSERT(monitor_panes, &me->panes, mp);
	}
	mp->generation = ms->generation;

	monitor_check_value(ms, me, s, wl, wp, value, &mp->last);
}

/* Remove all-panes entries not seen during the current scan. */
static void
monitor_sweep_all_panes(struct monitor_item *me, u_int generation)
{
	struct monitor_pane	*mp, *mp1;

	RB_FOREACH_SAFE(mp, monitor_panes, &me->panes, mp1) {
		if (mp->generation == generation)
			continue;
		RB_REMOVE(monitor_panes, &me->panes, mp);
		free(mp->last);
		free(mp);
	}
}

/* Check window subscription. */
static void
monitor_check_window(struct monitor_set *ms, struct monitor_item *me)
{
	struct client		*c = ms->client;
	struct session		*s = monitor_get_session(ms);
	struct window		*w;
	struct winlink		*wl;
	struct format_tree	*ft;
	char			*value;
	struct monitor_window	*mw, find;

	w = window_find_by_id(me->id);
	if (w == NULL)
		return;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->session != s)
			continue;

		ft = monitor_create_formats(c, s, wl, NULL);
		value = format_expand(ft, me->format);
		format_free(ft);

		find.window = w->id;
		find.idx = wl->idx;
		mw = RB_FIND(monitor_windows, &me->windows, &find);
		if (mw == NULL) {
			mw = xcalloc(1, sizeof *mw);
			mw->window = w->id;
			mw->idx = wl->idx;
			RB_INSERT(monitor_windows, &me->windows, mw);
		}

		monitor_check_value(ms, me, s, wl, NULL, value, &mw->last);
	}
}

/* Check one all-windows subscription. */
static void
monitor_check_all_windows_one(struct monitor_set *ms, struct monitor_item *me,
    struct format_tree *ft, struct winlink *wl)
{
	struct session		*s = monitor_get_session(ms);
	struct window		*w = wl->window;
	char			*value;
	struct monitor_window	*mw, find;

	value = format_expand(ft, me->format);

	find.window = w->id;
	find.idx = wl->idx;
	mw = RB_FIND(monitor_windows, &me->windows, &find);
	if (mw == NULL) {
		mw = xcalloc(1, sizeof *mw);
		mw->window = w->id;
		mw->idx = wl->idx;
		RB_INSERT(monitor_windows, &me->windows, mw);
	}
	mw->generation = ms->generation;

	monitor_check_value(ms, me, s, wl, NULL, value, &mw->last);
}

/* Remove all-windows entries not seen during the current scan. */
static void
monitor_sweep_all_windows(struct monitor_item *me, u_int generation)
{
	struct monitor_window	*mw, *mw1;

	RB_FOREACH_SAFE(mw, monitor_windows, &me->windows, mw1) {
		if (mw->generation == generation)
			continue;
		RB_REMOVE(monitor_windows, &me->windows, mw);
		free(mw->last);
		free(mw);
	}
}

/* Check session subscriptions. */
static void
monitor_check_sessions(struct monitor_set *ms)
{
	struct client		*c = ms->client;
	struct session		*s = monitor_get_session(ms);
	struct monitor_item	*me, *me1;
	struct format_tree	*ft;

	ft = monitor_create_formats(c, s, NULL, NULL);
	RB_FOREACH_SAFE(me, monitor_items, &ms->items, me1) {
		if (me->type == MONITOR_SESSION)
			monitor_check_session(ms, me, ft);
	}
	format_free(ft);
}

/* Check pane and window subscriptions. */
static void
monitor_check_panes_windows(struct monitor_set *ms)
{
	struct monitor_item	*me, *me1;

	RB_FOREACH_SAFE(me, monitor_items, &ms->items, me1) {
		switch (me->type) {
		case MONITOR_PANE:
			monitor_check_pane(ms, me);
			break;
		case MONITOR_WINDOW:
			monitor_check_window(ms, me);
			break;
		case MONITOR_SESSION:
		case MONITOR_ALL_PANES:
		case MONITOR_ALL_WINDOWS:
			break;
		}
	}
}

/* Check all-panes subscriptions. */
static void
monitor_check_all_panes(struct monitor_set *ms)
{
	struct client		*c = ms->client;
	struct session		*s = monitor_get_session(ms);
	struct monitor_item	*me, *me1;
	struct window_pane	*wp;
	struct format_tree	*ft;
	struct winlink		*wl;

	if (++ms->generation == 0)
		ms->generation = 1;
	RB_FOREACH(wl, winlinks, &s->windows) {
		TAILQ_FOREACH(wp, &wl->window->panes, entry) {
			ft = monitor_create_formats(c, s, wl, wp);
			RB_FOREACH_SAFE(me, monitor_items, &ms->items, me1) {
				if (me->type != MONITOR_ALL_PANES)
					continue;
				monitor_check_all_panes_one(ms, me, ft, wl, wp);
			}
			format_free(ft);
		}
	}
	RB_FOREACH_SAFE(me, monitor_items, &ms->items, me1) {
		if (me->type == MONITOR_ALL_PANES)
			monitor_sweep_all_panes(me, ms->generation);
	}
}

/* Check all-windows subscriptions. */
static void
monitor_check_all_windows(struct monitor_set *ms)
{
	struct client		*c = ms->client;
	struct session		*s = monitor_get_session(ms);
	struct monitor_item	*me, *me1;
	struct format_tree	*ft;
	struct winlink		*wl;

	if (++ms->generation == 0)
		ms->generation = 1;
	RB_FOREACH(wl, winlinks, &s->windows) {
		ft = monitor_create_formats(c, s, wl, NULL);
		RB_FOREACH_SAFE(me, monitor_items, &ms->items, me1) {
			if (me->type != MONITOR_ALL_WINDOWS)
				continue;
			monitor_check_all_windows_one(ms, me, ft, wl);
		}
		format_free(ft);
	}
	RB_FOREACH_SAFE(me, monitor_items, &ms->items, me1) {
		if (me->type == MONITOR_ALL_WINDOWS)
			monitor_sweep_all_windows(me, ms->generation);
	}
}

/* Check subscriptions. */
static void
monitor_timer(__unused int fd, __unused short events, void *data)
{
	struct monitor_set	*ms = data;
	struct monitor_item	*me;
	struct timeval		 tv = { .tv_sec = 1 };
	int			 have_session = 0, have_all_panes = 0;
	int			 have_all_windows = 0;

	log_debug("%s: timer fired", __func__);
	evtimer_add(&ms->timer, &tv);

	if (monitor_get_session(ms) == NULL)
		return;

	RB_FOREACH(me, monitor_items, &ms->items) {
		switch (me->type) {
		case MONITOR_SESSION:
			have_session = 1;
			break;
		case MONITOR_ALL_PANES:
			have_all_panes = 1;
			break;
		case MONITOR_ALL_WINDOWS:
			have_all_windows = 1;
			break;
		case MONITOR_PANE:
		case MONITOR_WINDOW:
			break;
		}
	}

	if (have_session)
		monitor_check_sessions(ms);
	monitor_check_panes_windows(ms);
	if (have_all_panes)
		monitor_check_all_panes(ms);
	if (have_all_windows)
		monitor_check_all_windows(ms);
}

/* Create a monitor set. */
static struct monitor_set *
monitor_create(monitor_cb cb, void *data)
{
	struct monitor_set	*ms;

	ms = xcalloc(1, sizeof *ms);
	ms->cb = cb;
	ms->data = data;
	RB_INIT(&ms->items);
	return (ms);
}

/* Create a client monitor set. */
struct monitor_set *
monitor_create_client(struct client *c, monitor_cb cb, void *data)
{
	struct monitor_set	*ms;

	ms = monitor_create(cb, data);
	ms->client = c;
	return (ms);
}

/* Create a monitor set for a session. */
struct monitor_set *
monitor_create_session(struct session *s, monitor_cb cb, void *data)
{
	struct monitor_set	*ms;

	ms = monitor_create(cb, data);
	ms->session = s;
	if (s != NULL)
		session_add_ref(s, __func__);
	return (ms);
}

/* Destroy a monitor set. */
void
monitor_destroy(struct monitor_set *ms)
{
	struct monitor_item	*me, *me1;

	if (ms != NULL) {
		if (evtimer_initialized(&ms->timer))
			evtimer_del(&ms->timer);
		RB_FOREACH_SAFE(me, monitor_items, &ms->items, me1)
			monitor_free_item(ms, me);
		if (ms->session != NULL)
			session_remove_ref(ms->session, __func__);
		free(ms);
	}
}

/* Parse a subscription. */
int
monitor_parse(const char *value, char **name, enum monitor_type *type, int *id,
    char **format)
{
	char	*copy, *what, *split;

	copy = xstrdup(value);
	*id = -1;

	what = strchr(copy, ':');
	if (what == NULL)
		goto fail;
	*what++ = '\0';

	split = strchr(what, ':');
	if (split == NULL)
		goto fail;
	*split++ = '\0';

	if (strcmp(what, "%*") == 0)
		*type = MONITOR_ALL_PANES;
	else if (sscanf(what, "%%%d", id) == 1 && *id >= 0)
		*type = MONITOR_PANE;
	else if (strcmp(what, "@*") == 0)
		*type = MONITOR_ALL_WINDOWS;
	else if (sscanf(what, "@%d", id) == 1 && *id >= 0)
		*type = MONITOR_WINDOW;
	else
		*type = MONITOR_SESSION;
	*name = xstrdup(copy);
	*format = xstrdup(split);

	free(copy);
	return (0);

fail:
	free(copy);
	return (-1);
}

/* Add a subscription. */
void
monitor_add(struct monitor_set *ms, const char *name, enum monitor_type type,
    int id, const char *format, int flags)
{
	struct monitor_item	*me, find = { .name = (char *)name };
	struct timeval		 tv = { .tv_sec = 1 };

	if ((me = RB_FIND(monitor_items, &ms->items, &find)) != NULL)
		monitor_free_item(ms, me);

	me = xcalloc(1, sizeof *me);
	me->name = xstrdup(name);
	me->format = xstrdup(format);
	me->type = type;
	me->id = id;
	me->flags = flags;
	RB_INIT(&me->panes);
	RB_INIT(&me->windows);
	RB_INSERT(monitor_items, &ms->items, me);

	if (!evtimer_initialized(&ms->timer))
		evtimer_set(&ms->timer, monitor_timer, ms);
	if (!evtimer_pending(&ms->timer, NULL))
		evtimer_add(&ms->timer, &tv);
}

/* Remove a subscription. */
void
monitor_remove(struct monitor_set *ms, const char *name)
{
	struct monitor_item	*me, find = { .name = (char *)name };

	if ((me = RB_FIND(monitor_items, &ms->items, &find)) != NULL)
		monitor_free_item(ms, me);
	if (RB_EMPTY(&ms->items) && evtimer_initialized(&ms->timer))
		evtimer_del(&ms->timer);
}
