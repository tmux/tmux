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

// Uncomment one of these #define lines to test and bench alternative mimic
// libraries (stb_image) instead of libjpeg.
//
// These are collectively referred to as
// WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_LIBJPEG.
//
// #define WUFFS_MIMICLIB_USE_STB_IMAGE_INSTEAD_OF_LIBJPEG 1

// -------------------------------- WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_LIBJPEG
#if defined(WUFFS_MIMICLIB_USE_STB_IMAGE_INSTEAD_OF_LIBJPEG)

#define WUFFS_MIMICLIB_JPEG_DOES_NOT_EXACTLY_MATCH_LIBJPEG 1

#define mimic_jpeg_decode mimic_stb_decode
#include "./stb.c"

// -------------------------------- WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_LIBJPEG
#else

#include "jpeglib.h"

const char*  //
mimic_jpeg_decode(uint64_t* n_bytes_out,
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

  const char* ret = NULL;

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, wuffs_base__io_buffer__reader_pointer(src),
               wuffs_base__io_buffer__reader_length(src));
  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    ret = "mimic_jpeg_decode: jpeg_read_header failed";
    goto cleanup0;
  }

  switch (pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      cinfo.out_color_space = JCS_GRAYSCALE;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      cinfo.out_color_space = JCS_EXT_BGRA;
      break;
    default:
      ret = "mimic_jpeg_decode: unsupported pixfmt";
      goto cleanup0;
  }

  jpeg_start_decompress(&cinfo);
  size_t stride =
      cinfo.output_width *
      ((size_t)(wuffs_base__pixel_format__bits_per_pixel(&pixfmt) / 8u));

  while (cinfo.output_scanline < cinfo.output_height) {
    if (wuffs_base__io_buffer__writer_length(dst) < stride) {
      ret = "mimic_jpeg_decode: image is too large";
      goto cleanup0;
    }
    JSAMPLE* scanlines[1] = {(void*)wuffs_base__io_buffer__writer_pointer(dst)};
    if (jpeg_read_scanlines(&cinfo, scanlines, 1)) {
      dst->meta.wi += stride;
      if (n_bytes_out) {
        *n_bytes_out += stride;
      }
    }
  }

  jpeg_finish_decompress(&cinfo);

cleanup0:;
  jpeg_destroy_decompress(&cinfo);
  return ret;
}

#endif
// -------------------------------- WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_LIBJPEG
