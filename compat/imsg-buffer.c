/*	$OpenBSD: imsg-buffer.c,v 1.1 2009/08/11 17:18:35 nicm Exp $	*/

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "imsg.h"

int	buf_realloc(struct buf *, size_t);
void	buf_enqueue(struct msgbuf *, struct buf *);
void	buf_dequeue(struct msgbuf *, struct buf *);

struct buf *
buf_open(size_t len)
{
	struct buf	*buf;

	if ((buf = calloc(1, sizeof(struct buf))) == NULL)
		return (NULL);
	if ((buf->buf = malloc(len)) == NULL) {
		free(buf);
		return (NULL);
	}
	buf->size = buf->max = len;
	buf->fd = -1;

	return (buf);
}

struct buf *
buf_dynamic(size_t len, size_t max)
{
	struct buf	*buf;

	if (max < len)
		return (NULL);

	if ((buf = buf_open(len)) == NULL)
		return (NULL);

	if (max > 0)
		buf->max = max;

	return (buf);
}

int
buf_realloc(struct buf *buf, size_t len)
{
	u_char	*b;

	/* on static buffers max is eq size and so the following fails */
	if (buf->wpos + len > buf->max) {
		errno = ENOMEM;
		return (-1);
	}

	b = realloc(buf->buf, buf->wpos + len);
	if (b == NULL)
		return (-1);
	buf->buf = b;
	buf->size = buf->wpos + len;

	return (0);
}

int
buf_add(struct buf *buf, const void *data, size_t len)
{
	if (buf->wpos + len > buf->size)
		if (buf_realloc(buf, len) == -1)
			return (-1);

	memcpy(buf->buf + buf->wpos, data, len);
	buf->wpos += len;
	return (0);
}

void *
buf_reserve(struct buf *buf, size_t len)
{
	void	*b;

	if (buf->wpos + len > buf->size)
		if (buf_realloc(buf, len) == -1)
			return (NULL);

	b = buf->buf + buf->wpos;
	buf->wpos += len;
	return (b);
}

void *
buf_seek(struct buf *buf, size_t pos, size_t len)
{
	/* only allowed to seek in already written parts */
	if (pos + len > buf->wpos)
		return (NULL);

	return (buf->buf + pos);
}

size_t
buf_size(struct buf *buf)
{
	return (buf->wpos);
}

size_t
buf_left(struct buf *buf)
{
	return (buf->max - buf->wpos);
}

void
buf_close(struct msgbuf *msgbuf, struct buf *buf)
{
	buf_enqueue(msgbuf, buf);
}

int
buf_write(struct msgbuf *msgbuf)
{
	struct iovec	 iov[IOV_MAX];
	struct buf	*buf, *next;
	unsigned int	 i = 0;
	ssize_t	n;

	bzero(&iov, sizeof(iov));
	TAILQ_FOREACH(buf, &msgbuf->bufs, entry) {
		if (i >= IOV_MAX)
			break;
		iov[i].iov_base = buf->buf + buf->rpos;
		iov[i].iov_len = buf->wpos - buf->rpos;
		i++;
	}

	if ((n = writev(msgbuf->fd, iov, i)) == -1) {
		if (errno == EAGAIN || errno == ENOBUFS ||
		    errno == EINTR)	/* try later */
			return (0);
		else
			return (-1);
	}

	if (n == 0) {			/* connection closed */
		errno = 0;
		return (-2);
	}

	for (buf = TAILQ_FIRST(&msgbuf->bufs); buf != NULL && n > 0;
	    buf = next) {
		next = TAILQ_NEXT(buf, entry);
		if (buf->rpos + n >= buf->wpos) {
			n -= buf->wpos - buf->rpos;
			buf_dequeue(msgbuf, buf);
		} else {
			buf->rpos += n;
			n = 0;
		}
	}

	return (0);
}

void
buf_free(struct buf *buf)
{
	free(buf->buf);
	free(buf);
}

void
msgbuf_init(struct msgbuf *msgbuf)
{
	msgbuf->queued = 0;
	msgbuf->fd = -1;
	TAILQ_INIT(&msgbuf->bufs);
}

void
msgbuf_clear(struct msgbuf *msgbuf)
{
	struct buf	*buf;

	while ((buf = TAILQ_FIRST(&msgbuf->bufs)) != NULL)
		buf_dequeue(msgbuf, buf);
}

int
msgbuf_write(struct msgbuf *msgbuf)
{
	struct iovec	 iov[IOV_MAX];
	struct buf	*buf, *next;
	unsigned int	 i = 0;
	ssize_t		 n;
	struct msghdr	 msg;
	struct cmsghdr	*cmsg;
	union {
		struct cmsghdr	hdr;
		char		buf[CMSG_SPACE(sizeof(int))];
	} cmsgbuf;

	bzero(&iov, sizeof(iov));
	bzero(&msg, sizeof(msg));
	TAILQ_FOREACH(buf, &msgbuf->bufs, entry) {
		if (i >= IOV_MAX)
			break;
		iov[i].iov_base = buf->buf + buf->rpos;
		iov[i].iov_len = buf->wpos - buf->rpos;
		i++;
		if (buf->fd != -1)
			break;
	}

	msg.msg_iov = iov;
	msg.msg_iovlen = i;

	if (buf != NULL && buf->fd != -1) {
		msg.msg_control = (caddr_t)&cmsgbuf.buf;
		msg.msg_controllen = sizeof(cmsgbuf.buf);
		cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		*(int *)CMSG_DATA(cmsg) = buf->fd;
	}

	if ((n = sendmsg(msgbuf->fd, &msg, 0)) == -1) {
		if (errno == EAGAIN || errno == ENOBUFS ||
		    errno == EINTR)	/* try later */
			return (0);
		else
			return (-1);
	}

	if (n == 0) {			/* connection closed */
		errno = 0;
		return (-2);
	}

	/*
	 * assumption: fd got sent if sendmsg sent anything
	 * this works because fds are passed one at a time
	 */
	if (buf != NULL && buf->fd != -1) {
		close(buf->fd);
		buf->fd = -1;
	}

	for (buf = TAILQ_FIRST(&msgbuf->bufs); buf != NULL && n > 0;
	    buf = next) {
		next = TAILQ_NEXT(buf, entry);
		if (buf->rpos + n >= buf->wpos) {
			n -= buf->wpos - buf->rpos;
			buf_dequeue(msgbuf, buf);
		} else {
			buf->rpos += n;
			n = 0;
		}
	}

	return (0);
}

void
buf_enqueue(struct msgbuf *msgbuf, struct buf *buf)
{
	TAILQ_INSERT_TAIL(&msgbuf->bufs, buf, entry);
	msgbuf->queued++;
}

void
buf_dequeue(struct msgbuf *msgbuf, struct buf *buf)
{
	TAILQ_REMOVE(&msgbuf->bufs, buf, entry);

	if (buf->fd != -1)
		close(buf->fd);

	msgbuf->queued--;
	buf_free(buf);
}
