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
#include <sys/user.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libutil.h>

struct kinfo_proc	*cmp_procs(struct kinfo_proc *, struct kinfo_proc *);
char			*osdep_get_name(int, char *);
char			*osdep_get_cwd(int);
struct event_base	*osdep_event_init(void);

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define is_runnable(p) \
	((p)->ki_stat == SRUN || (p)->ki_stat == SIDL)
#define is_stopped(p) \
	((p)->ki_stat == SSTOP || (p)->ki_stat == SZOMB)

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

	if (p1->ki_estcpu > p2->ki_estcpu)
		return (p1);
	if (p1->ki_estcpu < p2->ki_estcpu)
		return (p2);

	if (p1->ki_slptime < p2->ki_slptime)
		return (p1);
	if (p1->ki_slptime > p2->ki_slptime)
		return (p2);

	if (strcmp(p1->ki_comm, p2->ki_comm) < 0)
		return (p1);
	if (strcmp(p1->ki_comm, p2->ki_comm) > 0)
		return (p2);

	if (p1->ki_pid > p2->ki_pid)
		return (p1);
	return (p2);
}

char *
osdep_get_name(int fd, char *tty)
{
	int		 mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PGRP, 0 };
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

	if (sysctl(mib, nitems(mib), buf, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			goto retry;
		goto error;
	}

	bestp = NULL;
	for (i = 0; i < len / sizeof (struct kinfo_proc); i++) {
		if (buf[i].ki_tdev != sb.st_rdev)
			continue;
		if (bestp == NULL)
			bestp = &buf[i];
		else
			bestp = cmp_procs(&buf[i], bestp);
	}

	name = NULL;
	if (bestp != NULL)
		name = strdup(bestp->ki_comm);

	free(buf);
	return (name);

error:
	free(buf);
	return (NULL);
}

static char *
osdep_get_cwd_fallback(int fd)
{
	static char		 wd[PATH_MAX];
	struct kinfo_file	*info = NULL;
	pid_t			 pgrp;
	int			 nrecords, i;

	if ((pgrp = tcgetpgrp(fd)) == -1)
		return (NULL);

	if ((info = kinfo_getfile(pgrp, &nrecords)) == NULL)
		return (NULL);

	for (i = 0; i < nrecords; i++) {
		if (info[i].kf_fd == KF_FD_TYPE_CWD) {
			strlcpy(wd, info[i].kf_path, sizeof wd);
			free(info);
			return (wd);
		}
	}

	free(info);
	return (NULL);
}

#ifdef KERN_PROC_CWD
char *
osdep_get_cwd(int fd)
{
	static struct kinfo_file	info;
	static int			fallback;
	int	name[] = { CTL_KERN, KERN_PROC, KERN_PROC_CWD, 0 };
	size_t	len = sizeof info;

	if (fallback)
		return (osdep_get_cwd_fallback(fd));

	if ((name[3] = tcgetpgrp(fd)) == -1)
		return (NULL);

	if (sysctl(name, 4, &info, &len, NULL, 0) == -1) {
		if (errno == ENOENT) {
			fallback = 1;
			return (osdep_get_cwd_fallback(fd));
		}
		return (NULL);
	}
	return (info.kf_path);
}
#else /* !KERN_PROC_CWD */
char *
osdep_get_cwd(int fd)
{
	return (osdep_get_cwd_fallback(fd));
}
#endif /* KERN_PROC_CWD */

struct event_base *
osdep_event_init(void)
{
	/*
	 * On some versions of FreeBSD, kqueue doesn't work properly on tty
	 * file descriptors. This is fixed in recent FreeBSD versions.
	 */
	setenv("EVENT_NOKQUEUE", "1", 1);
	return (event_init());
}
