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

// manual-test-parse-number-f64.c tests Wuffs' parse_number_f64 function. The
// https://github.com/nigeltao/parse-number-fxx-test-data repository contains
// the data files, containing one test case per line, like:
//
// 3C00 3F800000 3FF0000000000000 1
// 3D00 3FA00000 3FF4000000000000 1.25
// 3D9A 3FB33333 3FF6666666666666 1.4
// 57B7 42F6E979 405EDD2F1A9FBE77 123.456
// 622A 44454000 4088A80000000000 789
// 7C00 7F800000 7FF0000000000000 123.456e789

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

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

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C++ file.
#include "../release/c/wuffs-unsupported-snapshot.c"

// Uncomment one or all of these to use the
//  - github.com/lemire/fast_double_parser
//  - github.com/lemire/fast_float
// libraries. These header-only libraries are C++, not C.
//
// #define USE_LEMIRE_FAST_DOUBLE_PARSER
// #define USE_LEMIRE_FAST_FLOAT

#ifdef USE_LEMIRE_FAST_DOUBLE_PARSER
#include "/the/path/to/fast_double_parser/include/fast_double_parser.h"
#endif
#ifdef USE_LEMIRE_FAST_FLOAT
#include "/the/path/to/fast_float/include/fast_float/fast_float.h"
#endif

const uint8_t hex[256] = {
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x00-0x07
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x08-0x0F
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x10-0x17
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x18-0x1F
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x20-0x27
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x28-0x2F
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,  // 0x30-0x37 0-7
    0x8, 0x9, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x38-0x3F 8-9

    0x0, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x0,  // 0x40-0x47 A-F
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x48-0x4F
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x50-0x57
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x58-0x5F
    0x0, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x0,  // 0x60-0x67 a-f
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x68-0x6F
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x70-0x77
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x78-0x7F

    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x80-0x87
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x88-0x8F
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x90-0x97
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0x98-0x9F
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xA0-0xA7
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xA8-0xAF
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xB0-0xB7
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xB8-0xBF

    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xC0-0xC7
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xC8-0xCF
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xD0-0xD7
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xD8-0xDF
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xE0-0xE7
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xE8-0xEF
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xF0-0xF7
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,  // 0xF8-0xFF
};

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#endif

uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE];

wuffs_base__io_buffer g_src;

const char* g_filename;
FILE* g_file;
uint64_t g_line;

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
                   g_src.data.len - g_src.meta.wi, g_file);
  g_src.meta.wi += n;
  g_src.meta.closed = feof(g_file);
  if ((n == 0) && !g_src.meta.closed) {
    return "main: read error";
  }
  return NULL;
}

void  //
fail_parse(const char* impl, const char* z) {
  fprintf(stderr, "main: %s could not parse \"%s\" at %s:%" PRIu64 "\n", impl,
          z, g_filename, g_line);
}

void  //
fail(const char* impl, const char* z, uint64_t have, uint64_t want) {
  fprintf(stderr, "main: %s mismatch at %s:%" PRIu64 "\n", impl, g_filename,
          g_line);
  fprintf(stderr, "src:  %s\n", z);
  fprintf(stderr, "have: 0x%016" PRIX64 " %f\n", have,
          wuffs_base__ieee_754_bit_representation__from_u64_to_f64(have));
  fprintf(stderr, "want: 0x%016" PRIX64 " %f\n", want,
          wuffs_base__ieee_754_bit_representation__from_u64_to_f64(want));
}

bool  //
process_line(wuffs_base__slice_u8 s) {
  if (s.len < 32) {
    fprintf(stderr, "main: short input at %s:%" PRIu64 "\n", g_filename,
            g_line);
    return false;
  } else if (s.len > 2048) {
    fprintf(stderr, "main: long input at %s:%" PRIu64 "\n", g_filename, g_line);
    return false;
  }
  uint64_t want = 0;
  for (int i = 14; i < 30; i++) {
    want = (want << 4) | hex[s.ptr[i]];
  }
  s.ptr += 31;
  s.len -= 31;

  // Convert ".123" to "0.123". Not all parsers like a leading dot.
  if (s.ptr[0] == '.') {
    s.ptr--;
    s.len++;
    s.ptr[0] = '0';
  }

  char z[2049];
  memcpy(&z[0], s.ptr, s.len);
  z[s.len] = 0;

  // Check libc's strtod.
  {
    double have_f64 = strtod(&z[0], NULL);
    uint64_t have =
        wuffs_base__ieee_754_bit_representation__from_f64_to_u64(have_f64);
    if (have != want) {
      fail("strtod", &z[0], have, want);
      return false;
    }
  }

#ifdef USE_LEMIRE_FAST_DOUBLE_PARSER
  // Check lemire/fast_double_parser's parse_number.
  //
  // https://github.com/lemire/fast_double_parser/blob/master/README.md says
  // "the parser will reject overly large values that would not fit in
  // binary64. It will not produce NaN or infinite values".
  if (want < 0x7FF0000000000000ul) {
    double have_f64;
    if (!fast_double_parser::decimal_separator_dot::parse_number(&z[0],
                                                                 &have_f64)) {
      fail_parse("lemire/fast_double_parser", &z[0]);
      return false;
    }
    uint64_t have =
        wuffs_base__ieee_754_bit_representation__from_f64_to_u64(have_f64);
    if (have != want) {
      fail("lemire/fast_double_parser", &z[0], have, want);
      return false;
    }
  }
#endif

#ifdef USE_LEMIRE_FAST_FLOAT
  // Check lemire/fast_float's from_chars.
  {
    double have_f64;
    fast_float::from_chars_result result = fast_float::from_chars(
        static_cast<char*>(static_cast<void*>(s.ptr)),
        static_cast<char*>(static_cast<void*>(s.ptr + s.len)), have_f64,
        fast_float::chars_format::general);
    if (result.ec != std::errc()) {
      fail_parse("lemire/fast_float", &z[0]);
      return false;
    }
    uint64_t have =
        wuffs_base__ieee_754_bit_representation__from_f64_to_u64(have_f64);
    if (have != want) {
      fail("lemire/fast_float", &z[0], have, want);
      return false;
    }
  }
#endif

  // Check Wuffs' wuffs_base__parse_number_f64.
  {
    wuffs_base__result_f64 res = wuffs_base__parse_number_f64(
        s, WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
    if (res.status.repr) {
      fail_parse("wuffs", &z[0]);
      return false;
    }
    uint64_t have =
        wuffs_base__ieee_754_bit_representation__from_f64_to_u64(res.value);
    if (have != want) {
      fail("wuffs", &z[0], have, want);
      return false;
    }
  }

  return true;
}

bool  //
process_file(char* filename) {
  if (g_file) {
    fclose(g_file);
    g_file = NULL;
  }
  g_filename = filename;
  g_file = fopen(g_filename, "rb");
  if (!g_file) {
    fprintf(stderr, "main: could not open %s\n", g_filename);
    return false;
  }
  g_line = 0;
  g_src = wuffs_base__slice_u8__writer(
      wuffs_base__make_slice_u8(&g_src_buffer_array[0], SRC_BUFFER_ARRAY_SIZE));

  while (true) {
    for (size_t i = g_src.meta.ri; i < g_src.meta.wi; i++) {
      if (g_src.data.ptr[i] == '\n') {
        g_line++;
        if (!process_line(wuffs_base__make_slice_u8(
                &g_src.data.ptr[g_src.meta.ri], i - g_src.meta.ri))) {
          return false;
        }
        g_src.meta.ri = i + 1;
        continue;
      }
    }

    if (g_src.meta.closed) {
      if (g_src.meta.ri != g_src.meta.wi) {
        fprintf(stderr, "main: unexpected end-of-file\n");
        return false;
      }
      break;
    }

    const char* error_msg = read_src();
    if (error_msg) {
      fprintf(stderr, "%s\n", error_msg);
      return false;
    }
  }

  printf("%8" PRIu64 " OK in %s\n", g_line, g_filename);
  return true;
}

int  //
main(int argc, char** argv) {
  g_file = NULL;
  for (int argi = 1; argi < argc; argi++) {
    if (!process_file(argv[argi])) {
      return 1;
    }
  }
  return 0;
}
