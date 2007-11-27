/* $Id: server-fn.c,v 1.35 2007-11-27 19:23:34 nicm Exp $ */

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

/* Find session from command message. */
struct session *
server_extract_session(struct msg_command_data *data, char *name, char **cause)
{
	struct session *s;
	u_int		i, n;

	if (name != NULL) {
		if ((s = session_find(name)) == NULL) {
			xasprintf(cause, "session not found: %s", name);
			return (NULL);
		}
		return (s);
	}

	if (data->pid != -1) {
		if (data->pid != getpid()) {
			xasprintf(cause, "wrong server: %lld", data->pid);
			return (NULL);
		}
		if (data->idx > ARRAY_LENGTH(&sessions)) {
			xasprintf(cause, "index out of range: %d", data->idx);
			return (NULL);
		}
		if ((s = ARRAY_ITEM(&sessions, data->idx)) == NULL) {
			xasprintf(
			    cause, "session doesn't exist: %u", data->idx);
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
		if (c != NULL && c->session == s)
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
server_clear_client(struct client *c)
{
	struct screen_draw_ctx	ctx;

	screen_draw_start_client(&ctx, c, 0, 0);
	screen_draw_clear_screen(&ctx);
	screen_draw_stop(&ctx);

	status_write_client(c);
}

void
server_redraw_client(struct client *c)
{
	struct screen_draw_ctx	ctx;
	struct window	       *w = c->session->curw->window;

	screen_draw_start_client(&ctx, c, 0, 0);
	window_draw(w, &ctx, 0, screen_size_y(&w->screen));
	screen_draw_stop(&ctx);

	status_write_client(c);
}

void
server_status_client(struct client *c)
{
	status_write_client(c);
}

void
server_clear_session(struct session *s)
{
	struct screen_draw_ctx	ctx;

	screen_draw_start_session(&ctx, s, 0, 0);
	screen_draw_clear_screen(&ctx);
	screen_draw_stop(&ctx);

	status_write_session(s);
}

void
server_redraw_session(struct session *s)
{
	struct screen_draw_ctx	ctx;
	struct window	       *w = s->curw->window;

	screen_draw_start_session(&ctx, s, 0, 0);
	window_draw(w, &ctx, 0, screen_size_y(&w->screen));
	screen_draw_stop(&ctx);

	status_write_session(s);
}

void
server_status_session(struct session *s)
{
	status_write_session(s);
}

void
server_clear_window(struct window *w)
{
	struct screen_draw_ctx	ctx;

	screen_draw_start_window(&ctx, w, 0, 0);
	screen_draw_clear_screen(&ctx);
	screen_draw_stop(&ctx);

	status_write_window(w);
}

void
server_redraw_window(struct window *w)
{
	struct screen_draw_ctx	ctx;

	screen_draw_start_window(&ctx, w, 0, 0);
	window_draw(w, &ctx, 0, screen_size_y(&w->screen));
	screen_draw_stop(&ctx);

	status_write_window(w);
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
	struct screen_draw_ctx	ctx;
	va_list			ap;
	char		       *msg;
	size_t			size;

	screen_draw_start_client(&ctx, c, 0, 0);
	screen_draw_move_cursor(&ctx, 0, c->sy - 1);
	screen_draw_set_attributes(&ctx, ATTR_REVERSE, 0x88);

	va_start(ap, fmt);
	xvasprintf(&msg, fmt, ap);
	va_end(ap);
	
	size = strlen(msg);
	if (size < c->sx - 1) {
		msg = xrealloc(msg, 1, c->sx);
		msg[c->sx - 1] = '\0';
		memset(msg + size, SCREEN_DEFDATA, (c->sx - 1) - size);
	}
	screen_draw_write_string(&ctx, "%s", msg);
	xfree(msg);

	buffer_flush(c->tty.fd, c->tty.in, c->tty.out);
	usleep(750000);	

	if (status_lines == 0) {
		window_draw(c->session->curw->window, &ctx, c->sy - 1, 1);
		screen_draw_stop(&ctx);
	} else {
		screen_draw_stop(&ctx);
		status_write_client(c);
	}
}
