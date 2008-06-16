/* $Id: server-fn.c,v 1.42 2008-06-16 17:35:40 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <string.h>
#include <unistd.h>

#include "tmux.h"

void
server_write_client(
    struct client *c, enum hdrtype type, const void *buf, size_t len)
{
	struct hdr	 hdr;

	log_debug("writing %d to client %d", type, c->fd);

	hdr.type = type;
	hdr.size = len;

	buffer_write(c->out, &hdr, sizeof hdr);
	if (buf != NULL)
		buffer_write(c->out, buf, len);
}

void
server_write_session(
    struct session *s, enum hdrtype type, const void *buf, size_t len)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session == s)
			server_write_client(c, type, buf, len);
	}
}

void
server_write_window(
    struct window *w, enum hdrtype type, const void *buf, size_t len)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session->curw->window == w)
			server_write_client(c, type, buf, len);
	}
}

void
server_redraw_client(struct client *c)
{
	c->flags |= CLIENT_REDRAW;
}

void
server_status_client(struct client *c)
{
	c->flags |= CLIENT_STATUS;
}

void
server_redraw_session(struct session *s)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session == s)
			server_redraw_client(c);
	}
}

void
server_status_session(struct session *s)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session == s)
			server_status_client(c);
	}
}

void
server_redraw_window(struct window *w)
{
	struct client	*c;
	u_int		 i;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c == NULL || c->session == NULL)
			continue;
		if (c->session->curw->window == w)
			server_redraw_client(c);
	}
}

void
server_status_window(struct window *w)
{
	struct session	*s;
	u_int		 i;

	/*
	 * This is slightly different. We want to redraw the status line of any
	 * clients containing this window rather than any where it is the
	 * current window.
	 */

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s != NULL && session_has(s, w))
			server_status_session(s);
	}
}

void printflike2
server_write_message(struct client *c, const char *fmt, ...)
{
	struct screen_redraw_ctx	ctx;
	va_list				ap;
	char			       *msg;
	size_t				size;
	u_int				slines;

	slines = options_get_number(&c->session->options, "status-lines");

	screen_redraw_start_client(&ctx, c);

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	msg = xrealloc(msg, 1, c->sx + 1);
	msg[c->sx] = '\0';

	size = strlen(msg);
	if (size < c->sx)
		memset(msg + size, ' ', c->sx - size);

	screen_redraw_move_cursor(&ctx, 0, c->sy - 1);
	screen_redraw_set_attributes(&ctx, ATTR_REVERSE, 0x88);
	screen_redraw_write_string(&ctx, "%s", msg);

	buffer_flush(c->tty.fd, c->tty.in, c->tty.out);
	usleep(750000);

	memset(msg, ' ', c->sx);

	screen_redraw_move_cursor(&ctx, 0, c->sy - 1);
	screen_redraw_set_attributes(&ctx, 0, 0x88);
	screen_redraw_write_string(&ctx, "%s", msg);

	xfree(msg);

	if (slines == 0) {
		screen_redraw_lines(&ctx, c->sy - 1, 1);
		screen_redraw_stop(&ctx);
	} else {
		screen_redraw_stop(&ctx);
		server_status_client(c);
	}
}
