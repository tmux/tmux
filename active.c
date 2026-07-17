/* $OpenBSD$ */

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
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>

#include "tmux.h"

/* Local last-window stack entry. */
struct active_window_entry {
	u_int					window;
	TAILQ_ENTRY(active_window_entry)	entry;
};

/* Local last-window stack. */
TAILQ_HEAD(active_window_stack, active_window_entry);

/* Per-client and per-session local active window state. */
struct active_window {
	struct client			*client;
	struct session			*session;

	u_int				 window;
	struct active_window_stack	 lastw;

	RB_ENTRY(active_window)		 entry;
};
RB_HEAD(active_windows, active_window);

/* Pane entry in a client-local last-pane stack. */
struct active_pane_entry {
	struct window_pane		*wp;
	TAILQ_ENTRY(active_pane_entry)	 entry;
};
TAILQ_HEAD(active_pane_stack, active_pane_entry);

/* Per-client and per-window local active pane state. */
struct active_pane {
	struct client			*client;
	struct window			*window;
	enum active_mode		 mode;
	struct window_pane		*pane;
	struct active_pane_stack	 last_panes;

	RB_ENTRY(active_pane)	 entry;
};
RB_HEAD(active_panes, active_pane);

/* Compare local active window states by client and session. */
static int
active_window_cmp(struct active_window *aw1, struct active_window *aw2)
{
	uintptr_t	c1 = (uintptr_t)aw1->client;
	uintptr_t	c2 = (uintptr_t)aw2->client;
	uintptr_t	s1 = (uintptr_t)aw1->session;
	uintptr_t	s2 = (uintptr_t)aw2->session;

	if (c1 < c2)
		return (-1);
	if (c1 > c2)
		return (1);
	if (s1 < s2)
		return (-1);
	if (s1 > s2)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(active_windows, active_window, entry, active_window_cmp);

/* Compare local active pane states by client and window. */
static int
active_pane_cmp(struct active_pane *ap1, struct active_pane *ap2)
{
	uintptr_t	c1 = (uintptr_t)ap1->client;
	uintptr_t	c2 = (uintptr_t)ap2->client;
	uintptr_t	w1 = (uintptr_t)ap1->window;
	uintptr_t	w2 = (uintptr_t)ap2->window;

	if (c1 < c2)
		return (-1);
	if (c1 > c2)
		return (1);
	if (w1 < w2)
		return (-1);
	if (w1 > w2)
		return (1);
	return (0);
}
RB_GENERATE_STATIC(active_panes, active_pane, entry, active_pane_cmp);

/* All local active window states. */
static struct active_windows active_windows = RB_INITIALIZER(&active_windows);

/* All local active pane states. */
static struct active_panes active_panes = RB_INITIALIZER(&active_panes);

/* Find local active window state for a client and session. */
static struct active_window *
active_window_get(struct client *c, struct session *s)
{
	struct active_window	aw;

	if (c == NULL || s == NULL)
		return (NULL);
	aw.client = c;
	aw.session = s;
	return (RB_FIND(active_windows, &active_windows, &aw));
}

/* Add local active window state for a client and session. */
static struct active_window *
active_window_add(struct client *c, struct session *s, struct winlink *wl)
{
	struct active_window	*aw;

	aw = active_window_get(c, s);
	if (aw == NULL) {
		aw = xcalloc(1, sizeof *aw);
		aw->client = c;
		aw->session = s;
		TAILQ_INIT(&aw->lastw);
		RB_INSERT(active_windows, &active_windows, aw);
	}
	if (wl != NULL)
		aw->window = wl->window->id;
	return (aw);
}

/* Remove a window from a local last-window stack. */
static void
active_window_stack_remove(struct active_window_stack *stack, u_int window)
{
	struct active_window_entry	*awe, *awe1;

	TAILQ_FOREACH_SAFE(awe, stack, entry, awe1) {
		if (awe->window == window) {
			TAILQ_REMOVE(stack, awe, entry);
			free(awe);
		}
	}
}

/* Push a window onto a local last-window stack. */
static void
active_window_stack_push(struct active_window_stack *stack, u_int window)
{
	struct active_window_entry	*awe;

	active_window_stack_remove(stack, window);
	awe = xcalloc(1, sizeof *awe);
	awe->window = window;
	TAILQ_INSERT_HEAD(stack, awe, entry);
}

/* Free local active window state. */
static void
active_window_free(struct active_window *aw)
{
	struct active_window_entry	*awe;

	while (!TAILQ_EMPTY(&aw->lastw)) {
		awe = TAILQ_FIRST(&aw->lastw);
		TAILQ_REMOVE(&aw->lastw, awe, entry);
		free(awe);
	}
	RB_REMOVE(active_windows, &active_windows, aw);
	free(aw);
}

/* Resolve local active window state to a valid winlink. */
static struct winlink *
active_window_resolve(struct active_window *aw)
{
	struct winlink	*wl, *next;
	u_int		 window;

	if (aw == NULL || aw->session->curw == NULL)
		return (NULL);

	wl = winlink_find_by_window_id(&aw->session->windows, aw->window);
	if (wl != NULL)
		return (wl);

	while (!TAILQ_EMPTY(&aw->lastw)) {
		window = TAILQ_FIRST(&aw->lastw)->window;

		next = winlink_find_by_window_id(&aw->session->windows, window);
		active_window_stack_remove(&aw->lastw, window);
		if (next != NULL) {
			aw->window = next->window->id;
			return (next);
		}
	}

	aw->window = aw->session->curw->window->id;
	return (aw->session->curw);
}

/* Notify the server that a client's effective window changed. */
static void
active_window_changed(struct client *c, struct session *s, struct winlink *wl,
    struct winlink *old)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	if (wl == NULL || wl == old)
		return;

	if (options_get_number(global_options, "focus-events")) {
		if (old != NULL)
			window_update_focus(old->window);
		window_update_focus(wl->window);
	}
	winlink_clear_flags(wl);
	window_update_activity(wl->window);
	tty_update_window_offset(wl->window);

	if (c != NULL && c->session == s)
		wl->window->latest = c;

	ep = event_payload_create();
	cmd_find_from_winlink(&fs, wl, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_client(ep, "client", c);
	event_payload_set_session(ep, "session", s);
	event_payload_set_window(ep, "window", wl->window);
	event_payload_set_window(ep, "new_window", wl->window);
	event_payload_set_int(ep, "window_index", wl->idx);
	event_payload_set_int(ep, "new_window_index", wl->idx);
	if (old != NULL) {
		event_payload_set_window(ep, "old_window", old->window);
		event_payload_set_int(ep, "old_window_index", old->idx);
	}
	events_fire("client-window-changed", ep);

	if (c != NULL) {
		c->flags |= CLIENT_REDRAWWINDOW|CLIENT_REDRAWSTATUS|
		    CLIENT_REDRAWBORDERS;
		tty_update_client_offset(c);
		status_update_cache(s);
	}
	recalculate_sizes();
}

/* Find local active pane state for a client and window. */
static struct active_pane *
active_pane_get(struct client *c, struct window *w)
{
	struct active_pane	ap;

	if (c == NULL || w == NULL)
		return (NULL);
	ap.client = c;
	ap.window = w;
	return (RB_FIND(active_panes, &active_panes, &ap));
}

/* Add local active pane state for a client and window. */
static struct active_pane *
active_pane_add(struct client *c, struct window *w)
{
	struct active_pane	*ap;

	ap = active_pane_get(c, w);
	if (ap == NULL) {
		ap = xcalloc(1, sizeof *ap);
		ap->client = c;
		ap->window = w;
		TAILQ_INIT(&ap->last_panes);
		RB_INSERT(active_panes, &active_panes, ap);
	}
	return (ap);
}

/* Remove a pane from a client-local last-pane stack. */
static void
active_pane_stack_remove(struct active_pane *ap,
    struct window_pane *wp)
{
	struct active_pane_entry	*ape, *ape1;

	TAILQ_FOREACH_SAFE(ape, &ap->last_panes, entry, ape1) {
		if (ape->wp == wp) {
			TAILQ_REMOVE(&ap->last_panes, ape, entry);
			free(ape);
		}
	}
}

/* Push a pane onto a client-local last-pane stack. */
static void
active_pane_stack_push(struct active_pane *ap, struct window_pane *wp)
{
	struct active_pane_entry	*ape;

	if (wp == NULL)
		return;
	active_pane_stack_remove(ap, wp);
	ape = xcalloc(1, sizeof *ape);
	ape->wp = wp;
	TAILQ_INSERT_HEAD(&ap->last_panes, ape, entry);
}

/* Free local active pane state. */
static void
active_pane_free(struct active_pane *ap)
{
	struct active_pane_entry	*ape, *ape1;

	RB_REMOVE(active_panes, &active_panes, ap);
	TAILQ_FOREACH_SAFE(ape, &ap->last_panes, entry, ape1) {
		TAILQ_REMOVE(&ap->last_panes, ape, entry);
		free(ape);
	}
	free(ap);
}

/* Check whether a pane can be the effective pane for a window. */
static int
active_pane_valid(struct window *w, struct window_pane *wp)
{
	if (w == NULL || wp == NULL || wp->window != w)
		return (0);
	if (!window_has_pane(w, wp))
		return (0);
	if (w->modal != NULL && wp != w->modal)
		return (0);
	return (1);
}

/* Get the shared pane for a window, accounting for modal panes. */
static struct window_pane *
active_pane_default(struct window *w)
{
	if (w == NULL)
		return (NULL);
	if (w->modal != NULL)
		return (w->modal);
	return (w->active);
}

/* Return if a client has local window selection for a session. */
int
active_is_local_window(struct client *c, struct session *s)
{
	return (active_window_get(c, s) != NULL);
}

/* Make a client use local or shared window selection for a session. */
void
active_set_local_window(struct client *c, struct session *s,
    enum active_mode mode)
{
	struct active_window	*aw;
	struct winlink		*wl, *old;

	if (c == NULL || s == NULL || s->curw == NULL)
		return;

	old = active_get_effective_winlink(c, s);
	if (mode == ACTIVE_LOCAL) {
		active_window_add(c, s, old);
		if (c != NULL && c->session == s)
			c->flags |= CLIENT_REDRAWSTATUS;
		return;
	}

	aw = active_window_get(c, s);
	if (aw != NULL) {
		active_window_free(aw);
		wl = active_get_effective_winlink(c, s);
		active_window_changed(c, s, wl, old);
		if (c != NULL && c->session == s)
			c->flags |= CLIENT_REDRAWSTATUS;
	}
}

/* Return the effective winlink for a client and session. */
struct winlink *
active_get_effective_winlink(struct client *c, struct session *s)
{
	struct active_window	*aw;

	if (s == NULL)
		return (NULL);
	aw = active_window_get(c, s);
	if (aw != NULL)
		return (active_window_resolve(aw));
	return (s->curw);
}

/* Return the effective window for a client and session. */
struct window *
active_get_effective_window(struct client *c, struct session *s)
{
	struct winlink	*wl;

	wl = active_get_effective_winlink(c, s);
	if (wl == NULL)
		return (NULL);
	return (wl->window);
}

/* Select a winlink for a client and session. */
int
active_select_window(struct client *c, struct session *s, struct winlink *wl)
{
	struct active_window	*aw;
	struct winlink		*old;
	int			 changed;

	if (wl == NULL)
		return (-1);
	aw = active_window_get(c, s);
	if (aw == NULL)
		return (session_set_current(s, wl));

	old = active_window_resolve(aw);
	if (old == wl)
		return (1);
	if (old != NULL)
		active_window_stack_push(&aw->lastw, old->window->id);
	active_window_stack_remove(&aw->lastw, wl->window->id);
	aw->window = wl->window->id;
	changed = (old != wl);
	active_window_changed(c, s, wl, old);
	return (changed ? 0 : 1);
}

/* Select a window by index. */
int
active_select_window_index(struct client *c, struct session *s, int idx)
{
	struct winlink	*wl;

	wl = winlink_find_by_index(&s->windows, idx);
	return (active_select_window(c, s, wl));
}

/* Select the next window. */
int
active_next_window(struct client *c, struct session *s, int alert)
{
	struct winlink	*wl;

	wl = active_get_effective_winlink(c, s);
	if (wl == NULL)
		return (-1);
	do {
		wl = winlink_next(wl);
		if (wl == NULL)
			wl = RB_MIN(winlinks, &s->windows);
		if (!alert || (wl->flags & WINLINK_ALERTFLAGS))
			return (active_select_window(c, s, wl));
	} while (wl != active_get_effective_winlink(c, s));
	return (-1);
}

/* Select the previous window. */
int
active_previous_window(struct client *c, struct session *s, int alert)
{
	struct winlink	*wl;

	wl = active_get_effective_winlink(c, s);
	if (wl == NULL)
		return (-1);
	do {
		wl = winlink_previous(wl);
		if (wl == NULL)
			wl = RB_MAX(winlinks, &s->windows);
		if (!alert || (wl->flags & WINLINK_ALERTFLAGS))
			return (active_select_window(c, s, wl));
	} while (wl != active_get_effective_winlink(c, s));
	return (-1);
}

/* Select the last window. */
int
active_last_window(struct client *c, struct session *s)
{
	struct active_window	*aw;
	struct winlink		*wl;
	u_int			 window;

	aw = active_window_get(c, s);
	if (aw == NULL)
		return (session_last(s));
	while (!TAILQ_EMPTY(&aw->lastw)) {
		window = TAILQ_FIRST(&aw->lastw)->window;

		wl = winlink_find_by_window_id(&s->windows, window);
		if (wl != NULL)
			return (active_select_window(c, s, wl));
		active_window_stack_remove(&aw->lastw, window);
	}
	return (-1);
}

/* Return if this winlink is the effective winlink. */
int
active_is_effective_window(struct client *c, struct session *s,
    struct winlink *wl)
{
	return (wl != NULL && wl == active_get_effective_winlink(c, s));
}

/* Return if this winlink is the last effective winlink. */
int
active_is_last_window(struct client *c, struct session *s, struct winlink *wl)
{
	struct active_window		*aw;
	struct active_window_entry	*awe;

	if (wl == NULL)
		return (0);
	aw = active_window_get(c, s);
	if (aw == NULL)
		return (wl == TAILQ_FIRST(&s->lastw));
	TAILQ_FOREACH(awe, &aw->lastw, entry) {
		if (winlink_find_by_window_id(&s->windows, awe->window) == NULL)
			continue;
		return (wl->window->id == awe->window);
	}
	return (0);
}

/* Return if a client has local pane selection for a window. */
int
active_has_local_pane(struct client *c, struct window *w)
{
	struct active_pane	*ap;

	ap = active_pane_get(c, w);
	return (ap != NULL && ap->mode == ACTIVE_LOCAL);
}

/* Make a client use local or shared pane selection for a window. */
void
active_set_pane_mode(struct client *c, struct window *w, enum active_mode mode)
{
	struct active_pane	*ap;
	struct window_pane	*wp;

	if (c == NULL || w == NULL)
		return;
	if (mode == ACTIVE_SHARED) {
		ap = active_pane_get(c, w);
		if (ap != NULL)
			ap->mode = ACTIVE_SHARED;
		server_redraw_client(c);
		server_status_client(c);
		return;
	}

	if (server_client_how_many() < 2) {
		ap = active_pane_get(c, w);
		if (ap != NULL)
			ap->mode = ACTIVE_SHARED;
		server_redraw_client(c);
		server_status_client(c);
		return;
	}

	wp = active_get_effective_pane(c, w);
	if (wp == NULL)
		wp = active_pane_default(w);
	ap = active_pane_add(c, w);
	ap->mode = ACTIVE_LOCAL;
	ap->pane = wp;
	server_redraw_client(c);
	server_status_client(c);
}

/* Return the effective active pane for a client and window. */
struct window_pane *
active_get_effective_pane(struct client *c, struct window *w)
{
	struct active_pane	*ap;
	struct window_pane	*wp;

	wp = active_pane_default(w);
	ap = active_pane_get(c, w);
	if (ap == NULL || ap->mode != ACTIVE_LOCAL)
		return (wp);
	if (!active_pane_valid(w, ap->pane)) {
		ap->pane = wp;
		if (wp != NULL)
			active_pane_stack_remove(ap, wp);
		return (wp);
	}
	return (ap->pane);
}

/* Return the effective last pane for a client and window. */
struct window_pane *
active_get_last_pane(struct client *c, struct window *w)
{
	struct active_pane		*ap;
	struct active_pane_entry	*ape, *ape1;

	if (!active_has_local_pane(c, w))
		return (w == NULL ? NULL : TAILQ_FIRST(&w->last_panes));

	ap = active_pane_get(c, w);
	TAILQ_FOREACH_SAFE(ape, &ap->last_panes, entry, ape1) {
		if (active_pane_valid(w, ape->wp))
			return (ape->wp);
		TAILQ_REMOVE(&ap->last_panes, ape, entry);
		free(ape);
	}
	return (NULL);
}

/* Select a pane using either shared or local state. */
int
active_set_pane(struct client *c, struct window *w, struct window_pane *wp,
    int notify)
{
	struct active_pane	*ap;
	struct window_pane	*lastwp;

	if (!active_pane_valid(w, wp))
		return (0);
	if (!active_has_local_pane(c, w))
		return (window_set_active_pane(w, wp, notify));

	lastwp = active_get_effective_pane(c, w);
	if (wp == lastwp)
		return (0);

	ap = active_pane_add(c, w);
	active_pane_stack_remove(ap, wp);
	active_pane_stack_push(ap, lastwp);
	ap->pane = wp;

	if (c != NULL) {
		server_redraw_client(c);
		server_status_client(c);
	}
	return (1);
}

/* Remove all local pane state if fewer than two clients remain. */
void
active_check_clients(void)
{
	struct active_pane	*ap, *ap1;

	if (server_client_how_many() >= 2)
		return;

	RB_FOREACH_SAFE(ap, active_panes, &active_panes, ap1) {
		if (ap->mode == ACTIVE_LOCAL) {
			server_redraw_client(ap->client);
			server_status_client(ap->client);
		}
		active_pane_free(ap);
	}
}

/* Remove all state for a client. */
void
active_remove_client(struct client *c)
{
	struct active_window	*aw, *aw1;
	struct active_pane		*ap, *ap1;

	RB_FOREACH_SAFE(aw, active_windows, &active_windows, aw1) {
		if (aw->client == c)
			active_window_free(aw);
	}
	RB_FOREACH_SAFE(ap, active_panes, &active_panes, ap1) {
		if (ap->client == c)
			active_pane_free(ap);
	}
}

/* Remove all window state for a session. */
void
active_remove_session(struct session *s)
{
	struct active_window	*aw, *aw1;

	RB_FOREACH_SAFE(aw, active_windows, &active_windows, aw1) {
		if (aw->session == s)
			active_window_free(aw);
	}
}

/* Remove a window from all local state. */
void
active_remove_window(struct window *w)
{
	struct active_window	*aw;
	struct active_pane		*ap, *ap1;

	RB_FOREACH(aw, active_windows, &active_windows) {
		active_window_stack_remove(&aw->lastw, w->id);
		if (aw->window == w->id && aw->session->curw != NULL)
			aw->window = aw->session->curw->window->id;
	}
	RB_FOREACH_SAFE(ap, active_panes, &active_panes, ap1) {
		if (ap->window == w)
			active_pane_free(ap);
	}
}

/* Remove a pane from all local pane state. */
void
active_remove_pane(struct window_pane *wp)
{
	struct active_pane	*ap, *ap1;

	RB_FOREACH_SAFE(ap, active_panes, &active_panes, ap1) {
		active_pane_stack_remove(ap, wp);
		if (ap->pane == wp)
			ap->pane = NULL;
	}
}
