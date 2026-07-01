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
crc32 prints the CRC-32 checksum (using the IEEE polynomial) of stdin. It is
similar to the standard /usr/bin/crc32 program, except that this example
program only reads from stdin.

To run:

$CXX crc32.cc && ./a.out < ../../README.md; rm -f a.out

for a C++ compiler $CXX, such as clang++ or g++.
*/

#if defined(__cplusplus) && (__cplusplus < 201103L)
#error "This C++ program requires -std=c++11 or later"
#endif

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>

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

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C++ file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (32 * 1024)
#endif

uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE];

int  //
main(int argc, char** argv) {
  wuffs_crc32__ieee_hasher h;
  wuffs_base__status status = h.initialize(sizeof h, WUFFS_VERSION, 0);
  if (!status.is_ok()) {
    fprintf(stderr, "%s\n", status.message());
    return 1;
  }

  while (true) {
    size_t n = fread(g_src_buffer_array, sizeof(uint8_t), SRC_BUFFER_ARRAY_SIZE,
                     stdin);
    uint32_t checksum =
        h.update_u32(wuffs_base__make_slice_u8(g_src_buffer_array, n));
    if (feof(stdin)) {
      printf("%08" PRIx32 "\n", checksum);
      return 0;
    } else if (ferror(stdin)) {
      fprintf(stderr, "read error\n");
      return 1;
    }
  }
}
