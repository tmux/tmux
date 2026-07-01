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
  $CC -std=c99 -Wall -Werror json.c && ./a.out
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
#define WUFFS_CONFIG__MODULE__JSON

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
// No mimic library.
#endif

// ---------------- Numeric Types Tests

const char*  //
test_wuffs_core_count_leading_zeroes_u64() {
  CHECK_FOCUS(__func__);

  struct {
    uint64_t num;
    uint32_t want;
  } test_cases[] = {
      {.num = 0x0000000000000000, .want = 64},
      {.num = 0x0000000000000001, .want = 63},
      {.num = 0x0000000000008001, .want = 48},
      {.num = 0x0000000040302010, .want = 33},
      {.num = 0x0123456789ABCDEF, .want = 7},
      {.num = 0x8000000000000001, .want = 0},
      {.num = 0xFFFFFFFFFFFFFFFF, .want = 0},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    uint32_t have = wuffs_base__count_leading_zeroes_u64(test_cases[tc].num);
    if (have != test_cases[tc].want) {
      RETURN_FAIL("0x%" PRIX64 ": have %" PRIu32 ", want %" PRIu32,
                  test_cases[tc].num, have, test_cases[tc].want);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_core_multiply_u64() {
  CHECK_FOCUS(__func__);

  struct {
    uint64_t x;
    uint64_t y;
    uint64_t want_hi;
    uint64_t want_lo;
  } test_cases[] = {
      {.x = 0x0000000000005678,
       .y = 0x0000000000001001,
       .want_hi = 0x0000000000000000,
       .want_lo = 0x000000000567D678},
      {.x = 0x00000000DEADBEEF,
       .y = 0x000000BEEEEEEEEF,
       .want_hi = 0x00000000000000A6,
       .want_lo = 0x14C912411FE97321},
      {.x = 0x0123456789ABCDEF,
       .y = 0x8080707066554321,
       .want_hi = 0x009234D666DAD50F,
       .want_lo = 0x89B3DE09506618CF},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__multiply_u64__output have =
        wuffs_base__multiply_u64(test_cases[tc].x, test_cases[tc].y);
    if ((have.hi != test_cases[tc].want_hi) ||
        (have.lo != test_cases[tc].want_lo)) {
      RETURN_FAIL("0x%" PRIX64 " * 0x%" PRIX64 ": have (0x%" PRIX64
                  ", 0x%" PRIX64 "), want (0x%" PRIX64 ", 0x%" PRIX64 ")",
                  test_cases[tc].x, test_cases[tc].y, have.hi, have.lo,
                  test_cases[tc].want_hi, test_cases[tc].want_lo);
    }
  }

  return NULL;
}

// ---------------- String Conversions Tests

// wuffs_private_impl__high_prec_dec__to_debug_string converts hpd into a
// human-readable NUL-terminated C string.
const char*  //
wuffs_private_impl__high_prec_dec__to_debug_string(
    wuffs_private_impl__high_prec_dec* hpd,
    wuffs_base__slice_u8 dst) {
  if (!hpd) {
    return "high_prec_dec__to_debug_string: invalid hpd";
  }
  uint8_t* p = dst.ptr;
  uint8_t* q = dst.ptr + dst.len;

  // Sign bit.
  if ((q - p) < 1) {
    goto too_short;
  }
  *p++ = hpd->negative ? '-' : '+';

  // Digits and decimal point.
  if (hpd->decimal_point > +WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE) {
    // We have "infinity".
    if ((q - p) < 3) {
      goto too_short;
    }
    *p++ = 'i';
    *p++ = 'n';
    *p++ = 'f';
    goto nul_terminator;

  } else if (hpd->decimal_point <
             -WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE) {
    // We have "epsilon": a very small number, equivalent to zero.
    if ((q - p) < 3) {
      goto too_short;
    }
    *p++ = 'e';
    *p++ = 'p';
    *p++ = 's';
    goto nul_terminator;

  } else if (hpd->num_digits == 0) {
    // We have "0".
    if ((q - p) < 1) {
      goto too_short;
    }
    *p++ = '0';
    goto nul_terminator;

  } else if (hpd->decimal_point < 0) {
    // Referring to the wuffs_private_impl__high_prec_dec typedef's comment, we
    // have something like ".00789".
    if ((q - p) < (hpd->num_digits + ((uint32_t)(-hpd->decimal_point)) + 1)) {
      goto too_short;
    }
    uint8_t* src = &hpd->digits[0];

    // Step A.1: write the ".".
    *p++ = '.';

    // Step A.2: write the "00".
    uint32_t n = ((uint32_t)(-hpd->decimal_point));
    if (n > 0) {
      memset(p, '0', n);
      p += n;
    }

    // Step A.3: write the "789".
    n = hpd->num_digits;
    while (n--) {
      *p++ = '0' | *src++;
    }

  } else if (((uint32_t)(hpd->decimal_point)) <= hpd->num_digits) {
    // Referring to the wuffs_private_impl__high_prec_dec typedef's comment, we
    // have something like "78.9".
    if ((q - p) < (hpd->num_digits + 1)) {
      goto too_short;
    }
    uint8_t* src = &hpd->digits[0];

    // Step B.1: write the "78".
    uint32_t n = ((uint32_t)(hpd->decimal_point));
    while (n--) {
      *p++ = '0' | *src++;
    }

    // Step B.2: write the ".".
    *p++ = '.';

    // Step B.3: write the "9".
    n = hpd->num_digits - ((uint32_t)(hpd->decimal_point));
    while (n--) {
      *p++ = '0' | *src++;
    }

  } else {
    // Referring to the wuffs_private_impl__high_prec_dec typedef's comment, we
    // have something like "78900.".
    if ((q - p) < (((uint32_t)(hpd->decimal_point)) + 1)) {
      goto too_short;
    }
    uint8_t* src = &hpd->digits[0];

    // Step C.1: write the "789".
    uint32_t n = hpd->num_digits;
    while (n--) {
      *p++ = '0' | *src++;
    }

    // Step C.2: write the "00".
    n = ((uint32_t)(hpd->decimal_point)) - hpd->num_digits;
    if (n > 0) {
      memset(p, '0', n);
      p += n;
    }

    // Step C.3: write the ".".
    *p++ = '.';
  }

  // Truncated bit.
  if (hpd->truncated) {
    if ((q - p) < 1) {
      goto too_short;
    }
    *p++ = '$';
  }

nul_terminator:
  if ((q - p) < 1) {
    goto too_short;
  }
  *p++ = '\x00';
  return NULL;

too_short:
  return "high_prec_dec__to_debug_string: dst buffer is too short";
}

const char*  //
test_wuffs_strconv_hpd_rounded_integer() {
  CHECK_FOCUS(__func__);

  struct {
    uint64_t want;
    const char* str;
  } test_cases[] = {
      {.want = 4, .str = "-3.9"},
      {.want = 3, .str = "-3.14159"},
      {.want = 0, .str = "+0"},
      {.want = 0, .str = "0.0000000009"},
      {.want = 0, .str = "0.1"},
      {.want = 1, .str = "0.9"},
      {.want = 12, .str = "1234e-2"},
      {.want = 57, .str = "5678e-2"},
      {.want = 60, .str = "60.0"},
      {.want = 60, .str = "60.4999"},
      {.want = 60, .str = "60.5"},
      {.want = 60, .str = "60.5000"},
      {.want = 61, .str = "60.5001"},
      {.want = 61, .str = "60.6"},
      {.want = 61, .str = "61.0"},
      {.want = 61, .str = "61.4999"},
      {.want = 62, .str = "61.5"},
      {.want = 62, .str = "61.5000"},
      {.want = 62, .str = "61.5001"},
      {.want = 62, .str = "61.6"},
      {.want = 62, .str = "62.0"},
      {.want = 62, .str = "62.4999"},
      {.want = 62, .str = "62.5"},
      {.want = 62, .str = "62.5000"},
      {.want = 63, .str = "62.5001"},
      {.want = 63, .str = "62.6"},
      {.want = 1000, .str = "999.999"},
      {.want = 4560000, .str = "456e+4"},

      // With round-to-even, ½ rounds to 0 but "a tiny bit more than ½" rounds
      // to 1, even if the HPD struct truncates that "1" digit.
      {.want = 0, .str = "0.5"},
      {.want = 1,  // 50 '0's per row.
       .str = "0.500000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000000"
              "00000000000000000000000000000000000000000000000001"},

      // Inputs with exactly 18 decimal digits before the decimal point.
      {.want = 123456789012345679, .str = "123456789012345678.9"},
      {.want = 1000000000000000000, .str = "999999999999999999.9"},

      // Inputs with exactly 19 decimal digits before the decimal point.
      {.want = UINT64_MAX, .str = "1234567890123456789"},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_private_impl__high_prec_dec hpd;
    CHECK_STATUS(
        "hpd__parse",
        wuffs_private_impl__high_prec_dec__parse(
            &hpd,
            wuffs_base__make_slice_u8((uint8_t*)(void*)test_cases[tc].str,
                                      strlen(test_cases[tc].str)),
            WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS));
    uint64_t have = wuffs_private_impl__high_prec_dec__rounded_integer(&hpd);
    if (have != test_cases[tc].want) {
      RETURN_FAIL("\"%s\": have %" PRIu64 ", want %" PRIu64, test_cases[tc].str,
                  have, test_cases[tc].want);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_strconv_hpd_shift() {
  CHECK_FOCUS(__func__);

  struct {
    const char* str;
    int32_t shift;  // -ve means left shift, +ve means right shift.
    const char* want;
  } test_cases[] = {
      {.str = "0", .shift = +2, .want = "+0"},
      {.str = "1", .shift = +3, .want = "+.125"},
      {.str = "12e3", .shift = +5, .want = "+375."},
      {.str = "-0.007", .shift = +8, .want = "-.00002734375"},
      {.str = "3.14159E+26",
       .shift = +60,
       .want = "+272489496.244698869986677891574800014495849609375"},

      {.str = "0", .shift = -2, .want = "+0"},
      {.str = ".125", .shift = -3, .want = "+1."},
      {.str = "3750e-1", .shift = -5, .want = "+12000."},
      {.str = "-2.734375e-5", .shift = -8, .want = "-.007"},
      {.str = "+272489496.244698869986677891574800014495849609375",
       .shift = -60,
       .want = "+314159000000000000000000000."},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_private_impl__high_prec_dec hpd;
    CHECK_STATUS(
        "hpd__parse",
        wuffs_private_impl__high_prec_dec__parse(
            &hpd,
            wuffs_base__make_slice_u8((uint8_t*)(void*)test_cases[tc].str,
                                      strlen(test_cases[tc].str)),
            WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS));
    int32_t shift = test_cases[tc].shift;
    if (shift > 0) {
      wuffs_private_impl__high_prec_dec__small_rshift(&hpd, (uint32_t)(+shift));
    } else if (shift < 0) {
      wuffs_private_impl__high_prec_dec__small_lshift(&hpd, (uint32_t)(-shift));
    }

    uint8_t have[1024];
    CHECK_STRING(wuffs_private_impl__high_prec_dec__to_debug_string(
        &hpd, wuffs_base__make_slice_u8(have, WUFFS_TESTLIB_ARRAY_SIZE(have))));
    if (strcmp(((const char*)(void*)(have)), test_cases[tc].want)) {
      RETURN_FAIL("\"%s\" %s %" PRId32 ":\n    have: \"%s\"\n    want: \"%s\"",
                  test_cases[tc].str, ((shift > 0) ? ">>" : "<<"),
                  ((shift > 0) ? +shift : -shift), have, test_cases[tc].want);
    }
  }
  return NULL;
}

// ----------------

const char*  //
test_wuffs_strconv_ieee_754_bit_representation_from_u16() {
  CHECK_FOCUS(__func__);

  double inf = 1.0 / 0.0;
  uint64_t nan_u64 = 0x7FFFFFFFFFFFFFFF;
  double nan =
      wuffs_base__ieee_754_bit_representation__from_u64_to_f64(nan_u64);

  struct {
    uint16_t u16_bits;
    uint64_t want_u64;
    double want_f64;
  } test_cases[] = {
      {
          .u16_bits = 0x0000,
          .want_u64 = 0x0000000000000000,
          .want_f64 = 0.0,
      },
      {
          .u16_bits = 0x0001,
          .want_u64 = 0x3E70000000000000,
          .want_f64 = 0.000000059604644775390625,
      },
      {
          .u16_bits = 0x0123,
          .want_u64 = 0x3EF2300000000000,
          .want_f64 = 0.000017344951629638671875,
      },
      {
          .u16_bits = 0x03FF,
          .want_u64 = 0x3F0FF80000000000,
          .want_f64 = 0.000060975551605224609375,
      },
      {
          .u16_bits = 0x0400,
          .want_u64 = 0x3F10000000000000,
          .want_f64 = 0.00006103515625,
      },
      {
          .u16_bits = 0x0401,
          .want_u64 = 0x3F10040000000000,
          .want_f64 = 0.000061094760894775390625,
      },
      {
          .u16_bits = 0x3C00,
          .want_u64 = 0x3FF0000000000000,
          .want_f64 = 1.0,
      },
      {
          .u16_bits = 0x4580,
          .want_u64 = 0x4016000000000000,
          .want_f64 = 5.5,
      },
      {
          .u16_bits = 0x7BFF,
          .want_u64 = 0x40EFFC0000000000,
          .want_f64 = 65504.0,
      },
      {
          .u16_bits = 0x7C00,
          .want_u64 = 0x7FF0000000000000,
          .want_f64 = inf,
      },
      {
          .u16_bits = 0x7FFF,
          .want_u64 = nan_u64,
          .want_f64 = nan,
      },
      {
          .u16_bits = 0xFC00,
          .want_u64 = 0xFFF0000000000000,
          .want_f64 = -inf,
      },
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    double have_f64 = wuffs_base__ieee_754_bit_representation__from_u16_to_f64(
        test_cases[tc].u16_bits);
    double want_f64 = test_cases[tc].want_f64;
    // See if they're equal, considering that NaN != NaN.
    bool equal = ((have_f64 == want_f64) ||
                  ((have_f64 != have_f64) && (want_f64 != want_f64)));
    if (!equal) {
      RETURN_FAIL("tc=%zu: to_f64: have %g, want %g", tc, have_f64, want_f64);
    }

    uint64_t have_u64 =
        wuffs_base__ieee_754_bit_representation__from_f64_to_u64(have_f64);
    uint64_t want_u64 = test_cases[tc].want_u64;
    // There's more than one representation of NaN.
    if ((have_u64 != want_u64) && (want_u64 != nan_u64)) {
      RETURN_FAIL("tc=%zu: to_u64: have 0x%016" PRIX64 ", want 0x%016" PRIX64,
                  tc, have_u64, want_u64);
    }

    int noise;
    for (noise = 0; noise < 2; noise++) {
      if ((noise > 0) && ((want_f64 == inf) || (want_f64 == -inf))) {
        continue;
      }
      wuffs_base__lossy_value_u16 lv =
          wuffs_base__ieee_754_bit_representation__from_f64_to_u16_truncate(
              wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                  want_u64 ^ ((uint64_t)noise)));
      if (lv.value != test_cases[tc].u16_bits) {
        RETURN_FAIL("tc=%zu: noise=%d: to_u16 value: have 0x%04" PRIX16
                    ", want 0x%04" PRIX16,
                    tc, noise, lv.value, test_cases[tc].u16_bits);
      }
      int want_lossy = (want_f64 == want_f64) ? noise : 0;
      if (((int)lv.lossy) != want_lossy) {
        RETURN_FAIL("tc=%zu: noise=%d: to_u16 lossy: have %d, want %d", tc,
                    noise, ((int)(lv.lossy)), want_lossy);
      }
    }
  }

  return NULL;
}

const char*  //
test_wuffs_strconv_ieee_754_bit_representation_from_u32() {
  CHECK_FOCUS(__func__);

  double inf = 1.0 / 0.0;
  uint64_t nan_u64 = 0x7FFFFFFFFFFFFFFF;
  double nan =
      wuffs_base__ieee_754_bit_representation__from_u64_to_f64(nan_u64);

  struct {
    uint32_t u32_bits;
    uint64_t want_u64;
    double want_f64;
  } test_cases[] = {
      {
          .u32_bits = 0x00000000,
          .want_u64 = 0x0000000000000000,
          .want_f64 = 0.0,
      },
      {
          .u32_bits = 0x00000001,
          .want_u64 = 0x36A0000000000000,
          .want_f64 = 1.4012984643248170709237295832899161312802619418765e-45,
      },
      {
          .u32_bits = 0x00000123,
          .want_u64 = 0x3722300000000000,
          .want_f64 = 4.0777785311852176763880530873736559420255622508607e-43,
      },
      {
          .u32_bits = 0x007FFFFF,
          .want_u64 = 0x380FFFFFC0000000,
          .want_f64 = 1.1754942106924410754870294448492873488270524287459e-38,
      },
      {
          .u32_bits = 0x00800000,
          .want_u64 = 0x3810000000000000,
          .want_f64 = 1.1754943508222875079687365372222456778186655567721e-38,
      },
      {
          .u32_bits = 0x00800001,
          .want_u64 = 0x3810000020000000,
          .want_f64 = 1.1754944909521339404504436295952040068102786847983e-38,
      },
      {
          .u32_bits = 0x3F800000,
          .want_u64 = 0x3FF0000000000000,
          .want_f64 = 1.0,
      },
      {
          .u32_bits = 0x40B00000,
          .want_u64 = 0x4016000000000000,
          .want_f64 = 5.5,
      },
      {
          .u32_bits = 0x7F7FFFFF,
          .want_u64 = 0x47EFFFFFE0000000,
          .want_f64 = 3.40282346638528859811704183484516925440e38,
      },
      {
          .u32_bits = 0x7F800000,
          .want_u64 = 0x7FF0000000000000,
          .want_f64 = inf,
      },
      {
          .u32_bits = 0x7FFFFFFF,
          .want_u64 = nan_u64,
          .want_f64 = nan,
      },
      {
          .u32_bits = 0xFF800000,
          .want_u64 = 0xFFF0000000000000,
          .want_f64 = -inf,
      },
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    double have_f64 = wuffs_base__ieee_754_bit_representation__from_u32_to_f64(
        test_cases[tc].u32_bits);
    double want_f64 = test_cases[tc].want_f64;
    // See if they're equal, considering that NaN != NaN.
    bool equal = ((have_f64 == want_f64) ||
                  ((have_f64 != have_f64) && (want_f64 != want_f64)));
    if (!equal) {
      RETURN_FAIL("tc=%zu: to_f64: have %g, want %g", tc, have_f64, want_f64);
    }

    uint64_t have_u64 =
        wuffs_base__ieee_754_bit_representation__from_f64_to_u64(have_f64);
    uint64_t want_u64 = test_cases[tc].want_u64;
    // There's more than one representation of NaN.
    if ((have_u64 != want_u64) && (want_u64 != nan_u64)) {
      RETURN_FAIL("tc=%zu: to_u64: have 0x%016" PRIX64 ", want 0x%016" PRIX64,
                  tc, have_u64, want_u64);
    }

    int noise;
    for (noise = 0; noise < 2; noise++) {
      if ((noise > 0) && ((want_f64 == inf) || (want_f64 == -inf))) {
        continue;
      }
      wuffs_base__lossy_value_u32 lv =
          wuffs_base__ieee_754_bit_representation__from_f64_to_u32_truncate(
              wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
                  want_u64 ^ ((uint64_t)noise)));
      if (lv.value != test_cases[tc].u32_bits) {
        RETURN_FAIL("tc=%zu: noise=%d: to_u32 value: have 0x%08" PRIX32
                    ", want 0x%08" PRIX32,
                    tc, noise, lv.value, test_cases[tc].u32_bits);
      }
      int want_lossy = (want_f64 == want_f64) ? noise : 0;
      if (((int)lv.lossy) != want_lossy) {
        RETURN_FAIL("tc=%zu: noise=%d: to_u32 lossy: have %d, want %d", tc,
                    noise, ((int)(lv.lossy)), want_lossy);
      }
    }
  }

  return NULL;
}

// ----------------

const char*  //
test_wuffs_strconv_base_16() {
  CHECK_FOCUS(__func__);
  const bool src_closed = true;

  {
    const char* str = "6A6b7";  // The "7" should cause "#base: bad data".
    wuffs_base__slice_u8 dst = g_have_slice_u8;
    wuffs_base__slice_u8 src =
        wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str));
    wuffs_base__transform__output have = wuffs_base__base_16__decode2(
        dst, src, src_closed, WUFFS_BASE__BASE_16__DEFAULT_OPTIONS);
    if (have.status.repr != wuffs_base__error__bad_data) {
      RETURN_FAIL("decode2: have \"%s\", want \"%s\"", have.status.repr,
                  wuffs_base__error__bad_data);
    }
    if (have.num_dst != 2) {
      RETURN_FAIL("decode2: num_dst: have %zu, want 2", have.num_dst);
    }
    if (have.num_src != 4) {
      RETURN_FAIL("decode2: num_src: have %zu, want 3", have.num_src);
    }
    if (g_have_array_u8[0] != 0x6A) {
      RETURN_FAIL("decode2: dst[0]: have 0x%02X, want 0x6A",
                  (int)(g_have_array_u8[0]));
    }
    if (g_have_array_u8[1] != 0x6B) {
      RETURN_FAIL("decode2: dst[1]: have 0x%02X, want 0x6B",
                  (int)(g_have_array_u8[1]));
    }
  }

  {
    const char* str = "\\xa9\\x00\\xFe";
    wuffs_base__slice_u8 dst = g_have_slice_u8;
    wuffs_base__slice_u8 src =
        wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str));
    wuffs_base__transform__output have = wuffs_base__base_16__decode4(
        dst, src, src_closed, WUFFS_BASE__BASE_16__DEFAULT_OPTIONS);
    if (have.status.repr) {
      RETURN_FAIL("decode2: %s", have.status.repr);
    }
    if (have.num_dst != 3) {
      RETURN_FAIL("decode4: num_dst: have %zu, want 3", have.num_dst);
    }
    if (have.num_src != 12) {
      RETURN_FAIL("decode4: num_src: have %zu, want 3", have.num_src);
    }
    if (g_have_array_u8[0] != 0xA9) {
      RETURN_FAIL("decode4: dst[0]: have 0x%02X, want 0xA9",
                  (int)(g_have_array_u8[0]));
    }
    if (g_have_array_u8[1] != 0x00) {
      RETURN_FAIL("decode4: dst[1]: have 0x%02X, want 0x00",
                  (int)(g_have_array_u8[1]));
    }
    if (g_have_array_u8[2] != 0xFE) {
      RETURN_FAIL("decode4: dst[2]: have 0x%02X, want 0xFE",
                  (int)(g_have_array_u8[2]));
    }
  }

  return NULL;
}

const char*  //
test_wuffs_strconv_base_64() {
  CHECK_FOCUS(__func__);

  const char* foobar = "foobar";
  const char* wants[] = {
      "",          //
      "Zg==",      //
      "Zm8=",      //
      "Zm9v",      //
      "Zm9vYg==",  //
      "Zm9vYmE=",  //
      "Zm9vYmFy",  //
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(wants); tc++) {
    const bool src_closed = true;

    wuffs_base__transform__output e = wuffs_base__base_64__encode(
        g_have_slice_u8,
        wuffs_base__make_slice_u8(((uint8_t*)(void*)foobar), tc), src_closed,
        WUFFS_BASE__BASE_64__ENCODE_EMIT_PADDING);
    if (e.status.repr) {
      RETURN_FAIL("tc=%zu: encode: \"%s\"", tc, e.status.repr);
    }
    if (e.num_src != tc) {
      RETURN_FAIL("tc=%zu: encode: num_src: have %zu, want %zu", tc, e.num_src,
                  tc);
    }
    if (e.num_dst != (((tc + 2) / 3) * 4)) {
      RETURN_FAIL("tc=%zu: encode: num_dst: have %zu, want %zu", tc, e.num_dst,
                  (((tc + 2) / 3) * 4));
    }
    if ((e.num_dst + 1) > g_have_slice_u8.len) {
      RETURN_FAIL("tc=%zu: encode: num_dst is too large", tc);
    }
    g_have_slice_u8.ptr[e.num_dst] = '\x00';
    if (strncmp((const char*)(g_have_slice_u8.ptr), wants[tc], e.num_dst) !=
        0) {
      RETURN_FAIL("tc=%zu: encode:\nhave \"%s\"\nwant \"%s\"", tc,
                  g_have_slice_u8.ptr, wants[tc]);
    }

    int trim;
    for (trim = 0; trim < 2; trim++) {
      size_t n = e.num_dst;
      if (trim) {
        while ((n > 0) && (g_have_slice_u8.ptr[n - 1] == '=')) {
          n--;
        }
      }

      wuffs_base__transform__output d = wuffs_base__base_64__decode(
          g_work_slice_u8, wuffs_base__make_slice_u8(g_have_slice_u8.ptr, n),
          src_closed, WUFFS_BASE__BASE_64__DECODE_ALLOW_PADDING);
      if (d.status.repr) {
        RETURN_FAIL("tc=%zu: trim=%d: decode: \"%s\"", tc, trim, d.status.repr);
      }
      if (d.num_src != n) {
        RETURN_FAIL("tc=%zu: trim=%d: decode: num_src: have %zu, want %zu", tc,
                    trim, d.num_src, n);
      }
      if (d.num_dst != tc) {
        RETURN_FAIL("tc=%zu: trim=%d: decode: num_dst: have %zu, want %zu", tc,
                    trim, d.num_dst, tc);
      }
      if ((d.num_dst + 1) > g_work_slice_u8.len) {
        RETURN_FAIL("tc=%zu: trim=%d: decode: num_dst is too large", tc, trim);
      }
      g_work_slice_u8.ptr[d.num_dst] = '\x00';
      if ((tc + 1) > g_want_slice_u8.len) {
        RETURN_FAIL("tc=%zu: trim=%d: decode: tc is too large", tc, trim);
      }
      memcpy(g_want_slice_u8.ptr, foobar, tc);
      g_want_slice_u8.ptr[tc] = '\x00';
      if (strcmp(((const char*)(g_work_slice_u8.ptr)),
                 ((const char*)(g_want_slice_u8.ptr))) != 0) {
        RETURN_FAIL("tc=%zu: trim=%d: decode:\nhave \"%s\"\nwant \"%s\"", tc,
                    trim, g_work_slice_u8.ptr, g_want_slice_u8.ptr);
      }
    }
  }

  return NULL;
}

// ----------------

const char*  //
test_wuffs_strconv_parse_number_f64_options() {
  CHECK_FOCUS(__func__);

  // Test WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES.
  {
    for (int o = 0; o < 2; o++) {
      const char* str = "001.25";
      wuffs_base__result_f64 r = wuffs_base__parse_number_f64(
          wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str)),
          (o ? WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES
             : WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS));

      if (o == 0) {
        if (r.status.repr != wuffs_base__error__bad_argument) {
          RETURN_FAIL(
              "ALLOW_MULTIPLE_LEADING_ZEROES off: have \"%s\", want \"%s\"",
              r.status.repr, wuffs_base__error__bad_argument);
        }
        continue;
      }

      CHECK_STATUS("ALLOW_MULTIPLE_LEADING_ZEROES on", r.status);
      uint64_t have =
          wuffs_base__ieee_754_bit_representation__from_f64_to_u64(r.value);
      uint64_t want = 0x3FF4000000000000;
      if (have != want) {
        RETURN_FAIL("ALLOW_MULTIPLE_LEADING_ZEROES on: have 0x%016" PRIX64
                    ", want 0x%016" PRIX64,
                    have, want);
      }
    }
  }

  // Test WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES.
  {
    for (int o = 0; o < 2; o++) {
      const char* str = "_1.2__5";
      wuffs_base__result_f64 r = wuffs_base__parse_number_f64(
          wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str)),
          (o ? WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES
             : WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS));

      if (o == 0) {
        if (r.status.repr != wuffs_base__error__bad_argument) {
          RETURN_FAIL("ALLOW_UNDERSCORES off: have \"%s\", want \"%s\"",
                      r.status.repr, wuffs_base__error__bad_argument);
        }
        continue;
      }

      CHECK_STATUS("ALLOW_UNDERSCORES on", r.status);
      uint64_t have =
          wuffs_base__ieee_754_bit_representation__from_f64_to_u64(r.value);
      uint64_t want = 0x3FF4000000000000;
      if (have != want) {
        RETURN_FAIL("ALLOW_UNDERSCORES on: have 0x%016" PRIX64
                    ", want 0x%016" PRIX64,
                    have, want);
      }
    }
  }

  // Test WUFFS_BASE__PARSE_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA.
  {
    for (int o = 0; o < 2; o++) {
      const char* str = "1,75";
      wuffs_base__result_f64 r = wuffs_base__parse_number_f64(
          wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str)),
          (o ? WUFFS_BASE__PARSE_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA
             : WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS));

      if (o == 0) {
        if (r.status.repr != wuffs_base__error__bad_argument) {
          RETURN_FAIL(
              "DECIMAL_SEPARATOR_IS_A_COMMA off: have \"%s\", want \"%s\"",
              r.status.repr, wuffs_base__error__bad_argument);
        }
        continue;
      }

      CHECK_STATUS("DECIMAL_SEPARATOR_IS_A_COMMA on", r.status);
      uint64_t have =
          wuffs_base__ieee_754_bit_representation__from_f64_to_u64(r.value);
      uint64_t want = 0x3FFC000000000000;
      if (have != want) {
        RETURN_FAIL("DECIMAL_SEPARATOR_IS_A_COMMA on: have 0x%016" PRIX64
                    ", want 0x%016" PRIX64,
                    have, want);
      }
    }
  }

  // Test WUFFS_BASE__PARSE_NUMBER_FXX__REJECT_INF_AND_NAN.
  {
    for (int o = 0; o < 4; o++) {
      const char* str = (o & 2) ? "1e999" : "nan";
      wuffs_base__result_f64 r = wuffs_base__parse_number_f64(
          wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str)),
          ((o & 1) ? WUFFS_BASE__PARSE_NUMBER_FXX__REJECT_INF_AND_NAN
                   : WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS));

      if (o & 1) {
        if (r.status.repr != wuffs_base__error__bad_argument) {
          RETURN_FAIL("REJECT_INF_AND_NAN off: have \"%s\", want \"%s\"",
                      r.status.repr, wuffs_base__error__bad_argument);
        }
        continue;
      }

      if (r.status.repr != NULL) {
        RETURN_FAIL("REJECT_INF_AND_NAN on: have \"%s\", want NULL",
                    r.status.repr);
      }
    }
  }

  return NULL;
}

const char*  //
test_wuffs_strconv_parse_number_f64_regular() {
  CHECK_FOCUS(__func__);

  const uint64_t fail = 0xDEADBEEF;

  struct {
    uint64_t want;
    const char* str;
  } test_cases[] = {
      // If adding new cases, consider updating u64TestCases in
      // script/print-render-number-f64-tests.go

      {.want = 0x0000000000000000, .str = "+0.0"},
      {.want = 0x0000000000000000, .str = "0"},
      {.want = 0x0000000000000000, .str = "0e0"},
      {.want = 0x0000000000000000, .str = "0e99"},
      {.want = 0x0000000000000000, .str = "1e-332"},
      {.want = 0x0000000000000001, .str = "4.9406564584124654e-324"},
      {.want = 0x0000000000000002, .str = "9.8813129168249309e-324"},
      {.want = 0x0000000000000003, .str = "1.4821969375237396e-323"},
      {.want = 0x000730D67819E8D2, .str = "1e-308"},
      {.want = 0x000FFFFFFFFFFFFF, .str = "2.2250738585072009E-308"},
      {.want = 0x0010000000000000, .str = "2.2250738585072014E-308"},
      {.want = 0x0031FA182C40C60D, .str = "1e-307"},
      {.want = 0x369C2DF8DA5B6CA8, .str = "1.234e-45"},
      {.want = 0x369C314ABE948EB1,
       .str = "0.0000000000000000000000000000000000000000000012345678900000"},
      {.want = 0x3E70000000000000, .str = "5.9604644775390625e-8"},
      {.want = 0x3F88000000000000, .str = "0.01171875"},
      {.want = 0x3FD0000000000000, .str = ".25"},
      {.want = 0x3FD3333333333333,
       .str = "0.2999999999999999888977697537484345957636833190917968750000"},
      {.want = 0x3FD3333333333333, .str = "0.3"},
      {.want = 0x3FD3333333333334, .str = "0.30000000000000004"},
      {.want = 0x3FD3333333333334,
       .str = "0.3000000000000000444089209850062616169452667236328125000000"},
      {.want = 0x3FD5555555555555, .str = "0.333333333333333333333333333333"},
      {.want = 0x3FEFFFFFFFFFFFFF, .str = "0.99999999999999988898"},
      {.want = 0x3FF0000000000000, .str = "0.999999999999999999999999999999"},
      {.want = 0x3FF0000000000000, .str = "1"},
      {.want = 0x3FF0000000000001, .str = "1.0000000000000002"},
      {.want = 0x3FF0000000000002, .str = "1.0000000000000004"},
      {.want = 0x3FF4000000000000, .str = "1.25"},
      {.want = 0x3FF8000000000000, .str = "+1.5"},
      {.want = 0x4008000000000000, .str = "3"},
      {.want = 0x400921F9F01B866E, .str = "3.14159"},
      {.want = 0x400921FB54442D11, .str = "3.14159265358979"},
      {.want = 0x400921FB54442D18, .str = "3.141592653589793"},
      {.want = 0x400921FB54442D18, .str = "3.141592653589793238462643383279"},
      {.want = 0x400921FB54442D18,
       .str = "3.1415926535897932384626433832795028841971693993751"},
      {.want = 0x400C000000000000, .str = "3.5"},
      {.want = 0x4014000000000000, .str = "5"},
      {.want = 0x4036000000000000, .str = "22"},
      {.want = 0x4036000000000000, .str = "_+__2_2__."},
      {.want = 0x4037000000000000, .str = "23"},
      {.want = 0x4038000000000000, .str = "2.4E+00000000001"},
      {.want = 0x4038000000000000, .str = "2.4E001"},
      {.want = 0x4038000000000000, .str = "2.4E1"},
      {.want = 0x4038000000000000, .str = "24"},
      {.want = 0x4038000000000000, .str = "2400_00000_00000.00000_e-_1_2"},
      {.want = 0x405E9AA48FBB2888, .str = "122.416294033786585"},
      {.want = 0x405E9AA48FBB2888, .str = "122.41629403378658"},
      {.want = 0x405E9AA48FBB2889, .str = "122.4162940337866"},
      {.want = 0x40FE240C9FCB0C02, .str = "123456.789012"},
      {.want = 0x41E0246690000001,
       .str = "2.16656806400000023841857910156251e9"},
      {.want = 0x4202A05F20000000, .str = "1e10"},
      {.want = 0x4330000000000000, .str = "4503599627370496"},  // 1 << 52.
      {.want = 0x4330000000000000, .str = "4503599627370496.5"},
      {.want = 0x4330000000000001, .str = "4503599627370497"},
      {.want = 0x4330000000000002, .str = "4503599627370497.5"},
      {.want = 0x4330000000000002, .str = "4503599627370498"},
      {.want = 0x433FFFFFFFFFFFFE, .str = "9007199254740990"},
      {.want = 0x433FFFFFFFFFFFFF, .str = "9.007199254740991e+15"},
      {.want = 0x4340000000000000, .str = "9007199254740992"},  // 1 << 53.
      {.want = 0x4340000000000000, .str = "9007199254740993"},
      {.want = 0x4340000000000001, .str = "9007199254740994"},
      {.want = 0x4340000000000002, .str = "9007199254740995"},
      {.want = 0x4340000000000002, .str = "9007199254740996"},
      {.want = 0x4340000000000002, .str = "9_007__199_254__740_996"},
      {.want = 0x4370000000000000, .str = "7.2057594037927933e+16"},
      {.want = 0x43E158E460913D00, .str = "9999999999999999999"},
      {.want = 0x43F002F1776DDA67, .str = "18459999196907202592"},
      {.want = 0x4415AF1D78B58C40, .str = "1e20"},
      {.want = 0x44B52D02C7E14AF6, .str = "1e+23"},
      {.want = 0x44B52D02C7E14AF6, .str = "1e23"},
      {.want = 0x46293E5939A08CEA, .str = "1e30"},
      {.want = 0x54B249AD2594C37D, .str = "+1E+100"},
      {.want = 0x54B249AD2594C37D, .str = "+_1_E_+_1_0_0_"},
      {.want = 0x54B2987670ADB613, .str = "1.0168286519992372611942638e+100"},
      {.want = 0x7BBA44DF832B8D46, .str = "1e+288"},
      {.want = 0x7BF06B0BB1FB384C, .str = "1e+289"},
      {.want = 0x7C2485CE9E7A065F, .str = "1e+290"},
      {.want = 0x7FAC7B1F3CAC7433, .str = "9999999999999999999e+288"},
      {.want = 0x7FE1CCF385EBC8A0, .str = "9999999999999999999e+289"},
      {.want = 0x7FEFFFFFFFFFFFFF, .str = "1.7976931348623157e308"},
      {.want = 0x7FF0000000000000, .str = "1.8e308"},
      {.want = 0x7FF0000000000000, .str = "1e+316"},
      {.want = 0x7FF0000000000000,
       .str = "10000000000000000000000000000000000000000000e+308"},
      {.want = 0x7FF0000000000000, .str = "1e999"},
      {.want = 0x7FF0000000000000, .str = "9999999999999999999e+290"},
      {.want = 0x7FF0000000000000, .str = "__InFinity__"},
      {.want = 0x7FF0000000000000, .str = "inf"},
      {.want = 0x7FFFFFFFFFFFFFFF, .str = "+nan"},
      {.want = 0x7FFFFFFFFFFFFFFF, .str = "_+_NaN_"},
      {.want = 0x7FFFFFFFFFFFFFFF, .str = "nan"},
      {.want = 0x8000000000000000, .str = "-0.000e0"},
      {.want = 0xC008000000000000, .str = "-3"},
      {.want = 0xFFF0000000000000, .str = "-2e308"},
      {.want = 0xFFF0000000000000, .str = "-inf"},
      {.want = 0xFFFFFFFFFFFFFFFF, .str = "-NAN"},

      // If adding new cases, consider updating u64TestCases in
      // script/print-render-number-f64-tests.go

      {.want = fail, .str = " 0"},
      {.want = fail, .str = ""},
      {.want = fail, .str = "."},
      {.want = fail, .str = "00"},
      {.want = fail, .str = "001.2"},
      {.want = fail, .str = "06.44"},
      {.want = fail, .str = "0644"},
      {.want = fail, .str = "1234 67.8e9"},
      {.want = fail, .str = "2,345,678"},  // Two ','s.
      {.want = fail, .str = "2.345,678"},  // One '.' and one ','.
      {.want = fail, .str = "7 "},
      {.want = fail, .str = "7 .9"},
      {.want = fail, .str = "7e"},
      {.want = fail, .str = "7e-"},
      {.want = fail, .str = "7e-+1"},
      {.want = fail, .str = "7e++1"},
      {.want = fail, .str = "NAN "},
      {.want = fail, .str = "NANA"},
      {.want = fail, .str = "inf_inity"},
      {.want = fail, .str = "nun"},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__result_f64 r = wuffs_base__parse_number_f64(
        wuffs_base__make_slice_u8((uint8_t*)(void*)test_cases[tc].str,
                                  strlen(test_cases[tc].str)),
        WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES);
    uint64_t have =
        (r.status.repr == NULL)
            ? wuffs_base__ieee_754_bit_representation__from_f64_to_u64(r.value)
            : fail;
    if (have != test_cases[tc].want) {
      RETURN_FAIL("\"%s\": have 0x%016" PRIX64 ", want 0x%016" PRIX64,
                  test_cases[tc].str, have, test_cases[tc].want);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_strconv_parse_number_i64() {
  CHECK_FOCUS(__func__);

  const int64_t fail = 0xDEADBEEF;

  struct {
    int64_t want;
    const char* str;
  } test_cases[] = {
      {.want = +0x0000000000000000, .str = "+0"},
      {.want = +0x0000000000000000, .str = "-0"},
      {.want = +0x0000000000000000, .str = "0"},
      {.want = +0x000000000000012C, .str = "+300"},
      {.want = +0x7FFFFFFFFFFFFFFF, .str = "+9223372036854775807"},
      {.want = +0x7FFFFFFFFFFFFFFF, .str = "9223372036854775807"},
      {.want = -0x0000000000000002, .str = "-2"},
      {.want = -0x00000000000000AB, .str = "_-_0x_AB"},
      {.want = -0x7FFFFFFFFFFFFFFF, .str = "-9223372036854775807"},
      {.want = (int64_t)(-0x8000000000000000), .str = "-9223372036854775808"},

      {.want = fail, .str = "+ 1"},
      {.want = fail, .str = "++1"},
      {.want = fail, .str = "+-1"},
      {.want = fail, .str = "+9223372036854775808"},  // 1 << 63.
      {.want = fail, .str = "-"},
      {.want = fail, .str = "-+1"},
      {.want = fail, .str = "-0x8000000000000001"},   // -((1 << 63) + 1).
      {.want = fail, .str = "-9223372036854775809"},  // -((1 << 63) + 1).
      {.want = fail, .str = "0x8000000000000000"},    // 1 << 63.
      {.want = fail, .str = "1-"},
      {.want = fail, .str = "9223372036854775808"},  // 1 << 63.
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__result_i64 r = wuffs_base__parse_number_i64(
        wuffs_base__make_slice_u8((uint8_t*)(void*)test_cases[tc].str,
                                  strlen(test_cases[tc].str)),
        WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES);
    int64_t have = (r.status.repr == NULL) ? r.value : fail;
    if (have != test_cases[tc].want) {
      RETURN_FAIL("\"%s\": have 0x%" PRIX64 ", want 0x%" PRIX64,
                  test_cases[tc].str, have, test_cases[tc].want);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_strconv_parse_number_u64() {
  CHECK_FOCUS(__func__);

  const uint64_t fail = 0xDEADBEEF;

  struct {
    uint64_t want;
    const char* str;
  } test_cases[] = {
      {.want = 0x0000000000000000, .str = "0"},
      {.want = 0x0000000000000000, .str = "0_"},
      {.want = 0x0000000000000000, .str = "0d0"},
      {.want = 0x0000000000000000, .str = "0x000"},
      {.want = 0x0000000000000000, .str = "_0"},
      {.want = 0x0000000000000000, .str = "__0__"},
      {.want = 0x000000000000004A, .str = "0x4A"},
      {.want = 0x000000000000004B, .str = "0x__4_B_"},
      {.want = 0x000000000000007B, .str = "123"},
      {.want = 0x000000000000007C, .str = "12_4"},
      {.want = 0x000000000000007D, .str = "_1__2________5_"},
      {.want = 0x00000000000001F4, .str = "0d500"},
      {.want = 0x00000000000001F5, .str = "0D___5_01__"},
      {.want = 0x00000000FFFFFFFF, .str = "4294967295"},
      {.want = 0x0000000100000000, .str = "4294967296"},
      {.want = 0x0123456789ABCDEF, .str = "0x0123456789ABCDEF"},
      {.want = 0x0123456789ABCDEF, .str = "0x0123456789abcdef"},
      {.want = 0xFFFFFFFFFFFFFFF9, .str = "18446744073709551609"},
      {.want = 0xFFFFFFFFFFFFFFFA, .str = "18446744073709551610"},
      {.want = 0xFFFFFFFFFFFFFFFE, .str = "0xFFFFffffFFFFfffe"},
      {.want = 0xFFFFFFFFFFFFFFFE, .str = "18446744073709551614"},
      {.want = 0xFFFFFFFFFFFFFFFF, .str = "0xFFFF_FFFF_FFFF_FFFF"},
      {.want = 0xFFFFFFFFFFFFFFFF, .str = "18446744073709551615"},

      {.want = fail, .str = " "},
      {.want = fail, .str = " 0"},
      {.want = fail, .str = " 12 "},
      {.want = fail, .str = ""},
      {.want = fail, .str = "+0"},
      {.want = fail, .str = "+1"},
      {.want = fail, .str = "-0"},
      {.want = fail, .str = "-1"},
      {.want = fail, .str = "0 "},
      {.want = fail, .str = "00"},
      {.want = fail, .str = "000000x"},
      {.want = fail, .str = "000000x0"},
      {.want = fail, .str = "007"},
      {.want = fail, .str = "0644"},
      {.want = fail, .str = "0_0"},
      {.want = fail, .str = "0_x1"},
      {.want = fail, .str = "0d___"},
      {.want = fail, .str = "0x"},
      {.want = fail, .str = "0x10000000000000000"},      // 1 << 64.
      {.want = fail, .str = "0x1_0000_0000_0000_0000"},  // 1 << 64.
      {.want = fail, .str = "1 23"},
      {.want = fail, .str = "1,23"},
      {.want = fail, .str = "1.23"},
      {.want = fail, .str = "123 "},
      {.want = fail, .str = "123456789012345678901234"},
      {.want = fail, .str = "12a3"},
      {.want = fail, .str = "18446744073709551616"},  // UINT64_MAX.
      {.want = fail, .str = "18446744073709551617"},
      {.want = fail, .str = "18446744073709551618"},
      {.want = fail, .str = "18446744073709551619"},
      {.want = fail, .str = "18446744073709551620"},
      {.want = fail, .str = "18446744073709551621"},
      {.want = fail, .str = "_"},
      {.want = fail, .str = "d"},
      {.want = fail, .str = "x"},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__result_u64 r = wuffs_base__parse_number_u64(
        wuffs_base__make_slice_u8((uint8_t*)(void*)test_cases[tc].str,
                                  strlen(test_cases[tc].str)),
        WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES);
    uint64_t have = (r.status.repr == NULL) ? r.value : fail;
    if (have != test_cases[tc].want) {
      RETURN_FAIL("\"%s\": have 0x%" PRIX64 ", want 0x%" PRIX64,
                  test_cases[tc].str, have, test_cases[tc].want);
    }
  }

  // Test WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES.
  {
    for (int o = 0; o < 2; o++) {
      const char* str = "007";
      wuffs_base__result_u64 r = wuffs_base__parse_number_u64(
          wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str)),
          (o ? WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES
             : WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS));

      if (o == 0) {
        if (r.status.repr != wuffs_base__error__bad_argument) {
          RETURN_FAIL(
              "ALLOW_MULTIPLE_LEADING_ZEROES off: have \"%s\", want \"%s\"",
              r.status.repr, wuffs_base__error__bad_argument);
        }
        continue;
      }

      CHECK_STATUS("ALLOW_MULTIPLE_LEADING_ZEROES on", r.status);
      uint64_t want = 7;
      if (r.value != want) {
        RETURN_FAIL("ALLOW_MULTIPLE_LEADING_ZEROES on: have %" PRIu64
                    ", want %" PRIu64,
                    r.value, want);
      }
    }
  }

  // Test WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES.
  {
    for (int o = 0; o < 2; o++) {
      const char* str = "56_7__8";
      wuffs_base__result_u64 r = wuffs_base__parse_number_u64(
          wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str)),
          (o ? WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES
             : WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS));

      if (o == 0) {
        if (r.status.repr != wuffs_base__error__bad_argument) {
          RETURN_FAIL(
              "ALLOW_MULTIPLE_LEADING_ZEROES off: have \"%s\", want \"%s\"",
              r.status.repr, wuffs_base__error__bad_argument);
        }
        continue;
      }

      CHECK_STATUS("ALLOW_MULTIPLE_LEADING_ZEROES on", r.status);
      uint64_t want = 5678;
      if (r.value != want) {
        RETURN_FAIL("ALLOW_MULTIPLE_LEADING_ZEROES on: have %" PRIu64
                    ", want %" PRIu64,
                    r.value, want);
      }
    }
  }

  return NULL;
}

// ----------------

const char*  //
test_wuffs_strconv_render_number_f64() {
  CHECK_FOCUS(__func__);

  struct {
    uint64_t x;
    // These want strings come from Go's strconv.FormatFloat.
    const char* want__e;  // FormatFloat(etc, 'e', -1, etc)
    const char* want__f;  // FormatFloat(etc, 'f', -1, etc)
    const char* want_0g;  // FormatFloat(etc, 'g', +0, etc)
    const char* want_2e;  // FormatFloat(etc, 'e', +2, etc)
    const char* want_3f;  // FormatFloat(etc, 'f', +3, etc)
    const char* want_4g;  // FormatFloat(etc, 'g', +4, etc)
  } test_cases[] = {
      // These test cases were generated by
      // script/print-render-number-f64-tests.go

      {
          .x = 0x0000000000000000,
          .want__e = "0e+00",
          .want__f = "0",
          .want_0g = "0",
          .want_2e = "0.00e+00",
          .want_3f = "0.000",
          .want_4g = "0",
      },
      {
          .x = 0x0000000000000001,
          .want__e = "5e-324",
          .want__f = "0.000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000005",
          .want_0g = "5e-324",
          .want_2e = "4.94e-324",
          .want_3f = "0.000",
          .want_4g = "4.941e-324",
      },
      {
          .x = 0x0000000000000002,
          .want__e = "1e-323",
          .want__f = "0.000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "0000000000000000000000001",
          .want_0g = "1e-323",
          .want_2e = "9.88e-324",
          .want_3f = "0.000",
          .want_4g = "9.881e-324",
      },
      {
          .x = 0x0000000000000003,
          .want__e = "1.5e-323",
          .want__f = "0.000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000015",
          .want_0g = "1e-323",
          .want_2e = "1.48e-323",
          .want_3f = "0.000",
          .want_4g = "1.482e-323",
      },
      {
          .x = 0x000730D67819E8D2,
          .want__e = "1e-308",
          .want__f = "0.000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "0000000001",
          .want_0g = "1e-308",
          .want_2e = "1.00e-308",
          .want_3f = "0.000",
          .want_4g = "1e-308",
      },
      {
          .x = 0x000FFFFFFFFFFFFF,
          .want__e = "2.225073858507201e-308",
          .want__f = "0.000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "0000000002225073858507201",
          .want_0g = "2e-308",
          .want_2e = "2.23e-308",
          .want_3f = "0.000",
          .want_4g = "2.225e-308",
      },
      {
          .x = 0x0010000000000000,
          .want__e = "2.2250738585072014e-308",
          .want__f = "0.000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000022250738585072014",
          .want_0g = "2e-308",
          .want_2e = "2.23e-308",
          .want_3f = "0.000",
          .want_4g = "2.225e-308",
      },
      {
          .x = 0x0031FA182C40C60D,
          .want__e = "1e-307",
          .want__f = "0.000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "000000001",
          .want_0g = "1e-307",
          .want_2e = "1.00e-307",
          .want_3f = "0.000",
          .want_4g = "1e-307",
      },
      {
          .x = 0x369C2DF8DA5B6CA8,
          .want__e = "1.234e-45",
          .want__f = "0.000000000000000000000000000000000000000000001234",
          .want_0g = "1e-45",
          .want_2e = "1.23e-45",
          .want_3f = "0.000",
          .want_4g = "1.234e-45",
      },
      {
          .x = 0x369C314ABE948EB1,
          .want__e = "1.23456789e-45",
          .want__f = "0.000000000000000000000000000000000000000000001234"
                     "56789",
          .want_0g = "1e-45",
          .want_2e = "1.23e-45",
          .want_3f = "0.000",
          .want_4g = "1.235e-45",
      },
      {
          .x = 0x3E70000000000000,
          .want__e = "5.960464477539063e-08",
          .want__f = "0.00000005960464477539063",
          .want_0g = "6e-08",
          .want_2e = "5.96e-08",
          .want_3f = "0.000",
          .want_4g = "5.96e-08",
      },
      {
          .x = 0x3F88000000000000,
          .want__e = "1.171875e-02",
          .want__f = "0.01171875",
          .want_0g = "0.01",
          .want_2e = "1.17e-02",
          .want_3f = "0.012",
          .want_4g = "0.01172",
      },
      {
          .x = 0x3FD0000000000000,
          .want__e = "2.5e-01",
          .want__f = "0.25",
          .want_0g = "0.2",
          .want_2e = "2.50e-01",
          .want_3f = "0.250",
          .want_4g = "0.25",
      },
      {
          .x = 0x3FD3333333333333,
          .want__e = "3e-01",
          .want__f = "0.3",
          .want_0g = "0.3",
          .want_2e = "3.00e-01",
          .want_3f = "0.300",
          .want_4g = "0.3",
      },
      {
          .x = 0x3FD3333333333334,
          .want__e = "3.0000000000000004e-01",
          .want__f = "0.30000000000000004",
          .want_0g = "0.3",
          .want_2e = "3.00e-01",
          .want_3f = "0.300",
          .want_4g = "0.3",
      },
      {
          .x = 0x3FD5555555555555,
          .want__e = "3.333333333333333e-01",
          .want__f = "0.3333333333333333",
          .want_0g = "0.3",
          .want_2e = "3.33e-01",
          .want_3f = "0.333",
          .want_4g = "0.3333",
      },
      {
          .x = 0x3FEFFFFFFFFFFFFF,
          .want__e = "9.999999999999999e-01",
          .want__f = "0.9999999999999999",
          .want_0g = "1",
          .want_2e = "1.00e+00",
          .want_3f = "1.000",
          .want_4g = "1",
      },
      {
          .x = 0x3FF0000000000000,
          .want__e = "1e+00",
          .want__f = "1",
          .want_0g = "1",
          .want_2e = "1.00e+00",
          .want_3f = "1.000",
          .want_4g = "1",
      },
      {
          .x = 0x3FF0000000000001,
          .want__e = "1.0000000000000002e+00",
          .want__f = "1.0000000000000002",
          .want_0g = "1",
          .want_2e = "1.00e+00",
          .want_3f = "1.000",
          .want_4g = "1",
      },
      {
          .x = 0x3FF0000000000002,
          .want__e = "1.0000000000000004e+00",
          .want__f = "1.0000000000000004",
          .want_0g = "1",
          .want_2e = "1.00e+00",
          .want_3f = "1.000",
          .want_4g = "1",
      },
      {
          .x = 0x3FF4000000000000,
          .want__e = "1.25e+00",
          .want__f = "1.25",
          .want_0g = "1",
          .want_2e = "1.25e+00",
          .want_3f = "1.250",
          .want_4g = "1.25",
      },
      {
          .x = 0x3FF8000000000000,
          .want__e = "1.5e+00",
          .want__f = "1.5",
          .want_0g = "2",
          .want_2e = "1.50e+00",
          .want_3f = "1.500",
          .want_4g = "1.5",
      },
      {
          .x = 0x400599999999999A,
          .want__e = "2.7e+00",
          .want__f = "2.7",
          .want_0g = "3",
          .want_2e = "2.70e+00",
          .want_3f = "2.700",
          .want_4g = "2.7",
      },
      {
          .x = 0x4005BE76C8B43958,
          .want__e = "2.718e+00",
          .want__f = "2.718",
          .want_0g = "3",
          .want_2e = "2.72e+00",
          .want_3f = "2.718",
          .want_4g = "2.718",
      },
      {
          .x = 0x4005BF0995AAF790,
          .want__e = "2.71828e+00",
          .want__f = "2.71828",
          .want_0g = "3",
          .want_2e = "2.72e+00",
          .want_3f = "2.718",
          .want_4g = "2.718",
      },
      {
          .x = 0x4005BF0A87427F01,
          .want__e = "2.7182818e+00",
          .want__f = "2.7182818",
          .want_0g = "3",
          .want_2e = "2.72e+00",
          .want_3f = "2.718",
          .want_4g = "2.718",
      },
      {
          .x = 0x4005BF0A8B4949CB,
          .want__e = "2.71828183e+00",
          .want__f = "2.71828183",
          .want_0g = "3",
          .want_2e = "2.72e+00",
          .want_3f = "2.718",
          .want_4g = "2.718",
      },
      {
          .x = 0x4005BF0AA21A719B,
          .want__e = "2.718282e+00",
          .want__f = "2.718282",
          .want_0g = "3",
          .want_2e = "2.72e+00",
          .want_3f = "2.718",
          .want_4g = "2.718",
      },
      {
          .x = 0x4005BF141205BC02,
          .want__e = "2.7183e+00",
          .want__f = "2.7183",
          .want_0g = "3",
          .want_2e = "2.72e+00",
          .want_3f = "2.718",
          .want_4g = "2.718",
      },
      {
          .x = 0x4005C28F5C28F5C3,
          .want__e = "2.72e+00",
          .want__f = "2.72",
          .want_0g = "3",
          .want_2e = "2.72e+00",
          .want_3f = "2.720",
          .want_4g = "2.72",
      },
      {
          .x = 0x4008000000000000,
          .want__e = "3e+00",
          .want__f = "3",
          .want_0g = "3",
          .want_2e = "3.00e+00",
          .want_3f = "3.000",
          .want_4g = "3",
      },
      {
          .x = 0x400921F9F01B866E,
          .want__e = "3.14159e+00",
          .want__f = "3.14159",
          .want_0g = "3",
          .want_2e = "3.14e+00",
          .want_3f = "3.142",
          .want_4g = "3.142",
      },
      {
          .x = 0x400921FB54442D11,
          .want__e = "3.14159265358979e+00",
          .want__f = "3.14159265358979",
          .want_0g = "3",
          .want_2e = "3.14e+00",
          .want_3f = "3.142",
          .want_4g = "3.142",
      },
      {
          .x = 0x400921FB54442D18,
          .want__e = "3.141592653589793e+00",
          .want__f = "3.141592653589793",
          .want_0g = "3",
          .want_2e = "3.14e+00",
          .want_3f = "3.142",
          .want_4g = "3.142",
      },
      {
          .x = 0x400C000000000000,
          .want__e = "3.5e+00",
          .want__f = "3.5",
          .want_0g = "4",
          .want_2e = "3.50e+00",
          .want_3f = "3.500",
          .want_4g = "3.5",
      },
      {
          .x = 0x4014000000000000,
          .want__e = "5e+00",
          .want__f = "5",
          .want_0g = "5",
          .want_2e = "5.00e+00",
          .want_3f = "5.000",
          .want_4g = "5",
      },
      {
          .x = 0x4036000000000000,
          .want__e = "2.2e+01",
          .want__f = "22",
          .want_0g = "2e+01",
          .want_2e = "2.20e+01",
          .want_3f = "22.000",
          .want_4g = "22",
      },
      {
          .x = 0x4037000000000000,
          .want__e = "2.3e+01",
          .want__f = "23",
          .want_0g = "2e+01",
          .want_2e = "2.30e+01",
          .want_3f = "23.000",
          .want_4g = "23",
      },
      {
          .x = 0x4038000000000000,
          .want__e = "2.4e+01",
          .want__f = "24",
          .want_0g = "2e+01",
          .want_2e = "2.40e+01",
          .want_3f = "24.000",
          .want_4g = "24",
      },
      {
          .x = 0x405E9AA48FBB2888,
          .want__e = "1.2241629403378658e+02",
          .want__f = "122.41629403378658",
          .want_0g = "1e+02",
          .want_2e = "1.22e+02",
          .want_3f = "122.416",
          .want_4g = "122.4",
      },
      {
          .x = 0x40FE240C9FCB0C02,
          .want__e = "1.23456789012e+05",
          .want__f = "123456.789012",
          .want_0g = "1e+05",
          .want_2e = "1.23e+05",
          .want_3f = "123456.789",
          .want_4g = "1.235e+05",
      },
      {
          .x = 0x41E0246690000001,
          .want__e = "2.1665680640000005e+09",
          .want__f = "2166568064.0000005",
          .want_0g = "2e+09",
          .want_2e = "2.17e+09",
          .want_3f = "2166568064.000",
          .want_4g = "2.167e+09",
      },
      {
          .x = 0x4202A05F20000000,
          .want__e = "1e+10",
          .want__f = "10000000000",
          .want_0g = "1e+10",
          .want_2e = "1.00e+10",
          .want_3f = "10000000000.000",
          .want_4g = "1e+10",
      },
      {
          .x = 0x4330000000000000,
          .want__e = "4.503599627370496e+15",
          .want__f = "4503599627370496",
          .want_0g = "5e+15",
          .want_2e = "4.50e+15",
          .want_3f = "4503599627370496.000",
          .want_4g = "4.504e+15",
      },
      {
          .x = 0x4330000000000001,
          .want__e = "4.503599627370497e+15",
          .want__f = "4503599627370497",
          .want_0g = "5e+15",
          .want_2e = "4.50e+15",
          .want_3f = "4503599627370497.000",
          .want_4g = "4.504e+15",
      },
      {
          .x = 0x4330000000000002,
          .want__e = "4.503599627370498e+15",
          .want__f = "4503599627370498",
          .want_0g = "5e+15",
          .want_2e = "4.50e+15",
          .want_3f = "4503599627370498.000",
          .want_4g = "4.504e+15",
      },
      {
          .x = 0x433FFFFFFFFFFFFE,
          .want__e = "9.00719925474099e+15",
          .want__f = "9007199254740990",
          .want_0g = "9e+15",
          .want_2e = "9.01e+15",
          .want_3f = "9007199254740990.000",
          .want_4g = "9.007e+15",
      },
      {
          .x = 0x433FFFFFFFFFFFFF,
          .want__e = "9.007199254740991e+15",
          .want__f = "9007199254740991",
          .want_0g = "9e+15",
          .want_2e = "9.01e+15",
          .want_3f = "9007199254740991.000",
          .want_4g = "9.007e+15",
      },
      {
          .x = 0x4340000000000000,
          .want__e = "9.007199254740992e+15",
          .want__f = "9007199254740992",
          .want_0g = "9e+15",
          .want_2e = "9.01e+15",
          .want_3f = "9007199254740992.000",
          .want_4g = "9.007e+15",
      },
      {
          .x = 0x4340000000000001,
          .want__e = "9.007199254740994e+15",
          .want__f = "9007199254740994",
          .want_0g = "9e+15",
          .want_2e = "9.01e+15",
          .want_3f = "9007199254740994.000",
          .want_4g = "9.007e+15",
      },
      {
          .x = 0x4340000000000002,
          .want__e = "9.007199254740996e+15",
          .want__f = "9007199254740996",
          .want_0g = "9e+15",
          .want_2e = "9.01e+15",
          .want_3f = "9007199254740996.000",
          .want_4g = "9.007e+15",
      },
      {
          .x = 0x4370000000000000,
          .want__e = "7.205759403792794e+16",
          .want__f = "72057594037927940",
          .want_0g = "7e+16",
          .want_2e = "7.21e+16",
          .want_3f = "72057594037927936.000",
          .want_4g = "7.206e+16",
      },
      {
          .x = 0x43E158E460913D00,
          .want__e = "1e+19",
          .want__f = "10000000000000000000",
          .want_0g = "1e+19",
          .want_2e = "1.00e+19",
          .want_3f = "10000000000000000000.000",
          .want_4g = "1e+19",
      },
      {
          .x = 0x43F002F1776DDA67,
          .want__e = "1.8459999196907205e+19",
          .want__f = "18459999196907205000",
          .want_0g = "2e+19",
          .want_2e = "1.85e+19",
          .want_3f = "18459999196907204608.000",
          .want_4g = "1.846e+19",
      },
      {
          .x = 0x4415AF1D78B58C40,
          .want__e = "1e+20",
          .want__f = "100000000000000000000",
          .want_0g = "1e+20",
          .want_2e = "1.00e+20",
          .want_3f = "100000000000000000000.000",
          .want_4g = "1e+20",
      },
      {
          .x = 0x44B52D02C7E14AF6,
          .want__e = "1e+23",
          .want__f = "100000000000000000000000",
          .want_0g = "1e+23",
          .want_2e = "1.00e+23",
          .want_3f = "99999999999999991611392.000",
          .want_4g = "1e+23",
      },
      {
          .x = 0x44DFC3842BD1F072,
          .want__e = "6e+23",
          .want__f = "600000000000000000000000",
          .want_0g = "6e+23",
          .want_2e = "6.00e+23",
          .want_3f = "600000000000000016777216.000",
          .want_4g = "6e+23",
      },
      {
          .x = 0x44DFDE9F10A8D361,
          .want__e = "6.02e+23",
          .want__f = "602000000000000000000000",
          .want_0g = "6e+23",
          .want_2e = "6.02e+23",
          .want_3f = "601999999999999995805696.000",
          .want_4g = "6.02e+23",
      },
      {
          .x = 0x44DFE154F457EA13,
          .want__e = "6.022e+23",
          .want__f = "602200000000000000000000",
          .want_0g = "6e+23",
          .want_2e = "6.02e+23",
          .want_3f = "602200000000000027262976.000",
          .want_4g = "6.022e+23",
      },
      {
          .x = 0x44DFE177A620AB35,
          .want__e = "6.0221e+23",
          .want__f = "602210000000000000000000",
          .want_0g = "6e+23",
          .want_2e = "6.02e+23",
          .want_3f = "602209999999999995281408.000",
          .want_4g = "6.022e+23",
      },
      {
          .x = 0x44DFE18586D75EDC,
          .want__e = "6.02214e+23",
          .want__f = "602214000000000000000000",
          .want_0g = "6e+23",
          .want_2e = "6.02e+23",
          .want_3f = "602213999999999969067008.000",
          .want_4g = "6.022e+23",
      },
      {
          .x = 0x44DFE185CA57C517,
          .want__e = "6.02214076e+23",
          .want__f = "602214076000000000000000",
          .want_0g = "6e+23",
          .want_2e = "6.02e+23",
          .want_3f = "602214075999999987023872.000",
          .want_4g = "6.022e+23",
      },
      {
          .x = 0x44DFE185CDE543BC,
          .want__e = "6.0221408e+23",
          .want__f = "602214080000000000000000",
          .want_0g = "6e+23",
          .want_2e = "6.02e+23",
          .want_3f = "602214080000000002097152.000",
          .want_4g = "6.022e+23",
      },
      {
          .x = 0x44DFE185DFA8BCF4,
          .want__e = "6.022141e+23",
          .want__f = "602214100000000000000000",
          .want_0g = "6e+23",
          .want_2e = "6.02e+23",
          .want_3f = "602214100000000010354688.000",
          .want_4g = "6.022e+23",
      },
      {
          .x = 0x46293E5939A08CEA,
          .want__e = "1e+30",
          .want__f = "1000000000000000000000000000000",
          .want_0g = "1e+30",
          .want_2e = "1.00e+30",
          .want_3f = "1000000000000000019884624838656.000",
          .want_4g = "1e+30",
      },
      {
          .x = 0x54B249AD2594C37D,
          .want__e = "1e+100",
          .want__f = "10000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "0",
          .want_0g = "1e+100",
          .want_2e = "1.00e+100",
          .want_3f = "10000000000000000159028911097599180468360808563945"
                     "28138978132755774783877217038106081346998585681510"
                     "4.000",
          .want_4g = "1e+100",
      },
      {
          .x = 0x54B2987670ADB613,
          .want__e = "1.0168286519992373e+100",
          .want__f = "10168286519992373000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "0",
          .want_0g = "1e+100",
          .want_2e = "1.02e+100",
          .want_3f = "10168286519992372611942638135241625267907715220891"
                     "31363562982164291689352904657807934098133246961254"
                     "4.000",
          .want_4g = "1.017e+100",
      },
      {
          .x = 0x7BBA44DF832B8D46,
          .want__e = "1e+288",
          .want__f = "10000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "000000000000000000000000000000000000000",
          .want_0g = "1e+288",
          .want_2e = "1.00e+288",
          .want_3f = "10000000000000000076304735395750356605147783355117"
                     "10750780086664439969510636494954611131549135839186"
                     "51398345555539522089568786054480958499982972526059"
                     "48732710873996264866061464425509888400169173946264"
                     "49536395208620267012778077787723395914064607119962"
                     "069483324573977857832138825282954985472.000",
          .want_4g = "1e+288",
      },
      {
          .x = 0x7BF06B0BB1FB384C,
          .want__e = "1e+289",
          .want__f = "10000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "0000000000000000000000000000000000000000",
          .want_0g = "1e+289",
          .want_2e = "1.00e+289",
          .want_3f = "10000000000000000617278335278671568869943723109630"
                     "11258310052850538813376539671558942539170944464796"
                     "69431045845149126131034590785433956171738211535366"
                     "98722855425910210916188218613474303381375362727338"
                     "59602462772449948462578903480308154011242367042019"
                     "1213257583185130503608895092113260150784.000",
          .want_4g = "1e+289",
      },
      {
          .x = 0x7C2485CE9E7A065F,
          .want__e = "1e+290",
          .want__f = "10000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000",
          .want_0g = "1e+290",
          .want_2e = "1.00e+290",
          .want_3f = "10000000000000000617278335278671568869943723109630"
                     "11258310052850538813376539671558942539170944464796"
                     "69431045845149126131034590785433956171738211535366"
                     "98722855425910210916188218613474303381375362727338"
                     "59602462772449948462578903480308154011242367042019"
                     "12132575831851305036088950921132601507840.000",
          .want_4g = "1e+290",
      },
      {
          .x = 0x7FAC7B1F3CAC7433,
          .want__e = "1e+307",
          .want__f = "10000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000",
          .want_0g = "1e+307",
          .want_2e = "1.00e+307",
          .want_3f = "99999999999999998603105976025645777170026418381263"
                     "63875249660735883565852672743849064846414228960666"
                     "78637928039265461539335317285025210333627595237061"
                     "53970107306916646893751785690398510731463396416232"
                     "66071126720011020169553304018596457812688561947201"
                     "17148846117292182213906692985128212200267666775002"
                     "1070848.000",
          .want_4g = "1e+307",
      },
      {
          .x = 0x7FE1CCF385EBC8A0,
          .want__e = "1e+308",
          .want__f = "10000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "000000000",
          .want_0g = "1e+308",
          .want_2e = "1.00e+308",
          .want_3f = "10000000000000000109790636294404554174049230967731"
                     "18463368106829031575854049114915371633289784946888"
                     "99061249669721172515611590283743140088328307009198"
                     "14604603127166450293302718569748969958855904333838"
                     "44661650011784268976262129451776280911957867074581"
                     "22783970171784415105291802893207873272974885715430"
                     "223118336.000",
          .want_4g = "1e+308",
      },
      {
          .x = 0x7FEFFFFFFFFFFFFF,
          .want__e = "1.7976931348623157e+308",
          .want__f = "17976931348623157000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "00000000000000000000000000000000000000000000000000"
                     "000000000",
          .want_0g = "2e+308",
          .want_2e = "1.80e+308",
          .want_3f = "17976931348623157081452742373170435679807056752584"
                     "49965989174768031572607800285387605895586327668781"
                     "71540458953514382464234321326889464182768467546703"
                     "53751698604991057655128207624549009038932894407586"
                     "85084551339423045832369032229481658085593321233482"
                     "74797826204144723168738177180919299881250404026184"
                     "124858368.000",
          .want_4g = "1.798e+308",
      },
      {
          .x = 0x7FF0000000000000,
          .want__e = "Inf",
          .want__f = "Inf",
          .want_0g = "Inf",
          .want_2e = "Inf",
          .want_3f = "Inf",
          .want_4g = "Inf",
      },
      {
          .x = 0x7FFFFFFFFFFFFFFF,
          .want__e = "NaN",
          .want__f = "NaN",
          .want_0g = "NaN",
          .want_2e = "NaN",
          .want_3f = "NaN",
          .want_4g = "NaN",
      },
      {
          .x = 0x8000000000000000,
          .want__e = "-0e+00",
          .want__f = "-0",
          .want_0g = "-0",
          .want_2e = "-0.00e+00",
          .want_3f = "-0.000",
          .want_4g = "-0",
      },
      {
          .x = 0xC008000000000000,
          .want__e = "-3e+00",
          .want__f = "-3",
          .want_0g = "-3",
          .want_2e = "-3.00e+00",
          .want_3f = "-3.000",
          .want_4g = "-3",
      },
      {
          .x = 0xFFF0000000000000,
          .want__e = "-Inf",
          .want__f = "-Inf",
          .want_0g = "-Inf",
          .want_2e = "-Inf",
          .want_3f = "-Inf",
          .want_4g = "-Inf",
      },
      {
          .x = 0xFFFFFFFFFFFFFFFF,
          .want__e = "NaN",
          .want__f = "NaN",
          .want_0g = "NaN",
          .want_2e = "NaN",
          .want_3f = "NaN",
          .want_4g = "NaN",
      },
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    double f64 = wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
        test_cases[tc].x);
    for (int o = 0; o < 6; o++) {
      uint32_t precision = 0;
      uint32_t options = 0;
      const char* want = NULL;
      if (o == 0) {
        options = WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_PRESENT |
                  WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION;
        want = test_cases[tc].want__e;
      } else if (o == 1) {
        options = WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT |
                  WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION;
        want = test_cases[tc].want__f;
      } else if (o == 2) {
        precision = 0;
        options = 0;
        want = test_cases[tc].want_0g;
      } else if (o == 3) {
        precision = 2;
        options = WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_PRESENT;
        want = test_cases[tc].want_2e;
      } else if (o == 4) {
        precision = 3;
        options = WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT;
        want = test_cases[tc].want_3f;
      } else if (o == 5) {
        precision = 4;
        options = 0;
        want = test_cases[tc].want_4g;
      }

      size_t n = wuffs_base__render_number_f64(g_have_slice_u8, f64, precision,
                                               options);
      if (n == 0) {
        RETURN_FAIL("x=0x%016" PRIX64 ", o=%d: could not render",
                    test_cases[tc].x, o);
      } else if (n >= g_have_slice_u8.len) {
        RETURN_FAIL("x=0x%016" PRIX64 ", o=%d: n is too large",
                    test_cases[tc].x, o);
      }
      g_have_slice_u8.ptr[n] = 0x00;
      if (strcmp((const char*)(g_have_slice_u8.ptr), want) != 0) {
        RETURN_FAIL("x=0x%016" PRIX64 ", o=%d: have \"%s\", want \"%s\"",
                    test_cases[tc].x, o, g_have_slice_u8.ptr, want);
      }
    }
  }

  // Test WUFFS_BASE__RENDER_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA.
  {
    for (int o = 0; o < 2; o++) {
      uint8_t dst[8] = {0};
      const double f64 = 1.75;
      const uint32_t precision = 2;
      size_t n = wuffs_base__render_number_f64(
          wuffs_base__make_slice_u8(&dst[0], WUFFS_TESTLIB_ARRAY_SIZE(dst)),
          f64, precision,
          WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT |
              (o ? WUFFS_BASE__RENDER_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA
                 : 0));
      if (n != 4) {
        RETURN_FAIL("DECIMAL_SEPARATOR_IS_A_COMMA, o=%d: n != 4", o);
      }
      uint8_t have = dst[1];
      uint8_t want = o ? ',' : '.';
      if (have != want) {
        RETURN_FAIL(
            "DECIMAL_SEPARATOR_IS_A_COMMA, o=%d: have 0x%02X, want 0x%02X", o,
            (int)have, (int)want);
      }
    }
  }

  return NULL;
}

const char*  //
test_wuffs_strconv_render_number_i64() {
  CHECK_FOCUS(__func__);

  struct {
    int64_t x;
    const char* want;
  } test_cases[] = {
      {.x = +0x0000000000000000ll, .want = "0"},
      {.x = +0x0000000000000009ll, .want = "9"},
      {.x = +0x000000000000000All, .want = "10"},
      {.x = +0x000000000000004All, .want = "74"},
      {.x = +0x0000000000000063ll, .want = "99"},
      {.x = +0x0000000000000064ll, .want = "100"},
      {.x = +0x000000000000007Cll, .want = "124"},
      {.x = +0x00000000000001F4ll, .want = "500"},
      {.x = +0x000000000000036Cll, .want = "876"},
      {.x = +0x000000000000036Fll, .want = "879"},
      {.x = +0x0000000000000929ll, .want = "2345"},
      {.x = +0x0000000000010932ll, .want = "67890"},
      {.x = +0x00000000FFFFFFFFll, .want = "4294967295"},
      {.x = +0x0000000100000000ll, .want = "4294967296"},
      {.x = +0x0123456789ABCDEFll, .want = "81985529216486895"},
      {.x = +0x7FFFFFFFFFFFFFFFll, .want = "9223372036854775807"},

      {.x = -0x0000000000000009ll, .want = "-9"},
      {.x = -0x000000000000000All, .want = "-10"},
      {.x = -0x000000000000004All, .want = "-74"},
      {.x = -0x0000000000000063ll, .want = "-99"},
      {.x = -0x0000000000000064ll, .want = "-100"},
      {.x = -0x000000000000007Cll, .want = "-124"},
      {.x = -0x00000000000001F4ll, .want = "-500"},
      {.x = -0x000000000000036Cll, .want = "-876"},
      {.x = -0x000000000000036Fll, .want = "-879"},
      {.x = -0x0000000000000929ll, .want = "-2345"},
      {.x = -0x0000000000010932ll, .want = "-67890"},
      {.x = -0x00000000FFFFFFFFll, .want = "-4294967295"},
      {.x = -0x0000000100000000ll, .want = "-4294967296"},
      {.x = -0x0123456789ABCDEFll, .want = "-81985529216486895"},
      {.x = -0x7FFFFFFFFFFFFFFFll, .want = "-9223372036854775807"},
      {.x = (int64_t)(-0x8000000000000000ll), .want = "-9223372036854775808"},
  };

  if (g_have_slice_u8.len < WUFFS_BASE__I64__BYTE_LENGTH__MAX_INCL) {
    return "g_have_slice_u8.len is too short";
  }

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    size_t n = wuffs_base__render_number_i64(
        g_have_slice_u8, test_cases[tc].x,
        WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
    if (n == 0) {
      RETURN_FAIL("%" PRId64 ": could not render", test_cases[tc].x);
    } else if (n >= g_have_slice_u8.len) {
      RETURN_FAIL("%" PRId64 ": n is too large", test_cases[tc].x);
    }
    g_have_slice_u8.ptr[n] = 0x00;
    if (strcmp((const char*)(g_have_slice_u8.ptr), test_cases[tc].want) != 0) {
      RETURN_FAIL("%" PRId64 ": have \"%s\", want \"%s\"", test_cases[tc].x,
                  g_have_slice_u8.ptr, test_cases[tc].want);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_strconv_render_number_u64() {
  CHECK_FOCUS(__func__);

  struct {
    uint64_t x;
    const char* want;
  } test_cases[] = {
      {.x = 0x0000000000000000, .want = "0"},
      {.x = 0x0000000000000009, .want = "9"},
      {.x = 0x000000000000000A, .want = "10"},
      {.x = 0x000000000000004A, .want = "74"},
      {.x = 0x0000000000000063, .want = "99"},
      {.x = 0x0000000000000064, .want = "100"},
      {.x = 0x000000000000007C, .want = "124"},
      {.x = 0x00000000000001F4, .want = "500"},
      {.x = 0x000000000000036C, .want = "876"},
      {.x = 0x000000000000036F, .want = "879"},
      {.x = 0x0000000000000929, .want = "2345"},
      {.x = 0x0000000000010932, .want = "67890"},
      {.x = 0x00000000FFFFFFFF, .want = "4294967295"},
      {.x = 0x0000000100000000, .want = "4294967296"},
      {.x = 0x0123456789ABCDEF, .want = "81985529216486895"},
      {.x = 0x7FFFFFFFFFFFFFFF, .want = "9223372036854775807"},
      {.x = 0x8000000000000000, .want = "9223372036854775808"},
      {.x = 0xFFFFFFFFFFFFFFF9, .want = "18446744073709551609"},
      {.x = 0xFFFFFFFFFFFFFFFA, .want = "18446744073709551610"},
      {.x = 0xFFFFFFFFFFFFFFFE, .want = "18446744073709551614"},
      {.x = 0xFFFFFFFFFFFFFFFF, .want = "18446744073709551615"},
  };

  if (g_have_slice_u8.len < WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL) {
    return "g_have_slice_u8.len is too short";
  }

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    size_t n = wuffs_base__render_number_u64(
        g_have_slice_u8, test_cases[tc].x,
        WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
    if (n == 0) {
      RETURN_FAIL("%" PRIu64 ": could not render", test_cases[tc].x);
    } else if (n >= g_have_slice_u8.len) {
      RETURN_FAIL("%" PRIu64 ": n is too large", test_cases[tc].x);
    }
    g_have_slice_u8.ptr[n] = 0x00;
    if (strcmp((const char*)(g_have_slice_u8.ptr), test_cases[tc].want) != 0) {
      RETURN_FAIL("%" PRIu64 ": have \"%s\", want \"%s\"", test_cases[tc].x,
                  g_have_slice_u8.ptr, test_cases[tc].want);
    }
  }

  // Test dst slice is too short.
  {
    uint8_t dst[5];
    size_t n = wuffs_base__render_number_u64(
        wuffs_base__make_slice_u8(&dst[0], sizeof(dst)), 123456,
        WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
    if (n != 0) {
      RETURN_FAIL("dst too short: have %zu, want 0", n);
    }
  }

  // Test options.
  {
    uint8_t dst[8];
    memcpy(&dst[0], "ABCDEFG\x00", 8);
    size_t n = wuffs_base__render_number_u64(
        wuffs_base__make_slice_u8(&dst[0], sizeof(dst) - 1), 1234,
        WUFFS_BASE__RENDER_NUMBER_XXX__ALIGN_RIGHT |
            WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN);
    if (n != 5) {
      RETURN_FAIL("ALIGN_RIGHT | LEADING_PLUS_SIGN: have %zu, want 5", n);
    }
    uint64_t have = wuffs_base__peek_u64be__no_bounds_check(&dst[0]);
    uint64_t want = 0x41422B3132333400;  // "AB+1234\x00".
    if (have != want) {
      RETURN_FAIL("ALIGN_RIGHT | LEADING_PLUS_SIGN: have 0x%" PRIX64
                  ", want 0x%" PRIX64,
                  have, want);
    }
  }

  return NULL;
}

// ----------------

const char*  //
test_wuffs_strconv_utf_8_next() {
  CHECK_FOCUS(__func__);

  // Special case the "\x00" string, which is valid UTF-8 but its strlen is
  // zero, not one.
  uint8_t the_nul_byte[1];
  the_nul_byte[0] = '\x00';

  struct {
    // The uint32_t want is packed as:
    //  - the high 8 bits are the byte length.
    //  - the low 24 bits are the code point.
    uint32_t want0;  // For wuffs_base__utf_8__next.
    uint32_t want1;  // For wuffs_base__utf_8__next_from_end.
    const char* str;
  } test_cases[] = {
      {.want0 = 0x00000000, .want1 = 0x00000000, .str = ""},
      {.want0 = 0x01000000, .want1 = 0x01000000, .str = "The <NUL> byte"},
      {.want0 = 0x01000009, .want1 = 0x01000009, .str = "\t"},
      {.want0 = 0x01000041, .want1 = 0x01000041, .str = "A"},
      {.want0 = 0x01000061, .want1 = 0x0100006A, .str = "abdefghij"},
      {.want0 = 0x0100007F, .want1 = 0x0100007F, .str = "\x7F"},
      {.want0 = 0x02000080, .want1 = 0x02000080, .str = "\xC2\x80"},
      {.want0 = 0x020007FF, .want1 = 0x020007FF, .str = "\xDF\xBF"},
      {.want0 = 0x03000800, .want1 = 0x03000800, .str = "\xE0\xA0\x80"},
      {.want0 = 0x0300FFFD, .want1 = 0x0300FFFD, .str = "\xEF\xBF\xBD"},
      {.want0 = 0x0300FFFF, .want1 = 0x0300FFFF, .str = "\xEF\xBF\xBF"},
      {.want0 = 0x04010000, .want1 = 0x04010000, .str = "\xF0\x90\x80\x80"},
      {.want0 = 0x0410FFFF, .want1 = 0x0410FFFF, .str = "\xF4\x8F\xBF\xBF"},

      // U+00000394 GREEK CAPITAL LETTER DELTA.
      {.want0 = 0x02000394, .want1 = 0x02000394, .str = "\xCE\x94"},
      {.want0 = 0x02000394, .want1 = 0x01000070, .str = "\xCE\x94p"},
      {.want0 = 0x02000394, .want1 = 0x01000071, .str = "\xCE\x94pq"},
      {.want0 = 0x02000394, .want1 = 0x01000072, .str = "\xCE\x94pqr"},
      {.want0 = 0x02000394, .want1 = 0x01000073, .str = "\xCE\x94pqrs"},
      {.want0 = 0x02000394, .want1 = 0x0100FFFD, .str = "\xCE\x94\x80"},
      {.want0 = 0x02000394, .want1 = 0x0100FFFD, .str = "\xCE\x94\x80\x81"},
      {.want0 = 0x02000394, .want1 = 0x0100FFFD, .str = "\xCE\x94\x80\x81\x82"},
      {.want0 = 0x02000394,
       .want1 = 0x0100FFFD,
       .str = "\xCE\x94\x80\x81\x82\x83"},
      {.want0 = 0x01000070, .want1 = 0x02000394, .str = "p\xCE\x94"},

      // U+00002603 SNOWMAN.
      {.want0 = 0x03002603, .want1 = 0x03002603, .str = "\xE2\x98\x83"},
      {.want0 = 0x03002603, .want1 = 0x01000070, .str = "\xE2\x98\x83p"},
      {.want0 = 0x03002603, .want1 = 0x01000071, .str = "\xE2\x98\x83pq"},
      {.want0 = 0x03002603, .want1 = 0x01000072, .str = "\xE2\x98\x83pqr"},
      {.want0 = 0x03002603, .want1 = 0x01000073, .str = "\xE2\x98\x83pqrs"},
      {.want0 = 0x03002603, .want1 = 0x0100FFFD, .str = "\xE2\x98\x83\xFF"},
      {.want0 = 0x01000070, .want1 = 0x03002603, .str = "p\xE2\x98\x83"},

      // U+0001F4A9 PILE OF POO.
      {.want0 = 0x0401F4A9, .want1 = 0x0401F4A9, .str = "\xF0\x9F\x92\xA9"},
      {.want0 = 0x0401F4A9, .want1 = 0x01000070, .str = "\xF0\x9F\x92\xA9p"},
      {.want0 = 0x0401F4A9, .want1 = 0x01000071, .str = "\xF0\x9F\x92\xA9pq"},
      {.want0 = 0x0401F4A9, .want1 = 0x01000072, .str = "\xF0\x9F\x92\xA9pqr"},
      {.want0 = 0x0401F4A9, .want1 = 0x01000073, .str = "\xF0\x9F\x92\xA9pqrs"},
      {.want0 = 0x0401F4A9, .want1 = 0x0100FFFD, .str = "\xF0\x9F\x92\xA9\xFF"},
      {.want0 = 0x01000070, .want1 = 0x0401F4A9, .str = "p\xF0\x9F\x92\xA9"},

      // Invalid.
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\x80"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xBF"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xC0\x80"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xC1\xBF"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xC2"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100007F, .str = "\xC2\x7F"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xC2\xC0"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xC2\xFF"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xCE"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xDF\xC0"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xDF\xFF"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xE0\x80"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xE0\x80\x81"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xE0\x9F\xBF"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xE2"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF0"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF0\x80\x81"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF0\x80\x81\x82"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF0\x8F\xBF\xBF"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF4\x90\x81\x82"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF5"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF6\x80"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF7\x80\x81"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xF8\x90\x91\x92\x93"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xFF\xFF\xFF\xFF"},

      // Invalid. UTF-8 cannot contain the surrogates U+D800 ..= U+DFFF.
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xED\xA0\x80"},
      {.want0 = 0x0100FFFD, .want1 = 0x0100FFFD, .str = "\xED\xBF\xBF"},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_base__slice_u8 s = wuffs_base__make_slice_u8(
        (uint8_t*)(void*)test_cases[tc].str, strlen(test_cases[tc].str));
    // Override "The <NUL> byte" with "\x00".
    if (test_cases[tc].want0 == 0x01000000) {
      s = wuffs_base__make_slice_u8(&the_nul_byte[0], 1);
    }

    // Test wuffs_base__utf_8__next.
    {
      uint32_t want_bl = test_cases[tc].want0 >> 24;
      uint32_t want_cp = test_cases[tc].want0 & 0xFFFFFF;
      wuffs_base__utf_8__next__output have =
          wuffs_base__utf_8__next(s.ptr, s.len);
      if ((have.code_point != want_cp) || (have.byte_length != want_bl)) {
        RETURN_FAIL("next(\"%s\"): have cp=0x%" PRIX32 " bl=%" PRIu32
                    ", want cp=0x%" PRIX32 " bl=%" PRIu32,
                    test_cases[tc].str, have.code_point, have.byte_length,
                    want_cp, want_bl);
      }
    }

    // Test wuffs_base__utf_8__next_from_end.
    {
      uint32_t want_bl = test_cases[tc].want1 >> 24;
      uint32_t want_cp = test_cases[tc].want1 & 0xFFFFFF;
      wuffs_base__utf_8__next__output have =
          wuffs_base__utf_8__next_from_end(s.ptr, s.len);
      if ((have.code_point != want_cp) || (have.byte_length != want_bl)) {
        RETURN_FAIL("next_from_end(\"%s\"): have cp=0x%" PRIX32 " bl=%" PRIu32
                    ", want cp=0x%" PRIX32 " bl=%" PRIu32,
                    test_cases[tc].str, have.code_point, have.byte_length,
                    want_cp, want_bl);
      }
    }
  }

  return NULL;
}

// ---------------- Golden Tests

golden_test g_json_australian_abc_gt = {
    .want_filename = "test/data/australian-abc-local-stations.tokens",
    .src_filename = "test/data/australian-abc-local-stations.json",
};

golden_test g_json_file_sizes_gt = {
    .src_filename = "test/data/file-sizes.json",
};

golden_test g_json_github_tags_gt = {
    .src_filename = "test/data/github-tags.json",
};

golden_test g_json_json_things_unformatted_gt = {
    .want_filename = "test/data/json-things.unformatted.tokens",
    .src_filename = "test/data/json-things.unformatted.json",
};

golden_test g_json_json_quirks_gt = {
    .want_filename = "test/data/json-quirks.tokens",
    .src_filename = "test/data/json-quirks.json",
};

golden_test g_json_nobel_prizes_gt = {
    .src_filename = "test/data/nobel-prizes.json",
};

// ---------------- JSON Tests

const char*  //
test_wuffs_json_decode_interface() {
  CHECK_FOCUS(__func__);

  {
    wuffs_json__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_json__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    CHECK_STRING(do_test__wuffs_base__token_decoder(
        wuffs_json__decoder__upcast_as__wuffs_base__token_decoder(&dec),
        &g_json_json_things_unformatted_gt));
  }

  {
    wuffs_json__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_json__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    CHECK_STRING(do_test__wuffs_base__token_decoder(
        wuffs_json__decoder__upcast_as__wuffs_base__token_decoder(&dec),
        &g_json_australian_abc_gt));
  }

  {
    uint32_t quirks[] = {
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_A,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_CAPITAL_U,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_E,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_QUESTION_MARK,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_SINGLE_QUOTE,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_V,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_X_AS_CODE_POINTS,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_ZERO,
        WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK,
        WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE,
        WUFFS_JSON__QUIRK_ALLOW_EXTRA_COMMA,
        WUFFS_JSON__QUIRK_ALLOW_INF_NAN_NUMBERS,
        WUFFS_JSON__QUIRK_ALLOW_LEADING_ASCII_RECORD_SEPARATOR,
        WUFFS_JSON__QUIRK_ALLOW_LEADING_UNICODE_BYTE_ORDER_MARK,
        WUFFS_JSON__QUIRK_ALLOW_TRAILING_FILLER,
        WUFFS_JSON__QUIRK_REPLACE_INVALID_UNICODE,
        0,
    };

    wuffs_json__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_json__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    for (size_t i = 0; quirks[i]; i++) {
      wuffs_json__decoder__set_quirk(&dec, quirks[i], 1);
    }
    CHECK_STRING(do_test__wuffs_base__token_decoder(
        wuffs_json__decoder__upcast_as__wuffs_base__token_decoder(&dec),
        &g_json_json_quirks_gt));
  }

  return NULL;
}

const char*  //
test_wuffs_json_decode_empty_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__token_buffer tok =
      wuffs_base__slice_token__writer(g_have_slice_token);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_json__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_json__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status =
      wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8);
  if (status.repr != wuffs_json__error__bad_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_json__error__bad_input);
  }
  return NULL;
}

const char*  //
wuffs_json_decode(wuffs_base__token_buffer* tok,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  wuffs_json__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_json__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                               wuffs_initialize_flags));

  while (true) {
    wuffs_base__token_buffer limited_tok =
        make_limited_token_writer(*tok, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_json__decoder__decode_tokens(
        &dec, &limited_tok, &limited_src, g_work_slice_u8);

    tok->meta.wi += limited_tok.meta.wi;
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
test_wuffs_json_decode_end_of_data() {
  CHECK_FOCUS(__func__);

  for (int i = 0; i < 2; i++) {
    uint8_t* src_ptr = (uint8_t*)("123null89");
    size_t src_len = i ? 3 : 9;

    wuffs_json__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_json__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    wuffs_base__token_buffer tok =
        wuffs_base__slice_token__writer(g_have_slice_token);
    wuffs_base__io_buffer src =
        wuffs_base__ptr_u8__reader(src_ptr, src_len, true);
    CHECK_STATUS("decode_tokens", wuffs_json__decoder__decode_tokens(
                                      &dec, &tok, &src, g_work_slice_u8));
    if (src.meta.ri != 3) {
      RETURN_FAIL("src.meta.ri: have %zu, want 3", src.meta.ri);
    }

    const char* have =
        wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
            .repr;
    if (have != wuffs_base__note__end_of_data) {
      RETURN_FAIL("decode_tokens: have \"%s\", want \"%s\"", have,
                  wuffs_base__note__end_of_data);
    }
    if (src.meta.ri != 3) {
      RETURN_FAIL("src.meta.ri: have %zu, want 3", src.meta.ri);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_json_decode_long_numbers() {
  CHECK_FOCUS(__func__);

  // Spelling "false" with four letters helps clang-format align the test
  // cases, when viewed in a fixed width font.
  const bool fals = false;

  // Each test case produces multiple test strings: the test_cases[tc].suffix
  // field is prefixed with N '9's, for multiple values of N, so that the test
  // string's total length is near WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL.
  // For example, a ".2e4" suffix means an overall string of "999etc999.2e4".
  //
  // The test_cases[tc].valid field holds the whether overall string is a valid
  // JSON number.
  struct {
    bool valid;
    const char* suffix;
  } test_cases[] = {
      {.valid = true, .suffix = ""},
      {.valid = true, .suffix = " "},
      {.valid = fals, .suffix = "."},
      {.valid = fals, .suffix = ". "},
      {.valid = fals, .suffix = "E"},
      {.valid = fals, .suffix = "E "},
      {.valid = fals, .suffix = "E-"},
      {.valid = fals, .suffix = "E- "},
      {.valid = true, .suffix = "e2"},
      {.valid = true, .suffix = "e2 "},
      {.valid = true, .suffix = "e+34"},
      {.valid = true, .suffix = "e+34 "},
      {.valid = true, .suffix = ".2"},
      {.valid = true, .suffix = ".2 "},
      {.valid = fals, .suffix = ".2e"},
      {.valid = fals, .suffix = ".2e "},
      {.valid = fals, .suffix = ".2e+"},
      {.valid = fals, .suffix = ".2e+ "},
      {.valid = true, .suffix = ".2e4"},
      {.valid = true, .suffix = ".2e4 "},
      {.valid = true, .suffix = ".2E+5"},
      {.valid = true, .suffix = ".2E+5 "},
      {.valid = true, .suffix = ".2e-5678"},
      {.valid = true, .suffix = ".2e-5678 "},
  };

  // src_array holds the overall test string. 119 is arbitrary but long enough.
  // See the "if (suffix_length > etc)" check below. 102 is also arbitrary but
  // larger than WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL.
  //
  // See also test_wuffs_json_decode_src_io_buffer_length.
  uint8_t src_array[119];
  memset(&src_array[0], '9', 102);
  if (102 <= WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL) {
    RETURN_FAIL("insufficient number_length test case coverage");
  }

  wuffs_json__decoder dec;

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    size_t suffix_length = strlen(test_cases[tc].suffix);
    if ((suffix_length + 1) > (119 - 102)) {  // +1 for the terminal NUL.
      RETURN_FAIL("tc=%zu: src_array is too short", tc);
    }
    bool ends_with_space = (suffix_length > 0) &&
                           (test_cases[tc].suffix[suffix_length - 1] == ' ');

    // Copying the terminal NUL isn't necessary for Wuffs' slices (which are a
    // pointer-length pair), but this backstop can help debugging with printf
    // where "%s" takes a C string (a bare pointer).
    memcpy(&src_array[102], test_cases[tc].suffix, suffix_length + 1);

    size_t nines_length;
    for (nines_length = 90; nines_length < 102; nines_length++) {
      wuffs_base__slice_u8 src_data = ((wuffs_base__slice_u8){
          .ptr = &src_array[102 - nines_length],
          .len = nines_length + suffix_length,
      });
      size_t number_length = src_data.len - (ends_with_space ? 1 : 0);

      int closed;
      for (closed = 0; closed < 2; closed++) {
        CHECK_STATUS(
            "initialize",
            wuffs_json__decoder__initialize(
                &dec, sizeof dec, WUFFS_VERSION,
                WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

        wuffs_base__token_buffer tok =
            wuffs_base__slice_token__writer(g_have_slice_token);
        wuffs_base__io_buffer src =
            wuffs_base__slice_u8__reader(src_data, closed != 0);
        const char* have = wuffs_json__decoder__decode_tokens(&dec, &tok, &src,
                                                              g_work_slice_u8)
                               .repr;

        size_t total_length = 0;
        while (tok.meta.ri < tok.meta.wi) {
          wuffs_base__token* t = &tok.data.ptr[tok.meta.ri++];
          total_length += wuffs_base__token__length(t);
        }
        if (total_length != src.meta.ri) {
          RETURN_FAIL(
              "tc=%zu, nines_length=%zu, closed=%d: total_length: have %zu, "
              "want %zu",
              tc, nines_length, closed, total_length, src.meta.ri);
        }

        const char* want;
        if (number_length > WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL) {
          want = wuffs_json__error__unsupported_number_length;
        } else if (closed || ends_with_space) {
          want = test_cases[tc].valid ? NULL : wuffs_json__error__bad_input;
        } else {
          want = wuffs_base__suspension__short_read;
        }

        if (have != want) {
          RETURN_FAIL(
              "tc=%zu, nines_length=%zu, closed=%d: have \"%s\", want \"%s\"",
              tc, nines_length, closed, have, want);
        }
      }
    }
  }

  return NULL;
}

// test_wuffs_json_decode_prior_valid_utf_8 tests that when encountering
// invalid or incomplete UTF-8, or a backslash-escape, any prior valid UTF-8 is
// still output. The decoder batches output so that, ignoring the quotation
// marks, "abc\xCE\x94efg" can be a single 8-length token instead of multiple
// (e.g. 3+2+3) tokens. On the other hand, while "abc\xFF" ends with one byte
// of invalid UTF-8, the 3 good bytes before that should still be output.
const char*  //
test_wuffs_json_decode_prior_valid_utf_8() {
  CHECK_FOCUS(__func__);

  // The test cases contain combinations of valid, partial and invalid UTF-8:
  //  - "\xCE\x94"         is U+00000394 GREEK CAPITAL LETTER DELTA.
  //  - "\xE2\x98\x83"     is U+00002603 SNOWMAN.
  //  - "\xF0\x9F\x92\xA9" is U+0001F4A9 PILE OF POO.
  //
  // The code below can also add trailing 's' bytes, which change e.g. the
  // partial multi-byte UTF-8 "\xE2" to be the invalid UTF-8 "\xE2s".
  const char* test_cases[] = {
      "",
      "\\t",
      "\\u",
      "\\u1234",
      "\x1F",  // Valid UTF-8 but invalid in a JSON string.
      "\x20",
      "\xCE",
      "\xCE\x94",
      "\xE2",
      "\xE2\x98",
      "\xE2\x98\x83",
      "\xE2\x98\x83\xCE",
      "\xE2\x98\x83\xCE\x94",
      "\xF0",
      "\xF0\x9F",
      "\xF0\x9F\x92",
      "\xF0\x9F\x92\xA9",
      "\xF0\x9F\x92\xA9\xCE",
      "\xF0\x9F\x92\xA9\xCE\x94",
  };

  size_t prefixes[] = {
      0,
      1,
      15,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 9,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 8,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 7,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 6,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 5,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 4,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 3,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 2,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL - 1,
      WUFFS_BASE__TOKEN__LENGTH__MAX_INCL + 0,
  };

  size_t suffixes[] = {0, 1, 17};

  wuffs_json__decoder dec;

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    size_t n = strlen(test_cases[tc]);
    size_t num_preceding = 0;
    while (num_preceding < n) {
      wuffs_base__utf_8__next__output x = wuffs_base__utf_8__next(
          (uint8_t*)(void*)(test_cases[tc]) + num_preceding, n - num_preceding);
      if (!wuffs_base__utf_8__next__output__is_valid(&x) ||
          (x.code_point < 0x20) || (x.code_point == '\\')) {
        break;
      }
      num_preceding += x.byte_length;
      if (num_preceding > n) {
        RETURN_FAIL("tc=%zu: utf_8__next overflow", tc);
      }
    }

    int pre;
    for (pre = 0; pre < WUFFS_TESTLIB_ARRAY_SIZE(prefixes); pre++) {
      size_t prefix = prefixes[pre];

      int suf;
      for (suf = 0; suf < WUFFS_TESTLIB_ARRAY_SIZE(suffixes); suf++) {
        size_t suffix = suffixes[suf];

        // Set src to "\"ppp...pppMIDDLEsss...sss", with a leading quotation
        // mark, where prefix and suffix are the number of 'p's and 's's and
        // test_cases[tc] is the "MIDDLE".
        wuffs_base__slice_u8 src_data = ((wuffs_base__slice_u8){
            .ptr = &g_src_array_u8[0],
            .len = 1 + prefix + n + suffix,
        });
        if (src_data.len > IO_BUFFER_ARRAY_SIZE) {
          RETURN_FAIL("total src length is too long");
        }
        src_data.ptr[0] = '\"';
        memset(&src_data.ptr[1], 'p', prefix);
        memcpy(&src_data.ptr[1 + prefix], test_cases[tc], n);
        memset(&src_data.ptr[1 + prefix + n], 's', suffix);

        int closed;
        for (closed = 0; closed < 2; closed++) {
          CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                         &dec, sizeof dec, WUFFS_VERSION, 0));

          wuffs_base__token_buffer tok =
              wuffs_base__slice_token__writer(g_have_slice_token);
          wuffs_base__io_buffer src =
              wuffs_base__slice_u8__reader(src_data, closed != 0);
          wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8);

          size_t have = 0;
          while (tok.meta.ri < tok.meta.wi) {
            wuffs_base__token* t = &tok.data.ptr[tok.meta.ri++];
            int64_t vbc = wuffs_base__token__value_base_category(t);
            if (vbc == WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT) {
              break;
            } else if (vbc == WUFFS_BASE__TOKEN__VBC__STRING) {
              have += wuffs_base__token__length(t);
            } else {
              RETURN_FAIL(
                  "tc=%zu, prefix=%zu, suffix=%zu, closed=%d: unexpected token",
                  tc, prefix, suffix, closed);
            }
          }
          size_t want = 1 + prefix + num_preceding;  // 1 for the leading '\"'.
          if (num_preceding == n) {
            want += suffix;
          }
          if (have != want) {
            RETURN_FAIL(
                "tc=%zu, prefix=%zu, suffix=%zu, closed=%d: have %zu, want %zu",
                tc, prefix, suffix, closed, have, want);
          }
        }
      }
    }
  }

  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_allow_backslash_etc() {
  CHECK_FOCUS(__func__);

  struct {
    uint32_t want;
    const char* str;
    uint32_t quirk;
  } test_cases[] = {
      {
          .want = 0x09,
          .str = "\"\t\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_ASCII_CONTROL_CODES,
      },
      {
          .want = 0x07,
          .str = "\"\\a\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_A,
      },
      {
          .want = 0x0001F4A9,
          .str = "\"\\U0001F4A9\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_CAPITAL_U,
      },
      {
          .want = 0x1B,
          .str = "\"\\e\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_E,
      },
      {
          .want = 0x0A,
          .str = "\"\\\n\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_NEW_LINE,
      },
      {
          .want = 0x3F,
          .str = "\"\\?\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_QUESTION_MARK,
      },
      {
          .want = 0x27,
          .str = "\"\\'\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_SINGLE_QUOTE,
      },
      {
          .want = 0x0B,
          .str = "\"\\v\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_V,
      },
      {
          .want = 0x00,
          .str = "\"\\0\"",
          .quirk = WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_ZERO,
      },
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    for (int q = 0; q < 2; q++) {
      wuffs_json__decoder dec;
      CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                     &dec, sizeof dec, WUFFS_VERSION,
                                     WUFFS_INITIALIZE__DEFAULT_OPTIONS));
      wuffs_json__decoder__set_quirk(&dec, test_cases[tc].quirk, q);

      wuffs_base__token_buffer tok =
          wuffs_base__slice_token__writer(g_have_slice_token);
      wuffs_base__io_buffer src =
          wuffs_base__ptr_u8__reader((uint8_t*)(void*)test_cases[tc].str,
                                     strlen(test_cases[tc].str), true);

      const char* have_status_repr =
          wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
              .repr;
      const char* want_status_repr =
          q ? NULL : wuffs_json__error__bad_backslash_escape;
      if ((test_cases[tc].quirk ==
           WUFFS_JSON__QUIRK_ALLOW_ASCII_CONTROL_CODES) &&
          want_status_repr) {
        want_status_repr = wuffs_json__error__bad_c0_control_code;
      }
      if (have_status_repr != want_status_repr) {
        RETURN_FAIL("tc=%zu, q=%d: decode_tokens: have \"%s\", want \"%s\"", tc,
                    q, have_status_repr, want_status_repr);
      }
      if (want_status_repr != NULL) {
        continue;
      }

      uint32_t have = 0;
      while (tok.meta.ri < tok.meta.wi) {
        wuffs_base__token* t = &tok.data.ptr[tok.meta.ri++];
        int64_t vbc = wuffs_base__token__value_base_category(t);
        uint64_t vbd = wuffs_base__token__value_base_detail(t);
        if (vbc == WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT) {
          have = vbd;
          break;
        }
      }
      if (have != test_cases[tc].want) {
        RETURN_FAIL("tc=%zu, q=%d: Unicode code point: have U+%04" PRIX32
                    ", want U+%04" PRIX32,
                    tc, q, have, test_cases[tc].want);
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_allow_backslash_x() {
  CHECK_FOCUS(__func__);

  struct {
    uint64_t want_code_points;
    const char* want_status_repr;
    const char* str;
  } test_cases[] = {
      // U+009A in UTF-8 is C2 9A.
      // U+00FF in UTF-8 is C3 BF.
      // U+3456 in UTF-8 is E3 91 96.
      {.want_code_points = 0x12E3919678C29A,
       .want_status_repr = NULL,
       .str = "\"\\x12\\u3456\\x78\\x9A\""},
      {.want_code_points = 0xC3BF,
       .want_status_repr = NULL,
       .str = "\"\\xFf\""},
      {.want_code_points = 0x00,
       .want_status_repr = wuffs_json__error__bad_backslash_escape,
       .str = "\"a\\X6A\""},
      {.want_code_points = 0x6A6B,
       .want_status_repr = NULL,
       .str = "\"a\\x6A\\x6bz\""},
      {.want_code_points = 0x6A,
       .want_status_repr = wuffs_json__error__bad_backslash_escape,
       .str = "\"a\\x6A\\x6yz\""},
      {.want_code_points = 0x00,
       .want_status_repr = wuffs_json__error__bad_backslash_escape,
       .str = "\"a\\x\""},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_json__decoder dec;
    CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                   &dec, sizeof dec, WUFFS_VERSION,
                                   WUFFS_INITIALIZE__DEFAULT_OPTIONS));
    wuffs_json__decoder__set_quirk(
        &dec, WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_X_AS_CODE_POINTS, 1);

    wuffs_base__token_buffer tok =
        wuffs_base__slice_token__writer(g_have_slice_token);
    wuffs_base__slice_u8 src_slice = wuffs_base__make_slice_u8(
        (uint8_t*)(void*)(test_cases[tc].str), strlen(test_cases[tc].str));
    wuffs_base__io_buffer src = wuffs_base__slice_u8__reader(src_slice, true);
    const char* have_status_repr =
        wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
            .repr;
    if (have_status_repr != test_cases[tc].want_status_repr) {
      RETURN_FAIL("tc=%zu: decode_tokens: have \"%s\", want \"%s\"", tc,
                  have_status_repr, test_cases[tc].want_status_repr);
    }

    uint64_t src_index = 0;
    uint64_t have = 0;
    while (tok.meta.ri < tok.meta.wi) {
      wuffs_base__token* t = &tok.data.ptr[tok.meta.ri++];
      int64_t vbc = wuffs_base__token__value_base_category(t);
      uint64_t vbd = wuffs_base__token__value_base_detail(t);
      uint64_t token_length = wuffs_base__token__length(t);
      if (vbc == WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT) {
        uint8_t b[WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL];
        size_t n = wuffs_base__utf_8__encode(
            wuffs_base__make_slice_u8(&b[0],
                                      WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL),
            vbd);
        size_t i = 0;
        for (; i < n; i++) {
          have <<= 8;
          have |= b[i];
        }
      }

      src_index += token_length;
    }

    if (src_index != src.meta.ri) {
      RETURN_FAIL("tc=%zu: src_index: have %" PRIu64 ", want %zu", tc,
                  src_index, src.meta.ri);
    }

    uint64_t want = test_cases[tc].want_code_points;
    if (have != want) {
      RETURN_FAIL("tc=%zu: have U+%08" PRIX64 ", want U+%08" PRIX64, tc, have,
                  want);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_allow_extra_comma() {
  CHECK_FOCUS(__func__);

  struct {
    // want has 2 bytes, one for each possible q:
    //  - q&1 sets WUFFS_JSON__QUIRK_ALLOW_EXTRA_COMMA.
    // An 'X', '+' or '-' means that decoding should succeed (and consume the
    // entire input), succeed (without consuming the entire input) or fail.
    const char* want;
    const char* str;
  } test_cases[] = {
      {.want = "-X", .str = "[0,]"},
      {.want = "-X", .str = "[[], {},{\"k\":\"v\",\n}\n,\n]"},
      {.want = "--", .str = "[,]"},
      {.want = "--", .str = "{,}"},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    for (int q = 0; q < 2; q++) {
      wuffs_json__decoder dec;
      CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                     &dec, sizeof dec, WUFFS_VERSION,
                                     WUFFS_INITIALIZE__DEFAULT_OPTIONS));
      wuffs_json__decoder__set_quirk(&dec, WUFFS_JSON__QUIRK_ALLOW_EXTRA_COMMA,
                                     q & 1);

      wuffs_base__token_buffer tok =
          wuffs_base__slice_token__writer(g_have_slice_token);
      wuffs_base__io_buffer src =
          wuffs_base__ptr_u8__reader((uint8_t*)(void*)test_cases[tc].str,
                                     strlen(test_cases[tc].str), true);
      const char* have =
          wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
              .repr;
      const char* want =
          (test_cases[tc].want[q] != '-') ? NULL : wuffs_json__error__bad_input;
      if (have != want) {
        RETURN_FAIL("tc=%zu, q=%d: decode_tokens: have \"%s\", want \"%s\"", tc,
                    q, have, want);
      }

      size_t total_length = 0;
      while (tok.meta.ri < tok.meta.wi) {
        total_length += wuffs_base__token__length(&tok.data.ptr[tok.meta.ri++]);
      }
      if (total_length != src.meta.ri) {
        RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                    total_length, src.meta.ri);
      }
      if (test_cases[tc].want[q] == 'X') {
        if (total_length != src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                      total_length, src.data.len);
        }
      } else if (test_cases[tc].want[q] == '+') {
        if (total_length >= src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want < %zu", tc, q,
                      total_length, src.data.len);
        }
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_allow_inf_nan_numbers() {
  CHECK_FOCUS(__func__);

  struct {
    // want has 2 bytes, one for each possible q:
    //  - q&1 sets WUFFS_JSON__QUIRK_ALLOW_INF_NAN_NUMBERS.
    // An 'X', '+' or '-' means that decoding should succeed (and consume the
    // entire input), succeed (without consuming the entire input) or fail.
    const char* want;
    const char* str;
  } test_cases[] = {
      {.want = "-X", .str = "InFiniTy"},
      {.want = "-X", .str = "[+inf, -infinity, +nan,-NaN,NAN]"},
      {.want = "-X", .str = "inf"},
      {.want = "-+", .str = "infinit"},
      {.want = "-+", .str = "infiQity"},
      {.want = "-+", .str = "nana"},
      {.want = "--", .str = "+-inf"},
      {.want = "--", .str = "-+inf"},
      {.want = "--", .str = "[infinit,"},
      {.want = "--", .str = "[infiQity,"},
      {.want = "--", .str = "[nana,"},
      {.want = "--", .str = "∞"},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    for (int q = 0; q < 2; q++) {
      wuffs_json__decoder dec;
      CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                     &dec, sizeof dec, WUFFS_VERSION,
                                     WUFFS_INITIALIZE__DEFAULT_OPTIONS));
      wuffs_json__decoder__set_quirk(
          &dec, WUFFS_JSON__QUIRK_ALLOW_INF_NAN_NUMBERS, q & 1);

      wuffs_base__token_buffer tok =
          wuffs_base__slice_token__writer(g_have_slice_token);
      wuffs_base__io_buffer src =
          wuffs_base__ptr_u8__reader((uint8_t*)(void*)test_cases[tc].str,
                                     strlen(test_cases[tc].str), true);
      const char* have =
          wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
              .repr;
      const char* want =
          (test_cases[tc].want[q] != '-') ? NULL : wuffs_json__error__bad_input;
      if (have != want) {
        RETURN_FAIL("tc=%zu, q=%d: decode_tokens: have \"%s\", want \"%s\"", tc,
                    q, have, want);
      }

      size_t total_length = 0;
      while (tok.meta.ri < tok.meta.wi) {
        total_length += wuffs_base__token__length(&tok.data.ptr[tok.meta.ri++]);
      }
      if (total_length != src.meta.ri) {
        RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                    total_length, src.meta.ri);
      }
      if (test_cases[tc].want[q] == 'X') {
        if (total_length != src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                      total_length, src.data.len);
        }
      } else if (test_cases[tc].want[q] == '+') {
        if (total_length >= src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want < %zu", tc, q,
                      total_length, src.data.len);
        }
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_allow_comment_etc() {
  CHECK_FOCUS(__func__);

  struct {
    // want has 4 bytes, one for each possible q:
    //  - q&1 sets WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK.
    //  - q&2 sets WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE.
    // An 'X', '+' or '-' means that decoding should succeed (and consume the
    // entire input), succeed (without consuming the entire input) or fail.
    const char* want;
    const char* str;
  } test_cases[] = {
      {.want = "-X-X", .str = "[ /*com*/ 0]"},
      {.want = "--XX", .str = "//l\n  //m\n0"},
      {.want = "---X", .str = "[ 0, /*com*/ 1 //l\n\n]"},
      {.want = "----", .str = "/*/0"},   // Not a valid slash-star comment.
      {.want = "----", .str = "[4/5]"},  // Lone slash.
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    for (int q = 0; q < 4; q++) {
      wuffs_json__decoder dec;
      CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                     &dec, sizeof dec, WUFFS_VERSION,
                                     WUFFS_INITIALIZE__DEFAULT_OPTIONS));
      wuffs_json__decoder__set_quirk(
          &dec, WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK, q & 1);
      wuffs_json__decoder__set_quirk(&dec, WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE,
                                     q & 2);

      wuffs_base__token_buffer tok =
          wuffs_base__slice_token__writer(g_have_slice_token);
      wuffs_base__io_buffer src =
          wuffs_base__ptr_u8__reader((uint8_t*)(void*)test_cases[tc].str,
                                     strlen(test_cases[tc].str), true);
      const char* have =
          wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
              .repr;
      const char* want =
          (test_cases[tc].want[q] != '-') ? NULL : wuffs_json__error__bad_input;
      if (have != want) {
        RETURN_FAIL("tc=%zu, q=%d: decode_tokens: have \"%s\", want \"%s\"", tc,
                    q, have, want);
      }

      size_t total_length = 0;
      while (tok.meta.ri < tok.meta.wi) {
        total_length += wuffs_base__token__length(&tok.data.ptr[tok.meta.ri++]);
      }
      if (total_length != src.meta.ri) {
        RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                    total_length, src.meta.ri);
      }
      if (test_cases[tc].want[q] == 'X') {
        if (total_length != src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                      total_length, src.data.len);
        }
      } else if (test_cases[tc].want[q] == '+') {
        if (total_length >= src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want < %zu", tc, q,
                      total_length, src.data.len);
        }
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_allow_leading_etc() {
  CHECK_FOCUS(__func__);

  struct {
    // want has 4 bytes, one for each possible q:
    //  - q&1 sets WUFFS_JSON__QUIRK_ALLOW_LEADING_ASCII_RECORD_SEPARATOR.
    //  - q&2 sets WUFFS_JSON__QUIRK_ALLOW_LEADING_UNICODE_BYTE_ORDER_MARK.
    // An 'X', '+' or '-' means that decoding should succeed (and consume the
    // entire input), succeed (without consuming the entire input) or fail.
    const char* want;
    const char* str;
  } test_cases[] = {
      {.want = "-X-X", .str = "\x1Etrue"},
      {.want = "--XX", .str = "\xEF\xBB\xBFtrue"},
      {.want = "---X", .str = "\x1E\xEF\xBB\xBFtrue"},
      {.want = "---X", .str = "\xEF\xBB\xBF\x1Etrue"},
      {.want = "----", .str = " \x1Etrue"},
      {.want = "----", .str = "\x1E \xEF\xBB\xBFtrue"},
      {.want = "----", .str = "\x1E\x1Etrue"},
      {.want = "----", .str = "\xEF\xBB"},
      {.want = "----", .str = "\xEF\xBB\xBF"},
      {.want = "----", .str = "\xEF\xBB\xBF$"},
      {.want = "----", .str = "\xEFtrue"},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    for (int q = 0; q < 4; q++) {
      wuffs_json__decoder dec;
      CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                     &dec, sizeof dec, WUFFS_VERSION,
                                     WUFFS_INITIALIZE__DEFAULT_OPTIONS));
      wuffs_json__decoder__set_quirk(
          &dec, WUFFS_JSON__QUIRK_ALLOW_LEADING_ASCII_RECORD_SEPARATOR, q & 1);
      wuffs_json__decoder__set_quirk(
          &dec, WUFFS_JSON__QUIRK_ALLOW_LEADING_UNICODE_BYTE_ORDER_MARK, q & 2);

      wuffs_base__token_buffer tok =
          wuffs_base__slice_token__writer(g_have_slice_token);
      wuffs_base__io_buffer src =
          wuffs_base__ptr_u8__reader((uint8_t*)(void*)test_cases[tc].str,
                                     strlen(test_cases[tc].str), true);
      const char* have =
          wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
              .repr;
      const char* want =
          (test_cases[tc].want[q] != '-') ? NULL : wuffs_json__error__bad_input;
      if (have != want) {
        RETURN_FAIL("tc=%zu, q=%d: decode_tokens: have \"%s\", want \"%s\"", tc,
                    q, have, want);
      }

      size_t total_length = 0;
      while (tok.meta.ri < tok.meta.wi) {
        total_length += wuffs_base__token__length(&tok.data.ptr[tok.meta.ri++]);
      }
      if (total_length != src.meta.ri) {
        RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                    total_length, src.meta.ri);
      }
      if (test_cases[tc].want[q] == 'X') {
        if (total_length != src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                      total_length, src.data.len);
        }
      } else if (test_cases[tc].want[q] == '+') {
        if (total_length >= src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want < %zu", tc, q,
                      total_length, src.data.len);
        }
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_allow_trailing_comments() {
  CHECK_FOCUS(__func__);

  // The first byte is a code.
  //  - '1' means that there is zero or one '\n' bytes
  //  - '2' means that there are two '\n' bytes but no comments
  //  - '3' means that there are comments
  //  - '4' means that there is non-filler after  eof-or-'\n'
  //  - '5' means that there is non-filler before eof-or-'\n'
  //
  // The second and third bytes are digits such that the overall string starts
  // with a three digit number, which is parsed as the top-level JSON value and
  // everything afterwards is trailer.
  //
  // That three digit number modulo 100 also acts as a 'line number' (matching
  // the tc loop variable), starting counting from zero, so that the 6th test
  // case, tc == 5, is the line starting with "something hundred and five".
  //
  // WUFFS_JSON__QUIRK_ALLOW_TRAILING_FILLER (together with
  // WUFFS_JSON__QUIRK_ALLOW_COMMENT_ETC) should decode the '1's, '2's and '3's
  // completely and the '4's and '5's up to but excluding the non-filler.
  //
  // WUFFS_JSON__QUIRK_EXPECT_TRAILING_NEW_LINE_OR_EOF should decode the '1's
  // completely and the '2's and '4's just after the first '\n'.
  const char* test_cases[] = {
      "100",                         //
      "101 \n",                      //
      "202\n\n",                     //
      "203 \n\n",                    //
      "304 /*foo*/",                 //
      "305 /*foo*/ ",                //
      "306 /*foo*/ \n",              //
      "307 /*foo*/ \n\n",            //
      "308/*bar\nbaz*/\n\n",         //
      "309 // qux",                  //
      "310 // qux\n",                //
      "311 // qux\n\n",              //
      "312 /*c0*/ /*c1*/\n\n",       //
      "313 /*c0*/ \n\n // c2 \n\n",  //
      "414 \n9",                     //
      "515 9",                       //
  };

  // Test ALLOW_ETC.
  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    void* tc_ptr = (void*)(test_cases[tc]);
    size_t tc_len = strlen(test_cases[tc]);
    char code = test_cases[tc][0];

    wuffs_base__token_buffer tok =
        wuffs_base__slice_token__writer(g_have_slice_token);
    wuffs_base__io_buffer src =
        wuffs_base__ptr_u8__reader((uint8_t*)tc_ptr, tc_len, true);
    wuffs_json__decoder dec;
    CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                   &dec, sizeof dec, WUFFS_VERSION,
                                   WUFFS_INITIALIZE__DEFAULT_OPTIONS));
    wuffs_json__decoder__set_quirk(&dec, WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK,
                                   1);
    wuffs_json__decoder__set_quirk(&dec, WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE,
                                   1);
    wuffs_json__decoder__set_quirk(&dec,
                                   WUFFS_JSON__QUIRK_ALLOW_TRAILING_FILLER, 1);

    const char* have_repr =
        wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
            .repr;
    if (have_repr != NULL) {
      RETURN_FAIL("tc=%zu, ALLOW_ETC: decode_tokens: have \"%s\", want NULL",
                  tc, have_repr);
    }

    size_t have_total_length = 0;
    while (tok.meta.ri < tok.meta.wi) {
      have_total_length +=
          wuffs_base__token__length(&tok.data.ptr[tok.meta.ri++]);
    }
    size_t want_total_length = tc_len - ((code >= '4') ? 1 : 0);
    if (have_total_length != src.meta.ri) {
      RETURN_FAIL("tc=%zu, ALLOW_ETC: total_length: have %zu, want %zu", tc,
                  have_total_length, src.meta.ri);
    } else if (have_total_length != want_total_length) {
      RETURN_FAIL("tc=%zu, ALLOW_ETC: total_length: have %zu, want %zu", tc,
                  have_total_length, want_total_length);
    }
  }

  // Test EXPECT_ETC.
  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    void* tc_ptr = (void*)(test_cases[tc]);
    size_t tc_len = strlen(test_cases[tc]);
    char code = test_cases[tc][0];

    wuffs_base__token_buffer tok =
        wuffs_base__slice_token__writer(g_have_slice_token);
    wuffs_base__io_buffer src =
        wuffs_base__ptr_u8__reader((uint8_t*)tc_ptr, tc_len, true);
    wuffs_json__decoder dec;
    CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                   &dec, sizeof dec, WUFFS_VERSION,
                                   WUFFS_INITIALIZE__DEFAULT_OPTIONS));
    wuffs_json__decoder__set_quirk(
        &dec, WUFFS_JSON__QUIRK_EXPECT_TRAILING_NEW_LINE_OR_EOF, 1);

    const char* have_repr =
        wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
            .repr;
    const char* want_repr =
        ((code == '3') || (code == '5')) ? wuffs_json__error__bad_input : NULL;
    if (have_repr != want_repr) {
      RETURN_FAIL("tc=%zu, EXPECT_ETC: decode_tokens: have \"%s\", want \"%s\"",
                  tc, have_repr, want_repr);
    } else if (have_repr != NULL) {
      continue;
    }

    size_t have_total_length = 0;
    while (tok.meta.ri < tok.meta.wi) {
      have_total_length +=
          wuffs_base__token__length(&tok.data.ptr[tok.meta.ri++]);
    }
    size_t want_total_length = tc_len - ((code == '1') ? 0 : 1);
    if (have_total_length != src.meta.ri) {
      RETURN_FAIL("tc=%zu, EXPECT_ETC: total_length: have %zu, want %zu", tc,
                  have_total_length, src.meta.ri);
    } else if (have_total_length != want_total_length) {
      RETURN_FAIL("tc=%zu, EXPECT_ETC: total_length: have %zu, want %zu", tc,
                  have_total_length, want_total_length);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_allow_trailing_filler() {
  CHECK_FOCUS(__func__);

  struct {
    // want has 3 bytes, one for each possible q:
    //  - q&1 sets WUFFS_JSON__QUIRK_ALLOW_TRAILING_FILLER.
    //  - q&2 sets WUFFS_JSON__QUIRK_EXPECT_TRAILING_NEW_LINE_OR_EOF.
    // An 'X', '+' or '-' means that decoding should succeed (and consume the
    // entire input), succeed (without consuming the entire input) or fail.
    const char* want;
    const char* str;
  } test_cases[] = {
      {.want = "+X+", .str = "0 \n "},      //
      {.want = "+X+", .str = "0 \n\n"},     //
      {.want = "+X+", .str = "0\n\n"},      //
      {.want = "++-", .str = "0 true \n"},  //
      {.want = "++-", .str = "007"},        //
      {.want = "++-", .str = "007\n"},      //
      {.want = "++-", .str = "0true "},     //
      {.want = "++-", .str = "0true"},      //
      {.want = "+XX", .str = "0 "},         //
      {.want = "+XX", .str = "0 \n"},       //
      {.want = "+XX", .str = "0\n"},        //
      {.want = "+XX", .str = "0\t\r\n"},    //
      {.want = "---", .str = "\n"},         //
      {.want = "XXX", .str = "0"},          //
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    for (int q = 0; q < 3; q++) {
      wuffs_json__decoder dec;
      CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                     &dec, sizeof dec, WUFFS_VERSION,
                                     WUFFS_INITIALIZE__DEFAULT_OPTIONS));
      wuffs_json__decoder__set_quirk(
          &dec, WUFFS_JSON__QUIRK_ALLOW_TRAILING_FILLER, q & 1);
      wuffs_json__decoder__set_quirk(
          &dec, WUFFS_JSON__QUIRK_EXPECT_TRAILING_NEW_LINE_OR_EOF, q & 2);

      wuffs_base__token_buffer tok =
          wuffs_base__slice_token__writer(g_have_slice_token);
      wuffs_base__io_buffer src =
          wuffs_base__ptr_u8__reader((uint8_t*)(void*)test_cases[tc].str,
                                     strlen(test_cases[tc].str), true);
      const char* have =
          wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8)
              .repr;
      const char* want =
          (test_cases[tc].want[q] != '-') ? NULL : wuffs_json__error__bad_input;
      if (have != want) {
        RETURN_FAIL("tc=%zu, q=%d: decode_tokens: have \"%s\", want \"%s\"", tc,
                    q, have, want);
      }

      size_t total_length = 0;
      while (tok.meta.ri < tok.meta.wi) {
        total_length += wuffs_base__token__length(&tok.data.ptr[tok.meta.ri++]);
      }
      if (total_length != src.meta.ri) {
        RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                    total_length, src.meta.ri);
      }
      if (test_cases[tc].want[q] == 'X') {
        if (total_length != src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want %zu", tc, q,
                      total_length, src.data.len);
        }
      } else if (test_cases[tc].want[q] == '+') {
        if (total_length >= src.data.len) {
          RETURN_FAIL("tc=%zu, q=%d: total_length: have %zu, want < %zu", tc, q,
                      total_length, src.data.len);
        }
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_json_decode_quirk_replace_invalid_unicode() {
  CHECK_FOCUS(__func__);

  // Decoding str should produce want, with invalid UTF-8 replaced by "?". A
  // proper JSON decoder (with the quirk enabled) would replace with
  // "\xEF\xBF\xBD", the UTF-8 encoding of U+FFFD, but using "?" leads to
  // clearer, shorter test cases.
  struct {
    const char* want;
    const char* str;
  } test_cases[] = {
      // Valid UTF-8.
      {.want = "abc", .str = "\"abc\""},
      {.want = "del\xCE\x94ta", .str = "\"del\\u0394ta\""},
      {.want = "del\xCE\x94ta", .str = "\"del\xCE\x94ta\""},

      // Invalid UTF-8: right byte lengths, wrong bytes.
      {.want = "1byte?yz", .str = "\"1byte\xFFyz\""},
      {.want = "2byte??yz", .str = "\"2byte\xCE\xFFyz\""},
      {.want = "3byte???yz", .str = "\"3byte\xE2\x98\xFFyz\""},
      {.want = "4byte????yz", .str = "\"4byte\xF0\x9F\x92\xFFyz\""},

      // Invalid UTF-8: wrong byte lengths.
      {.want = "?", .str = "\"\xCE\""},
      {.want = "?g", .str = "\"\xCEg\""},
      {.want = "?gh", .str = "\"\xCEgh\""},
      {.want = "j?", .str = "\"j\xE2\""},
      {.want = "j?l", .str = "\"j\xE2l\""},
      {.want = "j?lm", .str = "\"j\xE2lm\""},
      {.want = "?", .str = "\"\xF0\""},
      {.want = "?r", .str = "\"\xF0r\""},
      {.want = "?rs", .str = "\"\xF0rs\""},

      // U+DC00 (as an unpaired surrogate) is either 1 or 3 '?'s depending on
      // whether it's backslash-u or backslash-x.
      {.want = "a?z", .str = "\"a\\uDC00z\""},
      {.want = "a?zzzzzz", .str = "\"a\\uDC00zzzzzz\""},
      {.want = "a???z", .str = "\"a\xED\xB0\x80z\""},
      {.want = "a???zzzzzz", .str = "\"a\xED\xB0\x80zzzzzz\""},

      // 1 or 2 unpaired surrogates each become '?'s, but for 3 surrogates
      // where consecutive surrogates make a valid pair, there's only 1 '?'.
      {.want = "a?z", .str = "\"a\\uD800z\""},
      {.want = "a??z", .str = "\"a\\uD800\\uDBFFz\""},
      {.want = "a?\xF4\x8F\xBF\xBFz", .str = "\"a\\uD800\\uDBFF\\uDFFFz\""},
      {.want = "a\xF0\x90\x80\x80?z", .str = "\"a\\uD800\\uDC00\\uDFFFz\""},
  };

  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    wuffs_json__decoder dec;
    CHECK_STATUS("initialize", wuffs_json__decoder__initialize(
                                   &dec, sizeof dec, WUFFS_VERSION,
                                   WUFFS_INITIALIZE__DEFAULT_OPTIONS));
    wuffs_json__decoder__set_quirk(
        &dec, WUFFS_JSON__QUIRK_REPLACE_INVALID_UNICODE, 1);

    wuffs_base__io_buffer have = wuffs_base__slice_u8__writer(g_have_slice_u8);
    wuffs_base__token_buffer tok =
        wuffs_base__slice_token__writer(g_have_slice_token);
    wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(
        (uint8_t*)(void*)test_cases[tc].str, strlen(test_cases[tc].str), true);
    CHECK_STATUS("decode_tokens", wuffs_json__decoder__decode_tokens(
                                      &dec, &tok, &src, g_work_slice_u8));

    uint64_t src_index = 0;
    while (tok.meta.ri < tok.meta.wi) {
      wuffs_base__token* t = &tok.data.ptr[tok.meta.ri++];
      int64_t vbc = wuffs_base__token__value_base_category(t);
      uint64_t vbd = wuffs_base__token__value_base_detail(t);
      uint64_t token_length = wuffs_base__token__length(t);

      if (vbc == WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT) {
        uint8_t u[WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL];
        size_t n = wuffs_base__utf_8__encode(
            wuffs_base__make_slice_u8(&u[0],
                                      WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL),
            vbd);
        if (vbd == 0xFFFD) {
          u[0] = '?';
          n = 1;
        }
        if ((have.data.len - have.meta.wi) < n) {
          RETURN_FAIL("tc=%zu: token too long", tc);
        }
        memcpy(&have.data.ptr[have.meta.wi], &u[0], n);
        have.meta.wi += n;

      } else if (vbc == WUFFS_BASE__TOKEN__VBC__STRING) {
        if (vbd & WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_0_DST_1_SRC_DROP) {
          // No-op.
        } else if (vbd &
                   WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_1_DST_1_SRC_COPY) {
          if ((have.data.len - have.meta.wi) < token_length) {
            RETURN_FAIL("tc=%zu: token too long", tc);
          }
          memcpy(&have.data.ptr[have.meta.wi], &test_cases[tc].str[src_index],
                 token_length);
          have.meta.wi += token_length;
        } else {
          RETURN_FAIL("tc=%zu: unexpected string-token conversion", tc);
        }

      } else {
        RETURN_FAIL("tc=%zu: unexpected token", tc);
      }

      src_index += token_length;
    }

    if (src_index != src.meta.ri) {
      RETURN_FAIL("tc=%zu: src_index: have %" PRIu64 ", want %zu", tc,
                  src_index, src.meta.ri);
    }

    if (have.meta.wi >= have.data.len) {
      RETURN_FAIL("tc=%zu: too many have bytes", tc);
    }
    have.data.ptr[have.meta.wi] = '\x00';
    size_t len = strlen(test_cases[tc].want);
    if ((len != have.meta.wi) ||
        (memcmp(have.data.ptr, test_cases[tc].want, len) != 0)) {
      RETURN_FAIL("tc=%zu: have \"%s\", want \"%s\"", tc, have.data.ptr,
                  test_cases[tc].want);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_json_decode_unicode4_escapes() {
  CHECK_FOCUS(__func__);

  const uint32_t fail = 0xDEADBEEF;

  struct {
    uint32_t want;
    const char* str;
  } test_cases[] = {
      // Simple (non-surrogate) successes.
      {.want = 0x0000000A, .str = "\"\\u000a\""},
      {.want = 0x0000005C, .str = "\"\\\\u1234\""},  // U+005C is '\\'.
      {.want = 0x00001000, .str = "\"\\u10002345\""},
      {.want = 0x00001000, .str = "\"\\u1000234\""},
      {.want = 0x00001000, .str = "\"\\u100023\""},
      {.want = 0x00001000, .str = "\"\\u10002\""},
      {.want = 0x00001234, .str = "\"\\u1234\""},
      {.want = 0x0000D7FF, .str = "\"\\ud7ff\""},
      {.want = 0x0000E000, .str = "\"\\uE000\""},
      {.want = 0x0000FFFF, .str = "\"\\uFffF\""},

      // Unicode surrogate pair. U+0001F4A9 PILE OF POO is (U+D83D, U+DCA9),
      // because ((0x03D << 10) | 0x0A9) is 0xF4A9:
      //  - High surrogates are in the range U+D800 ..= U+DBFF.
      //  - Low  surrogates are in the range U+DC00 ..= U+DFFF.
      {.want = 0x0001F4A9, .str = "\"\\uD83D\\udca9\""},

      // More surrogate pairs.
      {.want = 0x00010000, .str = "\"\\uD800\\uDC00\""},
      {.want = 0x0010FFFF, .str = "\"\\uDBFF\\uDFFF\""},

      // Simple (non-surrogate) failures.
      {.want = fail, .str = "\"\\U1234\""},
      {.want = fail, .str = "\"\\u123"},
      {.want = fail, .str = "\"\\u123\""},
      {.want = fail, .str = "\"\\u123x\""},
      {.want = fail, .str = "\"u1234\""},

      // Invalid surrogate pairs.
      {.want = fail, .str = "\"\\uD800\""},         // High alone.
      {.want = fail, .str = "\"\\uD83D?udca9\""},   // High then not "\\u".
      {.want = fail, .str = "\"\\uD83D\\ud7ff\""},  // High then non-surrogate.
      {.want = fail, .str = "\"\\uD83D\\udbff\""},  // High then high.
      {.want = fail, .str = "\"\\uD83D\\ue000\""},  // High then non-surrogate.
      {.want = fail, .str = "\"\\uDC00\""},         // Low alone.
      {.want = fail, .str = "\"\\uDC00\\u0000\""},  // Low then non-surrogate.
      {.want = fail, .str = "\"\\uDC00\\ud800\""},  // Low then high.
      {.want = fail, .str = "\"\\uDC00\\udfff\""},  // Low then low.
      {.want = fail, .str = "\"\\uDFFF1234\""},     // Low alone.
  };

  wuffs_json__decoder dec;
  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    CHECK_STATUS("initialize",
                 wuffs_json__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    wuffs_base__token_buffer tok =
        wuffs_base__slice_token__writer(g_have_slice_token);
    wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(
        (uint8_t*)(void*)test_cases[tc].str, strlen(test_cases[tc].str), true);
    wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8);

    uint32_t have = fail;
    uint64_t total_length = 0;
    for (size_t i = tok.meta.ri; i < tok.meta.wi; i++) {
      wuffs_base__token* t = &tok.data.ptr[i];
      total_length =
          wuffs_base__u64__sat_add(total_length, wuffs_base__token__length(t));

      // Set have to the first Unicode code point token.
      if ((have == fail) && ((wuffs_base__token__value_base_category(t) ==
                              WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT))) {
        have = wuffs_base__token__value_base_detail(t);
        if (have > 0x10FFFF) {  // This also catches "have == fail".
          RETURN_FAIL("%s: invalid Unicode code point", test_cases[tc].str);
        }

        uint64_t have_length = wuffs_base__token__length(t);
        uint64_t want_length = (have == 0x5C) ? 2 : ((have <= 0xFFFF) ? 6 : 12);
        if (have_length != want_length) {
          RETURN_FAIL("%s: token length: have %" PRIu64 ", want %" PRIu64,
                      test_cases[tc].str, have_length, want_length);
        }
      }
    }

    if (have != test_cases[tc].want) {
      RETURN_FAIL("%s: have 0x%" PRIX32 ", want 0x%" PRIX32, test_cases[tc].str,
                  have, test_cases[tc].want);
    }

    if (total_length != src.meta.ri) {
      RETURN_FAIL("%s: total length: have %" PRIu64 ", want %zu",
                  test_cases[tc].str, total_length, src.meta.ri);
    }
  }

  return NULL;
}

// test_wuffs_json_decode_src_io_buffer_length tests that given a sufficient
// amount of source data (WUFFS_JSON__DECODER_SRC_IO_BUFFER_LENGTH_MIN_INCL or
// more), decoding will always return a conclusive result, not a suspension
// such as "$short read".
//
// The JSON specification doesn't give a maximum byte length for a number, but
// implementations are permitted to impose one. Wuffs' implementation imposes
// WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL.
const char*  //
test_wuffs_json_decode_src_io_buffer_length() {
  CHECK_FOCUS(__func__);

  if (WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL >=
      WUFFS_JSON__DECODER_SRC_IO_BUFFER_LENGTH_MIN_INCL) {
    RETURN_FAIL(
        "inconsistent WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL vs "
        "WUFFS_JSON__DECODER_SRC_IO_BUFFER_LENGTH_MIN_INCL");
  }

  // src_array holds the test string of repeated '7's. 107 is arbitrary but
  // long enough for the loop below.
  uint8_t src_array[107];
  memset(&src_array[0], '7', 107);

  wuffs_json__decoder dec;

  for (int i = WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL - 2;
       i <= WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL + 2; i++) {
    if (i < 0) {
      RETURN_FAIL("invalid test case: i=%d", i);
    } else if (i > 107) {
      RETURN_FAIL("invalid test case: i=%d", i);
    }

    wuffs_base__slice_u8 src_data = ((wuffs_base__slice_u8){
        .ptr = &src_array[0],
        .len = (size_t)(i),
    });

    int closed;
    for (closed = 0; closed < 2; closed++) {
      wuffs_base__token_buffer tok =
          wuffs_base__slice_token__writer(g_have_slice_token);
      wuffs_base__io_buffer src =
          wuffs_base__slice_u8__reader(src_data, closed != 0);
      CHECK_STATUS("initialize",
                   wuffs_json__decoder__initialize(
                       &dec, sizeof dec, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

      wuffs_base__status have =
          wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8);
      const char* want =
          (i > WUFFS_JSON__DECODER_NUMBER_LENGTH_MAX_INCL)
              ? wuffs_json__error__unsupported_number_length
              : (closed ? NULL : wuffs_base__suspension__short_read);
      if (have.repr != want) {
        RETURN_FAIL("i=%d, closed=%d: have \"%s\", want \"%s\"", i, closed,
                    have.repr, want);
      }

      if ((i >= WUFFS_JSON__DECODER_SRC_IO_BUFFER_LENGTH_MIN_INCL) &&
          wuffs_base__status__is_suspension(&have)) {
        RETURN_FAIL("i=%d, closed=%d: have a suspension", i, closed);
      }
    }
  }

  return NULL;
}

const char*  //
test_wuffs_json_decode_string() {
  CHECK_FOCUS(__func__);

  const char* bad_bac = wuffs_json__error__bad_backslash_escape;
  const char* bad_ccc = wuffs_json__error__bad_c0_control_code;
  const char* bad_utf = wuffs_json__error__bad_utf_8;

  struct {
    const char* want_status_repr;
    const char* str;
  } test_cases[] = {
      {.want_status_repr = NULL, .str = "\"+++\\\"+\\/+\\\\+++\""},
      {.want_status_repr = NULL, .str = "\"+++\\b+\\f+\\n+\\r+\\t+++\""},
      {.want_status_repr = NULL, .str = "\"\x20\""},              // U+00000020.
      {.want_status_repr = NULL, .str = "\"\xC2\x80\""},          // U+00000080.
      {.want_status_repr = NULL, .str = "\"\xCE\x94\""},          // U+00000394.
      {.want_status_repr = NULL, .str = "\"\xDF\xBF\""},          // U+000007FF.
      {.want_status_repr = NULL, .str = "\"\xE0\xA0\x80\""},      // U+00000800.
      {.want_status_repr = NULL, .str = "\"\xE2\x98\x83\""},      // U+00002603.
      {.want_status_repr = NULL, .str = "\"\xED\x80\x80\""},      // U+0000D000.
      {.want_status_repr = NULL, .str = "\"\xED\x9F\xBF\""},      // U+0000D7FF.
      {.want_status_repr = NULL, .str = "\"\xEE\x80\x80\""},      // U+0000E000.
      {.want_status_repr = NULL, .str = "\"\xEF\xBF\xBD\""},      // U+0000FFFD.
      {.want_status_repr = NULL, .str = "\"\xEF\xBF\xBF\""},      // U+0000FFFF.
      {.want_status_repr = NULL, .str = "\"\xF0\x90\x80\x80\""},  // U+00010000.
      {.want_status_repr = NULL, .str = "\"\xF0\x9F\x92\xA9\""},  // U+0001F4A9.
      {.want_status_repr = NULL, .str = "\"\xF0\xB0\x80\x81\""},  // U+00030001.
      {.want_status_repr = NULL, .str = "\"\xF1\xB0\x80\x82\""},  // U+00070002.
      {.want_status_repr = NULL, .str = "\"\xF3\xB0\x80\x83\""},  // U+000F0003.
      {.want_status_repr = NULL, .str = "\"\xF4\x80\x80\x84\""},  // U+00100004.
      {.want_status_repr = NULL, .str = "\"\xF4\x8F\xBF\xBF\""},  // U+0010FFFF.
      {.want_status_repr = NULL, .str = "\"abc\""},
      {.want_status_repr = NULL, .str = "\"i\x6Ak\""},
      {.want_status_repr = NULL, .str = "\"space+\x20+space\""},
      {.want_status_repr = NULL, .str = "\"tab+\\t+tab\""},
      {.want_status_repr = NULL, .str = "\"tab+\\u0009+tab\""},

      {.want_status_repr = bad_bac, .str = "\"\\uIJKL\""},
      {.want_status_repr = bad_bac, .str = "\"space+\\x20+space\""},

      {.want_status_repr = bad_ccc, .str = "\"\x1F\""},
      {.want_status_repr = bad_ccc, .str = "\"tab+\t+tab\""},

      {.want_status_repr = bad_utf, .str = "\"\x80\""},
      {.want_status_repr = bad_utf, .str = "\"\xBF\""},
      {.want_status_repr = bad_utf, .str = "\"\xC1\x80\""},
      {.want_status_repr = bad_utf, .str = "\"\xC2\x7F\""},
      {.want_status_repr = bad_utf, .str = "\"\xDF\xC0\""},
      {.want_status_repr = bad_utf, .str = "\"\xDF\xFF\""},
      {.want_status_repr = bad_utf, .str = "\"\xE0\x9F\xBF\""},
      {.want_status_repr = bad_utf, .str = "\"\xED\xA0\x80\""},  // U+0000D800.
      {.want_status_repr = bad_utf, .str = "\"\xED\xAF\xBF\""},  // U+0000DBFF.
      {.want_status_repr = bad_utf, .str = "\"\xED\xB0\x80\""},  // U+0000DC00.
      {.want_status_repr = bad_utf, .str = "\"\xED\xBF\xBF\""},  // U+0000DFFF.
      {.want_status_repr = bad_utf, .str = "\"\xF0\x80\x80\""},
      {.want_status_repr = bad_utf, .str = "\"\xF0\x8F\xBF\xBF\""},
      {.want_status_repr = bad_utf, .str = "\"\xF2\x7F\x80\x80\""},
      {.want_status_repr = bad_utf, .str = "\"\xF2\x80\x7F\x80\""},
      {.want_status_repr = bad_utf, .str = "\"\xF2\x80\x80\x7F\""},
      {.want_status_repr = bad_utf, .str = "\"\xF4\x90\x80\x80\""},
      {.want_status_repr = bad_utf, .str = "\"\xF5\""},
      {.want_status_repr = bad_utf, .str = "\"\xFF\xFF\xFF\xFF\""},
  };

  wuffs_json__decoder dec;
  for (size_t tc = 0; tc < WUFFS_TESTLIB_ARRAY_SIZE(test_cases); tc++) {
    CHECK_STATUS("initialize",
                 wuffs_json__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    wuffs_base__token_buffer tok =
        wuffs_base__slice_token__writer(g_have_slice_token);
    wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(
        (uint8_t*)(void*)test_cases[tc].str, strlen(test_cases[tc].str), true);
    wuffs_base__status have_status =
        wuffs_json__decoder__decode_tokens(&dec, &tok, &src, g_work_slice_u8);

    uint64_t total_length = 0;
    for (size_t i = tok.meta.ri; i < tok.meta.wi; i++) {
      wuffs_base__token* t = &tok.data.ptr[i];
      total_length =
          wuffs_base__u64__sat_add(total_length, wuffs_base__token__length(t));
    }

    if (have_status.repr != test_cases[tc].want_status_repr) {
      RETURN_FAIL("%s: have \"%s\", want \"%s\"", test_cases[tc].str,
                  have_status.repr, test_cases[tc].want_status_repr);
    }

    if (total_length != src.meta.ri) {
      RETURN_FAIL("%s: total length: have %" PRIu64 ", want %zu",
                  test_cases[tc].str, total_length, src.meta.ri);
    }
  }

  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

// No mimic tests.

#endif  // WUFFS_MIMIC

// ---------------- String Conversions Benches

const char*  //
do_bench_wuffs_strconv_parse_number_f64(const char* str,
                                        uint64_t iters_unscaled) {
  wuffs_base__slice_u8 s =
      wuffs_base__make_slice_u8((uint8_t*)(void*)str, strlen(str));

  bench_start();
  uint64_t iters = iters_unscaled * g_flags.iterscale;
  for (uint64_t i = 0; i < iters; i++) {
    CHECK_STATUS("", wuffs_base__parse_number_f64(
                         s, WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS)
                         .status);
  }
  bench_finish(iters, 0);

  return NULL;
}

const char*  //
bench_wuffs_strconv_parse_number_f64_1_lsh53_add0() {
  CHECK_FOCUS(__func__);
  // 9007_199254_740992 is 0x20_0000_0000_0000, aka ((1<<53) + 0).
  return do_bench_wuffs_strconv_parse_number_f64("9007199254740992", 1000);
}

const char*  //
bench_wuffs_strconv_parse_number_f64_1_lsh53_add1() {
  CHECK_FOCUS(__func__);
  // 9007_199254_740993 is 0x20_0000_0000_0001, aka ((1<<53) + 1).
  return do_bench_wuffs_strconv_parse_number_f64("9007199254740993", 1000);
}

const char*  //
bench_wuffs_strconv_parse_number_f64_pi_long() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_strconv_parse_number_f64(
      "3.141592653589793238462643383279", 1000);
}

const char*  //
bench_wuffs_strconv_parse_number_f64_pi_short() {
  CHECK_FOCUS(__func__);
  return do_bench_wuffs_strconv_parse_number_f64("3.14159", 1000);
}

const char*  //
do_bench_wuffs_strconv_render_number_f64(wuffs_base__slice_u64 test_cases,
                                         uint64_t iters_unscaled) {
  bench_start();
  uint64_t iters = iters_unscaled * g_flags.iterscale;
  for (uint64_t i = 0; i < iters; i++) {
    for (size_t tc = 0; tc < test_cases.len; tc++) {
      size_t n = wuffs_base__render_number_f64(
          g_have_slice_u8,
          wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
              test_cases.ptr[tc]),
          0, WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION);
      if (n == 0) {
        RETURN_FAIL("0x%016" PRIX64 ": failed", test_cases.ptr[tc]);
      }
    }
  }
  bench_finish(iters, 0);

  return NULL;
}

const char*  //
bench_wuffs_strconv_render_number_f64_just_enough_fractions() {
  CHECK_FOCUS(__func__);
  uint64_t test_cases[] = {
      0x400B000000000000,  // 3.375
      0x40753C74538EF34D,  // 339.7784
      0xCFA681AD0223D5B2,  // -5.09e+75
      0xAC5B4988314D17B8,  // -5.11e-95
      0x455987BF7CB8EC68,  // 1.2345678912345679e+26
      0xBFF8000000000000,  // -1.5
      0x405EDD2F1A9FBE77,  // 123.456
      0x502552E0653BDA6F,  // 1.23456e+78
      0x2FC24C42A75EFC06,  // 1.23456e-78
      0x00047A3A3EF0896C,  // 6.226662346353213e-309
  };
  return do_bench_wuffs_strconv_render_number_f64(
      wuffs_base__make_slice_u64(&test_cases[0],
                                 WUFFS_TESTLIB_ARRAY_SIZE(test_cases)),
      200);
}

const char*  //
bench_wuffs_strconv_render_number_f64_just_enough_small_integers() {
  CHECK_FOCUS(__func__);
  uint64_t test_cases[] = {
      0x4008000000000000,  // 3
      0x402C000000000000,  // 14
      0x4063E00000000000,  // 159
      0x40A4BA0000000000,  // 2653
      0x40ECCC6000000000,  // 58979
      0x4113C41800000000,  // 323846
      0x41442ADB80000000,  // 2643383
      0x417AA7CD00000000,  // 27950288
      0x41B9045F4B000000,  // 419716939
      0x4201766618E00000,  // 9375105820
  };
  return do_bench_wuffs_strconv_render_number_f64(
      wuffs_base__make_slice_u64(&test_cases[0],
                                 WUFFS_TESTLIB_ARRAY_SIZE(test_cases)),
      2000);
}

// ---------------- JSON Benches

const char*  //
bench_wuffs_json_decode_1k() {
  CHECK_FOCUS(__func__);
  return do_bench_token_decoder(
      wuffs_json_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_src, &g_json_github_tags_gt, UINT64_MAX, UINT64_MAX, 10000);
}

const char*  //
bench_wuffs_json_decode_21k_formatted() {
  CHECK_FOCUS(__func__);
  return do_bench_token_decoder(
      wuffs_json_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_src, &g_json_file_sizes_gt, UINT64_MAX, UINT64_MAX, 300);
}

const char*  //
bench_wuffs_json_decode_26k_compact() {
  CHECK_FOCUS(__func__);
  return do_bench_token_decoder(
      wuffs_json_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_src, &g_json_australian_abc_gt, UINT64_MAX, UINT64_MAX, 250);
}

const char*  //
bench_wuffs_json_decode_217k_stringy() {
  CHECK_FOCUS(__func__);
  return do_bench_token_decoder(
      wuffs_json_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      tcounter_src, &g_json_nobel_prizes_gt, UINT64_MAX, UINT64_MAX, 25);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

// No mimic benches.

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    // These core and strconv tests are really testing the Wuffs base library.
    // They aren't specific to the std/json code, but putting them here is as
    // good as any other place.
    test_wuffs_core_count_leading_zeroes_u64,
    test_wuffs_core_multiply_u64,
    test_wuffs_strconv_base_16,
    test_wuffs_strconv_base_64,
    test_wuffs_strconv_hpd_rounded_integer,
    test_wuffs_strconv_hpd_shift,
    test_wuffs_strconv_ieee_754_bit_representation_from_u16,
    test_wuffs_strconv_ieee_754_bit_representation_from_u32,
    test_wuffs_strconv_parse_number_f64_options,
    test_wuffs_strconv_parse_number_f64_regular,
    test_wuffs_strconv_parse_number_i64,
    test_wuffs_strconv_parse_number_u64,
    test_wuffs_strconv_render_number_f64,
    test_wuffs_strconv_render_number_i64,
    test_wuffs_strconv_render_number_u64,
    test_wuffs_strconv_utf_8_next,

    test_wuffs_json_decode_empty_input,
    test_wuffs_json_decode_end_of_data,
    test_wuffs_json_decode_interface,
    test_wuffs_json_decode_long_numbers,
    test_wuffs_json_decode_prior_valid_utf_8,
    test_wuffs_json_decode_quirk_allow_backslash_etc,
    test_wuffs_json_decode_quirk_allow_backslash_x,
    test_wuffs_json_decode_quirk_allow_comment_etc,
    test_wuffs_json_decode_quirk_allow_extra_comma,
    test_wuffs_json_decode_quirk_allow_inf_nan_numbers,
    test_wuffs_json_decode_quirk_allow_leading_etc,
    test_wuffs_json_decode_quirk_allow_trailing_comments,
    test_wuffs_json_decode_quirk_allow_trailing_filler,
    test_wuffs_json_decode_quirk_replace_invalid_unicode,
    test_wuffs_json_decode_src_io_buffer_length,
    test_wuffs_json_decode_string,
    test_wuffs_json_decode_unicode4_escapes,

#ifdef WUFFS_MIMIC

// No mimic tests.

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_strconv_parse_number_f64_1_lsh53_add0,
    bench_wuffs_strconv_parse_number_f64_1_lsh53_add1,
    bench_wuffs_strconv_parse_number_f64_pi_long,
    bench_wuffs_strconv_parse_number_f64_pi_short,
    bench_wuffs_strconv_render_number_f64_just_enough_fractions,
    bench_wuffs_strconv_render_number_f64_just_enough_small_integers,

    bench_wuffs_json_decode_1k,
    bench_wuffs_json_decode_21k_formatted,
    bench_wuffs_json_decode_26k_compact,
    bench_wuffs_json_decode_217k_stringy,

#ifdef WUFFS_MIMIC

// No mimic benches.

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/json";
  return test_main(argc, argv, g_tests, g_benches);
}
