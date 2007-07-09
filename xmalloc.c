/* $Id: xmalloc.c,v 1.1.1.1 2007-07-09 19:03:33 nicm Exp $ */

/*
 * Copyright (c) 2004 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

void *
ensure_for(void *buf, size_t *len, size_t size, size_t adj)
{
	if (adj == 0)
		log_fatalx("ensure_for: zero adj");

	if (SIZE_MAX - size < adj)
		log_fatalx("ensure_for: size + adj > SIZE_MAX");
	size += adj;

	if (*len == 0) {
		*len = BUFSIZ;
		buf = xmalloc(*len);
	}

	while (*len <= size) {
		buf = xrealloc(buf, 2, *len);
		*len *= 2;
	}

	return (buf);
}

void *
ensure_size(void *buf, size_t *len, size_t nmemb, size_t size)
{
	if (nmemb == 0 || size == 0)
		log_fatalx("ensure_size: zero size");
	if (SIZE_MAX / nmemb < size)
		log_fatalx("ensure_size: nmemb * size > SIZE_MAX");

	if (*len == 0) {
		*len = BUFSIZ;
		buf = xmalloc(*len);
	}

	while (*len <= nmemb * size) {
		buf = xrealloc(buf, 2, *len);
		*len *= 2;
	}

	return (buf);
}

char *
xstrdup(const char *s)
{
	void	*ptr;
	size_t	 len;

	len = strlen(s) + 1;
	ptr = xmalloc(len);

        return (strncpy(ptr, s, len));
}

void *
xcalloc(size_t nmemb, size_t size)
{
        void	*ptr;

        if (size == 0 || nmemb == 0)
                log_fatalx("xcalloc: zero size");
        if (SIZE_MAX / nmemb < size)
                log_fatalx("xcalloc: nmemb * size > SIZE_MAX");
        if ((ptr = calloc(nmemb, size)) == NULL)
		log_fatal("xcalloc");

        return (ptr);
}

void *
xmalloc(size_t size)
{
        void	*ptr;

        if (size == 0)
		log_fatalx("xmalloc: zero size");
        if ((ptr = malloc(size)) == NULL)
		log_fatal("xmalloc");

        return (ptr);
}

void *
xrealloc(void *oldptr, size_t nmemb, size_t size)
{
	size_t	 newsize = nmemb * size;
	void	*newptr;

	if (newsize == 0)
                log_fatalx("xrealloc: zero size");
        if (SIZE_MAX / nmemb < size)
                log_fatal("xrealloc: nmemb * size > SIZE_MAX");
        if ((newptr = realloc(oldptr, newsize)) == NULL)
		log_fatal("xrealloc");

        return (newptr);
}

void
xfree(void *ptr)
{
	if (ptr == NULL)
		log_fatalx("xfree: null pointer");
	free(ptr);
}

int printflike2
xasprintf(char **ret, const char *fmt, ...)
{
        va_list ap;
        int	i;

        va_start(ap, fmt);
        i = xvasprintf(ret, fmt, ap);
        va_end(ap);

	return (i);
}

int
xvasprintf(char **ret, const char *fmt, va_list ap)
{
	int	i;

	i = vasprintf(ret, fmt, ap);

        if (i < 0 || *ret == NULL)
                log_fatal("xvasprintf");

        return (i);
}

int printflike3
xsnprintf(char *buf, size_t len, const char *fmt, ...)
{
        va_list ap;
        int	i;

        va_start(ap, fmt);
        i = xvsnprintf(buf, len, fmt, ap);
        va_end(ap);

	return (i);
}

int
xvsnprintf(char *buf, size_t len, const char *fmt, va_list ap)
{
	int	i;

	if (len > INT_MAX) {
		errno = EINVAL;
		log_fatal("xvsnprintf");
	}

	i = vsnprintf(buf, len, fmt, ap);

        if (i < 0)
                log_fatal("xvsnprintf");

        return (i);
}

/*
 * Print a path. Same as xsnprintf, but return ENAMETOOLONG on truncation.
 */
int printflike3
printpath(char *buf, size_t len, const char *fmt, ...)
{
	va_list	ap;
	int	n;

	if (len > INT_MAX) {
		errno = ENAMETOOLONG;
		return (1);
	}

	va_start(ap, fmt);
	n = xvsnprintf(buf, len, fmt, ap);
	va_end(ap);

	if ((size_t) n > len) {
		errno = ENAMETOOLONG;
		return (1);
	}

	return (0);
}

/*
 * Some system modify the path in place. This function and xbasename below
 * avoid that by using a temporary buffer.
 */
char *
xdirname(const char *src)
{
	char	dst[MAXPATHLEN];

	strlcpy(dst, src, sizeof dst);
	return (dirname(dst));
}

char *
xbasename(const char *src)
{
	char	dst[MAXPATHLEN];

	strlcpy(dst, src, sizeof dst);
	return (basename(dst));
}
