// Copyright 2023 The Wuffs Authors.
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
This test program is typically run indirectly, by the "wuffs test" or "wuffs
bench" commands. These commands take an optional "-mimic" flag to check that
Wuffs' output mimics (i.e. exactly matches) other libraries' output, such as
giflib for GIF, libpng for PNG, etc.

To manually run this test:

for CC in clang gcc; do
  $CC -std=c99 -Wall -Werror lzma.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -llzma

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__LZMA

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/lzma.c"
#endif

// ---------------- Golden Tests

golden_test g_lzma_enwik5_gt = {
    .want_filename = "test/data/enwik5",
    .src_filename = "test/data/enwik5.lzma",
};

golden_test g_lzma_romeo_gt = {
    .want_filename = "test/data/romeo.txt",
    .src_filename = "test/data/romeo.txt.lzma",
};

// ---------------- LZMA Tests

const char*  //
test_wuffs_lzma_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_lzma__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_lzma__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__io_transformer(
      wuffs_lzma__decoder__upcast_as__wuffs_base__io_transformer(&dec),
      "test/data/romeo.txt.litonlylzma", 0, SIZE_MAX, 942, 0x0A);
}

const char*  //
wuffs_lzma_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  wuffs_lzma__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_lzma__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                               wuffs_initialize_flags));

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_lzma__decoder__transform_io(
        &dec, &limited_dst, &limited_src, g_work_slice_u8);

    dst->meta.wi += limited_dst.meta.wi;
    src->meta.ri += limited_src.meta.ri;

    if (((wlimit < UINT64_MAX) &&
         (status.repr == wuffs_base__suspension__short_write)) ||
        ((rlimit < UINT64_MAX) &&
         (status.repr == wuffs_base__suspension__short_read))) {
      continue;
    }
    return status.repr;
  }
}

const char*  //
test_wuffs_lzma_decode_enwik5() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_lzma_decode, &g_lzma_enwik5_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_wuffs_lzma_decode_romeo_lzma1() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_lzma_decode, &g_lzma_romeo_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_wuffs_lzma_decode_romeo_lzma2() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });

  CHECK_STRING(read_file(&src, "test/data/romeo.txt.xz"));
  if (src.meta.wi < 0x263) {
    RETURN_FAIL("source file was too short");
  }
  src.meta.ri = 0x018;
  src.meta.wi = 0x263;

  wuffs_lzma__decoder dec;
  CHECK_STATUS("initialize", wuffs_lzma__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  wuffs_lzma__decoder__set_quirk(&dec, WUFFS_LZMA__QUIRK_FORMAT_EXTENSION, 2);
  CHECK_STATUS("transform_io", wuffs_lzma__decoder__transform_io(
                                   &dec, &have, &src, g_work_slice_u8));

  CHECK_STRING(read_file(&want, "test/data/romeo.txt"));
  CHECK_STRING(check_io_buffers_equal("", &have, &want));
  if (src.meta.ri != src.meta.wi) {
    RETURN_FAIL("ri=0x%zX is not equal to wi=0x%zX", src.meta.ri, src.meta.wi);
  }
  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_lzma_decode_enwik5() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_lzma_decode, &g_lzma_enwik5_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_mimic_lzma_decode_romeo_lzma1() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_lzma_decode, &g_lzma_romeo_gt, UINT64_MAX,
                            UINT64_MAX);
}

#endif  // WUFFS_MIMIC

// ---------------- LZMA Benches

const char*  //
bench_wuffs_lzma_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_lzma_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_dst, &g_lzma_enwik5_gt, UINT64_MAX, UINT64_MAX, 5);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_lzma_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      mimic_lzma_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_dst, &g_lzma_enwik5_gt, UINT64_MAX, UINT64_MAX, 5);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_lzma_decode_enwik5,
    test_wuffs_lzma_decode_interface,
    test_wuffs_lzma_decode_romeo_lzma1,
    test_wuffs_lzma_decode_romeo_lzma2,

#ifdef WUFFS_MIMIC

    test_mimic_lzma_decode_enwik5,
    test_mimic_lzma_decode_romeo_lzma1,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_lzma_decode_100k,

#ifdef WUFFS_MIMIC

    bench_mimic_lzma_decode_100k,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/lzma";
  return test_main(argc, argv, g_tests, g_benches);
}
