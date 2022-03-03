/* $OpenBSD$ */

/*
 * Copyright (c) 2011 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <sys/param.h>
#include <sys/procfs.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "tmux.h"

char *
osdep_get_name(__unused int fd, char *tty)
{
	struct psinfo	 p;
	char		*path;
	ssize_t		 bytes;
	int		 f, ttyfd, retval;
	pid_t		 pgrp;

	if ((ttyfd = open(tty, O_RDONLY|O_NOCTTY)) == -1)
		return (NULL);

	retval = ioctl(ttyfd, TIOCGPGRP, &pgrp);
	close(ttyfd);
	if (retval == -1)
		return (NULL);

	xasprintf(&path, "/proc/%u/psinfo", (u_int) pgrp);
	f = open(path, O_RDONLY);
	free(path);
	if (f < 0)
		return (NULL);

	bytes = read(f, &p, sizeof(p));
	close(f);
	if (bytes != sizeof(p))
		return (NULL);

	return (xstrdup(p.pr_fname));
}

char *
osdep_get_cwd(int fd)
{
	static char      target[MAXPATHLEN + 1];
	char            *path;
	const char      *ttypath;
	ssize_t          n;
	pid_t            pgrp;
	int              len, retval, ttyfd;

	if ((ttypath = ptsname(fd)) == NULL)
		return (NULL);
	if ((ttyfd = open(ttypath, O_RDONLY|O_NOCTTY)) == -1)
		return (NULL);

	retval = ioctl(ttyfd, TIOCGPGRP, &pgrp);
	close(ttyfd);
	if (retval == -1)
		return (NULL);

	xasprintf(&path, "/proc/%u/cwd", (u_int) pgrp);
	n = readlink(path, target, MAXPATHLEN);
	free(path);
	if (n > 0) {
		target[n] = '\0';
		if ((len = strlen(target)) > 1 && target[len - 1] == '/')
			target[len - 1] = '\0';
		return (target);
	}
	return (NULL);
}

struct event_base *
osdep_event_init(void)
{
	return (event_init());
}
