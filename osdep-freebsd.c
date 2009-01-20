/* $Id: osdep-freebsd.c,v 1.1 2009-01-20 19:35:03 nicm Exp $ */

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

#ifdef __FreeBSD__

#include <sys/param.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char	*get_argv0(pid_t);

char *
get_argv0(pid_t pgrp)
{
	int	mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ARGS, 0 };
        size_t	size;
	char   *args, *args2, *procname;

	procname = NULL;

	mib[3] = pgrp;

	args = NULL;
	size = 128;
	while (size < SIZE_MAX / 2) {
		size *= 2;
		if ((args2 = realloc(args, 2 * size)) == NULL)
			break;
		args = args2;
		if (sysctl(mib, 4, args, &size, NULL, 0) == -1) {
			if (errno == ENOMEM)
				continue;
			break;
		}
		procname = strdup(args);
		break;
	}
	free(args);

	return (procname);
}

#endif
