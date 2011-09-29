# $Id$

# Miscellaneous autofoo bullshit.
AC_INIT(tmux, 1.6)

AC_CONFIG_AUX_DIR(etc)
AM_INIT_AUTOMAKE([foreign])

AC_CANONICAL_HOST

# When CFLAGS isn't set at this stage and gcc is detected by the macro below,
# autoconf will automatically use CFLAGS="-O2 -g". Prevent that by using an
# empty default.
: ${CFLAGS=""}

# Set up the compiler in two different ways and say yes we may want to install.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Check for various headers. Alternatives included from compat.h.
AC_CHECK_HEADERS(
	[ \
		bitstring.h \
		curses.h \
		dirent.h \
		fcntl.h \
		inttypes.h \
		libutil.h \
		ncurses.h \
		ndir.h \
		paths.h \
		pty.h \
		stdint.h \
		sys/dir.h \
		sys/ndir.h \
		sys/tree.h \
		term.h \
		util.h \
	]
)

# Is this a debug build?
found_debug=yes
AC_ARG_ENABLE(
	debug,
	AC_HELP_STRING(--enable-debug, create a debug build),
	found_debug=$enable_debug
)
AM_CONDITIONAL(IS_DEBUG, test "x$found_debug" = xyes)

# Is this a static build?
AC_ARG_ENABLE(
       static,
       AC_HELP_STRING(--enable-static, create a static build),
       [LDFLAGS="$LDFLAGS -static"]
)

# Is this gcc?
AM_CONDITIONAL(IS_GCC, test "x$GCC" = xyes)
AC_MSG_CHECKING(for gcc that whines about -I)
AC_EGREP_CPP(
	yes,
	[
		#if __GNUC__ > 3
		yes
		#endif
	],
	found_gcc4=yes,
	found_gcc4=no
)
AM_CONDITIONAL(IS_GCC4, test "x$found_gcc4" = xyes)
AC_MSG_RESULT($found_gcc4)

# Is this Sun CC?
AC_EGREP_CPP(
	yes,
	[
		#ifdef __SUNPRO_C
		yes
		#endif
	],
	found_suncc=yes,
	found_suncc=no
)
AM_CONDITIONAL(IS_SUNCC, test "x$found_suncc" = xyes)

# Is this glibc?
AC_MSG_CHECKING(for glibc)
AC_EGREP_CPP(
	yes,
	[
		#include <features.h>
		#ifdef __GLIBC__
		yes
		#endif
	],
	found_glibc=yes,
	found_glibc=no
)
AM_CONDITIONAL(IS_GLIBC, test "x$found_glibc" = xyes)
AC_MSG_RESULT($found_glibc)

# Look for clock_gettime. Must come before event_init.
AC_SEARCH_LIBS(clock_gettime, rt)

# Look for libevent.
AC_SEARCH_LIBS(
	event_init,
	[event event-1.4 event2],
	found_libevent=yes,
	found_libevent=no
)
if test "x$found_libevent" = xno; then
	AC_MSG_ERROR("libevent not found")
fi

# Look for curses.
AC_SEARCH_LIBS(
	setupterm,
	[terminfo curses ncurses],
	found_curses=yes,
	found_curses=no
)
if test "x$found_curses" = xno; then
    AC_MSG_ERROR("curses not found")
fi

# Check for b64_ntop.
AC_MSG_CHECKING(for b64_ntop)
AC_TRY_LINK(
	[
		#include <sys/types.h>
		#include <netinet/in.h>
		#include <resolv.h>
	],
	[b64_ntop(NULL, 0, NULL, 0);],
	found_b64_ntop=yes,
	found_b64_ntop=no
)
if test "x$found_b64_ntop" = xno; then
	AC_MSG_RESULT(no)

	AC_MSG_CHECKING(for b64_ntop with -lresolv)
	LIBS="$LIBS -lresolv"
	AC_TRY_LINK(
		[
			#include <sys/types.h>
			#include <netinet/in.h>
			#include <resolv.h>
		],
		[b64_ntop(NULL, 0, NULL, 0);],
		found_b64_ntop=yes,
		found_b64_ntop=no
	)
	if test "x$found_b64_ntop" = xno; then
		AC_MSG_RESULT(no)
	fi
fi
if test "x$found_b64_ntop" = xyes; then
	AC_DEFINE(HAVE_B64_NTOP)
	AC_MSG_RESULT(yes)
fi
AM_CONDITIONAL(NO_B64_NTOP, [test "x$found_b64_ntop" = xno])

# Look for networking libraries.
AC_SEARCH_LIBS(inet_ntoa, nsl)
AC_SEARCH_LIBS(socket, socket)
AC_CHECK_LIB(xnet, socket)

# Check for CMSG_DATA. Some platforms require _XOPEN_SOURCE_EXTENDED (for
# example see xopen_networking(7) on HP-UX).
XOPEN_DEFINES=
AC_MSG_CHECKING(for CMSG_DATA)
AC_EGREP_CPP(
	yes,
	[
		#include <sys/socket.h>
		#ifdef CMSG_DATA
		yes
		#endif
	],
	found_cmsg_data=yes,
	found_cmsg_data=no
)
AC_MSG_RESULT($found_cmsg_data)
if test "x$found_cmsg_data" = xno; then
	AC_MSG_CHECKING(if CMSG_DATA needs _XOPEN_SOURCE_EXTENDED)
	AC_EGREP_CPP(
		yes,
		[
			#define _XOPEN_SOURCE 1
			#define _XOPEN_SOURCE_EXTENDED 1
			#include <sys/socket.h>
			#ifdef CMSG_DATA
			yes
			#endif
		],
		found_cmsg_data=yes,
		found_cmsg_data=no
	)
	AC_MSG_RESULT($found_cmsg_data)
	if test "x$found_cmsg_data" = xyes; then
		XOPEN_DEFINES="-D_XOPEN_SOURCE -D_XOPEN_SOURCE_EXTENDED"
	else
    		AC_MSG_ERROR("CMSG_DATA not found")
	fi
fi
AC_SUBST(XOPEN_DEFINES)

# Look for imsg in libutil. compat/imsg.c is linked by Makefile.am if missing.
AC_SEARCH_LIBS(imsg_init, util, found_imsg_init=yes, found_imsg_init=no)
if test "x$found_imsg_init" = xyes; then
	AC_DEFINE(HAVE_IMSG)
fi
AM_CONDITIONAL(NO_IMSG, [test "x$found_imsg_init" = xno])

# Look for forkpty in libutil. compat/forkpty-*.c is linked if not found.
AC_SEARCH_LIBS(forkpty, util, found_forkpty=yes, found_forkpty=no)
if test "x$found_forkpty" = xyes; then
	AC_DEFINE(HAVE_FORKPTY)
fi
AM_CONDITIONAL(NO_FORKPTY, [test "x$found_forkpty" = xno])

# Look for closefrom, compat/closefrom.c used if missing.
AC_CHECK_FUNC(closefrom, found_closefrom=yes, found_closefrom=no)
if test "x$found_closefrom" = xyes; then
	AC_DEFINE(HAVE_CLOSEFROM)
fi
AM_CONDITIONAL(NO_CLOSEFROM, [test "x$found_closefrom" = xno])

# Look for daemon, compat/daemon.c used if missing.
AC_CHECK_FUNC(daemon, found_daemon=yes, found_daemon=no)
if test "x$found_daemon" = xyes; then
	AC_DEFINE(HAVE_DAEMON)
fi
AM_CONDITIONAL(NO_DAEMON, [test "x$found_daemon" = xno])

# Look for setenv, compat/setenv.c used if missing.
AC_CHECK_FUNC(setenv, found_setenv=yes, found_setenv=no)
if test "x$found_setenv" = xyes; then
	AC_DEFINE(HAVE_SETENV)
fi
AM_CONDITIONAL(NO_SETENV, [test "x$found_setenv" = xno])

# Look for strlcpy, compat/strlcpy.c used if missing.
AC_CHECK_FUNC(strlcpy, found_strlcpy=yes, found_strlcpy=no)
if test "x$found_strlcpy" = xyes; then
	AC_DEFINE(HAVE_STRLCPY)
fi
AM_CONDITIONAL(NO_STRLCPY, [test "x$found_strlcpy" = xno])

# Look for strlcat, compat/strlcat.c used if missing.
AC_CHECK_FUNC(strlcat, found_strlcat=yes, found_strlcat=no)
if test "x$found_strlcat" = xyes; then
	AC_DEFINE(HAVE_STRLCAT)
fi
AM_CONDITIONAL(NO_STRLCAT, [test "x$found_strlcat" = xno])

# Look for asprintf, compat/asprintf.c used if missing.
AC_CHECK_FUNC(asprintf, found_asprintf=yes, found_asprintf=no)
if test "x$found_asprintf" = xyes; then
	AC_DEFINE(HAVE_ASPRINTF)
fi
AM_CONDITIONAL(NO_ASPRINTF, [test "x$found_asprintf" = xno])

# Look for fgetln, compat/fgetln.c used if missing.
AC_CHECK_FUNC(fgetln, found_fgetln=yes, found_fgetln=no)
if test "x$found_fgetln" = xyes; then
	AC_DEFINE(HAVE_FGETLN)
fi
AM_CONDITIONAL(NO_FGETLN, [test "x$found_fgetln" = xno])

# Look for strcasestr, compat/strcasestr.c used if missing.
AC_CHECK_FUNC(strcasestr, found_strcasestr=yes, found_strcasestr=no)
if test "x$found_strcasestr" = xyes; then
	AC_DEFINE(HAVE_STRCASESTR)
fi
AM_CONDITIONAL(NO_STRCASESTR, [test "x$found_strcasestr" = xno])

# Look for strsep, compat/strsep.c used if missing.
AC_CHECK_FUNC(strsep, found_strsep=yes, found_strsep=no)
if test "x$found_strsep" = xyes; then
	AC_DEFINE(HAVE_STRSEP)
fi
AM_CONDITIONAL(NO_STRSEP, [test "x$found_strsep" = xno])

# Look for strtonum, compat/strtonum.c used if missing.
AC_CHECK_FUNC(strtonum, found_strtonum=yes, found_strtonum=no)
if test "x$found_strtonum" = xyes; then
	AC_DEFINE(HAVE_STRTONUM)
fi
AM_CONDITIONAL(NO_STRTONUM, [test "x$found_strtonum" = xno])

# Look for strnvis, compat/{vis,unvis}.c used if missing.
AC_CHECK_FUNC(strnvis, found_strnvis=yes, found_strnvis=no)
if test "x$found_strnvis" = xyes; then
	AC_DEFINE(HAVE_VIS)
fi
AM_CONDITIONAL(NO_VIS, [test "x$found_strnvis" = xno])

# Look for getopt. glibc's getopt does not enforce argument order and the ways
# of making it do so are stupid, so just use our own instead.
AC_CHECK_FUNC(getopt, found_getopt=yes, found_getopt=no)
if test "x$found_getopt" != xno; then	
	AC_CHECK_DECLS(
		[optarg, optind, optreset],
		,
		found_getopt=no,
		[
			#include <unistd.h>
		]
	)	
	if test "x$found_getopt" != xno; then	
		AC_MSG_CHECKING(if system getopt should be avoided)
		if test "x$found_glibc" = xyes; then
			found_getopt=no
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
			AC_DEFINE(HAVE_GETOPT)
		fi
	fi
fi
AM_CONDITIONAL(NO_GETOPT, [test "x$found_getopt" = xno])

# Check for some functions that are replaced or omitted.
AC_CHECK_FUNCS(
	[ \
		bzero \
		dirfd \
		setproctitle \
		sysconf \
	]
)

# Check for BSD-style integer types.
AC_MSG_CHECKING(for BSD-style unsigned types)
AC_COMPILE_IFELSE(
	[
		#include <sys/types.h>
		#ifdef HAVE_STDINT_H
		#include <stdint.h>
		#else
		#include <inttypes.h>
		#endif	
		int main(void)
		{ u_int8_t u8; u_int16_t u16; u_int32_t u32; u_int64_t u64; }
	],
	[AC_DEFINE(HAVE_BSD_TYPES) AC_MSG_RESULT(yes)],
	AC_MSG_RESULT(no)
)

# Look for a suitable queue.h.
AC_CHECK_DECL(
	TAILQ_PREV,
	found_queue_h=yes,
	found_queue_h=no,
	[#include <sys/queue.h>]
)
AC_CHECK_DECL(
	TAILQ_REPLACE,
	,
	found_queue_h=no,
	[#include <sys/queue.h>]
)
if test "x$found_queue_h" = xyes; then
	AC_DEFINE(HAVE_QUEUE_H)
fi

# Look for __progname.
AC_MSG_CHECKING(for __progname)
AC_LINK_IFELSE(
	[
		#include <stdio.h>
		#include <stdlib.h>
		extern char *__progname;
		int main(void) {
			const char *cp = __progname;
			printf("%s\n", cp);
			exit(0);
		}
	],
	[AC_DEFINE(HAVE___PROGNAME) AC_MSG_RESULT(yes)],
	AC_MSG_RESULT(no)
)

# Look for fcntl(F_CLOSEM).
AC_CHECK_DECL(
	F_CLOSEM,
	AC_DEFINE(HAVE_FCNTL_CLOSEM),
	,
	[#include <fcntl.h>]
)

# Look for /proc/$$.
AC_MSG_CHECKING(for /proc/\$\$)
if test -d /proc/$$; then
	AC_DEFINE(HAVE_PROC_PID)
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi

# Figure out the platform for osdep-*.c and forkpty-*.c.
AC_MSG_CHECKING(platform)
case "$host_os" in
	*aix*)
		AC_MSG_RESULT(aix)
		PLATFORM=aix
		;;
	*darwin*)
		AC_MSG_RESULT(darwin)
		AC_DEFINE(BROKEN_CMSG_FIRSTHDR)
		PLATFORM=darwin
		;;
	*dragonfly*)
		AC_MSG_RESULT(dragonfly)
		PLATFORM=dragonfly
		;;
	*linux*)
		AC_MSG_RESULT(linux)
		PLATFORM=linux
		;;
	*freebsd*)
		AC_MSG_RESULT(freebsd)
		PLATFORM=freebsd
		;;
	*netbsd*)
		AC_MSG_RESULT(netbsd)
		PLATFORM=netbsd
		;;
	*openbsd*)
		AC_MSG_RESULT(openbsd)
		PLATFORM=openbsd
		;;
	*sunos*)
		AC_MSG_RESULT(sunos)
		PLATFORM=sunos
		;;
	*solaris*)
		AC_MSG_RESULT(sunos)
		PLATFORM=sunos
		;;
	*hpux*)
		AC_MSG_RESULT(hpux)
		PLATFORM=hpux
		;;
	*)
		AC_MSG_RESULT(unknown)
		PLATFORM=unknown
		;;	
esac
AC_SUBST(PLATFORM)
AM_CONDITIONAL(IS_AIX, test "x$PLATFORM" = xaix)
AM_CONDITIONAL(IS_DARWIN, test "x$PLATFORM" = xdarwin)
AM_CONDITIONAL(IS_DRAGONFLY, test "x$PLATFORM" = xdragonfly)
AM_CONDITIONAL(IS_LINUX, test "x$PLATFORM" = xlinux)
AM_CONDITIONAL(IS_FREEBSD, test "x$PLATFORM" = xfreebsd)
AM_CONDITIONAL(IS_NETBSD, test "x$PLATFORM" = xnetbsd)
AM_CONDITIONAL(IS_OPENBSD, test "x$PLATFORM" = xopenbsd)
AM_CONDITIONAL(IS_SUNOS, test "x$PLATFORM" = xsunos)
AM_CONDITIONAL(IS_HPUX, test "x$PLATFORM" = xhpux)
AM_CONDITIONAL(IS_UNKNOWN, test "x$PLATFORM" = xunknown)

# autoconf should create a Makefile. A shock!
AC_OUTPUT(Makefile)
