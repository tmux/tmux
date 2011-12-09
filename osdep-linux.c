/* $Id$ */

/*
 * Copyright (c) 2009 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tmux.h"

char *
osdep_get_name(int fd, unused char *tty)
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
		xfree(path);
		return (NULL);
	}
	xfree(path);

	len = 0;
	buf = NULL;
	while ((ch = fgetc(f)) != EOF) {
		if (ch == '\0')
			break;
		buf = xrealloc(buf, 1, len + 2);
		buf[len++] = ch;
	}
	if (buf != NULL)
		buf[len] = '\0';

	fclose(f);
	return (buf);
}

char *
osdep_get_cwd(pid_t pid)
{
	static char	 target[MAXPATHLEN + 1];
	char		*path;
	ssize_t		 n;

	xasprintf(&path, "/proc/%d/cwd", pid);
	n = readlink(path, target, MAXPATHLEN);
	xfree(path);
	if (n > 0) {
		target[n] = '\0';
		return (target);
	}
	return (NULL);
}

struct event_base *
osdep_event_init(void)
{
	/*
	 * On Linux, epoll doesn't work on /dev/null (yes, really).
	 *
	 * This has been commented because libevent versions up until the very
	 * latest (1.4 git or 2.0.10) do not handle signals properly when using
	 * poll or select, causing hangs.
	 * 
	 */
	/* setenv("EVENT_NOEPOLL", "1", 1); */
	return (event_init());
}
