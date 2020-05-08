/* $OpenBSD$ */

/*
 * Copyright (c) 2019 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/queue.h>
#include <sys/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

static int	file_next_stream = 3;

RB_GENERATE(client_files, client_file, entry, file_cmp);

static char *
file_get_path(struct client *c, const char *file)
{
	char	*path;

	if (*file == '/')
		path = xstrdup(file);
	else
		xasprintf(&path, "%s/%s", server_client_get_cwd(c, NULL), file);
	return (path);
}

int
file_cmp(struct client_file *cf1, struct client_file *cf2)
{
	if (cf1->stream < cf2->stream)
		return (-1);
	if (cf1->stream > cf2->stream)
		return (1);
	return (0);
}

struct client_file *
file_create(struct client *c, int stream, client_file_cb cb, void *cbdata)
{
	struct client_file	*cf;

	cf = xcalloc(1, sizeof *cf);
	cf->c = c;
	cf->references = 1;
	cf->stream = stream;

	cf->buffer = evbuffer_new();
	if (cf->buffer == NULL)
		fatalx("out of memory");

	cf->cb = cb;
	cf->data = cbdata;

	if (cf->c != NULL) {
		RB_INSERT(client_files, &cf->c->files, cf);
		cf->c->references++;
	}

	return (cf);
}

void
file_free(struct client_file *cf)
{
	if (--cf->references != 0)
		return;

	evbuffer_free(cf->buffer);
	free(cf->path);

	if (cf->c != NULL) {
		RB_REMOVE(client_files, &cf->c->files, cf);
		server_client_unref(cf->c);
	}
	free(cf);
}

static void
file_fire_done_cb(__unused int fd, __unused short events, void *arg)
{
	struct client_file	*cf = arg;
	struct client		*c = cf->c;

	if (cf->cb != NULL && (c == NULL || (~c->flags & CLIENT_DEAD)))
		cf->cb(c, cf->path, cf->error, 1, cf->buffer, cf->data);
	file_free(cf);
}

void
file_fire_done(struct client_file *cf)
{
	event_once(-1, EV_TIMEOUT, file_fire_done_cb, cf, NULL);
}

void
file_fire_read(struct client_file *cf)
{
	struct client	*c = cf->c;

	if (cf->cb != NULL)
		cf->cb(c, cf->path, cf->error, 0, cf->buffer, cf->data);
}

int
file_can_print(struct client *c)
{
	if (c == NULL)
		return (0);
	if (c->session != NULL && (~c->flags & CLIENT_CONTROL))
		return (0);
	return (1);
}

void
file_print(struct client *c, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	file_vprint(c, fmt, ap);
	va_end(ap);
}

void
file_vprint(struct client *c, const char *fmt, va_list ap)
{
	struct client_file	 find, *cf;
	struct msg_write_open	 msg;

	if (!file_can_print(c))
		return;

	find.stream = 1;
	if ((cf = RB_FIND(client_files, &c->files, &find)) == NULL) {
		cf = file_create(c, 1, NULL, NULL);
		cf->path = xstrdup("-");

		evbuffer_add_vprintf(cf->buffer, fmt, ap);

		msg.stream = 1;
		msg.fd = STDOUT_FILENO;
		msg.flags = 0;
		proc_send(c->peer, MSG_WRITE_OPEN, -1, &msg, sizeof msg);
	} else {
		evbuffer_add_vprintf(cf->buffer, fmt, ap);
		file_push(cf);
	}
}

void
file_print_buffer(struct client *c, void *data, size_t size)
{
	struct client_file	 find, *cf;
	struct msg_write_open	 msg;

	if (!file_can_print(c))
		return;

	find.stream = 1;
	if ((cf = RB_FIND(client_files, &c->files, &find)) == NULL) {
		cf = file_create(c, 1, NULL, NULL);
		cf->path = xstrdup("-");

		evbuffer_add(cf->buffer, data, size);

		msg.stream = 1;
		msg.fd = STDOUT_FILENO;
		msg.flags = 0;
		proc_send(c->peer, MSG_WRITE_OPEN, -1, &msg, sizeof msg);
	} else {
		evbuffer_add(cf->buffer, data, size);
		file_push(cf);
	}
}

void
file_error(struct client *c, const char *fmt, ...)
{
	struct client_file	 find, *cf;
	struct msg_write_open	 msg;
	va_list			 ap;

	if (!file_can_print(c))
		return;

	va_start(ap, fmt);

	find.stream = 2;
	if ((cf = RB_FIND(client_files, &c->files, &find)) == NULL) {
		cf = file_create(c, 2, NULL, NULL);
		cf->path = xstrdup("-");

		evbuffer_add_vprintf(cf->buffer, fmt, ap);

		msg.stream = 2;
		msg.fd = STDERR_FILENO;
		msg.flags = 0;
		proc_send(c->peer, MSG_WRITE_OPEN, -1, &msg, sizeof msg);
	} else {
		evbuffer_add_vprintf(cf->buffer, fmt, ap);
		file_push(cf);
	}

	va_end(ap);
}

void
file_write(struct client *c, const char *path, int flags, const void *bdata,
    size_t bsize, client_file_cb cb, void *cbdata)
{
	struct client_file	*cf;
	FILE			*f;
	struct msg_write_open	*msg;
	size_t			 msglen;
	int			 fd = -1;
	const char		*mode;

	if (strcmp(path, "-") == 0) {
		cf = file_create(c, file_next_stream++, cb, cbdata);
		cf->path = xstrdup("-");

		fd = STDOUT_FILENO;
		if (c == NULL || c->flags & CLIENT_ATTACHED) {
			cf->error = EBADF;
			goto done;
		}
		goto skip;
	}

	cf = file_create(c, file_next_stream++, cb, cbdata);
	cf->path = file_get_path(c, path);

	if (c == NULL || c->flags & CLIENT_ATTACHED) {
		if (flags & O_APPEND)
			mode = "ab";
		else
			mode = "wb";
		f = fopen(cf->path, mode);
		if (f == NULL) {
			cf->error = errno;
			goto done;
		}
		if (fwrite(bdata, 1, bsize, f) != bsize) {
			fclose(f);
			cf->error = EIO;
			goto done;
		}
		fclose(f);
		goto done;
	}

skip:
	evbuffer_add(cf->buffer, bdata, bsize);

	msglen = strlen(cf->path) + 1 + sizeof *msg;
	if (msglen > MAX_IMSGSIZE - IMSG_HEADER_SIZE) {
		cf->error = E2BIG;
		goto done;
	}
	msg = xmalloc(msglen);
	msg->stream = cf->stream;
	msg->fd = fd;
	msg->flags = flags;
	memcpy(msg + 1, cf->path, msglen - sizeof *msg);
	if (proc_send(c->peer, MSG_WRITE_OPEN, -1, msg, msglen) != 0) {
		free(msg);
		cf->error = EINVAL;
		goto done;
	}
	free(msg);
	return;

done:
	file_fire_done(cf);
}

void
file_read(struct client *c, const char *path, client_file_cb cb, void *cbdata)
{
	struct client_file	*cf;
	FILE			*f;
	struct msg_read_open	*msg;
	size_t			 msglen, size;
	int			 fd = -1;
	char			 buffer[BUFSIZ];

	if (strcmp(path, "-") == 0) {
		cf = file_create(c, file_next_stream++, cb, cbdata);
		cf->path = xstrdup("-");

		fd = STDIN_FILENO;
		if (c == NULL || c->flags & CLIENT_ATTACHED) {
			cf->error = EBADF;
			goto done;
		}
		goto skip;
	}

	cf = file_create(c, file_next_stream++, cb, cbdata);
	cf->path = file_get_path(c, path);

	if (c == NULL || c->flags & CLIENT_ATTACHED) {
		f = fopen(cf->path, "rb");
		if (f == NULL) {
			cf->error = errno;
			goto done;
		}
		for (;;) {
			size = fread(buffer, 1, sizeof buffer, f);
			if (evbuffer_add(cf->buffer, buffer, size) != 0) {
				cf->error = ENOMEM;
				goto done;
			}
			if (size != sizeof buffer)
				break;
		}
		if (ferror(f)) {
			cf->error = EIO;
			goto done;
		}
		fclose(f);
		goto done;
	}

skip:
	msglen = strlen(cf->path) + 1 + sizeof *msg;
	if (msglen > MAX_IMSGSIZE - IMSG_HEADER_SIZE) {
		cf->error = E2BIG;
		goto done;
	}
	msg = xmalloc(msglen);
	msg->stream = cf->stream;
	msg->fd = fd;
	memcpy(msg + 1, cf->path, msglen - sizeof *msg);
	if (proc_send(c->peer, MSG_READ_OPEN, -1, msg, msglen) != 0) {
		free(msg);
		cf->error = EINVAL;
		goto done;
	}
	free(msg);
	return;

done:
	file_fire_done(cf);
}

static void
file_push_cb(__unused int fd, __unused short events, void *arg)
{
	struct client_file	*cf = arg;
	struct client		*c = cf->c;

	if (~c->flags & CLIENT_DEAD)
		file_push(cf);
	file_free(cf);
}

void
file_push(struct client_file *cf)
{
	struct client		*c = cf->c;
	struct msg_write_data	*msg;
	size_t			 msglen, sent, left;
	struct msg_write_close	 close;

	msg = xmalloc(sizeof *msg);
	left = EVBUFFER_LENGTH(cf->buffer);
	while (left != 0) {
		sent = left;
		if (sent > MAX_IMSGSIZE - IMSG_HEADER_SIZE - sizeof *msg)
			sent = MAX_IMSGSIZE - IMSG_HEADER_SIZE - sizeof *msg;

		msglen = (sizeof *msg) + sent;
		msg = xrealloc(msg, msglen);
		msg->stream = cf->stream;
		memcpy(msg + 1, EVBUFFER_DATA(cf->buffer), sent);
		if (proc_send(c->peer, MSG_WRITE, -1, msg, msglen) != 0)
			break;
		evbuffer_drain(cf->buffer, sent);

		left = EVBUFFER_LENGTH(cf->buffer);
		log_debug("%s: file %d sent %zu, left %zu", c->name, cf->stream,
		    sent, left);
	}
	if (left != 0) {
		cf->references++;
		event_once(-1, EV_TIMEOUT, file_push_cb, cf, NULL);
	} else if (cf->stream > 2) {
		close.stream = cf->stream;
		proc_send(c->peer, MSG_WRITE_CLOSE, -1, &close, sizeof close);
		file_fire_done(cf);
	}
	free(msg);
}
