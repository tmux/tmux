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
This test program is typically run indirectly, by the "wuffs test" or "wuffs
bench" commands. These commands take an optional "-mimic" flag to check that
Wuffs' output mimics (i.e. exactly matches) other libraries' output, such as
giflib for GIF, libpng for PNG, etc.

To manually run this test:

for CC in clang gcc; do
  $CC -std=c99 -Wall -Werror gzip.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -ldeflate -lz

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
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__GZIP

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/deflate-gzip-zlib.c"
#endif

// ---------------- Golden Tests

golden_test g_gzip_midsummer_gt = {
    .want_filename = "test/data/midsummer.txt",
    .src_filename = "test/data/midsummer.txt.gz",
};

golden_test g_gzip_pi_gt = {
    .want_filename = "test/data/pi.txt",
    .src_filename = "test/data/pi.txt.gz",
};

// ---------------- Gzip Tests

const char*  //
test_wuffs_gzip_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_gzip__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gzip__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__io_transformer(
      wuffs_gzip__decoder__upcast_as__wuffs_base__io_transformer(&dec),
      "test/data/romeo.txt.gz", 0, SIZE_MAX, 942, 0x0A);
}

const char*  //
test_wuffs_gzip_decode_truncated_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer have = wuffs_base__ptr_u8__writer(g_have_array_u8, 1);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_gzip__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gzip__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_gzip__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status =
      wuffs_gzip__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);
  if (status.repr != wuffs_gzip__error__truncated_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_gzip__error__truncated_input);
  }
  return NULL;
}

const char*  //
wuffs_gzip_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  wuffs_gzip__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gzip__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                               wuffs_initialize_flags));

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_gzip__decoder__transform_io(
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
do_test_wuffs_gzip_checksum(bool ignore_checksum, uint32_t bad_checksum) {
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });

  CHECK_STRING(read_file(&src, g_gzip_midsummer_gt.src_filename));

  // Flip a bit in the gzip checksum, which is in the last 8 bytes of the file.
  if (src.meta.wi < 8) {
    RETURN_FAIL("source file was too short");
  }
  if (bad_checksum) {
    src.data.ptr[src.meta.wi - 1 - (bad_checksum & 7)] ^= 1;
  }

  int end_limit;  // The rlimit, relative to the end of the data.
  for (end_limit = 0; end_limit < 10; end_limit++) {
    wuffs_gzip__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_gzip__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    wuffs_gzip__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM,
                                   (uint64_t)ignore_checksum);
    have.meta.wi = 0;
    src.meta.ri = 0;

    // Decode the src data in 1 or 2 chunks, depending on whether end_limit is
    // or isn't zero.
    for (int i = 0; i < 2; i++) {
      uint64_t rlimit = UINT64_MAX;
      const char* want_z = NULL;
      if (i == 0) {
        if (end_limit == 0) {
          continue;
        }
        if (src.meta.wi < end_limit) {
          RETURN_FAIL("end_limit=%d: not enough source data", end_limit);
        }
        rlimit = src.meta.wi - (uint64_t)(end_limit);
        want_z = wuffs_base__suspension__short_read;
      } else {
        want_z = (bad_checksum && !ignore_checksum)
                     ? wuffs_gzip__error__bad_checksum
                     : NULL;
      }

      wuffs_base__io_buffer limited_src = make_limited_reader(src, rlimit);
      wuffs_base__status have_z = wuffs_gzip__decoder__transform_io(
          &dec, &have, &limited_src, g_work_slice_u8);
      src.meta.ri += limited_src.meta.ri;
      if (have_z.repr != want_z) {
        RETURN_FAIL("end_limit=%d: have \"%s\", want \"%s\"", end_limit,
                    have_z.repr, want_z);
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_gzip_checksum_ignore() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gzip_checksum(true, 8 | 0);
}

const char*  //
test_wuffs_gzip_checksum_verify_bad0() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gzip_checksum(false, 8 | 0);
}

const char*  //
test_wuffs_gzip_checksum_verify_bad7() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gzip_checksum(false, 8 | 7);
}

const char*  //
test_wuffs_gzip_checksum_verify_good() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gzip_checksum(false, 0);
}

const char*  //
test_wuffs_gzip_decode_infrequent_compaction() {
  CHECK_FOCUS(__func__);

  wuffs_gzip__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gzip__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__io_buffer dst = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });

  // src_bytes holds the first 50 Fibonacci numbers, gzipped using Python's
  // gzip module.
  uint8_t src_bytes[183] = {
      0x1F, 0x8B, 0x08, 0x00, 0x07, 0x22, 0x16, 0x61, 0x02, 0xFF, 0x15, 0x8F,
      0xC1, 0x11, 0x00, 0x31, 0x08, 0x02, 0xFF, 0x54, 0x23, 0x6A, 0x44, 0xFB,
      0x6F, 0xEC, 0xB8, 0xC9, 0xCB, 0x59, 0x09, 0x6B, 0x80, 0x7E, 0x89, 0xC2,
      0xC3, 0x82, 0x85, 0x24, 0xAA, 0xF1, 0x3C, 0x1D, 0xD8, 0x8D, 0xAC, 0x42,
      0x49, 0x18, 0x06, 0x6E, 0x05, 0xBE, 0x13, 0xF2, 0x6D, 0xA3, 0xB9, 0xC4,
      0x68, 0x1E, 0x18, 0xD7, 0x03, 0x4A, 0xF4, 0x57, 0x3B, 0x4F, 0xE8, 0xA9,
      0x59, 0xE8, 0x45, 0x9A, 0x26, 0xEB, 0x0A, 0xBC, 0x71, 0x02, 0x45, 0xAD,
      0xD7, 0x1E, 0x3B, 0xF3, 0xB0, 0x95, 0xD1, 0xE1, 0xDE, 0x9E, 0x9C, 0x73,
      0xB9, 0xB6, 0xE2, 0x50, 0x2F, 0xFB, 0x69, 0xF1, 0x14, 0xB9, 0x2E, 0xBD,
      0x4C, 0xF5, 0x5F, 0xD4, 0x57, 0x61, 0x88, 0x6C, 0x9A, 0x53, 0xA8, 0x8B,
      0x5D, 0x3A, 0x3A, 0xE5, 0xC8, 0xAD, 0x35, 0xC2, 0xCA, 0xC6, 0xDE, 0x9E,
      0xF7, 0x36, 0xD8, 0x96, 0x1A, 0x9D, 0x0B, 0x6F, 0xD0, 0x66, 0xD7, 0x5D,
      0x82, 0x4C, 0x62, 0xE5, 0xF3, 0xE8, 0xFA, 0x0B, 0x8B, 0x59, 0x64, 0x6B,
      0x8A, 0xF4, 0x84, 0x3C, 0xD9, 0xFC, 0x85, 0x0A, 0xBD, 0xA1, 0x67, 0x3F,
      0x0D, 0x24, 0xAD, 0xDA, 0xD2, 0x87, 0x0F, 0x12, 0x69, 0x4A, 0xA8, 0x3B,
      0x01, 0x00, 0x00,
  };
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(src_bytes, sizeof src_bytes, true);

  // Decode 5 source bytes at a time. Compact every 15 source bytes.
  for (size_t i = 0; i < src.data.len; i += 5) {
    src.meta.wi = i;
    src.meta.closed = false;
    wuffs_base__status status =
        wuffs_gzip__decoder__transform_io(&dec, &dst, &src, g_work_slice_u8);
    if (status.repr != wuffs_base__suspension__short_read) {
      RETURN_FAIL("i=%zu: have \"%s\", want \"%s\"", i, status.repr,
                  wuffs_base__suspension__short_read);
    }
    if (i % 15 == 0) {
      dst.meta.ri = dst.meta.wi;
      wuffs_base__io_buffer__compact(&dst);
    }
  }

  src.meta.wi = src.data.len;
  src.meta.closed = true;
  CHECK_STATUS("final transform_io", wuffs_gzip__decoder__transform_io(
                                         &dec, &dst, &src, g_work_slice_u8));
  return NULL;
}

const char*  //
test_wuffs_gzip_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_gzip_decode, &g_gzip_midsummer_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_wuffs_gzip_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_gzip_decode, &g_gzip_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_gzip_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_gzip_decode, &g_gzip_midsummer_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_mimic_gzip_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_gzip_decode, &g_gzip_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

#endif  // WUFFS_MIMIC

// ---------------- Gzip Benches

const char*  //
bench_wuffs_gzip_decode_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_gzip_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_dst, &g_gzip_midsummer_gt, UINT64_MAX, UINT64_MAX, 300);
}

const char*  //
bench_wuffs_gzip_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_gzip_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_dst, &g_gzip_pi_gt, UINT64_MAX, UINT64_MAX, 30);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_gzip_decode_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_gzip_decode, 0, tcounter_dst,
                             &g_gzip_midsummer_gt, UINT64_MAX, UINT64_MAX, 300);
}

const char*  //
bench_mimic_gzip_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_gzip_decode, 0, tcounter_dst, &g_gzip_pi_gt,
                             UINT64_MAX, UINT64_MAX, 30);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

// Note that the gzip mimic tests and benches don't work with
// WUFFS_MIMICLIB_USE_MINIZ_INSTEAD_OF_ZLIB.

proc g_tests[] = {

    test_wuffs_gzip_checksum_ignore,
    test_wuffs_gzip_checksum_verify_bad0,
    test_wuffs_gzip_checksum_verify_bad7,
    test_wuffs_gzip_checksum_verify_good,
    test_wuffs_gzip_decode_infrequent_compaction,
    test_wuffs_gzip_decode_interface,
    test_wuffs_gzip_decode_midsummer,
    test_wuffs_gzip_decode_pi,
    test_wuffs_gzip_decode_truncated_input,

#ifdef WUFFS_MIMIC

    test_mimic_gzip_decode_midsummer,
    test_mimic_gzip_decode_pi,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_gzip_decode_10k,
    bench_wuffs_gzip_decode_100k,

#ifdef WUFFS_MIMIC

    bench_mimic_gzip_decode_10k,
    bench_mimic_gzip_decode_100k,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/gzip";
  return test_main(argc, argv, g_tests, g_benches);
}
