/* $XTermId: xterm_io.h,v 1.64 2017/05/18 21:15:14 tom Exp $ */

/*
 * Copyright 2000-2014,2017 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#ifndef	included_xterm_io_h
#define	included_xterm_io_h

#include <xterm.h>

/*
 * System-specific definitions (keep these chunks one-per-system!).
 *
 * FIXME:  some, such as those defining USE_TERMIOS should be moved to xterm.h
 * as they are integrated with the configure script.
 */
#if defined(__minix)
#define USE_POSIX_TERMIOS 1
#undef HAVE_POSIX_OPENPT	/* present, does not work */
#endif

#ifdef CSRG_BASED
#define USE_TERMIOS
#endif

#ifdef __CYGWIN__
#define ATT
#define SVR4
#define SYSV
#define USE_SYSV_TERMIO
#endif

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__INTERIX) || defined(__APPLE__) || defined(__UNIXWARE__) || defined(__hpux)
#ifndef USE_POSIX_TERMIOS
#define USE_POSIX_TERMIOS
#endif
#endif

#if defined(AIXV4)
#define USE_POSIX_TERMIOS
#ifndef SYSV
#define SYSV
#endif
#endif

#ifdef linux
#define USE_TERMIOS
#endif

#ifdef __SCO__
#define USE_TERMIOS
#ifndef _SVID3
#define _SVID3
#endif
#endif

#ifdef Lynx
#define USE_SYSV_TERMIO
#endif

#ifdef macII
#undef SYSV			/* pretend to be bsd (sgtty.h) */
#endif /* macII */

#if defined(__GLIBC__) && !defined(linux)
#define USE_POSIX_TERMIOS	/* GNU/Hurd, GNU/KFreeBSD and GNU/KNetBSD */
#endif

#ifdef __MVS__
#define SVR4
#define USE_POSIX_TERMIOS
#endif

#ifdef __QNX__
#define USE_POSIX_TERMIOS
#endif

#if defined(__osf__)
#define USE_POSIX_TERMIOS
#undef SYSV
#endif

/*
 * Indirect system dependencies
 */
#if defined(SVR4) && !defined(__sgi)
#define USE_TERMIOS
#endif

#ifdef SYSV
#define USE_SYSV_TERMIO
#endif

#if defined(USE_POSIX_TERMIOS) && !defined(USE_TERMIOS)
#define USE_TERMIOS
#endif

/*
 * Low-level ioctl, where it is needed or non-conflicting with termio/etc.
 */
#ifdef __QNX__
#include <ioctl.h>
#else
#include <sys/ioctl.h>
#endif

/*
 * Terminal I/O includes (termio, termios, sgtty headers).
 */
#if defined(USE_POSIX_TERMIOS) && !defined(__hpux)
#include <termios.h>
#elif defined(USE_TERMIOS)
#include <termios.h>
/* this hacked termios support only works on SYSV */
#define USE_ANY_SYSV_TERMIO
#define termio termios
#ifndef __CYGWIN__
#undef  TCGETA
#define TCGETA TCGETS
#undef  TCSETA
#define TCSETA TCSETS
#undef  TCSETAW
#define TCSETAW TCSETSW
#endif
#elif defined(USE_SYSV_TERMIO)
# define USE_ANY_SYSV_TERMIO
# ifdef Lynx
#  include <termio.h>
# else
#  include <sys/termio.h>
# endif
#elif defined(SYSV) || defined(ISC)
# include <sys/termio.h>
#elif !defined(VMS)
# include <sgtty.h>
#endif /* USE_POSIX_TERMIOS */

/*
 * Stream includes, which declare struct winsize or ttysize.
 */
#ifdef SYSV
#ifdef USE_USG_PTYS
#include <sys/stream.h>		/* get typedef used in ptem.h */
#ifdef HAVE_SYS_PTEM_H
#include <sys/ptem.h>		/* get struct winsize */
#endif
#endif /* USE_USG_PTYS */
#endif /* SYSV */

/*
 * Special cases (structures and definitions that have to be adjusted).
 */
#if defined(__CYGWIN__) && !defined(TIOCSPGRP)
#include <termios.h>
#define TIOCSPGRP (_IOW('t', 118, pid_t))
#endif

#ifdef __hpux
#include <sys/bsdtty.h>		/* defines TIOCSLTC */
#endif

#ifdef ISC
#define TIOCGPGRP TCGETPGRP
#define TIOCSPGRP TCSETPGRP
#endif

#ifdef Lynx
#include <resource.h>
#elif !(defined(SYSV) || defined(linux) || defined(VMS) || (defined(__QNX__)&&!defined(__QNXNTO__)))
#include <sys/resource.h>
#endif

#ifdef macII
#undef FIOCLEX
#undef FIONCLEX
#endif /* macII */

#if defined(__QNX__) || defined(__GNU__) || defined(__MVS__) || defined(__osf__)
#undef TIOCSLTC			/* <sgtty.h> conflicts with <termios.h> */
#undef TIOCSLTC
#endif

#if defined (__sgi) || (defined(__linux__) && defined(__sparc__)) || defined(__UNIXWARE__)
#undef TIOCLSET			/* defined, but not useable */
#endif

#if defined(sun) || defined(__UNIXWARE__)
#include <sys/filio.h>
#endif

#if defined(TIOCSLTC) && ! (defined(linux) || defined(__MVS__) || defined(Lynx) || defined(SVR4))
#define HAS_LTCHARS
#endif

#if !defined(TTYSIZE_STRUCT)
#if defined(TIOCSWINSZ)
#define USE_STRUCT_WINSIZE 1
#define TTYSIZE_STRUCT struct winsize
#define GET_TTYSIZE(fd, data) ioctl(fd, TIOCGWINSZ, (char *) &data)
#define SET_TTYSIZE(fd, data) ioctl(fd, TIOCSWINSZ, (char *) &data)
#define TTYSIZE_COLS(data) data.ws_col
#define TTYSIZE_ROWS(data) data.ws_row
#endif /* TIOCSWINSZ */
#endif /* TTYSIZE_STRUCT */

#ifndef USE_STRUCT_WINSIZE
#error "There is a configuration error with struct winsize ifdef"
#endif

/* "resize" depends upon order of assignments in this macro */
#ifdef USE_STRUCT_WINSIZE
#define setup_winsize(ts, rows, cols, height, width) \
    (ts).ws_xpixel = (ttySize_t) (width), \
    (ts).ws_ypixel = (ttySize_t) (height), \
    TTYSIZE_ROWS(ts) = (ttySize_t) (rows), \
    TTYSIZE_COLS(ts) = (ttySize_t) (cols)
#else
#define setup_winsize(ts, rows, cols, height, width) \
    TTYSIZE_ROWS(ts) = (ttySize_t) (rows), \
    TTYSIZE_COLS(ts) = (ttySize_t) (cols)
#endif

#if OPT_TRACE

#ifdef USE_STRUCT_WINSIZE
#define trace_winsize(ts, id) \
    TRACE(("%s@%d, TTYSIZE %s chars %dx%d pixels %dx%d\n", \
    	   __FILE__, __LINE__, id, \
	   TTYSIZE_ROWS(ts), TTYSIZE_COLS(ts), (ts).ws_ypixel, (ts).ws_xpixel))
#else
#define trace_winsize(ts, id) \
    TRACE(("%s@%d, TTYSIZE %s chars %dx%d\n", __FILE__, __LINE__, id, \
    	   TTYSIZE_ROWS(ts), TTYSIZE_COLS(ts)))
#endif

#define TRACE_GET_TTYSIZE(fd, id) { \
	    TTYSIZE_STRUCT debug_ttysize; \
	    if (GET_TTYSIZE(fd, debug_ttysize) == 0) \
		trace_winsize(debug_ttysize, id); \
	    else \
		TRACE(("%s@%d, TTYSIZE failed %s\n", __FILE__, __LINE__, strerror(errno))); \
	}
#else
#define trace_winsize(ts, id)	/* nothing */
#define TRACE_GET_TTYSIZE(fd, id)	/* nothing */
#endif

typedef unsigned short ttySize_t;

#endif /* included_xterm_io_h */
