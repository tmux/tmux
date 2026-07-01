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
  $CC -std=c99 -Wall -Werror jpeg.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -ljpeg

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
#define WUFFS_CONFIG__MODULE__JPEG

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/jpeg.c"
#endif

// ---------------- JPEG Tests

const char*  //
wuffs_jpeg_decode(uint64_t* n_bytes_out,
                  wuffs_base__io_buffer* dst,
                  uint32_t wuffs_initialize_flags,
                  wuffs_base__pixel_format pixfmt,
                  uint32_t* quirks_ptr,
                  size_t quirks_len,
                  wuffs_base__io_buffer* src) {
  wuffs_jpeg__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_jpeg__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                               wuffs_initialize_flags));
  return do_run__wuffs_base__image_decoder(
      wuffs_jpeg__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      n_bytes_out, dst, pixfmt, quirks_ptr, quirks_len, src);
}

const char*  //
test_wuffs_jpeg_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_jpeg__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_jpeg__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__image_decoder(
      wuffs_jpeg__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      "test/data/bricks-color.jpeg", 0, SIZE_MAX, 160, 120, 0xFF012466);
}

const char*  //
test_wuffs_jpeg_decode_lower_quality() {
  CHECK_FOCUS(__func__);
  wuffs_jpeg__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_jpeg__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  wuffs_jpeg__decoder__set_quirk(
      &dec, WUFFS_BASE__QUIRK_QUALITY,
      WUFFS_BASE__QUIRK_QUALITY__VALUE__LOWER_QUALITY);
  CHECK_STRING(do_test__wuffs_base__image_decoder(
      wuffs_jpeg__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      "test/data/bricks-color.jpeg", 0, SIZE_MAX, 160, 120, 0xFF012466));

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/bricks-color.jpeg"));
  CHECK_STATUS("initialize", wuffs_jpeg__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  CHECK_STATUS("decode_image_config",
               wuffs_jpeg__decoder__decode_image_config(&dec, NULL, &src));

  for (int q = 0; q < 2; q++) {
    if (q) {
      wuffs_jpeg__decoder__set_quirk(
          &dec, WUFFS_BASE__QUIRK_QUALITY,
          WUFFS_BASE__QUIRK_QUALITY__VALUE__LOWER_QUALITY);
    }
    uint64_t m = wuffs_jpeg__decoder__workbuf_len(&dec).min_incl;
    if ((q == 0) && (m == 0)) {
      RETURN_FAIL("q=%d: have %" PRIu64 ", want non-zero", q, m);
    } else if ((q != 0) && (m != 0)) {
      RETURN_FAIL("q=%d: have %" PRIu64 ", want zero", q, m);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_jpeg_decode_truncated_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_jpeg__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_jpeg__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_jpeg__decoder__decode_image_config(&dec, NULL, &src);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status = wuffs_jpeg__decoder__decode_image_config(&dec, NULL, &src);
  if (status.repr != wuffs_jpeg__error__truncated_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_jpeg__error__truncated_input);
  }
  return NULL;
}

const char*  //
do_test_wuffs_jpeg_decode_dht(wuffs_base__io_buffer* src,
                              const uint32_t arg_bits,
                              const uint32_t arg_n_bits,
                              const int32_t* want_dst_ptr,
                              const size_t want_dst_len,
                              const uint8_t (*want_symbols)[256],
                              const uint32_t (*want_slow)[16],
                              const uint16_t (*want_fast)[256]) {
  wuffs_jpeg__decoder dec;
  memset(&dec, 0xAB, sizeof dec);  // 0xAB is arbitrary, non-zero junk.
  CHECK_STATUS("initialize",
               wuffs_jpeg__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  // Decode that DHT payload.
  if (wuffs_base__io_buffer__reader_length(src) <= 0) {
    return "empty src";
  }
  uint32_t tc4_th = ((src->data.ptr[src->meta.ri] >> 2) & 0x04) |
                    ((src->data.ptr[src->meta.ri] >> 0) & 0x03);
  dec.private_impl.f_sof_marker = 0xC0;
  dec.private_impl.f_payload_length =
      ((uint32_t)(wuffs_base__io_buffer__reader_length(src)));
  CHECK_STATUS("decode_dht", wuffs_jpeg__decoder__decode_dht(&dec, src));

  if (memcmp(dec.private_impl.f_huff_tables_symbols[tc4_th], want_symbols,
             sizeof(*want_symbols))) {
    RETURN_FAIL("unexpected huff_tables_symbols");
  } else if (memcmp(dec.private_impl.f_huff_tables_slow[tc4_th], want_slow,
                    sizeof(*want_slow))) {
    RETURN_FAIL("unexpected huff_tables_slow");
  } else if (memcmp(dec.private_impl.f_huff_tables_fast[tc4_th], want_fast,
                    sizeof(*want_fast))) {
    RETURN_FAIL("unexpected huff_tables_fast");
  }

  const int32_t err_not_enough_bits = -1;
  const int32_t err_fast_not_applicable = -2;
  const int32_t err_invalid_code = -3;

  // Check decoding of (arg_bits, arg_n_bits).
  for (int use_fast = 0; use_fast < 2; use_fast++) {
    uint32_t bits = arg_bits;
    uint32_t n_bits = arg_n_bits;
    for (size_t i = 0; i < want_dst_len; i++) {
      int have = err_fast_not_applicable;

      if (use_fast) {
        uint16_t x = (*want_fast)[bits >> 24];
        uint32_t n = x >> 8;
        if (x == 0xFFFF) {
          have = err_fast_not_applicable;
        } else if (n > n_bits) {
          have = err_not_enough_bits;
        } else {
          bits <<= n;
          n_bits -= n;
          have = x & 0xFF;
        }
      }

      if (have == err_fast_not_applicable) {
        uint32_t code = 0;
        for (uint32_t j = 0; true; j++) {
          if (n_bits <= 0) {
            have = err_not_enough_bits;
            break;
          } else if (j >= 16) {
            have = err_invalid_code;
            break;
          }
          code = (code << 1) | (bits >> 31);
          bits <<= 1;
          n_bits -= 1;
          uint32_t x = (*want_slow)[j];
          if (code < (x >> 8)) {
            have = (*want_symbols)[(code + x) & 0xFF];
            break;
          }
        }
      }

      if (have != want_dst_ptr[i]) {
        RETURN_FAIL("output symbols: use_fast=%d, i=%zu: have %d, want %d",
                    use_fast, i, have, want_dst_ptr[i]);
      }
    }
  }

  return NULL;
}

const char*  //
test_wuffs_jpeg_decode_dht_easy() {
  CHECK_FOCUS(__func__);

  const uint8_t want_symbols[256] = {
      6, 7, 4, 5, 0, 3, 8, 9, 2, 1, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  //
  };

  // Set src to this fragment of "hd test/data/bricks-color.jpeg".
  //   000000b0  .. ff c4 00 1d 00 00 02  02 03 01 01 01 00 00 00
  //   000000c0  00 00 00 00 00 00 06 07  04 05 00 03 08 09 02 01
  // The "ff c4" is the DHT marker. The "00 1d" is the payload length. The
  // remaining payload has 0x001D - 2 = 0x1B = 17 + 10 bytes.
  g_src_array_u8[0x00] = 0x00;  // (tc, th) selectors are (0, 0).
  g_src_array_u8[0x01] = 0x00;  // 0 codes of bit_length 1.
  g_src_array_u8[0x02] = 0x02;  // 2 codes of bit_length 2.
  g_src_array_u8[0x03] = 0x02;  // 2 codes of bit_length 3.
  g_src_array_u8[0x04] = 0x03;  // 3 codes of bit_length 4.
  g_src_array_u8[0x05] = 0x01;  // 1 codes of bit_length 5.
  g_src_array_u8[0x06] = 0x01;  // 1 codes of bit_length 6.
  g_src_array_u8[0x07] = 0x01;  // 1 codes of bit_length 7.
  g_src_array_u8[0x08] = 0x00;  // 0 codes of bit_length 8.
  g_src_array_u8[0x09] = 0x00;  // 0 codes of bit_length 9.
  g_src_array_u8[0x0A] = 0x00;  // etc.
  g_src_array_u8[0x0B] = 0x00;
  g_src_array_u8[0x0C] = 0x00;
  g_src_array_u8[0x0D] = 0x00;
  g_src_array_u8[0x0E] = 0x00;
  g_src_array_u8[0x0F] = 0x00;
  g_src_array_u8[0x10] = 0x00;  // 0 codes of bit-length 16. 10 codes total.
  g_src_array_u8[0x11] = want_symbols[0];  // The 1st symbol is 0x06.
  g_src_array_u8[0x12] = want_symbols[1];  // The 2nd symbol is 0x07.
  g_src_array_u8[0x13] = want_symbols[2];  // The 3rd symbol is 0x04.
  g_src_array_u8[0x14] = want_symbols[3];  // etc.
  g_src_array_u8[0x15] = want_symbols[4];
  g_src_array_u8[0x16] = want_symbols[5];
  g_src_array_u8[0x17] = want_symbols[6];
  g_src_array_u8[0x18] = want_symbols[7];
  g_src_array_u8[0x19] = want_symbols[8];
  g_src_array_u8[0x1A] = want_symbols[9];
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0x1B, false);

  // The Huffman codes are:
  //   0b00......   bit_length=2   symbol=6
  //   0b01......   bit_length=2   symbol=7
  //   0b100.....   bit_length=3   symbol=4
  //   0b101.....   bit_length=3   symbol=5
  //   0b1100....   bit_length=4   symbol=0
  //   0b1101....   bit_length=4   symbol=3
  //   0b1110....   bit_length=4   symbol=8
  //   0b11110...   bit_length=5   symbol=9
  //   0b111110..   bit_length=6   symbol=2
  //   0b1111110.   bit_length=7   symbol=1
  //   0b1111111.   invalid

  // Running this on "wxyz" input should give these symbols:
  //   0x77 'w'      0x78 'x'      0x79 'y'      0x7A 'z'
  //   0b0111_0111   0b0111_1000   0b0111_1001   0b0111_1010
  //     01 1101 1101    1110 00     01 1110 01    01 1110 10
  //     s7 s3   s3      s8   s6     s7 s8   s7    s7 s8   err_not_enough_bits
  const uint32_t bits = 0x7778797A;
  const uint32_t n_bits = 32;
  const int32_t want_dst_ptr[11] = {7, 3, 3, 8, 6, 7, 8, 7, 7, 8, -1};
  const size_t want_dst_len = 11;

  const uint32_t want_slow[16] = {
      0x00000000, 0x00000200, 0x000006FE, 0x00000FF8,  //
      0x00001FE9, 0x00003FCA, 0x00007F8B, 0x00000000,  //
      0x00000000, 0x00000000, 0x00000000, 0x00000000,  //
      0x00000000, 0x00000000, 0x00000000, 0x00000000,  //
  };

  const uint16_t want_fast[256] = {
      0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206,  //
      0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206,  //
      0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206,  //
      0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206,  //
      0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206,  //
      0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206,  //
      0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206,  //
      0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206, 0x0206,  //

      0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207,  //
      0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207,  //
      0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207,  //
      0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207,  //
      0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207,  //
      0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207,  //
      0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207,  //
      0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207, 0x0207,  //

      0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304,  //
      0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304,  //
      0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304,  //
      0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304, 0x0304,  //
      0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305,  //
      0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305,  //
      0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305,  //
      0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305, 0x0305,  //

      0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,  //
      0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,  //
      0x0403, 0x0403, 0x0403, 0x0403, 0x0403, 0x0403, 0x0403, 0x0403,  //
      0x0403, 0x0403, 0x0403, 0x0403, 0x0403, 0x0403, 0x0403, 0x0403,  //
      0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408,  //
      0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408, 0x0408,  //
      0x0509, 0x0509, 0x0509, 0x0509, 0x0509, 0x0509, 0x0509, 0x0509,  //
      0x0602, 0x0602, 0x0602, 0x0602, 0x0701, 0x0701, 0xFFFF, 0xFFFF,  //
  };

  return do_test_wuffs_jpeg_decode_dht(&src, bits, n_bits, want_dst_ptr,
                                       want_dst_len, &want_symbols, &want_slow,
                                       &want_fast);
}

const char*  //
test_wuffs_jpeg_decode_dht_hard() {
  CHECK_FOCUS(__func__);

  const uint8_t want_symbols[256] = {
      0x01, 0x02, 0x03, 0x04, 0x05, 0x11, 0x00, 0x06,  //
      0x07, 0x12, 0x21, 0x31, 0x13, 0x41, 0x51, 0x08,  //
      0x14, 0x22, 0x61, 0x71, 0x32, 0x42, 0x81, 0x91,  //
      0xA1, 0xB1, 0x15, 0x23, 0x52, 0x33, 0x82, 0xC1,  //
      0xD1, 0xF0, 0x16, 0x62, 0x72, 0x92, 0xC2, 0xE1,  //
      0x17, 0x53, 0x63, 0xA2, 0xA4, 0xB2, 0xF1, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
  };

  // Set src to this fragment of "hd test/data/bricks-color.jpeg".
  //   000000d0  ff c4 00 42 10 00 02 01  03 02 04 03 05 06 03 05
  //   000000e0  06 07 00 00 00 01 02 03  04 05 11 00 06 07 12 21
  //   000000f0  31 13 41 51 08 14 22 61  71 32 42 81 91 a1 b1 15
  //   00000100  23 52 33 82 c1 d1 f0 16  62 72 92 c2 e1 17 53 63
  //   00000110  a2 a4 b2 f1 .. .. .. ..  .. .. .. .. .. .. .. ..
  // The "ff c4" is the DHT marker. The "00 42" is the payload length. The
  // remaining payload has 0x0042 - 2 = 0x40 = 17 + 47 bytes.
  g_src_array_u8[0x00] = 0x10;  // (tc, th) selectors are (1, 0).
  g_src_array_u8[0x01] = 0x00;  // 0 codes of bit_length 1.
  g_src_array_u8[0x02] = 0x02;  // 2 codes of bit_length 2.
  g_src_array_u8[0x03] = 0x01;  // 1 codes of bit_length 3.
  g_src_array_u8[0x04] = 0x03;  // 3 codes of bit_length 4.
  g_src_array_u8[0x05] = 0x02;  // 2 codes of bit_length 5.
  g_src_array_u8[0x06] = 0x04;  // 4 codes of bit_length 6.
  g_src_array_u8[0x07] = 0x03;  // 3 codes of bit_length 7.
  g_src_array_u8[0x08] = 0x05;  // 5 codes of bit_length 8.
  g_src_array_u8[0x09] = 0x06;  // 6 codes of bit_length 9.
  g_src_array_u8[0x0A] = 0x03;  // etc.
  g_src_array_u8[0x0B] = 0x05;
  g_src_array_u8[0x0C] = 0x06;
  g_src_array_u8[0x0D] = 0x07;
  g_src_array_u8[0x0E] = 0x00;
  g_src_array_u8[0x0F] = 0x00;
  g_src_array_u8[0x10] = 0x00;  // 0 codes of bit-length 16. 47 codes total.
  memcpy(&g_src_array_u8[0x11], want_symbols, 47);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0x40, false);

  // The Huffman codes are:
  //   0b00......_........   bit_length=0x02   symbol=0x01
  //   0b01......_........   bit_length=0x02   symbol=0x02
  //   0b100....._........   bit_length=0x03   symbol=0x03
  //   0b1010...._........   bit_length=0x04   symbol=0x04
  //   0b1011...._........   bit_length=0x04   symbol=0x05
  //   0b1100...._........   bit_length=0x04   symbol=0x11
  //   0b11010..._........   bit_length=0x05   symbol=0x00
  //   0b11011..._........   bit_length=0x05   symbol=0x06
  //   0b111000.._........   bit_length=0x06   symbol=0x07
  //   0b111001.._........   bit_length=0x06   symbol=0x12
  //   0b111010.._........   bit_length=0x06   symbol=0x21
  //   0b111011.._........   bit_length=0x06   symbol=0x31
  //   0b1111000._........   bit_length=0x07   symbol=0x13
  //   0b1111001._........   bit_length=0x07   symbol=0x41
  //   0b1111010._........   bit_length=0x07   symbol=0x51
  //   0b11110110_........   bit_length=0x08   symbol=0x08
  //   0b11110111_........   bit_length=0x08   symbol=0x14
  //   0b11111000_........   bit_length=0x08   symbol=0x22
  //   0b11111001_........   bit_length=0x08   symbol=0x61
  //   0b11111010_........   bit_length=0x08   symbol=0x71
  //   0b11111011_0.......   bit_length=0x09   symbol=0x32
  //   0b11111011_1.......   bit_length=0x09   symbol=0x42
  //   0b11111100_0.......   bit_length=0x09   symbol=0x81
  //   0b11111100_1.......   bit_length=0x09   symbol=0x91
  //   0b11111101_0.......   bit_length=0x09   symbol=0xA1
  //   0b11111101_1.......   bit_length=0x09   symbol=0xB1
  //   0b11111110_00......   bit_length=0x0A   symbol=0x15
  //   0b11111110_01......   bit_length=0x0A   symbol=0x23
  //   0b11111110_10......   bit_length=0x0A   symbol=0x52
  //   0b11111110_110.....   bit_length=0x0B   symbol=0x33
  //   0b11111110_111.....   bit_length=0x0B   symbol=0x82
  //   0b11111111_000.....   bit_length=0x0B   symbol=0xC1
  //   0b11111111_001.....   bit_length=0x0B   symbol=0xD1
  //   0b11111111_010.....   bit_length=0x0B   symbol=0xF0
  //   0b11111111_0110....   bit_length=0x0C   symbol=0x16
  //   0b11111111_0111....   bit_length=0x0C   symbol=0x62
  //   0b11111111_1000....   bit_length=0x0C   symbol=0x72
  //   0b11111111_1001....   bit_length=0x0C   symbol=0x92
  //   0b11111111_1010....   bit_length=0x0C   symbol=0xC2
  //   0b11111111_1011....   bit_length=0x0C   symbol=0xE1
  //   0b11111111_11000...   bit_length=0x0D   symbol=0x17
  //   0b11111111_11001...   bit_length=0x0D   symbol=0x53
  //   0b11111111_11010...   bit_length=0x0D   symbol=0x63
  //   0b11111111_11011...   bit_length=0x0D   symbol=0xA2
  //   0b11111111_11100...   bit_length=0x0D   symbol=0xA4
  //   0b11111111_11101...   bit_length=0x0D   symbol=0xB2
  //   0b11111111_11110...   bit_length=0x0D   symbol=0xF1
  //   0b11111111_11111...   invalid

  // Running this on "wx\x7Fh" input should give these symbols:
  //   0x77 'w'      0x78 'x'      0x7F '\x7F'   0x68 'h'
  //   0b0111_0111   0b0111_1000   0b0111_1111   0b0110_1000
  //     01 11011 1011    1100 00     11111110110       100 0
  //     s2 s6    s5      s11  s1     s33               s3  err_not_enough_bits
  const uint32_t bits = 0x77787F68;
  const uint32_t n_bits = 32;
  const int32_t want_dst_ptr[8] = {0x02, 0x06, 0x05, 0x11,
                                   0x01, 0x33, 0x03, -1};
  const size_t want_dst_len = 8;

  const uint32_t want_slow[16] = {
      0x00000000, 0x00000200, 0x000005FE, 0x00000DF9,  //
      0x00001CEC, 0x00003CD0, 0x00007B94, 0x0000FB19,  //
      0x0001FC1E, 0x0003FB22, 0x0007FB27, 0x000FFC2C,  //
      0x001FFF30, 0x00000000, 0x00000000, 0x00000000,  //
  };

  const uint16_t want_fast[256] = {
      0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201,  //
      0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201,  //
      0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201,  //
      0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201,  //
      0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201,  //
      0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201,  //
      0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201,  //
      0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201, 0x0201,  //

      0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,  //
      0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,  //
      0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,  //
      0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,  //
      0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,  //
      0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,  //
      0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,  //
      0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202, 0x0202,  //

      0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303,  //
      0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303,  //
      0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303,  //
      0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303, 0x0303,  //
      0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404,  //
      0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404, 0x0404,  //
      0x0405, 0x0405, 0x0405, 0x0405, 0x0405, 0x0405, 0x0405, 0x0405,  //
      0x0405, 0x0405, 0x0405, 0x0405, 0x0405, 0x0405, 0x0405, 0x0405,  //

      0x0411, 0x0411, 0x0411, 0x0411, 0x0411, 0x0411, 0x0411, 0x0411,  //
      0x0411, 0x0411, 0x0411, 0x0411, 0x0411, 0x0411, 0x0411, 0x0411,  //
      0x0500, 0x0500, 0x0500, 0x0500, 0x0500, 0x0500, 0x0500, 0x0500,  //
      0x0506, 0x0506, 0x0506, 0x0506, 0x0506, 0x0506, 0x0506, 0x0506,  //
      0x0607, 0x0607, 0x0607, 0x0607, 0x0612, 0x0612, 0x0612, 0x0612,  //
      0x0621, 0x0621, 0x0621, 0x0621, 0x0631, 0x0631, 0x0631, 0x0631,  //
      0x0713, 0x0713, 0x0741, 0x0741, 0x0751, 0x0751, 0x0808, 0x0814,  //
      0x0822, 0x0861, 0x0871, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,  //
  };

  return do_test_wuffs_jpeg_decode_dht(&src, bits, n_bits, want_dst_ptr,
                                       want_dst_len, &want_symbols, &want_slow,
                                       &want_fast);
}

const char*  //
test_wuffs_jpeg_decode_idct() {
  CHECK_FOCUS(__func__);

  // This is "test/data/bricks-color.jpeg"'s first MCU's first block, in
  // natural (not zig-zag) order.
  const uint16_t mcu_block[64] = {
      0xFFC9, 0xFFD8, 0x0014, 0xFFF7, 0x0002, 0x0000, 0x0000, 0x0000,  //
      0x006A, 0xFFE3, 0x001C, 0xFFF9, 0x0002, 0x0000, 0x0000, 0x0000,  //
      0x0015, 0x0002, 0x0002, 0xFFFE, 0x0001, 0x0000, 0x0000, 0x0001,  //
      0x000D, 0xFFEC, 0x0005, 0xFFFE, 0x0000, 0x0000, 0x0000, 0x0000,  //
      0xFFFA, 0xFFFA, 0x0002, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000,  //
      0x0001, 0xFFFD, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
      0x0000, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
      0x0001, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
  };

  // This is "test/data/bricks-color.jpeg"'s first quantization table, in
  // natural (not zig-zag) order.
  const uint16_t quant_table[64] = {
      0x03, 0x02, 0x02, 0x03, 0x04, 0x06, 0x08, 0x0A,  //
      0x02, 0x02, 0x02, 0x03, 0x04, 0x09, 0x0A, 0x09,  //
      0x02, 0x02, 0x03, 0x04, 0x06, 0x09, 0x0B, 0x09,  //
      0x02, 0x03, 0x04, 0x05, 0x08, 0x0E, 0x0D, 0x0A,  //
      0x03, 0x04, 0x06, 0x09, 0x0B, 0x11, 0x10, 0x0C,  //
      0x04, 0x06, 0x09, 0x0A, 0x0D, 0x11, 0x12, 0x0F,  //
      0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x13, 0x13, 0x10,  //
      0x0C, 0x0F, 0x0F, 0x10, 0x12, 0x10, 0x10, 0x10,  //
  };

  // This is the IDCT's expected result (including dequantization), again in
  // natural (not zig-zag) order.
  const uint8_t want_array[64] = {
      0x81, 0x7E, 0x82, 0x7E, 0x82, 0x92, 0xC5, 0xF2,  //
      0x81, 0x80, 0x84, 0x85, 0x85, 0x88, 0x9D, 0xB2,  //
      0x86, 0x81, 0x7A, 0x77, 0x72, 0x75, 0x7E, 0x8A,  //
      0x54, 0x58, 0x58, 0x5E, 0x5E, 0x6C, 0x79, 0x87,  //
      0x4D, 0x54, 0x56, 0x5B, 0x59, 0x65, 0x6E, 0x7A,  //
      0x4A, 0x4D, 0x4F, 0x53, 0x56, 0x5F, 0x67, 0x6E,  //
      0x4A, 0x4D, 0x54, 0x58, 0x5B, 0x58, 0x56, 0x54,  //
      0x4C, 0x4C, 0x52, 0x4F, 0x4D, 0x40, 0x3A, 0x35,  //
  };

  for (int f = 0; f < 2; f++) {
    wuffs_jpeg__decoder dec;
    CHECK_STATUS("initialize", wuffs_jpeg__decoder__initialize(
                                   &dec, sizeof dec, WUFFS_VERSION,
                                   WUFFS_INITIALIZE__DEFAULT_OPTIONS));

    memcpy(&dec.private_data.f_mcu_blocks[0], mcu_block, sizeof(mcu_block));

    const uint32_t q = 0;
    memcpy(&dec.private_impl.f_quant_tables[q], quant_table,
           sizeof(quant_table));

    const char* func_name = NULL;
    wuffs_base__empty_struct (*func)(
        wuffs_jpeg__decoder * self, wuffs_base__slice_u8 a_dst_buffer,
        uint64_t a_dst_stride, uint32_t a_q) = NULL;

    if (f == 0) {
      func_name = "choosy_default";
      func = &wuffs_jpeg__decoder__decode_idct__choosy_default;
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V3)
    } else if (wuffs_base__cpu_arch__have_x86_avx2()) {
      func_name = "x86_avx2";
      func = &wuffs_jpeg__decoder__decode_idct_x86_avx2;
#endif
    }

    if (!func) {
      continue;
    }
    uint8_t dst_array[64] = {0};
    (*func)(&dec, wuffs_base__make_slice_u8(&dst_array[0], 64), 8, q);

    wuffs_base__io_buffer have =
        wuffs_base__ptr_u8__reader(&dst_array[0], 64, true);
    wuffs_base__io_buffer want =
        wuffs_base__ptr_u8__reader((uint8_t*)(&want_array[0]), 64, true);

    char prefix[256];
    snprintf(prefix, 256, "f=%d (%s): ", f, func_name);
    CHECK_STRING(check_io_buffers_equal(prefix, &have, &want));
  }

  return NULL;
}

const char*  //
test_wuffs_jpeg_decode_mcu() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/bricks-color.jpeg"));

  wuffs_jpeg__decoder dec;
  CHECK_STATUS("initialize", wuffs_jpeg__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));

  // Bypass the "#missing Quantization table" check.
  dec.private_impl.f_seen_dqt[0] = true;
  dec.private_impl.f_seen_dqt[1] = true;

  // Decode the 0xC0 SOF marker, four 0xC4 DHT markers and 0xDA SOS marker. The
  // SOS marker is only partially processed, since complete SOS processing
  // would call wuffs_jpeg__decoder__decode_mcu multiple times.
  const uint32_t marker_positions[6] =  //
      {0x09E, 0x0B1, 0x0D0, 0x114, 0x133, 0x16E};
  for (int i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(marker_positions); i++) {
    uint8_t want_marker = (i <= 0) ? 0xC0 : ((i <= 4) ? 0xC4 : 0xDA);
    src.meta.ri = marker_positions[i];
    if ((src.meta.ri + 4) > src.meta.wi) {
      RETURN_FAIL("seek #%d: past EOF", i);
    } else if (src.data.ptr[src.meta.ri++] != 0xFF) {
      RETURN_FAIL("seek #%d: have 0x%02X, want 0x%02X", i,
                  src.data.ptr[src.meta.ri - 1], 0xFF);
    } else if (src.data.ptr[src.meta.ri++] != want_marker) {
      RETURN_FAIL("seek #%d: have 0x%02X, want 0x%02X", i,
                  src.data.ptr[src.meta.ri - 1], want_marker);
    }

    dec.private_impl.f_payload_length = 0;
    dec.private_impl.f_payload_length |= src.data.ptr[src.meta.ri++];
    dec.private_impl.f_payload_length <<= 8;
    dec.private_impl.f_payload_length |= src.data.ptr[src.meta.ri++];
    dec.private_impl.f_payload_length -= 2;

    if (i <= 0) {
      dec.private_impl.f_sof_marker = 0xC0;
      CHECK_STATUS("decode_sof", wuffs_jpeg__decoder__decode_sof(&dec, &src));
    } else if (i <= 4) {
      CHECK_STATUS("decode_dht", wuffs_jpeg__decoder__decode_dht(&dec, &src));
    } else {
      CHECK_STATUS("prepare_scan",
                   wuffs_jpeg__decoder__prepare_scan(&dec, &src));
      wuffs_jpeg__decoder__fill_bitstream(&dec, &src);
    }
  }

  // 4:2:0 chroma subsampling means that the MCU has 4 Y, 1 Cb and 1 Cr blocks.
  uint16_t wants[6][64] = {
      {
          0xFFC9, 0xFFD8, 0x0014, 0xFFF7, 0x0002, 0x0000, 0x0000, 0x0000,  //
          0x006A, 0xFFE3, 0x001C, 0xFFF9, 0x0002, 0x0000, 0x0000, 0x0000,  //
          0x0015, 0x0002, 0x0002, 0xFFFE, 0x0001, 0x0000, 0x0000, 0x0001,  //
          0x000D, 0xFFEC, 0x0005, 0xFFFE, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0xFFFA, 0xFFFA, 0x0002, 0xFFFF, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0001, 0xFFFD, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0001, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
      },
      {
          0xFFAA, 0x0070, 0x003A, 0xFFE0, 0xFFF9, 0x0004, 0x0000, 0x0001,  //
          0x004F, 0x005E, 0x0022, 0x0006, 0xFFF3, 0xFFFD, 0x0004, 0x0000,  //
          0xFFF4, 0xFFE8, 0x0002, 0x0012, 0x0003, 0x0000, 0xFFFF, 0x0002,  //
          0xFFEC, 0xFFF3, 0x000A, 0x000A, 0x0007, 0x0000, 0xFFFF, 0xFFFF,  //
          0xFFF3, 0xFFFB, 0xFFFD, 0x0002, 0x0002, 0x0000, 0x0001, 0x0000,  //
          0xFFFC, 0xFFFB, 0xFFFD, 0xFFFF, 0x0000, 0x0002, 0x0001, 0x0001,  //
          0x0000, 0xFFFF, 0xFFFF, 0xFFFF, 0x0000, 0x0001, 0x0001, 0x0001,  //
          0x0000, 0x0000, 0xFFFF, 0xFFFF, 0x0000, 0x0000, 0x0001, 0x0000,  //
      },
      {
          0xFF25, 0x000D, 0x0003, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x000E, 0x0006, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0005, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0004, 0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0002, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0001, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
      },
      {
          0xFF59, 0xFFD6, 0xFFCF, 0x000C, 0x0006, 0xFFFE, 0xFFFE, 0x0002,  //
          0xFFF3, 0x0013, 0x000C, 0xFFE6, 0x000E, 0x0001, 0xFFFC, 0x0004,  //
          0x0010, 0xFFF1, 0x0005, 0x0003, 0xFFFA, 0x0002, 0x0001, 0xFFFE,  //
          0xFFF9, 0x0005, 0x0000, 0xFFFE, 0x0002, 0x0000, 0xFFFE, 0x0002,  //
          0x0003, 0xFFFF, 0x0000, 0x0001, 0xFFFF, 0x0000, 0x0001, 0xFFFF,  //
          0xFFFE, 0x0001, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
      },
      {
          0x0077, 0x000F, 0xFFFA, 0x0001, 0x0000, 0xFFFF, 0x0002, 0xFFFF,  //
          0xFFFA, 0x0020, 0xFFF9, 0x0000, 0x0000, 0xFFFF, 0x0001, 0xFFFF,  //
          0xFFFA, 0x0008, 0x0000, 0x0000, 0xFFFF, 0x0001, 0x0000, 0x0000,  //
          0xFFFE, 0x0001, 0x0001, 0x0001, 0xFFFF, 0x0001, 0xFFFF, 0x0000,  //
          0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
      },
      {
          0xFF88, 0x0003, 0xFFFD, 0x0000, 0x0000, 0x0000, 0xFFFF, 0x0001,  //
          0xFFE4, 0xFFEC, 0x0001, 0x0001, 0x0000, 0x0001, 0xFFFF, 0x0001,  //
          0xFFFC, 0xFFFC, 0xFFFE, 0x0000, 0x0001, 0xFFFF, 0x0000, 0x0000,  //
          0x0001, 0xFFFF, 0xFFFF, 0x0000, 0x0001, 0xFFFF, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0xFFFE, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
          0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,  //
      },
  };

  // Decode and compare-to-golden the first MCU (Minimum Coded Unit).
  dec.private_impl.f_test_only_interrupt_decode_mcu = true;
  for (int b = 0; b < 6; b++) {
    const uint32_t mx = 0;
    const uint32_t my = 0;
    wuffs_base__pixel_buffer dst = {0};
    if (wuffs_jpeg__decoder__decode_mcu(&dec, &dst,
                                        wuffs_base__empty_slice_u8(), mx, my)) {
      RETURN_FAIL("decode_mcu failed");
    }

    wuffs_base__io_buffer have = wuffs_base__ptr_u8__reader(   //
        (uint8_t*)(void*)(&dec.private_data.f_mcu_blocks[0]),  //
        sizeof(dec.private_data.f_mcu_blocks[0]), true);
    wuffs_base__io_buffer want = wuffs_base__ptr_u8__reader(  //
        (uint8_t*)(void*)(&wants[b]),                         //
        sizeof(wants[b]), true);

    char prefix[64];
    snprintf(prefix, 64, "b=%d: ", b);
    CHECK_STRING(check_io_buffers_equal(prefix, &have, &want));
  }

  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
do_test_mimic_jpeg_decode(const char* filename) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, filename));

  src.meta.ri = 0;
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  CHECK_STRING(wuffs_jpeg_decode(
      NULL, &have, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, &src));

  src.meta.ri = 0;
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  CHECK_STRING(mimic_jpeg_decode(
      NULL, &want, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, &src));

#ifdef WUFFS_MIMICLIB_JPEG_DOES_NOT_EXACTLY_MATCH_LIBJPEG
  if (have.meta.wi != want.meta.wi) {
    RETURN_FAIL("decoded image size (in bytes): have %zu, want %zu.\n",
                have.meta.wi, want.meta.wi);
  }
  return NULL;
#else
  return check_io_buffers_equal("", &have, &want);
#endif  // WUFFS_MIMICLIB_JPEG_DOES_NOT_EXACTLY_MATCH_LIBJPEG
}

const char*  //
test_mimic_jpeg_decode_19k_8bpp() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_jpeg_decode("test/data/bricks-gray.jpeg");
}

const char*  //
test_mimic_jpeg_decode_30k_24bpp_progressive() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_jpeg_decode("test/data/peacock.progressive.jpeg");
}

const char*  //
test_mimic_jpeg_decode_30k_24bpp_sequential() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_jpeg_decode("test/data/peacock.default.jpeg");
}

const char*  //
test_mimic_jpeg_decode_552k_24bpp_420() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_jpeg_decode("test/data/hibiscus.regular.jpeg");
}

const char*  //
test_mimic_jpeg_decode_552k_24bpp_444() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_jpeg_decode("test/data/hibiscus.primitive.jpeg");
}

#endif  // WUFFS_MIMIC

// ---------------- JPEG Benches

const char*  //
bench_wuffs_jpeg_decode_19k_8bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__Y), NULL, 0,
      "test/data/bricks-gray.jpeg", 0, SIZE_MAX, 100);
}

const char*  //
bench_wuffs_jpeg_decode_30k_24bpp_progressive() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/peacock.progressive.jpeg", 0, SIZE_MAX, 50);
}

const char*  //
bench_wuffs_jpeg_decode_30k_24bpp_sequential() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/peacock.default.jpeg", 0, SIZE_MAX, 50);
}

const char*  //
bench_wuffs_jpeg_decode_77k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/bricks-color.jpeg", 0, SIZE_MAX, 30);
}

const char*  //
bench_wuffs_jpeg_decode_552k_24bpp_420() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hibiscus.regular.jpeg", 0, SIZE_MAX, 5);
}

const char*  //
bench_wuffs_jpeg_decode_552k_24bpp_444() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hibiscus.primitive.jpeg", 0, SIZE_MAX, 5);
}

const char*  //
bench_wuffs_jpeg_decode_4002k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &wuffs_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/harvesters.jpeg", 0, SIZE_MAX, 1);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_jpeg_decode_19k_8bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__Y), NULL, 0,
      "test/data/bricks-gray.jpeg", 0, SIZE_MAX, 100);
}

const char*  //
bench_mimic_jpeg_decode_30k_24bpp_progressive() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/peacock.progressive.jpeg", 0, SIZE_MAX, 50);
}

const char*  //
bench_mimic_jpeg_decode_30k_24bpp_sequential() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/peacock.default.jpeg", 0, SIZE_MAX, 50);
}

const char*  //
bench_mimic_jpeg_decode_77k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/bricks-color.jpeg", 0, SIZE_MAX, 30);
}

const char*  //
bench_mimic_jpeg_decode_552k_24bpp_420() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hibiscus.regular.jpeg", 0, SIZE_MAX, 5);
}

const char*  //
bench_mimic_jpeg_decode_552k_24bpp_444() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hibiscus.primitive.jpeg", 0, SIZE_MAX, 5);
}

const char*  //
bench_mimic_jpeg_decode_4002k_24bpp() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      &mimic_jpeg_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/harvesters.jpeg", 0, SIZE_MAX, 1);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_jpeg_decode_dht_easy,
    test_wuffs_jpeg_decode_dht_hard,
    test_wuffs_jpeg_decode_idct,
    test_wuffs_jpeg_decode_mcu,
    test_wuffs_jpeg_decode_interface,
    test_wuffs_jpeg_decode_lower_quality,
    test_wuffs_jpeg_decode_truncated_input,

#ifdef WUFFS_MIMIC

    test_mimic_jpeg_decode_19k_8bpp,
    test_mimic_jpeg_decode_30k_24bpp_progressive,
    test_mimic_jpeg_decode_30k_24bpp_sequential,
    test_mimic_jpeg_decode_552k_24bpp_420,
    test_mimic_jpeg_decode_552k_24bpp_444,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_jpeg_decode_19k_8bpp,
    bench_wuffs_jpeg_decode_30k_24bpp_progressive,
    bench_wuffs_jpeg_decode_30k_24bpp_sequential,
    bench_wuffs_jpeg_decode_77k_24bpp,
    bench_wuffs_jpeg_decode_552k_24bpp_420,
    bench_wuffs_jpeg_decode_552k_24bpp_444,
    bench_wuffs_jpeg_decode_4002k_24bpp,

#ifdef WUFFS_MIMIC

    bench_mimic_jpeg_decode_19k_8bpp,
    bench_mimic_jpeg_decode_30k_24bpp_progressive,
    bench_mimic_jpeg_decode_30k_24bpp_sequential,
    bench_mimic_jpeg_decode_77k_24bpp,
    bench_mimic_jpeg_decode_552k_24bpp_420,
    bench_mimic_jpeg_decode_552k_24bpp_444,
    bench_mimic_jpeg_decode_4002k_24bpp,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/jpeg";
  return test_main(argc, argv, g_tests, g_benches);
}
