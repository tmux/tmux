/* $OpenBSD: control-notify.c,v 1.37 2026/07/10 13:38:45 nicm Exp $ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicholas.marriott@gmail.com>
 * Copyright (c) 2012 George Nachman <tmux@georgester.com>
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

#include <stdlib.h>

#include "tmux.h"

/* Should this client be sent events? */
#define CONTROL_SHOULD_NOTIFY_CLIENT(c) \
	((c) != NULL && \
	 ((c)->flags & CLIENT_CONTROL) && \
	 (~(c)->flags & CLIENT_EXIT) && \
	 (c)->control_state != NULL)

/* Notify control clients that pane mode changed. */
static void
control_pane_mode_changed_cb(__unused const char *name,
    struct event_payload *ep, __unused void *sink_data)
{
	struct window_pane	*wp;
	struct client		*c;
	char			*value;

	wp = event_payload_get_pane(ep, "pane");
	if (wp != NULL) {
		TAILQ_FOREACH(c, &clients, entry) {
			if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
				continue;
			control_write(c, "%%pane-mode-changed %%%u", wp->id);
		}
		return;
	}

	value = event_payload_print(ep, "pane");
	if (value == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (CONTROL_SHOULD_NOTIFY_CLIENT(c))
			control_write(c, "%%pane-mode-changed %s", value);
	}
	free(value);
}

/* Notify control clients that window layout changed. */
static void
control_window_layout_changed_cb(__unused const char *name,
    struct event_payload *ep, __unused void *sink_data)
{
	struct client		*c;
	struct session		*s;
	struct winlink		*wl;
	struct window		*w = event_payload_get_window(ep, "window");
	const char		*template;
	char			*cp;

	if (w == NULL)
		return;

	template = "%layout-change #{window_id} #{window_layout} "
	    "#{window_visible_layout} #{window_raw_flags}";

	/*
	 * When the last pane in a window is closed it won't have a layout root
	 * and we don't need to inform the client about the layout change
	 * because the whole window will go away soon.
	 */
	wl = TAILQ_FIRST(&w->winlinks);
	if (wl == NULL || w->layout_root == NULL)
		return;
	cp = format_single(NULL, template, NULL, NULL, wl, NULL);

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
			continue;
		s = c->session;
		if (winlink_find_by_window_id(&s->windows, w->id) != NULL)
			control_write(c, "%s", cp);
	}
	free(cp);
}

/* Notify control clients that window pane changed. */
static void
control_window_pane_changed_cb(__unused const char *name,
    struct event_payload *ep, __unused void *sink_data)
{
	struct client	*c;
	struct window	*w = event_payload_get_window(ep, "window");

	if (w == NULL || w->active == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%window-pane-changed @%u %%%u", w->id,
		    w->active->id);
	}
}

/* Notify control clients that a window was unlinked. */
static void
control_window_unlinked_cb(__unused const char *name, struct event_payload *ep,
    __unused void *sink_data)
{
	struct client		*c;
	struct session		*cs;
	struct window		*w = event_payload_get_window(ep, "window");

	if (w == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
			continue;
		cs = c->session;

		if (winlink_find_by_window_id(&cs->windows, w->id) != NULL)
			control_write(c, "%%window-close @%u", w->id);
		else
			control_write(c, "%%unlinked-window-close @%u", w->id);
	}
}

/* Notify control clients that a window was linked. */
static void
control_window_linked_cb(__unused const char *name, struct event_payload *ep,
    __unused void *sink_data)
{
	struct client		*c;
	struct session		*cs;
	struct window		*w = event_payload_get_window(ep, "window");

	if (w == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
			continue;
		cs = c->session;

		if (winlink_find_by_window_id(&cs->windows, w->id) != NULL)
			control_write(c, "%%window-add @%u", w->id);
		else
			control_write(c, "%%unlinked-window-add @%u", w->id);
	}
}

/* Notify control clients that a window was renamed. */
static void
control_window_renamed_cb(__unused const char *name, struct event_payload *ep,
    __unused void *sink_data)
{
	struct client		*c;
	struct session		*cs;
	struct window		*w = event_payload_get_window(ep, "window");

	if (w == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
			continue;
		cs = c->session;

		if (winlink_find_by_window_id(&cs->windows, w->id) != NULL) {
			control_write(c, "%%window-renamed @%u %s", w->id,
			    w->name);
		} else {
			control_write(c, "%%unlinked-window-renamed @%u %s",
			    w->id, w->name);
		}
	}
}

/* Notify control clients that a client changed session. */
static void
control_client_session_changed_cb(__unused const char *name,
    struct event_payload *ep, __unused void *sink_data)
{
	struct client		*cc = event_payload_get_client(ep, "client");
	struct client		*c;
	struct session		*s;

	if (cc == NULL || cc->session == NULL)
		return;
	s = cc->session;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
			continue;

		if (cc == c) {
			control_write(c, "%%session-changed $%u %s", s->id,
			    s->name);
		} else {
			control_write(c, "%%client-session-changed %s $%u %s",
			    cc->name, s->id, s->name);
		}
	}
}

/* Notify control clients that a client detached. */
static void
control_client_detached_cb(__unused const char *name, struct event_payload *ep,
    __unused void *sink_data)
{
	struct client		*cc = event_payload_get_client(ep, "client");
	struct client		*c;

	if (cc == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (CONTROL_SHOULD_NOTIFY_CLIENT(c))
			control_write(c, "%%client-detached %s", cc->name);
	}
}

/* Notify control clients that a session was renamed. */
static void
control_session_renamed_cb(__unused const char *name, struct event_payload *ep,
    __unused void *sink_data)
{
	struct session		*s = event_payload_get_session(ep, "session");
	struct client		*c;

	if (s == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%session-renamed $%u %s", s->id, s->name);
	}
}

/* Notify control clients that sessions changed. */
static void
control_session_created_cb(__unused const char *name,
    __unused struct event_payload *ep, __unused void *sink_data)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%sessions-changed");
	}
}

/* Notify control clients that sessions changed. */
static void
control_session_closed_cb(__unused const char *name,
    __unused struct event_payload *ep, __unused void *sink_data)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%sessions-changed");
	}
}

/* Notify control clients that the current window changed. */
static void
control_session_window_changed_cb(__unused const char *name,
    struct event_payload *ep, __unused void *sink_data)
{
	struct session	*s = event_payload_get_session(ep, "session");
	struct client	*c;

	/*
	 * A deferred session-window-changed notification can fire after the
	 * session has been destroyed (which sets curw to NULL) but is kept
	 * alive by the notification's reference. Skip the notification.
	 */
	if (s == NULL || s->curw == NULL)
		return;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%session-window-changed $%u @%u", s->id,
		    s->curw->window->id);
	}
}

/* Notify control clients that a paste buffer changed. */
static void
control_paste_buffer_changed_cb(__unused const char *name,
    struct event_payload *ep, __unused void *sink_data)
{
	const char	*pbname = event_payload_get_string(ep, "paste_buffer");
	struct client	*c;

	if (pbname == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%paste-buffer-changed %s", pbname);
	}
}

/* Notify control clients that a paste buffer was deleted. */
static void
control_paste_buffer_deleted_cb(__unused const char *name,
    struct event_payload *ep, __unused void *sink_data)
{
	const char	*pbname = event_payload_get_string(ep, "paste_buffer");
	struct client	*c;

	if (pbname == NULL)
		return;
	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%paste-buffer-deleted %s", pbname);
	}
}

/* Build control event sinks. */
void
control_build_events(void)
{
	/* Control event sink callbacks. */
	static struct {
		const char	*name;
		events_cb	 cb;
	} events[] = {
		{ "pane-mode-changed", control_pane_mode_changed_cb },
		{ "window-layout-changed", control_window_layout_changed_cb },
		{ "window-pane-changed", control_window_pane_changed_cb },
		{ "window-unlinked", control_window_unlinked_cb },
		{ "window-linked", control_window_linked_cb },
		{ "window-renamed", control_window_renamed_cb },
		{ "client-session-changed", control_client_session_changed_cb },
		{ "client-detached", control_client_detached_cb },
		{ "session-renamed", control_session_renamed_cb },
		{ "session-created", control_session_created_cb },
		{ "session-closed", control_session_closed_cb },
		{ "session-window-changed", control_session_window_changed_cb },
		{ "paste-buffer-changed", control_paste_buffer_changed_cb },
		{ "paste-buffer-deleted", control_paste_buffer_deleted_cb }
	};
	u_int	i;

	for (i = 0; i < nitems(events); i++)
		events_add_sink(events[i].name, events[i].cb, NULL);
}
