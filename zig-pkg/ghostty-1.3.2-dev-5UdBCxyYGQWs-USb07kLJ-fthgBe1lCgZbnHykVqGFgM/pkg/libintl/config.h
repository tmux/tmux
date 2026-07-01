/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Witness that <config.h> has been included.  */
#define _GL_CONFIG_H_INCLUDED 1

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define if no multithread safety and no multithreading is desired. */
/* #undef AVOID_ANY_THREADS */

/* Define to the number of bits in type 'ptrdiff_t'. */
/* #undef BITSIZEOF_PTRDIFF_T */

/* Define to the number of bits in type 'sig_atomic_t'. */
/* #undef BITSIZEOF_SIG_ATOMIC_T */

/* Define to the number of bits in type 'size_t'. */
/* #undef BITSIZEOF_SIZE_T */

/* Define to the number of bits in type 'wchar_t'. */
/* #undef BITSIZEOF_WCHAR_T */

/* Define to the number of bits in type 'wint_t'. */
/* #undef BITSIZEOF_WINT_T */

/* Define if you wish *printf() functions that have a safe handling of
   non-IEEE-754 'long double' values. */
#define CHECK_PRINTF_SAFE 1

/* Define to 1 if using 'alloca.c'. */
/* #undef C_ALLOCA */

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'double'. */
#define DBL_EXPBIT0_BIT 20

/* Define as the word index where to find the exponent of 'double'. */
#define DBL_EXPBIT0_WORD 1

/* Define as the bit index in the word where to find the sign of 'double'. */
/* #undef DBL_SIGNBIT_BIT */

/* Define as the word index where to find the sign of 'double'. */
/* #undef DBL_SIGNBIT_WORD */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#define ENABLE_NLS 1

/* Define to nothing if C supports flexible array members, and to 1 if it does
   not. That way, with a declaration like 'struct s { int n; short
   d[FLEXIBLE_ARRAY_MEMBER]; };', the struct hack can be used with pre-C99
   compilers. Use 'FLEXSIZEOF (struct s, d, N * sizeof (short))' to calculate
   the size in bytes of such a struct containing an N-element array. */
#define FLEXIBLE_ARRAY_MEMBER /**/

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'float'. */
#define FLT_EXPBIT0_BIT 23

/* Define as the word index where to find the exponent of 'float'. */
#define FLT_EXPBIT0_WORD 0

/* Define as the bit index in the word where to find the sign of 'float'. */
/* #undef FLT_SIGNBIT_BIT */

/* Define as the word index where to find the sign of 'float'. */
/* #undef FLT_SIGNBIT_WORD */

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fscanf shall be considered present. */
#define GNULIB_FSCANF 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module lock shall be considered present. */
#define GNULIB_LOCK 1

/* Define to 1 if printf and friends should be labeled with attribute
   "__gnu_printf__" instead of "__printf__" */
/* #undef GNULIB_PRINTF_ATTRIBUTE_FLAVOR_GNU */

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module scanf shall be considered present. */
#define GNULIB_SCANF 1

/* Define to 1 when the gnulib module fgetc should be tested. */
#define GNULIB_TEST_FGETC 1

/* Define to 1 when the gnulib module fgets should be tested. */
#define GNULIB_TEST_FGETS 1

/* Define to 1 when the gnulib module fprintf should be tested. */
#define GNULIB_TEST_FPRINTF 1

/* Define to 1 when the gnulib module fputc should be tested. */
#define GNULIB_TEST_FPUTC 1

/* Define to 1 when the gnulib module fputs should be tested. */
#define GNULIB_TEST_FPUTS 1

/* Define to 1 when the gnulib module fread should be tested. */
#define GNULIB_TEST_FREAD 1

/* Define to 1 when the gnulib module free-posix should be tested. */
#define GNULIB_TEST_FREE_POSIX 1

/* Define to 1 when the gnulib module frexp should be tested. */
#define GNULIB_TEST_FREXP 1

/* Define to 1 when the gnulib module frexpl should be tested. */
#define GNULIB_TEST_FREXPL 1

/* Define to 1 when the gnulib module fscanf should be tested. */
#define GNULIB_TEST_FSCANF 1

/* Define to 1 when the gnulib module fwrite should be tested. */
#define GNULIB_TEST_FWRITE 1

/* Define to 1 when the gnulib module getc should be tested. */
#define GNULIB_TEST_GETC 1

/* Define to 1 when the gnulib module getchar should be tested. */
#define GNULIB_TEST_GETCHAR 1

/* Define to 1 when the gnulib module getcwd should be tested. */
#define GNULIB_TEST_GETCWD 1

/* Define to 1 when the gnulib module getlocalename_l-unsafe should be tested.
 */
#define GNULIB_TEST_GETLOCALENAME_L_UNSAFE 1

/* Define to 1 when the gnulib module localename-environ should be tested. */
#define GNULIB_TEST_LOCALENAME_ENVIRON 1

/* Define to 1 when the gnulib module localename-unsafe should be tested. */
#define GNULIB_TEST_LOCALENAME_UNSAFE 1

/* Define to 1 when the gnulib module mbrtowc should be tested. */
#define GNULIB_TEST_MBRTOWC 1

/* Define to 1 when the gnulib module mbsinit should be tested. */
#define GNULIB_TEST_MBSINIT 1

/* Define to 1 when the gnulib module mbszero should be tested. */
#define GNULIB_TEST_MBSZERO 1

/* Define to 1 when the gnulib module memchr should be tested. */
#define GNULIB_TEST_MEMCHR 1

/* Define to 1 when the gnulib module printf should be tested. */
#define GNULIB_TEST_PRINTF 1

/* Define to 1 when the gnulib module pthread-once should be tested. */
#define GNULIB_TEST_PTHREAD_ONCE 1

/* Define to 1 when the gnulib module putc should be tested. */
#define GNULIB_TEST_PUTC 1

/* Define to 1 when the gnulib module putchar should be tested. */
#define GNULIB_TEST_PUTCHAR 1

/* Define to 1 when the gnulib module puts should be tested. */
#define GNULIB_TEST_PUTS 1

/* Define to 1 when the gnulib module scanf should be tested. */
#define GNULIB_TEST_SCANF 1

/* Define to 1 when the gnulib module setlocale_null should be tested. */
#define GNULIB_TEST_SETLOCALE_NULL 1

/* Define to 1 when the gnulib module signbit should be tested. */
#define GNULIB_TEST_SIGNBIT 1

/* Define to 1 when the gnulib module tsearch should be tested. */
#define GNULIB_TEST_TSEARCH 1

/* Define to 1 when the gnulib module vfprintf should be tested. */
#define GNULIB_TEST_VFPRINTF 1

/* Define to 1 when the gnulib module vprintf should be tested. */
#define GNULIB_TEST_VPRINTF 1

/* Define to 1 when the gnulib module wgetcwd should be tested. */
#define GNULIB_TEST_WGETCWD 1

/* Define to 1 when the gnulib module wmemcpy should be tested. */
#define GNULIB_TEST_WMEMCPY 1

/* Define to 1 when the gnulib module wmemset should be tested. */
#define GNULIB_TEST_WMEMSET 1

/* Define if the __locale_t type contains the name of the LC_MESSAGES
   category. */
/* #undef HAVE_AIX72_LOCALES */

/* Define to 1 if you have 'alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if <alloca.h> works. */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the 'asprintf' function. */
#define HAVE_ASPRINTF 1

/* Define to 1 if you have the <bp-sym.h> header file. */
/* #undef HAVE_BP_SYM_H */

/* Define to 1 if the compiler understands __builtin_expect. */
#define HAVE_BUILTIN_EXPECT 1

/* Define to 1 if you have the Mac OS X function
   CFLocaleCopyPreferredLanguages in the CoreFoundation framework. */
#define HAVE_CFLOCALECOPYPREFERREDLANGUAGES 1

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
#define HAVE_CFPREFERENCESCOPYAPPVALUE 1

/* Define if the copysignf function is declared in <math.h> and available in
   libc. */
/* #undef HAVE_COPYSIGNF_IN_LIBC */

/* Define if the copysignl function is declared in <math.h> and available in
   libc. */
/* #undef HAVE_COPYSIGNL_IN_LIBC */

/* Define if the copysign function is declared in <math.h> and available in
   libc. */
/* #undef HAVE_COPYSIGN_IN_LIBC */

/* Define to 1 if you have the <crtdefs.h> header file. */
/* #undef HAVE_CRTDEFS_H */

/* Define to 1 if bool, true and false work as per C2023. */
/* #undef HAVE_C_BOOL */

/* Define to 1 if the static_assert keyword works. */
/* #undef HAVE_C_STATIC_ASSERT */

/* Define if the GNU dcgettext() function is already present or preinstalled.
 */
/* #undef HAVE_DCGETTEXT */

/* Define to 1 if you have the declaration of 'alarm', and to 0 if you don't.
 */
#define HAVE_DECL_ALARM 1

/* Define to 1 if you have the declaration of 'copysign', and to 0 if you
   don't. */
/* #undef HAVE_DECL_COPYSIGN */

/* Define to 1 if you have the declaration of 'copysignf', and to 0 if you
   don't. */
/* #undef HAVE_DECL_COPYSIGNF */

/* Define to 1 if you have the declaration of 'copysignl', and to 0 if you
   don't. */
/* #undef HAVE_DECL_COPYSIGNL */

/* Define to 1 if you have the declaration of 'ecvt', and to 0 if you don't.
 */
#define HAVE_DECL_ECVT 1

/* Define to 1 if you have the declaration of 'execvpe', and to 0 if you
   don't. */
#define HAVE_DECL_EXECVPE 0

/* Define to 1 if you have the declaration of 'fcloseall', and to 0 if you
   don't. */
#define HAVE_DECL_FCLOSEALL 0

/* Define to 1 if you have the declaration of 'fcvt', and to 0 if you don't.
 */
#define HAVE_DECL_FCVT 1

/* Define to 1 if you have the declaration of 'feof_unlocked', and to 0 if you
   don't. */
#define HAVE_DECL_FEOF_UNLOCKED 1

/* Define to 1 if you have the declaration of 'fgets_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FGETS_UNLOCKED 0

/* Define to 1 if you have the declaration of 'gcvt', and to 0 if you don't.
 */
#define HAVE_DECL_GCVT 1

/* Define to 1 if you have the declaration of 'getw', and to 0 if you don't.
 */
#define HAVE_DECL_GETW 1

/* Define to 1 if you have the declaration of 'mbrtowc', and to 0 if you
   don't. */
/* #undef HAVE_DECL_MBRTOWC */

/* Define to 1 if you have the declaration of 'mbsinit', and to 0 if you
   don't. */
/* #undef HAVE_DECL_MBSINIT */

/* Define to 1 if you have the declaration of 'putw', and to 0 if you don't.
 */
#define HAVE_DECL_PUTW 1

/* Define to 1 if you have the declaration of 'wcsdup', and to 0 if you don't.
 */
#define HAVE_DECL_WCSDUP 1

/* Define to 1 if you have the declaration of 'wcsnlen', and to 0 if you
   don't. */
#define HAVE_DECL_WCSNLEN 1

/* Define to 1 if you have the declaration of '_snprintf', and to 0 if you
   don't. */
#define HAVE_DECL__SNPRINTF 0

/* Define to 1 if you have the declaration of '_snwprintf', and to 0 if you
   don't. */
#define HAVE_DECL__SNWPRINTF 0

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `duplocale' function. */
#define HAVE_DUPLOCALE 1

/* Define if the locale_t type contains insufficient information, as on
   OpenBSD. */
/* #undef HAVE_FAKE_LOCALES */

/* Define to 1 if you have the <features.h> header file. */
/* #undef HAVE_FEATURES_H */

/* Define to 1 if you have the `freelocale' function. */
#define HAVE_FREELOCALE 1

/* Define if the 'free' function is guaranteed to preserve errno. */
/* #undef HAVE_FREE_POSIX */

/* Define if the frexpl function is available in libc. */
#define HAVE_FREXPL_IN_LIBC 1

/* Define if the frexp function is available in libc. */
#define HAVE_FREXP_IN_LIBC 1

/* Define to 1 if you have the 'getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the 'getegid' function. */
#define HAVE_GETEGID 1

/* Define to 1 if you have the 'geteuid' function. */
#define HAVE_GETEUID 1

/* Define to 1 if you have the 'getgid' function. */
#define HAVE_GETGID 1

/* Define to 1 if you have the 'getlocalename_l' function. */
/* #undef HAVE_GETLOCALENAME_L */

/* Define to 1 if you have the 'getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define to 1 if you have the 'getuid' function. */
#define HAVE_GETUID 1

/* Define if the uselocale function exists, may be safely called, and returns
   sufficient information. */
#define HAVE_GOOD_USELOCALE 1

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Define if you have the 'intmax_t' type in <stdint.h> or <inttypes.h>. */
#define HAVE_INTMAX_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if <inttypes.h> exists, doesn't clash with <sys/types.h>, and
   declares uintmax_t. */
#define HAVE_INTTYPES_H_WITH_UINTMAX 1

/* Define if the isnan(double) function is available in libc. */
#define HAVE_ISNAND_IN_LIBC 1

/* Define if the isnan(float) function is available in libc. */
#define HAVE_ISNANF_IN_LIBC 1

/* Define if the isnan(long double) function is available in libc. */
#define HAVE_ISNANL_IN_LIBC 1

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#define HAVE_LANGINFO_CODESET 1

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define if the ldexpl function is available in libc. */
#define HAVE_LDEXPL_IN_LIBC 1

/* Define if the ldexp function is available in libc. */
#define HAVE_LDEXP_IN_LIBC 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if the system has the type 'long long int'. */
#define HAVE_LONG_LONG_INT 1

/* Define to 1 if mmap()'s MAP_ANONYMOUS flag is available after including
   config.h and <sys/mman.h>. */
#define HAVE_MAP_ANONYMOUS 1

/* Define to 1 if you have the <math.h> header file. */
#define HAVE_MATH_H 1

/* Define to 1 if you have the 'mbrtowc' function. */
#define HAVE_MBRTOWC 1

/* Define to 1 if you have the 'mbsinit' function. */
#define HAVE_MBSINIT 1

/* Define to 1 if <wchar.h> declares mbstate_t. */
#define HAVE_MBSTATE_T 1

/* Define to 1 if you have the 'mempcpy' function. */
/* #undef HAVE_MEMPCPY */

/* Define to 1 if you have the <minix/config.h> header file. */
/* #undef HAVE_MINIX_CONFIG_H */

/* Define to 1 if you have a working 'mmap' system call. */
#define HAVE_MMAP 1

/* Define to 1 if you have the 'mprotect' function. */
#define HAVE_MPROTECT 1

/* Define to 1 if you have the 'munmap' function. */
#define HAVE_MUNMAP 1

/* Define if the locale_t type does not contain the name of each locale
   category. */
/* #undef HAVE_NAMELESS_LOCALES */

/* Define to 1 if you have the 'newlocale' function. */
#define HAVE_NEWLOCALE 1

/* Define to 1 if you have the `nl_langinfo' function. */
#define HAVE_NL_LANGINFO 1

/* Define if your printf() function supports format strings with positions. */
#define HAVE_POSIX_PRINTF 1

/* Define if you have the <pthread.h> header and the POSIX threads API. */
#define HAVE_PTHREAD_API 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define if the <pthread.h> defines PTHREAD_MUTEX_RECURSIVE. */
#define HAVE_PTHREAD_MUTEX_RECURSIVE 1

/* Define if the POSIX multithreading library has read/write locks. */
#define HAVE_PTHREAD_RWLOCK 1

/* Define if the 'pthread_rwlock_rdlock' function prefers a writer to a
   reader. */
#define HAVE_PTHREAD_RWLOCK_RDLOCK_PREFER_WRITER 1

/* Define to 1 if the system has the type 'pthread_spinlock_t'. */
/* #undef HAVE_PTHREAD_SPINLOCK_T */

/* Define to 1 if the system has the type 'pthread_t'. */
#define HAVE_PTHREAD_T 1

/* Define to 1 if 'long double' and 'double' have the same representation. */
#define HAVE_SAME_LONG_DOUBLE_AS_DOUBLE 1

/* Define to 1 if you have the <sched.h> header file. */
#define HAVE_SCHED_H 1

/* Define to 1 if you have the <search.h> header file. */
#define HAVE_SEARCH_H 1

/* Define to 1 if 'sig_atomic_t' is a signed integer type. */
/* #undef HAVE_SIGNED_SIG_ATOMIC_T */

/* Define to 1 if 'wchar_t' is a signed integer type. */
/* #undef HAVE_SIGNED_WCHAR_T */

/* Define to 1 if 'wint_t' is a signed integer type. */
/* #undef HAVE_SIGNED_WINT_T */

/* Define to 1 if you have the 'snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define if the return value of the snprintf function is the number of of
   bytes (excluding the terminating NUL) that would have been produced if the
   buffer had been large enough. */
#define HAVE_SNPRINTF_RETVAL_C99 1

/* Define if the string produced by the snprintf function is always NUL
   terminated. */
#define HAVE_SNPRINTF_TRUNCATION_C99 1

/* Define if the locale_t type is as on Solaris 11.4. */
/* #undef HAVE_SOLARIS114_LOCALES */

/* Define to 1 if you have the <stdbool.h> header file. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if <stdint.h> exists, doesn't clash with <sys/types.h>, and declares
   uintmax_t. */
#define HAVE_STDINT_H_WITH_UINTMAX 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the 'stpcpy' function. */
#define HAVE_STPCPY 1

/* Define to 1 if you have the 'strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the 'strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the 'swprintf' function. */
#define HAVE_SWPRINTF 1

/* Define to 1 if you have the 'symlink' function. */
#define HAVE_SYMLINK 1

/* Define to 1 if you have the <sys/bitypes.h> header file. */
/* #undef HAVE_SYS_BITYPES_H */

/* Define to 1 if you have the <sys/inttypes.h> header file. */
/* #undef HAVE_SYS_INTTYPES_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/single_threaded.h> header file. */
/* #undef HAVE_SYS_SINGLE_THREADED_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `thrd_create' function. */
/* #undef HAVE_THRD_CREATE */

/* Define to 1 if you have the <threads.h> header file. */
/* #undef HAVE_THREADS_H */

/* Define to 1 if you have the `tsearch' function. */
#define HAVE_TSEARCH 1

/* Define to 1 if you have the `twalk' function. */
#define HAVE_TWALK 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if the system has the type 'unsigned long long int'. */
#define HAVE_UNSIGNED_LONG_LONG_INT 1

/* Define to 1 if you have the 'uselocale' function. */
#define HAVE_USELOCALE 1

/* Define to 1 if you have the 'vasnprintf' function. */
/* #undef HAVE_VASNPRINTF */

/* Define to 1 or 0, depending whether the compiler supports simple visibility
   declarations. */
#define HAVE_VISIBILITY 1

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the 'wcrtomb' function. */
#define HAVE_WCRTOMB 1

/* Define to 1 if you have the 'wcslen' function. */
#define HAVE_WCSLEN 1

/* Define to 1 if you have the 'wcsnlen' function. */
#define HAVE_WCSNLEN 1

/* Define to 1 if the compiler and linker support weak declarations of
   symbols. */
/* #undef HAVE_WEAK_SYMBOLS */

/* Define to 1 if <locale.h> defines the _locale_t type. */
/* #undef HAVE_WINDOWS_LOCALE_T */

/* Define if you have the 'wint_t' type. */
#define HAVE_WINT_T 1

/* Define to 1 if O_NOATIME works. */
#define HAVE_WORKING_O_NOATIME 1

/* Define to 1 if O_NOFOLLOW works. */
#define HAVE_WORKING_O_NOFOLLOW 1

/* Define if the swprintf function works correctly when it produces output
   that contains null wide characters. */
/* #undef HAVE_WORKING_SWPRINTF */

/* Define if the uselocale function exists and may safely be called. */
#define HAVE_WORKING_USELOCALE 1

/* Define to 1 if you have the 'wprintf' function. */
#define HAVE_WPRINTF 1

/* Define to 1 if you have the <xlocale.h> header file. */
#define HAVE_XLOCALE_H 1

/* Define to 1 if you have the '__fsetlocking' function. */
/* #undef HAVE___FSETLOCKING */

/* Define to 1 if ctype.h defines __header_inline. */
#define HAVE___HEADER_INLINE 1

/* Please see the Gnulib manual for how to use these macros.

   Suppress extern inline with HP-UX cc, as it appears to be broken; see
   <https://lists.gnu.org/r/bug-texinfo/2013-02/msg00030.html>.

   Suppress extern inline with Sun C in standards-conformance mode, as it
   mishandles inline functions that call each other.  E.g., for 'inline void f
   (void) { } inline void g (void) { f (); }', c99 incorrectly complains
   'reference to static identifier "f" in extern inline function'.
   This bug was observed with Oracle Developer Studio 12.6
   (Sun C 5.15 SunOS_sparc 2017/05/30).

   Suppress extern inline (with or without __attribute__ ((__gnu_inline__)))
   on configurations that mistakenly use 'static inline' to implement
   functions or macros in standard C headers like <ctype.h>.  For example,
   if isdigit is mistakenly implemented via a static inline function,
   a program containing an extern inline function that calls isdigit
   may not work since the C standard prohibits extern inline functions
   from calling static functions (ISO C 99 section 6.7.4.(3).
   This bug is known to occur on:

     OS X 10.8 and earlier; see:
     https://lists.gnu.org/r/bug-gnulib/2012-12/msg00023.html

     DragonFly; see
     http://muscles.dragonflybsd.org/bulk/clang-master-potential/20141111_102002/logs/ah-tty-0.3.12.log

     FreeBSD; see:
     https://lists.gnu.org/r/bug-gnulib/2014-07/msg00104.html

   OS X 10.9 has a macro __header_inline indicating the bug is fixed for C and
   for clang but remains for g++; see <https://trac.macports.org/ticket/41033>.
   Assume DragonFly and FreeBSD will be similar.

   GCC 4.3 and above with -std=c99 or -std=gnu99 implements ISO C99
   inline semantics, unless -fgnu89-inline is used.  It defines a macro
   __GNUC_STDC_INLINE__ to indicate this situation or a macro
   __GNUC_GNU_INLINE__ to indicate the opposite situation.
   GCC 4.2 with -std=c99 or -std=gnu99 implements the GNU C inline
   semantics but warns, unless -fgnu89-inline is used:
     warning: C99 inline functions are not supported; using GNU89
     warning: to disable this warning use -fgnu89-inline or the gnu_inline
   function attribute It defines a macro __GNUC_GNU_INLINE__ to indicate this
   situation.
 */
#if (((defined __APPLE__ && defined __MACH__) || defined __DragonFly__ || \
      defined __FreeBSD__) &&                                             \
     (defined HAVE___HEADER_INLINE                                        \
          ? (defined __cplusplus && defined __GNUC_STDC_INLINE__ &&       \
             !defined __clang__)                                          \
          : ((!defined _DONT_USE_CTYPE_INLINE_ &&                         \
              (defined __GNUC__ || defined __cplusplus)) ||               \
             (defined _FORTIFY_SOURCE && 0 < _FORTIFY_SOURCE &&           \
              defined __GNUC__ && !defined __cplusplus))))
#define _GL_EXTERN_INLINE_STDHEADER_BUG
#endif
#if ((__GNUC__ ? (defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__ &&  \
                  !defined __PCC__)                                        \
               : (199901L <= __STDC_VERSION__ && !defined __HP_cc &&       \
                  !defined __PGI && !(defined __SUNPRO_C && __STDC__))) && \
     !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
#define _GL_INLINE inline
#define _GL_EXTERN_INLINE extern inline
#define _GL_EXTERN_INLINE_IN_USE
#elif (2 < __GNUC__ + (7 <= __GNUC_MINOR__) && !defined __STRICT_ANSI__ && \
       !defined __PCC__ && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
#if defined __GNUC_GNU_INLINE__ && __GNUC_GNU_INLINE__
/* __gnu_inline__ suppresses a GCC 4.2 diagnostic.  */
#define _GL_INLINE extern inline __attribute__((__gnu_inline__))
#else
#define _GL_INLINE extern inline
#endif
#define _GL_EXTERN_INLINE extern
#define _GL_EXTERN_INLINE_IN_USE
#else
#define _GL_INLINE _GL_UNUSED static
#define _GL_EXTERN_INLINE _GL_UNUSED static
#endif

/* In GCC 4.6 (inclusive) to 5.1 (exclusive),
   suppress bogus "no previous prototype for 'FOO'"
   and "no previous declaration for 'FOO'" diagnostics,
   when FOO is an inline function in the header; see
   <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54113> and
   <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63877>.  */
#if __GNUC__ == 4 && 6 <= __GNUC_MINOR__
#if defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__
#define _GL_INLINE_HEADER_CONST_PRAGMA
#else
#define _GL_INLINE_HEADER_CONST_PRAGMA \
  _Pragma("GCC diagnostic ignored \"-Wsuggest-attribute=const\"")
#endif
#define _GL_INLINE_HEADER_BEGIN                                        \
  _Pragma("GCC diagnostic push")                                       \
      _Pragma("GCC diagnostic ignored \"-Wmissing-prototypes\"")       \
          _Pragma("GCC diagnostic ignored \"-Wmissing-declarations\"") \
              _GL_INLINE_HEADER_CONST_PRAGMA
#define _GL_INLINE_HEADER_END _Pragma("GCC diagnostic pop")
#else
#define _GL_INLINE_HEADER_BEGIN
#define _GL_INLINE_HEADER_END
#endif

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'long double'. */
#define LDBL_EXPBIT0_BIT 20

/* Define as the word index where to find the exponent of 'long double'. */
#define LDBL_EXPBIT0_WORD 1

/* Define as the bit index in the word where to find the sign of 'long
   double'. */
/* #undef LDBL_SIGNBIT_BIT */

/* Define as the word index where to find the sign of 'long double'. */
/* #undef LDBL_SIGNBIT_WORD */

/* Define if localename.c overrides newlocale(), duplocale(), freelocale(). */
/* #undef LOCALENAME_ENHANCE_LOCALE_FUNCS */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Define to a substitute value for mmap()'s MAP_ANONYMOUS flag. */
/* #undef MAP_ANONYMOUS */

/* Define if the mbrtowc function does not return (size_t) -2 for empty input.
 */
/* #undef MBRTOWC_EMPTY_INPUT_BUG */

/* Define if the mbrtowc function may signal encoding errors in the C locale.
 */
/* #undef MBRTOWC_IN_C_LOCALE_MAYBE_EILSEQ */

/* Define if the mbrtowc function has the NULL pwc argument bug. */
/* #undef MBRTOWC_NULL_ARG1_BUG */

/* Define if the mbrtowc function has the NULL string argument bug. */
/* #undef MBRTOWC_NULL_ARG2_BUG */

/* Define if the mbrtowc function does not return 0 for a NUL character. */
/* #undef MBRTOWC_NUL_RETVAL_BUG */

/* Define if the mbrtowc function returns a wrong return value. */
/* #undef MBRTOWC_RETVAL_BUG */

/* Define if the mbrtowc function stores a wide character when reporting
   incomplete input. */
/* #undef MBRTOWC_STORES_INCOMPLETE_BUG */

/* Use GNU style printf and scanf.  */
#ifndef __USE_MINGW_ANSI_STDIO
#define __USE_MINGW_ANSI_STDIO 1
#endif

/* Define to 1 on musl libc. */
/* #undef MUSL_LIBC */

/* Define if the vasnprintf implementation needs special code for the 'a' and
   'A' directives. */
#define NEED_PRINTF_DIRECTIVE_A 1

/* Define if the vasnprintf implementation needs special code for the 'b'
   directive. */
#define NEED_PRINTF_DIRECTIVE_B 1

/* Define if the vasnprintf implementation needs special code for the 'F'
   directive. */
/* #undef NEED_PRINTF_DIRECTIVE_F */

/* Define if the vasnprintf implementation needs special code for the 'lc'
   directive. */
/* #undef NEED_PRINTF_DIRECTIVE_LC */

/* Define if the vasnprintf implementation needs special code for the 'ls'
   directive. */
/* #undef NEED_PRINTF_DIRECTIVE_LS */

/* Define if the vasnprintf implementation needs special code for 'double'
   arguments. */
#define NEED_PRINTF_DOUBLE 1

/* Define if the vasnprintf implementation needs special code for surviving
   out-of-memory conditions. */
#define NEED_PRINTF_ENOMEM 1

/* Define if the vasnprintf implementation needs special code for the # flag
   with a zero precision and a zero value in the 'x' and 'X' directives. */
/* #undef NEED_PRINTF_FLAG_ALT_PRECISION_ZERO */

/* Define if the vasnprintf implementation needs special code for the ' flag.
 */
/* #undef NEED_PRINTF_FLAG_GROUPING */

/* Define if the vasnprintf implementation needs special code for the '-'
   flag. */
/* #undef NEED_PRINTF_FLAG_LEFTADJUST */

/* Define if the vasnprintf implementation needs special code for the 0 flag.
 */
/* #undef NEED_PRINTF_FLAG_ZERO */

/* Define if the vasnprintf implementation needs special code for infinite
   'double' arguments. */
/* #undef NEED_PRINTF_INFINITE_DOUBLE */

/* Define if the vasnprintf implementation needs special code for infinite
   'long double' arguments. */
/* #undef NEED_PRINTF_INFINITE_LONG_DOUBLE */

/* Define if the vasnprintf implementation needs special code for 'long
   double' arguments. */
#define NEED_PRINTF_LONG_DOUBLE 1

/* Define if the vasnprintf implementation needs special code for supporting
   large precisions without arbitrary bounds. */
/* #undef NEED_PRINTF_UNBOUNDED_PRECISION */

/* Define if the vasnwprintf implementation needs special code for the 'c'
   directive. */
#define NEED_WPRINTF_DIRECTIVE_C 1

/* Define if the vasnwprintf implementation needs special code for the 'a'
   directive with 'long double' arguments. */
/* #undef NEED_WPRINTF_DIRECTIVE_LA */

/* Define if the vasnwprintf implementation needs special code for the 'lc'
   directive. */
#define NEED_WPRINTF_DIRECTIVE_LC 1

/* Name of package */
#define PACKAGE "libintl"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "bug-gettext@gnu.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "libintl"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libintl 0.24"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libintl"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.24"

/* Define if the pthread_in_use() detection is hard. */
/* #undef PTHREAD_IN_USE_DETECTION_HARD */

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'ptrdiff_t'. */
/* #undef PTRDIFF_T_SUFFIX */

/* Define if vasnprintf exists but is overridden by gnulib. */
/* #undef REPLACE_VASNPRINTF */

/* Define to 1 if setlocale (LC_ALL, NULL) is multithread-safe. */
#define SETLOCALE_NULL_ALL_MTSAFE 0

/* Define to 1 if setlocale (category, NULL) is multithread-safe. */
#define SETLOCALE_NULL_ONE_MTSAFE 1

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'sig_atomic_t'. */
/* #undef SIG_ATOMIC_T_SUFFIX */

/* Define as the maximum value of type 'size_t', if the system doesn't define
   it. */
#ifndef SIZE_MAX
/* # undef SIZE_MAX */
#endif

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'size_t'. */
/* #undef SIZE_T_SUFFIX */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
        STACK_DIRECTION > 0 => grows toward higher addresses
        STACK_DIRECTION < 0 => grows toward lower addresses
        STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Define if the combination of the ISO C and POSIX multithreading APIs can be
   used. */
/* #undef USE_ISOC_AND_POSIX_THREADS */

/* Define if the ISO C multithreading library can be used. */
/* #undef USE_ISOC_THREADS */

/* Define to enable the declarations of ISO C 23 Annex K types and functions. */
#if !(defined __STDC_WANT_LIB_EXT1__ && __STDC_WANT_LIB_EXT1__)
#undef /**/ __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif

/* Define if the POSIX multithreading library can be used. */
#define USE_POSIX_THREADS 1

/* Define if references to the POSIX multithreading library are satisfied by
   libc. */
/* #undef USE_POSIX_THREADS_FROM_LIBC */

/* Define if references to the POSIX multithreading library should be made
   weak. */
/* #undef USE_POSIX_THREADS_WEAK */

/* Enable extensions on AIX, Interix, z/OS.  */
#ifndef _ALL_SOURCE
#define _ALL_SOURCE 1
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
#define __EXTENSIONS__ 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
/* Enable X/Open compliant socket functions that do not require linking
   with -lxnet on HP-UX 11.11.  */
#ifndef _HPUX_ALT_XOPEN_SOCKET_API
#define _HPUX_ALT_XOPEN_SOCKET_API 1
#endif
/* Identify the host operating system as Minix.
   This macro does not affect the system headers' behavior.
   A future release of Autoconf may stop defining this macro.  */
#ifndef _MINIX
/* # undef _MINIX */
#endif
/* Enable general extensions on NetBSD.
   Enable NetBSD compatibility extensions on Minix.  */
#ifndef _NETBSD_SOURCE
#define _NETBSD_SOURCE 1
#endif
/* Enable OpenBSD compatibility extensions on NetBSD.
   Oddly enough, this does nothing on OpenBSD.  */
#ifndef _OPENBSD_SOURCE
#define _OPENBSD_SOURCE 1
#endif
/* Define to 1 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_SOURCE
/* # undef _POSIX_SOURCE */
#endif
/* Define to 2 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_1_SOURCE
/* # undef _POSIX_1_SOURCE */
#endif
/* Enable POSIX-compatible threading on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
#define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-5:2014.  */
#ifndef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
#define __STDC_WANT_IEC_60559_ATTRIBS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-1:2014.  */
#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
#define __STDC_WANT_IEC_60559_BFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-2:2015.  */
#ifndef __STDC_WANT_IEC_60559_DFP_EXT__
#define __STDC_WANT_IEC_60559_DFP_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex F.  */
#ifndef __STDC_WANT_IEC_60559_EXT__
#define __STDC_WANT_IEC_60559_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-4:2015.  */
#ifndef __STDC_WANT_IEC_60559_FUNCS_EXT__
#define __STDC_WANT_IEC_60559_FUNCS_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex H and ISO/IEC TS 18661-3:2015.  */
#ifndef __STDC_WANT_IEC_60559_TYPES_EXT__
#define __STDC_WANT_IEC_60559_TYPES_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TR 24731-2:2010.  */
#ifndef __STDC_WANT_LIB_EXT2__
#define __STDC_WANT_LIB_EXT2__ 1
#endif
/* Enable extensions specified by ISO/IEC 24747:2009.  */
#ifndef __STDC_WANT_MATH_SPEC_FUNCS__
#define __STDC_WANT_MATH_SPEC_FUNCS__ 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
#define _TANDEM_SOURCE 1
#endif
/* Enable X/Open extensions.  Define to 500 only if necessary
   to make mbstate_t available.  */
#ifndef _XOPEN_SOURCE
/* # undef _XOPEN_SOURCE */
#endif

/* Define if the native Windows multithreading API can be used. */
/* #undef USE_WINDOWS_THREADS */

/* Version number of package */
#define VERSION "0.24"

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'wchar_t'. */
/* #undef WCHAR_T_SUFFIX */

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'wint_t'. */
/* #undef WINT_T_SUFFIX */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
#if defined __BIG_ENDIAN__
#define WORDS_BIGENDIAN 1
#endif
#else
#ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
#endif
#endif

/* True if the compiler says it groks GNU C version MAJOR.MINOR.
    Except that
      - clang groks GNU C 4.2, even on Windows, where it does not define
        __GNUC__.
      - The OpenMandriva-modified clang compiler pretends that it groks
        GNU C version 13.1, but it doesn't: It does not support
        __attribute__ ((__malloc__ (f, i))), nor does it support
        __attribute__ ((__warning__ (message))) on a function redeclaration.
      - Users can make clang lie as well, through the -fgnuc-version option.  */
#if defined __GNUC__ && defined __GNUC_MINOR__ && !defined __clang__
#define _GL_GNUC_PREREQ(major, minor) \
  ((major) < __GNUC__ + ((minor) <= __GNUC_MINOR__))
#elif defined __clang__
/* clang really only groks GNU C 4.2.  */
#define _GL_GNUC_PREREQ(major, minor) ((major) < 4 + ((minor) <= 2))
#else
#define _GL_GNUC_PREREQ(major, minor) 0
#endif

/* Define to enable the declarations of ISO C 11 types and functions. */
/* #undef _ISOC11_SOURCE */

/* Define to 1 on Solaris. */
/* #undef _LCONV_C99 */

/* Define so that AIX headers are more compatible with GNU/Linux. */
#define _LINUX_SOURCE_COMPAT 1

/* The _Noreturn keyword of C11.  */
#ifndef _Noreturn
#if (defined __cplusplus &&                                                 \
     ((201103 <= __cplusplus && !(__GNUC__ == 4 && __GNUC_MINOR__ == 7)) || \
      (defined _MSC_VER && 1900 <= _MSC_VER)) &&                            \
     0)
/* [[noreturn]] is not practically usable, because with it the syntax
     extern _Noreturn void func (...);
   would not be valid; such a declaration would only be valid with 'extern'
   and '_Noreturn' swapped, or without the 'extern' keyword.  However, some
   AIX system header files and several gnulib header files use precisely
   this syntax with 'extern'.  */
#define _Noreturn [[noreturn]]
#elif (defined __clang__ && __clang_major__ < 16 && \
       defined _GL_WORK_AROUND_LLVM_BUG_59792)
/* Compile with -D_GL_WORK_AROUND_LLVM_BUG_59792 to work around
   that rare LLVM bug, though you may get many false-alarm warnings.  */
#define _Noreturn
#elif ((!defined __cplusplus || defined __clang__) &&                  \
       (201112 <= (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) || \
        (!defined __STRICT_ANSI__ &&                                   \
         (_GL_GNUC_PREREQ(4, 7) ||                                     \
          (defined __apple_build_version__                             \
               ? 6000000 <= __apple_build_version__                    \
               : 3 < __clang_major__ + (5 <= __clang_minor__))))))
/* _Noreturn works as-is.  */
#elif _GL_GNUC_PREREQ(2, 8) || defined __clang__ || 0x5110 <= __SUNPRO_C
#define _Noreturn __attribute__((__noreturn__))
#elif 1200 <= (defined _MSC_VER ? _MSC_VER : 0)
#define _Noreturn __declspec(noreturn)
#else
#define _Noreturn
#endif
#endif

/* For standard stat data types on VMS. */
#define _USE_STD_STAT 1

/* Define to 1 if the system <stdint.h> predates C++11. */
/* #undef __STDC_CONSTANT_MACROS */

/* Define to 1 if the system <stdint.h> predates C++11. */
/* #undef __STDC_LIMIT_MACROS */

/* The _GL_ASYNC_SAFE marker should be attached to functions that are
   signal handlers (for signals other than SIGABRT, SIGPIPE) or can be
   invoked from such signal handlers.  Such functions have some restrictions:
     * All functions that it calls should be marked _GL_ASYNC_SAFE as well,
       or should be listed as async-signal-safe in POSIX
       <https://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_04>
       section 2.4.3.  Note that malloc(), sprintf(), and fwrite(), in
       particular, are NOT async-signal-safe.
     * All memory locations (variables and struct fields) that these functions
       access must be marked 'volatile'.  This holds for both read and write
       accesses.  Otherwise the compiler might optimize away stores to and
       reads from such locations that occur in the program, depending on its
       data flow analysis.  For example, when the program contains a loop
       that is intended to inspect a variable set from within a signal handler
           while (!signal_occurred)
             ;
       the compiler is allowed to transform this into an endless loop if the
       variable 'signal_occurred' is not declared 'volatile'.
   Additionally, recall that:
     * A signal handler should not modify errno (except if it is a handler
       for a fatal signal and ends by raising the same signal again, thus
       provoking the termination of the process).  If it invokes a function
       that may clobber errno, it needs to save and restore the value of
       errno.  */
#define _GL_ASYNC_SAFE

/* Attributes.  */
/* Define _GL_HAS_ATTRIBUTE only once, because on FreeBSD, with gcc < 5, if
   <config.h> gets included once again after <sys/cdefs.h>, __has_attribute(x)
   expands to 0 always, and redefining _GL_HAS_ATTRIBUTE would turn off all
   attributes.  */
#ifndef _GL_HAS_ATTRIBUTE
#if (defined __has_attribute &&                                             \
     (!defined __clang_minor__ ||                                           \
      (defined __apple_build_version__ ? 7000000 <= __apple_build_version__ \
                                       : 5 <= __clang_major__)))
#define _GL_HAS_ATTRIBUTE(attr) __has_attribute(__##attr##__)
#else
#define _GL_HAS_ATTRIBUTE(attr) _GL_ATTR_##attr
#define _GL_ATTR_alloc_size _GL_GNUC_PREREQ(4, 3)
#define _GL_ATTR_always_inline _GL_GNUC_PREREQ(3, 2)
#define _GL_ATTR_artificial _GL_GNUC_PREREQ(4, 3)
#define _GL_ATTR_cold _GL_GNUC_PREREQ(4, 3)
#define _GL_ATTR_const _GL_GNUC_PREREQ(2, 95)
#define _GL_ATTR_deprecated _GL_GNUC_PREREQ(3, 1)
#define _GL_ATTR_diagnose_if 0
#define _GL_ATTR_error _GL_GNUC_PREREQ(4, 3)
#define _GL_ATTR_externally_visible _GL_GNUC_PREREQ(4, 1)
#define _GL_ATTR_fallthrough _GL_GNUC_PREREQ(7, 0)
#define _GL_ATTR_format _GL_GNUC_PREREQ(2, 7)
#define _GL_ATTR_leaf _GL_GNUC_PREREQ(4, 6)
#define _GL_ATTR_malloc _GL_GNUC_PREREQ(3, 0)
#ifdef _ICC
#define _GL_ATTR_may_alias 0
#else
#define _GL_ATTR_may_alias _GL_GNUC_PREREQ(3, 3)
#endif
#define _GL_ATTR_noinline _GL_GNUC_PREREQ(3, 1)
#define _GL_ATTR_nonnull _GL_GNUC_PREREQ(3, 3)
#define _GL_ATTR_nonstring _GL_GNUC_PREREQ(8, 0)
#define _GL_ATTR_nothrow _GL_GNUC_PREREQ(3, 3)
#define _GL_ATTR_packed _GL_GNUC_PREREQ(2, 7)
#define _GL_ATTR_pure _GL_GNUC_PREREQ(2, 96)
#define _GL_ATTR_reproducible 0 /* not yet supported, as of GCC 14 */
#define _GL_ATTR_returns_nonnull _GL_GNUC_PREREQ(4, 9)
#define _GL_ATTR_sentinel _GL_GNUC_PREREQ(4, 0)
#define _GL_ATTR_unsequenced 0 /* not yet supported, as of GCC 14 */
#define _GL_ATTR_unused _GL_GNUC_PREREQ(2, 7)
#define _GL_ATTR_warn_unused_result _GL_GNUC_PREREQ(3, 4)
#endif
#endif

/* Use __has_c_attribute if available.  However, do not use with
   pre-C23 GCC, which can issue false positives if -Wpedantic.  */
#if (defined __has_c_attribute && \
     !(_GL_GNUC_PREREQ(4, 6) &&   \
       (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) <= 201710))
#define _GL_HAVE___HAS_C_ATTRIBUTE 1
#else
#define _GL_HAVE___HAS_C_ATTRIBUTE 0
#endif

/* Attributes in bracket syntax [[...]] vs. attributes in __attribute__((...))
   syntax, in function declarations.  There are two problems here.
   (Last tested with gcc/g++ 14 and clang/clang++ 18.)

   1) We want that the _GL_ATTRIBUTE_* can be cumulated on the same declaration
      in any order.
      =========================== foo.c = foo.cc ===========================
      __attribute__ ((__deprecated__)) [[__nodiscard__]] int bar1 (int);
      [[__nodiscard__]] __attribute__ ((__deprecated__)) int bar2 (int);
      ======================================================================
      This gives a syntax error
        - in C mode with gcc
          <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=108796>, and
        - in C++ mode with clang++ version < 16, and
        - in C++ mode, inside extern "C" {}, still in newer clang++ versions
          <https://github.com/llvm/llvm-project/issues/101990>.
 */
/* Define if, in a function declaration, the attributes in bracket syntax
   [[...]] must come before the attributes in __attribute__((...)) syntax.
   If this is defined, it is best to avoid the bracket syntax, so that the
   various _GL_ATTRIBUTE_* can be cumulated on the same declaration in any
   order.  */
#ifdef __cplusplus
#if defined __clang__
#define _GL_BRACKET_BEFORE_ATTRIBUTE 1
#endif
#else
#if defined __GNUC__ && !defined __clang__
#define _GL_BRACKET_BEFORE_ATTRIBUTE 1
#endif
#endif
/*
   2) We want that the _GL_ATTRIBUTE_* can be placed in a declaration
        - without 'extern', in C as well as in C++,
        - with 'extern', in C,
        - with 'extern "C"', in C++
      in the same position.  That is, we don't want to be forced to use a
      macro which arranges for the attribute to come before 'extern' in
      one case and after 'extern' in the other case, because such a macro
      would make the source code of .h files pretty ugly.
      =========================== foo.c = foo.cc ===========================
      #ifdef __cplusplus
      # define CC "C"
      #else
      # define CC
      #endif

      #define ND   [[__nodiscard__]]
      #define WUR  __attribute__((__warn_unused_result__))

      #ifdef __cplusplus
      extern "C" {
      #endif
                                        // gcc   clang  g++   clang++

      ND int foo (int);
      int ND foo (int);                 // warn  error  warn  error
      int foo ND (int);
      int foo (int) ND;                 // warn  error  warn  error

      WUR int foo (int);
      int WUR foo (int);
      int fo1 WUR (int);                // error error  error error
      int foo (int) WUR;

      #ifdef __cplusplus
      }
      #endif

                                        // gcc   clang  g++   clang++

      ND extern CC int foo (int);       //              error error
      extern CC ND int foo (int);       // error error
      extern CC int ND foo (int);       // warn  error  warn  error
      extern CC int foo ND (int);
      extern CC int foo (int) ND;       // warn  error  warn  error

      WUR extern CC int foo (int);      //              warn
      extern CC WUR int foo (int);
      extern CC int WUR foo (int);
      extern CC int foo WUR (int);      // error error  error error
      extern CC int foo (int) WUR;

      ND EXTERN_C_FUNC int foo (int);   //              error error
      EXTERN_C_FUNC ND int foo (int);
      EXTERN_C_FUNC int ND foo (int);   // warn  error  warn  error
      EXTERN_C_FUNC int foo ND (int);
      EXTERN_C_FUNC int foo (int) ND;   // warn  error  warn  error

      WUR EXTERN_C_FUNC int foo (int);  //              warn
      EXTERN_C_FUNC WUR int foo (int);
      EXTERN_C_FUNC int WUR foo (int);
      EXTERN_C_FUNC int fo2 WUR (int);  // error error  error error
      EXTERN_C_FUNC int foo (int) WUR;
      ======================================================================
      So, if we insist on using the 'extern' keyword ('extern CC' idiom):
        * If _GL_ATTRIBUTE_* expands to bracket syntax [[...]]
          in both C and C++, there is one available position:
            - between the function name and the parameter list.
        * If _GL_ATTRIBUTE_* expands to __attribute__((...)) syntax
          in both C and C++, there are several available positions:
            - before the return type,
            - between return type and function name,
            - at the end of the declaration.
        * If _GL_ATTRIBUTE_* expands to bracket syntax [[...]] in C and to
          __attribute__((...)) syntax in C++, there is no available position:
          it would need to come before 'extern' in C but after 'extern "C"'
          in C++.
        * If _GL_ATTRIBUTE_* expands to __attribute__((...)) syntax in C and
          to bracket syntax [[...]] in C++, there is one available position:
            - before the return type.
      Whereas, if we use the 'EXTERN_C_FUNC' idiom, which conditionally
      omits the 'extern' keyword:
        * If _GL_ATTRIBUTE_* expands to bracket syntax [[...]]
          in both C and C++, there are two available positions:
            - before the return type,
            - between the function name and the parameter list.
        * If _GL_ATTRIBUTE_* expands to __attribute__((...)) syntax
          in both C and C++, there are several available positions:
            - before the return type,
            - between return type and function name,
            - at the end of the declaration.
        * If _GL_ATTRIBUTE_* expands to bracket syntax [[...]] in C and to
          __attribute__((...)) syntax in C++, there is one available position:
            - before the return type.
        * If _GL_ATTRIBUTE_* expands to __attribute__((...)) syntax in C and
          to bracket syntax [[...]] in C++, there is one available position:
            - before the return type.
      The best choice is therefore to use the 'EXTERN_C_FUNC' idiom and
      put the attributes before the return type. This works regardless
      to what the _GL_ATTRIBUTE_* macros expand.
 */

/* Attributes in bracket syntax [[...]] vs. attributes in __attribute__((...))
   syntax, in static/inline function definitions.

   There are similar constraints as for function declarations.  However, here,
   we cannot omit the storage-class specifier.  Therefore, the following rule
   applies:
     * The macros
         _GL_ATTRIBUTE_CONST
         _GL_ATTRIBUTE_DEPRECATED
         _GL_ATTRIBUTE_MAYBE_UNUSED
         _GL_ATTRIBUTE_NODISCARD
         _GL_ATTRIBUTE_PURE
         _GL_ATTRIBUTE_REPRODUCIBLE
         _GL_ATTRIBUTE_UNSEQUENCED
       which may expand to bracket syntax [[...]], must come first, before the
       storage-class specifier.
     * Other _GL_ATTRIBUTE_* macros, that expand to __attribute__((...)) syntax,
       are better placed between the storage-class specifier and the return
       type.
 */

/* Attributes in bracket syntax [[...]] vs. attributes in __attribute__((...))
   syntax, in variable declarations.

   At which position can they be placed?
   (Last tested with gcc/g++ 14 and clang/clang++ 18.)

      =========================== foo.c = foo.cc ===========================
      #ifdef __cplusplus
      # define CC "C"
      #else
      # define CC
      #endif

      #define BD   [[__deprecated__]]
      #define AD   __attribute__ ((__deprecated__))

                              // gcc   clang  g++    clang++

      BD extern CC int var;   //              error  error
      extern CC BD int var;   // error error
      extern CC int BD var;   // warn  error  warn   error
      extern CC int var BD;

      AD extern CC int var;   //              warn
      extern CC AD int var;
      extern CC int AD var;
      extern CC int var AD;

      BD extern CC int z[];   //              error  error
      extern CC BD int z[];   // error error
      extern CC int BD z[];   // warn  error  warn   error
      extern CC int z1 BD [];
      extern CC int z[] BD;   // warn  error         error

      AD extern CC int z[];   //              warn
      extern CC AD int z[];
      extern CC int AD z[];
      extern CC int z2 AD []; // error error  error  error
      extern CC int z[] AD;
      ======================================================================

   * For non-array variables, the only good position is after the variable name,
     that is, at the end of the declaration.
   * For array variables, you will need to distinguish C and C++:
       - In C, before the 'extern' keyword.
       - In C++, between the 'extern "C"' and the variable's type.
 */

/* _GL_ATTRIBUTE_ALLOC_SIZE ((N)) declares that the Nth argument of the function
   is the size of the returned memory block.
   _GL_ATTRIBUTE_ALLOC_SIZE ((M, N)) declares that the Mth argument multiplied
   by the Nth argument of the function is the size of the returned memory block.
 */
/* Applies to: functions, pointer to functions, function types.  */
#ifndef _GL_ATTRIBUTE_ALLOC_SIZE
#if _GL_HAS_ATTRIBUTE(alloc_size)
#define _GL_ATTRIBUTE_ALLOC_SIZE(args) __attribute__((__alloc_size__ args))
#else
#define _GL_ATTRIBUTE_ALLOC_SIZE(args)
#endif
#endif

/* _GL_ATTRIBUTE_ALWAYS_INLINE tells that the compiler should always inline the
   function and report an error if it cannot do so.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_ALWAYS_INLINE
#if _GL_HAS_ATTRIBUTE(always_inline)
#define _GL_ATTRIBUTE_ALWAYS_INLINE __attribute__((__always_inline__))
#else
#define _GL_ATTRIBUTE_ALWAYS_INLINE
#endif
#endif

/* _GL_ATTRIBUTE_ARTIFICIAL declares that the function is not important to show
    in stack traces when debugging.  The compiler should omit the function from
    stack traces.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_ARTIFICIAL
#if _GL_HAS_ATTRIBUTE(artificial)
#define _GL_ATTRIBUTE_ARTIFICIAL __attribute__((__artificial__))
#else
#define _GL_ATTRIBUTE_ARTIFICIAL
#endif
#endif

/* _GL_ATTRIBUTE_COLD declares that the function is rarely executed.  */
/* Applies to: functions.  */
/* Avoid __attribute__ ((cold)) on MinGW; see thread starting at
   <https://lists.gnu.org/r/emacs-devel/2019-04/msg01152.html>.
   Also, Oracle Studio 12.6 requires 'cold' not '__cold__'.  */
#ifndef _GL_ATTRIBUTE_COLD
#if _GL_HAS_ATTRIBUTE(cold) && !defined __MINGW32__
#ifndef __SUNPRO_C
#define _GL_ATTRIBUTE_COLD __attribute__((__cold__))
#else
#define _GL_ATTRIBUTE_COLD __attribute__((cold))
#endif
#else
#define _GL_ATTRIBUTE_COLD
#endif
#endif

/* _GL_ATTRIBUTE_CONST declares:
   It is OK for a compiler to move calls to the function and to omit
   calls to the function if another call has the same arguments or the
   result is not used.
   This attribute is safe for a function that neither depends on
   nor affects state, and always returns exactly once -
   e.g., does not raise an exception, call longjmp, or loop forever.
   (This attribute is stricter than _GL_ATTRIBUTE_PURE because the
   function cannot observe state.  It is stricter than
   _GL_ATTRIBUTE_UNSEQUENCED because the function must return exactly
   once and cannot depend on state addressed by its arguments.)  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_CONST
#if _GL_HAS_ATTRIBUTE(const)
#define _GL_ATTRIBUTE_CONST __attribute__((__const__))
#else
#define _GL_ATTRIBUTE_CONST _GL_ATTRIBUTE_UNSEQUENCED
#endif
#endif

/* _GL_ATTRIBUTE_DEALLOC (F, I) declares that the function returns pointers
   that can be freed by passing them as the Ith argument to the
   function F.
   _GL_ATTRIBUTE_DEALLOC_FREE declares that the function returns pointers that
   can be freed via 'free'; it can be used only after declaring 'free'.  */
/* Applies to: functions.  Cannot be used on inline functions.  */
#ifndef _GL_ATTRIBUTE_DEALLOC
#if _GL_GNUC_PREREQ(11, 0)
#define _GL_ATTRIBUTE_DEALLOC(f, i) __attribute__((__malloc__(f, i)))
#else
#define _GL_ATTRIBUTE_DEALLOC(f, i)
#endif
#endif
/* If gnulib's <string.h> or <wchar.h> has already defined this macro, continue
   to use this earlier definition, since <stdlib.h> may not have been included
   yet.  */
#ifndef _GL_ATTRIBUTE_DEALLOC_FREE
#if defined __cplusplus && defined __GNUC__ && !defined __clang__
/* Work around GCC bug <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=108231> */
#define _GL_ATTRIBUTE_DEALLOC_FREE \
  _GL_ATTRIBUTE_DEALLOC((void (*)(void*))free, 1)
#else
#define _GL_ATTRIBUTE_DEALLOC_FREE _GL_ATTRIBUTE_DEALLOC(free, 1)
#endif
#endif

/* _GL_ATTRIBUTE_DEPRECATED: Declares that an entity is deprecated.
   The compiler may warn if the entity is used.  */
/* Applies to:
     - function, variable,
     - struct, union, struct/union member,
     - enumeration, enumeration item,
     - typedef,
   in C++ also: namespace, class, template specialization.  */
#ifndef _GL_ATTRIBUTE_DEPRECATED
#ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#if _GL_HAVE___HAS_C_ATTRIBUTE
#if __has_c_attribute(__deprecated__)
#define _GL_ATTRIBUTE_DEPRECATED [[__deprecated__]]
#endif
#endif
#endif
#if !defined _GL_ATTRIBUTE_DEPRECATED && _GL_HAS_ATTRIBUTE(deprecated)
#define _GL_ATTRIBUTE_DEPRECATED __attribute__((__deprecated__))
#endif
#ifndef _GL_ATTRIBUTE_DEPRECATED
#define _GL_ATTRIBUTE_DEPRECATED
#endif
#endif

/* _GL_ATTRIBUTE_ERROR(msg) requests an error if a function is called and
   the function call is not optimized away.
   _GL_ATTRIBUTE_WARNING(msg) requests a warning if a function is called and
   the function call is not optimized away.  */
/* Applies to: functions.  */
#if !(defined _GL_ATTRIBUTE_ERROR && defined _GL_ATTRIBUTE_WARNING)
#if _GL_HAS_ATTRIBUTE(error)
#define _GL_ATTRIBUTE_ERROR(msg) __attribute__((__error__(msg)))
#define _GL_ATTRIBUTE_WARNING(msg) __attribute__((__warning__(msg)))
#elif _GL_HAS_ATTRIBUTE(diagnose_if)
#define _GL_ATTRIBUTE_ERROR(msg) \
  __attribute__((__diagnose_if__(1, msg, "error")))
#define _GL_ATTRIBUTE_WARNING(msg) \
  __attribute__((__diagnose_if__(1, msg, "warning")))
#else
#define _GL_ATTRIBUTE_ERROR(msg)
#define _GL_ATTRIBUTE_WARNING(msg)
#endif
#endif

/* _GL_ATTRIBUTE_EXTERNALLY_VISIBLE declares that the entity should remain
   visible to debuggers etc., even with '-fwhole-program'.  */
/* Applies to: functions, variables.  */
#ifndef _GL_ATTRIBUTE_EXTERNALLY_VISIBLE
#if _GL_HAS_ATTRIBUTE(externally_visible)
#define _GL_ATTRIBUTE_EXTERNALLY_VISIBLE __attribute__((externally_visible))
#else
#define _GL_ATTRIBUTE_EXTERNALLY_VISIBLE
#endif
#endif

/* _GL_ATTRIBUTE_FALLTHROUGH declares that it is not a programming mistake if
   the control flow falls through to the immediately following 'case' or
   'default' label.  The compiler should not warn in this case.  */
/* Applies to: Empty statement (;), inside a 'switch' statement.  */
/* Always expands to something.  */
#ifndef _GL_ATTRIBUTE_FALLTHROUGH
#if _GL_HAVE___HAS_C_ATTRIBUTE
#if __has_c_attribute(__fallthrough__)
#define _GL_ATTRIBUTE_FALLTHROUGH [[__fallthrough__]]
#endif
#endif
#if !defined _GL_ATTRIBUTE_FALLTHROUGH && _GL_HAS_ATTRIBUTE(fallthrough)
#define _GL_ATTRIBUTE_FALLTHROUGH __attribute__((__fallthrough__))
#endif
#ifndef _GL_ATTRIBUTE_FALLTHROUGH
#define _GL_ATTRIBUTE_FALLTHROUGH ((void)0)
#endif
#endif

/* _GL_ATTRIBUTE_FORMAT ((ARCHETYPE, STRING-INDEX, FIRST-TO-CHECK))
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
#ifndef _GL_ATTRIBUTE_FORMAT
#if _GL_HAS_ATTRIBUTE(format)
#define _GL_ATTRIBUTE_FORMAT(spec) __attribute__((__format__ spec))
#else
#define _GL_ATTRIBUTE_FORMAT(spec)
#endif
#endif

/* _GL_ATTRIBUTE_LEAF declares that if the function is called from some other
   compilation unit, it executes code from that unit only by return or by
   exception handling.  This declaration lets the compiler optimize that unit
   more aggressively.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_LEAF
#if _GL_HAS_ATTRIBUTE(leaf)
#define _GL_ATTRIBUTE_LEAF __attribute__((__leaf__))
#else
#define _GL_ATTRIBUTE_LEAF
#endif
#endif

/* _GL_ATTRIBUTE_MALLOC declares that the function returns a pointer to freshly
   allocated memory.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_MALLOC
#if _GL_HAS_ATTRIBUTE(malloc)
#define _GL_ATTRIBUTE_MALLOC __attribute__((__malloc__))
#else
#define _GL_ATTRIBUTE_MALLOC
#endif
#endif

/* _GL_ATTRIBUTE_MAY_ALIAS declares that pointers to the type may point to the
   same storage as pointers to other types.  Thus this declaration disables
   strict aliasing optimization.  */
/* Applies to: types.  */
/* Oracle Studio 12.6 mishandles may_alias despite __has_attribute OK.  */
#ifndef _GL_ATTRIBUTE_MAY_ALIAS
#if _GL_HAS_ATTRIBUTE(may_alias) && !defined __SUNPRO_C
#define _GL_ATTRIBUTE_MAY_ALIAS __attribute__((__may_alias__))
#else
#define _GL_ATTRIBUTE_MAY_ALIAS
#endif
#endif

/* _GL_ATTRIBUTE_MAYBE_UNUSED declares that it is not a programming mistake if
   the entity is not used.  The compiler should not warn if the entity is not
   used.  */
/* Applies to:
     - function, variable,
     - struct, union, struct/union member,
     - enumeration, enumeration item,
     - typedef,
   in C++ also: class.  */
/* In C++ and C23, this is spelled [[__maybe_unused__]].
   GCC's syntax is __attribute__ ((__unused__)).
   clang supports both syntaxes.  Except that with clang ≥ 6, < 10, in C++ mode,
   __has_c_attribute (__maybe_unused__) yields true but the use of
   [[__maybe_unused__]] nevertheless produces a warning.  */
#ifndef _GL_ATTRIBUTE_MAYBE_UNUSED
#ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#if defined __clang__ && defined __cplusplus
#if !defined __apple_build_version__ && __clang_major__ >= 10
#define _GL_ATTRIBUTE_MAYBE_UNUSED [[__maybe_unused__]]
#endif
#elif _GL_HAVE___HAS_C_ATTRIBUTE
#if __has_c_attribute(__maybe_unused__)
#define _GL_ATTRIBUTE_MAYBE_UNUSED [[__maybe_unused__]]
#endif
#endif
#endif
#ifndef _GL_ATTRIBUTE_MAYBE_UNUSED
#define _GL_ATTRIBUTE_MAYBE_UNUSED _GL_ATTRIBUTE_UNUSED
#endif
#endif
/* Alternative spelling of this macro, for convenience and for
   compatibility with glibc/include/libc-symbols.h.  */
#define _GL_UNUSED _GL_ATTRIBUTE_MAYBE_UNUSED
/* Earlier spellings of this macro.  */
#define _UNUSED_PARAMETER_ _GL_ATTRIBUTE_MAYBE_UNUSED

/* _GL_ATTRIBUTE_NODISCARD declares that the caller of the function should not
   discard the return value.  The compiler may warn if the caller does not use
   the return value, unless the caller uses something like ignore_value.  */
/* Applies to: function, enumeration, class.  */
#ifndef _GL_ATTRIBUTE_NODISCARD
#ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#if defined __clang__ && defined __cplusplus
/* With clang up to 15.0.6 (at least), in C++ mode, [[__nodiscard__]] produces
   a warning.
   The 1000 below means a yet unknown threshold.  When clang++ version X
   starts supporting [[__nodiscard__]] without warning about it, you can
   replace the 1000 with X.  */
#if __clang_major__ >= 1000
#define _GL_ATTRIBUTE_NODISCARD [[__nodiscard__]]
#endif
#elif _GL_HAVE___HAS_C_ATTRIBUTE
#if __has_c_attribute(__nodiscard__)
#define _GL_ATTRIBUTE_NODISCARD [[__nodiscard__]]
#endif
#endif
#endif
#if !defined _GL_ATTRIBUTE_NODISCARD && _GL_HAS_ATTRIBUTE(warn_unused_result)
#define _GL_ATTRIBUTE_NODISCARD __attribute__((__warn_unused_result__))
#endif
#ifndef _GL_ATTRIBUTE_NODISCARD
#define _GL_ATTRIBUTE_NODISCARD
#endif
#endif

/* _GL_ATTRIBUTE_NOINLINE tells that the compiler should not inline the
   function.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_NOINLINE
#if _GL_HAS_ATTRIBUTE(noinline)
#define _GL_ATTRIBUTE_NOINLINE __attribute__((__noinline__))
#else
#define _GL_ATTRIBUTE_NOINLINE
#endif
#endif

/* _GL_ATTRIBUTE_NONNULL ((N1, N2,...)) declares that the arguments N1, N2,...
   must not be NULL.
   _GL_ATTRIBUTE_NONNULL () declares that all pointer arguments must not be
   null.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_NONNULL
#if _GL_HAS_ATTRIBUTE(nonnull)
#define _GL_ATTRIBUTE_NONNULL(args) __attribute__((__nonnull__ args))
#else
#define _GL_ATTRIBUTE_NONNULL(args)
#endif
#endif

/* _GL_ATTRIBUTE_NONSTRING declares that the contents of a character array is
   not meant to be NUL-terminated.  */
/* Applies to: struct/union members and variables that are arrays of element
   type '[[un]signed] char'.  */
#ifndef _GL_ATTRIBUTE_NONSTRING
#if _GL_HAS_ATTRIBUTE(nonstring)
#define _GL_ATTRIBUTE_NONSTRING __attribute__((__nonstring__))
#else
#define _GL_ATTRIBUTE_NONSTRING
#endif
#endif

/* There is no _GL_ATTRIBUTE_NORETURN; use _Noreturn instead.  */

/* _GL_ATTRIBUTE_NOTHROW declares that the function does not throw exceptions.
 */
/* Applies to: functions.  */
/* After a function's parameter list, this attribute must come first, before
   other attributes.  */
#ifndef _GL_ATTRIBUTE_NOTHROW
#if defined __cplusplus
#if _GL_GNUC_PREREQ(2, 8) || __clang_major__ >= 4
#if __cplusplus >= 201103L
#define _GL_ATTRIBUTE_NOTHROW noexcept(true)
#else
#define _GL_ATTRIBUTE_NOTHROW throw()
#endif
#else
#define _GL_ATTRIBUTE_NOTHROW
#endif
#else
#if _GL_HAS_ATTRIBUTE(nothrow)
#define _GL_ATTRIBUTE_NOTHROW __attribute__((__nothrow__))
#else
#define _GL_ATTRIBUTE_NOTHROW
#endif
#endif
#endif

/* _GL_ATTRIBUTE_PACKED declares:
   For struct members: The member has the smallest possible alignment.
   For struct, union, class: All members have the smallest possible alignment,
   minimizing the memory required.  */
/* Applies to: struct members, struct, union,
   in C++ also: class.  */
#ifndef _GL_ATTRIBUTE_PACKED
/* Oracle Studio 12.6 miscompiles code with __attribute__ ((__packed__)) despite
   __has_attribute OK.  */
#if _GL_HAS_ATTRIBUTE(packed) && !defined __SUNPRO_C
#define _GL_ATTRIBUTE_PACKED __attribute__((__packed__))
#else
#define _GL_ATTRIBUTE_PACKED
#endif
#endif

/* _GL_ATTRIBUTE_PURE declares:
   It is OK for a compiler to move calls to the function and to omit
   calls to the function if another call has the same arguments or the
   result is not used, and if observable state is the same.
   This attribute is safe for a function that does not affect observable state
   and always returns exactly once.
   (This attribute is looser than _GL_ATTRIBUTE_CONST because the function
   can depend on observable state.  It is stricter than
   _GL_ATTRIBUTE_REPRODUCIBLE because the function must return exactly
   once and cannot affect state addressed by its arguments.)  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_PURE
#if _GL_HAS_ATTRIBUTE(pure)
#define _GL_ATTRIBUTE_PURE __attribute__((__pure__))
#else
#define _GL_ATTRIBUTE_PURE _GL_ATTRIBUTE_REPRODUCIBLE
#endif
#endif

/* _GL_ATTRIBUTE_REPRODUCIBLE declares:
   It is OK for a compiler to move calls to the function and to omit duplicate
   calls to the function with the same arguments, so long as the state
   addressed by its arguments is the same and is updated in time for
   the rest of the program.
   This attribute is safe for a function that is effectless and idempotent; see
   ISO C 23 § 6.7.12.7 for a definition of these terms.
   (This attribute is looser than _GL_ATTRIBUTE_UNSEQUENCED because
   the function need not be stateless and idempotent.  It is looser
   than _GL_ATTRIBUTE_PURE because the function need not return
   exactly once and can affect state addressed by its arguments.)
   See also <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2956.htm> and
   <https://stackoverflow.com/questions/76847905/>.
   ATTENTION! Efforts are underway to change the meaning of this attribute.
   See <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3424.htm>.  */
/* Applies to: functions, pointer to functions, function types.  */
#ifndef _GL_ATTRIBUTE_REPRODUCIBLE
/* This may be revisited when gcc and clang support [[reproducible]] or possibly
   __attribute__ ((__reproducible__)).  */
#ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#if _GL_HAS_ATTRIBUTE(reproducible)
#define _GL_ATTRIBUTE_REPRODUCIBLE [[reproducible]]
#endif
#endif
#ifndef _GL_ATTRIBUTE_REPRODUCIBLE
#define _GL_ATTRIBUTE_REPRODUCIBLE
#endif
#endif

/* _GL_ATTRIBUTE_RETURNS_NONNULL declares that the function's return value is
   a non-NULL pointer.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_RETURNS_NONNULL
#if _GL_HAS_ATTRIBUTE(returns_nonnull)
#define _GL_ATTRIBUTE_RETURNS_NONNULL __attribute__((__returns_nonnull__))
#else
#define _GL_ATTRIBUTE_RETURNS_NONNULL
#endif
#endif

/* _GL_ATTRIBUTE_SENTINEL(pos) declares that the variadic function expects a
   trailing NULL argument.
   _GL_ATTRIBUTE_SENTINEL () - The last argument is NULL (requires C99).
   _GL_ATTRIBUTE_SENTINEL ((N)) - The (N+1)st argument from the end is NULL.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_SENTINEL
#if _GL_HAS_ATTRIBUTE(sentinel)
#define _GL_ATTRIBUTE_SENTINEL(pos) __attribute__((__sentinel__ pos))
#else
#define _GL_ATTRIBUTE_SENTINEL(pos)
#endif
#endif

/* _GL_ATTRIBUTE_UNSEQUENCED declares:
   It is OK for a compiler to move calls to the function and to omit duplicate
   calls to the function with the same arguments, so long as the state
   addressed by its arguments is the same.
   This attribute is safe for a function that is effectless, idempotent,
   stateless, and independent; see ISO C 23 § 6.7.12.7 for a definition of
   these terms.
   (This attribute is stricter than _GL_ATTRIBUTE_REPRODUCIBLE because
   the function must be stateless and independent.  It is looser than
   _GL_ATTRIBUTE_CONST because the function need not return exactly
   once and can depend on state addressed by its arguments.)
   See also <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2956.htm> and
   <https://stackoverflow.com/questions/76847905/>.
   ATTENTION! Efforts are underway to change the meaning of this attribute.
   See <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3424.htm>.  */
/* Applies to: functions, pointer to functions, function types.  */
#ifndef _GL_ATTRIBUTE_UNSEQUENCED
/* This may be revisited when gcc and clang support [[unsequenced]] or possibly
   __attribute__ ((__unsequenced__)).  */
#ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#if _GL_HAS_ATTRIBUTE(unsequenced)
#define _GL_ATTRIBUTE_UNSEQUENCED [[unsequenced]]
#endif
#endif
#ifndef _GL_ATTRIBUTE_UNSEQUENCED
#define _GL_ATTRIBUTE_UNSEQUENCED
#endif
#endif

/* A helper macro.  Don't use it directly.  */
#ifndef _GL_ATTRIBUTE_UNUSED
#if _GL_HAS_ATTRIBUTE(unused)
#define _GL_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
#define _GL_ATTRIBUTE_UNUSED
#endif
#endif

/* _GL_UNUSED_LABEL; declares that it is not a programming mistake if the
   immediately preceding label is not used.  The compiler should not warn
   if the label is not used.  */
/* Applies to: label (both in C and C++).  */
/* Note that g++ < 4.5 does not support the '__attribute__ ((__unused__)) ;'
   syntax.  But clang does.  */
#ifndef _GL_UNUSED_LABEL
#if !(defined __cplusplus && !_GL_GNUC_PREREQ(4, 5)) || defined __clang__
#define _GL_UNUSED_LABEL _GL_ATTRIBUTE_UNUSED
#else
#define _GL_UNUSED_LABEL
#endif
#endif

/* The following attributes enable detection of multithread-safety problems
   and resource leaks at compile-time, by clang ≥ 15, when the warning option
   -Wthread-safety is enabled.  For usage, see
   <https://clang.llvm.org/docs/ThreadSafetyAnalysis.html>.  */
#ifndef _GL_ATTRIBUTE_CAPABILITY_TYPE
#if __clang_major__ >= 15
#define _GL_ATTRIBUTE_CAPABILITY_TYPE(concept) \
  __attribute__((__capability__(concept)))
#else
#define _GL_ATTRIBUTE_CAPABILITY_TYPE(concept)
#endif
#endif
#ifndef _GL_ATTRIBUTE_ACQUIRE_CAPABILITY
#if __clang_major__ >= 15
#define _GL_ATTRIBUTE_ACQUIRE_CAPABILITY(resource) \
  __attribute__((__acquire_capability__(resource)))
#else
#define _GL_ATTRIBUTE_ACQUIRE_CAPABILITY(resource)
#endif
#endif
#ifndef _GL_ATTRIBUTE_RELEASE_CAPABILITY
#if __clang_major__ >= 15
#define _GL_ATTRIBUTE_RELEASE_CAPABILITY(resource) \
  __attribute__((__release_capability__(resource)))
#else
#define _GL_ATTRIBUTE_RELEASE_CAPABILITY(resource)
#endif
#endif

/* In C++, there is the concept of "language linkage", that encompasses
    name mangling and function calling conventions.
    The following macros start and end a block of "C" linkage.  */
#ifdef __cplusplus
#define _GL_BEGIN_C_LINKAGE extern "C" {
#define _GL_END_C_LINKAGE }
#else
#define _GL_BEGIN_C_LINKAGE
#define _GL_END_C_LINKAGE
#endif

/* Hidden symbol. */
/* #undef frexp */

/* Hidden symbol. */
/* #undef frexpl */

/* Define to '__inline__' or '__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to long or long long if <stdint.h> and <inttypes.h> don't define. */
/* #undef intmax_t */

/* Work around a bug in Apple GCC 4.0.1 build 5465: In C99 mode, it supports
   the ISO C 99 semantics of 'extern inline' (unlike the GNU C semantics of
   earlier versions), but does not display it by setting __GNUC_STDC_INLINE__.
   __APPLE__ && __MACH__ test for Mac OS X.
   __APPLE_CC__ tests for the Apple compiler and its version.
   __STDC_VERSION__ tests for the C99 mode.  */
#if defined __APPLE__ && defined __MACH__ && __APPLE_CC__ >= 5465 && \
    !defined __cplusplus && __STDC_VERSION__ >= 199901L &&           \
    !defined __GNUC_STDC_INLINE__
#define __GNUC_STDC_INLINE__ 1
#endif

/* Hidden symbol. */
/* #undef mbrtowc */

/* Hidden symbol. */
/* #undef mbsinit */

/* Define to a type if <wchar.h> does not define. */
/* #undef mbstate_t */

/* Hidden symbol. */
/* #undef memchr */

/* _GL_CMP (n1, n2) performs a three-valued comparison on n1 vs. n2, where
   n1 and n2 are expressions without side effects, that evaluate to real
   numbers (excluding NaN).
   It returns
     1  if n1 > n2
     0  if n1 == n2
     -1 if n1 < n2
   The naïve code   (n1 > n2 ? 1 : n1 < n2 ? -1 : 0)  produces a conditional
   jump with nearly all GCC versions up to GCC 10.
   This variant     (n1 < n2 ? -1 : n1 > n2)  produces a conditional with many
   GCC versions up to GCC 9.
   The better code  (n1 > n2) - (n1 < n2)  from Hacker's Delight § 2-9
   avoids conditional jumps in all GCC versions >= 3.4.  */
#define _GL_CMP(n1, n2) (((n1) > (n2)) - ((n1) < (n2)))

/* Define to 'int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define as a signed integer type capable of holding a process identifier. */
/* #undef pid_t */

/* Define as the type of the result of subtracting two pointers, if the system
   doesn't define it. */
/* #undef ptrdiff_t */

/* Define to the equivalent of the C99 'restrict' keyword, or to
   nothing if this is not supported.  Do not define if restrict is
   supported only directly.  */
#define restrict __restrict__
/* Work around a bug in older versions of Sun C++, which did not
   #define __restrict__ or support _Restrict or __restrict__
   even though the corresponding Sun C compiler ended up with
   "#define restrict _Restrict" or "#define restrict __restrict__"
   in the previous line.  This workaround can be removed once
   we assume Oracle Developer Studio 12.5 (2016) or later.  */
#if defined __SUNPRO_CC && !defined __RESTRICT && !defined __restrict__
#define _Restrict
#define __restrict__
#endif

/* Hidden symbol. */
/* #undef rpl_fgetc */

/* Hidden symbol. */
/* #undef rpl_fgets */

/* Hidden symbol. */
/* #undef rpl_fprintf */

/* Hidden symbol. */
/* #undef rpl_fputc */

/* Hidden symbol. */
/* #undef rpl_fputs */

/* Hidden symbol. */
/* #undef rpl_fread */

/* Hidden symbol. */
/* #undef rpl_frexp */

/* Hidden symbol. */
/* #undef rpl_frexpl */

/* Hidden symbol. */
/* #undef rpl_fscanf */

/* Hidden symbol. */
/* #undef rpl_fwrite */

/* Hidden symbol. */
/* #undef rpl_mbrtowc */

/* Hidden symbol. */
/* #undef rpl_mbsinit */

/* Hidden symbol. */
/* #undef rpl_memchr */

/* Hidden symbol. */
/* #undef rpl_tdelete */

/* Hidden symbol. */
/* #undef rpl_tfind */

/* Hidden symbol. */
/* #undef rpl_tsearch */

/* Hidden symbol. */
/* #undef rpl_twalk */

/* Hidden symbol. */
/* #undef rpl_vfprintf */

/* Define as 'unsigned int' if <stddef.h> doesn't define. */
/* #undef size_t */

/* Define as a signed type of the same size as size_t. */
/* #undef ssize_t */

/* Hidden symbol. */
/* #undef tdelete */

/* Hidden symbol. */
/* #undef tfind */

/* Hidden symbol. */
/* #undef tsearch */

/* Hidden symbol. */
/* #undef twalk */

#define __libc_lock_t gl_lock_t
#define __libc_lock_define gl_lock_define
#define __libc_lock_define_initialized gl_lock_define_initialized
#define __libc_lock_init gl_lock_init
#define __libc_lock_lock gl_lock_lock
#define __libc_lock_unlock gl_lock_unlock
#define __libc_lock_recursive_t gl_recursive_lock_t
#define __libc_lock_define_recursive gl_recursive_lock_define
#define __libc_lock_define_initialized_recursive \
  gl_recursive_lock_define_initialized
#define __libc_lock_init_recursive gl_recursive_lock_init
#define __libc_lock_lock_recursive gl_recursive_lock_lock
#define __libc_lock_unlock_recursive gl_recursive_lock_unlock

#define asnprintf _libintl_asnprintf
#define rpl_asnprintf _libintl_asnprintf
/* Symbols defined by main intl code.  The prefix '_nl_' is used by glibc.
   For hiding the symbols on AIX and Solaris 10 with compilers that don't
   support the __visibility__ attribute,  map them to prefix '_libintl_'.  */
#define _nl_explode_name _libintl_explode_name
#define _nl_find_domain _libintl_find_domain
#define _nl_find_msg _libintl_find_msg
#define _nl_language_preferences_default _libintl_language_preferences_default
#define _nl_load_domain _libintl_load_domain
#define _nl_log_untranslated _libintl_log_untranslated
#define _nl_make_l10nflist _libintl_make_l10nflist
#define _nl_normalize_codeset _libintl_normalize_codeset
#define _nl_state_lock _libintl_state_lock
/* Symbols defined by gnulib module 'float'.  */
#define gl_LDBL_MAX _libintl_LDBL_MAX
/* Symbols defined by gnulib module 'free-posix'.  */
#define rpl_free _libintl_free
/* Symbols defined by gnulib module 'hard-locale'.  */
#define hard_locale _libintl_hard_locale
/* Symbols defined by gnulib module 'isnand-nolibm'.  */
#define rpl_isnand _libintl_isnand
/* Symbols defined by gnulib module 'isnanf-nolibm'.  */
#define rpl_isnanf _libintl_isnanf
/* Symbols defined by gnulib module 'isnanl-nolibm'.  */
#define rpl_isnanl _libintl_isnanl
/* Symbols defined by gnulib module 'localename'.  */
#define gl_locale_name_thread _libintl_locale_name_thread
#define gl_locale_name_posix _libintl_locale_name_posix
#define gl_locale_name _libintl_locale_name
/* Symbols defined by gnulib module 'localename-unsafe'.  */
#define gl_locale_name_canonicalize _libintl_locale_name_canonicalize
#define gl_locale_name_from_win32_LANGID _libintl_locale_name_from_win32_LANGID
#define gl_locale_name_from_win32_LCID _libintl_locale_name_from_win32_LCID
#define gl_locale_name_thread_unsafe _libintl_locale_name_thread_unsafe
#define gl_locale_name_posix_unsafe _libintl_locale_name_posix_unsafe
#define gl_locale_name_environ _libintl_locale_name_environ
#define gl_locale_name_default _libintl_locale_name_default
#define gl_locale_name_unsafe _libintl_locale_name_unsafe
#define rpl_newlocale _libintl_newlocale
#define rpl_duplocale _libintl_duplocale
#define rpl_freelocale _libintl_freelocale
/* Symbols defined by gnulib module 'lock'.  */
#if USE_ISOC_THREADS || USE_ISOC_AND_POSIX_THREADS
#define glthread_lock_init _libintl_lock_init
#define glthread_lock_lock _libintl_lock_lock
#define glthread_lock_unlock _libintl_lock_unlock
#define glthread_lock_destroy _libintl_lock_destroy
#define glthread_rwlock_init _libintl_rwlock_init
#define glthread_rwlock_rdlock _libintl_rwlock_rdlock
#define glthread_rwlock_wrlock _libintl_rwlock_wrlock
#define glthread_rwlock_unlock _libintl_rwlock_unlock
#define glthread_rwlock_destroy _libintl_rwlock_destroy
#define glthread_recursive_lock_init _libintl_recursive_lock_init
#define glthread_recursive_lock_lock _libintl_recursive_lock_lock
#define glthread_recursive_lock_unlock _libintl_recursive_lock_unlock
#define glthread_recursive_lock_destroy _libintl_recursive_lock_destroy
#endif
#define glthread_rwlock_init_for_glibc _libintl_rwlock_init_for_glibc
#define glthread_rwlock_init_multithreaded _libintl_rwlock_init_multithreaded
#define glthread_rwlock_rdlock_multithreaded \
  _libintl_rwlock_rdlock_multithreaded
#define glthread_rwlock_wrlock_multithreaded \
  _libintl_rwlock_wrlock_multithreaded
#define glthread_rwlock_unlock_multithreaded \
  _libintl_rwlock_unlock_multithreaded
#define glthread_rwlock_destroy_multithreaded \
  _libintl_rwlock_destroy_multithreaded
#define glthread_recursive_lock_init_multithreaded \
  _libintl_recursive_lock_init_multithreaded
#define glthread_recursive_lock_lock_multithreaded \
  _libintl_recursive_lock_lock_multithreaded
#define glthread_recursive_lock_unlock_multithreaded \
  _libintl_recursive_lock_unlock_multithreaded
#define glthread_recursive_lock_destroy_multithreaded \
  _libintl_recursive_lock_destroy_multithreaded
#define glthread_once_singlethreaded _libintl_once_singlethreaded
#define glthread_once_multithreaded _libintl_once_multithreaded
/* Symbols defined by gnulib module 'mbszero'.  */
#define mbszero _libintl_mbszero
/* Symbols defined by gnulib module 'printf-frexp'.  */
#define printf_frexp _libintl_printf_frexp
/* Symbols defined by gnulib module 'printf-frexpl'.  */
#define printf_frexpl _libintl_printf_frexpl
/* Symbols defined by gnulib module 'setlocale-null'.  */
#define setlocale_null _libintl_setlocale_null
#define setlocale_null_r _libintl_setlocale_null_r
/* Symbols defined by gnulib module 'setlocale-null-unlocked'.  */
#define setlocale_null_unlocked _libintl_setlocale_null_unlocked
#define setlocale_null_r_unlocked _libintl_setlocale_null_r_unlocked
/* Symbols defined by gnulib module 'signbit'.  */
#define gl_signbitf _libintl_signbitf
#define gl_signbitd _libintl_signbitd
#define gl_signbitl _libintl_signbitl
/* Symbols defined by gnulib module 'threadlib'.  */
#define glthread_in_use _libintl_glthread_in_use
/* Symbols defined by gnulib module 'vasnprintf'.  */
#define printf_fetchargs _libintl_printf_fetchargs
#define printf_parse _libintl_printf_parse
#define vasnprintf _libintl_vasnprintf
#define rpl_vasnprintf _libintl_vasnprintf
/* Symbols defined by gnulib module 'vasnwprintf'.  */
#define asnwprintf _libintl_asnwprintf
#define wprintf_parse _libintl_wprintf_parse
#define vasnwprintf _libintl_vasnwprintf
/* Symbols defined by gnulib module 'windows-mutex'.  */
#define glwthread_mutex_init _libintl_glwthread_mutex_init
#define glwthread_mutex_lock _libintl_glwthread_mutex_lock
#define glwthread_mutex_trylock _libintl_glwthread_mutex_trylock
#define glwthread_mutex_unlock _libintl_glwthread_mutex_unlock
#define glwthread_mutex_destroy _libintl_glwthread_mutex_destroy
/* Symbols defined by gnulib module 'windows-once'.  */
#define glwthread_once _libintl_glwthread_once
/* Symbols defined by gnulib module 'windows-recmutex'.  */
#define glwthread_recmutex_init _libintl_glwthread_recmutex_init
#define glwthread_recmutex_lock _libintl_glwthread_recmutex_lock
#define glwthread_recmutex_trylock _libintl_glwthread_recmutex_trylock
#define glwthread_recmutex_unlock _libintl_glwthread_recmutex_unlock
#define glwthread_recmutex_destroy _libintl_glwthread_recmutex_destroy
/* Symbols defined by gnulib module 'windows-rwlock'.  */
#define glwthread_rwlock_init _libintl_glwthread_rwlock_init
#define glwthread_rwlock_rdlock _libintl_glwthread_rwlock_rdlock
#define glwthread_rwlock_wrlock _libintl_glwthread_rwlock_wrlock
#define glwthread_rwlock_tryrdlock _libintl_glwthread_rwlock_tryrdlock
#define glwthread_rwlock_trywrlock _libintl_glwthread_rwlock_trywrlock
#define glwthread_rwlock_unlock _libintl_glwthread_rwlock_unlock
#define glwthread_rwlock_destroy _libintl_glwthread_rwlock_destroy
/* Symbols defined by gnulib module 'xsize'.  */
#define xmax _libintl_xmax
#define xsum _libintl_xsum
#define xsum3 _libintl_xsum3
#define xsum4 _libintl_xsum4

#if !(defined __cplusplus                                               \
          ? 1                                                           \
          : (defined __clang__                                          \
                 ? __STDC_VERSION__ >= 202000L && __clang_major__ >= 15 \
                 : (defined __GNUC__                                    \
                        ? __STDC_VERSION__ >= 202000L && __GNUC__ >= 13 \
                        : defined HAVE_C_BOOL)))
#if !defined __cplusplus && !defined __bool_true_false_are_defined
#if HAVE_STDBOOL_H
#include <stdbool.h>
#else
#if defined __SUNPRO_C
#error \
    "<stdbool.h> is not usable with this configuration. To make it usable, add -D_STDC_C99= to $CC."
#else
#error \
    "<stdbool.h> does not exist on this platform. Use gnulib module 'stdbool-c99' instead of gnulib module 'stdbool'."
#endif
#endif
#endif
#if !true
#define true (!false)
#endif
#endif

#if (!(defined __clang__                                                    \
           ? (defined __cplusplus                                           \
                  ? __cplusplus >= 201703L                                  \
                  : __STDC_VERSION__ >= 202000L && __clang_major__ >= 16 && \
                        !defined __sun)                                     \
           : (defined __GNUC__                                              \
                  ? (defined __cplusplus                                    \
                         ? __cplusplus >= 201103L && __GNUG__ >= 6          \
                         : __STDC_VERSION__ >= 202000L && __GNUC__ >= 13 && \
                               !defined __sun)                              \
                  : defined HAVE_C_STATIC_ASSERT)) &&                       \
     !defined assert &&                                                     \
     (!defined __cplusplus ||                                               \
      (__cpp_static_assert < 201411 && __GNUG__ < 6 && __clang_major__ < 6)))
#include <assert.h>
#undef /**/ assert
#ifdef __sgi
#undef /**/ __ASSERT_H__
#endif
/* Solaris 11.4 <assert.h> defines static_assert as a macro with 2 arguments.
   We need it also to be invocable with a single argument.
   Haiku 2022 <assert.h> does not define static_assert at all.  */
#if (__STDC_VERSION__ - 0 >= 201112L) && !defined __cplusplus
#undef /**/ static_assert
#define static_assert _Static_assert
#endif
#endif

/* Tweak gnulib code according to the needs of this library.  */
#define IN_LIBINTL 1

/* On Windows, variables that may be in a DLL must be marked specially.
   The symbols marked with DLL_VARIABLE should be exported if and only if the
   object file gets included in a DLL.  Libtool, on Windows platforms, defines
   the C macro DLL_EXPORT (together with PIC) when compiling for a shared
   library (called DLL under Windows) and does not define it when compiling
   an object file meant to be linked statically into some executable.  */
#if (defined _MSC_VER && defined DLL_EXPORT) && !defined IN_RELOCWRAPPER
#define DLL_VARIABLE __declspec(dllimport)
#else
#define DLL_VARIABLE
#endif

/* Extra OS/2 (emx+gcc) defines.  */
#if defined __EMX__ && !defined __KLIBC__
#include "os2compat.h"
#endif

/* ADDED FOR GHOSTTY. This is needed to ensure that all gnulib-lib
 * source files have the locale_t type and all the LC_ constants.
 * It looks like some files do this and some don't so we just put it
 * here to ensure we always have it. This surely can't be the best or
 * correct way to do this but it works.
 *
 * When upgrading gettext we just need to be careful about this. This
 * is noted in the build.zig as well.
 */
#if HAVE_GOOD_USELOCALE
/* Mac OS X 10.5 defines the locale_t type in <xlocale.h>.  */
#if defined __APPLE__ && defined __MACH__
#include <xlocale.h>
#endif
#endif
