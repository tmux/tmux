# configure.ac

AC_INIT(tmux, 2.6)
AC_PREREQ([2.60])

AC_CONFIG_AUX_DIR(etc)
AC_CONFIG_LIBOBJ_DIR(compat)
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CANONICAL_HOST

# When CFLAGS isn't set at this stage and gcc is detected by the macro below,
# autoconf will automatically use CFLAGS="-O2 -g". Prevent that by using an
# empty default.
: ${CFLAGS=""}

# Save user CPPFLAGS, CFLAGS and LDFLAGS. We need to change them because
# AC_CHECK_HEADER doesn't give us any other way to update the include
# paths. But for Makefile.am we want to use AM_CPPFLAGS and friends.
SAVED_CFLAGS="$CFLAGS"
SAVED_CPPFLAGS="$CPPFLAGS"
SAVED_LDFLAGS="$LDFLAGS"

# Set up the compiler in two different ways and say yes we may want to install.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_CPP
AC_PROG_EGREP
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG
AC_USE_SYSTEM_EXTENSIONS

# Default tmux.conf goes in /etc not ${prefix}/etc.
test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc

# Is this --enable-debug?
test "x$VERSION" = xmaster && enable_debug=yes
AC_ARG_ENABLE(
	debug,
	AC_HELP_STRING(--enable-debug, enable debug build flags),
)
AM_CONDITIONAL(IS_DEBUG, test "x$enable_debug" = xyes)

# Is this a static build?
AC_ARG_ENABLE(
	static,
	AC_HELP_STRING(--enable-static, create a static build)
)
if test "x$enable_static" = xyes; then
	test "x$PKG_CONFIG" != x && PKG_CONFIG="$PKG_CONFIG --static"
	AM_LDFLAGS="-static $AM_LDFLAGS"
	LDFLAGS="$AM_LDFLAGS $SAVED_LDFLAGS"
fi

# Is this gcc?
AM_CONDITIONAL(IS_GCC, test "x$GCC" = xyes)

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

# Check for various headers. Alternatives included from compat.h.
AC_CHECK_HEADERS([ \
	bitstring.h \
	dirent.h \
	fcntl.h \
	inttypes.h \
	libutil.h \
	ndir.h \
	paths.h \
	pty.h \
	stdint.h \
	sys/dir.h \
	sys/ndir.h \
	sys/tree.h \
	util.h \
])

# Look for library needed for flock.
AC_SEARCH_LIBS(flock, bsd)

# Check for functions that are replaced or omitted.
AC_CHECK_FUNCS([ \
	dirfd \
	flock \
	prctl \
	sysconf \
])

# Check for functions with a compatibility implementation.
AC_REPLACE_FUNCS([ \
	asprintf \
	cfmakeraw \
	closefrom \
	explicit_bzero \
	fgetln \
	freezero \
	getdtablecount \
	getprogname \
	memmem \
	recallocarray \
	reallocarray \
	setenv \
	setproctitle \
	strcasestr \
	strlcat \
	strlcpy \
	strndup \
	strsep \
	strtonum \
])
AC_FUNC_STRNLEN

# Look for clock_gettime. Must come before event_init.
AC_SEARCH_LIBS(clock_gettime, rt)

# Look for libevent.
PKG_CHECK_MODULES(
	LIBEVENT,
	libevent,
	[
		AM_CFLAGS="$LIBEVENT_CFLAGS $AM_CFLAGS"
		CFLAGS="$AM_CFLAGS $SAVED_CFLAGS"
		LIBS="$LIBEVENT_LIBS $LIBS"
		found_libevent=yes
	],
	[
		AC_SEARCH_LIBS(
			event_init,
			[event event-1.4 event2],
			found_libevent=yes,
			found_libevent=no
		)
	]
)
AC_CHECK_HEADER(
	event.h,
	,
	found_libevent=no
)
if test "x$found_libevent" = xno; then
	AC_MSG_ERROR("libevent not found")
fi

# Look for ncurses.
PKG_CHECK_MODULES(
	LIBTINFO,
	tinfo,
	found_ncurses=yes,
	found_ncurses=no
)
if test "x$found_ncurses" = xno; then
	PKG_CHECK_MODULES(
		LIBNCURSES,
		ncurses,
		found_ncurses=yes,
		found_ncurses=no
	)
fi
if test "x$found_ncurses" = xno; then
	PKG_CHECK_MODULES(
		LIBNCURSES,
		ncursesw,
		found_ncurses=yes,
		found_ncurses=no
	)
fi
if test "x$found_ncurses" = xyes; then
	AM_CFLAGS="$LIBNCURSES_CFLAGS $LIBTINFO_CFLAGS $AM_CFLAGS"
	CFLAGS="$LIBNCURSES_CFLAGS $LIBTINFO_CFLAGS $CFLAGS"
	LIBS="$LIBNCURSES_LIBS $LIBTINFO_LIBS $LIBS"
else
	# pkg-config didn't work, try ncurses.
	AC_CHECK_LIB(
		tinfo,
		setupterm,
		found_ncurses=yes,
		found_ncurses=no
	)
	if test "x$found_ncurses" = xno; then
		AC_CHECK_LIB(
			ncurses,
			setupterm,
			found_ncurses=yes,
			found_ncurses=no
		)
	fi
	if test "x$found_ncurses" = xyes; then
		AC_CHECK_HEADER(
			ncurses.h,
			LIBS="$LIBS -lncurses",
			found_ncurses=no)
	fi
fi
if test "x$found_ncurses" = xyes; then
	AC_DEFINE(HAVE_NCURSES_H)
else
	# No ncurses, try curses.
	AC_CHECK_LIB(
		curses,
		setupterm,
		found_curses=yes,
		found_curses=no
	)
	AC_CHECK_HEADER(
		curses.h,
		,
		found_curses=no)
	if test "x$found_curses" = xyes; then
		LIBS="$LIBS -lcurses"
		AC_DEFINE(HAVE_CURSES_H)
	else
		AC_MSG_ERROR("curses not found")
	fi
fi

# Look for utempter.
AC_ARG_ENABLE(
	utempter,
	AC_HELP_STRING(--enable-utempter, use utempter if it is installed)
)
if test "x$enable_utempter" = xyes; then
	AC_CHECK_HEADER(utempter.h, enable_utempter=yes, enable_utempter=no)
	if test "x$enable_utempter" = xyes; then
		AC_SEARCH_LIBS(
			utempter_add_record,
			utempter,
			enable_utempter=yes,
			enable_utempter=no
		)
	fi
	if test "x$enable_utempter" = xyes; then
		AC_DEFINE(HAVE_UTEMPTER)
	else
		AC_MSG_ERROR("utempter not found")
	fi
fi

# Look for utf8proc.
AC_ARG_ENABLE(
	utf8proc,
	AC_HELP_STRING(--enable-utf8proc, use utf8proc if it is installed)
)
if test "x$enable_utf8proc" = xyes; then
	AC_CHECK_HEADER(utf8proc.h, enable_utf8proc=yes, enable_utf8proc=no)
	if test "x$enable_utf8proc" = xyes; then
		AC_SEARCH_LIBS(
			utf8proc_charwidth,
			utf8proc,
			enable_utf8proc=yes,
			enable_utf8proc=no
		)
	fi
	if test "x$enable_utf8proc" = xyes; then
		AC_DEFINE(HAVE_UTF8PROC)
	else
		AC_MSG_ERROR("utf8proc not found")
	fi
fi
AM_CONDITIONAL(HAVE_UTF8PROC, [test "x$enable_utf8proc" = xyes])

# Check for b64_ntop. If we have b64_ntop, we assume b64_pton as well.
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
else
	AC_LIBOBJ(base64)
fi

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

# Look for err and friends in err.h.
AC_CHECK_FUNC(err, found_err_h=yes, found_err_h=no)
AC_CHECK_FUNC(errx, , found_err_h=no)
AC_CHECK_FUNC(warn, , found_err_h=no)
AC_CHECK_FUNC(warnx, , found_err_h=no)
if test "x$found_err_h" = xyes; then
	AC_CHECK_HEADER(err.h, , found_err_h=no)
else
	AC_LIBOBJ(err)
fi

# Look for imsg_init in libutil.
AC_SEARCH_LIBS(imsg_init, util, found_imsg_init=yes, found_imsg_init=no)
if test "x$found_imsg_init" = xyes; then
	AC_DEFINE(HAVE_IMSG)
else
	AC_LIBOBJ(imsg)
	AC_LIBOBJ(imsg-buffer)
fi

# Look for daemon, compat/daemon.c used if missing. Solaris 10 has it in
# libresolv, but no declaration anywhere, so check for declaration as well as
# function.
AC_CHECK_FUNC(daemon, found_daemon=yes, found_daemon=no)
AC_CHECK_DECL(
	daemon,
	,
	found_daemon=no,
	[
		#include <stdlib.h>
		#include <unistd.h>
	]
)
if test "x$found_daemon" = xyes; then
	AC_DEFINE(HAVE_DAEMON)
else
	AC_LIBOBJ(daemon)
fi

# Look for stravis, compat/{vis,unvis}.c used if missing.
AC_CHECK_FUNC(stravis, found_stravis=yes, found_stravis=no)
if test "x$found_stravis" = xyes; then
	AC_MSG_CHECKING(if strnvis is broken)
	AC_EGREP_HEADER([strnvis\(char \*, const char \*, size_t, int\)],
			vis.h,
			AC_MSG_RESULT(no),
			[found_stravis=no])
	if test "x$found_stravis" = xno; then
		AC_MSG_RESULT(yes)
	fi
fi
if test "x$found_stravis" = xyes; then
	AC_CHECK_DECL(
		VIS_DQ,
		,
		found_stravis=no,
		[
			#include <stdlib.h>
			#include <vis.h>
		]
)
fi
if test "x$found_stravis" = xyes; then
	AC_DEFINE(HAVE_VIS)
else
	AC_LIBOBJ(vis)
	AC_LIBOBJ(unvis)
fi

# Look for getopt. glibc's getopt does not enforce argument order and the ways
# of making it do so are stupid, so just use our own instead.
AC_CHECK_FUNC(getopt, found_getopt=yes, found_getopt=no)
if test "x$found_getopt" != xno; then
	AC_MSG_CHECKING(if getopt is suitable)
	AC_EGREP_CPP(
		yes,
		[
			#include <features.h>
			#ifdef __GLIBC__
			yes
			#endif
		],
		[
			found_getopt=no
			AC_MSG_RESULT(no)
		],
		AC_MSG_RESULT(yes))
fi
if test "x$found_getopt" != xno; then
	AC_CHECK_DECLS(
		[optarg, optind, optreset],
		,
		found_getopt=no,
		[
			#include <unistd.h>
		])
fi
if test "x$found_getopt" != xno; then
	AC_DEFINE(HAVE_GETOPT)
else
	AC_LIBOBJ(getopt)
fi

# Look for fparseln in libutil.
AC_SEARCH_LIBS(fparseln, util, found_fparseln=yes, found_fparseln=no)
if test "x$found_fparseln" = xyes; then
	AC_DEFINE(HAVE_FPARSELN)
else
	AC_LIBOBJ(fparseln)
fi

# Look for fdforkpty and forkpty in libutil.
AC_SEARCH_LIBS(fdforkpty, util, found_fdforkpty=yes, found_fdforkpty=no)
if test "x$found_fdforkpty" = xyes; then
	AC_DEFINE(HAVE_FDFORKPTY)
else
	AC_LIBOBJ(fdforkpty)
fi
AC_SEARCH_LIBS(forkpty, util, found_forkpty=yes, found_forkpty=no)
if test "x$found_forkpty" = xyes; then
	AC_DEFINE(HAVE_FORKPTY)
fi
AM_CONDITIONAL(NEED_FORKPTY, test "x$found_forkpty" = xno)

# Look for a suitable queue.h.
AC_CHECK_DECL(
	TAILQ_CONCAT,
	found_queue_h=yes,
	found_queue_h=no,
	[#include <sys/queue.h>]
)
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
AC_LINK_IFELSE([AC_LANG_SOURCE(
	[
		#include <stdio.h>
		#include <stdlib.h>
		extern char *__progname;
		int main(void) {
			const char *cp = __progname;
			printf("%s\n", cp);
			exit(0);
		}
	])],
	[AC_DEFINE(HAVE___PROGNAME) AC_MSG_RESULT(yes)],
	AC_MSG_RESULT(no)
)

# Look for program_invocation_short_name.
AC_MSG_CHECKING(for program_invocation_short_name)
AC_LINK_IFELSE([AC_LANG_SOURCE(
	[
		#include <errno.h>
		#include <stdio.h>
		#include <stdlib.h>
		int main(void) {
			const char *cp = program_invocation_short_name;
			printf("%s\n", cp);
			exit(0);
		}
	])],
	[AC_DEFINE(HAVE_PROGRAM_INVOCATION_SHORT_NAME) AC_MSG_RESULT(yes)],
	AC_MSG_RESULT(no)
)

# Look for prctl(PR_SET_NAME).
AC_CHECK_DECL(
	PR_SET_NAME,
	AC_DEFINE(HAVE_PR_SET_NAME),
	,
	[#include <sys/prctl.h>]
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

# Man page defaults to mdoc.
MANFORMAT=mdoc
AC_SUBST(MANFORMAT)

# Figure out the platform.
AC_MSG_CHECKING(platform)
case "$host_os" in
	*aix*)
		AC_MSG_RESULT(aix)
		PLATFORM=aix
		;;
	*darwin*)
		AC_MSG_RESULT(darwin)
		PLATFORM=darwin
		#
		# OS X CMSG_FIRSTHDR is broken, so redefine it with a working
		# one. daemon works but has some stupid side effects, so use
		# our internal version which has a workaround.
		#
		AC_DEFINE(BROKEN_CMSG_FIRSTHDR)
		AC_LIBOBJ(daemon)
		AC_LIBOBJ(daemon-darwin)
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
		MANFORMAT=man
		;;
	*hpux*)
		AC_MSG_RESULT(hpux)
		PLATFORM=hpux
		;;
	*cygwin*)
		AC_MSG_RESULT(cygwin)
		PLATFORM=cygwin
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

# Save our CFLAGS/CPPFLAGS/LDFLAGS for the Makefile and restore the old user
# variables.
AC_SUBST(AM_CPPFLAGS)
CPPFLAGS="$SAVED_CPPFLAGS"
AC_SUBST(AM_CFLAGS)
CFLAGS="$SAVED_CFLAGS"
AC_SUBST(AM_LDFLAGS)
LDFLAGS="$SAVED_LDFLAGS"

# autoconf should create a Makefile.
AC_OUTPUT(Makefile)
