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
This test program is typically run indirectly, by the "wuffs test" or "wuffs
bench" commands. These commands take an optional "-mimic" flag to check that
Wuffs' output mimics (i.e. exactly matches) other libraries' output, such as
giflib for GIF, libpng for PNG, etc.

To manually run this test:

for CC in clang gcc; do
  $CC -std=c99 -Wall -Werror bmp.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC

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
#define WUFFS_CONFIG__MODULE__BMP

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
// No mimic library.
#endif

// ---------------- BMP Tests

const char*  //
wuffs_bmp_decode(uint64_t* n_bytes_out,
                 wuffs_base__io_buffer* dst,
                 uint32_t wuffs_initialize_flags,
                 wuffs_base__pixel_format pixfmt,
                 uint32_t* quirks_ptr,
                 size_t quirks_len,
                 wuffs_base__io_buffer* src) {
  wuffs_bmp__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_bmp__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                              wuffs_initialize_flags));
  return do_run__wuffs_base__image_decoder(
      wuffs_bmp__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      n_bytes_out, dst, pixfmt, quirks_ptr, quirks_len, src);
}

// --------

const char*  //
test_wuffs_bmp_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_bmp__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_bmp__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__image_decoder(
      wuffs_bmp__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      "test/data/hippopotamus.bmp", 0, SIZE_MAX, 36, 28, 0xFFF5F5F5);
}

const char*  //
test_wuffs_bmp_decode_truncated_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_bmp__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_bmp__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_bmp__decoder__decode_image_config(&dec, NULL, &src);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status = wuffs_bmp__decoder__decode_image_config(&dec, NULL, &src);
  if (status.repr != wuffs_bmp__error__truncated_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_bmp__error__truncated_input);
  }
  return NULL;
}

const char*  //
test_wuffs_bmp_decode_frame_config() {
  CHECK_FOCUS(__func__);
  wuffs_bmp__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_bmp__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/hat.bmp"));
  CHECK_STATUS("decode_frame_config #0",
               wuffs_bmp__decoder__decode_frame_config(&dec, &fc, &src));

  wuffs_base__status status =
      wuffs_bmp__decoder__decode_frame_config(&dec, &fc, &src);
  if (status.repr != wuffs_base__note__end_of_data) {
    RETURN_FAIL("decode_frame_config #1: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__note__end_of_data);
  }
  return NULL;
}

const char*  //
test_wuffs_bmp_decode_io_redirect() {
  CHECK_FOCUS(__func__);
  wuffs_bmp__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_bmp__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/rgb24png.bmp"));
  if (src.meta.wi != 1210) {
    RETURN_FAIL("file size: have %zu, want 1210", src.meta.wi);
  }

  wuffs_base__status status =
      wuffs_bmp__decoder__decode_image_config(&dec, NULL, &src);
  if (status.repr != wuffs_base__note__i_o_redirect) {
    RETURN_FAIL("decode_image_config: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__note__i_o_redirect);
  }

  wuffs_base__io_buffer empty = wuffs_base__empty_io_buffer();
  wuffs_base__more_information minfo = wuffs_base__empty_more_information();
  CHECK_STATUS("tell_me_more",
               wuffs_bmp__decoder__tell_me_more(&dec, &empty, &minfo, &src));
  if (minfo.flavor != WUFFS_BASE__MORE_INFORMATION__FLAVOR__IO_REDIRECT) {
    RETURN_FAIL("flavor: have %" PRIu32 ", want %" PRIu32, minfo.flavor,
                WUFFS_BASE__MORE_INFORMATION__FLAVOR__IO_REDIRECT);
  }

  uint32_t have_fourcc =
      wuffs_base__more_information__io_redirect__fourcc(&minfo);
  if (have_fourcc != WUFFS_BASE__FOURCC__PNG) {
    RETURN_FAIL("fourcc: have 0x%08" PRIX32 ", want 0x%08" PRIX32, have_fourcc,
                WUFFS_BASE__FOURCC__PNG);
  }

  wuffs_base__range_ie_u64 have_range =
      wuffs_base__more_information__io_redirect__range(&minfo);
  if (have_range.min_incl != 138) {
    RETURN_FAIL("range.min_incl: have %" PRIu64 ", want 138",
                have_range.min_incl);
  } else if (have_range.max_excl < 1210) {
    RETURN_FAIL("range.max_excl: have %" PRIu64 ", want >= 1210",
                have_range.max_excl);
  }
  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

// No mimic tests.

#endif  // WUFFS_MIMIC

// ---------------- BMP Benches

const char*  //
bench_wuffs_bmp_decode_40k() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_bmp_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hat.bmp", 0, SIZE_MAX, 1000);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

// No mimic benches.

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_bmp_decode_frame_config,
    test_wuffs_bmp_decode_interface,
    test_wuffs_bmp_decode_io_redirect,
    test_wuffs_bmp_decode_truncated_input,

#ifdef WUFFS_MIMIC

// No mimic tests.

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_bmp_decode_40k,

#ifdef WUFFS_MIMIC

// No mimic benches.

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/bmp";
  return test_main(argc, argv, g_tests, g_benches);
}
