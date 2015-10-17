/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tmux.h"

FILE	*log_file;

void	 log_event_cb(int, const char *);
void	 log_vwrite(const char *, va_list);

/* Log callback for libevent. */
void
log_event_cb(unused int severity, const char *msg)
{
	log_debug("%s", msg);
}

/* Open logging to file. */
void
log_open(const char *path)
{
	if (log_file != NULL)
		fclose(log_file);

	log_file = fopen(path, "w");
	if (log_file == NULL)
		return;

	setvbuf(log_file, NULL, _IOLBF, 0);
	event_set_log_callback(log_event_cb);
}

/* Close logging. */
void
log_close(void)
{
	if (log_file != NULL)
		fclose(log_file);
	log_file = NULL;

	event_set_log_callback(NULL);
}

/* Write a log message. */
void
log_vwrite(const char *msg, va_list ap)
{
	char		*fmt, *out;
	struct timeval	 tv;

	if (log_file == NULL)
		return;

	if (vasprintf(&fmt, msg, ap) == -1)
		exit(1);
	if (stravis(&out, fmt, VIS_OCTAL|VIS_CSTYLE|VIS_TAB|VIS_NL) == -1)
		exit(1);

	gettimeofday(&tv, NULL);
	if (fprintf(log_file, "%lld.%06d %s\n", (long long)tv.tv_sec,
	    (int)tv.tv_usec, out) == -1)
		exit(1);
	fflush(log_file);

	free(out);
	free(fmt);
}

/* Log a debug message. */
void
log_debug(const char *msg, ...)
{
	va_list	ap;

	va_start(ap, msg);
	log_vwrite(msg, ap);
	va_end(ap);
}

/* Log a critical error with error string and die. */
__dead void
log_fatal(const char *msg, ...)
{
	char	*fmt;
	va_list	 ap;

	va_start(ap, msg);
	if (asprintf(&fmt, "fatal: %s: %s", msg, strerror(errno)) == -1)
		exit(1);
	log_vwrite(fmt, ap);
	exit(1);
}

/* Log a critical error and die. */
__dead void
log_fatalx(const char *msg, ...)
{
	char	*fmt;
	va_list	 ap;

	va_start(ap, msg);
	if (asprintf(&fmt, "fatal: %s", msg) == -1)
		exit(1);
	log_vwrite(fmt, ap);
	exit(1);
}
