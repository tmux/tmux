/* $OpenBSD$ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <stdlib.h>

#include "tmux.h"

static int	alerts_fired;

static void	alerts_timer(int, short, void *);
static int	alerts_enabled(struct window *, int);
static void	alerts_callback(int, short, void *);
static void	alerts_reset(struct window *);

static int	alerts_check_all(struct window *);
static int	alerts_check_bell(struct window *);
static int	alerts_check_activity(struct window *);
static int	alerts_check_silence(struct window *);
static void printflike(2, 3) alerts_set_message(struct session *, const char *,
		    ...);
static void	alerts_ring_bell(struct session *);

static TAILQ_HEAD(, window) alerts_list = TAILQ_HEAD_INITIALIZER(alerts_list);

static void
alerts_timer(__unused int fd, __unused short events, void *arg)
{
	struct window	*w = arg;

	log_debug("@%u alerts timer expired", w->id);
	alerts_reset(w);
	alerts_queue(w, WINDOW_SILENCE);
}

static void
alerts_callback(__unused int fd, __unused short events, __unused void *arg)
{
	struct window	*w, *w1;
	int		 alerts;

	TAILQ_FOREACH_SAFE(w, &alerts_list, alerts_entry, w1) {
		alerts = alerts_check_all(w);
		log_debug("@%u alerts check, alerts %#x", w->id, alerts);

		w->alerts_queued = 0;
		TAILQ_REMOVE(&alerts_list, w, alerts_entry);

		w->flags &= ~WINDOW_ALERTFLAGS;
		window_remove_ref(w);
	}
	alerts_fired = 0;
}

static int
alerts_check_all(struct window *w)
{
	int	alerts;

	alerts  = alerts_check_bell(w);
	alerts |= alerts_check_activity(w);
	alerts |= alerts_check_silence(w);
	return (alerts);
}

void
alerts_check_session(struct session *s)
{
	struct winlink	*wl;

	RB_FOREACH(wl, winlinks, &s->windows)
		alerts_check_all(wl->window);
}

static int
alerts_enabled(struct window *w, int flags)
{
	if (flags & WINDOW_BELL)
		return (1);
	if (flags & WINDOW_ACTIVITY) {
		if (options_get_number(w->options, "monitor-activity"))
			return (1);
	}
	if (flags & WINDOW_SILENCE) {
		if (options_get_number(w->options, "monitor-silence") != 0)
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

static void
alerts_reset(struct window *w)
{
	struct timeval	tv;

	w->flags &= ~WINDOW_SILENCE;
	event_del(&w->alerts_timer);

	timerclear(&tv);
	tv.tv_sec = options_get_number(w->options, "monitor-silence");

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

	if ((w->flags & flags) != flags) {
		w->flags |= flags;
		log_debug("@%u alerts flags added %#x", w->id, flags);
	}

	if (!w->alerts_queued) {
		w->alerts_queued = 1;
		TAILQ_INSERT_TAIL(&alerts_list, w, alerts_entry);
		w->references++;
	}

	if (!alerts_fired && alerts_enabled(w, flags)) {
		log_debug("alerts check queued (by @%u)", w->id);
		event_once(-1, EV_TIMEOUT, alerts_callback, NULL, NULL);
		alerts_fired = 1;
	}
}

static int
alerts_check_bell(struct window *w)
{
	struct window	*ws;
	struct winlink	*wl;
	struct session	*s;
	struct client	*c;
	int		 action, visual;

	if (~w->flags & WINDOW_BELL)
		return (0);

	TAILQ_FOREACH(wl, &w->winlinks, wentry)
		wl->session->flags &= ~SESSION_ALERTED;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->flags & WINLINK_BELL)
			continue;
		s = wl->session;
		if (s->curw != wl) {
			wl->flags |= WINLINK_BELL;
			notify_winlink("alert-bell", s, wl);
		}

		if (s->flags & SESSION_ALERTED)
			continue;
		s->flags |= SESSION_ALERTED;

		action = options_get_number(s->options, "bell-action");
		if (action == BELL_NONE)
			return (0);

		visual = options_get_number(s->options, "visual-bell");
		TAILQ_FOREACH(c, &clients, entry) {
			if (c->session != s || c->flags & CLIENT_CONTROL)
				continue;
			ws = c->session->curw->window;

			if (action == BELL_CURRENT && ws != w)
				action = BELL_NONE;
			if (action == BELL_OTHER && ws != w)
				action = BELL_NONE;

			if (!visual) {
				if (action != BELL_NONE)
					tty_putcode(&c->tty, TTYC_BEL);
				continue;
			}
			if (action == BELL_CURRENT)
				status_message_set(c, "Bell in current window");
			else if (action != BELL_NONE) {
				status_message_set(c, "Bell in window %d",
				    wl->idx);
			}
		}
	}

	return (WINDOW_BELL);
}

static int
alerts_check_activity(struct window *w)
{
	struct winlink	*wl;
	struct session	*s;

	if (~w->flags & WINDOW_ACTIVITY)
		return (0);
	if (!options_get_number(w->options, "monitor-activity"))
		return (0);

	TAILQ_FOREACH(wl, &w->winlinks, wentry)
		wl->session->flags &= ~SESSION_ALERTED;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->flags & WINLINK_ACTIVITY)
			continue;
		s = wl->session;
		if (s->curw == wl)
			continue;

		wl->flags |= WINLINK_ACTIVITY;
		notify_winlink("alert-activity", s, wl);

		if (s->flags & SESSION_ALERTED)
			continue;
		s->flags |= SESSION_ALERTED;

		if (options_get_number(s->options, "bell-on-alert"))
			alerts_ring_bell(s);
		if (options_get_number(s->options, "visual-activity"))
			alerts_set_message(s, "Activity in window %d", wl->idx);
	}

	return (WINDOW_ACTIVITY);
}

static int
alerts_check_silence(struct window *w)
{
	struct winlink	*wl;
	struct session	*s;

	if (~w->flags & WINDOW_SILENCE)
		return (0);
	if (!options_get_number(w->options, "monitor-silence"))
		return (0);

	TAILQ_FOREACH(wl, &w->winlinks, wentry)
		wl->session->flags &= ~SESSION_ALERTED;

	TAILQ_FOREACH(wl, &w->winlinks, wentry) {
		if (wl->flags & WINLINK_SILENCE)
			continue;
		s = wl->session;
		if (s->curw == wl)
			continue;
		wl->flags |= WINLINK_SILENCE;
		notify_winlink("alert-silence", s, wl);

		if (s->flags & SESSION_ALERTED)
			continue;
		s->flags |= SESSION_ALERTED;

		if (options_get_number(s->options, "bell-on-alert"))
			alerts_ring_bell(s);

		if (!options_get_number(s->options, "visual-silence"))
			alerts_set_message(s, "Silence in window %d", wl->idx);
	}

	return (WINDOW_SILENCE);
}

static void
alerts_set_message(struct session *s, const char *fmt, ...)
{
	struct client	*c;
	va_list		 ap;
	char		*message;

	va_start(ap, fmt);
	xvasprintf(&message, fmt, ap);
	va_end(ap);

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == s)
			status_message_set(c, "%s", message);
	}

	free(message);
}

static void
alerts_ring_bell(struct session *s)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->session == s && !(c->flags & CLIENT_CONTROL))
			tty_putcode(&c->tty, TTYC_BEL);
	}
}
