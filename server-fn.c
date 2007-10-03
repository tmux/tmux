/* $Id: server-fn.c,v 1.14 2007-10-03 21:31:07 nicm Exp $ */

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

/* Find session from sessid. */
struct session *
server_find_sessid(struct sessid *sid, char **cause)
{
	struct session *s;
	u_int		i, n;

	if (*sid->name != '\0') {
		sid->name[(sizeof sid->name) - 1] = '\0';
		if ((s = session_find(sid->name)) == NULL) {
			xasprintf(cause, "session not found: %s", sid->name);
			return (NULL);
		}
		return (s);
	}

	if (sid->pid != -1) {
		if (sid->pid != getpid()) {
			xasprintf(cause, "wrong server: %lld", sid->pid);
			return (NULL);
		}
		if (sid->idx > ARRAY_LENGTH(&sessions)) {
			xasprintf(cause, "index out of range: %u", sid->idx);
			return (NULL);
		}
		if ((s = ARRAY_ITEM(&sessions, sid->idx)) == NULL) {
			xasprintf(cause, "session doesn't exist: %u", sid->idx);
			return (NULL);
		}
		return (s);
	}

	s = NULL;
	n = 0;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if (ARRAY_ITEM(&sessions, i) != NULL) {
			s = ARRAY_ITEM(&sessions, i);
			n++;
		}
	}
	if (s == NULL) {
		xasprintf(cause, "no sessions found");
		return (NULL);
	}
	if (n != 1) {
		xasprintf(cause, "multiple sessions and session not specified");
		return (NULL);
	}
	return (s);		
}

/* Write command to a client. */
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

/* Write command to a client with two buffers. */
void
server_write_client2(struct client *c, enum hdrtype type,
    const void *buf1, size_t len1, const void *buf2, size_t len2)
{
	struct hdr	 hdr;

	log_debug("writing %d to client %d", type, c->fd);

	hdr.type = type;
	hdr.size = len1 + len2;

	buffer_write(c->out, &hdr, sizeof hdr);
	if (buf1 != NULL)
		buffer_write(c->out, buf1, len1);
	if (buf2 != NULL)
		buffer_write(c->out, buf2, len2);
}

/* Write command to all clients attached to a specific window. */
void
server_write_clients(
    struct window *w, enum hdrtype type, const void *buf, size_t len)
{
	struct client	*c;
 	struct hdr	 hdr;
	u_int		 i;

	hdr.type = type;
	hdr.size = len;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && c->session != NULL) {
			if (c->flags & CLIENT_HOLD) /* XXX OUTPUT only */
				continue;
			if (c->session->window == w) {
				log_debug(
				    "writing %d to clients: %d", type, c->fd);
				buffer_write(c->out, &hdr, sizeof hdr);
				if (buf != NULL)
					buffer_write(c->out, buf, len);
			}
		}
	}
}

/* Changed client window. */
void
server_window_changed(struct client *c)
{
	struct window	*w;

	w = c->session->window;
	if (c->sx != w->screen.sx || c->sy != w->screen.sy)
		window_resize(w, c->sx, c->sy);
	server_draw_client(c);
}

/* Draw window on client. */
void
server_draw_client(struct client *c)
{
	struct hdr	 hdr;
	size_t		 size;
	struct screen	*s = &c->session->window->screen;

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	screen_draw(s, c->out, 0, s->sy - 1);

	size = BUFFER_USED(c->out) - size;
	log_debug("redrawing screen, %zu bytes", size);
	if (size != 0) {
		hdr.type = MSG_DATA;
		hdr.size = size;
		memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	} else
		buffer_reverse_add(c->out, sizeof hdr);

	server_draw_status(c);
}

/* Draw status line. */
void
server_draw_status(struct client *c)
{
	struct hdr	hdr;
	size_t		size;

	if (status_lines == 0)
		return;

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	status_write(c);

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_DATA;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
}

/* Send error message command to client. */
void
server_write_error(struct client *c, const char *fmt, ...)
{
	va_list	ap;
	char   *msg;

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);

	server_write_client(c, MSG_ERROR, msg, strlen(msg));
	xfree(msg);
}

/* Write message command to a client. */
void
server_write_message(struct client *c, const char *fmt, ...)
{
	struct hdr	 hdr;
	va_list		 ap;
	char		*msg;
	size_t		 size;
	u_int		 i;

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	input_store_zero(c->out, CODE_CURSOROFF);
	input_store_two(c->out, CODE_CURSORMOVE, c->sy + status_lines, 1);
	input_store_two(c->out, CODE_ATTRIBUTES, ATTR_REVERSE, 0x88);
	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);
	if (strlen(msg) > c->sx - 1)
		msg[c->sx - 1] = '\0';
	buffer_write(c->out, msg, strlen(msg));
	for (i = strlen(msg); i < c->sx; i++)
		input_store8(c->out, ' ');
	xfree(msg);

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_DATA;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);

	hdr.type = MSG_PAUSE;
	hdr.size = 0;
	buffer_write(c->out, &hdr, sizeof hdr);

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	if (status_lines == 0) {
		screen_draw(
		    &c->session->window->screen, c->out, c->sy - 1, c->sy - 1);
	} else
		status_write(c);

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_DATA;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
}
