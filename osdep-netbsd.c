/* $Id: osdep-netbsd.c,v 1.6 2009-07-28 22:28:11 tcunha Exp $ */

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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define is_runnable(p) \
        ((p)->p_stat == LSRUN || (p)->p_stat == SIDL)
#define is_stopped(p) \
        ((p)->p_stat == SSTOP || (p)->p_stat == SZOMB)

char	*osdep_get_name(int, char *);

char *
osdep_get_name(int fd, __unused char *tty)
{
	int		 mib[6];
	struct stat	 sb;
	size_t		 len, i;
	struct kinfo_proc2 *buf, *newbuf, *p, *bestp;
	char		*name;

	if (stat(tty, &sb) == -1)
		return (NULL);
	if ((mib[3] = tcgetpgrp(fd)) == -1)
		return (NULL);

	buf = NULL;
	len = sizeof(p);
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC2;
	mib[2] = KERN_PROC_PGRP;
	mib[4] = sizeof (*buf);
	mib[5] = 0;

retry:
	if (sysctl(mib, __arraycount(mib), NULL, &len, NULL, 0) == -1)
		return (NULL);

	if ((newbuf = realloc(buf, len * sizeof (*buf))) == NULL) {
		free(buf);
		return (NULL);
	}
	buf = newbuf;

	mib[5] = len / sizeof(*buf);
	if (sysctl(mib, __arraycount(mib), buf, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			goto retry; /* possible infinite loop? */
		free(buf);
		return (NULL);
	}

	bestp = NULL;
	for (i = 0; i < len / sizeof (*buf); i++) {
		if (buf[i].p_tdev != sb.st_rdev)
			continue;
		p = &buf[i];
		if (bestp == NULL) {
			bestp = p;
			continue;
		}

		if (is_runnable(p) && !is_runnable(bestp)) {
			bestp = p;
			continue;
		} else if (!is_runnable(p) && is_runnable(bestp))
			continue;

		if (!is_stopped(p) && is_stopped(bestp)) {
			bestp = p;
			continue;
		} else if (is_stopped(p) && !is_stopped(bestp))
			continue;

		if (p->p_estcpu > bestp->p_estcpu) {
			bestp = p;
			continue;
		} else if (p->p_estcpu < bestp->p_estcpu)
			continue;

		if (p->p_slptime < bestp->p_slptime) {
			bestp = p;
			continue;
		} else if (p->p_slptime > bestp->p_slptime)
			continue;

		if (p->p_pid > bestp->p_pid)
			bestp = p;
	}

	name = NULL;
	if (bestp != NULL)
		name = strdup(bestp->p_comm);
	
	free(buf);
	return (name);
}
