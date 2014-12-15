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
#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif
#include <string.h>

#include "tmux.h"

int
asprintf(char **ret, const char *fmt, ...)
{
	va_list	ap;
	int	n;

	va_start(ap, fmt);
	n = vasprintf(ret, fmt, ap);
	va_end(ap);

	return (n);
}

int
vasprintf(char **ret, const char *fmt, va_list ap)
{
	int	 n;
	va_list  ap2;

	va_copy(ap2, ap);

	if ((n = vsnprintf(NULL, 0, fmt, ap)) < 0)
		goto error;

	*ret = xmalloc(n + 1);
	if ((n = vsnprintf(*ret, n + 1, fmt, ap2)) < 0) {
		free(*ret);
		goto error;
	}
	va_end(ap2);

	return (n);

error:
	va_end(ap2);
	*ret = NULL;
	return (-1);
}
