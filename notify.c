/* $OpenBSD$ */

/*
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

enum notify_type {
	NOTIFY_WINDOW_LAYOUT_CHANGED,
	NOTIFY_WINDOW_UNLINKED,
	NOTIFY_WINDOW_LINKED,
	NOTIFY_WINDOW_RENAMED,
	NOTIFY_ATTACHED_SESSION_CHANGED,
	NOTIFY_SESSION_RENAMED,
	NOTIFY_SESSION_CREATED,
	NOTIFY_SESSION_CLOSED
};

struct notify_entry {
	enum notify_type	 type;

	struct client		*client;
	struct session		*session;
	struct window		*window;

	TAILQ_ENTRY(notify_entry) entry;
};
TAILQ_HEAD(, notify_entry) notify_queue = TAILQ_HEAD_INITIALIZER(notify_queue);
int	notify_enabled = 1;

void	notify_drain(void);
void	notify_add(enum notify_type, struct client *, struct session *,
	    struct window *);

void
notify_enable(void)
{
	notify_enabled = 1;
	notify_drain();
}

void
notify_disable(void)
{
	notify_enabled = 0;
}

void
notify_add(enum notify_type type, struct client *c, struct session *s,
    struct window *w)
{
	struct notify_entry	*ne;

	ne = xcalloc(1, sizeof *ne);
	ne->type = type;
	ne->client = c;
	ne->session = s;
	ne->window = w;
	TAILQ_INSERT_TAIL(&notify_queue, ne, entry);

	if (c != NULL)
		c->references++;
	if (s != NULL)
		s->references++;
	if (w != NULL)
		w->references++;
}

void
notify_drain(void)
{
	struct notify_entry	*ne, *ne1;

	if (!notify_enabled)
		return;

	TAILQ_FOREACH_SAFE(ne, &notify_queue, entry, ne1) {
		switch (ne->type) {
		case NOTIFY_WINDOW_LAYOUT_CHANGED:
			control_notify_window_layout_changed(ne->window);
			break;
		case NOTIFY_WINDOW_UNLINKED:
			control_notify_window_unlinked(ne->session, ne->window);
			break;
		case NOTIFY_WINDOW_LINKED:
			control_notify_window_linked(ne->session, ne->window);
			break;
		case NOTIFY_WINDOW_RENAMED:
			control_notify_window_renamed(ne->window);
			break;
		case NOTIFY_ATTACHED_SESSION_CHANGED:
			control_notify_attached_session_changed(ne->client);
			break;
		case NOTIFY_SESSION_RENAMED:
			control_notify_session_renamed(ne->session);
			break;
		case NOTIFY_SESSION_CREATED:
			control_notify_session_created(ne->session);
			break;
		case NOTIFY_SESSION_CLOSED:
			control_notify_session_close(ne->session);
			break;
		}

		if (ne->client != NULL)
			server_client_unref(ne->client);
		if (ne->session != NULL)
			session_unref(ne->session);
		if (ne->window != NULL)
			window_remove_ref(ne->window);

		TAILQ_REMOVE(&notify_queue, ne, entry);
		free(ne);
	}
}

void
notify_input(struct window_pane *wp, struct evbuffer *input)
{
	struct client	*c;

	/*
	 * notify_input() is not queued and only does anything when
	 * notifications are enabled.
	 */
	if (!notify_enabled)
		return;

	TAILQ_FOREACH(c, &clients, entry) {
		if (c->flags & CLIENT_CONTROL)
			control_notify_input(c, wp, input);
	}
}

void
notify_window_layout_changed(struct window *w)
{
	notify_add(NOTIFY_WINDOW_LAYOUT_CHANGED, NULL, NULL, w);
	notify_drain();
}

void
notify_window_unlinked(struct session *s, struct window *w)
{
	notify_add(NOTIFY_WINDOW_UNLINKED, NULL, s, w);
	notify_drain();
}

void
notify_window_linked(struct session *s, struct window *w)
{
	notify_add(NOTIFY_WINDOW_LINKED, NULL, s, w);
	notify_drain();
}

void
notify_window_renamed(struct window *w)
{
	notify_add(NOTIFY_WINDOW_RENAMED, NULL, NULL, w);
	notify_drain();
}

void
notify_attached_session_changed(struct client *c)
{
	notify_add(NOTIFY_ATTACHED_SESSION_CHANGED, c, NULL, NULL);
	notify_drain();
}

void
notify_session_renamed(struct session *s)
{
	notify_add(NOTIFY_SESSION_RENAMED, NULL, s, NULL);
	notify_drain();
}

void
notify_session_created(struct session *s)
{
	notify_add(NOTIFY_SESSION_CREATED, NULL, s, NULL);
	notify_drain();
}

void
notify_session_closed(struct session *s)
{
	notify_add(NOTIFY_SESSION_CLOSED, NULL, s, NULL);
	notify_drain();
}
