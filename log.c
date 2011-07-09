/* $Id$ */

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
#include <time.h>

#include "tmux.h"

/* Logging type. */
#define LOG_TYPE_OFF 0
#define LOG_TYPE_TTY 1
#define LOG_TYPE_FILE 2
int	log_type = LOG_TYPE_OFF;

/* Log file, if needed. */
FILE   *log_file;

/* Debug level. */
int	log_level;

void		 log_vwrite(int, const char *, va_list);
__dead void	 log_vfatal(const char *, va_list);

/* Open logging to tty. */
void
log_open_tty(int level)
{
	log_type = LOG_TYPE_TTY;
	log_level = level;

	setlinebuf(stderr);
	setlinebuf(stdout);

	tzset();
}

/* Open logging to file. */
void
log_open_file(int level, const char *path)
{
	log_file = fopen(path, "w");
	if (log_file == NULL)
		return;

	log_type = LOG_TYPE_FILE;
	log_level = level;

	setlinebuf(log_file);

	tzset();
}

/* Close logging. */
void
log_close(void)
{
	if (log_type == LOG_TYPE_FILE)
		fclose(log_file);

	log_type = LOG_TYPE_OFF;
}

/* Write a log message. */
void
log_vwrite(int pri, const char *msg, va_list ap)
{
	char	*fmt;
	FILE	*f = log_file;

	switch (log_type) {
	case LOG_TYPE_TTY:
		if (pri == LOG_INFO)
			f = stdout;
		else
			f = stderr;
		/* FALLTHROUGH */
	case LOG_TYPE_FILE:
		if (asprintf(&fmt, "%s\n", msg) == -1)
			exit(1);
		if (vfprintf(f, fmt, ap) == -1)
			exit(1);
		fflush(f);
		free(fmt);
		break;
	}
}

/* Log a warning with error string. */
void printflike1
log_warn(const char *msg, ...)
{
	va_list	 ap;
	char	*fmt;

	va_start(ap, msg);
	if (asprintf(&fmt, "%s: %s", msg, strerror(errno)) == -1)
		exit(1);
	log_vwrite(LOG_CRIT, fmt, ap);
	free(fmt);
	va_end(ap);
}

/* Log a warning. */
void printflike1
log_warnx(const char *msg, ...)
{
	va_list	ap;

	va_start(ap, msg);
	log_vwrite(LOG_CRIT, msg, ap);
	va_end(ap);
}

/* Log an informational message. */
void printflike1
log_info(const char *msg, ...)
{
	va_list	ap;

	if (log_level > -1) {
		va_start(ap, msg);
		log_vwrite(LOG_INFO, msg, ap);
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
		log_vwrite(LOG_DEBUG, msg, ap);
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
		log_vwrite(LOG_DEBUG, msg, ap);
		va_end(ap);
	}
}

/* Log a critical error, with error string if necessary, and die. */
__dead void
log_vfatal(const char *msg, va_list ap)
{
	char	*fmt;

	if (errno != 0) {
		if (asprintf(&fmt, "fatal: %s: %s", msg, strerror(errno)) == -1)
			exit(1);
		log_vwrite(LOG_CRIT, fmt, ap);
	} else {
		if (asprintf(&fmt, "fatal: %s", msg) == -1)
			exit(1);
		log_vwrite(LOG_CRIT, fmt, ap);
	}
	free(fmt);

	exit(1);
}

/* Log a critical error, with error string, and die. */
__dead void printflike1
log_fatal(const char *msg, ...)
{
	va_list	ap;

	va_start(ap, msg);
	log_vfatal(msg, ap);
}

/* Log a critical error and die. */
__dead void printflike1
log_fatalx(const char *msg, ...)
{
	va_list	ap;

	errno = 0;
	va_start(ap, msg);
	log_vfatal(msg, ap);
}
