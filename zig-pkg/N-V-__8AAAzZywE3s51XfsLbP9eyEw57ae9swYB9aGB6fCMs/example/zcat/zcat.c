// Copyright 2018 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

/*
zcat decodes gzip'ed data to stdout. It is similar to the standard /bin/zcat
program, except that this example program only reads from stdin. On Linux, it
also self-imposes a SECCOMP_MODE_STRICT sandbox. To run:

$CC zcat.c && ./a.out < ../../test/data/romeo.txt.gz; rm -f a.out

for a C compiler $CC, such as clang or gcc.
*/

#include <errno.h>
#include <unistd.h>

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__STATIC_FUNCTIONS macro is optional, but when
// combined with WUFFS_IMPLEMENTATION, it demonstrates making all of Wuffs'
// functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#define WUFFS_CONFIG__STATIC_FUNCTIONS

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__GZIP

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

#if defined(__linux__)
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#define WUFFS_EXAMPLE_USE_SECCOMP
#endif

#ifndef DST_BUFFER_ARRAY_SIZE
#define DST_BUFFER_ARRAY_SIZE (128 * 1024)
#endif

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (128 * 1024)
#endif

#define WORK_BUFFER_ARRAY_SIZE \
  WUFFS_GZIP__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE

uint8_t g_dst_buffer_array[DST_BUFFER_ARRAY_SIZE];
uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE];
#if WORK_BUFFER_ARRAY_SIZE > 0
uint8_t g_work_buffer_array[WORK_BUFFER_ARRAY_SIZE];
#else
// Not all C/C++ compilers support 0-length arrays.
uint8_t g_work_buffer_array[1];
#endif

// ----

static bool g_sandboxed = false;

struct {
  int remaining_argc;
  char** remaining_argv;

  bool fail_if_unsandboxed;
  bool ignore_checksum;
} g_flags = {0};

const char*  //
parse_flags(int argc, char** argv) {
  int c = (argc > 0) ? 1 : 0;  // Skip argv[0], the program name.
  for (; c < argc; c++) {
    char* arg = argv[c];
    if (*arg++ != '-') {
      break;
    }

    // A double-dash "--foo" is equivalent to a single-dash "-foo". As special
    // cases, a bare "-" is not a flag (some programs may interpret it as
    // stdin) and a bare "--" means to stop parsing flags.
    if (*arg == '\x00') {
      break;
    } else if (*arg == '-') {
      arg++;
      if (*arg == '\x00') {
        c++;
        break;
      }
    }

    if (!strcmp(arg, "fail-if-unsandboxed")) {
      g_flags.fail_if_unsandboxed = true;
      continue;
    } else if (!strcmp(arg, "ignore-checksum")) {
      g_flags.ignore_checksum = true;
      continue;
    }

    return "main: unrecognized flag argument";
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return NULL;
}

// ----

// ignore_return_value suppresses errors from -Wall -Werror.
static void  //
ignore_return_value(int ignored) {}

const char*  //
main1(int argc, char** argv) {
  const char* z = parse_flags(argc, argv);
  if (z) {
    return z;
  } else if (g_flags.remaining_argc > 0) {
    return "main: bad argument: use \"program < input\", not \"program input\"";
  } else if (g_flags.fail_if_unsandboxed && !g_sandboxed) {
    return "main: unsandboxed";
  }

  wuffs_gzip__decoder dec;
  wuffs_base__status status =
      wuffs_gzip__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION, 0);
  if (!wuffs_base__status__is_ok(&status)) {
    return wuffs_base__status__message(&status);
  }

  if (g_flags.ignore_checksum) {
    wuffs_gzip__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, 1);
  }

  wuffs_base__io_buffer dst;
  dst.data.ptr = g_dst_buffer_array;
  dst.data.len = DST_BUFFER_ARRAY_SIZE;
  dst.meta.wi = 0;
  dst.meta.ri = 0;
  dst.meta.pos = 0;
  dst.meta.closed = false;

  wuffs_base__io_buffer src;
  src.data.ptr = g_src_buffer_array;
  src.data.len = SRC_BUFFER_ARRAY_SIZE;
  src.meta.wi = 0;
  src.meta.ri = 0;
  src.meta.pos = 0;
  src.meta.closed = false;

  while (true) {
    const int stdin_fd = 0;
    ssize_t n =
        read(stdin_fd, src.data.ptr + src.meta.wi, src.data.len - src.meta.wi);
    if (n < 0) {
      if (errno != EINTR) {
        return strerror(errno);
      }
      continue;
    }
    src.meta.wi += n;
    if (n == 0) {
      src.meta.closed = true;
    }

    while (true) {
      status = wuffs_gzip__decoder__transform_io(
          &dec, &dst, &src,
          wuffs_base__make_slice_u8(g_work_buffer_array,
                                    WORK_BUFFER_ARRAY_SIZE));

      if (dst.meta.ri < dst.meta.wi) {
        // TODO: handle EINTR and other write errors; see "man 2 write".
        const int stdout_fd = 1;
        ignore_return_value(write(stdout_fd, g_dst_buffer_array + dst.meta.ri,
                                  dst.meta.wi - dst.meta.ri));
        dst.meta.ri = dst.meta.wi;

        wuffs_base__optional_u63 hrl =
            wuffs_gzip__decoder__dst_history_retain_length(&dec);
        wuffs_base__io_buffer__compact_retaining(
            &dst, wuffs_base__optional_u63__value_or(&hrl, UINT64_MAX));
        if (dst.meta.wi == dst.data.len) {
          return "main: unsupported history length (a.k.a. dictionary size)";
        }
      }

      if (status.repr == wuffs_base__suspension__short_read) {
        break;
      }
      if (status.repr == wuffs_base__suspension__short_write) {
        continue;
      }
      return wuffs_base__status__message(&status);
    }

    wuffs_base__io_buffer__compact(&src);
  }
}

int  //
compute_exit_code(const char* status_msg) {
  if (!status_msg) {
    return 0;
  }
  size_t n = strnlen(status_msg, 2047);
  if (n >= 2047) {
    status_msg = "main: internal error: error message is too long";
    n = strlen(status_msg);
  }
  const int stderr_fd = 2;
  ignore_return_value(write(stderr_fd, status_msg, n));
  ignore_return_value(write(stderr_fd, "\n", 1));
  // Return an exit code of 1 for regular (foreseen) errors, e.g. badly
  // formatted or unsupported input.
  //
  // Return an exit code of 2 for internal (exceptional) errors, e.g. defensive
  // run-time checks found that an internal invariant did not hold.
  //
  // Automated testing, including badly formatted inputs, can therefore
  // discriminate between expected failure (exit code 1) and unexpected failure
  // (other non-zero exit codes). Specifically, exit code 2 for internal
  // invariant violation, exit code 139 (which is 128 + SIGSEGV on x86_64
  // linux) for a segmentation fault (e.g. null pointer dereference).
  return strstr(status_msg, "internal error:") ? 2 : 1;
}

int  //
main(int argc, char** argv) {
#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
  g_sandboxed = true;
#endif

  int exit_code = compute_exit_code(main1(argc, argv));

#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  // Call SYS_exit explicitly, instead of calling SYS_exit_group implicitly by
  // either calling _exit or returning from main. SECCOMP_MODE_STRICT allows
  // only SYS_exit.
  syscall(SYS_exit, exit_code);
#endif
  return exit_code;
}
