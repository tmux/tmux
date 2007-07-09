/* $Id: log.c,v 1.1.1.1 2007-07-09 19:03:33 nicm Exp $ */

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
#include <syslog.h>

#include "tmux.h"

/* Logging enabled. */
int	 log_enabled;

/* Log stream or NULL to use syslog. */
FILE	*log_stream;

/* Debug level. */
int	 log_level;

/* Open log. */
void
log_open(FILE *f, int facility, int level)
{
	log_stream = f;
	log_level = level;

	if (f == NULL)
		openlog(__progname, LOG_PID|LOG_NDELAY, facility);
	tzset();

	log_enabled = 1;
}

/* Close logging. */
void
log_close(void)
{
	if (log_stream != NULL)
		fclose(log_stream);

	log_enabled = 0;
}

/* Write a log message. */
void
log_write(FILE *f, int priority, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	log_vwrite(f, priority, fmt, ap);
	va_end(ap);
}

/* Write a log message. */
void
log_vwrite(FILE *f, int priority, const char *fmt, va_list ap)
{
	if (!log_enabled)
		return;

	if (f == NULL) {
		vsyslog(priority, fmt, ap);
		return;
	}

	if (vfprintf(f, fmt, ap) == -1)
		exit(1);
	if (fputc('\n', f) == EOF)
		exit(1);
	fflush(log_stream);
}

/* Log a warning with error string. */
void printflike1
log_warn(const char *msg, ...)
{
	va_list	 ap;
	char	*fmt;

	if (!log_enabled)
		return;

	va_start(ap, msg);
	if (asprintf(&fmt, "%s: %s", msg, strerror(errno)) == -1)
		exit(1);
	log_vwrite(log_stream, LOG_CRIT, fmt, ap);
	xfree(fmt);
	va_end(ap);
}

/* Log a warning. */
void printflike1
log_warnx(const char *msg, ...)
{
	va_list	ap;

	va_start(ap, msg);
	log_vwrite(log_stream, LOG_CRIT, msg, ap);
	va_end(ap);
}

/* Log an informational message. */
void printflike1
log_info(const char *msg, ...)
{
	va_list	ap;

	if (log_level > -1) {
		va_start(ap, msg);
		if (log_stream == stderr)
			log_vwrite(stdout, LOG_INFO, msg, ap);
		else
			log_vwrite(log_stream, LOG_INFO, msg, ap);
		va_end(ap);
	}
}

/* Log a debug message. */
void printflike1
log_debug(const char *msg, ...)
{
	va_list	ap;

	if (log_level > 0) {
		va_start(ap, msg);
		log_vwrite(log_stream, LOG_DEBUG, msg, ap);
		va_end(ap);
	}
}

/* Log a debug message at level 2. */
void printflike1
log_debug2(const char *msg, ...)
{
	va_list	ap;

	if (log_level > 1) {
		va_start(ap, msg);
		log_vwrite(log_stream, LOG_DEBUG, msg, ap);
		va_end(ap);
	}
}

/* Log a debug message at level 3. */
void printflike1
log_debug3(const char *msg, ...)
{
	va_list	ap;

	if (log_level > 2) {
		va_start(ap, msg);
		log_vwrite(log_stream, LOG_DEBUG, msg, ap);
		va_end(ap);
	}
}

/* Log a critical error, with error string if necessary, and die. */
__dead void
log_vfatal(const char *msg, va_list ap)
{
	char	*fmt;

	if (!log_enabled)
		exit(1);

	if (errno != 0) {
		if (asprintf(&fmt, "fatal: %s: %s", msg, strerror(errno)) == -1)
			exit(1);
		log_vwrite(log_stream, LOG_CRIT, fmt, ap);
	} else {
		if (asprintf(&fmt, "fatal: %s", msg) == -1)
			exit(1);
		log_vwrite(log_stream, LOG_CRIT, fmt, ap);
	}
	xfree(fmt);

	exit(1);
}

/* Log a critical error, with error string, and die. */
__dead void
log_fatal(const char *msg, ...)
{
	va_list	ap;

	va_start(ap, msg);
	log_vfatal(msg, ap);
}

/* Log a critical error and die. */
__dead void
log_fatalx(const char *msg, ...)
{
	va_list	ap;

	errno = 0;
	va_start(ap, msg);
	log_vfatal(msg, ap);
}
