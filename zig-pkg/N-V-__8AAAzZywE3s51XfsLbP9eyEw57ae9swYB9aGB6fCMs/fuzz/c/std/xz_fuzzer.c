// Copyright 2024 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// Silence the nested slash-star warning for the next comment's command line.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomment"

/*
This fuzzer (the fuzz function) is typically run indirectly, by a framework
such as https://github.com/google/oss-fuzz calling LLVMFuzzerTestOneInput.

When working on the fuzz implementation, or as a coherence check, defining
WUFFS_CONFIG__FUZZLIB_MAIN will let you manually run fuzz over a set of files:

gcc -DWUFFS_CONFIG__FUZZLIB_MAIN xz_fuzzer.c
./a.out ../../../test/data/*.xz
rm -f ./a.out

It should print "PASS", amongst other information, and exit(0).
*/

#pragma clang diagnostic pop

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

#if defined(WUFFS_CONFIG__FUZZLIB_MAIN)
// Defining the WUFFS_CONFIG__STATIC_FUNCTIONS macro is optional, but when
// combined with WUFFS_IMPLEMENTATION, it demonstrates making all of Wuffs'
// functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#endif  // defined(WUFFS_CONFIG__FUZZLIB_MAIN)

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
#define WUFFS_CONFIG__MODULE__CRC64
#define WUFFS_CONFIG__MODULE__LZMA
#define WUFFS_CONFIG__MODULE__SHA256
#define WUFFS_CONFIG__MODULE__XZ

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../fuzzlib/fuzzlib.c"

// 64 KiB.
#define DST_BUFFER_ARRAY_SIZE 65536

// Wuffs allows either statically or dynamically allocated work buffers. This
// program exercises static allocation.
//
// We hard-code 64 MiB, as WUFFS_XZ__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
// can be over 4 GiB. Using it can cause "relocation truncated to fit:
// R_X86_64_PC32 against symbol etc" compiler errors.
#define WORK_BUFFER_ARRAY_SIZE 67108864
#if WORK_BUFFER_ARRAY_SIZE > 0
uint8_t g_work_buffer_array[WORK_BUFFER_ARRAY_SIZE];
#else
// Not all C/C++ compilers support 0-length arrays.
uint8_t g_work_buffer_array[1];
#endif

const char*  //
fuzz(wuffs_base__io_buffer* src, uint64_t hash) {
  wuffs_xz__decoder dec;
  wuffs_base__status status = wuffs_xz__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      (hash & 1) ? WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED : 0);
  if (!wuffs_base__status__is_ok(&status)) {
    return wuffs_base__status__message(&status);
  }

  // Ignore the checksum for 99.99%-ish of all input. When fuzzers generate
  // random input, the checkum is very unlikely to match. Still, it's useful to
  // verify that checksumming does not lead to e.g. buffer overflows.
  wuffs_xz__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM,
                               hash & 0xFFFE);

  static uint8_t dst_buffer[DST_BUFFER_ARRAY_SIZE];
  wuffs_base__io_buffer dst =
      wuffs_base__ptr_u8__writer(&dst_buffer[0], DST_BUFFER_ARRAY_SIZE);

  while (true) {
    dst.meta.wi = 0;
    status = wuffs_xz__decoder__transform_io(
        &dec, &dst, src,
        wuffs_base__make_slice_u8(g_work_buffer_array, WORK_BUFFER_ARRAY_SIZE));
    if (status.repr != wuffs_base__suspension__short_write) {
      break;
    }
    if (dst.meta.wi == 0) {
      fprintf(stderr, "wuffs_xz__decoder__transform_io made no progress\n");
      intentional_segfault();
    }
  }
  return wuffs_base__status__message(&status);
}
