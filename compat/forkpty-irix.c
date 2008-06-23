/* $Id: forkpty-irix.c,v 1.1 2008-06-23 21:54:48 nicm Exp $ */

/*
 * Copyright (c) 2008 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <sys/stat.h>

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

pid_t
forkpty(int *master,
    unused char *name, unused struct termios *tio, struct winsize *ws)
{
	int	slave, fd;
	char   *path;
	pid_t	pid;
	void   *old;

	path = _getpty(master, O_RDWR, 0622, 0);
	if (path == NULL)
		goto out;

	if ((slave = open(path, O_RDWR|O_NOCTTY)) == -1)
		goto out;

	switch (pid = fork()) {
	case -1:
		goto out;
	case 0:
		close(*master);

		setsid();
		
		old = signal(SIGHUP, SIG_IGN);
		vhangup();
		signal(SIGHUP, old);

		if ((fd = open(path, O_RDWR)) == -1)
			fatal("open failed");
		close(slave);
		slave = fd;

		if (ioctl(slave, TIOCSWINSZ, ws) == -1)
			fatal("ioctl failed");

		dup2(slave, 0);
		dup2(slave, 1);
		dup2(slave, 2);
		if (slave > 2)
			close(slave);
		return (0);
	}

	close(slave);
	return (pid);

out:
	if (*master != -1)
		close(*master);
	if (slave != -1)
		close(slave);
	return (-1);
}
