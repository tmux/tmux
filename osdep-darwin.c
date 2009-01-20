/* $Id: osdep-darwin.c,v 1.1 2009-01-20 22:17:53 nicm Exp $ */

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

#ifdef __APPLE__

#include <sys/types.h>
#include <sys/sysctl.h>

#include <stdlib.h>

char	*get_argv0(pid_t);

/*
 * XXX This actually returns the executable path, not the process's argv[0].
 * Anyone who wishes to complain about this is welcome to grab a copy of
 * Apple's 'ps' source and start digging.
 */

char *
get_argv0(pid_t pgrp)
{
	int	mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, 0 };
        size_t	size;
	struct kinfo_proc kp;
	
	mib[3] = pgrp;
	size = sizeof kp;
	if (sysctl(mib, 4, &kp, &size, NULL, 0) == -1 ||
	    kp.kp_proc.p_comm[0] == '\0')
		return (NULL);

	return (strdup(kp.kp_proc.p_comm));
}

#endif
