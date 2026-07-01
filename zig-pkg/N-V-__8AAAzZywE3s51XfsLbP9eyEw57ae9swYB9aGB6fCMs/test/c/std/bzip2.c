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
This test program is typically run indirectly, by the "wuffs test" or "wuffs
bench" commands. These commands take an optional "-mimic" flag to check that
Wuffs' output mimics (i.e. exactly matches) other libraries' output, such as
giflib for GIF, libpng for PNG, etc.

To manually run this test:

for CC in clang gcc; do
  $CC -std=c99 -Wall -Werror bzip2.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -lbz2

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
#define WUFFS_CONFIG__MODULE__BZIP2

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/bzip2.c"
#endif

// ---------------- Golden Tests

golden_test g_bzip2_256_bytes_gt = {
    .want_filename = "test/data/256.bytes",
    .src_filename = "test/data/256.bytes.bz2",
};

golden_test g_bzip2_bad_number_of_sections_gt = {
    .want_filename = "test/data/0.bytes",
    .src_filename = "test/data/artificial-bzip2/bad-number-of-sections.bz2",
};

golden_test g_bzip2_huffman_258_gt = {
    .want_filename = "test/data/abraca.txt",
    .src_filename = "test/data/artificial-bzip2/huffman-258.bz2",
};

golden_test g_bzip2_midsummer_gt = {
    .want_filename = "test/data/midsummer.txt",
    .src_filename = "test/data/midsummer.txt.bz2",
};

golden_test g_bzip2_pi_gt = {
    .want_filename = "test/data/pi.txt",
    .src_filename = "test/data/pi.txt.bz2",
};

// ---------------- Bzip2 Tests

const char*  //
test_wuffs_bzip2_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_bzip2__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_bzip2__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__io_transformer(
      wuffs_bzip2__decoder__upcast_as__wuffs_base__io_transformer(&dec),
      "test/data/romeo.txt.bz2", 0, SIZE_MAX, 942, 0x0A);
}

const char*  //
test_wuffs_bzip2_decode_truncated_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer have = wuffs_base__ptr_u8__writer(g_have_array_u8, 1);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_bzip2__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_bzip2__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_bzip2__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status =
      wuffs_bzip2__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);
  if (status.repr != wuffs_bzip2__error__truncated_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_bzip2__error__truncated_input);
  }
  return NULL;
}

const char*  //
wuffs_bzip2_decode(wuffs_base__io_buffer* dst,
                   wuffs_base__io_buffer* src,
                   uint32_t wuffs_initialize_flags,
                   uint64_t wlimit,
                   uint64_t rlimit) {
  wuffs_bzip2__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_bzip2__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                                wuffs_initialize_flags));

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_bzip2__decoder__transform_io(
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
test_wuffs_bzip2_decode_256_bytes() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_bzip2_decode, &g_bzip2_256_bytes_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_bzip2_decode_bad_number_of_sections() {
  CHECK_FOCUS(__func__);
  const char* have =
      do_test_io_buffers(wuffs_bzip2_decode, &g_bzip2_bad_number_of_sections_gt,
                         UINT64_MAX, UINT64_MAX);
  const char* want = wuffs_bzip2__error__bad_number_of_sections;
  if (have != want) {
    RETURN_FAIL("have \"%s\", want \"%s\"", have, want);
  }
  return NULL;
}

const char*  //
test_wuffs_bzip2_decode_huffman_258() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_bzip2_decode, &g_bzip2_huffman_258_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_bzip2_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_bzip2_decode, &g_bzip2_midsummer_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_bzip2_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_bzip2_decode, &g_bzip2_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_bzip2_decode_256_bytes() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_bzip2_decode, &g_bzip2_256_bytes_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_bzip2_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_bzip2_decode, &g_bzip2_midsummer_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_bzip2_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_bzip2_decode, &g_bzip2_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

#endif  // WUFFS_MIMIC

// ---------------- Bzip2 Benches

const char*  //
bench_wuffs_bzip2_decode_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bzip2_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_bzip2_midsummer_gt, UINT64_MAX, UINT64_MAX, 20);
}

const char*  //
bench_wuffs_bzip2_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bzip2_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_bzip2_pi_gt, UINT64_MAX, UINT64_MAX, 2);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_bzip2_decode_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      mimic_bzip2_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_bzip2_midsummer_gt, UINT64_MAX, UINT64_MAX, 20);
}

const char*  //
bench_mimic_bzip2_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      mimic_bzip2_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_bzip2_pi_gt, UINT64_MAX, UINT64_MAX, 2);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_bzip2_decode_256_bytes,
    test_wuffs_bzip2_decode_bad_number_of_sections,
    test_wuffs_bzip2_decode_huffman_258,
    test_wuffs_bzip2_decode_interface,
    test_wuffs_bzip2_decode_midsummer,
    test_wuffs_bzip2_decode_pi,
    test_wuffs_bzip2_decode_truncated_input,

#ifdef WUFFS_MIMIC

    test_mimic_bzip2_decode_256_bytes,
    test_mimic_bzip2_decode_midsummer,
    test_mimic_bzip2_decode_pi,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_bzip2_decode_10k,
    bench_wuffs_bzip2_decode_100k,

#ifdef WUFFS_MIMIC

    bench_mimic_bzip2_decode_10k,
    bench_mimic_bzip2_decode_100k,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/bzip2";
  return test_main(argc, argv, g_tests, g_benches);
}
