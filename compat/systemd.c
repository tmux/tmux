/* $OpenBSD$ */

/*
 * Copyright (c) 2022 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/un.h>

#include <systemd/sd-daemon.h>

#include "tmux.h"

int
systemd_create_socket(int flags, char **cause)
{
	int			fds;
	int			fd;
	struct sockaddr_un	sa;
	int			addrlen = sizeof sa;

	fds = sd_listen_fds(0);
	if (fds > 1) { /* too many file descriptors */
		errno = E2BIG;
		goto fail;
	}

	if (fds == 1) { /* socket-activated */
		fd = SD_LISTEN_FDS_START;
		if (!sd_is_socket_unix(fd, SOCK_STREAM, 1, NULL, 0)) {
			errno = EPFNOSUPPORT;
			goto fail;
		}
		if (getsockname(fd, (struct sockaddr *)&sa, &addrlen) == -1)
			goto fail;
		socket_path = xstrdup(sa.sun_path);
		return (fd);
	}

	return (server_create_socket(flags, cause));

fail:
	if (cause != NULL)
		xasprintf(cause, "systemd socket error (%s)", strerror(errno));
	return (-1);
}
