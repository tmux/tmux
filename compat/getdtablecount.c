/*
 * Copyright (c) 2017 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <sys/types.h>

#include <glob.h>
#include <unistd.h>

#include "compat.h"

void fatal(const char *, ...);
void fatalx(const char *, ...);

#ifdef HAVE_PROC_PID
int
getdtablecount(void)
{
	char	path[PATH_MAX];
	glob_t	g;
	int	n = 0;

	if (snprintf(path, sizeof path, "/proc/%ld/fd/*", (long)getpid()) < 0)
		fatal("snprintf overflow");
	if (glob(path, 0, NULL, &g) == 0)
		n = g.gl_pathc;
	globfree(&g);
	return (n);
}
#else
int
getdtablecount(void)
{
	return (0);
}
#endif
