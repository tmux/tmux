/* $OpenBSD$ */

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

#define CONTROL_SHOULD_NOTIFY_CLIENT(c) \
	((c) != NULL && ((c)->flags & CLIENT_CONTROL))

void
control_notify_pane_mode_changed(int pane)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%pane-mode-changed %%%u", pane);
	}
}

void
control_notify_window_layout_changed(struct window *w)
{
	struct client	*c;
	struct session	*s;
	struct winlink	*wl;
	const char	*template;
	char		*cp;

	template = "%layout-change #{window_id} #{window_layout} "
	    "#{window_visible_layout} #{window_raw_flags}";

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
			continue;
		s = c->session;

		if (winlink_find_by_window_id(&s->windows, w->id) == NULL)
			continue;

		/*
		 * When the last pane in a window is closed it won't have a
		 * layout root and we don't need to inform the client about the
		 * layout change because the whole window will go away soon.
		 */
		if (w->layout_root == NULL)
			continue;

		wl = winlink_find_by_window(&s->windows, w);
		if (wl != NULL) {
			cp = format_single(NULL, template, c, NULL, wl, NULL);
			control_write(c, "%s", cp);
			free(cp);
		}
	}
}

void
control_notify_window_pane_changed(struct window *w)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%window-pane-changed @%u %%%u", w->id,
		    w->active->id);
	}
}

void
control_notify_window_unlinked(__unused struct session *s, struct window *w)
{
	struct client	*c;
	struct session	*cs;

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

void
control_notify_window_linked(__unused struct session *s, struct window *w)
{
	struct client	*c;
	struct session	*cs;

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

void
control_notify_window_renamed(struct window *w)
{
	struct client	*c;
	struct session	*cs;

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

void
control_notify_client_session_changed(struct client *cc)
{
	struct client	*c;
	struct session	*s;

	if (cc->session == NULL)
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

void
control_notify_client_detached(struct client *cc)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (CONTROL_SHOULD_NOTIFY_CLIENT(c))
			control_write(c, "%%client-detached %s", cc->name);
	}
}

void
control_notify_session_renamed(struct session *s)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%session-renamed $%u %s", s->id, s->name);
	}
}

void
control_notify_session_created(__unused struct session *s)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%sessions-changed");
	}
}

void
control_notify_session_closed(__unused struct session *s)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%sessions-changed");
	}
}

void
control_notify_session_window_changed(struct session *s)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%session-window-changed $%u @%u", s->id,
		    s->curw->window->id);
	}
}

void
control_notify_paste_buffer_changed(const char *name)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%paste-buffer-changed %s", name);
	}
}

void
control_notify_paste_buffer_deleted(const char *name)
{
	struct client	*c;

	TAILQ_FOREACH(c, &clients, entry) {
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%paste-buffer-deleted %s", name);
	}
}

void
control_notify_popup(struct client *c, int status, char *buf, size_t len,
                     int wp)
{
	struct evbuffer *message = evbuffer_new();

	if (message == NULL)
		fatalx("out of memory");
	evbuffer_add_printf(message, "%%popup %d", status);
	if (wp != -1)
		evbuffer_add_printf(message, " %u", wp);
	evbuffer_add_printf(message, " : ");
	control_escape(message, buf, len);
	control_write_buffer(c, message);
	evbuffer_free(message);
}
