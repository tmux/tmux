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
#include <event.h>
#include <unistd.h>

#include "tmux.h"

/* Fill buffers from socket based on poll results. */
int
buffer_poll(int fd, int events, struct buffer *in, struct buffer *out)
{
	ssize_t	n;

	if (in != NULL && events & EV_READ) {
		buffer_ensure(in, BUFSIZ);
		n = read(fd, BUFFER_IN(in), BUFFER_FREE(in));
		if (n == 0)
			return (-1);
		if (n == -1) {
			if (errno != EINTR && errno != EAGAIN)
				return (-1);
		} else
			buffer_add(in, n);
	}
	if (out != NULL && BUFFER_USED(out) > 0 && events & EV_WRITE) {
		n = write(fd, BUFFER_OUT(out), BUFFER_USED(out));
		if (n == -1) {
			if (errno != EINTR && errno != EAGAIN)
				return (-1);
		} else
			buffer_remove(out, n);
	}
	return (0);
}
