/* $Id: forkpty-sunos.c,v 1.1 2008-06-18 19:52:29 nicm Exp $ */

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

#include "tmux.h"

pid_t
forkpty(int *master, int *slave,
    char *name, struct termios *tio, struct winsize *ws)
{
	char   *path;

	if ((*master = open("/dev/ptmx", O_RDWR)) == -1)
		return (-1);
	if (grantpt(*master) != 0)
		goto out;
	if (unlockpt(*master) != 0)
		goto out;
	
	if ((path = ptsname(*master)) == NULL)
		goto out;
	if ((*slave = open(path, O_RDWR)) == -1)
		goto out;
	
	if (ioctl(*slave, I_PUSH, "ptem") == -1)
		goto out;
	if (ioctl(*slave, I_PUSH, "ldterm") == -1)
		goto out;

	switch (pid = fork()) {
	case -1:
		goto out;
	case 0:
		close(*master);
		return (0);
	}

	close(*slave);
	return (pid);

out:
	if (*master != -1)
		close(*master);
	if (*slave != -1)
		close(*slave);
	return (-1);
}
