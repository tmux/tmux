/* $Id$ */

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
#include <sys/sysctl.h>

#include <event.h>
#include <libproc.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char			*osdep_get_name(int, char *);
char			*osdep_get_cwd(pid_t);
struct event_base	*osdep_event_init(void);

#define unused __attribute__ ((unused))

char *
osdep_get_name(int fd, unused char *tty)
{
	int	mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, 0 };
	size_t	size;
	struct kinfo_proc kp;

	if ((mib[3] = tcgetpgrp(fd)) == -1)
		return (NULL);

	size = sizeof kp;
	if (sysctl(mib, 4, &kp, &size, NULL, 0) == -1)
		return (NULL);
	if (*kp.kp_proc.p_comm == '\0')
		return (NULL);

	return (strdup(kp.kp_proc.p_comm));
}

char *
osdep_get_cwd(pid_t pid)
{
	static char 			wd[PATH_MAX];
	struct proc_vnodepathinfo	pathinfo;
	int				ret;

	ret = proc_pidinfo(
	    pid, PROC_PIDVNODEPATHINFO, 0, &pathinfo, sizeof pathinfo);
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
