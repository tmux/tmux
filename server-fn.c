/* $Id: server-fn.c,v 1.2 2007-09-26 18:09:23 nicm Exp $ */

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

#include "tmux.h"

/* Write command to a client. */
void
server_write_client(struct client *c, u_int cmd, void *buf, size_t len)
{
	struct hdr	 hdr;

	hdr.type = cmd;
	hdr.size = len;

	buffer_write(c->out, &hdr, sizeof hdr);
	if (buf != NULL)
		buffer_write(c->out, buf, len);
}

/* Write command to a client with two buffers. */
void
server_write_client2(struct client *c,
    u_int cmd, void *buf1, size_t len1, void *buf2, size_t len2)
{
	struct hdr	 hdr;

	hdr.type = cmd;
	hdr.size = len1 + len2;

	buffer_write(c->out, &hdr, sizeof hdr);
	if (buf1 != NULL)
		buffer_write(c->out, buf1, len1);
	if (buf2 != NULL)
		buffer_write(c->out, buf2, len2);
}

/* Write command to all clients attached to a specific window. */
void
server_write_clients(struct window *w, u_int cmd, void *buf, size_t len)
{
	struct client	*c;
 	struct hdr	 hdr;
	u_int		 i;

	hdr.type = cmd;
	hdr.size = len;

	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && c->session != NULL) {
			if (c->session->window == w) {
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
	server_draw_client(c, 0, c->sy - 1);
}

/* Draw window on client. */
void
server_draw_client(struct client *c, u_int py_upper, u_int py_lower)
{
	struct hdr	hdr;
	size_t		size;

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	screen_draw(&c->session->window->screen, c->out, py_upper, py_lower);

	size = BUFFER_USED(c->out) - size;
	log_debug("redrawing screen, %zu bytes", size);
	if (size != 0) {
		hdr.type = MSG_OUTPUT;
		hdr.size = size;
		memcpy(
		    BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
	} else
		buffer_reverse_add(c->out, sizeof hdr);
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
	input_store_two(c->out, CODE_CURSORMOVE, c->sy, 1);
	input_store_one(c->out, CODE_ATTRIBUTES, 2);
	input_store16(c->out, 0);
	input_store16(c->out, 7);
	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);
	buffer_write(c->out, msg, strlen(msg));
	for (i = strlen(msg); i < c->sx; i++)
		input_store8(c->out, ' ');
	xfree(msg);

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_OUTPUT;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);

	hdr.type = MSG_PAUSE;
	hdr.size = 0;
	buffer_write(c->out, &hdr, sizeof hdr);

	buffer_ensure(c->out, sizeof hdr);
	buffer_add(c->out, sizeof hdr);
	size = BUFFER_USED(c->out);

	screen_draw(&c->session->window->screen, c->out, c->sy - 1, c->sy - 1);

	size = BUFFER_USED(c->out) - size;
	hdr.type = MSG_OUTPUT;
	hdr.size = size;
	memcpy(BUFFER_IN(c->out) - size - sizeof hdr, &hdr, sizeof hdr);
}
