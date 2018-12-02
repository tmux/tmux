# ===========================================================================
#      http://www.gnu.org/software/autoconf-archive/ax_with_curses.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_WITH_CURSES
#
# DESCRIPTION
#
#   This macro checks whether a SysV or X/Open-compatible Curses library is
#   present, along with the associated header file.  The NcursesW
#   (wide-character) library is searched for first, followed by Ncurses,
#   then the system-default plain Curses.  The first library found is the
#   one returned. Finding libraries will first be attempted by using
#   pkg-config, and should the pkg-config files not be available, will
#   fallback to combinations of known flags itself.
#
#   The following options are understood: --with-ncursesw, --with-ncurses,
#   --without-ncursesw, --without-ncurses.  The "--with" options force the
#   macro to use that particular library, terminating with an error if not
#   found.  The "--without" options simply skip the check for that library.
#   The effect on the search pattern is:
#
#     (no options)                           - NcursesW, Ncurses, Curses
#     --with-ncurses     --with-ncursesw     - NcursesW only [*]
#     --without-ncurses  --with-ncursesw     - NcursesW only [*]
#                        --with-ncursesw     - NcursesW only [*]
#     --with-ncurses     --without-ncursesw  - Ncurses only [*]
#     --with-ncurses                         - NcursesW, Ncurses [**]
#     --without-ncurses  --without-ncursesw  - Curses only
#                        --without-ncursesw  - Ncurses, Curses
#     --without-ncurses                      - NcursesW, Curses
#
#   [*]  If the library is not found, abort the configure script.
#
#   [**] If the second library (Ncurses) is not found, abort configure.
#
#   The following preprocessor symbols may be defined by this macro if the
#   appropriate conditions are met:
#
#     HAVE_CURSES             - if any SysV or X/Open Curses library found
#     HAVE_CURSES_ENHANCED    - if library supports X/Open Enhanced functions
#     HAVE_CURSES_COLOR       - if library supports color (enhanced functions)
#     HAVE_CURSES_OBSOLETE    - if library supports certain obsolete features
#     HAVE_NCURSESW           - if NcursesW (wide char) library is to be used
#     HAVE_NCURSES            - if the Ncurses library is to be used
#
#     HAVE_CURSES_H           - if <curses.h> is present and should be used
#     HAVE_NCURSESW_H         - if <ncursesw.h> should be used
#     HAVE_NCURSES_H          - if <ncurses.h> should be used
#     HAVE_NCURSESW_CURSES_H  - if <ncursesw/curses.h> should be used
#     HAVE_NCURSES_CURSES_H   - if <ncurses/curses.h> should be used
#
#   (These preprocessor symbols are discussed later in this document.)
#
#   The following output variables are defined by this macro; they are
#   precious and may be overridden on the ./configure command line:
#
#     CURSES_LIBS  - library to add to xxx_LDADD
#     CURSES_CFLAGS  - include paths to add to xxx_CPPFLAGS
#
#   In previous versions of this macro, the flags CURSES_LIB and
#   CURSES_CPPFLAGS were defined. These have been renamed, in keeping with
#   AX_WITH_CURSES's close bigger brother, PKG_CHECK_MODULES, which should
#   eventually supersede the use of AX_WITH_CURSES. Neither the library
#   listed in CURSES_LIBS, nor the flags in CURSES_CFLAGS are added to LIBS,
#   respectively CPPFLAGS, by default. You need to add both to the
#   appropriate xxx_LDADD/xxx_CPPFLAGS line in your Makefile.am. For
#   example:
#
#     prog_LDADD = @CURSES_LIBS@
#     prog_CPPFLAGS = @CURSES_CFLAGS@
#
#   If CURSES_LIBS is set on the configure command line (such as by running
#   "./configure CURSES_LIBS=-lmycurses"), then the only header searched for
#   is <curses.h>. If the user needs to specify an alternative path for a
#   library (such as for a non-standard NcurseW), the user should use the
#   LDFLAGS variable.
#
#   The following shell variables may be defined by this macro:
#
#     ax_cv_curses           - set to "yes" if any Curses library found
#     ax_cv_curses_enhanced  - set to "yes" if Enhanced functions present
#     ax_cv_curses_color     - set to "yes" if color functions present
#     ax_cv_curses_obsolete  - set to "yes" if obsolete features present
#
#     ax_cv_ncursesw      - set to "yes" if NcursesW library found
#     ax_cv_ncurses       - set to "yes" if Ncurses library found
#     ax_cv_plaincurses   - set to "yes" if plain Curses library found
#     ax_cv_curses_which  - set to "ncursesw", "ncurses", "plaincurses" or "no"
#
#   These variables can be used in your configure.ac to determine the level
#   of support you need from the Curses library.  For example, if you must
#   have either Ncurses or NcursesW, you could include:
#
#     AX_WITH_CURSES
#     if test "x$ax_cv_ncursesw" != xyes && test "x$ax_cv_ncurses" != xyes; then
#         AC_MSG_ERROR([requires either NcursesW or Ncurses library])
#     fi
#
#   If any Curses library will do (but one must be present and must support
#   color), you could use:
#
#     AX_WITH_CURSES
#     if test "x$ax_cv_curses" != xyes || test "x$ax_cv_curses_color" != xyes; then
#         AC_MSG_ERROR([requires an X/Open-compatible Curses library with color])
#     fi
#
#   Certain preprocessor symbols and shell variables defined by this macro
#   can be used to determine various features of the Curses library.  In
#   particular, HAVE_CURSES and ax_cv_curses are defined if the Curses
#   library found conforms to the traditional SysV and/or X/Open Base Curses
#   definition.  Any working Curses library conforms to this level.
#
#   HAVE_CURSES_ENHANCED and ax_cv_curses_enhanced are defined if the
#   library supports the X/Open Enhanced Curses definition.  In particular,
#   the wide-character types attr_t, cchar_t and wint_t, the functions
#   wattr_set() and wget_wch() and the macros WA_NORMAL and _XOPEN_CURSES
#   are checked.  The Ncurses library does NOT conform to this definition,
#   although NcursesW does.
#
#   HAVE_CURSES_COLOR and ax_cv_curses_color are defined if the library
#   supports color functions and macros such as COLOR_PAIR, A_COLOR,
#   COLOR_WHITE, COLOR_RED and init_pair().  These are NOT part of the
#   X/Open Base Curses definition, but are part of the Enhanced set of
#   functions.  The Ncurses library DOES support these functions, as does
#   NcursesW.
#
#   HAVE_CURSES_OBSOLETE and ax_cv_curses_obsolete are defined if the
#   library supports certain features present in SysV and BSD Curses but not
#   defined in the X/Open definition.  In particular, the functions
#   getattrs(), getcurx() and getmaxx() are checked.
#
#   To use the HAVE_xxx_H preprocessor symbols, insert the following into
#   your system.h (or equivalent) header file:
#
#     #if defined HAVE_NCURSESW_CURSES_H
#     #  include <ncursesw/curses.h>
#     #elif defined HAVE_NCURSESW_H
#     #  include <ncursesw.h>
#     #elif defined HAVE_NCURSES_CURSES_H
#     #  include <ncurses/curses.h>
#     #elif defined HAVE_NCURSES_H
#     #  include <ncurses.h>
#     #elif defined HAVE_CURSES_H
#     #  include <curses.h>
#     #else
#     #  error "SysV or X/Open-compatible Curses header file required"
#     #endif
#
#   For previous users of this macro: you should not need to change anything
#   in your configure.ac or Makefile.am, as the previous (serial 10)
#   semantics are still valid.  However, you should update your system.h (or
#   equivalent) header file to the fragment shown above. You are encouraged
#   also to make use of the extended functionality provided by this version
#   of AX_WITH_CURSES, as well as in the additional macros
#   AX_WITH_CURSES_PANEL, AX_WITH_CURSES_MENU and AX_WITH_CURSES_FORM.
#
# LICENSE
#
#   Copyright (c) 2009 Mark Pulford <mark@kyne.com.au>
#   Copyright (c) 2009 Damian Pietras <daper@daper.net>
#   Copyright (c) 2012 Reuben Thomas <rrt@sc3d.org>
#   Copyright (c) 2011 John Zaitseff <J.Zaitseff@zap.org.au>
#
#   This program is free software: you can redistribute it and/or modify it
#   under the terms of the GNU General Public License as published by the
#   Free Software Foundation, either version 3 of the License, or (at your
#   option) any later version.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
#   Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program. If not, see <http://www.gnu.org/licenses/>.
#
#   As a special exception, the respective Autoconf Macro's copyright owner
#   gives unlimited permission to copy, distribute and modify the configure
#   scripts that are the output of Autoconf when processing the Macro. You
#   need not follow the terms of the GNU General Public License when using
#   or distributing such scripts, even though portions of the text of the
#   Macro appear in them. The GNU General Public License (GPL) does govern
#   all other use of the material that constitutes the Autoconf Macro.
#
#   This special exception to the GPL applies to versions of the Autoconf
#   Macro released by the Autoconf Archive. When you make and distribute a
#   modified version of the Autoconf Macro, you may extend this special
#   exception to the GPL to apply to your modified version as well.

#serial 17

# internal function to factorize common code that is used by both ncurses
# and ncursesw
AC_DEFUN([_FIND_CURSES_FLAGS], [
    AC_MSG_CHECKING([for $1 via pkg-config])

    AX_REQUIRE_DEFINED([PKG_CHECK_EXISTS])
    _PKG_CONFIG([_ax_cv_$1_libs], [libs], [$1])
    _PKG_CONFIG([_ax_cv_$1_cppflags], [cflags], [$1])

    AS_IF([test "x$pkg_failed" = "xyes" || test "x$pkg_failed" = "xuntried"],[
        AC_MSG_RESULT([no])
        # No suitable .pc file found, have to find flags via fallback
        AC_CACHE_CHECK([for $1 via fallback], [ax_cv_$1], [
            AS_ECHO()
            pkg_cv__ax_cv_$1_libs="-l$1"
            pkg_cv__ax_cv_$1_cppflags="-D_GNU_SOURCE $CURSES_CFLAGS"
            LIBS="$ax_saved_LIBS $pkg_cv__ax_cv_$1_libs"
            CPPFLAGS="$ax_saved_CPPFLAGS $pkg_cv__ax_cv_$1_cppflags"

            AC_MSG_CHECKING([for initscr() with $pkg_cv__ax_cv_$1_libs])
            AC_LINK_IFELSE([AC_LANG_CALL([], [initscr])],
                [
                    AC_MSG_RESULT([yes])
                    AC_MSG_CHECKING([for nodelay() with $pkg_cv__ax_cv_$1_libs])
                    AC_LINK_IFELSE([AC_LANG_CALL([], [nodelay])],[
                        ax_cv_$1=yes
                        ],[
                        AC_MSG_RESULT([no])
                        m4_if(
                            [$1],[ncursesw],[pkg_cv__ax_cv_$1_libs="$pkg_cv__ax_cv_$1_libs -ltinfow"],
                            [$1],[ncurses],[pkg_cv__ax_cv_$1_libs="$pkg_cv__ax_cv_$1_libs -ltinfo"]
                        )
                        LIBS="$ax_saved_LIBS $pkg_cv__ax_cv_$1_libs"

                        AC_MSG_CHECKING([for nodelay() with $pkg_cv__ax_cv_$1_libs])
                        AC_LINK_IFELSE([AC_LANG_CALL([], [nodelay])],[
                            ax_cv_$1=yes
                            ],[
                            ax_cv_$1=no
                        ])
                    ])
                ],[
                    ax_cv_$1=no
            ])
        ])
        ],[
        AC_MSG_RESULT([yes])
        # Found .pc file, using its information
        LIBS="$ax_saved_LIBS $pkg_cv__ax_cv_$1_libs"
        CPPFLAGS="$ax_saved_CPPFLAGS $pkg_cv__ax_cv_$1_cppflags"
        ax_cv_$1=yes
    ])
])

AU_ALIAS([MP_WITH_CURSES], [AX_WITH_CURSES])
AC_DEFUN([AX_WITH_CURSES], [
    AC_ARG_VAR([CURSES_LIBS], [linker library for Curses, e.g. -lcurses])
    AC_ARG_VAR([CURSES_CFLAGS], [preprocessor flags for Curses, e.g. -I/usr/include/ncursesw])
    AC_ARG_WITH([ncurses], [AS_HELP_STRING([--with-ncurses],
        [force the use of Ncurses or NcursesW])],
        [], [with_ncurses=check])
    AC_ARG_WITH([ncursesw], [AS_HELP_STRING([--without-ncursesw],
        [do not use NcursesW (wide character support)])],
        [], [with_ncursesw=check])

    ax_saved_LIBS=$LIBS
    ax_saved_CPPFLAGS=$CPPFLAGS

    AS_IF([test "x$with_ncurses" = xyes || test "x$with_ncursesw" = xyes],
        [ax_with_plaincurses=no], [ax_with_plaincurses=check])

    ax_cv_curses_which=no

    # Test for NcursesW
    AS_IF([test "x$CURSES_LIBS" = x && test "x$with_ncursesw" != xno], [
        _FIND_CURSES_FLAGS([ncursesw])

        AS_IF([test "x$ax_cv_ncursesw" = xno && test "x$with_ncursesw" = xyes], [
            AC_MSG_ERROR([--with-ncursesw specified but could not find NcursesW library])
        ])

        AS_IF([test "x$ax_cv_ncursesw" = xyes], [
            ax_cv_curses=yes
            ax_cv_curses_which=ncursesw
            CURSES_LIBS="$pkg_cv__ax_cv_ncursesw_libs"
            CURSES_CFLAGS="$pkg_cv__ax_cv_ncursesw_cppflags"
            AC_DEFINE([HAVE_NCURSESW], [1], [Define to 1 if the NcursesW library is present])
            AC_DEFINE([HAVE_CURSES],   [1], [Define to 1 if a SysV or X/Open compatible Curses library is present])

            AC_CACHE_CHECK([for working ncursesw/curses.h], [ax_cv_header_ncursesw_curses_h], [
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        @%:@define _XOPEN_SOURCE_EXTENDED 1
                        @%:@include <ncursesw/curses.h>
                    ]], [[
                        chtype a = A_BOLD;
                        int b = KEY_LEFT;
                        chtype c = COLOR_PAIR(1) & A_COLOR;
                        attr_t d = WA_NORMAL;
                        cchar_t e;
                        wint_t f;
                        int g = getattrs(stdscr);
                        int h = getcurx(stdscr) + getmaxx(stdscr);
                        initscr();
                        init_pair(1, COLOR_WHITE, COLOR_RED);
                        wattr_set(stdscr, d, 0, NULL);
                        wget_wch(stdscr, &f);
                    ]])],
                    [ax_cv_header_ncursesw_curses_h=yes],
                    [ax_cv_header_ncursesw_curses_h=no])
            ])
            AS_IF([test "x$ax_cv_header_ncursesw_curses_h" = xyes], [
                ax_cv_curses_enhanced=yes
                ax_cv_curses_color=yes
                ax_cv_curses_obsolete=yes
                AC_DEFINE([HAVE_CURSES_ENHANCED],   [1], [Define to 1 if library supports X/Open Enhanced functions])
                AC_DEFINE([HAVE_CURSES_COLOR],      [1], [Define to 1 if library supports color (enhanced functions)])
                AC_DEFINE([HAVE_CURSES_OBSOLETE],   [1], [Define to 1 if library supports certain obsolete features])
                AC_DEFINE([HAVE_NCURSESW_CURSES_H], [1], [Define to 1 if <ncursesw/curses.h> is present])
            ])

            AC_CACHE_CHECK([for working ncursesw.h], [ax_cv_header_ncursesw_h], [
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        @%:@define _XOPEN_SOURCE_EXTENDED 1
                        @%:@include <ncursesw.h>
                    ]], [[
                        chtype a = A_BOLD;
                        int b = KEY_LEFT;
                        chtype c = COLOR_PAIR(1) & A_COLOR;
                        attr_t d = WA_NORMAL;
                        cchar_t e;
                        wint_t f;
                        int g = getattrs(stdscr);
                        int h = getcurx(stdscr) + getmaxx(stdscr);
                        initscr();
                        init_pair(1, COLOR_WHITE, COLOR_RED);
                        wattr_set(stdscr, d, 0, NULL);
                        wget_wch(stdscr, &f);
                    ]])],
                    [ax_cv_header_ncursesw_h=yes],
                    [ax_cv_header_ncursesw_h=no])
            ])
            AS_IF([test "x$ax_cv_header_ncursesw_h" = xyes], [
                ax_cv_curses_enhanced=yes
                ax_cv_curses_color=yes
                ax_cv_curses_obsolete=yes
                AC_DEFINE([HAVE_CURSES_ENHANCED], [1], [Define to 1 if library supports X/Open Enhanced functions])
                AC_DEFINE([HAVE_CURSES_COLOR],    [1], [Define to 1 if library supports color (enhanced functions)])
                AC_DEFINE([HAVE_CURSES_OBSOLETE], [1], [Define to 1 if library supports certain obsolete features])
                AC_DEFINE([HAVE_NCURSESW_H],      [1], [Define to 1 if <ncursesw.h> is present])
            ])

            AC_CACHE_CHECK([for working ncurses.h], [ax_cv_header_ncurses_h_with_ncursesw], [
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        @%:@define _XOPEN_SOURCE_EXTENDED 1
                        @%:@include <ncurses.h>
                    ]], [[
                        chtype a = A_BOLD;
                        int b = KEY_LEFT;
                        chtype c = COLOR_PAIR(1) & A_COLOR;
                        attr_t d = WA_NORMAL;
                        cchar_t e;
                        wint_t f;
                        int g = getattrs(stdscr);
                        int h = getcurx(stdscr) + getmaxx(stdscr);
                        initscr();
                        init_pair(1, COLOR_WHITE, COLOR_RED);
                        wattr_set(stdscr, d, 0, NULL);
                        wget_wch(stdscr, &f);
                    ]])],
                    [ax_cv_header_ncurses_h_with_ncursesw=yes],
                    [ax_cv_header_ncurses_h_with_ncursesw=no])
            ])
            AS_IF([test "x$ax_cv_header_ncurses_h_with_ncursesw" = xyes], [
                ax_cv_curses_enhanced=yes
                ax_cv_curses_color=yes
                ax_cv_curses_obsolete=yes
                AC_DEFINE([HAVE_CURSES_ENHANCED], [1], [Define to 1 if library supports X/Open Enhanced functions])
                AC_DEFINE([HAVE_CURSES_COLOR],    [1], [Define to 1 if library supports color (enhanced functions)])
                AC_DEFINE([HAVE_CURSES_OBSOLETE], [1], [Define to 1 if library supports certain obsolete features])
                AC_DEFINE([HAVE_NCURSES_H],       [1], [Define to 1 if <ncurses.h> is present])
            ])

            AS_IF([test "x$ax_cv_header_ncursesw_curses_h" = xno && test "x$ax_cv_header_ncursesw_h" = xno && test "x$ax_cv_header_ncurses_h_with_ncursesw" = xno], [
                AC_MSG_WARN([could not find a working ncursesw/curses.h, ncursesw.h or ncurses.h])
            ])
        ])
    ])
    unset pkg_cv__ax_cv_ncursesw_libs
    unset pkg_cv__ax_cv_ncursesw_cppflags

    # Test for Ncurses
    AS_IF([test "x$CURSES_LIBS" = x && test "x$with_ncurses" != xno && test "x$ax_cv_curses_which" = xno], [
        _FIND_CURSES_FLAGS([ncurses])

        AS_IF([test "x$ax_cv_ncurses" = xno && test "x$with_ncurses" = xyes], [
            AC_MSG_ERROR([--with-ncurses specified but could not find Ncurses library])
        ])

        AS_IF([test "x$ax_cv_ncurses" = xyes], [
            ax_cv_curses=yes
            ax_cv_curses_which=ncurses
            CURSES_LIBS="$pkg_cv__ax_cv_ncurses_libs"
            CURSES_CFLAGS="$pkg_cv__ax_cv_ncurses_cppflags"
            AC_DEFINE([HAVE_NCURSES], [1], [Define to 1 if the Ncurses library is present])
            AC_DEFINE([HAVE_CURSES],  [1], [Define to 1 if a SysV or X/Open compatible Curses library is present])

            AC_CACHE_CHECK([for working ncurses/curses.h], [ax_cv_header_ncurses_curses_h], [
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        @%:@include <ncurses/curses.h>
                    ]], [[
                        chtype a = A_BOLD;
                        int b = KEY_LEFT;
                        chtype c = COLOR_PAIR(1) & A_COLOR;
                        int g = getattrs(stdscr);
                        int h = getcurx(stdscr) + getmaxx(stdscr);
                        initscr();
                        init_pair(1, COLOR_WHITE, COLOR_RED);
                    ]])],
                    [ax_cv_header_ncurses_curses_h=yes],
                    [ax_cv_header_ncurses_curses_h=no])
            ])
            AS_IF([test "x$ax_cv_header_ncurses_curses_h" = xyes], [
                ax_cv_curses_color=yes
                ax_cv_curses_obsolete=yes
                AC_DEFINE([HAVE_CURSES_COLOR],     [1], [Define to 1 if library supports color (enhanced functions)])
                AC_DEFINE([HAVE_CURSES_OBSOLETE],  [1], [Define to 1 if library supports certain obsolete features])
                AC_DEFINE([HAVE_NCURSES_CURSES_H], [1], [Define to 1 if <ncurses/curses.h> is present])
            ])

            AC_CACHE_CHECK([for working ncurses.h], [ax_cv_header_ncurses_h], [
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        @%:@include <ncurses.h>
                    ]], [[
                        chtype a = A_BOLD;
                        int b = KEY_LEFT;
                        chtype c = COLOR_PAIR(1) & A_COLOR;
                        int g = getattrs(stdscr);
                        int h = getcurx(stdscr) + getmaxx(stdscr);
                        initscr();
                        init_pair(1, COLOR_WHITE, COLOR_RED);
                    ]])],
                    [ax_cv_header_ncurses_h=yes],
                    [ax_cv_header_ncurses_h=no])
            ])
            AS_IF([test "x$ax_cv_header_ncurses_h" = xyes], [
                ax_cv_curses_color=yes
                ax_cv_curses_obsolete=yes
                AC_DEFINE([HAVE_CURSES_COLOR],    [1], [Define to 1 if library supports color (enhanced functions)])
                AC_DEFINE([HAVE_CURSES_OBSOLETE], [1], [Define to 1 if library supports certain obsolete features])
                AC_DEFINE([HAVE_NCURSES_H],       [1], [Define to 1 if <ncurses.h> is present])
            ])

            AS_IF([test "x$ax_cv_header_ncurses_curses_h" = xno && test "x$ax_cv_header_ncurses_h" = xno], [
                AC_MSG_WARN([could not find a working ncurses/curses.h or ncurses.h])
            ])
        ])
    ])
    unset pkg_cv__ax_cv_ncurses_libs
    unset pkg_cv__ax_cv_ncurses_cppflags

    # Test for plain Curses (or if CURSES_LIBS was set by user)
    AS_IF([test "x$with_plaincurses" != xno && test "x$ax_cv_curses_which" = xno], [
        AS_IF([test "x$CURSES_LIBS" != x], [
            LIBS="$ax_saved_LIBS $CURSES_LIBS"
        ], [
            LIBS="$ax_saved_LIBS -lcurses"
        ])

        AC_CACHE_CHECK([for Curses library], [ax_cv_plaincurses], [
            AC_LINK_IFELSE([AC_LANG_CALL([], [initscr])],
                [ax_cv_plaincurses=yes], [ax_cv_plaincurses=no])
        ])

        AS_IF([test "x$ax_cv_plaincurses" = xyes], [
            ax_cv_curses=yes
            ax_cv_curses_which=plaincurses
            AS_IF([test "x$CURSES_LIBS" = x], [
                CURSES_LIBS="-lcurses"
            ])
            AC_DEFINE([HAVE_CURSES], [1], [Define to 1 if a SysV or X/Open compatible Curses library is present])

            # Check for base conformance (and header file)

            AC_CACHE_CHECK([for working curses.h], [ax_cv_header_curses_h], [
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        @%:@include <curses.h>
                    ]], [[
                        chtype a = A_BOLD;
                        int b = KEY_LEFT;
                        initscr();
                    ]])],
                    [ax_cv_header_curses_h=yes],
                    [ax_cv_header_curses_h=no])
            ])
            AS_IF([test "x$ax_cv_header_curses_h" = xyes], [
                AC_DEFINE([HAVE_CURSES_H], [1], [Define to 1 if <curses.h> is present])

                # Check for X/Open Enhanced conformance

                AC_CACHE_CHECK([for X/Open Enhanced Curses conformance], [ax_cv_plaincurses_enhanced], [
                    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                            @%:@define _XOPEN_SOURCE_EXTENDED 1
                            @%:@include <curses.h>
                            @%:@ifndef _XOPEN_CURSES
                            @%:@error "this Curses library is not enhanced"
                            "this Curses library is not enhanced"
                            @%:@endif
                        ]], [[
                            chtype a = A_BOLD;
                            int b = KEY_LEFT;
                            chtype c = COLOR_PAIR(1) & A_COLOR;
                            attr_t d = WA_NORMAL;
                            cchar_t e;
                            wint_t f;
                            initscr();
                            init_pair(1, COLOR_WHITE, COLOR_RED);
                            wattr_set(stdscr, d, 0, NULL);
                            wget_wch(stdscr, &f);
                        ]])],
                        [ax_cv_plaincurses_enhanced=yes],
                        [ax_cv_plaincurses_enhanced=no])
                ])
                AS_IF([test "x$ax_cv_plaincurses_enhanced" = xyes], [
                    ax_cv_curses_enhanced=yes
                    ax_cv_curses_color=yes
                    AC_DEFINE([HAVE_CURSES_ENHANCED], [1], [Define to 1 if library supports X/Open Enhanced functions])
                    AC_DEFINE([HAVE_CURSES_COLOR],    [1], [Define to 1 if library supports color (enhanced functions)])
                ])

                # Check for color functions

                AC_CACHE_CHECK([for Curses color functions], [ax_cv_plaincurses_color], [
                    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        @%:@define _XOPEN_SOURCE_EXTENDED 1
                        @%:@include <curses.h>
                        ]], [[
                            chtype a = A_BOLD;
                            int b = KEY_LEFT;
                            chtype c = COLOR_PAIR(1) & A_COLOR;
                            initscr();
                            init_pair(1, COLOR_WHITE, COLOR_RED);
                        ]])],
                        [ax_cv_plaincurses_color=yes],
                        [ax_cv_plaincurses_color=no])
                ])
                AS_IF([test "x$ax_cv_plaincurses_color" = xyes], [
                    ax_cv_curses_color=yes
                    AC_DEFINE([HAVE_CURSES_COLOR], [1], [Define to 1 if library supports color (enhanced functions)])
                ])

                # Check for obsolete functions

                AC_CACHE_CHECK([for obsolete Curses functions], [ax_cv_plaincurses_obsolete], [
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                        @%:@include <curses.h>
                    ]], [[
                        chtype a = A_BOLD;
                        int b = KEY_LEFT;
                        int g = getattrs(stdscr);
                        int h = getcurx(stdscr) + getmaxx(stdscr);
                        initscr();
                    ]])],
                    [ax_cv_plaincurses_obsolete=yes],
                    [ax_cv_plaincurses_obsolete=no])
                ])
                AS_IF([test "x$ax_cv_plaincurses_obsolete" = xyes], [
                    ax_cv_curses_obsolete=yes
                    AC_DEFINE([HAVE_CURSES_OBSOLETE], [1], [Define to 1 if library supports certain obsolete features])
                ])
            ])

            AS_IF([test "x$ax_cv_header_curses_h" = xno], [
                AC_MSG_WARN([could not find a working curses.h])
            ])
        ])
    ])

    AS_IF([test "x$ax_cv_curses"          != xyes], [ax_cv_curses=no])
    AS_IF([test "x$ax_cv_curses_enhanced" != xyes], [ax_cv_curses_enhanced=no])
    AS_IF([test "x$ax_cv_curses_color"    != xyes], [ax_cv_curses_color=no])
    AS_IF([test "x$ax_cv_curses_obsolete" != xyes], [ax_cv_curses_obsolete=no])

    LIBS=$ax_saved_LIBS
    CPPFLAGS=$ax_saved_CPPFLAGS

    unset ax_saved_LIBS
    unset ax_saved_CPPFLAGS
])dnl
