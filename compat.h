/* $Id: compat.h,v 1.9 2009-08-14 21:13:48 tcunha Exp $ */

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

#ifndef HAVE_PATHS_H
#define	_PATH_BSHELL	"/bin/sh"
#define	_PATH_TMP	"/tmp/"
#define _PATH_DEVNULL	"/dev/null"
#define _PATH_TTY	"/dev/tty"
#define _PATH_DEV	"/dev/"
#endif

#ifdef HAVE_QUEUE_H
#include <sys/queue.h>
#else
#include "compat/queue.h"
#endif

#ifdef HAVE_TREE_H
#include <sys/tree.h>
#else
#include "compat/tree.h"
#endif
 
#ifdef HAVE_BITSTRING_H
#include <bitstring.h>
#else
#include "compat/bitstring.h"
#endif

#ifdef HAVE_POLL
#include <poll.h>
#else
#include "compat/bsd-poll.h"
#endif

#ifdef HAVE_GETOPT
#include <getopt.h>
#endif

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef HAVE_FORKPTY
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#ifdef HAVE_PTY_H
#include <pty.h>
#endif
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#endif

#ifdef HAVE_VIS
#include <vis.h>
#else
#include "compat/vis.h"
#endif

#ifndef HAVE_IMSG
#include "compat/imsg.h"
#endif

#ifndef INFTIM
#define INFTIM -1
#endif

#ifndef WAIT_ANY
#define WAIT_ANY -1
#endif

#ifndef SUN_LEN
#define SUN_LEN(sun) (sizeof (sun)->sun_path)
#endif

#ifndef __dead
#define __dead __attribute__ ((__noreturn__))
#endif
#ifndef __packed
#define __packed __attribute__ ((__packed__))
#endif

#ifndef timercmp
#define	timercmp(tvp, uvp, cmp)						\
	(((tvp)->tv_sec == (uvp)->tv_sec) ?				\
	    ((tvp)->tv_usec cmp (uvp)->tv_usec) :			\
	    ((tvp)->tv_sec cmp (uvp)->tv_sec))
#endif

#ifndef timeradd
#define	timeradd(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec + (uvp)->tv_usec;	\
		if ((vvp)->tv_usec >= 1000000) {			\
			(vvp)->tv_sec++;				\
			(vvp)->tv_usec -= 1000000;			\
		}							\
	} while (0)
#endif

#ifndef PASS_MAX
#define PASS_MAX 128
#endif

#ifndef TTY_NAME_MAX
#define TTY_NAME_MAX 32
#endif

#ifndef HAVE_STRCASESTR
/* strcasestr.c */
char		*strcasestr(const char *, const char *);
#endif

#ifndef HAVE_STRTONUM
/* strtonum.c */
long long	 strtonum(const char *, long long, long long, const char **);
#endif

#ifndef HAVE_STRLCPY
/* strlcpy.c */
size_t	 	 strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCAT
/* strlcat.c */
size_t	 	 strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_DAEMON
/* daemon.c */
int	 	 daemon(int, int);
#endif

#ifndef HAVE_FORKPTY
/* forkpty.c */
pid_t		 forkpty(int *, char *, struct termios *, struct winsize *);
#endif

#ifndef HAVE_ASPRINTF
/* asprintf.c */
int	asprintf(char **, const char *, ...);
int	vasprintf(char **, const char *, va_list);
#endif

#ifndef HAVE_FGETLN
/* fgetln.c */
char   *fgetln(FILE *, size_t *);
#endif

#ifndef HAVE_GETOPT
/* getopt.c */
extern int	BSDopterr;
extern int	BSDoptind;
extern int	BSDoptopt;
extern int	BSDoptreset;
extern char    *BSDoptarg;
int	BSDgetopt(int, char *const *, const char *);
#define getopt(ac, av, o)  BSDgetopt(ac, av, o)
#define opterr             BSDopterr
#define optind             BSDoptind
#define optopt             BSDoptopt
#define optreset           BSDoptreset
#define optarg             BSDoptarg
#endif
#ifndef HAVE_STRTONUM
/* strtonum.c */
long long	 strtonum(const char *, long long, long long, const char **);
#endif

#ifndef HAVE_STRLCPY
/* strlcpy.c */
size_t	 	 strlcpy(char *, const char *, size_t);
#endif

#ifndef HAVE_STRLCAT
/* strlcat.c */
size_t	 	 strlcat(char *, const char *, size_t);
#endif

#ifndef HAVE_DAEMON
/* daemon.c */
int	 	 daemon(int, int);
#endif

#ifndef HAVE_FORKPTY
/* forkpty.c */
pid_t		 forkpty(int *, char *, struct termios *, struct winsize *);
#endif

#ifndef HAVE_ASPRINTF
/* asprintf.c */
int	asprintf(char **, const char *, ...);
int	vasprintf(char **, const char *, va_list);
#endif

#ifndef HAVE_FGETLN
/* fgetln.c */
char   *fgetln(FILE *, size_t *);
#endif

#ifndef HAVE_GETOPT
/* getopt.c */
extern int	BSDopterr;
extern int	BSDoptind;
extern int	BSDoptopt;
extern int	BSDoptreset;
extern char    *BSDoptarg;
int	BSDgetopt(int, char *const *, const char *);
#define getopt(ac, av, o)  BSDgetopt(ac, av, o)
#define opterr             BSDopterr
#define optind             BSDoptind
#define optopt             BSDoptopt
#define optreset           BSDoptreset
#define optarg             BSDoptarg
#endif
