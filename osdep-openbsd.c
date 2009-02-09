/* $Id: osdep-openbsd.c,v 1.12 2009-02-09 18:08:01 nicm Exp $ */

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

#ifdef __OpenBSD__

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))

#define is_runnable(p) \
	((p)->p_stat == SRUN || (p)->p_stat == SIDL || (p)->p_stat == SONPROC)
#define is_stopped(p) \
	((p)->p_stat == SSTOP || (p)->p_stat == SZOMB || (p)->p_stat == SDEAD)

int	 osdep_get_name(int, char *, pid_t *, char **);
char	*osdep_get_argv0(pid_t);

int
osdep_get_name(int fd, char *tty, pid_t *last_pid, char **name)
{
	int		 mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PGRP, 0 };
	struct stat	 sb;
	size_t		 len;
	struct kinfo_proc *buf, *newbuf;
	struct proc	*p, *bestp;
	u_int		 i;

	*name = NULL;

	buf = NULL;

	if (stat(tty, &sb) == -1)
		return (-1);
	if ((mib[3] = tcgetpgrp(fd)) == -1)
		return (-1);

retry:
	if (sysctl(mib, nitems(mib), NULL, &len, NULL, 0) == -1)
		return (-1);
	len = (len * 5) / 4;

	if ((newbuf = realloc(buf, len)) == NULL) {
		free(buf);
		return (-1);
	}
	buf = newbuf;

	if (sysctl(mib, nitems(mib), buf, &len, NULL, 0) == -1) {
		if (errno == ENOMEM)
			goto retry;
		free(buf);
		return (-1);
	}

	bestp = NULL;
	for (i = 0; i < len / sizeof (struct kinfo_proc); i++) {
		if (buf[i].kp_eproc.e_tdev != sb.st_rdev)
			continue;
		p = &buf[i].kp_proc;
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

		if (p->p_flag & P_SINTR && !(bestp->p_flag & P_SINTR))
			bestp = p;
		else if (!(p->p_flag & P_SINTR) && bestp->p_flag & P_SINTR)
			continue;

		if (LIST_FIRST(&p->p_children) == NULL &&
		    LIST_FIRST(&bestp->p_children) != NULL) /* XXX ugh */
			bestp = p;
		else if (LIST_FIRST(&p->p_children) != NULL &&
		    LIST_FIRST(&bestp->p_children) == NULL)
			continue;

		if (strcmp(p->p_comm, p->p_comm) < 0)
			bestp = p;
		else if (strcmp(p->p_comm, p->p_comm) > 0)
			continue;

		if (p->p_pid > bestp->p_pid)
			bestp = p;
	}

	if (bestp == NULL) {
		free(buf);
		return (-1);
	}

	if (bestp->p_pid == *last_pid) {
 		free(buf);
		return (1);
	}
	*last_pid = bestp->p_pid;

	*name = osdep_get_argv0(bestp->p_pid);
	if (*name == NULL || **name == '\0') {
		free(*name);
		*name = strdup(bestp->p_comm);
	}
	free(buf);
	return (0);
}

char *
osdep_get_argv0(pid_t pid)
{
	int	mib[4] = { CTL_KERN, KERN_PROC_ARGS, 0, KERN_PROC_ARGV };
        size_t	size;
	char  **args, **args2, *procname;

	procname = NULL;

	mib[2] = pid;

	args = NULL;
	size = 128;
	while (size < SIZE_MAX / 2) {
		size *= 2;
		if ((args2 = realloc(args, size)) == NULL)
			break;
		args = args2;
		if (sysctl(mib, 4, args, &size, NULL, 0) == -1) {
			if (errno == ENOMEM)
				continue;
			break;
		}
		if (*args != NULL)
			procname = strdup(*args);
		break;
	}
	free(args);

	return (procname);
}

#endif
