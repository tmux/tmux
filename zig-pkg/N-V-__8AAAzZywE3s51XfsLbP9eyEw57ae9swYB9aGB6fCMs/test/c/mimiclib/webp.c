// Copyright 2024 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "webp/decode.h"

static void  //
mimiclib_convert_to_y_from_bgra_nonpremul(uint8_t* dst,
                                          const uint8_t* src,
                                          uint32_t n) {
  while (n--) {
    uint32_t s0_nonpremul = wuffs_base__peek_u32le__no_bounds_check(src);
    uint32_t s0_premul =
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
            s0_nonpremul);
    dst[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0_premul);
    dst += 1;
    src += 4;
  }
}

const char*  //
mimic_webp_decode(uint64_t* n_bytes_out,
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

  WebPDecoderConfig config;
  if (!WebPInitDecoderConfig(&config)) {
    return "mimic_webp_decode: WebPInitDecoderConfig failed";
  } else if (WebPGetFeatures(wuffs_base__io_buffer__reader_pointer(src),
                             wuffs_base__io_buffer__reader_length(src),
                             &config.input) != VP8_STATUS_OK) {
    return "mimic_webp_decode: WebPGetFeatures failed";
  }

  const uint32_t w = config.input.width;
  const uint32_t h = config.input.height;
  if ((w > 0x4000) || (h > 0x4000)) {
    return "mimic_webp_decode: impossible WebP dimensions";
  }

  switch (pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      if (((1 * w * h) > wuffs_base__io_buffer__writer_length(dst)) ||
          ((4 * w * h) > g_mimiclib_scratch_slice_u8.len)) {
        return "mimic_webp_decode: image is too large";
      }
      config.output.u.RGBA.rgba = g_mimiclib_scratch_slice_u8.ptr;
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      if ((4 * w * h) > wuffs_base__io_buffer__writer_length(dst)) {
        return "mimic_webp_decode: image is too large";
      }
      config.output.u.RGBA.rgba = wuffs_base__io_buffer__writer_pointer(dst);
      break;

    default:
      return "mimic_webp_decode: unsupported pixfmt";
  }

  config.output.colorspace = MODE_BGRA;
  config.output.width = w;
  config.output.height = h;
  config.output.is_external_memory = 1;
  config.output.u.RGBA.stride = 4 * w;
  config.output.u.RGBA.size = 4 * w * h;

  VP8StatusCode status =
      WebPDecode(wuffs_base__io_buffer__reader_pointer(src),
                 wuffs_base__io_buffer__reader_length(src), &config);
  WebPFreeDecBuffer(&config.output);
  if (status != VP8_STATUS_OK) {
    return "mimic_webp_decode: WebPDecode failed";
  }

  size_t n = 0;
  switch (pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      mimiclib_convert_to_y_from_bgra_nonpremul(
          wuffs_base__io_buffer__writer_pointer(dst),
          g_mimiclib_scratch_slice_u8.ptr, 1 * w * h);
      n = 1 * w * h;
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      n = 4 * w * h;
      break;
    default:
      break;
  }

  dst->meta.wi += n;
  if (n_bytes_out) {
    *n_bytes_out += n;
  }

  return NULL;
}
