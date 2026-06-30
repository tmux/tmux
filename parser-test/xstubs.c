/* Minimal implementations of the helpers cmd-parse.y links against. */

#include "tmux.h"

#include <errno.h>

void *
xmalloc(size_t size)
{
	void	*ptr;

	if (size == 0)
		size = 1;
	if ((ptr = malloc(size)) == NULL)
		fatalx("xmalloc");
	return (ptr);
}

void *
xcalloc(size_t nmemb, size_t size)
{
	void	*ptr;

	if (nmemb == 0 || size == 0)
		nmemb = size = 1;
	if ((ptr = calloc(nmemb, size)) == NULL)
		fatalx("xcalloc");
	return (ptr);
}

void *
xrealloc(void *oldptr, size_t newsize)
{
	void	*ptr;

	if (newsize == 0)
		newsize = 1;
	if ((ptr = realloc(oldptr, newsize)) == NULL)
		fatalx("xrealloc");
	return (ptr);
}

char *
xstrdup(const char *s)
{
	char	*ptr;

	if ((ptr = strdup(s)) == NULL)
		fatalx("xstrdup");
	return (ptr);
}

char *
xstrndup(const char *s, size_t maxlen)
{
	char	*ptr;

	if ((ptr = strndup(s, maxlen)) == NULL)
		fatalx("xstrndup");
	return (ptr);
}

int
xvasprintf(char **ret, const char *fmt, va_list ap)
{
	int	i;

	if ((i = vasprintf(ret, fmt, ap)) < 0)
		fatalx("xvasprintf");
	return (i);
}

int
xasprintf(char **ret, const char *fmt, ...)
{
	va_list	ap;
	int	i;

	va_start(ap, fmt);
	i = xvasprintf(ret, fmt, ap);
	va_end(ap);
	return (i);
}

void
log_debug(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

__dead void
fatal(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(errno));
	exit(1);
}

__dead void
fatalx(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}
