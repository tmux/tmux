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
  $CC -std=c99 -Wall -Werror cbor.c && ./a.out
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
#define WUFFS_CONFIG__MODULE__CBOR

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
// No mimic library.
#endif

// ---------------- Golden Tests

golden_test g_cbor_cbor_rfc_7049_examples_gt = {
    .want_filename = "test/data/cbor-rfc-7049-examples.tokens",
    .src_filename = "test/data/cbor-rfc-7049-examples.cbor",
};

// ---------------- CBOR Tests

const char*  //
test_wuffs_cbor_decode_interface() {
  CHECK_FOCUS(__func__);

  wuffs_cbor__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_cbor__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  CHECK_STRING(do_test__wuffs_base__token_decoder(
      wuffs_cbor__decoder__upcast_as__wuffs_base__token_decoder(&dec),
      &g_cbor_cbor_rfc_7049_examples_gt));

  return NULL;
}

const char*  //
test_wuffs_cbor_decode_empty_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__token_buffer tok =
      wuffs_base__slice_token__writer(g_have_slice_token);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_cbor__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_cbor__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_cbor__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status =
      wuffs_cbor__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8);
  if (status.repr != wuffs_cbor__error__bad_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_cbor__error__bad_input);
  }
  return NULL;
}

const char*  //
test_wuffs_cbor_decode_invalid() {
  CHECK_FOCUS(__func__);

  // The official suite of CBOR test vectors (collected in this repo as
  // test/data/cbor-rfc-7049-examples.cbor) contain valid examples.
  //
  // This suite contains invalid examples, which should be rejected.
  const char* test_cases[] = {
      // Truncated (integer; major type 0) value.
      "\x18",
      // Tag in array, immediately before an 0xFF stop code. Some discussion is
      // at https://github.com/cbor/cbor.github.io/issues/65
      "\x9F\xD0\xFF",
      // Map with 1 element (an odd number).
      "\xA1\x01",
      // Map with 3 elements (an odd number).
      "\xBF\x01\x02\x03\xFF",
      // Tag in map, immediately before an 0xFF stop code. Some discussion is
      // at https://github.com/cbor/cbor.github.io/issues/65
      "\xBF\xD0\xFF",
      // Unused opcode.
      "\xFE",
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__token tok_array[256];
    wuffs_base__token_buffer tok_buf =
        wuffs_base__slice_token__writer(wuffs_base__make_slice_token(
            &tok_array[0], WUFFS_TESTLIB_ARRAY_SIZE(tok_array)));
    const bool closed = true;
    wuffs_base__io_buffer io_buf = wuffs_base__slice_u8__reader(
        wuffs_base__make_slice_u8((uint8_t*)(void*)(test_cases[tc]),
                                  strlen(test_cases[tc])),
        closed);

    wuffs_cbor__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_cbor__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    wuffs_base__status status = wuffs_cbor__decoder__decode_tokens(
        &dec, &tok_buf, &io_buf, g_work_slice_u8);
    if (!wuffs_base__status__is_error(&status)) {
      RETURN_FAIL("tc=%zu: have \"%s\", want an error", tc, status.repr);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_cbor_decode_valid() {
  CHECK_FOCUS(__func__);

  // This suite contains valid examples, similar to the
  // test_wuffs_cbor_decode_invalid examples, but they should be accepted.
  const char* test_cases[] = {
      // Map with 2 elements (an even number).
      "\xA1\x01\x02",
      // Tag immediately before an empty array.
      "\xD0\x9F\xFF",
      // Tag immediately before an empty map.
      "\xD0\xBF\xFF",
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__token tok_array[256];
    wuffs_base__token_buffer tok_buf =
        wuffs_base__slice_token__writer(wuffs_base__make_slice_token(
            &tok_array[0], WUFFS_TESTLIB_ARRAY_SIZE(tok_array)));
    const bool closed = true;
    wuffs_base__io_buffer io_buf = wuffs_base__slice_u8__reader(
        wuffs_base__make_slice_u8((uint8_t*)(void*)(test_cases[tc]),
                                  strlen(test_cases[tc])),
        closed);

    wuffs_cbor__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_cbor__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    wuffs_base__status status = wuffs_cbor__decoder__decode_tokens(
        &dec, &tok_buf, &io_buf, g_work_slice_u8);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("tc=%zu: have \"%s\", want no error", tc, status.repr);
    }
  }
  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

// No mimic tests.

#endif  // WUFFS_MIMIC

// ---------------- CBOR Benches

// No CBOR benches.

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

// No mimic benches.

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_cbor_decode_empty_input,
    test_wuffs_cbor_decode_interface,
    test_wuffs_cbor_decode_invalid,
    test_wuffs_cbor_decode_valid,

#ifdef WUFFS_MIMIC

// No mimic tests.

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

// No CBOR benches.

#ifdef WUFFS_MIMIC

// No mimic benches.

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/cbor";
  return test_main(argc, argv, g_tests, g_benches);
}
