/* $Id: server-msg.c,v 1.15 2007-10-03 09:17:00 nicm Exp $ */

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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

int	server_msg_fn_attach(struct hdr *, struct client *);
int	server_msg_fn_create(struct hdr *, struct client *);
int	server_msg_fn_input(struct hdr *, struct client *);
int	server_msg_fn_last(struct hdr *, struct client *);
int	server_msg_fn_new(struct hdr *, struct client *);
int	server_msg_fn_next(struct hdr *, struct client *);
int	server_msg_fn_previous(struct hdr *, struct client *);
int	server_msg_fn_refresh(struct hdr *, struct client *);
int	server_msg_fn_rename(struct hdr *, struct client *);
int	server_msg_fn_select(struct hdr *, struct client *);
int	server_msg_fn_sessions(struct hdr *, struct client *);
int	server_msg_fn_size(struct hdr *, struct client *);
int	server_msg_fn_windowlist(struct hdr *, struct client *);
int	server_msg_fn_windowinfo(struct hdr *, struct client *);
int	server_msg_fn_windows(struct hdr *, struct client *);

struct server_msg {
	enum hdrtype	type;
	
	int	        (*fn)(struct hdr *, struct client *);
};
const struct server_msg server_msg_table[] = {
	{ MSG_ATTACH, server_msg_fn_attach },
	{ MSG_CREATE, server_msg_fn_create },
	{ MSG_INPUT, server_msg_fn_input },
	{ MSG_LAST, server_msg_fn_last },
	{ MSG_NEW, server_msg_fn_new },
	{ MSG_NEXT, server_msg_fn_next },
	{ MSG_PREVIOUS, server_msg_fn_previous },
	{ MSG_REFRESH, server_msg_fn_refresh },
	{ MSG_RENAME, server_msg_fn_rename },
	{ MSG_SELECT, server_msg_fn_select },
	{ MSG_SESSIONS, server_msg_fn_sessions },
	{ MSG_SIZE, server_msg_fn_size },
	{ MSG_WINDOWLIST, server_msg_fn_windowlist },
	{ MSG_WINDOWINFO, server_msg_fn_windowinfo },
	{ MSG_WINDOWS, server_msg_fn_windows },
};
#define NSERVERMSG (sizeof server_msg_table / sizeof server_msg_table[0])

int
server_msg_dispatch(struct client *c)
{
	struct hdr		 hdr;
	const struct server_msg	*msg;
	u_int		 	 i;
	int			 n;

	for (;;) {
		if (BUFFER_USED(c->in) < sizeof hdr)
			return (0);
		memcpy(&hdr, BUFFER_OUT(c->in), sizeof hdr);
		if (BUFFER_USED(c->in) < (sizeof hdr) + hdr.size)
			return (0);
		buffer_remove(c->in, sizeof hdr);
		
		for (i = 0; i < NSERVERMSG; i++) {
			msg = server_msg_table + i;
			if (msg->type == hdr.type) {
				if ((n = msg->fn(&hdr, c)) != 0)
					return (n);
				break;
			}
		}	
		if (i == NSERVERMSG)
			fatalx("unexpected message");
	}
}

/* New message from client. */
int
server_msg_fn_new(struct hdr *hdr, struct client *c)
{
	struct new_data	 data;
	const char      *shell;
	char		*cmd, *msg;
	
	if (c->session != NULL)
		return (0);
	if (hdr->size != sizeof data)
		fatalx("bad MSG_NEW size");
	buffer_read(c->in, &data, hdr->size);

	c->sx = data.sx;
	if (c->sx == 0)
		c->sx = 80;
	c->sy = data.sy;
	if (c->sy == 0)
		c->sy = 25;

	if (c->sy >= status_lines)
		c->sy -= status_lines;

	data.name[(sizeof data.name) - 1] = '\0';
	if (*data.name != '\0' && session_find(data.name) != NULL) {
		xasprintf(&msg, "duplicate session: %s", data.name);
		server_write_client(c, MSG_ERROR, msg, strlen(msg));
		xfree(msg);
		return (0);
	}

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = "/bin/ksh";
	xasprintf(&cmd, "%s -l", shell);
	c->session = session_create(data.name, cmd, c->sx, c->sy);
	if (c->session == NULL)
		fatalx("session_create failed");
	xfree(cmd);

	server_write_client(c, MSG_DONE, NULL, 0);
	server_draw_client(c, 0, c->sy - 1);

	return (0);
}

/* Attach message from client. */
int
server_msg_fn_attach(struct hdr *hdr, struct client *c)
{
	struct attach_data	 data;
	char			*cause;
	
	if (c->session != NULL)
		return (0);
	if (hdr->size != sizeof data)
		fatalx("bad MSG_ATTACH size");
	buffer_read(c->in, &data, hdr->size);

	c->sx = data.sx;
	if (c->sx == 0)
		c->sx = 80;
	c->sy = data.sy;
	if (c->sy == 0)
		c->sy = 25;

	if (c->sy >= status_lines)
		c->sy -= status_lines;

	if ((c->session = server_find_sessid(&data.sid, &cause)) == NULL) {
		server_write_error(c, "%s", cause);
		xfree(cause);
		return (0);
	}

	server_draw_client(c, 0, c->sy - 1);

	return (0);
}

/* Create message from client. */
int
server_msg_fn_create(struct hdr *hdr, struct client *c)
{
	const char	*shell;
	char		*cmd;

	if (c->session == NULL)
		return (0);
	if (hdr->size != 0)
		fatalx("bad MSG_CREATE size");

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = "/bin/ksh";
	xasprintf(&cmd, "%s -l", shell);
	if (session_new(c->session, cmd, c->sx, c->sy) != 0)
		fatalx("session_new failed");
	xfree(cmd);

	server_draw_client(c, 0, c->sy - 1);

	return (0);
}

/* Next message from client. */
int
server_msg_fn_next(struct hdr *hdr, struct client *c)
{
	if (c->session == NULL)
		return (0);
	if (hdr->size != 0)
		fatalx("bad MSG_NEXT size");

	if (session_next(c->session) == 0)
		server_window_changed(c);
	else
		server_write_message(c, "No next window"); 

	return (0);
}

/* Previous message from client. */
int
server_msg_fn_previous(struct hdr *hdr, struct client *c)
{
	if (c->session == NULL)
		return (0);
	if (hdr->size != 0)
		fatalx("bad MSG_PREVIOUS size");

	if (session_previous(c->session) == 0)
		server_window_changed(c);
	else
		server_write_message(c, "No previous window"); 

	return (0);
}

/* Size message from client. */
int
server_msg_fn_size(struct hdr *hdr, struct client *c)
{
	struct size_data	data;

	if (c->session == NULL)
		return (0);
	if (hdr->size != sizeof data)
		fatalx("bad MSG_SIZE size");
	buffer_read(c->in, &data, hdr->size);

	c->sx = data.sx;
	if (c->sx == 0)
		c->sx = 80;
	c->sy = data.sy;
	if (c->sy == 0)
		c->sy = 25;

	if (c->sy >= status_lines)
		c->sy -= status_lines;

	if (window_resize(c->session->window, c->sx, c->sy) != 0)
		server_draw_client(c, 0, c->sy - 1);

	return (0);
}

/* Input message from client. */
int
server_msg_fn_input(struct hdr *hdr, struct client *c)
{
	if (c->session == NULL)
		return (0);
	
	window_input(c->session->window, c->in, hdr->size);

	return (0);
}

/* Refresh message from client. */
int
server_msg_fn_refresh(struct hdr *hdr, struct client *c)
{
	struct refresh_data	data;

	if (c->session == NULL)
		return (0);
	if (hdr->size != 0 && hdr->size != sizeof data)
		fatalx("bad MSG_REFRESH size");

	server_draw_client(c, 0, c->sy - 1);

	return (0);
}

/* Select message from client. */
int
server_msg_fn_select(struct hdr *hdr, struct client *c)
{
	struct select_data	data;

	if (c->session == NULL)
		return (0);
	if (hdr->size != sizeof data)
		fatalx("bad MSG_SELECT size");
	buffer_read(c->in, &data, hdr->size);

	if (c->session == NULL)
		return (0);
	if (session_select(c->session, data.idx) == 0)
		server_window_changed(c);
	else
		server_write_message(c, "Window %u not present", data.idx); 

	return (0);
}

/* Sessions message from client. */
int
server_msg_fn_sessions(struct hdr *hdr, struct client *c)
{
	struct sessions_data	 data;
	struct sessions_entry	 entry;
	struct session		*s;
	u_int			 i, j;

	if (hdr->size != sizeof data)
		fatalx("bad MSG_SESSIONS size");
	buffer_read(c->in, &data, hdr->size);

	data.sessions = 0;
	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		if (ARRAY_ITEM(&sessions, i) != NULL)
			data.sessions++;
	}
	server_write_client2(c, MSG_SESSIONS,
	    &data, sizeof data, NULL, data.sessions * sizeof entry);

	for (i = 0; i < ARRAY_LENGTH(&sessions); i++) {
		s = ARRAY_ITEM(&sessions, i);
		if (s == NULL)
			continue;
		strlcpy(entry.name, s->name, sizeof entry.name);
		entry.tim = s->tim;
		entry.windows = 0;
		for (j = 0; j < ARRAY_LENGTH(&s->windows); j++) {
			if (ARRAY_ITEM(&s->windows, j) != NULL)
				entry.windows++;
		}
		buffer_write(c->out, &entry, sizeof entry);
	}

	return (0);
}

/* Windows message from client. */
int
server_msg_fn_windows(struct hdr *hdr, struct client *c)
{
	struct windows_data	 data;
	struct windows_entry	 entry;
	struct session		*s;
	struct window		*w;
	u_int			 i;
	char		 	*cause;

	if (hdr->size != sizeof data)
		fatalx("bad MSG_WINDOWS size");
	buffer_read(c->in, &data, hdr->size);

	if ((s = server_find_sessid(&data.sid, &cause)) == NULL) {
		server_write_error(c, "%s", cause);
		xfree(cause);
		return (0);
	}

	data.windows = 0;
	for (i = 0; i < ARRAY_LENGTH(&s->windows); i++) {
		if (ARRAY_ITEM(&s->windows, i) != NULL)
			data.windows++;
	}
	server_write_client2(c, MSG_WINDOWS,
	    &data, sizeof data, NULL, data.windows * sizeof entry);
	
	for (i = 0; i < ARRAY_LENGTH(&s->windows); i++) {
		w = ARRAY_ITEM(&s->windows, i);
		if (w == NULL)
			continue;
		entry.idx = i;
		strlcpy(entry.name, w->name, sizeof entry.name);
		strlcpy(entry.title, w->screen.title, sizeof entry.title);
		if (ttyname_r(w->fd, entry.tty, sizeof entry.tty) != 0)
			*entry.tty = '\0';
		buffer_write(c->out, &entry, sizeof entry);
	}

	return (0);
}

/* Rename message from client. */
int
server_msg_fn_rename(struct hdr *hdr, struct client *c)
{
	struct rename_data	data;
	char                   *cause;
	struct window	       *w;
	struct session	       *s;
	u_int			i;

	if (hdr->size != sizeof data)
		fatalx("bad MSG_RENAME size");

	buffer_read(c->in, &data, hdr->size);

	data.newname[(sizeof data.newname) - 1] = '\0';
	if ((s = server_find_sessid(&data.sid, &cause)) == NULL) {
		server_write_error(c, "%s", cause);
		xfree(cause);
		return (0);
	}

	if (data.idx == -1)
		w = s->window;
	else {
		if (data.idx < 0)
			fatalx("bad window index");
		w = window_at(&s->windows, data.idx);
		if (w == NULL) { 
			server_write_error(c, "window not found: %d", data.idx);
			return (0);
		}
	}

	strlcpy(w->name, data.newname, sizeof w->name);

	server_write_client(c, MSG_DONE, NULL, 0);
	for (i = 0; i < ARRAY_LENGTH(&clients); i++) {
		c = ARRAY_ITEM(&clients, i);
		if (c != NULL && c->session != NULL) {
			if (session_has(c->session, w))
				server_draw_status(c);
		}
	}

	return (0);
}

/* Last window message from client */
int
server_msg_fn_last(struct hdr *hdr, struct client *c)
{
	if (c->session == NULL)
		return (0);
	if (hdr->size != 0)
		fatalx("bad MSG_LAST size");

	if (session_last(c->session) == 0)
		server_window_changed(c);
	else
		server_write_message(c, "No last window"); 

	return (0);
}

/* Window list message from client */
int
server_msg_fn_windowlist(struct hdr *hdr, struct client *c)
{
	struct window	*w;
	char 		*buf;
	size_t		 len, off;
	u_int 		 i;

	if (c->session == NULL)
		return (0);
	if (hdr->size != 0)
		fatalx("bad MSG_WINDOWLIST size");

	len = c->sx + 1;
	buf = xmalloc(len);
	off = 0;

	*buf = '\0';
	for (i = 0; i < ARRAY_LENGTH(&c->session->windows); i++) {
		w = ARRAY_ITEM(&c->session->windows, i);
		if (w == NULL)
			continue;
		off += xsnprintf(buf + off, len - off, "%u:%s%s ", i, w->name, 
		    w == c->session->window ? "*" : "");
		if (off >= len)
			break;
	}

	server_write_message(c, "%s", buf);
	xfree(buf);

	return (0);
}

/* Window info message from client */
int
server_msg_fn_windowinfo(struct hdr *hdr, struct client *c)
{
	struct window	*w;
	char 		*buf;
	size_t		 len;
	u_int		 i;

	if (c->session == NULL)
		return (0);
	if (hdr->size != 0)
		fatalx("bad MSG_WINDOWINFO size");

	len = c->sx + 1;
	buf = xmalloc(len);

	w = c->session->window;
	window_index(&c->session->windows, w, &i);
	xsnprintf(buf, len, "%u:%s \"%s\" (size %u,%u) (cursor %u,%u) "
	    "(region %u,%u)", i, w->name, w->screen.title, w->screen.sx,
	    w->screen.sy, w->screen.cx, w->screen.cy, w->screen.ry_upper,
	    w->screen.ry_lower);

	server_write_message(c, "%s", buf);
	xfree(buf);

	return (0);
}
