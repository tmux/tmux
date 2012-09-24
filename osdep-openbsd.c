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

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/stat.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define is_runnable(p) \
	((p)->p_stat == SRUN || (p)->p_stat == SIDL || (p)->p_stat == SONPROC)
#define is_stopped(p) \
	((p)->p_stat == SSTOP || (p)->p_stat == SZOMB || (p)->p_stat == SDEAD)

struct kinfo_proc	*cmp_procs(struct kinfo_proc *, struct kinfo_proc *);
char			*osdep_get_name(int, char *);
char			*osdep_get_cwd(pid_t);
struct event_base	*osdep_event_init(void);

struct kinfo_proc *
cmp_procs(struct kinfo_proc *p1, struct kinfo_proc *p2)
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

	if ((p1->p_flag & P_SINTR) && !(p2->p_flag & P_SINTR))
		return (p1);
	if (!(p1->p_flag & P_SINTR) && (p2->p_flag & P_SINTR))
		return (p2);

	if (strcmp(p1->p_comm, p2->p_comm) < 0)
		return (p1);
	if (strcmp(p1->p_comm, p2->p_comm) > 0)
		return (p2);

	if (p1->p_pid > p2->p_pid)
		return (p1);
	return (p2);
}

char *
osdep_get_name(int fd, char *tty)
{
	int		 mib[6] = { CTL_KERN, KERN_PROC, KERN_PROC_PGRP, 0,
				    sizeof(struct kinfo_proc), 0 };
	struct stat	 sb;
	size_t		 len;
	struct kinfo_proc *buf, *newbuf, *bestp;
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

	if ((newbuf = realloc(buf, len)) == NULL)
		goto error;
	buf = newbuf;

	mib[5] = (int)(len / sizeof(struct kinfo_proc));
	if (sysctl(mib, nitems(mib), buf, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			goto retry;
		goto error;
	}

	bestp = NULL;
	for (i = 0; i < len / sizeof (struct kinfo_proc); i++) {
		if ((dev_t)buf[i].p_tdev != sb.st_rdev)
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

char*
osdep_get_cwd(pid_t pid)
{
	int		name[] = { CTL_KERN, KERN_PROC_CWD, 0 };
	static char	path[MAXPATHLEN];
	size_t		pathlen = sizeof path;

	if ((name[2] = tcgetpgrp(fd)) == -1)
		return (NULL);
	if (sysctl(name, 3, path, &pathlen, NULL, 0) != 0)
		return (NULL);
	return (path);
}

struct event_base *
osdep_event_init(void)
{
	return (event_init());
}
