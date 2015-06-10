/*
 * Copyright (c) 2013 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>

#include "tmux.h"

int
openat(int fd, const char *path, int flags, ...)
{
	mode_t	mode;
	va_list ap;
	int	dotfd, retval, saved_errno;

	if (flags & O_CREAT) {
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	} else
		mode = 0;

	dotfd = -1;
	if (fd != AT_FDCWD) {
		dotfd = open(".", O_RDONLY);
		if (dotfd == -1)
			return (-1);
		if (fchdir(fd) != 0) {
			saved_errno = errno;
			close(dotfd);
			errno = saved_errno;
			return (-1);
		}
	}

	retval = open(path, flags, mode);

	if (dotfd != -1) {
		if (fchdir(dotfd) != 0) {
			saved_errno = errno;
			close(retval);
			close(dotfd);
			errno = saved_errno;
			return (-1);
		}
		close(dotfd);
	}

	return (retval);
}
