// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// process-json-numbers.c processes all the numbers in the JSON-formatted data
// read from stdin. It succeeds (with exit code 0) if the input is valid JSON
// and all of the numbers within were processed without error.
//
// Without further flags, processing is a no-op and the program only verifies
// the JSON structure.
//
// Pass -e (--emit-number-str) to emit each number (as a string) on its own
// line.
//
// Pass -p (--parse-number-f64) to call wuffs_base__parse_number_f64 on each
// number. Timing this program with and without this flag gives a rough measure
// of how much time is spent solely in wuffs_base__parse_number_f64.
//
// Pass -r (--render-number-f64) to call wuffs_base__render_number_f64 (with
// WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION) on each number. Timing
// this program with and without this flag gives a rough measure of how much
// time is spent solely in wuffs_base__render_number_f64.
//
// The -r flag is ignored unless -p is also passed.
//
// This program's purpose is to benchmark the wuffs_base__etc_f64 functions.
// It's not about JSON per se, but JSON files are a source of realistic
// floating point numbers.

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
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
#define WUFFS_CONFIG__MODULE__JSON

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../release/c/wuffs-unsupported-snapshot.c"

// Uncomment this to use the github.com/lemire/fast_double_parser library. This
// header-only library is C++, not C.
// #define USE_LEMIRE_FAST_DOUBLE_PARSER

#ifdef USE_LEMIRE_FAST_DOUBLE_PARSER
#include "/the/path/to/fast_double_parser/include/fast_double_parser.h"
#endif

// Wuffs allows either statically or dynamically allocated work buffers. This
// program exercises static allocation.
#define WORK_BUFFER_ARRAY_SIZE \
  WUFFS_JSON__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
#if WORK_BUFFER_ARRAY_SIZE > 0
uint8_t g_work_buffer_array[WORK_BUFFER_ARRAY_SIZE];
#else
// Not all C/C++ compilers support 0-length arrays.
uint8_t g_work_buffer_array[1];
#endif

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#endif
#ifndef TOKEN_BUFFER_ARRAY_SIZE
#define TOKEN_BUFFER_ARRAY_SIZE (128 * 1024)
#endif

uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE];
wuffs_base__token g_tok_buffer_array[TOKEN_BUFFER_ARRAY_SIZE];

wuffs_base__io_buffer g_src;
wuffs_base__token_buffer g_tok;

wuffs_json__decoder g_dec;

#define TRY(error_msg)         \
  do {                         \
    const char* z = error_msg; \
    if (z) {                   \
      return z;                \
    }                          \
  } while (false)

// ignore_return_value suppresses errors from -Wall -Werror.
static void  //
ignore_return_value(int ignored) {}

const char*  //
read_src() {
  if (g_src.meta.closed) {
    return "main: internal error: read requested on a closed source";
  }
  wuffs_base__io_buffer__compact(&g_src);
  if (g_src.meta.wi >= g_src.data.len) {
    return "main: g_src buffer is full";
  }
  size_t n = fread(g_src.data.ptr + g_src.meta.wi, sizeof(uint8_t),
                   g_src.data.len - g_src.meta.wi, stdin);
  g_src.meta.wi += n;
  g_src.meta.closed = feof(stdin);
  if ((n == 0) && !g_src.meta.closed) {
    return "main: read error";
  }
  return NULL;
}

// ----

struct {
  int remaining_argc;
  char** remaining_argv;

  bool emit_number_str;
  bool parse_number_f64;
  bool render_number_f64;
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

    if (!strcmp(arg, "e") || !strcmp(arg, "emit-number-str")) {
      g_flags.emit_number_str = true;
      continue;
    }
    if (!strcmp(arg, "p") || !strcmp(arg, "parse-number-f64")) {
      g_flags.parse_number_f64 = true;
      continue;
    }
    if (!strcmp(arg, "r") || !strcmp(arg, "render-number-f64")) {
      g_flags.render_number_f64 = true;
      continue;
    }

    return "main: unrecognized flag argument";
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return NULL;
}

const char*  //
main1(int argc, char** argv) {
  TRY(parse_flags(argc, argv));
  if (g_flags.remaining_argc > 0) {
    return "main: bad argument: use \"program < input\", not \"program input\"";
  }

  uint8_t new_line[1];
  new_line[0] = '\n';

  g_src = wuffs_base__make_io_buffer(
      wuffs_base__make_slice_u8(g_src_buffer_array, SRC_BUFFER_ARRAY_SIZE),
      wuffs_base__empty_io_buffer_meta());

  g_tok = wuffs_base__make_token_buffer(
      wuffs_base__make_slice_token(g_tok_buffer_array, TOKEN_BUFFER_ARRAY_SIZE),
      wuffs_base__empty_token_buffer_meta());

  wuffs_base__status init_status = wuffs_json__decoder__initialize(
      &g_dec, sizeof__wuffs_json__decoder(), WUFFS_VERSION, 0);
  if (!wuffs_base__status__is_ok(&init_status)) {
    return wuffs_base__status__message(&init_status);
  }

  uint64_t pos = 0;
  while (true) {
    wuffs_base__status status = wuffs_json__decoder__decode_tokens(
        &g_dec, &g_tok, &g_src,
        wuffs_base__make_slice_u8(g_work_buffer_array, WORK_BUFFER_ARRAY_SIZE));

    while (g_tok.meta.ri < g_tok.meta.wi) {
      wuffs_base__token* t = &g_tok.data.ptr[g_tok.meta.ri++];
      uint64_t len = wuffs_base__token__length(t);

      if (wuffs_base__token__value_base_category(t) ==
          WUFFS_BASE__TOKEN__VBC__NUMBER) {
        uint64_t buf_pos = pos - g_src.meta.pos;
        uint64_t buf_len = g_src.data.len;
        if ((buf_len < buf_pos) || ((buf_len - buf_pos) < len)) {
          return "main: internal error: inconsistent token position/length";
        }

        if (g_flags.emit_number_str) {
          const int stdout_fd = 1;
          ignore_return_value(write(stdout_fd, &g_src.data.ptr[buf_pos], len));
          ignore_return_value(write(stdout_fd, &new_line[0], 1));
        }

        if (g_flags.parse_number_f64) {
          wuffs_base__result_f64 r;

#ifdef USE_LEMIRE_FAST_DOUBLE_PARSER
          // Wuffs (and its JSON parser) works with slices (pointer-length
          // pairs) but fast_double_parser works with NUL-terminated strings.
          char buf[1024];
          if (len > 1023) {
            return "main: number-as-string is too long";
          }
          memcpy(&buf[0], &g_src.data.ptr[buf_pos], len);
          buf[len] = 0;
          if (!fast_double_parser::decimal_separator_dot::parse_number(
                  &buf[0], &r.value)) {
            return "main: could not parse number";
          }
          r.status = wuffs_base__make_status(NULL);
#else
          r = wuffs_base__parse_number_f64(
              wuffs_base__make_slice_u8(&g_src.data.ptr[buf_pos], len),
              WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
          if (!wuffs_base__status__is_ok(&r.status)) {
            return wuffs_base__status__message(&r.status);
          }
#endif

          if (g_flags.render_number_f64) {
            uint8_t render_buffer[2048];
            size_t n = wuffs_base__render_number_f64(
                wuffs_base__make_slice_u8(&render_buffer[0], 2048), r.value, 0,
                WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION);
            if (n == 0) {
              return "main: internal error: couldn't render_number_f64";
            }
          }
        }
      }

      pos += len;
      if (0 > ((int64_t)pos)) {
        return "main: input is too long";
      }
    }

    if (status.repr == NULL) {
      return NULL;
    } else if (status.repr == wuffs_base__suspension__short_read) {
      TRY(read_src());
    } else if (status.repr == wuffs_base__suspension__short_write) {
      wuffs_base__token_buffer__compact(&g_tok);
    } else {
      return wuffs_base__status__message(&status);
    }
  }
}

// ----

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
  fprintf(stderr, "%s\n", status_msg);
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
  const char* z = main1(argc, argv);
  int exit_code = compute_exit_code(z);
  return exit_code;
}
