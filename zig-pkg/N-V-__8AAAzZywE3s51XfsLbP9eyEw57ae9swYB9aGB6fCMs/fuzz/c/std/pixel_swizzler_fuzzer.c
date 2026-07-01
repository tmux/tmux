// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// Silence the nested slash-star warning for the next comment's command line.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomment"

/*
This fuzzer (the fuzz function) is typically run indirectly, by a framework
such as https://github.com/google/oss-fuzz calling LLVMFuzzerTestOneInput.

When working on the fuzz implementation, or as a coherence check, defining
WUFFS_CONFIG__FUZZLIB_MAIN will let you manually run fuzz over a set of files:

gcc -DWUFFS_CONFIG__FUZZLIB_MAIN pixel_swizzler_fuzzer.c
./a.out ../../../test/data
rm -f ./a.out

It should print "PASS", amongst other information, and exit(0).
*/

#pragma clang diagnostic pop

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

#if defined(WUFFS_CONFIG__FUZZLIB_MAIN)
// Defining the WUFFS_CONFIG__STATIC_FUNCTIONS macro is optional, but when
// combined with WUFFS_IMPLEMENTATION, it demonstrates making all of Wuffs'
// functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#endif  // defined(WUFFS_CONFIG__FUZZLIB_MAIN)

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE

#include <sys/mman.h>
#include <unistd.h>

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../fuzzlib/fuzzlib.c"

const uint32_t pixfmts[] = {
    WUFFS_BASE__PIXEL_FORMAT__Y,
    WUFFS_BASE__PIXEL_FORMAT__Y_16BE,
    WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL,
    WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL,
    WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY,
    WUFFS_BASE__PIXEL_FORMAT__BGR_565,
    WUFFS_BASE__PIXEL_FORMAT__BGR,
    WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL,
    WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE,
    WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL,
    WUFFS_BASE__PIXEL_FORMAT__BGRX,
    WUFFS_BASE__PIXEL_FORMAT__RGB,
    WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL,
    WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL,
};

const wuffs_base__pixel_blend blends[] = {
    WUFFS_BASE__PIXEL_BLEND__SRC,
    WUFFS_BASE__PIXEL_BLEND__SRC_OVER,
};

size_t  //
round_up_to_pagesize(size_t n) {
  size_t ps = getpagesize();
  if (ps <= 0) {
    return n;
  }
  return ((n + (ps - 1)) / ps) * ps;
}

// allocate_guarded_pages allocates (2 * len) bytes of memory. The first half
// has read|write permissions. The second half has no permissions, so that
// attempting to read or write to it will cause a segmentation fault.
const char*  //
allocate_guarded_pages(uint8_t** ptr_out, size_t len) {
  void* ptr =
      mmap(NULL, 2 * len, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (ptr == MAP_FAILED) {
    return "fuzz: internal error: mmap failed";
  }
  if (mprotect(ptr, len, PROT_READ | PROT_WRITE)) {
    return "fuzz: internal error: mprotect failed";
  }
  *ptr_out = ptr;
  return NULL;
}

// fuzz_swizzle_interleaved_from_slice tests that, regardless of the randomized
// inputs, calling wuffs_base__pixel_swizzler__swizzle_interleaved_from_slice
// will not crash the fuzzer (e.g. due to reads or write past buffer bounds).
const char*  //
fuzz_swizzle_interleaved_from_slice(wuffs_base__io_buffer* src, uint64_t hash) {
  uint8_t dst_palette_array[1024];
  uint8_t src_palette_array[1024];
  if ((src->meta.wi - src->meta.ri) >= 2048) {
    memcpy(dst_palette_array, src->data.ptr + src->meta.ri, 1024);
    src->meta.ri += 1024;
    memcpy(src_palette_array, src->data.ptr + src->meta.ri, 1024);
    src->meta.ri += 1024;
  } else {
    for (uint32_t i = 0; i < 1024; i++) {
      dst_palette_array[i] = (uint8_t)i;
      src_palette_array[i] = (uint8_t)i;
    }
  }
  wuffs_base__slice_u8 dst_palette =
      wuffs_base__make_slice_u8(&dst_palette_array[0], 1024);
  wuffs_base__slice_u8 src_palette =
      wuffs_base__make_slice_u8(&src_palette_array[0], 1024);

  size_t num_pixfmts = sizeof(pixfmts) / sizeof(pixfmts[0]);
  wuffs_base__pixel_format dst_pixfmt = wuffs_base__make_pixel_format(
      pixfmts[(0xFF & (hash >> 0)) % num_pixfmts]);
  wuffs_base__pixel_format src_pixfmt = wuffs_base__make_pixel_format(
      pixfmts[(0xFF & (hash >> 8)) % num_pixfmts]);

  size_t num_blends = sizeof(blends) / sizeof(blends[0]);
  wuffs_base__pixel_blend blend = blends[(0xFF & (hash >> 16)) % num_blends];

  size_t dst_len = 0xFF & (hash >> 24);
  size_t src_len = 0xFF & (hash >> 32);

  wuffs_base__pixel_swizzler swizzler;
  wuffs_base__status status = wuffs_base__pixel_swizzler__prepare(
      &swizzler, dst_pixfmt, dst_palette, src_pixfmt, src_palette, blend);
  if (status.repr) {
    return wuffs_base__status__message(&status);
  }

  static size_t alloc_size = 0;
  if (alloc_size == 0) {
    alloc_size = round_up_to_pagesize(0x100);
  }

  static uint8_t* dst_alloc = NULL;
  if (!dst_alloc) {
    const char* z = allocate_guarded_pages(&dst_alloc, alloc_size);
    if (z != NULL) {
      return z;
    }
  }

  static uint8_t* src_alloc = NULL;
  if (!src_alloc) {
    const char* z = allocate_guarded_pages(&src_alloc, alloc_size);
    if (z != NULL) {
      return z;
    }
  }

  // Position dst_slice and src_slice so that reading or writing one byte past
  // their end will cause a segmentation fault.
  wuffs_base__slice_u8 dst_slice =
      wuffs_base__make_slice_u8(dst_alloc + alloc_size - dst_len, dst_len);
  wuffs_base__slice_u8 src_slice =
      wuffs_base__make_slice_u8(src_alloc + alloc_size - src_len, src_len);

  // Fill in dst_slice and src_slice.
  if ((src->meta.wi - src->meta.ri) >= (dst_len + src_len)) {
    memcpy(dst_slice.ptr, src->data.ptr + src->meta.ri, dst_len);
    src->meta.ri += dst_len;
    memcpy(src_slice.ptr, src->data.ptr + src->meta.ri, src_len);
    src->meta.ri += src_len;
  } else {
    for (size_t i = 0; i < dst_len; i++) {
      dst_slice.ptr[i] = (uint8_t)i;
    }
    for (size_t i = 0; i < src_len; i++) {
      src_slice.ptr[i] = (uint8_t)i;
    }
  }

  // When manually testing this program, enabling this code should lead to a
  // segmentation fault.
#if 0
  src_slice.ptr[src_slice.len]++;
#endif

  // Calling etc__swizzle_interleaved_from_slice should not crash, whether for
  // reading/writing out of bounds or for other reasons.
  wuffs_base__pixel_swizzler__swizzle_interleaved_from_slice(
      &swizzler, dst_slice, dst_palette, src_slice);

  return NULL;
}

const char*  //
fuzz_swizzle_ycck(wuffs_base__io_buffer* src, uint64_t hash) {
  uint8_t dst_palette_array[1024] = {0};
  wuffs_base__slice_u8 dst_palette =
      wuffs_base__make_slice_u8(&dst_palette_array[0], 1024);

  size_t num_pixfmts = sizeof(pixfmts) / sizeof(pixfmts[0]);
  wuffs_base__pixel_format dst_pixfmt = wuffs_base__make_pixel_format(
      pixfmts[(0xFF & (hash >> 0)) % num_pixfmts]);

  uint32_t x_min_incl = (63 & (hash >> 8)) + 1;
  uint32_t x_max_excl = (63 & (hash >> 14)) + 1;
  uint32_t y_min_incl = (63 & (hash >> 20)) + 1;
  uint32_t y_max_excl = (63 & (hash >> 26)) + 1;

  uint32_t width_in_mcus = (3 & (hash >> 32)) + 1;
  uint32_t height_in_mcus = (3 & (hash >> 34)) + 1;

  uint32_t allow_hv3 = 1 & (hash >> 36);
  bool is_rgb_or_cmyk = 1 & (hash >> 37);
  bool triangle_filter_for_2to1 = 1 & (hash >> 38);
  bool use_4th_component = 1 & (hash >> 39);

  if (triangle_filter_for_2to1) {
    x_min_incl = 0;
    y_min_incl = 0;
  } else {
    if (x_min_incl > x_max_excl) {
      uint32_t a = x_min_incl;
      x_min_incl = x_max_excl;
      x_max_excl = a;
    }
    if (y_min_incl > y_max_excl) {
      uint32_t a = y_min_incl;
      y_min_incl = y_max_excl;
      y_max_excl = a;
    }
  }

  uint32_t possible_hv_values[2][4] = {
      {1, 1, 2, 4},
      {1, 1, 3, 3},
  };
  uint32_t h0 = possible_hv_values[allow_hv3][3 & (hash >> 28)];
  uint32_t h1 = possible_hv_values[allow_hv3][3 & (hash >> 30)];
  uint32_t h2 = possible_hv_values[allow_hv3][3 & (hash >> 32)];
  uint32_t h3 =
      use_4th_component ? possible_hv_values[allow_hv3][3 & (hash >> 34)] : 0;
  uint32_t v0 = possible_hv_values[allow_hv3][3 & (hash >> 36)];
  uint32_t v1 = possible_hv_values[allow_hv3][3 & (hash >> 38)];
  uint32_t v2 = possible_hv_values[allow_hv3][3 & (hash >> 40)];
  uint32_t v3 =
      use_4th_component ? possible_hv_values[allow_hv3][3 & (hash >> 42)] : 0;

  uint32_t width0 = 8 * width_in_mcus * h0;
  uint32_t width1 = 8 * width_in_mcus * h1;
  uint32_t width2 = 8 * width_in_mcus * h2;
  uint32_t width3 = 8 * width_in_mcus * h3;
  uint32_t height0 = 8 * height_in_mcus * v0;
  uint32_t height1 = 8 * height_in_mcus * v1;
  uint32_t height2 = 8 * height_in_mcus * v2;
  uint32_t height3 = 8 * height_in_mcus * v3;

  uint32_t hmax = wuffs_base__u32__max(
      h0, wuffs_base__u32__max(h1, wuffs_base__u32__max(h2, h3)));
  uint32_t vmax = wuffs_base__u32__max(
      v0, wuffs_base__u32__max(v1, wuffs_base__u32__max(v2, v3)));
  x_max_excl = x_min_incl + wuffs_base__u32__min(x_max_excl - x_min_incl,
                                                 8 * width_in_mcus * hmax);
  y_max_excl = y_min_incl + wuffs_base__u32__min(y_max_excl - y_min_incl,
                                                 8 * height_in_mcus * vmax);

  static size_t dst_alloc_size = 0;
  if (dst_alloc_size == 0) {
    dst_alloc_size = round_up_to_pagesize(8 * 64 * 64);
  }

  static uint8_t* dst_alloc = NULL;
  if (!dst_alloc) {
    const char* z = allocate_guarded_pages(&dst_alloc, dst_alloc_size);
    if (z != NULL) {
      return z;
    }
  }

  wuffs_base__pixel_buffer dst_pixbuf;
  wuffs_base__pixel_config dst_pixcfg;
  wuffs_base__pixel_config__set(&dst_pixcfg, dst_pixfmt.repr, 0, x_max_excl,
                                y_max_excl);
  uint64_t dst_pixbuf_len = wuffs_base__pixel_config__pixbuf_len(&dst_pixcfg);
  if (dst_pixbuf_len > dst_alloc_size) {
    return "fuzz: internal error: dst_alloc_size is too small";
  } else {
    wuffs_base__status status = wuffs_base__pixel_buffer__set_from_slice(
        &dst_pixbuf, &dst_pixcfg,
        wuffs_base__make_slice_u8(dst_alloc + dst_alloc_size - dst_pixbuf_len,
                                  dst_pixbuf_len));
    if (status.repr) {
      return "fuzz: internal error: wuffs_base__pixel_buffer__set_from_slice "
             "failed";
    }
  }

  static size_t src_alloc_size = 0;
  if (src_alloc_size == 0) {
    src_alloc_size = round_up_to_pagesize(8 * 4 * 4 * 8 * 4 * 4);
  }

  static uint8_t* src_alloc0 = NULL;
  static uint8_t* src_alloc1 = NULL;
  static uint8_t* src_alloc2 = NULL;
  static uint8_t* src_alloc3 = NULL;
  if (!src_alloc0) {
    const char* z = allocate_guarded_pages(&src_alloc0, src_alloc_size);
    if (z != NULL) {
      return z;
    }
  }
  if (!src_alloc1) {
    const char* z = allocate_guarded_pages(&src_alloc1, src_alloc_size);
    if (z != NULL) {
      return z;
    }
  }
  if (!src_alloc2) {
    const char* z = allocate_guarded_pages(&src_alloc2, src_alloc_size);
    if (z != NULL) {
      return z;
    }
  }
  if (!src_alloc3) {
    const char* z = allocate_guarded_pages(&src_alloc3, src_alloc_size);
    if (z != NULL) {
      return z;
    }
  }

  uint32_t src_len0 = width0 * height0;
  uint32_t src_len1 = width1 * height1;
  uint32_t src_len2 = width2 * height2;
  uint32_t src_len3 = width3 * height3;
  if ((src_len0 > src_alloc_size) ||  //
      (src_len1 > src_alloc_size) ||  //
      (src_len2 > src_alloc_size) ||  //
      (src_len3 > src_alloc_size)) {
    return "fuzz: internal error: src_alloc_size is too small";
  }

  uint8_t s0 = 0x90;
  if (src->meta.ri < src->meta.wi) {
    s0 = src->data.ptr[src->meta.ri++];
  }
  uint8_t s1 = 0x91;
  if (src->meta.ri < src->meta.wi) {
    s1 = src->data.ptr[src->meta.ri++];
  }
  uint8_t s2 = 0x92;
  if (src->meta.ri < src->meta.wi) {
    s2 = src->data.ptr[src->meta.ri++];
  }
  uint8_t s3 = 0x93;
  if (src->meta.ri < src->meta.wi) {
    s3 = src->data.ptr[src->meta.ri++];
  }

  wuffs_base__slice_u8 src0 = wuffs_base__make_slice_u8(
      src_alloc0 + src_alloc_size - src_len0, src_len0);
  memset(src0.ptr, s0, src0.len);
  wuffs_base__slice_u8 src1 = wuffs_base__make_slice_u8(
      src_alloc1 + src_alloc_size - src_len1, src_len1);
  memset(src1.ptr, s1, src1.len);
  wuffs_base__slice_u8 src2 = wuffs_base__make_slice_u8(
      src_alloc2 + src_alloc_size - src_len2, src_len2);
  memset(src2.ptr, s2, src2.len);
  wuffs_base__slice_u8 src3 = wuffs_base__empty_slice_u8();
  if (use_4th_component) {
    wuffs_base__slice_u8 src3 = wuffs_base__make_slice_u8(
        src_alloc3 + src_alloc_size - src_len3, src_len3);
    memset(src3.ptr, s3, src3.len);
  }

  wuffs_base__pixel_swizzler swizzler = {0};
  uint8_t scratch_buffer[2048];
  wuffs_base__status status = wuffs_base__pixel_swizzler__swizzle_ycck(
      &swizzler, &dst_pixbuf, dst_palette,  //
      x_min_incl, x_max_excl,               //
      y_min_incl, y_max_excl,               //
      src0, src1, src2, src3,               //
      width0, width1, width2, width3,       //
      height0, height1, height2, height3,   //
      width0, width1, width2, width3,       //
      h0, h1, h2, h3,                       //
      v0, v1, v2, v3,                       //
      is_rgb_or_cmyk,                       //
      triangle_filter_for_2to1,             //
      wuffs_base__make_slice_u8(scratch_buffer, sizeof(scratch_buffer)));
  if (status.repr) {
    return wuffs_base__status__message(&status);
  }
  return NULL;
}

const char*  //
fuzz(wuffs_base__io_buffer* src, uint64_t hash) {
  const char* s0 = fuzz_swizzle_interleaved_from_slice(src, hash);
  const char* s1 = fuzz_swizzle_ycck(src, hash);
  if (s0 && strstr(s0, "internal error:")) {
    return s0;
  }
  if (s1 && strstr(s1, "internal error:")) {
    return s1;
  }
  return s0 ? s0 : s1;
}
