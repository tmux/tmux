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

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "compat.h"

void
err(int eval, const char *fmt, ...)
{
	va_list	 ap;
	int	 saved_errno = errno;

	fprintf(stderr, "%s: ", getprogname());

	va_start(ap, fmt);
	if (fmt != NULL) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	va_end(ap);

	fprintf(stderr, "%s\n", strerror(saved_errno));
	exit(eval);
}

void
errx(int eval, const char *fmt, ...)
{
	va_list	 ap;

	fprintf(stderr, "%s: ", getprogname());

	va_start(ap, fmt);
	if (fmt != NULL)
		vfprintf(stderr, fmt, ap);
	va_end(ap);

	putc('\n', stderr);
	exit(eval);
}

void
warn(const char *fmt, ...)
{
	va_list	 ap;
	int	 saved_errno = errno;

	fprintf(stderr, "%s: ", getprogname());

	va_start(ap, fmt);
	if (fmt != NULL) {
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, ": ");
	}
	va_end(ap);

	fprintf(stderr, "%s\n", strerror(saved_errno));
}

void
warnx(const char *fmt, ...)
{
	va_list	 ap;

	fprintf(stderr, "%s: ", getprogname());

	va_start(ap, fmt);
	if (fmt != NULL)
		vfprintf(stderr, fmt, ap);
	va_end(ap);

	putc('\n', stderr);
}
