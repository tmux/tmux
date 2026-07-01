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
  $CC -std=c99 -Wall -Werror xxhash32.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -lxxhash

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
#define WUFFS_CONFIG__MODULE__XXHASH32

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/xxhash.c"
#endif

// ---------------- Golden Tests

golden_test g_xxhash32_midsummer_gt = {
    .src_filename = "test/data/midsummer.txt",
};

golden_test g_xxhash32_pi_gt = {
    .src_filename = "test/data/pi.txt",
};

// ---------------- XXHash32 Tests

const char*  //
test_wuffs_xxhash32_interface() {
  CHECK_FOCUS(__func__);
  wuffs_xxhash32__hasher h;
  CHECK_STATUS("initialize",
               wuffs_xxhash32__hasher__initialize(
                   &h, sizeof h, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__hasher_u32(
      wuffs_xxhash32__hasher__upcast_as__wuffs_base__hasher_u32(&h),
      "test/data/hat.lossy.webp", 0, SIZE_MAX, 0x1A54B53D);
}

const char*  //
test_wuffs_xxhash32_golden() {
  CHECK_FOCUS(__func__);

  struct {
    const char* filename;
    // The want values are determined by script/checksum.go.
    uint32_t want;
  } test_cases[] = {
      {
          .filename = "test/data/hat.bmp",
          .want = 0xCAD975D7,
      },
      {
          .filename = "test/data/hat.gif",
          .want = 0x27633229,
      },
      {
          .filename = "test/data/hat.jpeg",
          .want = 0xEEF96C12,
      },
      {
          .filename = "test/data/hat.lossless.webp",
          .want = 0xA731CF6A,
      },
      {
          .filename = "test/data/hat.lossy.webp",
          .want = 0x1A54B53D,
      },
      {
          .filename = "test/data/hat.png",
          .want = 0x2EF9D842,
      },
      {
          .filename = "test/data/hat.tiff",
          .want = 0x244C2A7F,
      },
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    CHECK_STRING(read_file(&src, test_cases[tc].filename));

    for (int j = 0; j < 2; j++) {
      wuffs_xxhash32__hasher checksum;
      CHECK_STATUS("initialize",
                   wuffs_xxhash32__hasher__initialize(
                       &checksum, sizeof checksum, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

      uint32_t have = 0;
      size_t num_fragments = 0;
      size_t num_bytes = 0;
      do {
        wuffs_base__slice_u8 data = ((wuffs_base__slice_u8){
            .ptr = src.data.ptr + num_bytes,
            .len = src.meta.wi - num_bytes,
        });
        size_t limit = 101 + 103 * num_fragments;
        if ((j > 0) && (data.len > limit)) {
          data.len = limit;
        }
        have = wuffs_xxhash32__hasher__update_u32(&checksum, data);
        num_fragments++;
        num_bytes += data.len;
      } while (num_bytes < src.meta.wi);

      if (have != test_cases[tc].want) {
        RETURN_FAIL("tc=%zu, j=%d, filename=\"%s\": have 0x%08" PRIX32
                    ", want 0x%08" PRIX32 "\n",
                    tc, j, test_cases[tc].filename, have, test_cases[tc].want);
      }
    }
  }
  return NULL;
}

const char*  //
do_test_xxxxx_xxhash32_pi(bool mimic) {
  const char* digits =
      "3."
      "141592653589793238462643383279502884197169399375105820974944592307816406"
      "2862089986280348253421170";
  if (strlen(digits) != 99) {
    RETURN_FAIL("strlen(digits): have %d, want 99", (int)(strlen(digits)));
  }

  // The want values are determined by script/checksum.go.
  //
  // wants[i] is the checksum of the first i bytes of the digits string.
  uint32_t wants[100] = {
      0x02CC5D05, 0x9CEC73C4, 0x8882F465, 0x76EB9891, 0x65EE94C3, 0x1D582EB0,
      0x3F23315C, 0xF7876132, 0x5C7905AB, 0xB13CFCB0, 0x249A3696, 0x8DFDDDDE,
      0x074C32E3, 0x01832398, 0x342CC9FD, 0x27DAF5DF, 0xA724DADF, 0x82C243CD,
      0x058657E3, 0x7702E9E9, 0x0BB1F08C, 0x8663CF29, 0x9EE80972, 0x8072A394,
      0x896E216F, 0x2BA14621, 0xC8F505C1, 0xA1E25C52, 0x3775542D, 0x7A89E5C6,
      0xACD02748, 0x6C4406C8, 0x260382A6, 0x6AD6D4BD, 0xB3CC8788, 0xF8DCB125,
      0xA5BBCDFB, 0x82CC4E8C, 0xCDF34B78, 0xD8D22CCE, 0x64C57168, 0xA8DE94FF,
      0x9DD2BAA1, 0x9D44B437, 0x5A136E82, 0x1907E88D, 0x80F7AA44, 0x1DC870E6,
      0xD300C657, 0xC6F80CA0, 0xECA7845A, 0xEA33A5CA, 0x6113E405, 0x8D952878,
      0x08853159, 0x83AD2241, 0x0B776C22, 0x17B74D73, 0x0A5503C1, 0x4BB9F48F,
      0xA044A6F2, 0xC7BD90E6, 0x23B9D53F, 0x512A214F, 0xDA5BF238, 0xCE112793,
      0xD6833E33, 0x28911D30, 0x588E359B, 0xC161984D, 0xD87050E1, 0xDBF9126A,
      0x676E7A0D, 0xEA6AAC3D, 0x5392F46E, 0xC3851030, 0x3714254B, 0x7136006D,
      0xD7683690, 0xDA681B6E, 0x6AE5712A, 0x30CB24D5, 0x9D760EA6, 0x5B0020E6,
      0xDC118CC1, 0xFC764944, 0x27163F53, 0x91DFA8D9, 0x2D3B63BA, 0x3790770D,
      0x9012C9F0, 0xF0F5377B, 0x624B4744, 0xF376E821, 0x8900258A, 0x5E01F292,
      0xE77B80AE, 0x335F4A44, 0x40374C75, 0x7E7BD839,
  };

  for (int i = 0; i < 100; i++) {
    uint32_t have = 0;
    wuffs_base__slice_u8 data = ((wuffs_base__slice_u8){
        .ptr = (uint8_t*)(digits),
        .len = (size_t)(i),
    });

    if (mimic) {
#ifdef WUFFS_MIMIC
      have = mimic_xxhash32_one_shot_checksum_u32(data);
#endif  // WUFFS_MIMIC

    } else {
      wuffs_xxhash32__hasher checksum;
      CHECK_STATUS("initialize",
                   wuffs_xxhash32__hasher__initialize(
                       &checksum, sizeof checksum, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
      have = wuffs_xxhash32__hasher__update_u32(&checksum, data);
    }

    if (have != wants[i]) {
      RETURN_FAIL("i=%d: have 0x%08" PRIX32 ", want 0x%08" PRIX32, i, have,
                  wants[i]);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_xxhash32_pi() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_xxhash32_pi(false);
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_xxhash32_pi() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_xxhash32_pi(true);
}

#endif  // WUFFS_MIMIC

// ---------------- XXHash32 Benches

uint32_t g_wuffs_xxhash32_unused_u32;

const char*  //
wuffs_bench_xxhash32(wuffs_base__io_buffer* dst,
                     wuffs_base__io_buffer* src,
                     uint32_t wuffs_initialize_flags,
                     uint64_t wlimit,
                     uint64_t rlimit) {
  uint64_t len = src->meta.wi - src->meta.ri;
  if (rlimit) {
    len = wuffs_base__u64__min(len, rlimit);
  }
  wuffs_xxhash32__hasher checksum = {0};
  CHECK_STATUS("initialize", wuffs_xxhash32__hasher__initialize(
                                 &checksum, sizeof checksum, WUFFS_VERSION,
                                 wuffs_initialize_flags));
  g_wuffs_xxhash32_unused_u32 = wuffs_xxhash32__hasher__update_u32(
      &checksum, ((wuffs_base__slice_u8){
                     .ptr = src->data.ptr + src->meta.ri,
                     .len = len,
                 }));
  src->meta.ri += len;
  return NULL;
}

const char*  //
bench_wuffs_xxhash32_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_xxhash32,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_xxhash32_midsummer_gt, UINT64_MAX, UINT64_MAX, 5000);
}

const char*  //
bench_wuffs_xxhash32_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_xxhash32,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_xxhash32_pi_gt, UINT64_MAX, UINT64_MAX, 500);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_xxhash32_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_bench_xxhash32, 0, tcounter_src,
                             &g_xxhash32_midsummer_gt, UINT64_MAX, UINT64_MAX,
                             5000);
}

const char*  //
bench_mimic_xxhash32_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_bench_xxhash32, 0, tcounter_src,
                             &g_xxhash32_pi_gt, UINT64_MAX, UINT64_MAX, 500);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_xxhash32_golden,
    test_wuffs_xxhash32_interface,
    test_wuffs_xxhash32_pi,

#ifdef WUFFS_MIMIC

    test_mimic_xxhash32_pi,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_xxhash32_10k,
    bench_wuffs_xxhash32_100k,

#ifdef WUFFS_MIMIC

    bench_mimic_xxhash32_10k,
    bench_mimic_xxhash32_100k,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/xxhash32";
  return test_main(argc, argv, g_tests, g_benches);
}
