/* $Id: asprintf.c,v 1.4 2008-09-26 06:45:28 nicm Exp $ */

/*
 * Copyright (c) 2006 Nicholas Marriott <nicm@users.sourceforge.net>
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

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "tmux.h"

int
asprintf(char **ret, const char *format, ...)
{
	va_list	ap;
	int	n;

	va_start(ap, format);
	n = vasprintf(ret, format, ap);
	va_end(ap);

	return (n);
}

#ifndef BROKEN_VSNPRINTF
int
vasprintf(char **ret, const char *format, va_list ap)
{
	int	 n;

	if ((n = vsnprintf(NULL, 0, format, ap)) < 0)
		goto error;

	*ret = xmalloc(n + 1);
	if ((n = vsnprintf(*ret, n + 1, format, ap)) < 0) {
		xfree(*ret);
		goto error;
	}

	return (n);

error:
	*ret = NULL;
	return (-1);
}
#else
int
vasprintf(char **ret, const char *fmt, va_list ap)
{
	va_list       aq;
	size_t        len;
	char         *buf;
	int           n;

	len = 64;
	buf = xmalloc(len);

	for (;;) {
		va_copy(aq, ap);
		n = vsnprintf(buf, len, fmt, aq);
		va_end(aq);

		if (n != -1) {
			*ret = buf;
			return (n);
		}

		if (len > SIZE_MAX / 2) {
			xfree(buf);
			return (-1);
		}
		len *= 2;
	}
}
#endif
