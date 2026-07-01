// This recreates parts of the generated config.h from cmake. Most of the
// defines actually happen directly in libxml2.zig. Some of these SHOULD
// be converted to build-time determined.

/* Whether struct sockaddr::__ss_family exists */
// #define HAVE_BROKEN_SS_FAMILY 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Have dlopen based dso */
#define HAVE_DLOPEN 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `ftime' function. */
#define HAVE_FTIME 1

/* Define if getaddrinfo is there */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `isascii' function. */
#define HAVE_ISASCII 1

/* Define to 1 if you have the `mmap' function. */
#define HAVE_MMAP 1

/* Define to 1 if you have the `munmap' function. */
#define HAVE_MUNMAP 1

/* mmap() is no good without munmap() */
#if defined(HAVE_MMAP) && !defined(HAVE_MUNMAP)
#  undef /**/ HAVE_MMAP
#endif

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if you have the `rand_r' function. */
#define HAVE_RAND_R 1

/* Define to 1 if you have the <resolv.h> header file. */
#define HAVE_RESOLV_H 1

/* Define to 1 if you have the `stat' function. */
#define HAVE_STAT 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Whether va_copy() is available */
#define HAVE_VA_COPY 1

/* Define to 1 if you have the <zlib.h> header file. */
#define HAVE_ZLIB_H 1

/* Whether __va_copy() is available */
#define HAVE___VA_COPY 1

/* Support for IPv6 */
#define SUPPORT_IP6 1

/* Define if va_list is an array type */
#define VA_LIST_IS_ARRAY 1
