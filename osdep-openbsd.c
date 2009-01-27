/* $Id: osdep-openbsd.c,v 1.6 2009-01-27 23:10:18 nicm Exp $ */

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

char	*get_argv0(int, char *);
char	*get_proc_argv0(pid_t);

char *
get_argv0(__attribute__ ((unused)) int fd, char *tty)
{
	int		 mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_TTY, 0 };
	struct stat	 sb;
	size_t		 len;
	struct kinfo_proc *buf, *newbuf;
	struct proc	*p, *bestp;
	char		*procname;
	u_int		 i;

	buf = NULL;

	if (stat(tty, &sb) == -1)
		return (NULL);
	mib[3] = sb.st_rdev;

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
		if (buf[i].kp_eproc.e_tdev != sb.st_rdev)
			continue;
		p = &buf[i].kp_proc;
		//log_debug("XXX %d %s %d %d %d", i, p->p_comm, p->p_stat, p->p_estcpu, p->p_slptime);
		if (bestp == NULL)
			bestp = p;

		if (p->p_stat != SRUN &&
		    p->p_stat != SIDL &&
		    p->p_stat != SONPROC)
			continue;
		if (p->p_estcpu < bestp->p_estcpu)
			continue;
		if (p->p_slptime > bestp->p_slptime)
			continue;
		if (!(p->p_flag & P_SINTR) && bestp->p_flag & P_SINTR)
			continue;
		if (p->p_pid < bestp->p_pid)
			continue;
		bestp = p;
	}	
	if (bestp != NULL) {
		procname = get_proc_argv0(bestp->p_pid);
		if (procname == NULL || *procname == '\0') {
			free(procname);
			procname = strdup(bestp->p_comm);
		}
	} else
		procname = NULL;

	free(buf);
	return (procname);
}

char *
get_proc_argv0(pid_t pid)
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
