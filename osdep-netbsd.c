/* $Id: osdep-netbsd.c,v 1.4 2009-06-26 15:54:52 nicm Exp $ */

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
#include <sys/sysctl.h>

#include <string.h>
#include <unistd.h>

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

char	*osdep_get_name(int, char *);

char *
osdep_get_name(int fd, __unused char *tty)
{
	int		 mib[6];
	size_t		 len;
	struct kinfo_proc2 p;

	if ((mib[3] = tcgetpgrp(fd)) == -1)
		return (NULL);

	len = sizeof(p);
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC2;
	mib[2] = KERN_PROC_PID;
	mib[4] = sizeof(p);
	mib[5] = 1;
	if (sysctl(mib, nitems(mib), &p, &len, NULL, 0) == -1)
		return (NULL);

	return (strdup(p.p_comm));
}
