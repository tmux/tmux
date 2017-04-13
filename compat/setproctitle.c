/*
 * Copyright (c) 2016 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdarg.h>
#include <string.h>

#include "compat.h"

#if defined(HAVE_PRCTL) && defined(HAVE_PR_SET_NAME)

#include <sys/prctl.h>

void
setproctitle(const char *fmt, ...)
{
	char	title[16], name[16], *cp;
	va_list	ap;
	int	used;

	va_start(ap, fmt);
	vsnprintf(title, sizeof title, fmt, ap);
	va_end(ap);

	used = snprintf(name, sizeof name, "%s: %s", getprogname(), title);
	if (used >= (int)sizeof name) {
		cp = strrchr(name, ' ');
		if (cp != NULL)
			*cp = '\0';
	}
	prctl(PR_SET_NAME, name);
}
#else
void
setproctitle(__unused const char *fmt, ...)
{
}
#endif
