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

#ifndef COMPAT_H
#define COMPAT_H

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <fnmatch.h>
#include <limits.h>
#include <stdio.h>
#include <termios.h>
#include <wchar.h>

#ifdef HAVE_EVENT2_EVENT_H
#include <event2/event.h>
#include <event2/event_compat.h>
#include <event2/event_struct.h>
#include <event2/buffer.h>
#include <event2/buffer_compat.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_struct.h>
#include <event2/bufferevent_compat.h>
#else
#include <event.h>
#ifndef EVBUFFER_EOL_LF
/*
 * This doesn't really work because evbuffer_readline is broken, but gets us to
 * build with very old (older than 1.4.14) libevent.
 */
#define EVBUFFER_EOL_LF
#define evbuffer_readln(a, b, c) evbuffer_readline(a)
#endif
#endif

#ifdef HAVE_MALLOC_TRIM
#include <malloc.h>
#endif

#ifdef HAVE_UTF8PROC
#include <utf8proc.h>
#endif

#ifndef __GNUC__
#define __attribute__(a)
#endif

#ifdef BROKEN___DEAD
#undef __dead
#endif

#ifndef __unused
#define __unused __attribute__ ((__unused__))
#endif
#ifndef __dead
#define __dead __attribute__ ((__noreturn__))
#endif
#ifndef __packed
#define __packed __attribute__ ((__packed__))
#endif
#ifndef __weak
#define __weak __attribute__ ((__weak__))
#endif

#ifndef ECHOPRT
#define ECHOPRT 0
#endif

#ifndef ACCESSPERMS
#define ACCESSPERMS (S_IRWXU|S_IRWXG|S_IRWXO)
#endif

#if !defined(FIONREAD) && defined(__sun)
#include <sys/filio.h>
#endif

#ifdef HAVE_ERR_H
#include <err.h>
#else
void	err(int, const char *, ...);
void	errx(int, const char *, ...);
void	warn(const char *, ...);
void	warnx(const char *, ...);
#endif

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifndef _PATH_BSHELL
#define _PATH_BSHELL	"/bin/sh"
#endif

#ifndef _PATH_TMP
#define _PATH_TMP	"/tmp/"
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL	"/dev/null"
#endif

#ifndef _PATH_TTY
#define _PATH_TTY	"/dev/tty"
#endif

#ifndef _PATH_DEV
#define _PATH_DEV	"/dev/"
#endif

#ifndef _PATH_DEFPATH
#define _PATH_DEFPATH	"/usr/bin:/bin"
#endif

#ifndef _PATH_VI
#define _PATH_VI	"/usr/bin/vi"
#endif

#ifndef __OpenBSD__
#define pledge(s, p) (0)
#endif

#ifndef IMAXBEL
#define IMAXBEL 0
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
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

#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif

#ifdef HAVE_PTY_H
#include <pty.h>
#endif

#ifdef HAVE_UTIL_H
#include <util.h>
#endif

#ifdef HAVE_VIS
#include <vis.h>
#else
#include "compat/vis.h"
#endif

#ifdef HAVE_IMSG
#include <imsg.h>
#else
#include "compat/imsg.h"
#endif

#ifdef BROKEN_CMSG_FIRSTHDR
#undef CMSG_FIRSTHDR
#define CMSG_FIRSTHDR(mhdr) \
	((mhdr)->msg_controllen >= sizeof(struct cmsghdr) ? \
	    (struct cmsghdr *)(mhdr)->msg_control :	    \
	    (struct cmsghdr *)NULL)
#endif

#ifndef CMSG_ALIGN
#ifdef _CMSG_DATA_ALIGN
#define CMSG_ALIGN _CMSG_DATA_ALIGN
#else
#define CMSG_ALIGN(len) (((len) + sizeof(long) - 1) & ~(sizeof(long) - 1))
#endif
#endif

#ifndef CMSG_SPACE
#define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(len))
#endif

#ifndef CMSG_LEN
#define CMSG_LEN(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (len))
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

#ifndef FNM_CASEFOLD
#ifdef FNM_IGNORECASE
#define FNM_CASEFOLD FNM_IGNORECASE
#else
#define FNM_CASEFOLD 0
#endif
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

#ifndef timersub
#define timersub(tvp, uvp, vvp)                                         \
	do {                                                            \
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;          \
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;       \
		if ((vvp)->tv_usec < 0) {                               \
			(vvp)->tv_sec--;                                \
			(vvp)->tv_usec += 1000000;                      \
		}                                                       \
	} while (0)
#endif

#ifndef TTY_NAME_MAX
#define TTY_NAME_MAX 32
#endif

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC CLOCK_REALTIME
#endif

#ifndef HAVE_FLOCK
#define LOCK_SH 0
#define LOCK_EX 0
#define LOCK_NB 0
#define flock(fd, op) (0)
#endif

#ifndef HAVE_EXPLICIT_BZERO
/* explicit_bzero.c */
void		 explicit_bzero(void *, size_t);
#endif

#ifndef HAVE_GETDTABLECOUNT
/* getdtablecount.c */
int		 getdtablecount(void);
#endif

#ifndef HAVE_GETDTABLESIZE
/* getdtablesize.c */
int		 getdtablesize(void);
#endif

#ifndef HAVE_CLOSEFROM
/* closefrom.c */
void		 closefrom(int);
#endif

#ifndef HAVE_STRCASESTR
/* strcasestr.c */
char		*strcasestr(const char *, const char *);
#endif

#ifndef HAVE_STRSEP
/* strsep.c */
char		*strsep(char **, const char *);
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

#ifndef HAVE_STRNLEN
/* strnlen.c */
size_t		 strnlen(const char *, size_t);
#endif

#ifndef HAVE_STRNDUP
/* strndup.c */
char		*strndup(const char *, size_t);
#endif

#ifndef HAVE_MEMMEM
/* memmem.c */
void		*memmem(const void *, size_t, const void *, size_t);
#endif

#ifndef HAVE_HTONLL
/* htonll.c */
#undef htonll
uint64_t	 htonll(uint64_t);
#endif

#ifndef HAVE_NTOHLL
/* ntohll.c */
#undef ntohll
uint64_t	 ntohll(uint64_t);
#endif

#ifndef HAVE_GETPEEREID
/* getpeereid.c */
int		getpeereid(int, uid_t *, gid_t *);
#endif

#ifndef HAVE_DAEMON
/* daemon.c */
int	 	 daemon(int, int);
#endif

#ifndef HAVE_GETPROGNAME
/* getprogname.c */
const char	*getprogname(void);
#endif

#ifndef HAVE_SETPROCTITLE
/* setproctitle.c */
void		 setproctitle(const char *, ...);
#endif

#ifndef HAVE_CLOCK_GETTIME
/* clock_gettime.c */
int		 clock_gettime(int, struct timespec *);
#endif

#ifndef HAVE_B64_NTOP
/* base64.c */
#undef b64_ntop
#undef b64_pton
int		 b64_ntop(const char *, size_t, char *, size_t);
int		 b64_pton(const char *, u_char *, size_t);
#endif

#ifndef HAVE_FDFORKPTY
/* fdforkpty.c */
int		 getptmfd(void);
pid_t		 fdforkpty(int, int *, char *, struct termios *,
		     struct winsize *);
#endif

#ifndef HAVE_FORKPTY
/* forkpty.c */
pid_t		 forkpty(int *, char *, struct termios *, struct winsize *);
#endif

#ifndef HAVE_ASPRINTF
/* asprintf.c */
int		 asprintf(char **, const char *, ...);
int		 vasprintf(char **, const char *, va_list);
#endif

#ifndef HAVE_FGETLN
/* fgetln.c */
char		*fgetln(FILE *, size_t *);
#endif

#ifndef HAVE_GETLINE
/* getline.c */
ssize_t		 getline(char **, size_t *, FILE *);
#endif

#ifndef HAVE_SETENV
/* setenv.c */
int		 setenv(const char *, const char *, int);
int		 unsetenv(const char *);
#endif

#ifndef HAVE_CFMAKERAW
/* cfmakeraw.c */
void		 cfmakeraw(struct termios *);
#endif

#ifndef HAVE_FREEZERO
/* freezero.c */
void		 freezero(void *, size_t);
#endif

#ifndef HAVE_REALLOCARRAY
/* reallocarray.c */
void		*reallocarray(void *, size_t, size_t);
#endif

#ifndef HAVE_RECALLOCARRAY
/* recallocarray.c */
void		*recallocarray(void *, size_t, size_t, size_t);
#endif

#ifdef HAVE_SYSTEMD
/* systemd.c */
int		 systemd_activated(void);
int		 systemd_create_socket(int, char **);
int		 systemd_move_pid_to_new_cgroup(pid_t, char **);
#endif

#ifdef HAVE_UTF8PROC
/* utf8proc.c */
int		 utf8proc_wcwidth(wchar_t);
int		 utf8proc_mbtowc(wchar_t *, const char *, size_t);
int		 utf8proc_wctomb(char *, wchar_t);
#endif

#ifdef NEED_FUZZING
/* tmux.c */
#define main __weak main
#endif

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

#endif /* COMPAT_H */
