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

/*
json-to-cbor reads UTF-8 JSON (a text format) from stdin and writes the
equivalent CBOR (a binary format) to stdout.

See the "const char* g_usage" string below for details.

----

To run:

$CXX json-to-cbor.cc && ./a.out < ../../test/data/github-tags.json; rm -f a.out

for a C++ compiler $CXX, such as clang++ or g++.
*/

#if defined(__cplusplus) && (__cplusplus < 201103L)
#error "This C++ program requires -std=c++11 or later"
#endif

#include <stdio.h>

#include <string>
#include <vector>

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
#define WUFFS_CONFIG__MODULE__AUX__BASE
#define WUFFS_CONFIG__MODULE__AUX__JSON
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__JSON

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C++ file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

#define TRY(error_msg)         \
  do {                         \
    std::string z = error_msg; \
    if (!z.empty()) {          \
      return z;                \
    }                          \
  } while (false)

static const char* g_usage =
    "Usage: json-to-cbor -flags input.json\n"
    "\n"
    "Flags:\n"
    "            -input-allow-comments\n"
    "            -input-allow-extra-comma\n"
    "            -input-allow-inf-nan-numbers\n"
    "            -input-jwcc\n"
    "            -jwcc\n"
    "\n"
    "The input.json filename is optional. If absent, it reads from stdin.\n"
    "\n"
    "----\n"
    "\n"
    "json-to-cbor reads UTF-8 JSON (a text format) from stdin and writes the\n"
    "equivalent CBOR (a binary format) to stdout.\n"
    "\n"
    "The conversion may be lossy. For example, \"0.99999999999999999\" and\n"
    "\"1.0\" are (technically) different JSON values, but they are converted\n"
    "to the same CBOR bytes: F9 3C 00. Similarly, integer values outside ±M\n"
    "may lose precision, where M is ((1<<53)-1), also known as JavaScript's\n"
    "Number.MAX_SAFE_INTEGER.\n"
    "\n"
    "The CBOR output is not canonicalized in the RFC 7049 Section 3.9 sense.\n"
    "Map keys are not guaranteed to be sorted or de-duplicated.\n"
    "\n"
    "----\n"
    "\n"
    "The -input-allow-comments flag allows \"/*slash-star*/\" and\n"
    "\"//slash-slash\" C-style comments within JSON input.\n"
    "\n"
    "The -input-allow-extra-comma flag allows input like \"[1,2,]\", with a\n"
    "comma after the final element of a JSON list or dictionary.\n"
    "\n"
    "The -input-allow-inf-nan-numbers flag allows non-finite floating point\n"
    "numbers (infinities and not-a-numbers) within JSON input.\n"
    "\n"
    "Combining some of those flags results in speaking JWCC (JSON With Commas\n"
    "and Comments), not plain JSON. For convenience, the -input-jwcc or -jwcc\n"
    "flags enables all of:\n"
    "            -input-allow-comments\n"
    "            -input-allow-extra-comma\n"
    "\n"
#if defined(WUFFS_EXAMPLE_SPEAK_JWCC_NOT_JSON)
    "This program was configured at compile time to always use -jwcc.\n"
    "\n"
#endif
    "----\n"
    "\n"
    "The JSON specification permits implementations to set their own maximum\n"
    "input depth. This JSON implementation sets it to 1024.";

// ----

#ifndef DST_BUFFER_ARRAY_SIZE
#define DST_BUFFER_ARRAY_SIZE (32 * 1024)
#endif

uint8_t g_dst_array[DST_BUFFER_ARRAY_SIZE];
wuffs_base__io_buffer g_dst;

std::vector<wuffs_aux::QuirkKeyValuePair> g_quirks;

struct {
  int remaining_argc;
  char** remaining_argv;
} g_flags = {0};

std::string  //
parse_flags(int argc, char** argv) {
#if defined(WUFFS_EXAMPLE_SPEAK_JWCC_NOT_JSON)
  g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK, 1});
  g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE, 1});
  g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_EXTRA_COMMA, 1});
#endif

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

    if (!strcmp(arg, "input-allow-comments")) {
      g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK, 1});
      g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE, 1});
      continue;
    }
    if (!strcmp(arg, "input-allow-extra-comma")) {
      g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_EXTRA_COMMA, 1});
      continue;
    }
    if (!strcmp(arg, "input-allow-inf-nan-numbers")) {
      g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_INF_NAN_NUMBERS, 1});
      continue;
    }
    if (!strcmp(arg, "input-jwcc") || !strcmp(arg, "jwcc")) {
      g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK, 1});
      g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE, 1});
      g_quirks.push_back({WUFFS_JSON__QUIRK_ALLOW_EXTRA_COMMA, 1});
      continue;
    }

    return g_usage;
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return "";
}

// ----

std::string  //
flush_dst() {
  while (true) {
    size_t n = g_dst.reader_length();
    if (n == 0) {
      break;
    }
    size_t i = fwrite(g_dst.reader_pointer(), 1, n, stdout);
    g_dst.meta.ri += i;
    if (i < n) {
      return "main: error writing to stdout";
    }
  }
  g_dst.compact();
  return "";
}

std::string  //
write_dst_slow(const void* s, size_t n) {
  const uint8_t* p = static_cast<const uint8_t*>(s);
  while (n > 0) {
    size_t i = g_dst.writer_length();
    if (i == 0) {
      TRY(flush_dst());
      i = g_dst.writer_length();
      if (i == 0) {
        return "main: g_dst buffer is full";
      }
    }

    if (i > n) {
      i = n;
    }
    memcpy(g_dst.data.ptr + g_dst.meta.wi, p, i);
    g_dst.meta.wi += i;
    p += i;
    n -= i;
  }
  return "";
}

inline std::string  //
write_dst(const void* s, size_t n) {
  if (n <= (DST_BUFFER_ARRAY_SIZE - g_dst.meta.wi)) {
    memcpy(g_dst.data.ptr + g_dst.meta.wi, s, n);
    g_dst.meta.wi += n;
    return "";
  }
  return write_dst_slow(s, n);
}

// ----

class Callbacks : public wuffs_aux::DecodeJsonCallbacks {
 public:
  Callbacks() = default;

  std::string Append(uint64_t n, uint8_t base) {
    uint8_t c[9];
    if (n < 0x18) {
      c[0] = base | static_cast<uint8_t>(n);
      return write_dst(&c[0], 1);
    } else if (n <= 0xFF) {
      c[0] = base | 0x18;
      c[1] = static_cast<uint8_t>(n);
      return write_dst(&c[0], 2);
    } else if (n <= 0xFFFF) {
      c[0] = base | 0x19;
      wuffs_base__poke_u16be__no_bounds_check(&c[1], static_cast<uint16_t>(n));
      return write_dst(&c[0], 3);
    } else if (n <= 0xFFFFFFFF) {
      c[0] = base | 0x1A;
      wuffs_base__poke_u32be__no_bounds_check(&c[1], static_cast<uint32_t>(n));
      return write_dst(&c[0], 5);
    }
    c[0] = base | 0x1B;
    wuffs_base__poke_u64be__no_bounds_check(&c[1], n);
    return write_dst(&c[0], 9);
  }

  std::string AppendNull() override { return write_dst("\xF6", 1); }

  std::string AppendBool(bool val) override {
    return write_dst(val ? "\xF5" : "\xF4", 1);
  }

  std::string AppendF64(double val) override {
    uint8_t c[9];
    wuffs_base__lossy_value_u16 lv16 =
        wuffs_base__ieee_754_bit_representation__from_f64_to_u16_truncate(val);
    if (!lv16.lossy) {
      c[0] = 0xF9;
      wuffs_base__poke_u16be__no_bounds_check(&c[1], lv16.value);
      return write_dst(&c[0], 3);
    }
    wuffs_base__lossy_value_u32 lv32 =
        wuffs_base__ieee_754_bit_representation__from_f64_to_u32_truncate(val);
    if (!lv32.lossy) {
      c[0] = 0xFA;
      wuffs_base__poke_u32be__no_bounds_check(&c[1], lv32.value);
      return write_dst(&c[0], 5);
    }
    c[0] = 0xFB;
    wuffs_base__poke_u64be__no_bounds_check(
        &c[1], wuffs_base__ieee_754_bit_representation__from_f64_to_u64(val));
    return write_dst(&c[0], 9);
  }

  std::string AppendI64(int64_t val) override {
    return (val >= 0) ? Append(static_cast<uint64_t>(val), 0x00)
                      : Append(static_cast<uint64_t>(-(val + 1)), 0x20);
  }

  std::string AppendTextString(std::string&& val) override {
    TRY(Append(val.size(), 0x60));
    return write_dst(val.data(), val.size());
  }

  std::string Push(uint32_t flags) override {
    return write_dst(
        (flags & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST) ? "\x9F" : "\xBF",
        1);
  }

  std::string Pop(uint32_t flags) override { return write_dst("\xFF", 1); }
};

// ----

std::string  //
main1(int argc, char** argv) {
  g_dst = wuffs_base__ptr_u8__writer(&g_dst_array[0], DST_BUFFER_ARRAY_SIZE);

  TRY(parse_flags(argc, argv));

  FILE* in = stdin;
  if (g_flags.remaining_argc > 1) {
    return g_usage;
  } else if (g_flags.remaining_argc == 1) {
    in = fopen(g_flags.remaining_argv[0], "rb");
    if (!in) {
      return std::string("main: cannot read input file");
    }
  }

  Callbacks callbacks;
  wuffs_aux::sync_io::FileInput input(in);
  return wuffs_aux::DecodeJson(
             callbacks, input,
             wuffs_aux::DecodeJsonArgQuirks(g_quirks.data(), g_quirks.size()))
      .error_message;
}

// ----

int  //
compute_exit_code(std::string status_msg) {
  if (status_msg.empty()) {
    return 0;
  }
  fputs(status_msg.c_str(), stderr);
  fputc('\n', stderr);
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
  size_t i = status_msg.find('=');
  if (i != std::string::npos) {
    status_msg = status_msg.substr(0, i);
  }
  return (status_msg.find("internal error:") != std::string::npos) ? 2 : 1;
}

int  //
main(int argc, char** argv) {
  std::string z1 = main1(argc, argv);
  std::string z2 = flush_dst();
  int exit_code = compute_exit_code(z1.empty() ? z2 : z1);
  return exit_code;
}
