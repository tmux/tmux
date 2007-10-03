/* $Id: xmalloc.c,v 1.4 2007-10-03 13:07:42 nicm Exp $ */

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

#include <sys/param.h>

#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

void *
ensure_for(void *buf, size_t *len, size_t size, size_t adj)
{
	if (adj == 0)
		fatalx("zero adj");

	if (SIZE_MAX - size < adj)
		fatalx("size + adj > SIZE_MAX");
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
		fatalx("zero size");
	if (SIZE_MAX / nmemb < size)
		fatalx("nmemb * size > SIZE_MAX");

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
xmemstrdup(const char *buf, size_t len)
{
	char	*s;

	s = xmalloc(len + 1);
	if (len > 0)
		memcpy(s, buf, len);
	s[len] = '\0';

	return (s);
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
                fatalx("zero size");
        if (SIZE_MAX / nmemb < size)
                fatalx("nmemb * size > SIZE_MAX");
        if ((ptr = calloc(nmemb, size)) == NULL)
		fatal("xcalloc failed");

#ifdef DEBUG
	xmalloc_new(xmalloc_caller(), ptr, nmemb * size);
#endif
        return (ptr);
}

void *
xmalloc(size_t size)
{
	void	*ptr;

        if (size == 0)
                fatalx("zero size");
        if ((ptr = malloc(size)) == NULL)
		fatal("xmalloc failed");

#ifdef DEBUG
	xmalloc_new(xmalloc_caller(), ptr, size);
#endif
        return (ptr);
}

void *
xrealloc(void *oldptr, size_t nmemb, size_t size)
{
	size_t	 newsize = nmemb * size;
	void	*newptr;

	if (newsize == 0)
                fatalx("zero size");
        if (SIZE_MAX / nmemb < size)
                fatalx("nmemb * size > SIZE_MAX");
        if ((newptr = realloc(oldptr, newsize)) == NULL)
		fatal("xrealloc failed");

#ifdef DEBUG
	xmalloc_change(xmalloc_caller(), oldptr, newptr, nmemb * size);
#endif
        return (newptr);
}

void
xfree(void *ptr)
{
	if (ptr == NULL)
		fatalx("null pointer");
	free(ptr);

#ifdef DEBUG
	xmalloc_free(ptr);
#endif
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
                fatal("xvasprintf failed");

#ifdef DEBUG
	xmalloc_new(xmalloc_caller(), *ret, i + 1);
#endif
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

	if (len > INT_MAX)
		fatalx("len > INT_MAX");

	i = vsnprintf(buf, len, fmt, ap);
        if (i < 0)
                fatal("vsnprintf failed");

        return (i);
}

/*
 * Some systems modify the path in place. This function and xbasename below
 * avoid that by using a temporary buffer.
 */
char *
xdirname(const char *src)
{
	static char	dst[MAXPATHLEN];

	strlcpy(dst, src, sizeof dst);
	return (dirname(dst));
}

char *
xbasename(const char *src)
{
	static char	dst[MAXPATHLEN];

	strlcpy(dst, src, sizeof dst);
	return (basename(dst));
}
