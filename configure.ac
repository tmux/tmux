# configure.ac

AC_INIT([tmux], next-3.6)
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

# Is this oss-fuzz build?
AC_ARG_ENABLE(
	fuzzing,
	AS_HELP_STRING(--enable-fuzzing, build fuzzers)
)
AC_ARG_VAR(
	FUZZING_LIBS,
	AS_HELP_STRING(libraries to link fuzzing targets with)
)

# Set up convenient fuzzing defaults before initializing compiler.
if test "x$enable_fuzzing" = xyes; then
	AC_DEFINE(NEED_FUZZING)
	test "x$CC" = x && CC=clang
	test "x$FUZZING_LIBS" = x && \
		FUZZING_LIBS="-fsanitize=fuzzer"
	test "x$SAVED_CFLAGS" = x && \
		AM_CFLAGS="-g -fsanitize=fuzzer-no-link,address"
fi

# Set up the compiler in two different ways and say yes we may want to install.
AC_PROG_CC
AM_PROG_CC_C_O
m4_version_prereq(2.70, [AC_PROG_CC], [AC_PROG_CC_C99])
AC_PROG_CPP
AC_PROG_EGREP
AC_PROG_INSTALL
AC_PROG_YACC
PKG_PROG_PKG_CONFIG
AC_USE_SYSTEM_EXTENSIONS

# Default tmux.conf goes in /etc not ${prefix}/etc.
test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc

# Is this --enable-debug?
AC_ARG_ENABLE(
	debug,
	AS_HELP_STRING(--enable-debug, enable debug build flags),
	,
	[case "x$VERSION" in xnext*) enable_debug=yes;; esac]
)
AM_CONDITIONAL(IS_DEBUG, test "x$enable_debug" = xyes)

# Is this a static build?
AC_ARG_ENABLE(
	static,
	AS_HELP_STRING(--enable-static, create a static build)
)
if test "x$enable_static" = xyes; then
	case "$host_os" in
		*darwin*)
			AC_MSG_ERROR([static linking is not supported on macOS])
			;;
	esac
	test "x$PKG_CONFIG" != x && PKG_CONFIG="$PKG_CONFIG --static"
	AM_LDFLAGS="-static $AM_LDFLAGS"
	LDFLAGS="$AM_LDFLAGS $SAVED_LDFLAGS"
fi

# Allow default TERM to be set.
AC_ARG_WITH(
	TERM,
	AS_HELP_STRING(--with-TERM, set default TERM),
	[DEFAULT_TERM=$withval],
	[DEFAULT_TERM=]
)
case "x$DEFAULT_TERM" in
	xscreen*|xtmux*|x)
	;;
	*)
		AC_MSG_ERROR("unsuitable TERM (must be screen* or tmux*)")
	;;
esac

# Do we need fuzzers?
AM_CONDITIONAL(NEED_FUZZING, test "x$enable_fuzzing" = xyes)

# Is this gcc?
AM_CONDITIONAL(IS_GCC, test "x$GCC" = xyes -a "x$enable_fuzzing" != xyes)

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
	libproc.h \
	libutil.h \
	ndir.h \
	paths.h \
	pty.h \
	stdint.h \
	sys/dir.h \
	sys/ndir.h \
	sys/tree.h \
	ucred.h \
	util.h \
])

# Look for sys_signame.
AC_SEARCH_LIBS(sys_signame, , AC_DEFINE(HAVE_SYS_SIGNAME))

# Look for fmod.
AC_CHECK_LIB(m, fmod)

# Look for library needed for flock.
AC_SEARCH_LIBS(flock, bsd)

# Check for functions that are replaced or omitted.
AC_CHECK_FUNCS([ \
	dirfd \
	flock \
	prctl \
	proc_pidinfo \
	getpeerucred \
	sysconf
])

# Check for functions with a compatibility implementation.
AC_REPLACE_FUNCS([ \
	asprintf \
	cfmakeraw \
	clock_gettime \
	closefrom \
	explicit_bzero \
	fgetln \
	freezero \
	getdtablecount \
	getdtablesize \
	getpeereid \
	getline \
	getprogname \
	htonll \
	memmem \
	ntohll \
	setenv \
	setproctitle \
	strcasestr \
	strlcat \
	strlcpy \
	strndup \
	strsep \
])
AC_FUNC_STRNLEN

# Check if strtonum works.
AC_MSG_CHECKING([for working strtonum])
AC_RUN_IFELSE([AC_LANG_PROGRAM(
		[#include <stdlib.h>],
		[return (strtonum("0", 0, 1, NULL) == 0 ? 0 : 1);]
	)],
	[AC_DEFINE(HAVE_STRTONUM) AC_MSG_RESULT(yes)],
	[AC_LIBOBJ(strtonum) AC_MSG_RESULT(no)],
	[AC_LIBOBJ(strtonum) AC_MSG_RESULT(no)]
)

# Clang sanitizers wrap reallocarray even if it isn't available on the target
# system. When compiled it always returns NULL and crashes the program. To
# detect this we need a more complicated test.
AC_MSG_CHECKING([for working reallocarray])
AC_RUN_IFELSE([AC_LANG_PROGRAM(
		[#include <stdlib.h>],
		[return (reallocarray(NULL, 1, 1) == NULL);]
	)],
	AC_MSG_RESULT(yes),
	[AC_LIBOBJ(reallocarray) AC_MSG_RESULT([no])],
	[AC_LIBOBJ(reallocarray) AC_MSG_RESULT([no])]
)
AC_MSG_CHECKING([for working recallocarray])
AC_RUN_IFELSE([AC_LANG_PROGRAM(
		[#include <stdlib.h>],
		[return (recallocarray(NULL, 1, 1, 1) == NULL);]
	)],
	AC_MSG_RESULT(yes),
	[AC_LIBOBJ(recallocarray) AC_MSG_RESULT([no])],
	[AC_LIBOBJ(recallocarray) AC_MSG_RESULT([no])]
)

# Look for clock_gettime. Must come before event_init.
AC_SEARCH_LIBS(clock_gettime, rt)

# Always use our getopt because 1) glibc's doesn't enforce argument order 2)
# musl does not set optarg to NULL for flags without arguments (although it is
# not required to, but it is helpful) 3) there are probably other weird
# implementations.
AC_LIBOBJ(getopt)

# Look for libevent. Try libevent_core or libevent with pkg-config first then
# look for the library.
PKG_CHECK_MODULES(
	LIBEVENT_CORE,
	[libevent_core >= 2],
	[
		AM_CPPFLAGS="$LIBEVENT_CORE_CFLAGS $AM_CPPFLAGS"
		CPPFLAGS="$AM_CPPFLAGS $SAVED_CPPFLAGS"
		LIBS="$LIBEVENT_CORE_LIBS $LIBS"
		found_libevent=yes
	],
	found_libevent=no
)
if test x$found_libevent = xno; then
	PKG_CHECK_MODULES(
		LIBEVENT,
		[libevent >= 2],
		[
			AM_CPPFLAGS="$LIBEVENT_CFLAGS $AM_CPPFLAGS"
			CPPFLAGS="$AM_CPPFLAGS $SAVED_CPPFLAGS"
			LIBS="$LIBEVENT_LIBS $LIBS"
			found_libevent=yes
		],
		found_libevent=no
	)
fi
if test x$found_libevent = xno; then
	AC_SEARCH_LIBS(
		event_init,
		[event_core event event-1.4],
		found_libevent=yes,
		found_libevent=no
	)
fi
AC_CHECK_HEADER(
	event2/event.h,
	AC_DEFINE(HAVE_EVENT2_EVENT_H),
	[
		AC_CHECK_HEADER(
			event.h,
			AC_DEFINE(HAVE_EVENT_H),
			found_libevent=no
		)
	]
)
if test "x$found_libevent" = xno; then
	AC_MSG_ERROR("libevent not found")
fi

# Look for yacc.
AC_CHECK_PROG(found_yacc, $YACC, yes, no)
if test "x$found_yacc" = xno; then
	AC_MSG_ERROR("yacc not found")
fi

# Look for ncurses or curses. Try pkg-config first then directly for the
# library.
PKG_CHECK_MODULES(
	LIBTINFO,
	tinfo,
	[
		AM_CPPFLAGS="$LIBTINFO_CFLAGS $AM_CPPFLAGS"
		CPPFLAGS="$LIBTINFO_CFLAGS $SAVED_CPPFLAGS"
		LIBS="$LIBTINFO_LIBS $LIBS"
		found_ncurses=yes
	],
	found_ncurses=no
)
if test "x$found_ncurses" = xno; then
	PKG_CHECK_MODULES(
		LIBNCURSES,
		ncurses,
		[
			AM_CPPFLAGS="$LIBNCURSES_CFLAGS $AM_CPPFLAGS"
			CPPFLAGS="$LIBNCURSES_CFLAGS $SAVED_CPPFLAGS"
			LIBS="$LIBNCURSES_LIBS $LIBS"
			found_ncurses=yes
		],
		found_ncurses=no
	)
fi
if test "x$found_ncurses" = xno; then
	PKG_CHECK_MODULES(
		LIBNCURSESW,
		ncursesw,
		[
			AM_CPPFLAGS="$LIBNCURSESW_CFLAGS $AM_CPPFLAGS"
			CPPFLAGS="$LIBNCURSESW_CFLAGS $SAVED_CPPFLAGS"
			LIBS="$LIBNCURSESW_LIBS $LIBS"
			found_ncurses=yes
		],
		found_ncurses=no
	)
fi
if test "x$found_ncurses" = xno; then
	AC_SEARCH_LIBS(
		setupterm,
		[tinfo terminfo ncurses ncursesw],
		found_ncurses=yes,
		found_ncurses=no
	)
	if test "x$found_ncurses" = xyes; then
		AC_CHECK_HEADER(
			ncurses.h,
			LIBS="$LIBS -lncurses",
			found_ncurses=no
		)
	fi
fi
if test "x$found_ncurses" = xyes; then
	CPPFLAGS="$CPPFLAGS -DHAVE_NCURSES_H"
	AC_DEFINE(HAVE_NCURSES_H)
else
	AC_CHECK_LIB(
		curses,
		setupterm,
		found_curses=yes,
		found_curses=no
	)
	AC_CHECK_HEADER(
		curses.h,
		,
		found_curses=no
	)
	if test "x$found_curses" = xyes; then
		LIBS="$LIBS -lcurses"
		CPPFLAGS="$CPPFLAGS -DHAVE_CURSES_H"
		AC_DEFINE(HAVE_CURSES_H)
	else
		AC_MSG_ERROR("curses not found")
	fi
fi
AC_CHECK_FUNCS([ \
	tiparm \
	tiparm_s \
])

# Look for utempter.
AC_ARG_ENABLE(
	utempter,
	AS_HELP_STRING(--enable-utempter, use utempter if it is installed)
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
	AS_HELP_STRING(--enable-utf8proc, use utf8proc if it is installed)
)
if test "x$enable_utf8proc" = xyes; then
	PKG_CHECK_MODULES(
		LIBUTF8PROC,
		libutf8proc,
		[
			AM_CPPFLAGS="$LIBUTF8PROC_CFLAGS $AM_CPPFLAGS"
			CPPFLAGS="$LIBUTF8PROC_CFLAGS $SAVED_CPPFLAGS"
			LIBS="$LIBUTF8PROC_LIBS $LIBS"
		]
	)
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

# Check for systemd support.
AC_ARG_ENABLE(
	systemd,
	AS_HELP_STRING(--enable-systemd, enable systemd integration)
)
if test x"$enable_systemd" = xyes; then
	PKG_CHECK_MODULES(
		SYSTEMD,
		libsystemd,
		[
			AM_CPPFLAGS="$SYSTEMD_CFLAGS $AM_CPPFLAGS"
			CPPFLAGS="$AM_CPPFLAGS $SAVED_CPPFLAGS"
			LIBS="$SYSTEMD_LIBS $LIBS"
			found_systemd=yes
		],
		found_systemd=no
	)
	if test "x$found_systemd" = xyes; then
		AC_DEFINE(HAVE_SYSTEMD)
	else
		AC_MSG_ERROR("systemd not found")
	fi
fi
AM_CONDITIONAL(HAVE_SYSTEMD, [test "x$found_systemd" = xyes])
AC_ARG_ENABLE(
	cgroups,
	AS_HELP_STRING(--disable-cgroups, disable adding panes to new cgroups with systemd)
)
if test "x$enable_cgroups" = x; then
	# Default to the same as $enable_systemd.
	enable_cgroups=$enable_systemd
fi
if test "x$enable_cgroups" = xyes; then
	if test "x$found_systemd" = xyes; then
		AC_DEFINE(ENABLE_CGROUPS)
	else
		AC_MSG_ERROR("cgroups requires systemd to be enabled")
	fi
fi

# Enable sixel support.
AC_ARG_ENABLE(
	sixel,
	AS_HELP_STRING(--enable-sixel, enable sixel images)
)
if test "x$enable_sixel" = xyes; then
	AC_DEFINE(ENABLE_SIXEL)
fi
AM_CONDITIONAL(ENABLE_SIXEL, [test "x$enable_sixel" = xyes])

# Check for b64_ntop. If we have b64_ntop, we assume b64_pton as well.
AC_MSG_CHECKING(for b64_ntop)
AC_LINK_IFELSE([AC_LANG_PROGRAM(
	[
		#include <sys/types.h>
		#include <netinet/in.h>
		#include <resolv.h>
	],
	[
		b64_ntop(NULL, 0, NULL, 0);
	])],
	found_b64_ntop=yes,
	found_b64_ntop=no
)
AC_MSG_RESULT($found_b64_ntop)
OLD_LIBS="$LIBS"
if test "x$found_b64_ntop" = xno; then
	AC_MSG_CHECKING(for b64_ntop with -lresolv)
	LIBS="$OLD_LIBS -lresolv"
	AC_LINK_IFELSE([AC_LANG_PROGRAM(
		[
			#include <sys/types.h>
			#include <netinet/in.h>
			#include <resolv.h>
		],
		[
			b64_ntop(NULL, 0, NULL, 0);
		])],
		found_b64_ntop=yes,
		found_b64_ntop=no
	)
	AC_MSG_RESULT($found_b64_ntop)
fi
if test "x$found_b64_ntop" = xno; then
	AC_MSG_CHECKING(for b64_ntop with -lnetwork)
	LIBS="$OLD_LIBS -lnetwork"
	AC_LINK_IFELSE([AC_LANG_PROGRAM(
		[
			#include <sys/types.h>
			#include <netinet/in.h>
			#include <resolv.h>
		],
		[
			b64_ntop(NULL, 0, NULL, 0);
		])],
		found_b64_ntop=yes,
		found_b64_ntop=no
	)
	AC_MSG_RESULT($found_b64_ntop)
fi
if test "x$found_b64_ntop" = xyes; then
	AC_DEFINE(HAVE_B64_NTOP)
else
	LIBS="$OLD_LIBS"
	AC_LIBOBJ(base64)
fi

# Look for networking libraries.
AC_SEARCH_LIBS(inet_ntoa, nsl)
AC_SEARCH_LIBS(socket, socket)
AC_CHECK_LIB(xnet, socket)

# Check if using glibc and have malloc_trim(3). The glibc free(3) is pretty bad
# about returning memory to the kernel unless the application tells it when to
# with malloc_trim(3).
AC_MSG_CHECKING(if free doesn't work very well)
AC_LINK_IFELSE([AC_LANG_SOURCE(
	[
		#include <stdlib.h>
		#ifdef __GLIBC__
		#include <malloc.h>
		int main(void) {
			malloc_trim (0);
			exit(0);
		}
		#else
		no
		#endif
	])],
	found_malloc_trim=yes,
	found_malloc_trim=no
)
AC_MSG_RESULT($found_malloc_trim)
if test "x$found_malloc_trim" = xyes; then
	AC_DEFINE(HAVE_MALLOC_TRIM)
fi

# Build against jemalloc if requested.
AC_ARG_ENABLE(
	jemalloc,
	AS_HELP_STRING(--enable-jemalloc, use jemalloc if it is installed)
)
if test "x$enable_jemalloc" = xyes; then
	PKG_CHECK_MODULES(
		JEMALLOC,
		jemalloc,
		[
			AM_CPPFLAGS="$JEMALLOC_CFLAGS $AM_CPPFLAGS"
			CPPFLAGS="$AM_CPPFLAGS $SAVED_CPPFLAGS"
			LIBS="$LIBS $JEMALLOC_LIBS"
		],
		AC_MSG_ERROR("jemalloc not found")
	)
fi

# Check for CMSG_DATA. On some platforms like HP-UX this requires UNIX 95
# (_XOPEN_SOURCE and _XOPEN_SOURCE_EXTENDED) (see xopen_networking(7)). On
# others, UNIX 03 (_XOPEN_SOURCE 600, see standards(7) on Solaris).
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
	fi
fi
if test "x$found_cmsg_data" = xno; then
	AC_MSG_CHECKING(if CMSG_DATA needs _XOPEN_SOURCE 600)
	AC_EGREP_CPP(
		yes,
		[
			#define _XOPEN_SOURCE 600
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
		XOPEN_DEFINES="-D_XOPEN_SOURCE=600"
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

# Look for kinfo_getfile in libutil.
AC_SEARCH_LIBS(kinfo_getfile, [util util-freebsd])

# Look for a suitable queue.h.
AC_CHECK_DECL(
	TAILQ_CONCAT,
	found_queue_h=yes,
	found_queue_h=no,
	[#include <sys/queue.h>]
)
AC_CHECK_DECL(
	TAILQ_PREV,
	,
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

# Look for setsockopt(SO_PEERCRED).
AC_CHECK_DECL(
	SO_PEERCRED,
	AC_DEFINE(HAVE_SO_PEERCRED),
	,
	[#include <sys/socket.h>]
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

# Try to figure out what the best value for TERM might be.
if test "x$DEFAULT_TERM" = x; then
	DEFAULT_TERM=screen
	AC_MSG_CHECKING(TERM)
	AC_RUN_IFELSE([AC_LANG_SOURCE(
		[
			#include <stdio.h>
			#include <stdlib.h>
			#if defined(HAVE_CURSES_H)
			#include <curses.h>
			#elif defined(HAVE_NCURSES_H)
			#include <ncurses.h>
			#endif
			#include <term.h>
			int main(void) {
				if (setupterm("screen-256color", -1, NULL) != OK)
					exit(1);
				exit(0);
			}
		 ])],
		 [DEFAULT_TERM=screen-256color],
		 ,
		 [DEFAULT_TERM=screen]
	)
	AC_RUN_IFELSE([AC_LANG_SOURCE(
		[
			#include <stdio.h>
			#include <stdlib.h>
			#if defined(HAVE_CURSES_H)
			#include <curses.h>
			#elif defined(HAVE_NCURSES_H)
			#include <ncurses.h>
			#endif
			#include <term.h>
			int main(void) {
				if (setupterm("tmux", -1, NULL) != OK)
					exit(1);
				exit(0);
			}
		 ])],
		 [DEFAULT_TERM=tmux],
		 ,
		 [DEFAULT_TERM=screen]
	)
	AC_RUN_IFELSE([AC_LANG_SOURCE(
		[
			#include <stdio.h>
			#include <stdlib.h>
			#if defined(HAVE_CURSES_H)
			#include <curses.h>
			#elif defined(HAVE_NCURSES_H)
			#include <ncurses.h>
			#endif
			#include <term.h>
			int main(void) {
				if (setupterm("tmux-256color", -1, NULL) != OK)
					exit(1);
				exit(0);
			}
		 ])],
		 [DEFAULT_TERM=tmux-256color],
		 ,
		 [DEFAULT_TERM=screen]
	)
	AC_MSG_RESULT($DEFAULT_TERM)
fi
AC_SUBST(DEFAULT_TERM)

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
		# macOS uses __dead2 instead of __dead, like FreeBSD. But it defines
		# __dead away so it needs to be removed before we can replace it.
		#
		AC_DEFINE(BROKEN___DEAD)
		#
		# macOS CMSG_FIRSTHDR is broken, so redefine it with a working one.
		# daemon works but has some stupid side effects, so use our internal
		# version which has a workaround.
		#
		AC_DEFINE(BROKEN_CMSG_FIRSTHDR)
		AC_LIBOBJ(daemon)
		AC_LIBOBJ(daemon-darwin)
		#
		# macOS wcwidth(3) is bad, so complain and suggest using utf8proc
		# instead.
		#
		if test "x$enable_utf8proc" = x; then
			AC_MSG_NOTICE([])
			AC_MSG_NOTICE([	   macOS library support for Unicode is very poor,])
			AC_MSG_NOTICE([	   particularly for complex codepoints like emojis;])
			AC_MSG_NOTICE([	   to use these correctly, configuring with])
			AC_MSG_NOTICE([	   --enable-utf8proc is recommended. To build])
			AC_MSG_NOTICE([	   without anyway, use --disable-utf8proc])
			AC_MSG_NOTICE([])
			AC_MSG_ERROR([must give --enable-utf8proc or --disable-utf8proc])
		fi
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
		case `/usr/bin/nroff --version 2>&1` in
			*GNU*)
				# Solaris 11.4 and later use GNU groff.
				MANFORMAT=mdoc
				;;
			*)
				if test `uname -o 2>/dev/null` = illumos; then
					# Illumos uses mandoc.
					MANFORMAT=mdoc
				else
					# Solaris 2.0 to 11.3 use AT&T nroff.
					MANFORMAT=man
				fi
				;;
		esac
		;;
	*hpux*)
		AC_MSG_RESULT(hpux)
		PLATFORM=hpux
		;;
	*cygwin*|*msys*)
		AC_MSG_RESULT(cygwin)
		PLATFORM=cygwin
		;;
	*haiku*)
		AC_MSG_RESULT(haiku)
		PLATFORM=haiku
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
AM_CONDITIONAL(IS_CYGWIN, test "x$PLATFORM" = xcygwin)
AM_CONDITIONAL(IS_HAIKU, test "x$PLATFORM" = xhaiku)
AM_CONDITIONAL(IS_UNKNOWN, test "x$PLATFORM" = xunknown)

# Set the default lock command
DEFAULT_LOCK_CMD="lock -np"
AC_MSG_CHECKING(lock-command)
if test "x$PLATFORM" = xlinux; then
	AC_CHECK_PROG(found_vlock, vlock, yes, no)
	if test "x$found_vlock" = xyes; then
		DEFAULT_LOCK_CMD="vlock"
	fi
fi
AC_MSG_RESULT($DEFAULT_LOCK_CMD)
AC_SUBST(DEFAULT_LOCK_CMD)


# Save our CFLAGS/CPPFLAGS/LDFLAGS for the Makefile and restore the old user
# variables.
AC_SUBST(AM_CPPFLAGS)
CPPFLAGS="$SAVED_CPPFLAGS"
AC_SUBST(AM_CFLAGS)
CFLAGS="$SAVED_CFLAGS"
AC_SUBST(AM_LDFLAGS)
LDFLAGS="$SAVED_LDFLAGS"

# autoconf should create a Makefile.
AC_CONFIG_FILES(Makefile)
AC_OUTPUT
