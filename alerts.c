/* $OpenBSD$ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <event.h>

#include "tmux.h"

int	alerts_fired;

void	alerts_timer(int, short, void *);
int	alerts_enabled(struct window *, int);
void	alerts_callback(int, short, void *);
void	alerts_reset(struct window *);

int	alerts_check_bell(struct session *, struct winlink *);
int	alerts_check_activity(struct session *, struct winlink *);
int	alerts_check_silence(struct session *, struct winlink *);
void	alerts_ring_bell(struct session *);

void
alerts_timer(unused int fd, unused short events, void *arg)
{
	struct window	*w = arg;

	log_debug("@%u alerts timer expired", w->id);
	alerts_reset(w);
	alerts_queue(w, WINDOW_SILENCE);
}

void
alerts_callback(unused int fd, unused short events, unused void *arg)
{
	struct window	*w;
	struct session	*s;
	struct winlink	*wl;
	int		 flags, alerts;

	RB_FOREACH(w, windows, &windows) {
		RB_FOREACH(s, sessions, &sessions) {
			RB_FOREACH(wl, winlinks, &s->windows) {
				if (wl->window != w)
					continue;
				flags = w->flags;

				alerts  = alerts_check_bell(s, wl);
				alerts |= alerts_check_activity(s, wl);
				alerts |= alerts_check_silence(s, wl);
				if (alerts != 0)
					server_status_session(s);

				log_debug("%s:%d @%u alerts check, alerts %#x, "
				    "flags %#x", s->name, wl->idx, w->id,
				    alerts, flags);
			}
		}
	}
	alerts_fired = 0;
}

int
alerts_enabled(struct window *w, int flags)
{
	struct session	*s;

	if (flags & WINDOW_ACTIVITY) {
		if (options_get_number(&w->options, "monitor-activity"))
			return (1);
	}
	if (flags & WINDOW_SILENCE) {
		if (options_get_number(&w->options, "monitor-silence") != 0)
			return (1);
	}
	if (~flags & WINDOW_BELL)
		return (0);
	RB_FOREACH(s, sessions, &sessions) {
		if (!session_has(s, w))
			continue;
		if (options_get_number(&s->options, "bell-action") != BELL_NONE)
			return (1);
	}
	return (0);
}

void
alerts_reset_all(void)
{
	struct window	*w;

	RB_FOREACH(w, windows, &windows)
		alerts_reset(w);
}

void
alerts_reset(struct window *w)
{
	struct timeval	tv;

	w->flags &= ~WINDOW_SILENCE;
	event_del(&w->alerts_timer);

	timerclear(&tv);
	tv.tv_sec = options_get_number(&w->options, "monitor-silence");

	log_debug("@%u alerts timer reset %u", w->id, (u_int)tv.tv_sec);
	if (tv.tv_sec != 0)
		event_add(&w->alerts_timer, &tv);
}

void
alerts_queue(struct window *w, int flags)
{
	if (w->flags & WINDOW_ACTIVITY)
		alerts_reset(w);

	if (!event_initialized(&w->alerts_timer))
		evtimer_set(&w->alerts_timer, alerts_timer, w);

	if (w->flags & flags)
		return;
	w->flags |= flags;
	log_debug("@%u alerts flags added %#x", w->id, flags);

	if (!alerts_fired && alerts_enabled(w, flags)) {
		log_debug("alerts check queued (by @%u)", w->id);
		event_once(-1, EV_TIMEOUT, alerts_callback, NULL, NULL);
		alerts_fired = 1;
	}
}

int
alerts_check_bell(struct session *s, struct winlink *wl)
{
	struct client	*c;
	struct window	*w = wl->window;
	int		 action, visual;

	if (!(w->flags & WINDOW_BELL) || wl->flags & WINLINK_BELL)
		return (0);
	if (s->curw != wl || s->flags & SESSION_UNATTACHED)
		wl->flags |= WINLINK_BELL;
	if (s->flags & SESSION_UNATTACHED)
		return (0);
	if (s->curw->window == w)
		w->flags &= ~WINDOW_BELL;

	action = options_get_number(&s->options, "bell-action");
	if (action == BELL_NONE)
		return (0);

	visual = options_get_number(&s->options, "visual-bell");
	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session != s || c->flags & CLIENT_CONTROL)
			continue;
		if (!visual) {
			if ((action == BELL_CURRENT &&
			    c->session->curw->window == w) ||
			    (action == BELL_OTHER &&
			    c->session->curw->window != w) ||
			    action == BELL_ANY)
				tty_putcode(&c->tty, TTYC_BEL);
			continue;
		}
		if (action == BELL_CURRENT && c->session->curw->window == w)
			status_message_set(c, "Bell in current window");
		else if (action == BELL_ANY || (action == BELL_OTHER &&
		    c->session->curw->window != w))
			status_message_set(c, "Bell in window %d", wl->idx);
	}

	return (WINDOW_BELL);
}

int
alerts_check_activity(struct session *s, struct winlink *wl)
{
	struct client	*c;
	struct window	*w = wl->window;

	if (s->curw->window == w)
		w->flags &= ~WINDOW_ACTIVITY;

	if (!(w->flags & WINDOW_ACTIVITY) || wl->flags & WINLINK_ACTIVITY)
		return (0);
	if (s->curw == wl && !(s->flags & SESSION_UNATTACHED))
		return (0);

	if (!options_get_number(&w->options, "monitor-activity"))
		return (0);

	if (options_get_number(&s->options, "bell-on-alert"))
		alerts_ring_bell(s);
	wl->flags |= WINLINK_ACTIVITY;

	if (options_get_number(&s->options, "visual-activity")) {
		TAILQ_FOREACH(c, &clients, entry) {
			if (c->session != s)
				continue;
			status_message_set(c, "Activity in window %d", wl->idx);
		}
	}

	return (WINDOW_ACTIVITY);
}

int
alerts_check_silence(struct session *s, struct winlink *wl)
{
	struct client	*c;
	struct window	*w = wl->window;

	if (s->curw->window == w)
		w->flags &= ~WINDOW_SILENCE;

	if (!(w->flags & WINDOW_SILENCE) || wl->flags & WINLINK_SILENCE)
		return (0);
	if (s->curw == wl && !(s->flags & SESSION_UNATTACHED))
		return (0);

	if (options_get_number(&w->options, "monitor-silence") == 0)
		return (0);

	if (options_get_number(&s->options, "bell-on-alert"))
		alerts_ring_bell(s);
	wl->flags |= WINLINK_SILENCE;

	if (options_get_number(&s->options, "visual-silence")) {
		TAILQ_FOREACH(c, &clients, entry) {
			if (c->session != s)
				continue;
			status_message_set(c, "Silence in window %d", wl->idx);
		}
	}

	return (WINDOW_SILENCE);
}

void
alerts_ring_bell(struct session *s)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == s && !(c->flags & CLIENT_CONTROL))
			tty_putcode(&c->tty, TTYC_BEL);
	}
}
