// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// We #include a foo.cpp file, not a foo.h file, as stb_image is a "single file
// C library".
//
// Consider also #define'ing STBI_NO_SIMD.
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STB_IMAGE_IMPLEMENTATION
#include "/path/to/your/copy/of/github.com/nothings/stb/stb_image.h"

const char*  //
mimic_stb_decode(uint64_t* n_bytes_out,
                 wuffs_base__io_buffer* dst,
                 uint32_t wuffs_initialize_flags,
                 wuffs_base__pixel_format pixfmt,
                 uint32_t* quirks_ptr,
                 size_t quirks_len,
                 wuffs_base__io_buffer* src) {
  wuffs_base__io_buffer dst_fallback =
      wuffs_base__slice_u8__writer(g_mimiclib_scratch_slice_u8);
  if (!dst) {
    dst = &dst_fallback;
  }

  uint64_t n = 0;
  bool swap_bgra_rgba = false;
  switch (pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      n = 1;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      n = 4;
      // stb_image doesn't do BGRA8. RGBA8 is the closest approximation. We'll
      // fix it up later.
      swap_bgra_rgba = true;
      break;
    default:
      return "mimic_stb_decode: unsupported pixfmt";
  }

  int width = 0;
  int height = 0;
  int channels_in_file = 0;
  unsigned char* output =
      stbi_load_from_memory(wuffs_base__io_buffer__reader_pointer(src),  //
                            wuffs_base__io_buffer__reader_length(src),   //
                            &width, &height, &channels_in_file, n);
  if (!output) {
    return "mimic_stb_decode: could not load image";
  }

  const char* ret = NULL;

  if ((width > 0xFFFF) || (height > 0xFFFF)) {
    ret = "mimic_stb_decode: image is too large";
    goto cleanup0;
  }
  n *= ((uint64_t)width) * ((uint64_t)height);
  if (n > wuffs_base__io_buffer__writer_length(dst)) {
    ret = "mimic_stb_decode: image is too large";
    goto cleanup0;
  }

  // Copy from the mimic library's output buffer to Wuffs' dst buffer.
  uint8_t* dst_ptr = wuffs_base__io_buffer__writer_pointer(dst);
  memcpy(dst_ptr, output, n);
  dst->meta.wi += n;
  if (n_bytes_out) {
    *n_bytes_out += n;
  }

  // Fix up BGRA8 vs RGBA8.
  if (swap_bgra_rgba) {
    for (; n >= 4; n -= 4) {
      uint8_t swap = dst_ptr[0];
      dst_ptr[0] = dst_ptr[2];
      dst_ptr[2] = swap;
      dst_ptr += 4;
    }
  }

cleanup0:;
  stbi_image_free(output);
  return ret;
}
