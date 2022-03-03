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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tmux.h"

#define is_runnable(p) \
	((p)->p_stat == LSRUN || (p)->p_stat == SIDL)
#define is_stopped(p) \
	((p)->p_stat == SSTOP || (p)->p_stat == SZOMB)

struct kinfo_proc2	*cmp_procs(struct kinfo_proc2 *, struct kinfo_proc2 *);

struct kinfo_proc2 *
cmp_procs(struct kinfo_proc2 *p1, struct kinfo_proc2 *p2)
{
	if (is_runnable(p1) && !is_runnable(p2))
		return (p1);
	if (!is_runnable(p1) && is_runnable(p2))
		return (p2);

	if (is_stopped(p1) && !is_stopped(p2))
		return (p1);
	if (!is_stopped(p1) && is_stopped(p2))
		return (p2);

	if (p1->p_estcpu > p2->p_estcpu)
		return (p1);
	if (p1->p_estcpu < p2->p_estcpu)
		return (p2);

	if (p1->p_slptime < p2->p_slptime)
		return (p1);
	if (p1->p_slptime > p2->p_slptime)
		return (p2);

	if (p1->p_pid > p2->p_pid)
		return (p1);
	return (p2);
}

char *
osdep_get_name(int fd, __unused char *tty)
{
	int		 mib[6];
	struct stat	 sb;
	size_t		 len, i;
	struct kinfo_proc2 *buf, *newbuf, *bestp;
	char		*name;

	if (stat(tty, &sb) == -1)
		return (NULL);
	if ((mib[3] = tcgetpgrp(fd)) == -1)
		return (NULL);

	buf = NULL;
	len = sizeof bestp;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC2;
	mib[2] = KERN_PROC_PGRP;
	mib[4] = sizeof *buf;

retry:
	mib[5] = 0;

	if (sysctl(mib, __arraycount(mib), NULL, &len, NULL, 0) == -1)
		return (NULL);

	if ((newbuf = realloc(buf, len)) == NULL)
		goto error;
	buf = newbuf;

	mib[5] = len / (sizeof *buf);
	if (sysctl(mib, __arraycount(mib), buf, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			goto retry;
		goto error;
	}

	bestp = NULL;
	for (i = 0; i < len / (sizeof *buf); i++) {
		if (buf[i].p_tdev != sb.st_rdev)
			continue;
		if (bestp == NULL)
			bestp = &buf[i];
		else
			bestp = cmp_procs(&buf[i], bestp);
	}

	name = NULL;
	if (bestp != NULL)
		name = strdup(bestp->p_comm);

	free(buf);
	return (name);

error:
	free(buf);
	return (NULL);
}

char *
osdep_get_cwd(int fd)
{
	static char	target[PATH_MAX + 1];
	pid_t		pgrp;
#ifdef KERN_PROC_CWD
	int		mib[4];
	size_t		len;
#else
	char		*path;
	ssize_t		n;
#endif

	if ((pgrp = tcgetpgrp(fd)) == -1)
		return (NULL);

#ifdef KERN_PROC_CWD
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC_ARGS;
	mib[2] = pgrp;
	mib[3] = KERN_PROC_CWD;
	len = sizeof(target);
	if (sysctl(mib, __arraycount(mib), target, &len, NULL, 0) == 0)
		return (target);
#else
	xasprintf(&path, "/proc/%lld/cwd", (long long) pgrp);
	n = readlink(path, target, sizeof(target) - 1);
	free(path);
	if (n > 0) {
		target[n] = '\0';
		return (target);
	}
#endif

	return (NULL);
}

struct event_base *
osdep_event_init(void)
{
	return (event_init());
}
