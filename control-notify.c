/* $OpenBSD$ */

/*
 * Copyright (c) 2012 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include "tmux.h"

#define CONTROL_SHOULD_NOTIFY_CLIENT(c) \
	((c) != NULL && ((c)->flags & CLIENT_CONTROL))

void
control_notify_input(struct client *c, struct window_pane *wp,
    struct evbuffer *input)
{
	u_char		*buf;
	size_t		 len;
	struct evbuffer *message;
	u_int		 i;

	if (c->session == NULL)
	    return;

	buf = EVBUFFER_DATA(input);
	len = EVBUFFER_LENGTH(input);

	/*
	 * Only write input if the window pane is linked to a window belonging
	 * to the client's session.
	 */
	if (winlink_find_by_window(&c->session->windows, wp->window) != NULL) {
		message = evbuffer_new();
		evbuffer_add_printf(message, "%%output %%%u ", wp->id);
		for (i = 0; i < len; i++) {
			if (buf[i] < ' ' || buf[i] == '\\')
			    evbuffer_add_printf(message, "\\%03o", buf[i]);
			else
			    evbuffer_add_printf(message, "%c", buf[i]);
		}
		control_write_buffer(c, message);
		evbuffer_free(message);
	}
}

void
control_notify_window_layout_changed(struct window *w)
{
	struct client		*c;
	struct session		*s;
	struct format_tree	*ft;
	struct winlink		*wl;
	u_int			 i;
	const char		*template;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
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
		template = "%layout-change #{window_id} #{window_layout}";

		ft = format_create();
		wl = winlink_find_by_window(&s->windows, w);
		if (wl != NULL) {
			format_winlink(ft, c->session, wl);
			control_write(c, "%s", format_expand(ft, template));
		}
		format_free(ft);
	}
}

void
control_notify_window_unlinked(unused struct session *s, struct window *w)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
			continue;

		control_write(c, "%%window-close @%u", w->id);
	}
}

void
control_notify_window_linked(unused struct session *s, struct window *w)
{
	struct client	*c;
	struct session	*cs;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
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
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
			continue;

		control_write(c, "%%window-renamed @%u %s", w->id, w->name);
	}
}

void
control_notify_attached_session_changed(struct client *c)
{
	struct session	*s;

	if (!CONTROL_SHOULD_NOTIFY_CLIENT(c) || c->session == NULL)
		return;
	s = c->session;

	control_write(c, "%%session-changed $%u %s", s->id, s->name);
}

void
control_notify_session_renamed(struct session *s)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%session-renamed $%u %s", s->id, s->name);
	}
}

void
control_notify_session_created(unused struct session *s)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%sessions-changed");
	}
}

void
control_notify_session_close(unused struct session *s)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (!CONTROL_SHOULD_NOTIFY_CLIENT(c))
			continue;

		control_write(c, "%%sessions-changed");
	}
}
