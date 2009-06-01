/* $OpenBSD$ */

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

#include <errno.h>
#include <unistd.h>

#include "tmux.h"

/* Set up pollfd for buffers. */
void
buffer_set(
    struct pollfd *pfd, int fd, unused struct buffer *in, struct buffer *out)
{
	pfd->fd = fd;
	pfd->events = POLLIN;
	if (BUFFER_USED(out) > 0)
		pfd->events |= POLLOUT;
}

/* Fill buffers from socket based on poll results. */
int
buffer_poll(struct pollfd *pfd, struct buffer *in, struct buffer *out)
{
	ssize_t	n;

#if 0
	log_debug("buffer_poll (%ld): fd=%d, revents=%d; out=%zu in=%zu",
	    (long) getpid(),
	    pfd->fd, pfd->revents, BUFFER_USED(out), BUFFER_USED(in));
#endif

	if (pfd->revents & (POLLERR|POLLNVAL|POLLHUP))
		return (-1);
	if (pfd->revents & POLLIN) {
		buffer_ensure(in, BUFSIZ);
		n = read(pfd->fd, BUFFER_IN(in), BUFFER_FREE(in));
#if 0
		log_debug("buffer_poll: fd=%d, read=%zd", pfd->fd, n);
#endif
		if (n == 0)
			return (-1);
		if (n == -1) {
			if (errno != EINTR && errno != EAGAIN)
				return (-1);
		} else
			buffer_add(in, n);
	}
	if (BUFFER_USED(out) > 0 && pfd->revents & POLLOUT) {
		n = write(pfd->fd, BUFFER_OUT(out), BUFFER_USED(out));
#if 0
		log_debug("buffer_poll: fd=%d, write=%zd", pfd->fd, n);
#endif
		if (n == -1) {
			if (errno != EINTR && errno != EAGAIN)
				return (-1);
		} else
			buffer_remove(out, n);
	}
	return (0);
}

/* Flush buffer output to socket. */
void
buffer_flush(int fd, struct buffer *in, struct buffer *out)
{
	struct pollfd	pfd;

	while (BUFFER_USED(out) > 0) {
		buffer_set(&pfd, fd, in, out);

		if (poll(&pfd, 1, INFTIM) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fatal("poll failed");
		}

		if (buffer_poll(&pfd, in, out) != 0)
			break;
	}
}
