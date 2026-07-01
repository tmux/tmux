// Copyright 2017 The Wuffs Authors.
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
  $CC -std=c99 -Wall -Werror zlib.c && ./a.out
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
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ZLIB

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/deflate-gzip-zlib.c"
#endif

// ---------------- Golden Tests

golden_test g_deflate_romeo_gt = {
    .want_filename = "test/data/romeo.txt",
    .src_filename = "test/data/romeo.txt.deflate",
};

golden_test g_zlib_midsummer_gt = {
    .want_filename = "test/data/midsummer.txt",
    .src_filename = "test/data/midsummer.txt.zlib",
};

golden_test g_zlib_pi_gt = {
    .want_filename = "test/data/pi.txt",
    .src_filename = "test/data/pi.txt.zlib",
};

// This dictionary-using zlib-encoded data comes from
// https://play.golang.org/p/Jh9Wyp6PLID, also mentioned in the RAC spec.
const char* g_zlib_sheep_src_ptr =
    "\x78\xf9\x0b\xe0\x02\x6e\x0a\x29\xcf\x87\x31\x01\x01\x00\x00\xff\xff\x18"
    "\x0c\x03\xa8";
const size_t g_zlib_sheep_src_len = 21;
const char* g_zlib_sheep_dict_ptr = " sheep.\n";
const size_t g_zlib_sheep_dict_len = 8;
const char* g_zlib_sheep_want_ptr = "Two sheep.\n";
const size_t g_zlib_sheep_want_len = 11;

// ---------------- Zlib Tests

const char*  //
test_wuffs_zlib_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_zlib__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_zlib__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__io_transformer(
      wuffs_zlib__decoder__upcast_as__wuffs_base__io_transformer(&dec),
      "test/data/romeo.txt.zlib", 0, SIZE_MAX, 942, 0x0A);
}

const char*  //
test_wuffs_zlib_decode_truncated_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer have = wuffs_base__ptr_u8__writer(g_have_array_u8, 1);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_zlib__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_zlib__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_zlib__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status =
      wuffs_zlib__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);
  if (status.repr != wuffs_zlib__error__truncated_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_zlib__error__truncated_input);
  }
  return NULL;
}

const char*  //
wuffs_raw_deflate_decode(wuffs_base__io_buffer* dst,
                         wuffs_base__io_buffer* src,
                         uint32_t wuffs_initialize_flags,
                         uint64_t wlimit,
                         uint64_t rlimit) {
  wuffs_zlib__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_zlib__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                               wuffs_initialize_flags));

  // This wuffs_zlib__decoder__set_quirk call is the only difference between
  // wuffs_raw_deflate_decode and wuffs_zlib_decode immediately below.
  wuffs_zlib__decoder__set_quirk(&dec, WUFFS_ZLIB__QUIRK_JUST_RAW_DEFLATE, 1);

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_zlib__decoder__transform_io(
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
wuffs_zlib_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  wuffs_zlib__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_zlib__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                               wuffs_initialize_flags));

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_zlib__decoder__transform_io(
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
do_test_wuffs_zlib_checksum(bool ignore_checksum, uint32_t bad_checksum) {
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });

  CHECK_STRING(read_file(&src, g_zlib_midsummer_gt.src_filename));
  // Flip a bit in the zlib checksum, which is in the last 4 bytes of the file.
  if (src.meta.wi < 4) {
    RETURN_FAIL("source file was too short");
  }
  if (bad_checksum) {
    src.data.ptr[src.meta.wi - 1 - (bad_checksum & 3)] ^= 1;
  }

  int end_limit;  // The rlimit, relative to the end of the data.
  for (end_limit = 0; end_limit < 10; end_limit++) {
    wuffs_zlib__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_zlib__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    wuffs_zlib__decoder__set_quirk(&dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM,
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
                     ? wuffs_zlib__error__bad_checksum
                     : NULL;
      }

      wuffs_base__io_buffer limited_src = make_limited_reader(src, rlimit);

      wuffs_base__status have_z = wuffs_zlib__decoder__transform_io(
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
test_wuffs_zlib_checksum_ignore() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_zlib_checksum(true, 4 | 0);
}

const char*  //
test_wuffs_zlib_checksum_verify_bad0() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_zlib_checksum(false, 4 | 0);
}

const char*  //
test_wuffs_zlib_checksum_verify_bad3() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_zlib_checksum(false, 4 | 3);
}

const char*  //
test_wuffs_zlib_checksum_verify_good() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_zlib_checksum(false, 0);
}

const char*  //
test_wuffs_zlib_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_zlib_decode, &g_zlib_midsummer_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_wuffs_zlib_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_zlib_decode, &g_zlib_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_wuffs_zlib_decode_raw_deflate_romeo() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_raw_deflate_decode, &g_deflate_romeo_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_zlib_decode_sheep() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer src =
      make_io_buffer_from_string(g_zlib_sheep_src_ptr, g_zlib_sheep_src_len);

  wuffs_zlib__decoder dec;
  CHECK_STATUS("initialize", wuffs_zlib__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));

  for (int i = 0; i < 3; i++) {
    wuffs_base__status status =
        wuffs_zlib__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);

    if (status.repr != wuffs_zlib__note__dictionary_required) {
      RETURN_FAIL("transform_io (before dict): have \"%s\", want \"%s\"",
                  status.repr, wuffs_zlib__note__dictionary_required);
    }

    uint32_t dict_id_have = wuffs_zlib__decoder__dictionary_id(&dec);
    uint32_t dict_id_want = 0x0BE0026E;
    if (dict_id_have != dict_id_want) {
      RETURN_FAIL("dictionary_id: have 0x%08" PRIX32 ", want 0x%08x" PRIX32,
                  dict_id_have, dict_id_want);
    }
  }

  wuffs_zlib__decoder__add_dictionary(
      &dec, ((wuffs_base__slice_u8){
                .ptr = ((uint8_t*)(g_zlib_sheep_dict_ptr)),
                .len = g_zlib_sheep_dict_len,
            }));

  CHECK_STATUS(
      "transform_io (after dict)",
      wuffs_zlib__decoder__transform_io(&dec, &have, &src, g_work_slice_u8));

  wuffs_base__io_buffer want =
      make_io_buffer_from_string(g_zlib_sheep_want_ptr, g_zlib_sheep_want_len);
  return check_io_buffers_equal("", &have, &want);
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_zlib_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_zlib_decode, &g_zlib_midsummer_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_mimic_zlib_decode_pi() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_zlib_decode, &g_zlib_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_mimic_zlib_decode_sheep() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer src =
      make_io_buffer_from_string(g_zlib_sheep_src_ptr, g_zlib_sheep_src_len);
  wuffs_base__slice_u8 dict = ((wuffs_base__slice_u8){
      .ptr = ((uint8_t*)(g_zlib_sheep_dict_ptr)),
      .len = g_zlib_sheep_dict_len,
  });
  const char* status = mimic_zlib_decode_with_dictionary(&have, &src, dict);
  if (status) {
    return status;
  }
  wuffs_base__io_buffer want =
      make_io_buffer_from_string(g_zlib_sheep_want_ptr, g_zlib_sheep_want_len);
  return check_io_buffers_equal("", &have, &want);
}

#endif  // WUFFS_MIMIC

// ---------------- Zlib Benches

const char*  //
bench_wuffs_zlib_decode_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_zlib_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_dst, &g_zlib_midsummer_gt, UINT64_MAX, UINT64_MAX, 300);
}

const char*  //
bench_wuffs_zlib_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_zlib_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_dst, &g_zlib_pi_gt, UINT64_MAX, UINT64_MAX, 30);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_zlib_decode_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_zlib_decode, 0, tcounter_dst,
                             &g_zlib_midsummer_gt, UINT64_MAX, UINT64_MAX, 300);
}

const char*  //
bench_mimic_zlib_decode_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_zlib_decode, 0, tcounter_dst, &g_zlib_pi_gt,
                             UINT64_MAX, UINT64_MAX, 30);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_zlib_checksum_ignore,
    test_wuffs_zlib_checksum_verify_bad0,
    test_wuffs_zlib_checksum_verify_bad3,
    test_wuffs_zlib_checksum_verify_good,
    test_wuffs_zlib_decode_interface,
    test_wuffs_zlib_decode_midsummer,
    test_wuffs_zlib_decode_pi,
    test_wuffs_zlib_decode_raw_deflate_romeo,
    test_wuffs_zlib_decode_sheep,
    test_wuffs_zlib_decode_truncated_input,

#ifdef WUFFS_MIMIC

    test_mimic_zlib_decode_midsummer,
    test_mimic_zlib_decode_pi,
#ifndef WUFFS_MIMICLIB_ZLIB_DOES_NOT_SUPPORT_DICTIONARIES
    test_mimic_zlib_decode_sheep,
#endif

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_zlib_decode_10k,
    bench_wuffs_zlib_decode_100k,

#ifdef WUFFS_MIMIC

    bench_mimic_zlib_decode_10k,
    bench_mimic_zlib_decode_100k,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/zlib";
  return test_main(argc, argv, g_tests, g_benches);
}
