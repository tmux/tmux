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

#ifdef __NetBSD__

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))

#define is_runnable(p) \
	((p)->p_stat == LSRUN || (p)->p_stat == SIDL)
#define is_stopped(p) \
	((p)->p_stat == LSSTOP || (p)->p_stat == SZOMB)

char	*osdep_get_name(int, char *);

char *
osdep_get_name(int fd, char *tty)
{
	int		 mib[4] = { CTL_KERN, KERN_PROC2, KERN_PROC_PGRP, 0 };
	struct stat	 sb;
	size_t		 len;
	struct kinfo_proc2 *buf, *newbuf, *p, *bestp;
	u_int		 i;
	char		*name;

	buf = NULL;

	if (stat(tty, &sb) == -1)
		return (NULL);
	if ((mib[3] = tcgetpgrp(fd)) == -1)
		return (NULL);

retry:
	if (sysctl(mib, nitems(mib), NULL, &len, NULL, 0) == -1)
		return (NULL);
	len = (len * 5) / 4;

	if ((newbuf = realloc(buf, len)) == NULL) {
		free(buf);
		return (NULL);
	}
	buf = newbuf;

	if (sysctl(mib, nitems(mib), buf, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			goto retry;
		free(buf);
		return (NULL);
	}

	bestp = NULL;
	for (i = 0; i < len / sizeof (struct kinfo_proc); i++) {
		if (buf[i].p_tdev != sb.st_rdev)
			continue;
		p = &buf[i];
		if (bestp == NULL) {
			bestp = p;
			continue;
		}

		if (is_runnable(p) && !is_runnable(bestp))
			bestp = p;
		else if (!is_runnable(p) && is_runnable(bestp))
			continue;

		if (!is_stopped(p) && is_stopped(bestp))
			bestp = p;
		else if (is_stopped(p) && !is_stopped(bestp))
			continue;

		if (p->p_estcpu > bestp->p_estcpu)
			bestp = p;
		else if (p->p_estcpu < bestp->p_estcpu)
			continue;

		if (p->p_slptime < bestp->p_slptime)
			bestp = p;
		else if (p->p_slptime > bestp->p_slptime)
			continue;

		if (strcmp(p->p_comm, p->p_comm) < 0)
			bestp = p;
		else if (strcmp(p->p_comm, p->p_comm) > 0)
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

#endif
