/* $OpenBSD$ */

/*
 * Copyright (c) 2017 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/ioctl.h>
#include <sys/time.h>
#ifdef __OpenBSD__
#include <sys/tty.h>
#endif

#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"

int	pty_open(int *);
pid_t	pty_fork(int, int *, char *, size_t, struct winsize *);

#ifdef __OpenBSD__
int
pty_open(int *fd)
{
	*fd = open(PATH_PTMDEV, O_RDWR|O_CLOEXEC);
	if (*fd < 0)
	    return (-1);
	return (0);
}
#else
int
pty_open(__unused int *fd)
{
	*fd = -1;
	return (0);
}
#endif

#ifdef __OpenBSD__
pid_t
pty_fork(int ptmfd, int *fd, char *name, size_t namelen, struct winsize *ws)
{
	struct ptmget	ptm;
	pid_t		pid;

	if (ioctl(ptmfd, PTMGET, &ptm) == -1)
		return (-1);

	strlcpy(name, ptm.sn, namelen);
	ioctl(ptm.sfd, TIOCSWINSZ, ws);

	switch (pid = fork()) {
	case -1:
		close(ptm.cfd);
		close(ptm.sfd);
		return (-1);
	case 0:
		close(ptm.cfd);
		login_tty(ptm.sfd);
		return (0);
	}
	*fd = ptm.cfd;
	close(ptm.sfd);
	return (pid);
}
#else
pid_t
pty_fork(__unused int ptmfd, int *fd, char *name, __unused size_t namelen,
    struct winsize *ws)
{
	return (forkpty(fd, name, NULL, ws));
}
#endif
