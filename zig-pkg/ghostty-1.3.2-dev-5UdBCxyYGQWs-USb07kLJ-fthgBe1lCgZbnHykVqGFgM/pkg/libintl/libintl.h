/* Message catalogs for internationalization.
   Copyright (C) 1995-2025 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#ifndef _LIBINTL_H
#define _LIBINTL_H 1

#include <locale.h>
#if (defined __APPLE__ && defined __MACH__) && 1
#include <xlocale.h>
#endif

/* The LC_MESSAGES locale category is the category used by the functions
   gettext() and dgettext().  It is specified in POSIX, but not in ANSI C.
   On systems that don't define it, use an arbitrary value instead.
   On Solaris, <locale.h> defines __LOCALE_H (or _LOCALE_H in Solaris 2.5)
   then includes <libintl.h> (i.e. this file!) and then only defines
   LC_MESSAGES.  To avoid a redefinition warning, don't define LC_MESSAGES
   in this case.  */
#if !defined LC_MESSAGES && \
    !(defined __LOCALE_H || (defined _LOCALE_H && defined __sun))
#define LC_MESSAGES 1729
#endif

/* We define an additional symbol to signal that we use the GNU
   implementation of gettext.  */
#define __USE_GNU_GETTEXT 1

/* Provide information about the supported file formats.  Returns the
   maximum minor revision number supported for a given major revision.  */
#define __GNU_GETTEXT_SUPPORTED_REVISION(major) \
  ((major) == 0 || (major) == 1 ? 1 : -1)

/* Resolve a platform specific conflict on DJGPP.  GNU gettext takes
   precedence over _conio_gettext.  */
#ifdef __DJGPP__
#undef gettext
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Version number: (major<<16) + (minor<<8) + subminor */
#define LIBINTL_VERSION 0x001800
extern int libintl_version;

/* We redirect the functions to those prefixed with "libintl_".  This is
   necessary, because some systems define gettext/textdomain/... in the C
   library (namely, Solaris 2.4 and newer, and GNU libc 2.0 and newer).
   If we used the unprefixed names, there would be cases where the
   definition in the C library would override the one in the libintl.so
   shared library.  Recall that on ELF systems, the symbols are looked
   up in the following order:
     1. in the executable,
     2. in the shared libraries specified on the link command line, in order,
     3. in the dependencies of the shared libraries specified on the link
        command line,
     4. in the dlopen()ed shared libraries, in the order in which they were
        dlopen()ed.
   The definition in the C library would override the one in libintl.so if
   either
     * -lc is given on the link command line and -lintl isn't, or
     * -lc is given on the link command line before -lintl, or
     * libintl.so is a dependency of a dlopen()ed shared library but not
       linked to the executable at link time.
   Since Solaris gettext() behaves differently than GNU gettext(), this
   would be unacceptable.

   For the redirection, three mechanisms are available:
     * _INTL_REDIRECT_ASM uses a function declaration with 'asm', that
       specifies a different symbol at the linker level than at the C level.
     * _INTL_REDIRECT_INLINE uses an inline function definition.  In C,
       we use 'static inline' to force the override.  In C++, it is better
       to use 'inline' without 'static'.  But since the override is only
       effective if the inlining happens, we need to use
       __attribute__ ((__always_inline__)), which is supported in g++ >= 3.1
       and clang.  MSVC has a similar keyword __forceinline (see
       <https://learn.microsoft.com/en-us/cpp/cpp/inline-functions-cpp>),
       but it has an effect only when optimizing is enabled, and there is no
       preprocessor macro that tells us whether optimizing is enabled.
     * _INTL_REDIRECT_MACROS uses C macros.
   The drawbacks are:
     * _INTL_REDIRECT_ASM and _INTL_REDIRECT_INLINE don't work when the
       function has an inline function definition in a system header file;
       this mostly affects mingw and MSVC.  In these cases,
       _INTL_REDIRECT_MACROS is the only mechanism that works.
     * _INTL_REDIRECT_MACROS can interfere with symbols used in structs and
       classes (especially in C++, but also in C).  For example, Qt has a class
       with an 'asprintf' member, and our '#define asprintf libintl_asprintf'
       triggers a compilation error.
     * _INTL_REDIRECT_INLINE in C mode has the effect that each function's
       address, such as &gettext, is different in each compilation unit.
 */

/* _INTL_FORCE_INLINE ensures inlining of a function, even when not
   optimizing.  */
/* Applies to: functions.  */
/* Supported by g++ >= 3.1 and clang.  Actually needed for g++ < 4.0.  */
#if (defined __GNUC__ && __GNUC__ + (__GNUC_MINOR__ >= 1) > 3) || \
    defined __clang__
#define _INTL_HAS_FORCE_INLINE
#define _INTL_FORCE_INLINE __attribute__((__always_inline__))
#else
#define _INTL_FORCE_INLINE
#endif

/* The user can define _INTL_REDIRECT_INLINE or _INTL_REDIRECT_MACROS.
   If he doesn't, we choose the method.  */
#if !(defined _INTL_REDIRECT_INLINE || defined _INTL_REDIRECT_MACROS)
#if defined __GNUC__ && __GNUC__ >= 2 &&                                   \
    !(defined __APPLE_CC__ && __APPLE_CC__ > 1) && !defined __MINGW32__ && \
    !(__GNUC__ == 2 && defined _AIX) &&                                    \
    (defined __STDC__ || defined __cplusplus)
#define _INTL_REDIRECT_ASM
#else
#if defined __cplusplus && defined _INTL_HAS_FORCE_INLINE
#define _INTL_REDIRECT_INLINE
#else
#define _INTL_REDIRECT_MACROS
#endif
#endif
#endif
/* Auxiliary macros.  */
#ifdef _INTL_REDIRECT_ASM
#define _INTL_ASM(cname) __asm__(_INTL_ASMNAME(__USER_LABEL_PREFIX__, #cname))
#define _INTL_ASMNAME(prefix, cnamestring) _INTL_STRINGIFY(prefix) cnamestring
#define _INTL_STRINGIFY(prefix) #prefix
#else
#define _INTL_ASM(cname)
#endif

/* _INTL_MAY_RETURN_STRING_ARG(n) declares that the given function may return
   its n-th argument literally.  This enables GCC to warn for example about
   printf (gettext ("foo %y")).  */
#if ((defined __GNUC__ && __GNUC__ >= 3) || defined __clang__) &&  \
    !(defined __APPLE_CC__ && __APPLE_CC__ > 1 &&                  \
      !(defined __clang__ && __clang__ && __clang_major__ >= 3) && \
      defined __cplusplus)
#define _INTL_MAY_RETURN_STRING_ARG(n) __attribute__((__format_arg__(n)))
#else
#define _INTL_MAY_RETURN_STRING_ARG(n)
#endif

/* _INTL_ATTRIBUTE_FORMAT ((ARCHETYPE, STRING-INDEX, FIRST-TO-CHECK))
   declares that the STRING-INDEXth function argument is a format string of
   style ARCHETYPE, which is one of:
     printf, gnu_printf
     scanf, gnu_scanf,
     strftime, gnu_strftime,
     strfmon,
   or the same thing prefixed and suffixed with '__'.
   If FIRST-TO-CHECK is not 0, arguments starting at FIRST-TO_CHECK
   are suitable for the format string.  */
/* Applies to: functions.  */
#if (defined __GNUC__ && __GNUC__ + (__GNUC_MINOR__ >= 7) > 2) || \
    defined __clang__
#define _INTL_ATTRIBUTE_FORMAT(spec) __attribute__((__format__ spec))
#else
#define _INTL_ATTRIBUTE_FORMAT(spec)
#endif

/* _INTL_ATTRIBUTE_SPEC_PRINTF_STANDARD
   An __attribute__ __format__ specifier for a function that takes a format
   string and arguments, where the format string directives are the ones
   standardized by ISO C99 and POSIX.  */
/* __gnu_printf__ is supported in GCC >= 4.4.  */
#if defined __GNUC__ && __GNUC__ + (__GNUC_MINOR__ >= 4) > 4
#define _INTL_ATTRIBUTE_SPEC_PRINTF_STANDARD __gnu_printf__
#else
#define _INTL_ATTRIBUTE_SPEC_PRINTF_STANDARD __printf__
#endif

/* _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD
   indicates to GCC that the function takes a format string and arguments,
   where the format string directives are the ones standardized by ISO C99
   and POSIX.  */
#define _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(formatstring_parameter, \
                                               first_argument)         \
  _INTL_ATTRIBUTE_FORMAT((_INTL_ATTRIBUTE_SPEC_PRINTF_STANDARD,        \
                          formatstring_parameter, first_argument))

/* _INTL_ARG_NONNULL ((N1, N2,...)) declares that the arguments N1, N2,...
   must not be NULL.  */
/* Applies to: functions.  */
#if (defined __GNUC__ && __GNUC__ + (__GNUC_MINOR__ >= 3) > 3) || \
    defined __clang__
#define _INTL_ARG_NONNULL(params) __attribute__((__nonnull__ params))
#else
#define _INTL_ARG_NONNULL(params)
#endif

/* Look up MSGID in the current default message catalog for the current
   LC_MESSAGES locale.  If not found, returns MSGID itself (the default
   text).  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_gettext(const char* __msgid)
    _INTL_MAY_RETURN_STRING_ARG(1);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_MAY_RETURN_STRING_ARG(1) char* gettext(const char* __msgid) {
  return libintl_gettext(__msgid);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define gettext libintl_gettext
#endif
extern char* gettext(const char* __msgid) _INTL_ASM(libintl_gettext)
    _INTL_MAY_RETURN_STRING_ARG(1);
#endif

/* Look up MSGID in the DOMAINNAME message catalog for the current
   LC_MESSAGES locale.  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_dgettext(const char* __domainname, const char* __msgid)
    _INTL_MAY_RETURN_STRING_ARG(2);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_MAY_RETURN_STRING_ARG(2) char* dgettext(const char* __domainname,
                                                  const char* __msgid) {
  return libintl_dgettext(__domainname, __msgid);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define dgettext libintl_dgettext
#endif
extern char* dgettext(const char* __domainname, const char* __msgid)
    _INTL_ASM(libintl_dgettext) _INTL_MAY_RETURN_STRING_ARG(2);
#endif

/* Look up MSGID in the DOMAINNAME message catalog for the current CATEGORY
   locale.  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_dcgettext(const char* __domainname,
                               const char* __msgid,
                               int __category) _INTL_MAY_RETURN_STRING_ARG(2);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_MAY_RETURN_STRING_ARG(2) char* dcgettext(const char* __domainname,
                                                   const char* __msgid,
                                                   int __category) {
  return libintl_dcgettext(__domainname, __msgid, __category);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define dcgettext libintl_dcgettext
#endif
extern char* dcgettext(const char* __domainname,
                       const char* __msgid,
                       int __category) _INTL_ASM(libintl_dcgettext)
    _INTL_MAY_RETURN_STRING_ARG(2);
#endif

/* Similar to 'gettext' but select the plural form corresponding to the
   number N.  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_ngettext(const char* __msgid1,
                              const char* __msgid2,
                              unsigned long int __n)
    _INTL_MAY_RETURN_STRING_ARG(1) _INTL_MAY_RETURN_STRING_ARG(2);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_MAY_RETURN_STRING_ARG(1)
        _INTL_MAY_RETURN_STRING_ARG(2) char* ngettext(const char* __msgid1,
                                                      const char* __msgid2,
                                                      unsigned long int __n) {
  return libintl_ngettext(__msgid1, __msgid2, __n);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define ngettext libintl_ngettext
#endif
extern char* ngettext(const char* __msgid1,
                      const char* __msgid2,
                      unsigned long int __n) _INTL_ASM(libintl_ngettext)
    _INTL_MAY_RETURN_STRING_ARG(1) _INTL_MAY_RETURN_STRING_ARG(2);
#endif

/* Similar to 'dgettext' but select the plural form corresponding to the
   number N.  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_dngettext(const char* __domainname,
                               const char* __msgid1,
                               const char* __msgid2,
                               unsigned long int __n)
    _INTL_MAY_RETURN_STRING_ARG(2) _INTL_MAY_RETURN_STRING_ARG(3);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_MAY_RETURN_STRING_ARG(2)
        _INTL_MAY_RETURN_STRING_ARG(3) char* dngettext(const char* __domainname,
                                                       const char* __msgid1,
                                                       const char* __msgid2,
                                                       unsigned long int __n) {
  return libintl_dngettext(__domainname, __msgid1, __msgid2, __n);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define dngettext libintl_dngettext
#endif
extern char* dngettext(const char* __domainname,
                       const char* __msgid1,
                       const char* __msgid2,
                       unsigned long int __n) _INTL_ASM(libintl_dngettext)
    _INTL_MAY_RETURN_STRING_ARG(2) _INTL_MAY_RETURN_STRING_ARG(3);
#endif

/* Similar to 'dcgettext' but select the plural form corresponding to the
   number N.  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_dcngettext(const char* __domainname,
                                const char* __msgid1,
                                const char* __msgid2,
                                unsigned long int __n,
                                int __category) _INTL_MAY_RETURN_STRING_ARG(2)
    _INTL_MAY_RETURN_STRING_ARG(3);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_MAY_RETURN_STRING_ARG(2) _INTL_MAY_RETURN_STRING_ARG(
        3) char* dcngettext(const char* __domainname,
                            const char* __msgid1,
                            const char* __msgid2,
                            unsigned long int __n,
                            int __category) {
  return libintl_dcngettext(__domainname, __msgid1, __msgid2, __n, __category);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define dcngettext libintl_dcngettext
#endif
extern char* dcngettext(const char* __domainname,
                        const char* __msgid1,
                        const char* __msgid2,
                        unsigned long int __n,
                        int __category) _INTL_ASM(libintl_dcngettext)
    _INTL_MAY_RETURN_STRING_ARG(2) _INTL_MAY_RETURN_STRING_ARG(3);
#endif

/* Set the current default message catalog to DOMAINNAME.
   If DOMAINNAME is null, return the current default.
   If DOMAINNAME is "", reset to the default of "messages".  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_textdomain(const char* __domainname);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE char*
    textdomain(const char* __domainname) {
  return libintl_textdomain(__domainname);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define textdomain libintl_textdomain
#endif
extern char* textdomain(const char* __domainname) _INTL_ASM(libintl_textdomain);
#endif

/* Specify that the DOMAINNAME message catalog will be found
   in DIRNAME rather than in the system locale data base.  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_bindtextdomain(const char* __domainname,
                                    const char* __dirname);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE char*
    bindtextdomain(const char* __domainname, const char* __dirname) {
  return libintl_bindtextdomain(__domainname, __dirname);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define bindtextdomain libintl_bindtextdomain
#endif
extern char* bindtextdomain(const char* __domainname, const char* __dirname)
    _INTL_ASM(libintl_bindtextdomain);
#endif

#if defined _WIN32 && !defined __CYGWIN__
/* Specify that the DOMAINNAME message catalog will be found
   in WDIRNAME rather than in the system locale data base.  */
#ifdef _INTL_REDIRECT_INLINE
extern wchar_t* libintl_wbindtextdomain(const char* __domainname,
                                        const wchar_t* __wdirname);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE wchar_t*
    wbindtextdomain(const char* __domainname, const wchar_t* __wdirname) {
  return libintl_wbindtextdomain(__domainname, __wdirname);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define wbindtextdomain libintl_wbindtextdomain
#endif
extern wchar_t* wbindtextdomain(const char* __domainname,
                                const wchar_t* __wdirname)
    _INTL_ASM(libintl_wbindtextdomain);
#endif
#endif

/* Specify the character encoding in which the messages from the
   DOMAINNAME message catalog will be returned.  */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_bind_textdomain_codeset(const char* __domainname,
                                             const char* __codeset);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE char*
    bind_textdomain_codeset(const char* __domainname, const char* __codeset) {
  return libintl_bind_textdomain_codeset(__domainname, __codeset);
}
#else
#ifdef _INTL_REDIRECT_MACROS
#define bind_textdomain_codeset libintl_bind_textdomain_codeset
#endif
extern char* bind_textdomain_codeset(const char* __domainname,
                                     const char* __codeset)
    _INTL_ASM(libintl_bind_textdomain_codeset);
#endif

/* Support for format strings with positions in *printf(), following the
   POSIX/XSI specification.
   Note: These replacements for the *printf() functions are visible only
   in source files that #include <libintl.h> or #include "gettext.h".
   Packages that use *printf() in source files that don't refer to _()
   or gettext() but for which the format string could be the return value
   of _() or gettext() need to add this #include.  Oh well.  */

/* Note: In C++ mode, it is not sufficient to redefine a symbol at the
   preprocessor macro level, such as
     #define sprintf libintl_sprintf
   Some programs may reference std::sprintf after including <libintl.h>.
   Therefore we must make sure that std::libintl_sprintf is defined and
   identical to ::libintl_sprintf.
   The user can define _INTL_CXX_NO_CLOBBER_STD_NAMESPACE to avoid this.
   In such cases, they will not benefit from the overrides when using
   the 'std' namespace, and they will need to do the references to the
   'std' namespace *before* including <libintl.h> or "gettext.h".  */

#if !1

#include <stddef.h>
#include <stdio.h>

/* Get va_list.  */
#if (defined __STDC__ && __STDC__) || defined __cplusplus || defined _MSC_VER
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#if !((defined fprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_fprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !(defined __MINGW32__ || defined _MSC_VER)
extern int libintl_vfprintf(FILE*, const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 3)
        _INTL_ARG_NONNULL((1, 2)) int fprintf(FILE* __stream,
                                              const char* __format,
                                              ...) {
  va_list __args;
  int __ret;
  va_start(__args, __format);
  __ret = libintl_vfprintf(__stream, __format, __args);
  va_end(__args);
  return __ret;
}
#elif !defined _INTL_NO_DEFINE_MACRO_FPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || defined _MSC_VER
#undef fprintf
#define fprintf libintl_fprintf
#endif
extern int fprintf(FILE*, const char*, ...) _INTL_ASM(libintl_fprintf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 3) _INTL_ARG_NONNULL((1, 2));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || \
     defined _MSC_VER) &&                                    \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_fprintf;
}
#endif
#endif
#endif
#if !((defined vfprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_vfprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !(defined __MINGW32__ || defined _MSC_VER)
extern int libintl_vfprintf(FILE*, const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0)
        _INTL_ARG_NONNULL((1, 2)) int vfprintf(FILE* __stream,
                                               const char* __format,
                                               va_list __args) {
  return libintl_vfprintf(__stream, __format, __args);
}
#elif !defined _INTL_NO_DEFINE_MACRO_VFPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || defined _MSC_VER
#undef vfprintf
#define vfprintf libintl_vfprintf
#endif
extern int vfprintf(FILE*, const char*, va_list) _INTL_ASM(libintl_vfprintf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || \
     defined _MSC_VER) &&                                    \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_vfprintf;
}
#endif
#endif
#endif

#if !((defined printf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_printf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !(defined __MINGW32__ || defined _MSC_VER)
extern int libintl_vprintf(const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(1, 0) _INTL_ARG_NONNULL((1));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(1, 2)
        _INTL_ARG_NONNULL((1)) int printf(const char* __format, ...) {
  va_list __args;
  int __ret;
  va_start(__args, __format);
  __ret = libintl_vprintf(__format, __args);
  va_end(__args);
  return __ret;
}
#elif !defined _INTL_NO_DEFINE_MACRO_PRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || defined _MSC_VER
#undef printf
#if defined __NetBSD__ || defined __BEOS__ || defined __CYGWIN__ || \
    defined __MINGW32__
/* Don't break __attribute__((format(printf,M,N))).
   This redefinition is only possible because the libc in NetBSD, Cygwin,
   mingw does not have a function __printf__.
   Alternatively, we could have done this redirection only when compiling with
   __GNUC__, together with a symbol redirection:
       extern int printf (const char *, ...)
              __asm__ (#__USER_LABEL_PREFIX__ "libintl_printf");
   But doing it now would introduce a binary incompatibility with already
   distributed versions of libintl on these systems.  */
#define libintl_printf __printf__
#endif
#define printf libintl_printf
#endif
extern int printf(const char*, ...) _INTL_ASM(libintl_printf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(1, 2) _INTL_ARG_NONNULL((1));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || \
     defined _MSC_VER) &&                                    \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_printf;
}
#endif
#endif
#endif
#if !((defined vprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_vprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !(defined __MINGW32__ || defined _MSC_VER)
extern int libintl_vprintf(const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(1, 0) _INTL_ARG_NONNULL((1));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(1, 0)
        _INTL_ARG_NONNULL((1)) int vprintf(const char* __format,
                                           va_list __args) {
  return libintl_vprintf(__format, __args);
}
#elif !defined _INTL_NO_DEFINE_MACRO_VPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || defined _MSC_VER
#undef vprintf
#define vprintf libintl_vprintf
#endif
extern int vprintf(const char*, va_list) _INTL_ASM(libintl_vprintf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(1, 0) _INTL_ARG_NONNULL((1));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || \
     defined _MSC_VER) &&                                    \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_vprintf;
}
#endif
#endif
#endif

#if !((defined sprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_sprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !(defined __MINGW32__ || defined _MSC_VER)
extern int libintl_vsprintf(char*, const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 3)
        _INTL_ARG_NONNULL((1, 2)) int sprintf(char* __result,
                                              const char* __format,
                                              ...) {
  va_list __args;
  int __ret;
  va_start(__args, __format);
  __ret = libintl_vsprintf(__result, __format, __args);
  va_end(__args);
  return __ret;
}
#elif !defined _INTL_NO_DEFINE_MACRO_SPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || defined _MSC_VER
#undef sprintf
#define sprintf libintl_sprintf
#endif
extern int sprintf(char*, const char*, ...) _INTL_ASM(libintl_sprintf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 3) _INTL_ARG_NONNULL((1, 2));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || \
     defined _MSC_VER) &&                                    \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_sprintf;
}
#endif
#endif
#endif
#if !((defined vsprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_vsprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !(defined __MINGW32__ || defined _MSC_VER)
extern int libintl_vsprintf(char*, const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0)
        _INTL_ARG_NONNULL((1, 2)) int vsprintf(char* __result,
                                               const char* __format,
                                               va_list __args) {
  return libintl_vsprintf(__result, __format, __args);
}
#elif !defined _INTL_NO_DEFINE_MACRO_VSPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || defined _MSC_VER
#undef vsprintf
#define vsprintf libintl_vsprintf
#endif
extern int vsprintf(char*, const char*, va_list) _INTL_ASM(libintl_vsprintf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__ || \
     defined _MSC_VER) &&                                    \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_vsprintf;
}
#endif
#endif
#endif

#if 1

#if !((defined snprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_snprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vsnprintf(char*, size_t, const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(3, 0) _INTL_ARG_NONNULL((3));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(3, 4)
        _INTL_ARG_NONNULL((3)) int snprintf(char* __result,
                                            size_t __maxlen,
                                            const char* __format,
                                            ...) {
  va_list __args;
  int __ret;
  va_start(__args, __format);
  __ret = libintl_vsnprintf(__result, __maxlen, __format, __args);
  va_end(__args);
  return __ret;
}
#elif !defined _INTL_NO_DEFINE_MACRO_SNPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef snprintf
#define snprintf libintl_snprintf
#endif
extern int snprintf(char*, size_t, const char*, ...) _INTL_ASM(libintl_snprintf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(3, 4) _INTL_ARG_NONNULL((3));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_snprintf;
}
#endif
#endif
#endif
#if !((defined vsnprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_vsnprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vsnprintf(char*, size_t, const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(3, 0) _INTL_ARG_NONNULL((3));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(3, 0)
        _INTL_ARG_NONNULL((3)) int vsnprintf(char* __result,
                                             size_t __maxlen,
                                             const char* __format,
                                             va_list __args) {
  return libintl_vsnprintf(__result, __maxlen, __format, __args);
}
#elif !defined _INTL_NO_DEFINE_MACRO_VSNPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef vsnprintf
#define vsnprintf libintl_vsnprintf
#endif
extern int vsnprintf(char*, size_t, const char*, va_list)
    _INTL_ASM(libintl_vsnprintf) _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(3, 0)
        _INTL_ARG_NONNULL((3));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_vsnprintf;
}
#endif
#endif
#endif

#endif

#if 1

#if !((defined asprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_asprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vasprintf(char**, const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 3)
        _INTL_ARG_NONNULL((1, 2)) int asprintf(char** __result,
                                               const char* __format,
                                               ...) {
  va_list __args;
  int __ret;
  va_start(__args, __format);
  __ret = libintl_vasprintf(__result, __format, __args);
  va_end(__args);
  return __ret;
}
#elif !defined _INTL_NO_DEFINE_MACRO_ASPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef asprintf
#define asprintf libintl_asprintf
#endif
extern int asprintf(char**, const char*, ...) _INTL_ASM(libintl_asprintf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 3) _INTL_ARG_NONNULL((1, 2));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_asprintf;
}
#endif
#endif
#endif
#if !((defined vasprintf && defined _GL_STDIO_H) || \
      defined GNULIB_overrides_vasprintf) /* don't override gnulib */
#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vasprintf(char**, const char*, va_list)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0)
        _INTL_ARG_NONNULL((1, 2)) int vasprintf(char** __result,
                                                const char* __format,
                                                va_list __args) {
  return libintl_vasprintf(__result, __format, __args);
}
#elif !defined _INTL_NO_DEFINE_MACRO_VASPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef vasprintf
#define vasprintf libintl_vasprintf
#endif
extern int vasprintf(char**, const char*, va_list) _INTL_ASM(libintl_vasprintf)
    _INTL_ATTRIBUTE_FORMAT_PRINTF_STANDARD(2, 0) _INTL_ARG_NONNULL((1, 2));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_vasprintf;
}
#endif
#endif
#endif

#endif

#if 1

#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vfwprintf(FILE*, const wchar_t*, va_list)
    _INTL_ARG_NONNULL((1, 2));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ARG_NONNULL((1, 2)) int fwprintf(FILE* __stream,
                                           const wchar_t* __format,
                                           ...) {
  va_list __args;
  int __ret;
  va_start(__args, __format);
  __ret = libintl_vfwprintf(__stream, __format, __args);
  va_end(__args);
  return __ret;
}
#elif !defined _INTL_NO_DEFINE_MACRO_FWPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef fwprintf
#define fwprintf libintl_fwprintf
#endif
extern int fwprintf(FILE*, const wchar_t*, ...) _INTL_ASM(libintl_fwprintf)
    _INTL_ARG_NONNULL((1, 2));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_fwprintf;
}
#endif
#endif
#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vfwprintf(FILE*, const wchar_t*, va_list)
    _INTL_ARG_NONNULL((1, 2));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ARG_NONNULL((1, 2)) int vfwprintf(FILE* __stream,
                                            const wchar_t* __format,
                                            va_list __args) {
  return libintl_vfwprintf(__stream, __format, __args);
}
#elif !defined _INTL_NO_DEFINE_MACRO_VFWPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef vfwprintf
#define vfwprintf libintl_vfwprintf
#endif
extern int vfwprintf(FILE*, const wchar_t*, va_list)
    _INTL_ASM(libintl_vfwprintf) _INTL_ARG_NONNULL((1, 2));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_vfwprintf;
}
#endif
#endif

#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vwprintf(const wchar_t*, va_list) _INTL_ARG_NONNULL((1));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ARG_NONNULL((1)) int wprintf(const wchar_t* __format, ...) {
  va_list __args;
  int __ret;
  va_start(__args, __format);
  __ret = libintl_vwprintf(__format, __args);
  va_end(__args);
  return __ret;
}
#elif !defined _INTL_NO_DEFINE_MACRO_WPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef wprintf
#define wprintf libintl_wprintf
#endif
extern int wprintf(const wchar_t*, ...) _INTL_ASM(libintl_wprintf)
    _INTL_ARG_NONNULL((1));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_wprintf;
}
#endif
#endif
#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vwprintf(const wchar_t*, va_list) _INTL_ARG_NONNULL((1));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ARG_NONNULL((1)) int vwprintf(const wchar_t* __format,
                                        va_list __args) {
  return libintl_vwprintf(__format, __args);
}
#elif !defined _INTL_NO_DEFINE_MACRO_VWPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef vwprintf
#define vwprintf libintl_vwprintf
#endif
extern int vwprintf(const wchar_t*, va_list) _INTL_ASM(libintl_vwprintf)
    _INTL_ARG_NONNULL((1));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_vwprintf;
}
#endif
#endif

#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vswprintf(wchar_t*, size_t, const wchar_t*, va_list)
    _INTL_ARG_NONNULL((1, 3));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ARG_NONNULL((1, 3)) int swprintf(wchar_t* __result,
                                           size_t __maxlen,
                                           const wchar_t* __format,
                                           ...) {
  va_list __args;
  int __ret;
  va_start(__args, __format);
  __ret = libintl_vswprintf(__result, __maxlen, __format, __args);
  va_end(__args);
  return __ret;
}
#elif !defined _INTL_NO_DEFINE_MACRO_SWPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef swprintf
#define swprintf libintl_swprintf
#endif
extern int swprintf(wchar_t*, size_t, const wchar_t*, ...)
    _INTL_ASM(libintl_swprintf) _INTL_ARG_NONNULL((1, 3));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_swprintf;
}
#endif
#endif
#if defined _INTL_REDIRECT_INLINE && !defined __MINGW32__
extern int libintl_vswprintf(wchar_t*, size_t, const wchar_t*, va_list)
    _INTL_ARG_NONNULL((1, 3));
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE
    _INTL_ARG_NONNULL((1, 3)) int vswprintf(wchar_t* __result,
                                            size_t __maxlen,
                                            const wchar_t* __format,
                                            va_list __args) {
  return libintl_vswprintf(__result, __maxlen, __format, __args);
}
#elif !defined _INTL_NO_DEFINE_MACRO_VSWPRINTF
#if defined _INTL_REDIRECT_MACROS || defined __MINGW32__
#undef vswprintf
#define vswprintf libintl_vswprintf
#endif
extern int vswprintf(wchar_t*, size_t, const wchar_t*, va_list)
    _INTL_ASM(libintl_vswprintf) _INTL_ARG_NONNULL((1, 3));
#if (defined _INTL_REDIRECT_MACROS || defined __MINGW32__) && \
    defined __cplusplus && !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_vswprintf;
}
#endif
#endif

#endif

#endif

/* Support for retrieving the name of a locale_t object.  */
#if 0

#ifndef GNULIB_defined_newlocale /* don't override gnulib */
#ifdef _INTL_REDIRECT_INLINE
extern locale_t libintl_newlocale(int, const char*, locale_t);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE locale_t
    newlocale(int __category_mask, const char* __name, locale_t __base) {
  return libintl_newlocale(__category_mask, __name, __base);
}
#elif !defined _INTL_NO_DEFINE_MACRO_NEWLOCALE
#ifdef _INTL_REDIRECT_MACROS
#undef newlocale
#define newlocale libintl_newlocale
#endif
extern locale_t newlocale(int, const char*, locale_t)
    _INTL_ASM(libintl_newlocale);
#if defined _INTL_REDIRECT_MACROS && defined __cplusplus && \
    !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_newlocale;
}
#endif
#endif
#endif

#ifndef GNULIB_defined_duplocale /* don't override gnulib */
#ifdef _INTL_REDIRECT_INLINE
extern locale_t libintl_duplocale(locale_t);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE locale_t
    duplocale(locale_t __locale) {
  return libintl_duplocale(__locale);
}
#elif !defined _INTL_NO_DEFINE_MACRO_DUPLOCALE
#ifdef _INTL_REDIRECT_MACROS
#undef duplocale
#define duplocale libintl_duplocale
#endif
extern locale_t duplocale(locale_t) _INTL_ASM(libintl_duplocale);
#if defined _INTL_REDIRECT_MACROS && defined __cplusplus && \
    !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_duplocale;
}
#endif
#endif
#endif

#ifndef GNULIB_defined_freelocale /* don't override gnulib */
#ifdef _INTL_REDIRECT_INLINE
extern void libintl_freelocale(locale_t);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE void
    freelocale(locale_t __locale) {
  libintl_freelocale(__locale);
}
#elif !defined _INTL_NO_DEFINE_MACRO_FREELOCALE
#ifdef _INTL_REDIRECT_MACROS
#undef freelocale
#define freelocale libintl_freelocale
#endif
extern void freelocale(locale_t) _INTL_ASM(libintl_freelocale);
#if defined _INTL_REDIRECT_MACROS && defined __cplusplus && \
    !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_freelocale;
}
#endif
#endif
#endif

#endif

/* Support for the locale chosen by the user.  */
#if (defined __APPLE__ && defined __MACH__) || defined _WIN32 || \
    defined __CYGWIN__

#ifndef GNULIB_defined_setlocale /* don't override gnulib */
#ifdef _INTL_REDIRECT_INLINE
extern char* libintl_setlocale(int, const char*);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE char*
    setlocale(int __category, const char* __locale) {
  return libintl_setlocale(__category, __locale);
}
#elif !defined _INTL_NO_DEFINE_MACRO_SETLOCALE
#ifdef _INTL_REDIRECT_MACROS
#undef setlocale
#define setlocale libintl_setlocale
#endif
extern char* setlocale(int, const char*) _INTL_ASM(libintl_setlocale);
#if defined _INTL_REDIRECT_MACROS && defined __cplusplus && \
    !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_setlocale;
}
#endif
#endif
#endif

#if 1

/* Declare newlocale() only if the system headers define the 'locale_t' type. */
#if !(defined __CYGWIN__ && !defined LC_ALL_MASK)
#ifdef _INTL_REDIRECT_INLINE
extern locale_t libintl_newlocale(int, const char*, locale_t);
#ifndef __cplusplus
static
#endif
    inline _INTL_FORCE_INLINE locale_t
    newlocale(int __category_mask, const char* __name, locale_t __base) {
  return libintl_newlocale(__category_mask, __name, __base);
}
#elif !defined _INTL_NO_DEFINE_MACRO_NEWLOCALE
#ifdef _INTL_REDIRECT_MACROS
#undef newlocale
#define newlocale libintl_newlocale
#endif
extern locale_t newlocale(int, const char*, locale_t)
    _INTL_ASM(libintl_newlocale);
#if defined _INTL_REDIRECT_MACROS && defined __cplusplus && \
    !defined _INTL_CXX_NO_CLOBBER_STD_NAMESPACE
namespace std {
using ::libintl_newlocale;
}
#endif
#endif
#endif

#endif

#endif

/* Support for relocatable packages.  */

/* Sets the original and the current installation prefix of the package.
   Relocation simply replaces a pathname starting with the original prefix
   by the corresponding pathname with the current prefix instead.  Both
   prefixes should be directory names without trailing slash (i.e. use ""
   instead of "/").  */
#define libintl_set_relocation_prefix libintl_set_relocation_prefix
extern void libintl_set_relocation_prefix(const char* orig_prefix,
                                          const char* curr_prefix);

#ifdef __cplusplus
}
#endif

#endif /* libintl.h */
