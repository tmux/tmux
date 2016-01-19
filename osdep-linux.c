/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <sys/stat.h>
#include <sys/param.h>

#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

char *
osdep_get_name(int fd, __unused char *tty)
{
	FILE	*f;
	char	*path, *buf;
	size_t	 len;
	int	 ch;
	pid_t	 pgrp;

	if ((pgrp = tcgetpgrp(fd)) == -1)
		return (NULL);

	xasprintf(&path, "/proc/%lld/cmdline", (long long) pgrp);
	if ((f = fopen(path, "r")) == NULL) {
		free(path);
		return (NULL);
	}
	free(path);

	len = 0;
	buf = NULL;
	while ((ch = fgetc(f)) != EOF) {
		if (ch == '\0')
			break;
		buf = xrealloc(buf, len + 2);
		buf[len++] = ch;
	}
	if (buf != NULL)
		buf[len] = '\0';

	fclose(f);
	return (buf);
}

char *
osdep_get_cwd(int fd)
{
	static char	 target[MAXPATHLEN + 1];
	char		*path;
	pid_t		 pgrp, sid;
	ssize_t		 n;

	if ((pgrp = tcgetpgrp(fd)) == -1)
		return (NULL);

	xasprintf(&path, "/proc/%lld/cwd", (long long) pgrp);
	n = readlink(path, target, MAXPATHLEN);
	free(path);

	if (n == -1 && ioctl(fd, TIOCGSID, &sid) != -1) {
		xasprintf(&path, "/proc/%lld/cwd", (long long) sid);
		n = readlink(path, target, MAXPATHLEN);
		free(path);
	}

	if (n > 0) {
		target[n] = '\0';
		return (target);
	}
	return (NULL);
}

struct event_base *
osdep_event_init(void)
{
	/* On Linux, epoll doesn't work on /dev/null (yes, really). */
	setenv("EVENT_NOEPOLL", "1", 1);
	return (event_init());
}
