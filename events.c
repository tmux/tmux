/* $OpenBSD: events.c,v 1.1 2026/07/10 13:38:45 nicm Exp $ */

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
#include <string.h>

#include "tmux.h"

/* Event sink. */
struct events_sink {
	char				*name;
	events_cb			 cb;
	void				*data;
	int				 dead;
	u_int				 generation;

	TAILQ_ENTRY(events_sink)	 entry;
};

TAILQ_HEAD(events_sinks, events_sink);
static struct events_sinks events_sinks = TAILQ_HEAD_INITIALIZER(events_sinks);

static u_int events_dispatching;
static u_int events_generation;

/* Free an event sink. */
static void
events_free_sink(struct events_sink *es)
{
	TAILQ_REMOVE(&events_sinks, es, entry);
	free(es->name);
	free(es);
}

/* Free dead event sinks. */
static void
events_free_dead(void)
{
	struct events_sink	*es, *es1;

	TAILQ_FOREACH_SAFE(es, &events_sinks, entry, es1) {
		if (es->dead)
			events_free_sink(es);
	}
}

/* Add an event sink. */
struct events_sink *
events_add_sink(const char *name, events_cb cb, void *data)
{
	struct events_sink	*es;

	es = xcalloc(1, sizeof *es);
	es->name = xstrdup(name);
	es->cb = cb;
	es->data = data;
	es->generation = ++events_generation;

	TAILQ_INSERT_TAIL(&events_sinks, es, entry);
	return (es);
}

/* Remove an event sink. */
void
events_remove_sink(struct events_sink *es)
{
	if (es != NULL && !es->dead) {
		if (events_dispatching != 0)
			es->dead = 1;
		else
			events_free_sink(es);
	}
}

/* Fire an event. */
void
events_fire(const char *name, struct event_payload *ep)
{
	struct events_sink	*es;
	u_int			 generation = events_generation;

	event_payload_set_string(ep, "event", "%s", name);

	if (log_get_level() != 0)
		event_payload_log(ep, "%s: %s: ", __func__, name);

	events_dispatching++;
	TAILQ_FOREACH(es, &events_sinks, entry) {
		if (es->dead || es->generation > generation)
			continue;
		if (strcmp(es->name, name) == 0)
			es->cb(name, ep, es->data);
	}
	if (--events_dispatching == 0)
		events_free_dead();
	event_payload_free(ep);
}

/* Fire a client event. */
void
events_fire_client(const char *name, struct client *c)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	cmd_find_from_client(&fs, c, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_client(ep, "client", c);
	if (fs.s != NULL)
		event_payload_set_session(ep, "session", fs.s);
	if (fs.w != NULL)
		event_payload_set_window(ep, "window", fs.w);
	if (fs.wl != NULL)
		event_payload_set_int(ep, "window_index", fs.wl->idx);
	else if (fs.idx != -1)
		event_payload_set_int(ep, "window_index", fs.idx);
	if (fs.wp != NULL)
		event_payload_set_pane(ep, "pane", fs.wp);
	events_fire(name, ep);
}

/* Fire a session event. */
void
events_fire_session(const char *name, struct session *s)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	if (session_alive(s)) {
		cmd_find_from_session(&fs, s, 0);
		event_payload_set_target(ep, &fs);
	}
	event_payload_set_session(ep, "session", s);
	events_fire(name, ep);
}

/* Fire a window event. */
void
events_fire_window(const char *name, struct window *w)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	cmd_find_from_window(&fs, w, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_window(ep, "window", w);
	events_fire(name, ep);
}

/* Fire a pane event. */
void
events_fire_pane(const char *name, struct window_pane *wp)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	cmd_find_from_pane(&fs, wp, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_pane(ep, "pane", wp);
	event_payload_set_window(ep, "window", wp->window);
	events_fire(name, ep);
}

/* Fire a winlink event. */
void
events_fire_winlink(const char *name, struct winlink *wl)
{
	struct event_payload	*ep;
	struct cmd_find_state	 fs;

	ep = event_payload_create();
	cmd_find_from_winlink(&fs, wl, 0);
	event_payload_set_target(ep, &fs);
	event_payload_set_session(ep, "session", wl->session);
	event_payload_set_window(ep, "window", wl->window);
	event_payload_set_int(ep, "window_index", wl->idx);
	events_fire(name, ep);
}
