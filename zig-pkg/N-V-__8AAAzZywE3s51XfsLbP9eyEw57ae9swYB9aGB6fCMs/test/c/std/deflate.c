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
  $CC -std=c99 -Wall -Werror deflate.c && ./a.out
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
#define WUFFS_CONFIG__MODULE__DEFLATE

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/deflate-gzip-zlib.c"
#endif

// ---------------- Golden Tests

// The src_offset0 and src_offset1 magic numbers come from:
//
// go run script/extract-flate-offsets.go test/data/*.gz

golden_test g_deflate_256_bytes_gt = {
    .want_filename = "test/data/256.bytes",
    .src_filename = "test/data/256.bytes.gz",
    .src_offset0 = 20,
    .src_offset1 = 281,
};

golden_test g_deflate_deflate_backref_crosses_blocks_gt = {
    .want_filename =
        "test/data/artificial-deflate/"
        "backref-crosses-blocks.deflate.decompressed",
    .src_filename =
        "test/data/artificial-deflate/"
        "backref-crosses-blocks.deflate",
};

golden_test g_deflate_deflate_degenerate_huffman_gt = {
    .want_filename =
        "test/data/artificial-deflate/"
        "degenerate-huffman.deflate.decompressed",
    .src_filename =
        "test/data/artificial-deflate/"
        "degenerate-huffman.deflate",
};

golden_test g_deflate_deflate_distance_32768_gt = {
    .want_filename =
        "test/data/artificial-deflate/"
        "distance-32768.deflate.decompressed",
    .src_filename =
        "test/data/artificial-deflate/"
        "distance-32768.deflate",
};

golden_test g_deflate_deflate_distance_code_31_gt = {
    .want_filename =
        "test/data/artificial-deflate/"
        "qdistance-code-31.deflate.decompressed",
    .src_filename =
        "test/data/artificial-deflate/"
        "distance-code-31.deflate",
};

golden_test g_deflate_deflate_huffman_primlen_9_gt = {
    .want_filename =
        "test/data/artificial-deflate/"
        "huffman-primlen-9.deflate.decompressed",
    .src_filename =
        "test/data/artificial-deflate/"
        "huffman-primlen-9.deflate",
};

golden_test g_deflate_midsummer_gt = {
    .want_filename = "test/data/midsummer.txt",
    .src_filename = "test/data/midsummer.txt.gz",
    .src_offset0 = 24,
    .src_offset1 = 5166,
};

golden_test g_deflate_pi_gt = {
    .want_filename = "test/data/pi.txt",
    .src_filename = "test/data/pi.txt.gz",
    .src_offset0 = 17,
    .src_offset1 = 48335,
};

golden_test g_deflate_romeo_gt = {
    .want_filename = "test/data/romeo.txt",
    .src_filename = "test/data/romeo.txt.gz",
    .src_offset0 = 20,
    .src_offset1 = 550,
};

golden_test g_deflate_romeo_fixed_gt = {
    .want_filename = "test/data/romeo.txt",
    .src_filename = "test/data/romeo.txt.fixed-huff.deflate",
};

// ---------------- Deflate Tests

const char*  //
test_wuffs_deflate_decode_interface() {
  CHECK_FOCUS(__func__);
  wuffs_deflate__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_deflate__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__io_transformer(
      wuffs_deflate__decoder__upcast_as__wuffs_base__io_transformer(&dec),
      "test/data/romeo.txt.deflate", 0, SIZE_MAX, 942, 0x0A);
}

const char*  //
test_wuffs_deflate_decode_truncated_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer have = wuffs_base__ptr_u8__writer(g_have_array_u8, 1);
  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_deflate__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_deflate__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_deflate__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status =
      wuffs_deflate__decoder__transform_io(&dec, &have, &src, g_work_slice_u8);
  if (status.repr != wuffs_deflate__error__truncated_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_deflate__error__truncated_input);
  }
  return NULL;
}

const char*  //
wuffs_deflate_decode(wuffs_base__io_buffer* dst,
                     wuffs_base__io_buffer* src,
                     uint32_t wuffs_initialize_flags,
                     uint64_t wlimit,
                     uint64_t rlimit) {
  wuffs_deflate__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_deflate__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION, wuffs_initialize_flags));

  while (true) {
    wuffs_base__io_buffer limited_dst = make_limited_writer(*dst, wlimit);
    wuffs_base__io_buffer limited_src = make_limited_reader(*src, rlimit);

    wuffs_base__status status = wuffs_deflate__decoder__transform_io(
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
test_wuffs_deflate_decode_256_bytes() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode, &g_deflate_256_bytes_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_deflate_decode_deflate_backref_crosses_blocks() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode,
                            &g_deflate_deflate_backref_crosses_blocks_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_deflate_decode_deflate_degenerate_huffman() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode,
                            &g_deflate_deflate_degenerate_huffman_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_deflate_decode_deflate_distance_32768() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode,
                            &g_deflate_deflate_distance_32768_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_wuffs_deflate_decode_deflate_distance_code_31() {
  CHECK_FOCUS(__func__);
  const char* have = do_test_io_buffers(wuffs_deflate_decode,
                                        &g_deflate_deflate_distance_code_31_gt,
                                        UINT64_MAX, UINT64_MAX);
  if (have != wuffs_deflate__error__bad_huffman_code) {
    RETURN_FAIL("have \"%s\", want \"%s\"", have,
                wuffs_deflate__error__bad_huffman_code);
  }
  return NULL;
}

const char*  //
test_wuffs_deflate_decode_deflate_huffman_primlen_9() {
  CHECK_FOCUS(__func__);

  // First, treat this like any other compare-to-golden test.
  CHECK_STRING(do_test_io_buffers(wuffs_deflate_decode,
                                  &g_deflate_deflate_huffman_primlen_9_gt,
                                  UINT64_MAX, UINT64_MAX));

  // Second, check that the decoder's huffman table sizes match those predicted
  // by the script/print-deflate-huff-table-size.go program.
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });

  golden_test* gt = &g_deflate_deflate_huffman_primlen_9_gt;
  CHECK_STRING(read_file(&src, gt->src_filename));

  wuffs_deflate__decoder dec;
  CHECK_STATUS("initialize", wuffs_deflate__decoder__initialize(
                                 &dec, sizeof dec, WUFFS_VERSION,
                                 WUFFS_INITIALIZE__DEFAULT_OPTIONS));
  CHECK_STATUS("transform_io", wuffs_deflate__decoder__transform_io(
                                   &dec, &have, &src, g_work_slice_u8));

  for (int i = 0; i < 2; i++) {
    // Find the first unused (i.e. zero) entry in the i'th huffs table.
    int have = WUFFS_DEFLATE__HUFFS_TABLE_SIZE;
    while ((have > 0) && (dec.private_data.f_huffs[i][have - 1] == 0)) {
      have--;
    }

    // See script/print-deflate-huff-table-size.go with primLen = 9 for how
    // these expected values are derived.
    int want = (i == 0) ? 852 : 592;
    if (have != want) {
      RETURN_FAIL("i=%d: have %d, want %d", i, have, want);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_deflate_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode, &g_deflate_midsummer_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_deflate_decode_pi_just_one_read() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode, &g_deflate_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_wuffs_deflate_decode_pi_many_big_reads() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode, &g_deflate_pi_gt, UINT64_MAX,
                            4096);
}

const char*  //
test_wuffs_deflate_decode_pi_many_medium_reads() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode, &g_deflate_pi_gt, UINT64_MAX,
                            599);
}

const char*  //
test_wuffs_deflate_decode_pi_many_small_writes_reads() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode, &g_deflate_pi_gt, 59, 61);
}

const char*  //
test_wuffs_deflate_decode_romeo() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode, &g_deflate_romeo_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_deflate_decode_romeo_fixed() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(wuffs_deflate_decode, &g_deflate_romeo_fixed_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_wuffs_deflate_decode_split_src() {
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

  golden_test* gt = &g_deflate_256_bytes_gt;
  CHECK_STRING(read_file(&src, gt->src_filename));
  CHECK_STRING(read_file(&want, gt->want_filename));

  for (int i = 1; i < 32; i++) {
    size_t split = gt->src_offset0 + i;
    if (split >= gt->src_offset1) {
      RETURN_FAIL("i=%d: split was not an interior split", i);
    }
    have.meta.wi = 0;

    wuffs_deflate__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_deflate__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    src.meta.closed = false;
    src.meta.ri = gt->src_offset0;
    src.meta.wi = split;
    wuffs_base__status z0 = wuffs_deflate__decoder__transform_io(
        &dec, &have, &src, g_work_slice_u8);

    src.meta.closed = true;
    src.meta.ri = split;
    src.meta.wi = gt->src_offset1;
    wuffs_base__status z1 = wuffs_deflate__decoder__transform_io(
        &dec, &have, &src, g_work_slice_u8);

    if (z0.repr != wuffs_base__suspension__short_read) {
      RETURN_FAIL("i=%d: z0: have \"%s\", want \"%s\"", i, z0.repr,
                  wuffs_base__suspension__short_read);
    }

    if (z1.repr) {
      RETURN_FAIL("i=%d: z1: have \"%s\"", i, z1.repr);
    }

    char prefix[64];
    snprintf(prefix, 64, "i=%d: ", i);
    CHECK_STRING(check_io_buffers_equal(prefix, &have, &want));
  }
  return NULL;
}

const char*  //
do_test_wuffs_deflate_history(int i,
                              golden_test* gt,
                              wuffs_base__io_buffer* src,
                              wuffs_base__io_buffer* have,
                              wuffs_deflate__decoder* dec,
                              uint32_t starting_history_index,
                              uint64_t wlimit,
                              const char* want_z) {
  src->meta.ri = gt->src_offset0;
  src->meta.wi = gt->src_offset1;
  have->meta.ri = 0;
  have->meta.wi = 0;

  wuffs_base__io_buffer limited_have = make_limited_writer(*have, wlimit);

  dec->private_impl.f_history_index = starting_history_index;

  wuffs_base__status have_z = wuffs_deflate__decoder__transform_io(
      dec, &limited_have, src, g_work_slice_u8);
  have->meta.wi += limited_have.meta.wi;
  if (have_z.repr != want_z) {
    RETURN_FAIL("i=%d: starting_history_index=0x%04" PRIX32
                ": decode: have \"%s\", want \"%s\"",
                i, starting_history_index, have_z.repr, want_z);
  }

  // Check that head and the tail of the ringbuffer match.
  if (wuffs_base__status__is_suspension(&have_z)) {
    const size_t max_length_minus_1 = 257;

    wuffs_base__io_buffer head = ((wuffs_base__io_buffer){
        .data = ((wuffs_base__slice_u8){
            .ptr = dec->private_data.f_history + 0,
            .len = max_length_minus_1,
        }),
    });
    head.meta.wi = max_length_minus_1;

    wuffs_base__io_buffer tail = ((wuffs_base__io_buffer){
        .data = ((wuffs_base__slice_u8){
            .ptr = dec->private_data.f_history + 0x8000,
            .len = max_length_minus_1,
        }),
    });
    tail.meta.wi = max_length_minus_1;

    CHECK_STRING(check_io_buffers_equal("head vs tail ", &head, &tail));
  }

  return NULL;
}

const char*  //
test_wuffs_deflate_history_full() {
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

  golden_test* gt = &g_deflate_pi_gt;
  CHECK_STRING(read_file(&src, gt->src_filename));
  CHECK_STRING(read_file(&want, gt->want_filename));

  const int full_history_size = 0x8000;
  for (int i = -2; i <= +2; i++) {
    wuffs_deflate__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_deflate__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    CHECK_STRING(do_test_wuffs_deflate_history(
        i, gt, &src, &have, &dec, 0, want.meta.wi + i,
        i >= 0 ? NULL : wuffs_base__suspension__short_write));

    uint32_t want_history_index = i >= 0 ? 0 : full_history_size;
    if (dec.private_impl.f_history_index != want_history_index) {
      RETURN_FAIL("i=%d: history_index: have %" PRIu32 ", want %" PRIu32, i,
                  dec.private_impl.f_history_index, want_history_index);
    }
    if (i >= 0) {
      continue;
    }

    wuffs_base__io_buffer history_have = ((wuffs_base__io_buffer){
        .data = ((wuffs_base__slice_u8){
            .ptr = dec.private_data.f_history,
            .len = full_history_size,
        }),
    });
    history_have.meta.wi = full_history_size;
    if (want.meta.wi < full_history_size - i) {
      RETURN_FAIL("i=%d: want file is too short", i);
    }
    wuffs_base__io_buffer history_want = ((wuffs_base__io_buffer){
        .data = ((wuffs_base__slice_u8){
            .ptr = g_want_array_u8 + want.meta.wi - (full_history_size - i),
            .len = full_history_size,
        }),
    });
    history_want.meta.wi = full_history_size;

    CHECK_STRING(check_io_buffers_equal("", &history_have, &history_want));
  }
  return NULL;
}

const char*  //
test_wuffs_deflate_history_partial() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });

  golden_test* gt = &g_deflate_pi_gt;
  CHECK_STRING(read_file(&src, gt->src_filename));

  uint32_t starting_history_indexes[] = {
      0x0000, 0x0001, 0x1234, 0x7FFB, 0x7FFC, 0x7FFD, 0x7FFE, 0x7FFF,
      0x8000, 0x8001, 0x9234, 0xFFFB, 0xFFFC, 0xFFFD, 0xFFFE, 0xFFFF,
  };

  for (int i = 0; i < WUFFS_TESTLIB_ARRAY_SIZE(starting_history_indexes); i++) {
    uint32_t starting_history_index = starting_history_indexes[i];

    // The flate_pi_gt golden test file decodes to the digits of pi.
    const char* fragment = "3.14";
    const uint32_t fragment_length = 4;

    wuffs_deflate__decoder dec;
    memset(&(dec.private_data.f_history), 0,
           sizeof(dec.private_data.f_history));
    CHECK_STATUS("initialize",
                 wuffs_deflate__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    CHECK_STRING(do_test_wuffs_deflate_history(
        i, gt, &src, &have, &dec, starting_history_index, fragment_length,
        wuffs_base__suspension__short_write));

    bool have_full = dec.private_impl.f_history_index >= 0x8000;
    uint32_t have_history_index = dec.private_impl.f_history_index & 0x7FFF;
    bool want_full = (starting_history_index + fragment_length) >= 0x8000;
    uint32_t want_history_index =
        (starting_history_index + fragment_length) & 0x7FFF;
    if ((have_full != want_full) ||
        (have_history_index != want_history_index)) {
      RETURN_FAIL("i=%d: starting_history_index=0x%04" PRIX32
                  ": history_index: have %d;%04" PRIX32 ", want %d;%04" PRIX32,
                  i, starting_history_index, (int)(have_full),
                  have_history_index, (int)(want_full), want_history_index);
    }

    for (int j = -2; j < (int)(fragment_length) + 2; j++) {
      uint32_t index = (starting_history_index + j) & 0x7FFF;
      uint8_t have = dec.private_data.f_history[index];
      uint8_t want = (0 <= j && j < fragment_length) ? fragment[j] : 0;
      if (have != want) {
        RETURN_FAIL("i=%d: starting_history_index=0x%04" PRIX32
                    ": j=%d: have 0x%02" PRIX8 ", want 0x%02" PRIX8,
                    i, starting_history_index, j, have, want);
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_deflate_table_redirect() {
  CHECK_FOCUS(__func__);

  // Call init_huff with a Huffman code that looks like:
  //
  //           code_bits  cl   c   r   s          1st  2nd
  //  0b_______________0   1   1   1   0  0b........0
  //  0b______________10   2   1   1   1  0b.......01
  //  0b_____________110   3   1   1   2  0b......011
  //  0b____________1110   4   1   1   3  0b.....0111
  //  0b__________1_1110   5   1   1   4  0b....01111
  //  0b_________11_1110   6   1   1   5  0b...011111
  //  0b________111_1110   7   1   1   6  0b..0111111
  //                       8   0   2
  //  0b_____1_1111_1100   9   1   3   7  0b001111111
  //  0b____11_1111_1010  10   1   5   8  0b101111111  0b..0   (3 bits)
  //                      11   0  10
  //  0b__1111_1110_1100  12  19  19   9  0b101111111  0b001
  //  0b__1111_1110_1101  12      18  10  0b101111111  0b101
  //  0b__1111_1110_1110  12      17  11  0b101111111  0b011
  //  0b__1111_1110_1111  12      16  12  0b101111111  0b111
  //  0b__1111_1111_0000  12      15  13  0b011111111  0b000   (3 bits)
  //  0b__1111_1111_0001  12      14  14  0b011111111  0b100
  //  0b__1111_1111_0010  12      13  15  0b011111111  0b010
  //  0b__1111_1111_0011  12      12  16  0b011111111  0b110
  //  0b__1111_1111_0100  12      11  17  0b011111111  0b001
  //  0b__1111_1111_0101  12      10  18  0b011111111  0b101
  //  0b__1111_1111_0110  12       9  19  0b011111111  0b011
  //  0b__1111_1111_0111  12       8  20  0b011111111  0b111
  //  0b__1111_1111_1000  12       7  21  0b111111111  0b.000  (4 bits)
  //  0b__1111_1111_1001  12       6  22  0b111111111  0b.100
  //  0b__1111_1111_1010  12       5  23  0b111111111  0b.010
  //  0b__1111_1111_1011  12       4  24  0b111111111  0b.110
  //  0b__1111_1111_1100  12       3  25  0b111111111  0b.001
  //  0b__1111_1111_1101  12       2  26  0b111111111  0b.101
  //  0b__1111_1111_1110  12       1  27  0b111111111  0b.011
  //  0b1_1111_1111_1110  13   2   1  28  0b111111111  0b0111
  //  0b1_1111_1111_1111  13       0  29  0b111111111  0b1111
  //
  // cl  is the code_length.
  // c   is counts[code_length]
  // r   is the number of codes (of that code_length) remaining.
  // s   is the symbol
  // 1st is the key in the first level table (9 bits).
  // 2nd is the key in the second level table (variable bits).

  wuffs_deflate__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_deflate__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  memset(&(dec.private_data.f_huffs), 0, sizeof(dec.private_data.f_huffs));

  int n = 0;
  dec.private_data.f_code_lengths[n++] = 1;
  dec.private_data.f_code_lengths[n++] = 2;
  dec.private_data.f_code_lengths[n++] = 3;
  dec.private_data.f_code_lengths[n++] = 4;
  dec.private_data.f_code_lengths[n++] = 5;
  dec.private_data.f_code_lengths[n++] = 6;
  dec.private_data.f_code_lengths[n++] = 7;
  dec.private_data.f_code_lengths[n++] = 9;
  dec.private_data.f_code_lengths[n++] = 10;
  for (int i = 0; i < 19; i++) {
    dec.private_data.f_code_lengths[n++] = 12;
  }
  dec.private_data.f_code_lengths[n++] = 13;
  dec.private_data.f_code_lengths[n++] = 13;

  CHECK_STATUS("init_huff",
               wuffs_deflate__decoder__init_huff(&dec, 0, 0, n, 257));

  // There is one 1st-level table (9 bits), and three 2nd-level tables (3, 3
  // and 4 bits). f_huffs[0]'s elements should be non-zero for those tables and
  // should be zero outside of those tables.
  const int n_f_huffs = sizeof(dec.private_data.f_huffs[0]) /
                        sizeof(dec.private_data.f_huffs[0][0]);
  for (int i = 0; i < n_f_huffs; i++) {
    bool have = dec.private_data.f_huffs[0][i] == 0;
    bool want = i >= (1 << 9) + (1 << 3) + (1 << 3) + (1 << 4);
    if (have != want) {
      RETURN_FAIL("huffs[0][%d] == 0: have %d, want %d", i, have, want);
    }
  }

  // The redirects in the 1st-level table should be at:
  //  - 0b101111111 (0x017F) to the table offset 512 (0x0200), a 3-bit table.
  //  - 0b011111111 (0x00FF) to the table offset 520 (0x0208), a 3-bit table.
  //  - 0b111111111 (0x01FF) to the table offset 528 (0x0210), a 4-bit table.
  uint32_t have;
  uint32_t want;
  have = dec.private_data.f_huffs[0][0x017F];
  want = 0x10020039;
  if (have != want) {
    RETURN_FAIL("huffs[0][0x017F]: have 0x%08" PRIX32 ", want 0x%08" PRIX32,
                have, want);
  }
  have = dec.private_data.f_huffs[0][0x00FF];
  want = 0x10020839;
  if (have != want) {
    RETURN_FAIL("huffs[0][0x00FF]: have 0x%08" PRIX32 ", want 0x%08" PRIX32,
                have, want);
  }
  have = dec.private_data.f_huffs[0][0x01FF];
  want = 0x10021049;
  if (have != want) {
    RETURN_FAIL("huffs[0][0x01FF]: have 0x%08" PRIX32 ", want 0x%08" PRIX32,
                have, want);
  }

  // The first 2nd-level table should look like wants.
  const uint32_t wants[8] = {
      0x80000801, 0x80000903, 0x80000801, 0x80000B03,
      0x80000801, 0x80000A03, 0x80000801, 0x80000C03,
  };
  for (int i = 0; i < 8; i++) {
    have = dec.private_data.f_huffs[0][0x0200 + i];
    want = wants[i];
    if (have != want) {
      RETURN_FAIL("huffs[0][0x%04" PRIX32 "]: have 0x%08" PRIX32
                  ", want 0x%08" PRIX32,
                  (uint32_t)(0x0200 + i), have, want);
    }
  }
  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
test_mimic_deflate_decode_256_bytes() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode, &g_deflate_256_bytes_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_deflate_decode_deflate_backref_crosses_blocks() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode,
                            &g_deflate_deflate_backref_crosses_blocks_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_deflate_decode_deflate_degenerate_huffman() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode,
                            &g_deflate_deflate_degenerate_huffman_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_deflate_decode_deflate_distance_32768() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode,
                            &g_deflate_deflate_distance_32768_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_mimic_deflate_decode_deflate_distance_code_31() {
  CHECK_FOCUS(__func__);
  const char* have = do_test_io_buffers(mimic_deflate_decode,
                                        &g_deflate_deflate_distance_code_31_gt,
                                        UINT64_MAX, UINT64_MAX);
  if (!strings_are_equal(have, "inflate failed (data error)") &&
      !strings_are_equal(have, "libdeflate: bad data")) {
    RETURN_FAIL("have \"%s\", want \"bad data\" or similar", have);
  }
  return NULL;
}

const char*  //
test_mimic_deflate_decode_deflate_huffman_primlen_9() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode,
                            &g_deflate_deflate_huffman_primlen_9_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_mimic_deflate_decode_midsummer() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode, &g_deflate_midsummer_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_deflate_decode_pi_just_one_read() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode, &g_deflate_pi_gt, UINT64_MAX,
                            UINT64_MAX);
}

const char*  //
test_mimic_deflate_decode_pi_many_big_reads() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode, &g_deflate_pi_gt, UINT64_MAX,
                            4096);
}

const char*  //
test_mimic_deflate_decode_romeo() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode, &g_deflate_romeo_gt,
                            UINT64_MAX, UINT64_MAX);
}

const char*  //
test_mimic_deflate_decode_romeo_fixed() {
  CHECK_FOCUS(__func__);
  return do_test_io_buffers(mimic_deflate_decode, &g_deflate_romeo_fixed_gt,
                            UINT64_MAX, UINT64_MAX);
}

#endif  // WUFFS_MIMIC

// ---------------- Deflate Benches

const char*  //
bench_wuffs_deflate_decode_1k_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(wuffs_deflate_decode,
                             WUFFS_INITIALIZE__DEFAULT_OPTIONS, tcounter_dst,
                             &g_deflate_romeo_gt, UINT64_MAX, UINT64_MAX, 2000);
}

const char*  //
bench_wuffs_deflate_decode_1k_part_init() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_deflate_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_deflate_romeo_gt, UINT64_MAX, UINT64_MAX, 2000);
}

const char*  //
bench_wuffs_deflate_decode_10k_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_deflate_decode, WUFFS_INITIALIZE__DEFAULT_OPTIONS, tcounter_dst,
      &g_deflate_midsummer_gt, UINT64_MAX, UINT64_MAX, 300);
}

const char*  //
bench_wuffs_deflate_decode_10k_part_init() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_deflate_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_deflate_midsummer_gt, UINT64_MAX, UINT64_MAX, 300);
}

const char*  //
bench_wuffs_deflate_decode_100k_just_one_read() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_deflate_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_deflate_pi_gt, UINT64_MAX, UINT64_MAX, 30);
}

const char*  //
bench_wuffs_deflate_decode_100k_many_big_reads() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(
      wuffs_deflate_decode,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED, tcounter_dst,
      &g_deflate_pi_gt, UINT64_MAX, 4096, 30);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_deflate_decode_1k_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_deflate_decode, 0, tcounter_dst,
                             &g_deflate_romeo_gt, UINT64_MAX, UINT64_MAX, 2000);
}

const char*  //
bench_mimic_deflate_decode_10k_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_deflate_decode, 0, tcounter_dst,
                             &g_deflate_midsummer_gt, UINT64_MAX, UINT64_MAX,
                             300);
}

const char*  //
bench_mimic_deflate_decode_100k_just_one_read() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_deflate_decode, 0, tcounter_dst,
                             &g_deflate_pi_gt, UINT64_MAX, UINT64_MAX, 30);
}

const char*  //
bench_mimic_deflate_decode_100k_many_big_reads() {
  CHECK_FOCUS(__func__);
  return do_bench_io_buffers(mimic_deflate_decode, 0, tcounter_dst,
                             &g_deflate_pi_gt, UINT64_MAX, 4096, 30);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    test_wuffs_deflate_decode_256_bytes,
    test_wuffs_deflate_decode_deflate_backref_crosses_blocks,
    test_wuffs_deflate_decode_deflate_degenerate_huffman,
    test_wuffs_deflate_decode_deflate_distance_32768,
    test_wuffs_deflate_decode_deflate_distance_code_31,
    test_wuffs_deflate_decode_deflate_huffman_primlen_9,
    test_wuffs_deflate_decode_interface,
    test_wuffs_deflate_decode_midsummer,
    test_wuffs_deflate_decode_pi_just_one_read,
    test_wuffs_deflate_decode_pi_many_big_reads,
    test_wuffs_deflate_decode_pi_many_medium_reads,
    test_wuffs_deflate_decode_pi_many_small_writes_reads,
    test_wuffs_deflate_decode_romeo,
    test_wuffs_deflate_decode_romeo_fixed,
    test_wuffs_deflate_decode_split_src,
    test_wuffs_deflate_decode_truncated_input,
    test_wuffs_deflate_history_full,
    test_wuffs_deflate_history_partial,
    test_wuffs_deflate_table_redirect,

#ifdef WUFFS_MIMIC

    test_mimic_deflate_decode_256_bytes,
    test_mimic_deflate_decode_deflate_backref_crosses_blocks,
    test_mimic_deflate_decode_deflate_degenerate_huffman,
    test_mimic_deflate_decode_deflate_distance_32768,
    test_mimic_deflate_decode_deflate_distance_code_31,
    test_mimic_deflate_decode_deflate_huffman_primlen_9,
    test_mimic_deflate_decode_midsummer,
    test_mimic_deflate_decode_pi_just_one_read,
#ifndef WUFFS_MIMICLIB_DEFLATE_DOES_NOT_SUPPORT_STREAMING
    test_mimic_deflate_decode_pi_many_big_reads,
#endif
    test_mimic_deflate_decode_romeo,
    test_mimic_deflate_decode_romeo_fixed,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_deflate_decode_1k_full_init,
    bench_wuffs_deflate_decode_1k_part_init,
    bench_wuffs_deflate_decode_10k_full_init,
    bench_wuffs_deflate_decode_10k_part_init,
    bench_wuffs_deflate_decode_100k_just_one_read,
    bench_wuffs_deflate_decode_100k_many_big_reads,

#ifdef WUFFS_MIMIC

    bench_mimic_deflate_decode_1k_full_init,
    bench_mimic_deflate_decode_10k_full_init,
    bench_mimic_deflate_decode_100k_just_one_read,
#ifndef WUFFS_MIMICLIB_DEFLATE_DOES_NOT_SUPPORT_STREAMING
    bench_mimic_deflate_decode_100k_many_big_reads,
#endif

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/deflate";
  return test_main(argc, argv, g_tests, g_benches);
}
