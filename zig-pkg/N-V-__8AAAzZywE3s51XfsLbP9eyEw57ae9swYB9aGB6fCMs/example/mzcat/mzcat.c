// Copyright 2022 The Wuffs Authors.
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
mzcat decompresses stdin to stdout. It is similar to the standard /bin/bzcat,
/bin/lzcat or /bin/zcat programs but the single program speaks multiple file
formats (listed below). On Linux, it also self-imposes a SECCOMP_MODE_STRICT
sandbox. To run:

$CC mzcat.c && ./a.out < ../../test/data/romeo.txt.bz2; rm -f a.out

for a C compiler $CC, such as clang or gcc.

Supported compression formats:
- bzip2
- gzip
- lzip
- lzma
- xz
- zlib
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
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__BZIP2
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__CRC64
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__GZIP
#define WUFFS_CONFIG__MODULE__LZIP
#define WUFFS_CONFIG__MODULE__LZMA
#define WUFFS_CONFIG__MODULE__SHA256
#define WUFFS_CONFIG__MODULE__XZ
#define WUFFS_CONFIG__MODULE__ZLIB

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
#define DST_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#endif

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#endif

// 80 MiB is big enough to decode files encoded with "xz -9".
#ifndef WORKBUF_ARRAY_SIZE
#define WORKBUF_ARRAY_SIZE (80 * 1024 * 1024)
#endif

uint8_t g_dst_buffer_array[DST_BUFFER_ARRAY_SIZE];
uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE];
uint8_t g_workbuf_array[WORKBUF_ARRAY_SIZE];

// ----

static bool g_sandboxed = false;

int32_t g_fourcc = 0;

wuffs_base__io_transformer* g_io_transformer = NULL;
union {
  wuffs_bzip2__decoder bzip2;
  wuffs_gzip__decoder gzip;
  wuffs_lzip__decoder lzip;
  wuffs_lzma__decoder lzma;
  wuffs_xz__decoder xz;
  wuffs_zlib__decoder zlib;
} g_potential_decoders;

wuffs_crc32__ieee_hasher g_digest_hasher;

// ----

struct {
  int remaining_argc;
  char** remaining_argv;

  bool fail_if_unsandboxed;
  bool ignore_checksum;
  bool output_crc32_digest;
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
    } else if (!strcmp(arg, "output-crc32-digest")) {
      g_flags.output_crc32_digest = true;
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
initialize_io_transformer(uint8_t input_first_byte) {
  wuffs_base__status status =
      wuffs_base__make_status("main: unrecognized input compression format");
  wuffs_base__io_transformer* io_transformer = NULL;

  switch (input_first_byte) {
    case 0x1F:
      status = wuffs_gzip__decoder__initialize(
          &g_potential_decoders.gzip, sizeof g_potential_decoders.gzip,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      io_transformer =
          wuffs_gzip__decoder__upcast_as__wuffs_base__io_transformer(
              &g_potential_decoders.gzip);
      break;

    case 0x42:
      status = wuffs_bzip2__decoder__initialize(
          &g_potential_decoders.bzip2, sizeof g_potential_decoders.bzip2,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      io_transformer =
          wuffs_bzip2__decoder__upcast_as__wuffs_base__io_transformer(
              &g_potential_decoders.bzip2);
      break;

    case 0x4C:
      status = wuffs_lzip__decoder__initialize(
          &g_potential_decoders.lzip, sizeof g_potential_decoders.lzip,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      io_transformer =
          wuffs_lzip__decoder__upcast_as__wuffs_base__io_transformer(
              &g_potential_decoders.lzip);
      break;

    case 0x5D:
      status = wuffs_lzma__decoder__initialize(
          &g_potential_decoders.lzma, sizeof g_potential_decoders.lzma,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      io_transformer =
          wuffs_lzma__decoder__upcast_as__wuffs_base__io_transformer(
              &g_potential_decoders.lzma);
      break;

    case 0x78:
      status = wuffs_zlib__decoder__initialize(
          &g_potential_decoders.zlib, sizeof g_potential_decoders.zlib,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      io_transformer =
          wuffs_zlib__decoder__upcast_as__wuffs_base__io_transformer(
              &g_potential_decoders.zlib);
      break;

    case 0xFD:
      status = wuffs_xz__decoder__initialize(
          &g_potential_decoders.xz, sizeof g_potential_decoders.xz,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      io_transformer = wuffs_xz__decoder__upcast_as__wuffs_base__io_transformer(
          &g_potential_decoders.xz);
      wuffs_base__io_transformer__set_quirk(
          io_transformer,
          WUFFS_XZ__QUIRK_DECODE_STANDALONE_CONCATENATED_STREAMS, 1);
      break;
  }

  if (g_flags.ignore_checksum) {
    wuffs_base__io_transformer__set_quirk(io_transformer,
                                          WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, 1);
  }

  if (!wuffs_base__status__is_ok(&status)) {
    return wuffs_base__status__message(&status);
  }
  g_io_transformer = io_transformer;
  return NULL;
}

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

    if (g_io_transformer) {
      // No-op.
    } else if (src.meta.ri == src.meta.wi) {
      return "main: invalid empty input";
    } else {
      const char* z = initialize_io_transformer(src.data.ptr[src.meta.ri]);
      if (z) {
        return z;
      } else if (g_flags.output_crc32_digest) {
        wuffs_base__status status = wuffs_crc32__ieee_hasher__initialize(
            &g_digest_hasher, sizeof g_digest_hasher, WUFFS_VERSION,
            WUFFS_INITIALIZE__DEFAULT_OPTIONS);
        if (status.repr) {
          return wuffs_base__status__message(&status);
        }
      }
    }

    while (true) {
      wuffs_base__status status = wuffs_base__io_transformer__transform_io(
          g_io_transformer, &dst, &src,
          wuffs_base__make_slice_u8(&g_workbuf_array[0],
                                    sizeof(g_workbuf_array)));

      if (dst.meta.ri < dst.meta.wi) {
        if (g_flags.output_crc32_digest) {
          wuffs_crc32__ieee_hasher__update(
              &g_digest_hasher, wuffs_base__io_buffer__reader_slice(&dst));
        } else {
          // TODO: handle EINTR and other write errors; see "man 2 write".
          const int stdout_fd = 1;
          ignore_return_value(write(stdout_fd, g_dst_buffer_array + dst.meta.ri,
                                    dst.meta.wi - dst.meta.ri));
        }
        dst.meta.ri = dst.meta.wi;
        wuffs_base__optional_u63 hrl =
            wuffs_base__io_transformer__dst_history_retain_length(
                g_io_transformer);
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

void  //
print_crc32_digest(bool bad) {
  const char* hex = "0123456789abcdef";
  uint32_t hash = wuffs_crc32__ieee_hasher__checksum_u32(&g_digest_hasher);
  char buf[13];
  memcpy(buf + 0, bad ? "BAD " : "OK. ", 4);
  for (int i = 0; i < 8; i++) {
    buf[4 + i] = hex[hash >> 28];
    hash <<= 4;
  }
  buf[12] = '\n';
  const int stdout_fd = 1;
  ignore_return_value(write(stdout_fd, buf, 13));
}

int  //
main(int argc, char** argv) {
#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
  g_sandboxed = true;
#endif

  int exit_code = compute_exit_code(main1(argc, argv));
  if (g_flags.output_crc32_digest) {
    print_crc32_digest(exit_code != 0);
  }

#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  // Call SYS_exit explicitly, instead of calling SYS_exit_group implicitly by
  // either calling _exit or returning from main. SECCOMP_MODE_STRICT allows
  // only SYS_exit.
  syscall(SYS_exit, exit_code);
#endif
  return exit_code;
}
