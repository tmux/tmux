/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
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
#include <unistd.h>

#include "tmux.h"

static FILE	*log_file;
static int	 log_level;

/* Log callback for libevent. */
static void
log_event_cb(__unused int severity, const char *msg)
{
	log_debug("%s", msg);
}

/* Increment log level. */
void
log_add_level(void)
{
	log_level++;
}

/* Get log level. */
int
log_get_level(void)
{
	return (log_level);
}

/* Open logging to file. */
void
log_open(const char *name)
{
	char	*path;

	if (log_level == 0)
		return;
	log_close();

	xasprintf(&path, "tmux-%s-%ld.log", name, (long)getpid());
	log_file = fopen(path, "a");
	free(path);
	if (log_file == NULL)
		return;

	setvbuf(log_file, NULL, _IOLBF, 0);
	event_set_log_callback(log_event_cb);
}

/* Toggle logging. */
void
log_toggle(const char *name)
{
	if (log_level == 0) {
		log_level = 1;
		log_open(name);
		log_debug("log opened");
	} else {
		log_debug("log closed");
		log_level = 0;
		log_close();
	}
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
static void printflike(1, 0)
log_vwrite(const char *msg, va_list ap, const char *prefix)
{
	char		*s, *out;
	struct timeval	 tv;

	if (log_file == NULL)
		return;

	if (vasprintf(&s, msg, ap) == -1)
		return;
	if (stravis(&out, s, VIS_OCTAL|VIS_CSTYLE|VIS_TAB|VIS_NL) == -1) {
		free(s);
		return;
	}
	free(s);

	gettimeofday(&tv, NULL);
	if (fprintf(log_file, "%lld.%06d %s%s\n", (long long)tv.tv_sec,
	    (int)tv.tv_usec, prefix, out) != -1)
		fflush(log_file);
	free(out);
}

/* Log a debug message. */
void
log_debug(const char *msg, ...)
{
	va_list	ap;

	if (log_file == NULL)
		return;

	va_start(ap, msg);
	log_vwrite(msg, ap, "");
	va_end(ap);
}

/* Log a critical error with error string and die. */
__dead void
fatal(const char *msg, ...)
{
	char	 tmp[256];
	va_list	 ap;

	if (snprintf(tmp, sizeof tmp, "fatal: %s: ", strerror(errno)) < 0)
		exit(1);

	va_start(ap, msg);
	log_vwrite(msg, ap, tmp);
	va_end(ap);

	exit(1);
}

/* Log a critical error and die. */
__dead void
fatalx(const char *msg, ...)
{
	va_list	 ap;

	va_start(ap, msg);
	log_vwrite(msg, ap, "fatal: ");
	va_end(ap);

	exit(1);
}
