/* $OpenBSD$ */

/*
 * Copyright (c) 2009 Joshua Elsasser <josh@elsasser.org>
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

#include <event.h>
#include <libproc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char			*osdep_get_name(int, char *);
char			*osdep_get_cwd(int);
struct event_base	*osdep_event_init(void);

#define unused __attribute__ ((unused))

char *
osdep_get_name(int fd, unused char *tty)
{
	struct proc_bsdinfo		bsdinfo;
	pid_t				pgrp;
	int				ret;

	if ((pgrp = tcgetpgrp(fd)) == -1)
		return (NULL);

	ret = proc_pidinfo(pgrp, PROC_PIDTBSDINFO, 0,
	    &bsdinfo, sizeof bsdinfo);
	if (ret == sizeof bsdinfo && *bsdinfo.pbi_comm != '\0')
		return (strdup(bsdinfo.pbi_comm));
	return (NULL);
}

char *
osdep_get_cwd(int fd)
{
	static char			wd[PATH_MAX];
	struct proc_vnodepathinfo	pathinfo;
	pid_t				pgrp;
	int				ret;

	if ((pgrp = tcgetpgrp(fd)) == -1)
		return (NULL);

	ret = proc_pidinfo(pgrp, PROC_PIDVNODEPATHINFO, 0,
	    &pathinfo, sizeof pathinfo);
	if (ret == sizeof pathinfo) {
		strlcpy(wd, pathinfo.pvi_cdir.vip_path, sizeof wd);
		return (wd);
	}
	return (NULL);
}

struct event_base *
osdep_event_init(void)
{
	/*
	 * On OS X, kqueue and poll are both completely broken and don't
	 * work on anything except socket file descriptors (yes, really).
	 */
	setenv("EVENT_NOKQUEUE", "1", 1);
	setenv("EVENT_NOPOLL", "1", 1);
	return (event_init());
}
