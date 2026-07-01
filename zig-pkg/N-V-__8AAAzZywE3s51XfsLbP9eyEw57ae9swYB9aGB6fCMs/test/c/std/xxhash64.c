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
  $CC -std=c99 -Wall -Werror xxhash64.c && ./a.out
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
#define WUFFS_CONFIG__MODULE__XXHASH64

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/xxhash.c"
#endif

// ---------------- Golden Tests

golden_test g_xxhash64_midsummer_gt = {
    .src_filename = "test/data/midsummer.txt",
};

golden_test g_xxhash64_pi_gt = {
    .src_filename = "test/data/pi.txt",
};

// ---------------- XXHash64 Tests

const char*  //
test_wuffs_xxhash64_interface() {
  CHECK_FOCUS(__func__);
  wuffs_xxhash64__hasher h;
  CHECK_STATUS("initialize",
               wuffs_xxhash64__hasher__initialize(
                   &h, sizeof h, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__hasher_u64(
      wuffs_xxhash64__hasher__upcast_as__wuffs_base__hasher_u64(&h),
      "test/data/hat.lossy.webp", 0, SIZE_MAX, 0x85D813707FE352B7);
}

const char*  //
test_wuffs_xxhash64_golden() {
  CHECK_FOCUS(__func__);

  struct {
    const char* filename;
    // The want values are determined by script/checksum.go.
    uint64_t want;
  } test_cases[] = {
      {
          .filename = "test/data/hat.bmp",
          .want = 0xA7D576E6A9BAF900,
      },
      {
          .filename = "test/data/hat.gif",
          .want = 0x38E8A7CAFE15E5B8,
      },
      {
          .filename = "test/data/hat.jpeg",
          .want = 0x6B8E028CE8CC09AD,
      },
      {
          .filename = "test/data/hat.lossless.webp",
          .want = 0xCA571B25E75792DA,
      },
      {
          .filename = "test/data/hat.lossy.webp",
          .want = 0x85D813707FE352B7,
      },
      {
          .filename = "test/data/hat.png",
          .want = 0x6096D53175D9C0B5,
      },
      {
          .filename = "test/data/hat.tiff",
          .want = 0x2B7A9E69AEB07DD1,
      },
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
        .data = g_src_slice_u8,
    });
    CHECK_STRING(read_file(&src, test_cases[tc].filename));

    for (int j = 0; j < 2; j++) {
      wuffs_xxhash64__hasher checksum;
      CHECK_STATUS("initialize",
                   wuffs_xxhash64__hasher__initialize(
                       &checksum, sizeof checksum, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

      uint64_t have = 0;
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
        have = wuffs_xxhash64__hasher__update_u64(&checksum, data);
        num_fragments++;
        num_bytes += data.len;
      } while (num_bytes < src.meta.wi);

      if (have != test_cases[tc].want) {
        RETURN_FAIL("tc=%zu, j=%d, filename=\"%s\": have 0x%016" PRIX64
                    ", want 0x%016" PRIX64 "\n",
                    tc, j, test_cases[tc].filename, have, test_cases[tc].want);
      }
    }
  }
  return NULL;
}

const char*  //
do_test_xxxxx_xxhash64_pi(bool mimic) {
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
  uint64_t wants[100] = {
      0xEF46DB3751D8E999, 0x26167C2AF5162CA4, 0x05BEDEA4D7DD3935,
      0x765F8073D4013B31, 0x3E160875545B6BE3, 0x0BC5FB5031C01569,
      0x7F4E574C0FE47F1B, 0xFD47EAC9931E5611, 0x9ECF69693F684A04,
      0x71C02736251798B6, 0x6C21272990F120AC, 0xE4727A188A905D0A,
      0x259CE02196F0FB6C, 0x5E34060EB8B01C23, 0xD13DA3CBF5A601C2,
      0xF127AEAAA3C7373B, 0xDB620698899E4B6D, 0x6E478EE5FA6DD2E9,
      0x1AE794F2917D2E95, 0x276ADD06C59EC853, 0x491CE0EDFE9825B0,
      0x6E74453240292289, 0x22B769287778C836, 0xBF35609B690EC521,
      0x33C6958E166EF7FB, 0xB68BFC69363BA321, 0xF80FA8B7954AEBFA,
      0x0BF5ACA4705A6293, 0x0556B78D45BCAAF2, 0xEE2CEF405184E046,
      0x73227D21D75B5FE8, 0xD0DF37F5BDB842D2, 0x28EE2A083406DB5A,
      0x374E44E23156B38C, 0x2337A79B3AE153E7, 0xF584A7417BA286F4,
      0x5E3C84336022F4D8, 0x59399EA49A971651, 0x2B320610ADC6F17F,
      0xC36EBC282E7312C2, 0x1C81100B2FE75440, 0x1372BAA075FFF382,
      0xE8937E82A1F75179, 0xBAC1E93B15E462CA, 0x562C0E62274601C0,
      0x6F4A0CB8ACFF7034, 0xEA51C1C9C8F23049, 0xCA413E3603CBDCCF,
      0xA7E5B91D287D545F, 0x1C323C89D01E9460, 0xA6DDEB12F0B41B72,
      0x4C4BD80B6559D8D2, 0x9D84AF3AF8CCF566, 0x1DAE74E2D7F65F4F,
      0x214AA9F23CF62937, 0xBA95E37E94F03C41, 0x00C40774F9799DE7,
      0x623CA815A53DC0A0, 0x2B966F603BAA005A, 0x4A7F01729330A03C,
      0x3AA3C3B6AF1ACE45, 0x8EEDAFF7174EBDC5, 0x78005039F4CEA4AA,
      0x4D36AAB2FCA2C150, 0xDDE323A66BF337F5, 0x6F7E47861B7A1277,
      0xB86088670CA3BAA8, 0x218C45C8727FBAA0, 0x76D3167331343EF7,
      0x78DF6DE9AADD9E63, 0x9DD67E3E0CCF388B, 0xB571630663016120,
      0x349C904FA4D6AFC9, 0xA4321D9FB73EC5D5, 0xA31CB6B845CF52B1,
      0xED771A139FE20B86, 0xC05857A1061CE915, 0xE1C69AF2BA7BE706,
      0x88DF0DDA58781E75, 0xBFE6E4B61B923F50, 0x2D1797888A57F9FC,
      0x37F0A88CB6BB2317, 0x1E5AF6EBC5D5CD77, 0xBCF0BB798CF609D2,
      0x6C74415B840C8F42, 0x5F92AC0AEFBB2A2A, 0xAEA80952AC83CDCC,
      0x148E6336080BC9FC, 0x440A9EAC0572D0BC, 0xBB2DCE23A2FCF164,
      0xC63F825E6738F990, 0x4F4B89A6AB0C59DF, 0x2B1B23B3AAE125F7,
      0x02AFCD3AA9D1B454, 0xFCEF5F3517819564, 0x54B73F4D8F06CD33,
      0x9B59C3BAA7819081, 0xF406BAA777860094, 0xC66B599CB8D22647,
      0x7DBC5F307AC4DB70,
  };

  for (int i = 0; i < 100; i++) {
    uint64_t have = 0;
    wuffs_base__slice_u8 data = ((wuffs_base__slice_u8){
        .ptr = (uint8_t*)(digits),
        .len = (size_t)(i),
    });

    if (mimic) {
#ifdef WUFFS_MIMIC
      have = mimic_xxhash64_one_shot_checksum_u64(data);
#endif  // WUFFS_MIMIC

    } else {
      wuffs_xxhash64__hasher checksum;
      CHECK_STATUS("initialize",
                   wuffs_xxhash64__hasher__initialize(
                       &checksum, sizeof checksum, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
      have = wuffs_xxhash64__hasher__update_u64(&checksum, data);
    }

    if (have != wants[i]) {
      RETURN_FAIL("i=%d: have 0x%016" PRIX64 ", want 0x%016" PRIX64, i, have,
                  wants[i]);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_xxhash64_pi() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_xxhash64_pi(false);
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_xxhash64_pi() {
  CHECK_FOCUS(__func__);
  return do_test_xxxxx_xxhash64_pi(true);
}

#endif  // WUFFS_MIMIC

// ---------------- XXHash64 Benches

uint64_t g_wuffs_xxhash64_unused_u64;

const char*  //
wuffs_bench_xxhash64(wuffs_base__io_buffer* dst,
                     wuffs_base__io_buffer* src,
                     uint32_t wuffs_initialize_flags,
                     uint64_t wlimit,
                     uint64_t rlimit) {
  uint64_t len = src->meta.wi - src->meta.ri;
  if (rlimit) {
    len = wuffs_base__u64__min(len, rlimit);
  }
  wuffs_xxhash64__hasher checksum = {0};
  CHECK_STATUS("initialize", wuffs_xxhash64__hasher__initialize(
                                 &checksum, sizeof checksum, WUFFS_VERSION,
                                 wuffs_initialize_flags));
  g_wuffs_xxhash64_unused_u64 = wuffs_xxhash64__hasher__update_u64(
      &checksum, ((wuffs_base__slice_u8){
                     .ptr = src->data.ptr + src->meta.ri,
                     .len = len,
                 }));
  src->meta.ri += len;
  return NULL;
}

const char*  //
bench_wuffs_xxhash64_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_xxhash64,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_xxhash64_midsummer_gt, UINT64_MAX, UINT64_MAX, 5000);
}

const char*  //
bench_wuffs_xxhash64_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_bench_xxhash64,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_src,
      &g_xxhash64_pi_gt, UINT64_MAX, UINT64_MAX, 500);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_xxhash64_10k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_bench_xxhash64, 0, tcounter_src,
                             &g_xxhash64_midsummer_gt, UINT64_MAX, UINT64_MAX,
                             5000);
}

const char*  //
bench_mimic_xxhash64_100k() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_bench_xxhash64, 0, tcounter_src,
                             &g_xxhash64_pi_gt, UINT64_MAX, UINT64_MAX, 500);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_xxhash64_golden,
    test_wuffs_xxhash64_interface,
    test_wuffs_xxhash64_pi,

#ifdef WUFFS_MIMIC

    test_mimic_xxhash64_pi,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_xxhash64_10k,
    bench_wuffs_xxhash64_100k,

#ifdef WUFFS_MIMIC

    bench_mimic_xxhash64_10k,
    bench_mimic_xxhash64_100k,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/xxhash64";
  return test_main(argc, argv, g_tests, g_benches);
}
