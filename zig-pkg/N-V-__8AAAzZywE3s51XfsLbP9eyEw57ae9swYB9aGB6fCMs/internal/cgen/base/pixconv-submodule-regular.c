// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Pixel Swizzler

#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2")
static uint64_t  //
wuffs_private_impl__swizzle_bgrw__bgr__x86_sse42(uint8_t* dst_ptr,
                                                 size_t dst_len,
                                                 uint8_t* dst_palette_ptr,
                                                 size_t dst_palette_len,
                                                 const uint8_t* src_ptr,
                                                 size_t src_len);

static uint64_t  //
wuffs_private_impl__swizzle_bgrw__rgb__x86_sse42(uint8_t* dst_ptr,
                                                 size_t dst_len,
                                                 uint8_t* dst_palette_ptr,
                                                 size_t dst_palette_len,
                                                 const uint8_t* src_ptr,
                                                 size_t src_len);

WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2")
static uint64_t  //
wuffs_private_impl__swizzle_swap_rgbx_bgrx__x86_sse42(uint8_t* dst_ptr,
                                                      size_t dst_len,
                                                      uint8_t* dst_palette_ptr,
                                                      size_t dst_palette_len,
                                                      const uint8_t* src_ptr,
                                                      size_t src_len);

WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2")
static uint64_t  //
wuffs_private_impl__swizzle_xxxx__y__x86_sse42(uint8_t* dst_ptr,
                                               size_t dst_len,
                                               uint8_t* dst_palette_ptr,
                                               size_t dst_palette_len,
                                               const uint8_t* src_ptr,
                                               size_t src_len);
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)

// --------

static inline uint32_t  //
wuffs_private_impl__swap_u32_argb_abgr(uint32_t u) {
  uint32_t o = u & 0xFF00FF00ul;
  uint32_t r = u & 0x00FF0000ul;
  uint32_t b = u & 0x000000FFul;
  return o | (r >> 16) | (b << 16);
}

static inline uint64_t  //
wuffs_private_impl__swap_u64_argb_abgr(uint64_t u) {
  uint64_t o = u & 0xFFFF0000FFFF0000ull;
  uint64_t r = u & 0x0000FFFF00000000ull;
  uint64_t b = u & 0x000000000000FFFFull;
  return o | (r >> 32) | (b << 32);
}

static inline uint32_t  //
wuffs_private_impl__color_u64__as__color_u32__swap_u32_argb_abgr(uint64_t c) {
  uint32_t a = ((uint32_t)(0xFF & (c >> 56)));
  uint32_t r = ((uint32_t)(0xFF & (c >> 40)));
  uint32_t g = ((uint32_t)(0xFF & (c >> 24)));
  uint32_t b = ((uint32_t)(0xFF & (c >> 8)));
  return (a << 24) | (b << 16) | (g << 8) | (r << 0);
}

// --------

WUFFS_BASE__MAYBE_STATIC wuffs_base__color_u32_argb_premul  //
wuffs_base__pixel_buffer__color_u32_at(const wuffs_base__pixel_buffer* pb,
                                       uint32_t x,
                                       uint32_t y) {
  if (!pb || (x >= pb->pixcfg.private_impl.width) ||
      (y >= pb->pixcfg.private_impl.height)) {
    return 0;
  }

  if (wuffs_base__pixel_format__is_planar(&pb->pixcfg.private_impl.pixfmt)) {
    // TODO: support planar formats.
    return 0;
  }

  size_t stride = pb->private_impl.planes[0].stride;
  const uint8_t* row = pb->private_impl.planes[0].ptr + (stride * ((size_t)y));

  switch (pb->pixcfg.private_impl.pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
      return wuffs_base__peek_u32le__no_bounds_check(row + (4 * ((size_t)x)));

    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY: {
      uint8_t* palette = pb->private_impl.planes[3].ptr;
      return wuffs_base__peek_u32le__no_bounds_check(palette +
                                                     (4 * ((size_t)row[x])));
    }

      // Common formats above. Rarer formats below.

    case WUFFS_BASE__PIXEL_FORMAT__Y:
      return 0xFF000000 | (0x00010101 * ((uint32_t)(row[x])));
    case WUFFS_BASE__PIXEL_FORMAT__Y_16LE:
      return 0xFF000000 | (0x00010101 * ((uint32_t)(row[(2 * x) + 1])));
    case WUFFS_BASE__PIXEL_FORMAT__Y_16BE:
      return 0xFF000000 | (0x00010101 * ((uint32_t)(row[(2 * x) + 0])));
    case WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL:
      return wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
          (((uint32_t)(row[(2 * x) + 1])) << 24) |
          (((uint32_t)(row[(2 * x) + 0])) * 0x00010101));

    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL: {
      uint8_t* palette = pb->private_impl.planes[3].ptr;
      return wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
          wuffs_base__peek_u32le__no_bounds_check(palette +
                                                  (4 * ((size_t)row[x]))));
    }

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      return wuffs_base__color_u16_rgb_565__as__color_u32_argb_premul(
          wuffs_base__peek_u16le__no_bounds_check(row + (2 * ((size_t)x))));
    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      return 0xFF000000 |
             wuffs_base__peek_u24le__no_bounds_check(row + (3 * ((size_t)x)));
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      return wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
          wuffs_base__peek_u32le__no_bounds_check(row + (4 * ((size_t)x))));
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      return wuffs_base__color_u64_argb_nonpremul__as__color_u32_argb_premul(
          wuffs_base__peek_u64le__no_bounds_check(row + (8 * ((size_t)x))));
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      return 0xFF000000 |
             wuffs_base__peek_u32le__no_bounds_check(row + (4 * ((size_t)x)));

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      return 0xFF000000 |
             wuffs_base__peek_u24be__no_bounds_check(row + (3 * ((size_t)x)));
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      return wuffs_private_impl__swap_u32_argb_abgr(
          wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
              wuffs_base__peek_u32le__no_bounds_check(row +
                                                      (4 * ((size_t)x)))));
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
      return wuffs_private_impl__swap_u32_argb_abgr(
          wuffs_base__peek_u32le__no_bounds_check(row + (4 * ((size_t)x))));
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      return wuffs_private_impl__swap_u32_argb_abgr(
          0xFF000000 |
          wuffs_base__peek_u32le__no_bounds_check(row + (4 * ((size_t)x))));

    default:
      // TODO: support more formats.
      break;
  }

  return 0;
}

// --------

WUFFS_BASE__MAYBE_STATIC wuffs_base__status  //
wuffs_base__pixel_buffer__set_color_u32_at(
    wuffs_base__pixel_buffer* pb,
    uint32_t x,
    uint32_t y,
    wuffs_base__color_u32_argb_premul color) {
  if (!pb) {
    return wuffs_base__make_status(wuffs_base__error__bad_receiver);
  }
  if ((x >= pb->pixcfg.private_impl.width) ||
      (y >= pb->pixcfg.private_impl.height)) {
    return wuffs_base__make_status(wuffs_base__error__bad_argument);
  }

  if (wuffs_base__pixel_format__is_planar(&pb->pixcfg.private_impl.pixfmt)) {
    // TODO: support planar formats.
    return wuffs_base__make_status(wuffs_base__error__unsupported_option);
  }

  size_t stride = pb->private_impl.planes[0].stride;
  uint8_t* row = pb->private_impl.planes[0].ptr + (stride * ((size_t)y));

  switch (pb->pixcfg.private_impl.pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      wuffs_base__poke_u32le__no_bounds_check(row + (4 * ((size_t)x)), color);
      break;

      // Common formats above. Rarer formats below.

    case WUFFS_BASE__PIXEL_FORMAT__Y:
      wuffs_base__poke_u8__no_bounds_check(
          row + ((size_t)x),
          wuffs_base__color_u32_argb_premul__as__color_u8_gray(color));
      break;
    case WUFFS_BASE__PIXEL_FORMAT__Y_16LE:
      wuffs_base__poke_u16le__no_bounds_check(
          row + (2 * ((size_t)x)),
          wuffs_base__color_u32_argb_premul__as__color_u16_gray(color));
      break;
    case WUFFS_BASE__PIXEL_FORMAT__Y_16BE:
      wuffs_base__poke_u16be__no_bounds_check(
          row + (2 * ((size_t)x)),
          wuffs_base__color_u32_argb_premul__as__color_u16_gray(color));
      break;
    case WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL:
      wuffs_base__poke_u16le__no_bounds_check(
          row + (2 * ((size_t)x)),
          wuffs_base__color_u32_argb_premul__as__color_u16_alpha_gray_nonpremul(
              color));
      break;

    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY:
      wuffs_base__poke_u8__no_bounds_check(
          row + ((size_t)x), wuffs_base__pixel_palette__closest_element(
                                 wuffs_base__pixel_buffer__palette(pb),
                                 pb->pixcfg.private_impl.pixfmt, color));
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      wuffs_base__poke_u16le__no_bounds_check(
          row + (2 * ((size_t)x)),
          wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(color));
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      wuffs_base__poke_u24le__no_bounds_check(row + (3 * ((size_t)x)), color);
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      wuffs_base__poke_u32le__no_bounds_check(
          row + (4 * ((size_t)x)),
          wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(
              color));
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      wuffs_base__poke_u64le__no_bounds_check(
          row + (8 * ((size_t)x)),
          wuffs_base__color_u32_argb_premul__as__color_u64_argb_nonpremul(
              color));
      break;
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
      wuffs_base__poke_u32le__no_bounds_check(
          row + (4 * ((size_t)x)), (color >> 31) ? (color | 0xFF000000) : 0);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      wuffs_base__poke_u24le__no_bounds_check(
          row + (3 * ((size_t)x)),
          wuffs_private_impl__swap_u32_argb_abgr(color));
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      wuffs_base__poke_u32le__no_bounds_check(
          row + (4 * ((size_t)x)),
          wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(
              wuffs_private_impl__swap_u32_argb_abgr(color)));
      break;
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      wuffs_base__poke_u32le__no_bounds_check(
          row + (4 * ((size_t)x)),
          wuffs_private_impl__swap_u32_argb_abgr(color));
      break;

    default:
      // TODO: support more formats.
      return wuffs_base__make_status(wuffs_base__error__unsupported_option);
  }

  return wuffs_base__make_status(NULL);
}

// --------

static inline void  //
wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xx(
    wuffs_base__pixel_buffer* pb,
    wuffs_base__rect_ie_u32 rect,
    uint16_t color) {
  size_t stride = pb->private_impl.planes[0].stride;
  uint32_t width = wuffs_base__rect_ie_u32__width(&rect);
  if ((stride == (2 * ((uint64_t)width))) && (rect.min_incl_x == 0)) {
    uint8_t* ptr =
        pb->private_impl.planes[0].ptr + (stride * ((size_t)rect.min_incl_y));
    uint32_t height = wuffs_base__rect_ie_u32__height(&rect);
    size_t n;
    for (n = ((size_t)width) * ((size_t)height); n > 0; n--) {
      wuffs_base__poke_u16le__no_bounds_check(ptr, color);
      ptr += 2;
    }
    return;
  }

  uint32_t y;
  for (y = rect.min_incl_y; y < rect.max_excl_y; y++) {
    uint8_t* ptr = pb->private_impl.planes[0].ptr + (stride * ((size_t)y)) +
                   (2 * ((size_t)rect.min_incl_x));
    uint32_t n;
    for (n = width; n > 0; n--) {
      wuffs_base__poke_u16le__no_bounds_check(ptr, color);
      ptr += 2;
    }
  }
}

static inline void  //
wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxx(
    wuffs_base__pixel_buffer* pb,
    wuffs_base__rect_ie_u32 rect,
    uint32_t color) {
  size_t stride = pb->private_impl.planes[0].stride;
  uint32_t width = wuffs_base__rect_ie_u32__width(&rect);
  if ((stride == (3 * ((uint64_t)width))) && (rect.min_incl_x == 0)) {
    uint8_t* ptr =
        pb->private_impl.planes[0].ptr + (stride * ((size_t)rect.min_incl_y));
    uint32_t height = wuffs_base__rect_ie_u32__height(&rect);
    size_t n;
    for (n = ((size_t)width) * ((size_t)height); n > 0; n--) {
      wuffs_base__poke_u24le__no_bounds_check(ptr, color);
      ptr += 3;
    }
    return;
  }

  uint32_t y;
  for (y = rect.min_incl_y; y < rect.max_excl_y; y++) {
    uint8_t* ptr = pb->private_impl.planes[0].ptr + (stride * ((size_t)y)) +
                   (3 * ((size_t)rect.min_incl_x));
    uint32_t n;
    for (n = width; n > 0; n--) {
      wuffs_base__poke_u24le__no_bounds_check(ptr, color);
      ptr += 3;
    }
  }
}

static inline void  //
wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxxx(
    wuffs_base__pixel_buffer* pb,
    wuffs_base__rect_ie_u32 rect,
    uint32_t color) {
  size_t stride = pb->private_impl.planes[0].stride;
  uint32_t width = wuffs_base__rect_ie_u32__width(&rect);
  if ((stride == (4 * ((uint64_t)width))) && (rect.min_incl_x == 0)) {
    uint8_t* ptr =
        pb->private_impl.planes[0].ptr + (stride * ((size_t)rect.min_incl_y));
    uint32_t height = wuffs_base__rect_ie_u32__height(&rect);
    size_t n;
    for (n = ((size_t)width) * ((size_t)height); n > 0; n--) {
      wuffs_base__poke_u32le__no_bounds_check(ptr, color);
      ptr += 4;
    }
    return;
  }

  uint32_t y;
  for (y = rect.min_incl_y; y < rect.max_excl_y; y++) {
    uint8_t* ptr = pb->private_impl.planes[0].ptr + (stride * ((size_t)y)) +
                   (4 * ((size_t)rect.min_incl_x));
    uint32_t n;
    for (n = width; n > 0; n--) {
      wuffs_base__poke_u32le__no_bounds_check(ptr, color);
      ptr += 4;
    }
  }
}

static inline void  //
wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxxxxxxx(
    wuffs_base__pixel_buffer* pb,
    wuffs_base__rect_ie_u32 rect,
    uint64_t color) {
  size_t stride = pb->private_impl.planes[0].stride;
  uint32_t width = wuffs_base__rect_ie_u32__width(&rect);
  if ((stride == (8 * ((uint64_t)width))) && (rect.min_incl_x == 0)) {
    uint8_t* ptr =
        pb->private_impl.planes[0].ptr + (stride * ((size_t)rect.min_incl_y));
    uint32_t height = wuffs_base__rect_ie_u32__height(&rect);
    size_t n;
    for (n = ((size_t)width) * ((size_t)height); n > 0; n--) {
      wuffs_base__poke_u64le__no_bounds_check(ptr, color);
      ptr += 8;
    }
    return;
  }

  uint32_t y;
  for (y = rect.min_incl_y; y < rect.max_excl_y; y++) {
    uint8_t* ptr = pb->private_impl.planes[0].ptr + (stride * ((size_t)y)) +
                   (8 * ((size_t)rect.min_incl_x));
    uint32_t n;
    for (n = width; n > 0; n--) {
      wuffs_base__poke_u64le__no_bounds_check(ptr, color);
      ptr += 8;
    }
  }
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__status  //
wuffs_base__pixel_buffer__set_color_u32_fill_rect(
    wuffs_base__pixel_buffer* pb,
    wuffs_base__rect_ie_u32 rect,
    wuffs_base__color_u32_argb_premul color) {
  if (!pb) {
    return wuffs_base__make_status(wuffs_base__error__bad_receiver);
  } else if (wuffs_base__rect_ie_u32__is_empty(&rect)) {
    return wuffs_base__make_status(NULL);
  }
  wuffs_base__rect_ie_u32 bounds =
      wuffs_base__pixel_config__bounds(&pb->pixcfg);
  if (!wuffs_base__rect_ie_u32__contains_rect(&bounds, rect)) {
    return wuffs_base__make_status(wuffs_base__error__bad_argument);
  }

  if (wuffs_base__pixel_format__is_planar(&pb->pixcfg.private_impl.pixfmt)) {
    // TODO: support planar formats.
    return wuffs_base__make_status(wuffs_base__error__unsupported_option);
  }

  switch (pb->pixcfg.private_impl.pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxxx(pb, rect,
                                                                      color);
      return wuffs_base__make_status(NULL);

      // Common formats above. Rarer formats below.

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xx(
          pb, rect,
          wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(color));
      return wuffs_base__make_status(NULL);

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxx(pb, rect,
                                                                     color);
      return wuffs_base__make_status(NULL);

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxxx(
          pb, rect,
          wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(
              color));
      return wuffs_base__make_status(NULL);

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxxxxxxx(
          pb, rect,
          wuffs_base__color_u32_argb_premul__as__color_u64_argb_nonpremul(
              color));
      return wuffs_base__make_status(NULL);

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxxx(
          pb, rect,
          wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(
              wuffs_private_impl__swap_u32_argb_abgr(color)));
      return wuffs_base__make_status(NULL);

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      wuffs_private_impl__pixel_buffer__set_color_u32_fill_rect__xxxx(
          pb, rect, wuffs_private_impl__swap_u32_argb_abgr(color));
      return wuffs_base__make_status(NULL);
  }

  uint32_t y;
  for (y = rect.min_incl_y; y < rect.max_excl_y; y++) {
    uint32_t x;
    for (x = rect.min_incl_x; x < rect.max_excl_x; x++) {
      wuffs_base__pixel_buffer__set_color_u32_at(pb, x, y, color);
    }
  }
  return wuffs_base__make_status(NULL);
}

// --------

WUFFS_BASE__MAYBE_STATIC uint8_t  //
wuffs_base__pixel_palette__closest_element(
    wuffs_base__slice_u8 palette_slice,
    wuffs_base__pixel_format palette_format,
    wuffs_base__color_u32_argb_premul c) {
  size_t n = palette_slice.len / 4;
  if (n > (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
    n = (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4);
  }
  size_t best_index = 0;
  uint64_t best_score = 0xFFFFFFFFFFFFFFFF;

  // Work in 16-bit color.
  uint32_t ca = 0x101 * (0xFF & (c >> 24));
  uint32_t cr = 0x101 * (0xFF & (c >> 16));
  uint32_t cg = 0x101 * (0xFF & (c >> 8));
  uint32_t cb = 0x101 * (0xFF & (c >> 0));

  switch (palette_format.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY: {
      bool nonpremul = palette_format.repr ==
                       WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL;

      size_t i;
      for (i = 0; i < n; i++) {
        // Work in 16-bit color.
        uint32_t pb = 0x101 * ((uint32_t)(palette_slice.ptr[(4 * i) + 0]));
        uint32_t pg = 0x101 * ((uint32_t)(palette_slice.ptr[(4 * i) + 1]));
        uint32_t pr = 0x101 * ((uint32_t)(palette_slice.ptr[(4 * i) + 2]));
        uint32_t pa = 0x101 * ((uint32_t)(palette_slice.ptr[(4 * i) + 3]));

        // Convert to premultiplied alpha.
        if (nonpremul && (pa != 0xFFFF)) {
          pb = (pb * pa) / 0xFFFF;
          pg = (pg * pa) / 0xFFFF;
          pr = (pr * pa) / 0xFFFF;
        }

        // These deltas are conceptually int32_t (signed) but after squaring,
        // it's equivalent to work in uint32_t (unsigned).
        pb -= cb;
        pg -= cg;
        pr -= cr;
        pa -= ca;
        uint64_t score = ((uint64_t)(pb * pb)) + ((uint64_t)(pg * pg)) +
                         ((uint64_t)(pr * pr)) + ((uint64_t)(pa * pa));
        if (best_score > score) {
          best_score = score;
          best_index = i;
        }
      }
      break;
    }
  }

  return (uint8_t)best_index;
}

// --------

static inline uint32_t  //
wuffs_private_impl__composite_nonpremul_nonpremul_u32_axxx(
    uint32_t dst_nonpremul,
    uint32_t src_nonpremul) {
  // Extract 16-bit color components.
  //
  // If the destination is transparent then SRC_OVER is equivalent to SRC: just
  // return src_nonpremul. This isn't just an optimization (skipping the rest
  // of the function's computation). It also preserves the nonpremul
  // distinction between e.g. transparent red and transparent blue that would
  // otherwise be lost by converting from nonpremul to premul and back.
  uint32_t da = 0x101 * (0xFF & (dst_nonpremul >> 24));
  if (da == 0) {
    return src_nonpremul;
  }
  uint32_t dr = 0x101 * (0xFF & (dst_nonpremul >> 16));
  uint32_t dg = 0x101 * (0xFF & (dst_nonpremul >> 8));
  uint32_t db = 0x101 * (0xFF & (dst_nonpremul >> 0));
  uint32_t sa = 0x101 * (0xFF & (src_nonpremul >> 24));
  uint32_t sr = 0x101 * (0xFF & (src_nonpremul >> 16));
  uint32_t sg = 0x101 * (0xFF & (src_nonpremul >> 8));
  uint32_t sb = 0x101 * (0xFF & (src_nonpremul >> 0));

  // Convert dst from nonpremul to premul.
  dr = (dr * da) / 0xFFFF;
  dg = (dg * da) / 0xFFFF;
  db = (db * da) / 0xFFFF;

  // Calculate the inverse of the src-alpha: how much of the dst to keep.
  uint32_t ia = 0xFFFF - sa;

  // Composite src (nonpremul) over dst (premul).
  da = sa + ((da * ia) / 0xFFFF);
  dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
  dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
  db = ((sb * sa) + (db * ia)) / 0xFFFF;

  // Convert dst from premul to nonpremul.
  if (da != 0) {
    dr = (dr * 0xFFFF) / da;
    dg = (dg * 0xFFFF) / da;
    db = (db * 0xFFFF) / da;
  }

  // Convert from 16-bit color to 8-bit color.
  da >>= 8;
  dr >>= 8;
  dg >>= 8;
  db >>= 8;

  // Combine components.
  return (db << 0) | (dg << 8) | (dr << 16) | (da << 24);
}

static inline uint64_t  //
wuffs_private_impl__composite_nonpremul_nonpremul_u64_axxx(
    uint64_t dst_nonpremul,
    uint64_t src_nonpremul) {
  // Extract components.
  //
  // If the destination is transparent then SRC_OVER is equivalent to SRC: just
  // return src_nonpremul. This isn't just an optimization (skipping the rest
  // of the function's computation). It also preserves the nonpremul
  // distinction between e.g. transparent red and transparent blue that would
  // otherwise be lost by converting from nonpremul to premul and back.
  uint64_t da = 0xFFFF & (dst_nonpremul >> 48);
  if (da == 0) {
    return src_nonpremul;
  }
  uint64_t dr = 0xFFFF & (dst_nonpremul >> 32);
  uint64_t dg = 0xFFFF & (dst_nonpremul >> 16);
  uint64_t db = 0xFFFF & (dst_nonpremul >> 0);
  uint64_t sa = 0xFFFF & (src_nonpremul >> 48);
  uint64_t sr = 0xFFFF & (src_nonpremul >> 32);
  uint64_t sg = 0xFFFF & (src_nonpremul >> 16);
  uint64_t sb = 0xFFFF & (src_nonpremul >> 0);

  // Convert dst from nonpremul to premul.
  dr = (dr * da) / 0xFFFF;
  dg = (dg * da) / 0xFFFF;
  db = (db * da) / 0xFFFF;

  // Calculate the inverse of the src-alpha: how much of the dst to keep.
  uint64_t ia = 0xFFFF - sa;

  // Composite src (nonpremul) over dst (premul).
  da = sa + ((da * ia) / 0xFFFF);
  dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
  dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
  db = ((sb * sa) + (db * ia)) / 0xFFFF;

  // Convert dst from premul to nonpremul.
  if (da != 0) {
    dr = (dr * 0xFFFF) / da;
    dg = (dg * 0xFFFF) / da;
    db = (db * 0xFFFF) / da;
  }

  // Combine components.
  return (db << 0) | (dg << 16) | (dr << 32) | (da << 48);
}

static inline uint32_t  //
wuffs_private_impl__composite_nonpremul_premul_u32_axxx(uint32_t dst_nonpremul,
                                                        uint32_t src_premul) {
  // Extract 16-bit color components.
  uint32_t da = 0x101 * (0xFF & (dst_nonpremul >> 24));
  uint32_t dr = 0x101 * (0xFF & (dst_nonpremul >> 16));
  uint32_t dg = 0x101 * (0xFF & (dst_nonpremul >> 8));
  uint32_t db = 0x101 * (0xFF & (dst_nonpremul >> 0));
  uint32_t sa = 0x101 * (0xFF & (src_premul >> 24));
  uint32_t sr = 0x101 * (0xFF & (src_premul >> 16));
  uint32_t sg = 0x101 * (0xFF & (src_premul >> 8));
  uint32_t sb = 0x101 * (0xFF & (src_premul >> 0));

  // Convert dst from nonpremul to premul.
  dr = (dr * da) / 0xFFFF;
  dg = (dg * da) / 0xFFFF;
  db = (db * da) / 0xFFFF;

  // Calculate the inverse of the src-alpha: how much of the dst to keep.
  uint32_t ia = 0xFFFF - sa;

  // Composite src (premul) over dst (premul).
  da = sa + ((da * ia) / 0xFFFF);
  dr = sr + ((dr * ia) / 0xFFFF);
  dg = sg + ((dg * ia) / 0xFFFF);
  db = sb + ((db * ia) / 0xFFFF);

  // Convert dst from premul to nonpremul.
  if (da != 0) {
    dr = (dr * 0xFFFF) / da;
    dg = (dg * 0xFFFF) / da;
    db = (db * 0xFFFF) / da;
  }

  // Convert from 16-bit color to 8-bit color.
  da >>= 8;
  dr >>= 8;
  dg >>= 8;
  db >>= 8;

  // Combine components.
  return (db << 0) | (dg << 8) | (dr << 16) | (da << 24);
}

static inline uint64_t  //
wuffs_private_impl__composite_nonpremul_premul_u64_axxx(uint64_t dst_nonpremul,
                                                        uint64_t src_premul) {
  // Extract components.
  uint64_t da = 0xFFFF & (dst_nonpremul >> 48);
  uint64_t dr = 0xFFFF & (dst_nonpremul >> 32);
  uint64_t dg = 0xFFFF & (dst_nonpremul >> 16);
  uint64_t db = 0xFFFF & (dst_nonpremul >> 0);
  uint64_t sa = 0xFFFF & (src_premul >> 48);
  uint64_t sr = 0xFFFF & (src_premul >> 32);
  uint64_t sg = 0xFFFF & (src_premul >> 16);
  uint64_t sb = 0xFFFF & (src_premul >> 0);

  // Convert dst from nonpremul to premul.
  dr = (dr * da) / 0xFFFF;
  dg = (dg * da) / 0xFFFF;
  db = (db * da) / 0xFFFF;

  // Calculate the inverse of the src-alpha: how much of the dst to keep.
  uint64_t ia = 0xFFFF - sa;

  // Composite src (premul) over dst (premul).
  da = sa + ((da * ia) / 0xFFFF);
  dr = sr + ((dr * ia) / 0xFFFF);
  dg = sg + ((dg * ia) / 0xFFFF);
  db = sb + ((db * ia) / 0xFFFF);

  // Convert dst from premul to nonpremul.
  if (da != 0) {
    dr = (dr * 0xFFFF) / da;
    dg = (dg * 0xFFFF) / da;
    db = (db * 0xFFFF) / da;
  }

  // Combine components.
  return (db << 0) | (dg << 16) | (dr << 32) | (da << 48);
}

static inline uint32_t  //
wuffs_private_impl__composite_premul_nonpremul_u32_axxx(
    uint32_t dst_premul,
    uint32_t src_nonpremul) {
  // Extract 16-bit color components.
  uint32_t da = 0x101 * (0xFF & (dst_premul >> 24));
  uint32_t dr = 0x101 * (0xFF & (dst_premul >> 16));
  uint32_t dg = 0x101 * (0xFF & (dst_premul >> 8));
  uint32_t db = 0x101 * (0xFF & (dst_premul >> 0));
  uint32_t sa = 0x101 * (0xFF & (src_nonpremul >> 24));
  uint32_t sr = 0x101 * (0xFF & (src_nonpremul >> 16));
  uint32_t sg = 0x101 * (0xFF & (src_nonpremul >> 8));
  uint32_t sb = 0x101 * (0xFF & (src_nonpremul >> 0));

  // Calculate the inverse of the src-alpha: how much of the dst to keep.
  uint32_t ia = 0xFFFF - sa;

  // Composite src (nonpremul) over dst (premul).
  da = sa + ((da * ia) / 0xFFFF);
  dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
  dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
  db = ((sb * sa) + (db * ia)) / 0xFFFF;

  // Convert from 16-bit color to 8-bit color.
  da >>= 8;
  dr >>= 8;
  dg >>= 8;
  db >>= 8;

  // Combine components.
  return (db << 0) | (dg << 8) | (dr << 16) | (da << 24);
}

static inline uint64_t  //
wuffs_private_impl__composite_premul_nonpremul_u64_axxx(
    uint64_t dst_premul,
    uint64_t src_nonpremul) {
  // Extract components.
  uint64_t da = 0xFFFF & (dst_premul >> 48);
  uint64_t dr = 0xFFFF & (dst_premul >> 32);
  uint64_t dg = 0xFFFF & (dst_premul >> 16);
  uint64_t db = 0xFFFF & (dst_premul >> 0);
  uint64_t sa = 0xFFFF & (src_nonpremul >> 48);
  uint64_t sr = 0xFFFF & (src_nonpremul >> 32);
  uint64_t sg = 0xFFFF & (src_nonpremul >> 16);
  uint64_t sb = 0xFFFF & (src_nonpremul >> 0);

  // Calculate the inverse of the src-alpha: how much of the dst to keep.
  uint64_t ia = 0xFFFF - sa;

  // Composite src (nonpremul) over dst (premul).
  da = sa + ((da * ia) / 0xFFFF);
  dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
  dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
  db = ((sb * sa) + (db * ia)) / 0xFFFF;

  // Combine components.
  return (db << 0) | (dg << 16) | (dr << 32) | (da << 48);
}

static inline uint32_t  //
wuffs_private_impl__composite_premul_premul_u32_axxx(uint32_t dst_premul,
                                                     uint32_t src_premul) {
  // Extract 16-bit color components.
  uint32_t da = 0x101 * (0xFF & (dst_premul >> 24));
  uint32_t dr = 0x101 * (0xFF & (dst_premul >> 16));
  uint32_t dg = 0x101 * (0xFF & (dst_premul >> 8));
  uint32_t db = 0x101 * (0xFF & (dst_premul >> 0));
  uint32_t sa = 0x101 * (0xFF & (src_premul >> 24));
  uint32_t sr = 0x101 * (0xFF & (src_premul >> 16));
  uint32_t sg = 0x101 * (0xFF & (src_premul >> 8));
  uint32_t sb = 0x101 * (0xFF & (src_premul >> 0));

  // Calculate the inverse of the src-alpha: how much of the dst to keep.
  uint32_t ia = 0xFFFF - sa;

  // Composite src (premul) over dst (premul).
  da = sa + ((da * ia) / 0xFFFF);
  dr = sr + ((dr * ia) / 0xFFFF);
  dg = sg + ((dg * ia) / 0xFFFF);
  db = sb + ((db * ia) / 0xFFFF);

  // Convert from 16-bit color to 8-bit color.
  da >>= 8;
  dr >>= 8;
  dg >>= 8;
  db >>= 8;

  // Combine components.
  return (db << 0) | (dg << 8) | (dr << 16) | (da << 24);
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_squash_align4_bgr_565_8888(uint8_t* dst_ptr,
                                                       size_t dst_len,
                                                       const uint8_t* src_ptr,
                                                       size_t src_len,
                                                       bool nonpremul) {
  size_t len = (dst_len < src_len ? dst_len : src_len) / 4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n--) {
    uint32_t argb = wuffs_base__peek_u32le__no_bounds_check(s);
    if (nonpremul) {
      argb =
          wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(argb);
    }
    uint32_t b5 = 0x1F & (argb >> (8 - 5));
    uint32_t g6 = 0x3F & (argb >> (16 - 6));
    uint32_t r5 = 0x1F & (argb >> (24 - 5));
    uint32_t alpha = argb & 0xFF000000;
    wuffs_base__poke_u32le__no_bounds_check(
        d, alpha | (r5 << 11) | (g6 << 5) | (b5 << 0));
    s += 4;
    d += 4;
  }
  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_squash_align4_y_8888(uint8_t* dst_ptr,
                                                 size_t dst_len,
                                                 const uint8_t* src_ptr,
                                                 size_t src_len,
                                                 bool nonpremul) {
  size_t len = (dst_len < src_len ? dst_len : src_len) / 4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n--) {
    uint32_t argb = wuffs_base__peek_u32le__no_bounds_check(s);
    if (nonpremul) {
      argb =
          wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(argb);
    }
    uint32_t s0 = wuffs_base__color_u32_argb_premul__as__color_u8_gray(argb);
    wuffs_base__poke_u32le__no_bounds_check(
        d, (argb & 0xFF000000) | (s0 * 0x010101));
    s += 4;
    d += 4;
  }
  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_swap_rgb_bgr(uint8_t* dst_ptr,
                                         size_t dst_len,
                                         uint8_t* dst_palette_ptr,
                                         size_t dst_palette_len,
                                         const uint8_t* src_ptr,
                                         size_t src_len) {
  size_t len = (dst_len < src_len ? dst_len : src_len) / 3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n--) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    d[0] = s2;
    d[1] = s1;
    d[2] = s0;
    s += 3;
    d += 3;
  }
  return len;
}

// ‼ WUFFS MULTI-FILE SECTION +x86_sse42
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2")
static uint64_t  //
wuffs_private_impl__swizzle_swap_rgbx_bgrx__x86_sse42(uint8_t* dst_ptr,
                                                      size_t dst_len,
                                                      uint8_t* dst_palette_ptr,
                                                      size_t dst_palette_len,
                                                      const uint8_t* src_ptr,
                                                      size_t src_len) {
  size_t len = (dst_len < src_len ? dst_len : src_len) / 4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  __m128i shuffle = _mm_set_epi8(+0x0F, +0x0C, +0x0D, +0x0E,  //
                                 +0x0B, +0x08, +0x09, +0x0A,  //
                                 +0x07, +0x04, +0x05, +0x06,  //
                                 +0x03, +0x00, +0x01, +0x02);

  while (n >= 4) {
    __m128i x;
    x = _mm_lddqu_si128((const __m128i*)(const void*)s);
    x = _mm_shuffle_epi8(x, shuffle);
    _mm_storeu_si128((__m128i*)(void*)d, x);

    s += 4 * 4;
    d += 4 * 4;
    n -= 4;
  }

  while (n--) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    uint8_t s3 = s[3];
    d[0] = s2;
    d[1] = s1;
    d[2] = s0;
    d[3] = s3;
    s += 4;
    d += 4;
  }
  return len;
}
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
// ‼ WUFFS MULTI-FILE SECTION -x86_sse42

static uint64_t  //
wuffs_private_impl__swizzle_swap_rgbx_bgrx(uint8_t* dst_ptr,
                                           size_t dst_len,
                                           uint8_t* dst_palette_ptr,
                                           size_t dst_palette_len,
                                           const uint8_t* src_ptr,
                                           size_t src_len) {
  size_t len = (dst_len < src_len ? dst_len : src_len) / 4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n--) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    uint8_t s3 = s[3];
    d[0] = s2;
    d[1] = s1;
    d[2] = s0;
    d[3] = s3;
    s += 4;
    d += 4;
  }
  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_copy_1_1(uint8_t* dst_ptr,
                                     size_t dst_len,
                                     uint8_t* dst_palette_ptr,
                                     size_t dst_palette_len,
                                     const uint8_t* src_ptr,
                                     size_t src_len) {
  size_t len = (dst_len < src_len) ? dst_len : src_len;
  if (len > 0) {
    memmove(dst_ptr, src_ptr, len);
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_copy_2_2(uint8_t* dst_ptr,
                                     size_t dst_len,
                                     uint8_t* dst_palette_ptr,
                                     size_t dst_palette_len,
                                     const uint8_t* src_ptr,
                                     size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len2 < src_len2) ? dst_len2 : src_len2;
  if (len > 0) {
    memmove(dst_ptr, src_ptr, len * 2);
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_copy_3_3(uint8_t* dst_ptr,
                                     size_t dst_len,
                                     uint8_t* dst_palette_ptr,
                                     size_t dst_palette_len,
                                     const uint8_t* src_ptr,
                                     size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len3 < src_len3) ? dst_len3 : src_len3;
  if (len > 0) {
    memmove(dst_ptr, src_ptr, len * 3);
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_copy_4_4(uint8_t* dst_ptr,
                                     size_t dst_len,
                                     uint8_t* dst_palette_ptr,
                                     size_t dst_palette_len,
                                     const uint8_t* src_ptr,
                                     size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  if (len > 0) {
    memmove(dst_ptr, src_ptr, len * 4);
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_copy_8_8(uint8_t* dst_ptr,
                                     size_t dst_len,
                                     uint8_t* dst_palette_ptr,
                                     size_t dst_palette_len,
                                     const uint8_t* src_ptr,
                                     size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len8 < src_len8) ? dst_len8 : src_len8;
  if (len > 0) {
    memmove(dst_ptr, src_ptr, len * 8);
  }
  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__bgr(uint8_t* dst_ptr,
                                         size_t dst_len,
                                         uint8_t* dst_palette_ptr,
                                         size_t dst_palette_len,
                                         const uint8_t* src_ptr,
                                         size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len2 < src_len3) ? dst_len2 : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t b5 = (uint32_t)(s[0] >> 3);
    uint32_t g6 = (uint32_t)(s[1] >> 2);
    uint32_t r5 = (uint32_t)(s[2] >> 3);
    uint32_t rgb_565 = (r5 << 11) | (g6 << 5) | (b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)rgb_565);

    s += 1 * 3;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__bgrx(uint8_t* dst_ptr,
                                          size_t dst_len,
                                          uint8_t* dst_palette_ptr,
                                          size_t dst_palette_len,
                                          const uint8_t* src_ptr,
                                          size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t b5 = (uint32_t)(s[0] >> 3);
    uint32_t g6 = (uint32_t)(s[1] >> 2);
    uint32_t r5 = (uint32_t)(s[2] >> 3);
    uint32_t rgb_565 = (r5 << 11) | (g6 << 5) | (b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)rgb_565);

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__bgra_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2),
        wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(
            wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
                wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)))));

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__bgra_nonpremul_4x16le__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len2 < src_len8) ? dst_len2 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2),
        wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(
            wuffs_base__color_u64_argb_nonpremul__as__color_u32_argb_premul(
                wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8)))));

    s += 1 * 8;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sr = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sb = 0x101 * ((uint32_t)s[0]);

    // Convert from 565 color to 16-bit color.
    uint32_t old_rgb_565 = wuffs_base__peek_u16le__no_bounds_check(d + (0 * 2));
    uint32_t old_r5 = 0x1F & (old_rgb_565 >> 11);
    uint32_t dr = (0x8421 * old_r5) >> 4;
    uint32_t old_g6 = 0x3F & (old_rgb_565 >> 5);
    uint32_t dg = (0x1041 * old_g6) >> 2;
    uint32_t old_b5 = 0x1F & (old_rgb_565 >> 0);
    uint32_t db = (0x8421 * old_b5) >> 4;

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
    db = ((sb * sa) + (db * ia)) / 0xFFFF;

    // Convert from 16-bit color to 565 color and combine the components.
    uint32_t new_r5 = 0x1F & (dr >> 11);
    uint32_t new_g6 = 0x3F & (dg >> 10);
    uint32_t new_b5 = 0x1F & (db >> 11);
    uint32_t new_rgb_565 = (new_r5 << 11) | (new_g6 << 5) | (new_b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)new_rgb_565);

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__bgra_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len2 < src_len8) ? dst_len2 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t sa = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 6));
    uint32_t sr = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 4));
    uint32_t sg = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 2));
    uint32_t sb = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 0));

    // Convert from 565 color to 16-bit color.
    uint32_t old_rgb_565 = wuffs_base__peek_u16le__no_bounds_check(d + (0 * 2));
    uint32_t old_r5 = 0x1F & (old_rgb_565 >> 11);
    uint32_t dr = (0x8421 * old_r5) >> 4;
    uint32_t old_g6 = 0x3F & (old_rgb_565 >> 5);
    uint32_t dg = (0x1041 * old_g6) >> 2;
    uint32_t old_b5 = 0x1F & (old_rgb_565 >> 0);
    uint32_t db = (0x8421 * old_b5) >> 4;

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
    db = ((sb * sa) + (db * ia)) / 0xFFFF;

    // Convert from 16-bit color to 565 color and combine the components.
    uint32_t new_r5 = 0x1F & (dr >> 11);
    uint32_t new_g6 = 0x3F & (dg >> 10);
    uint32_t new_b5 = 0x1F & (db >> 11);
    uint32_t new_rgb_565 = (new_r5 << 11) | (new_g6 << 5) | (new_b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)new_rgb_565);

    s += 1 * 8;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__bgra_premul__src(uint8_t* dst_ptr,
                                                      size_t dst_len,
                                                      uint8_t* dst_palette_ptr,
                                                      size_t dst_palette_len,
                                                      const uint8_t* src_ptr,
                                                      size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2), wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(
                         wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4))));

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__bgra_premul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sr = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sb = 0x101 * ((uint32_t)s[0]);

    // Convert from 565 color to 16-bit color.
    uint32_t old_rgb_565 = wuffs_base__peek_u16le__no_bounds_check(d + (0 * 2));
    uint32_t old_r5 = 0x1F & (old_rgb_565 >> 11);
    uint32_t dr = (0x8421 * old_r5) >> 4;
    uint32_t old_g6 = 0x3F & (old_rgb_565 >> 5);
    uint32_t dg = (0x1041 * old_g6) >> 2;
    uint32_t old_b5 = 0x1F & (old_rgb_565 >> 0);
    uint32_t db = (0x8421 * old_b5) >> 4;

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (premul) over dst (premul).
    dr = sr + ((dr * ia) / 0xFFFF);
    dg = sg + ((dg * ia) / 0xFFFF);
    db = sb + ((db * ia) / 0xFFFF);

    // Convert from 16-bit color to 565 color and combine the components.
    uint32_t new_r5 = 0x1F & (dr >> 11);
    uint32_t new_g6 = 0x3F & (dg >> 10);
    uint32_t new_b5 = 0x1F & (db >> 11);
    uint32_t new_rgb_565 = (new_r5 << 11) | (new_g6 << 5) | (new_b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)new_rgb_565);

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__rgb(uint8_t* dst_ptr,
                                         size_t dst_len,
                                         uint8_t* dst_palette_ptr,
                                         size_t dst_palette_len,
                                         const uint8_t* src_ptr,
                                         size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len2 < src_len3) ? dst_len2 : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t r5 = (uint32_t)(s[0] >> 3);
    uint32_t g6 = (uint32_t)(s[1] >> 2);
    uint32_t b5 = (uint32_t)(s[2] >> 3);
    uint32_t rgb_565 = (r5 << 11) | (g6 << 5) | (b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)rgb_565);

    s += 1 * 3;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__rgba_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2),
        wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(
            wuffs_private_impl__swap_u32_argb_abgr(
                wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
                    wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4))))));

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__rgba_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sb = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sr = 0x101 * ((uint32_t)s[0]);

    // Convert from 565 color to 16-bit color.
    uint32_t old_rgb_565 = wuffs_base__peek_u16le__no_bounds_check(d + (0 * 2));
    uint32_t old_r5 = 0x1F & (old_rgb_565 >> 11);
    uint32_t dr = (0x8421 * old_r5) >> 4;
    uint32_t old_g6 = 0x3F & (old_rgb_565 >> 5);
    uint32_t dg = (0x1041 * old_g6) >> 2;
    uint32_t old_b5 = 0x1F & (old_rgb_565 >> 0);
    uint32_t db = (0x8421 * old_b5) >> 4;

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
    db = ((sb * sa) + (db * ia)) / 0xFFFF;

    // Convert from 16-bit color to 565 color and combine the components.
    uint32_t new_r5 = 0x1F & (dr >> 11);
    uint32_t new_g6 = 0x3F & (dg >> 10);
    uint32_t new_b5 = 0x1F & (db >> 11);
    uint32_t new_rgb_565 = (new_r5 << 11) | (new_g6 << 5) | (new_b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)new_rgb_565);

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__rgba_premul__src(uint8_t* dst_ptr,
                                                      size_t dst_len,
                                                      uint8_t* dst_palette_ptr,
                                                      size_t dst_palette_len,
                                                      const uint8_t* src_ptr,
                                                      size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2),
        wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(
            wuffs_private_impl__swap_u32_argb_abgr(
                wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)))));

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__rgba_premul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len2 < src_len4) ? dst_len2 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sb = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sr = 0x101 * ((uint32_t)s[0]);

    // Convert from 565 color to 16-bit color.
    uint32_t old_rgb_565 = wuffs_base__peek_u16le__no_bounds_check(d + (0 * 2));
    uint32_t old_r5 = 0x1F & (old_rgb_565 >> 11);
    uint32_t dr = (0x8421 * old_r5) >> 4;
    uint32_t old_g6 = 0x3F & (old_rgb_565 >> 5);
    uint32_t dg = (0x1041 * old_g6) >> 2;
    uint32_t old_b5 = 0x1F & (old_rgb_565 >> 0);
    uint32_t db = (0x8421 * old_b5) >> 4;

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (premul) over dst (premul).
    dr = sr + ((dr * ia) / 0xFFFF);
    dg = sg + ((dg * ia) / 0xFFFF);
    db = sb + ((db * ia) / 0xFFFF);

    // Convert from 16-bit color to 565 color and combine the components.
    uint32_t new_r5 = 0x1F & (dr >> 11);
    uint32_t new_g6 = 0x3F & (dg >> 10);
    uint32_t new_b5 = 0x1F & (db >> 11);
    uint32_t new_rgb_565 = (new_r5 << 11) | (new_g6 << 5) | (new_b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)new_rgb_565);

    s += 1 * 4;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__y(uint8_t* dst_ptr,
                                       size_t dst_len,
                                       uint8_t* dst_palette_ptr,
                                       size_t dst_palette_len,
                                       const uint8_t* src_ptr,
                                       size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t len = (dst_len2 < src_len) ? dst_len2 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t y5 = (uint32_t)(s[0] >> 3);
    uint32_t y6 = (uint32_t)(s[0] >> 2);
    uint32_t rgb_565 = (y5 << 11) | (y6 << 5) | (y5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)rgb_565);

    s += 1 * 1;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__y_16be(uint8_t* dst_ptr,
                                            size_t dst_len,
                                            uint8_t* dst_palette_ptr,
                                            size_t dst_palette_len,
                                            const uint8_t* src_ptr,
                                            size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len2 < src_len2) ? dst_len2 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t y5 = (uint32_t)(s[0] >> 3);
    uint32_t y6 = (uint32_t)(s[0] >> 2);
    uint32_t rgb_565 = (y5 << 11) | (y6 << 5) | (y5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)rgb_565);

    s += 1 * 2;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__ya_nonpremul__src(uint8_t* dst_ptr,
                                                       size_t dst_len,
                                                       uint8_t* dst_palette_ptr,
                                                       size_t dst_palette_len,
                                                       const uint8_t* src_ptr,
                                                       size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len2 < src_len2) ? dst_len2 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);

    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2),
        wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(
            wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
                s0)));

    s += 1 * 2;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__ya_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len2 < src_len2) ? dst_len2 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t sa = 0x101 * ((uint32_t)s[1]);
    uint32_t sy = 0x101 * ((uint32_t)s[0]);

    // Convert from 565 color to 16-bit color.
    uint32_t old_rgb_565 = wuffs_base__peek_u16le__no_bounds_check(d + (0 * 2));
    uint32_t old_r5 = 0x1F & (old_rgb_565 >> 11);
    uint32_t dr = (0x8421 * old_r5) >> 4;
    uint32_t old_g6 = 0x3F & (old_rgb_565 >> 5);
    uint32_t dg = (0x1041 * old_g6) >> 2;
    uint32_t old_b5 = 0x1F & (old_rgb_565 >> 0);
    uint32_t db = (0x8421 * old_b5) >> 4;

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sy * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sy * sa) + (dg * ia)) / 0xFFFF;
    db = ((sy * sa) + (db * ia)) / 0xFFFF;

    // Convert from 16-bit color to 565 color and combine the components.
    uint32_t new_r5 = 0x1F & (dr >> 11);
    uint32_t new_g6 = 0x3F & (dg >> 10);
    uint32_t new_b5 = 0x1F & (db >> 11);
    uint32_t new_rgb_565 = (new_r5 << 11) | (new_g6 << 5) | (new_b5 << 0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)new_rgb_565);

    s += 1 * 2;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__index__src(uint8_t* dst_ptr,
                                                size_t dst_len,
                                                uint8_t* dst_palette_ptr,
                                                size_t dst_palette_len,
                                                const uint8_t* src_ptr,
                                                size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len2 = dst_len / 2;
  size_t len = (dst_len2 < src_len) ? dst_len2 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  const size_t loop_unroll_count = 4;

  while (n >= loop_unroll_count) {
    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2), wuffs_base__peek_u16le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[0] * 4)));
    wuffs_base__poke_u16le__no_bounds_check(
        d + (1 * 2), wuffs_base__peek_u16le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[1] * 4)));
    wuffs_base__poke_u16le__no_bounds_check(
        d + (2 * 2), wuffs_base__peek_u16le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[2] * 4)));
    wuffs_base__poke_u16le__no_bounds_check(
        d + (3 * 2), wuffs_base__peek_u16le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[3] * 4)));

    s += loop_unroll_count * 1;
    d += loop_unroll_count * 2;
    n -= loop_unroll_count;
  }

  while (n >= 1) {
    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2), wuffs_base__peek_u16le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[0] * 4)));

    s += 1 * 1;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__index_bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len2 = dst_len / 2;
  size_t len = (dst_len2 < src_len) ? dst_len2 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = wuffs_base__color_u16_rgb_565__as__color_u32_argb_premul(
        wuffs_base__peek_u16le__no_bounds_check(d + (0 * 2)));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    wuffs_base__poke_u16le__no_bounds_check(
        d + (0 * 2),
        wuffs_base__color_u32_argb_premul__as__color_u16_rgb_565(
            wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0)));

    s += 1 * 1;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr_565__index_binary_alpha__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len2 = dst_len / 2;
  size_t len = (dst_len2 < src_len) ? dst_len2 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    if (s0) {
      wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)s0);
    }

    s += 1 * 1;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_bgr__bgr_565(uint8_t* dst_ptr,
                                         size_t dst_len,
                                         uint8_t* dst_palette_ptr,
                                         size_t dst_palette_len,
                                         const uint8_t* src_ptr,
                                         size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len3 < src_len2) ? dst_len3 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_base__color_u16_rgb_565__as__color_u32_argb_premul(
        wuffs_base__peek_u16le__no_bounds_check(s + (0 * 2)));
    wuffs_base__poke_u24le__no_bounds_check(d + (0 * 3), s0);

    s += 1 * 2;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__bgra_nonpremul__src(uint8_t* dst_ptr,
                                                     size_t dst_len,
                                                     uint8_t* dst_palette_ptr,
                                                     size_t dst_palette_len,
                                                     const uint8_t* src_ptr,
                                                     size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 =
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
            wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u24le__no_bounds_check(d + (0 * 3), s0);

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__bgra_nonpremul_4x16le__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len3 < src_len8) ? dst_len3 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 =
        wuffs_base__color_u64_argb_nonpremul__as__color_u32_argb_premul(
            wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8)));
    wuffs_base__poke_u24le__no_bounds_check(d + (0 * 3), s0);

    s += 1 * 8;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t dr = 0x101 * ((uint32_t)d[2]);
    uint32_t dg = 0x101 * ((uint32_t)d[1]);
    uint32_t db = 0x101 * ((uint32_t)d[0]);
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sr = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sb = 0x101 * ((uint32_t)s[0]);

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
    db = ((sb * sa) + (db * ia)) / 0xFFFF;

    // Convert from 16-bit color to 8-bit color.
    d[0] = (uint8_t)(db >> 8);
    d[1] = (uint8_t)(dg >> 8);
    d[2] = (uint8_t)(dr >> 8);

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__bgra_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len3 < src_len8) ? dst_len3 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t dr = 0x101 * ((uint32_t)d[2]);
    uint32_t dg = 0x101 * ((uint32_t)d[1]);
    uint32_t db = 0x101 * ((uint32_t)d[0]);
    uint32_t sa = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 6));
    uint32_t sr = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 4));
    uint32_t sg = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 2));
    uint32_t sb = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 0));

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
    db = ((sb * sa) + (db * ia)) / 0xFFFF;

    // Convert from 16-bit color to 8-bit color.
    d[0] = (uint8_t)(db >> 8);
    d[1] = (uint8_t)(dg >> 8);
    d[2] = (uint8_t)(dr >> 8);

    s += 1 * 8;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__bgra_premul__src(uint8_t* dst_ptr,
                                                  size_t dst_len,
                                                  uint8_t* dst_palette_ptr,
                                                  size_t dst_palette_len,
                                                  const uint8_t* src_ptr,
                                                  size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    d[0] = s0;
    d[1] = s1;
    d[2] = s2;

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__bgra_premul__src_over(uint8_t* dst_ptr,
                                                       size_t dst_len,
                                                       uint8_t* dst_palette_ptr,
                                                       size_t dst_palette_len,
                                                       const uint8_t* src_ptr,
                                                       size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t dr = 0x101 * ((uint32_t)d[2]);
    uint32_t dg = 0x101 * ((uint32_t)d[1]);
    uint32_t db = 0x101 * ((uint32_t)d[0]);
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sr = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sb = 0x101 * ((uint32_t)s[0]);

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (premul) over dst (premul).
    dr = sr + ((dr * ia) / 0xFFFF);
    dg = sg + ((dg * ia) / 0xFFFF);
    db = sb + ((db * ia) / 0xFFFF);

    // Convert from 16-bit color to 8-bit color.
    d[0] = (uint8_t)(db >> 8);
    d[1] = (uint8_t)(dg >> 8);
    d[2] = (uint8_t)(dr >> 8);

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__rgba_nonpremul__src(uint8_t* dst_ptr,
                                                     size_t dst_len,
                                                     uint8_t* dst_palette_ptr,
                                                     size_t dst_palette_len,
                                                     const uint8_t* src_ptr,
                                                     size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
            wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4))));
    wuffs_base__poke_u24le__no_bounds_check(d + (0 * 3), s0);

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__rgba_nonpremul_4x16le__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len3 < src_len8) ? dst_len3 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__color_u64_argb_nonpremul__as__color_u32_argb_premul(
            wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8))));
    wuffs_base__poke_u24le__no_bounds_check(d + (0 * 3), s0);

    s += 1 * 8;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__rgba_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t dr = 0x101 * ((uint32_t)d[2]);
    uint32_t dg = 0x101 * ((uint32_t)d[1]);
    uint32_t db = 0x101 * ((uint32_t)d[0]);
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sb = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sr = 0x101 * ((uint32_t)s[0]);

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
    db = ((sb * sa) + (db * ia)) / 0xFFFF;

    // Convert from 16-bit color to 8-bit color.
    d[0] = (uint8_t)(db >> 8);
    d[1] = (uint8_t)(dg >> 8);
    d[2] = (uint8_t)(dr >> 8);

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__rgba_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len3 < src_len8) ? dst_len3 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t dr = 0x101 * ((uint32_t)d[2]);
    uint32_t dg = 0x101 * ((uint32_t)d[1]);
    uint32_t db = 0x101 * ((uint32_t)d[0]);
    uint32_t sa = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 6));
    uint32_t sb = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 4));
    uint32_t sg = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 2));
    uint32_t sr = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 0));

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
    db = ((sb * sa) + (db * ia)) / 0xFFFF;

    // Convert from 16-bit color to 8-bit color.
    d[0] = (uint8_t)(db >> 8);
    d[1] = (uint8_t)(dg >> 8);
    d[2] = (uint8_t)(dr >> 8);

    s += 1 * 8;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__rgba_premul__src(uint8_t* dst_ptr,
                                                  size_t dst_len,
                                                  uint8_t* dst_palette_ptr,
                                                  size_t dst_palette_len,
                                                  const uint8_t* src_ptr,
                                                  size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    d[0] = s2;
    d[1] = s1;
    d[2] = s0;

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__rgba_premul__src_over(uint8_t* dst_ptr,
                                                       size_t dst_len,
                                                       uint8_t* dst_palette_ptr,
                                                       size_t dst_palette_len,
                                                       const uint8_t* src_ptr,
                                                       size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t dr = 0x101 * ((uint32_t)d[2]);
    uint32_t dg = 0x101 * ((uint32_t)d[1]);
    uint32_t db = 0x101 * ((uint32_t)d[0]);
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sb = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sr = 0x101 * ((uint32_t)s[0]);

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (premul) over dst (premul).
    dr = sr + ((dr * ia) / 0xFFFF);
    dg = sg + ((dg * ia) / 0xFFFF);
    db = sb + ((db * ia) / 0xFFFF);

    // Convert from 16-bit color to 8-bit color.
    d[0] = (uint8_t)(db >> 8);
    d[1] = (uint8_t)(dg >> 8);
    d[2] = (uint8_t)(dr >> 8);

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgr__rgbx(uint8_t* dst_ptr,
                                      size_t dst_len,
                                      uint8_t* dst_palette_ptr,
                                      size_t dst_palette_len,
                                      const uint8_t* src_ptr,
                                      size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint8_t b0 = s[0];
    uint8_t b1 = s[1];
    uint8_t b2 = s[2];
    d[0] = b2;
    d[1] = b1;
    d[2] = b0;

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_nonpremul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__bgra_nonpremul_4x16le__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len4 < src_len8) ? dst_len4 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4), wuffs_base__color_u64__as__color_u32(
                         wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8))));

    s += 1 * 8;
    d += 1 * 4;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__bgra_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len4 < src_len8) ? dst_len4 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint64_t d0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4)));
    uint64_t s0 = wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u64__as__color_u32(
            wuffs_private_impl__composite_nonpremul_nonpremul_u64_axxx(d0,
                                                                       s0)));

    s += 1 * 8;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__bgra_premul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__bgra_premul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_nonpremul_premul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__index_bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len4 = dst_len / 4;
  size_t len = (dst_len4 < src_len) ? dst_len4 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_nonpremul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 1;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__rgba_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_nonpremul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__rgba_premul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__rgba_premul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_nonpremul_premul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__ya_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len4 < src_len2) ? dst_len4 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    wuffs_base__poke_u32le__no_bounds_check(d + (0 * 4), s0);

    s += 1 * 2;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul__ya_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len4 < src_len2) ? dst_len4 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_nonpremul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 2;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    uint8_t s3 = s[3];
    d[0] = s0;
    d[1] = s0;
    d[2] = s1;
    d[3] = s1;
    d[4] = s2;
    d[5] = s2;
    d[6] = s3;
    d[7] = s3;

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t d0 = wuffs_base__peek_u64le__no_bounds_check(d + (0 * 8));
    uint64_t s0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8),
        wuffs_private_impl__composite_nonpremul_nonpremul_u64_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len8 < src_len8) ? dst_len8 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t d0 = wuffs_base__peek_u64le__no_bounds_check(d + (0 * 8));
    uint64_t s0 = wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8));
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8),
        wuffs_private_impl__composite_nonpremul_nonpremul_u64_axxx(d0, s0));

    s += 1 * 8;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_premul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t s0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(
            wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4))));
    wuffs_base__poke_u64le__no_bounds_check(d + (0 * 8), s0);

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_premul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t d0 = wuffs_base__peek_u64le__no_bounds_check(d + (0 * 8));
    uint64_t s0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8),
        wuffs_private_impl__composite_nonpremul_premul_u64_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__index_bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len8 = dst_len / 8;
  size_t len = (dst_len8 < src_len) ? dst_len8 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint64_t d0 = wuffs_base__peek_u64le__no_bounds_check(d + (0 * 8));
    uint64_t s0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                ((size_t)s[0] * 4)));
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8),
        wuffs_private_impl__composite_nonpremul_nonpremul_u64_axxx(d0, s0));

    s += 1 * 1;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__rgba_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    uint8_t s3 = s[3];
    d[0] = s2;
    d[1] = s2;
    d[2] = s1;
    d[3] = s1;
    d[4] = s0;
    d[5] = s0;
    d[6] = s3;
    d[7] = s3;

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__rgba_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t d0 = wuffs_base__peek_u64le__no_bounds_check(d + (0 * 8));
    uint64_t s0 = wuffs_base__color_u32__as__color_u64(
        wuffs_private_impl__swap_u32_argb_abgr(
            wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4))));
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8),
        wuffs_private_impl__composite_nonpremul_nonpremul_u64_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__rgba_premul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t s0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(
            wuffs_private_impl__swap_u32_argb_abgr(
                wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)))));
    wuffs_base__poke_u64le__no_bounds_check(d + (0 * 8), s0);

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__rgba_premul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t d0 = wuffs_base__peek_u64le__no_bounds_check(d + (0 * 8));
    uint64_t s0 = wuffs_base__color_u32__as__color_u64(
        wuffs_private_impl__swap_u32_argb_abgr(
            wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4))));
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8),
        wuffs_private_impl__composite_nonpremul_premul_u64_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__ya_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len8 < src_len2) ? dst_len8 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t s0 = ((uint64_t)(s[1]) * 0x0101000000000000) |
                  ((uint64_t)(s[0]) * 0x0000010101010101);
    wuffs_base__poke_u64le__no_bounds_check(d + (0 * 8), s0);

    s += 1 * 2;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__ya_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len8 < src_len2) ? dst_len8 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    uint64_t d0 = wuffs_base__peek_u64le__no_bounds_check(d + (0 * 8));
    uint64_t s0 = ((uint64_t)(s[1]) * 0x0101000000000000) |
                  ((uint64_t)(s[0]) * 0x0000010101010101);
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8),
        wuffs_private_impl__composite_nonpremul_nonpremul_u64_axxx(d0, s0));

    s += 1 * 2;
    d += 1 * 8;
    n -= 1;
  }
  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul_4x16le__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len4 < src_len8) ? dst_len4 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint64_t s0 = wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u64_argb_nonpremul__as__color_u32_argb_premul(s0));

    s += 1 * 8;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len4 < src_len8) ? dst_len4 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint64_t d0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4)));
    uint64_t s0 = wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u64__as__color_u32(
            wuffs_private_impl__composite_premul_nonpremul_u64_axxx(d0, s0)));

    s += 1 * 8;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__bgra_premul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_premul_premul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__index_bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len4 = dst_len / 4;
  size_t len = (dst_len4 < src_len) ? dst_len4 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 1;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul_4x16le__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len4 < src_len8) ? dst_len4 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint64_t s0 = wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__swap_u32_argb_abgr(
            wuffs_base__color_u64_argb_nonpremul__as__color_u32_argb_premul(
                s0)));

    s += 1 * 8;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len4 < src_len8) ? dst_len4 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint64_t d0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4)));
    uint64_t s0 = wuffs_private_impl__swap_u64_argb_abgr(
        wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u64__as__color_u32(
            wuffs_private_impl__composite_premul_nonpremul_u64_axxx(d0, s0)));

    s += 1 * 8;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__rgba_premul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_premul_premul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__ya_nonpremul__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len4 < src_len2) ? dst_len4 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(s0));

    s += 1 * 2;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgra_premul__ya_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len4 < src_len2) ? dst_len4 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4));
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 2;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_bgrw__bgr(uint8_t* dst_ptr,
                                      size_t dst_len,
                                      uint8_t* dst_palette_ptr,
                                      size_t dst_palette_len,
                                      const uint8_t* src_ptr,
                                      size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len4 < src_len3) ? dst_len4 : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        0xFF000000 | wuffs_base__peek_u24le__no_bounds_check(s + (0 * 3)));

    s += 1 * 3;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgrw__bgr_565(uint8_t* dst_ptr,
                                          size_t dst_len,
                                          uint8_t* dst_palette_ptr,
                                          size_t dst_palette_len,
                                          const uint8_t* src_ptr,
                                          size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len4 < src_len2) ? dst_len4 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4), wuffs_base__color_u16_rgb_565__as__color_u32_argb_premul(
                         wuffs_base__peek_u16le__no_bounds_check(s + (0 * 2))));

    s += 1 * 2;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgrw__bgrx(uint8_t* dst_ptr,
                                       size_t dst_len,
                                       uint8_t* dst_palette_ptr,
                                       size_t dst_palette_len,
                                       const uint8_t* src_ptr,
                                       size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        0xFF000000 | wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

// ‼ WUFFS MULTI-FILE SECTION +x86_sse42
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2")
static uint64_t  //
wuffs_private_impl__swizzle_bgrw__bgr__x86_sse42(uint8_t* dst_ptr,
                                                 size_t dst_len,
                                                 uint8_t* dst_palette_ptr,
                                                 size_t dst_palette_len,
                                                 const uint8_t* src_ptr,
                                                 size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len4 < src_len3) ? dst_len4 : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  __m128i shuffle = _mm_set_epi8(+0x00, +0x0B, +0x0A, +0x09,  //
                                 +0x00, +0x08, +0x07, +0x06,  //
                                 +0x00, +0x05, +0x04, +0x03,  //
                                 +0x00, +0x02, +0x01, +0x00);
  __m128i or_ff = _mm_set_epi8(-0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00);

  while (n >= 6) {
    __m128i x;
    x = _mm_lddqu_si128((const __m128i*)(const void*)s);
    x = _mm_shuffle_epi8(x, shuffle);
    x = _mm_or_si128(x, or_ff);
    _mm_storeu_si128((__m128i*)(void*)d, x);

    s += 4 * 3;
    d += 4 * 4;
    n -= 4;
  }

  while (n >= 1) {
    uint8_t b0 = s[0];
    uint8_t b1 = s[1];
    uint8_t b2 = s[2];
    d[0] = b0;
    d[1] = b1;
    d[2] = b2;
    d[3] = 0xFF;

    s += 1 * 3;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2")
static uint64_t  //
wuffs_private_impl__swizzle_bgrw__rgb__x86_sse42(uint8_t* dst_ptr,
                                                 size_t dst_len,
                                                 uint8_t* dst_palette_ptr,
                                                 size_t dst_palette_len,
                                                 const uint8_t* src_ptr,
                                                 size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len4 < src_len3) ? dst_len4 : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  __m128i shuffle = _mm_set_epi8(+0x00, +0x09, +0x0A, +0x0B,  //
                                 +0x00, +0x06, +0x07, +0x08,  //
                                 +0x00, +0x03, +0x04, +0x05,  //
                                 +0x00, +0x00, +0x01, +0x02);
  __m128i or_ff = _mm_set_epi8(-0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00);

  while (n >= 6) {
    __m128i x;
    x = _mm_lddqu_si128((const __m128i*)(const void*)s);
    x = _mm_shuffle_epi8(x, shuffle);
    x = _mm_or_si128(x, or_ff);
    _mm_storeu_si128((__m128i*)(void*)d, x);

    s += 4 * 3;
    d += 4 * 4;
    n -= 4;
  }

  while (n >= 1) {
    uint8_t b0 = s[0];
    uint8_t b1 = s[1];
    uint8_t b2 = s[2];
    d[0] = b2;
    d[1] = b1;
    d[2] = b0;
    d[3] = 0xFF;

    s += 1 * 3;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
// ‼ WUFFS MULTI-FILE SECTION -x86_sse42

static uint64_t  //
wuffs_private_impl__swizzle_bgrw__rgb(uint8_t* dst_ptr,
                                      size_t dst_len,
                                      uint8_t* dst_palette_ptr,
                                      size_t dst_palette_len,
                                      const uint8_t* src_ptr,
                                      size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len4 < src_len3) ? dst_len4 : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint8_t b0 = s[0];
    uint8_t b1 = s[1];
    uint8_t b2 = s[2];
    d[0] = b2;
    d[1] = b1;
    d[2] = b0;
    d[3] = 0xFF;

    s += 1 * 3;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgrw__rgbx(uint8_t* dst_ptr,
                                       size_t dst_len,
                                       uint8_t* dst_palette_ptr,
                                       size_t dst_palette_len,
                                       const uint8_t* src_ptr,
                                       size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len4 < src_len4) ? dst_len4 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint8_t b0 = s[0];
    uint8_t b1 = s[1];
    uint8_t b2 = s[2];
    d[0] = b2;
    d[1] = b1;
    d[2] = b0;
    d[3] = 0xFF;

    s += 1 * 4;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_bgrw_4x16le__bgr(uint8_t* dst_ptr,
                                             size_t dst_len,
                                             uint8_t* dst_palette_ptr,
                                             size_t dst_palette_len,
                                             const uint8_t* src_ptr,
                                             size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len8 < src_len3) ? dst_len8 : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    d[0] = s0;
    d[1] = s0;
    d[2] = s1;
    d[3] = s1;
    d[4] = s2;
    d[5] = s2;
    d[6] = 0xFF;
    d[7] = 0xFF;

    s += 1 * 3;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgrw_4x16le__bgr_565(uint8_t* dst_ptr,
                                                 size_t dst_len,
                                                 uint8_t* dst_palette_ptr,
                                                 size_t dst_palette_len,
                                                 const uint8_t* src_ptr,
                                                 size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len8 < src_len2) ? dst_len8 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8),
        wuffs_base__color_u32__as__color_u64(
            wuffs_base__color_u16_rgb_565__as__color_u32_argb_premul(
                wuffs_base__peek_u16le__no_bounds_check(s + (0 * 2)))));

    s += 1 * 2;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgrw_4x16le__bgrx(uint8_t* dst_ptr,
                                              size_t dst_len,
                                              uint8_t* dst_palette_ptr,
                                              size_t dst_palette_len,
                                              const uint8_t* src_ptr,
                                              size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len8 < src_len4) ? dst_len8 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    d[0] = s0;
    d[1] = s0;
    d[2] = s1;
    d[3] = s1;
    d[4] = s2;
    d[5] = s2;
    d[6] = 0xFF;
    d[7] = 0xFF;

    s += 1 * 4;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_bgrw_4x16le__rgb(uint8_t* dst_ptr,
                                             size_t dst_len,
                                             uint8_t* dst_palette_ptr,
                                             size_t dst_palette_len,
                                             const uint8_t* src_ptr,
                                             size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len8 < src_len3) ? dst_len8 : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    uint8_t s2 = s[2];
    d[0] = s2;
    d[1] = s2;
    d[2] = s1;
    d[3] = s1;
    d[4] = s0;
    d[5] = s0;
    d[6] = 0xFF;
    d[7] = 0xFF;

    s += 1 * 3;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_rgb__bgr_565(uint8_t* dst_ptr,
                                         size_t dst_len,
                                         uint8_t* dst_palette_ptr,
                                         size_t dst_palette_len,
                                         const uint8_t* src_ptr,
                                         size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len3 < src_len2) ? dst_len3 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u24le__no_bounds_check(
        d + (0 * 3),
        wuffs_private_impl__swap_u32_argb_abgr(
            wuffs_base__color_u16_rgb_565__as__color_u32_argb_premul(
                wuffs_base__peek_u16le__no_bounds_check(s + (0 * 2)))));

    s += 1 * 2;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_rgba_nonpremul__bgra_nonpremul_4x16le__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len4 < src_len8) ? dst_len4 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;

  size_t n = len;
  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__color_u64__as__color_u32__swap_u32_argb_abgr(
            wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8))));

    s += 1 * 8;
    d += 1 * 4;
    n -= 1;
  }
  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_rgba_nonpremul__bgra_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len4 < src_len8) ? dst_len4 : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint64_t d0 = wuffs_base__color_u32__as__color_u64(
        wuffs_base__peek_u32le__no_bounds_check(d + (0 * 4)));
    uint64_t s0 = wuffs_private_impl__swap_u64_argb_abgr(
        wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_base__color_u64__as__color_u32(
            wuffs_private_impl__composite_nonpremul_nonpremul_u64_axxx(d0,
                                                                       s0)));

    s += 1 * 8;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_rgbw__bgr_565(uint8_t* dst_ptr,
                                          size_t dst_len,
                                          uint8_t* dst_palette_ptr,
                                          size_t dst_palette_len,
                                          const uint8_t* src_ptr,
                                          size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len4 < src_len2) ? dst_len4 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4),
        wuffs_private_impl__swap_u32_argb_abgr(
            wuffs_base__color_u16_rgb_565__as__color_u32_argb_premul(
                wuffs_base__peek_u16le__no_bounds_check(s + (0 * 2)))));

    s += 1 * 2;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_xxx__index__src(uint8_t* dst_ptr,
                                            size_t dst_len,
                                            uint8_t* dst_palette_ptr,
                                            size_t dst_palette_len,
                                            const uint8_t* src_ptr,
                                            size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len3 = dst_len / 3;
  size_t len = (dst_len3 < src_len) ? dst_len3 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  const size_t loop_unroll_count = 4;

  // The comparison in the while condition is ">", not ">=", because with
  // ">=", the last 4-byte store could write past the end of the dst slice.
  //
  // Each 4-byte store writes one too many bytes, but a subsequent store
  // will overwrite that with the correct byte. There is always another
  // store, whether a 4-byte store in this loop or a 1-byte store in the
  // next loop.
  while (n > loop_unroll_count) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 3), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[0] * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (1 * 3), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[1] * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (2 * 3), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[2] * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (3 * 3), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[3] * 4)));

    s += loop_unroll_count * 1;
    d += loop_unroll_count * 3;
    n -= loop_unroll_count;
  }

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    wuffs_base__poke_u24le__no_bounds_check(d + (0 * 3), s0);

    s += 1 * 1;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxx__index_bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len3 = dst_len / 3;
  size_t len = (dst_len3 < src_len) ? dst_len3 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 =
        wuffs_base__peek_u24le__no_bounds_check(d + (0 * 3)) | 0xFF000000;
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    wuffs_base__poke_u24le__no_bounds_check(
        d + (0 * 3),
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 1;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxx__index_binary_alpha__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len3 = dst_len / 3;
  size_t len = (dst_len3 < src_len) ? dst_len3 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  const size_t loop_unroll_count = 4;

  while (n >= loop_unroll_count) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    if (s0) {
      wuffs_base__poke_u24le__no_bounds_check(d + (0 * 3), s0);
    }
    uint32_t s1 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[1] * 4));
    if (s1) {
      wuffs_base__poke_u24le__no_bounds_check(d + (1 * 3), s1);
    }
    uint32_t s2 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[2] * 4));
    if (s2) {
      wuffs_base__poke_u24le__no_bounds_check(d + (2 * 3), s2);
    }
    uint32_t s3 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[3] * 4));
    if (s3) {
      wuffs_base__poke_u24le__no_bounds_check(d + (3 * 3), s3);
    }

    s += loop_unroll_count * 1;
    d += loop_unroll_count * 3;
    n -= loop_unroll_count;
  }

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    if (s0) {
      wuffs_base__poke_u24le__no_bounds_check(d + (0 * 3), s0);
    }

    s += 1 * 1;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxx__xxxx(uint8_t* dst_ptr,
                                      size_t dst_len,
                                      uint8_t* dst_palette_ptr,
                                      size_t dst_palette_len,
                                      const uint8_t* src_ptr,
                                      size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len3 < src_len4) ? dst_len3 : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    wuffs_base__poke_u24le__no_bounds_check(
        d + (0 * 3), wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));

    s += 1 * 4;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxx__y(uint8_t* dst_ptr,
                                   size_t dst_len,
                                   uint8_t* dst_palette_ptr,
                                   size_t dst_palette_len,
                                   const uint8_t* src_ptr,
                                   size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t len = (dst_len3 < src_len) ? dst_len3 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint8_t s0 = s[0];
    d[0] = s0;
    d[1] = s0;
    d[2] = s0;

    s += 1 * 1;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxx__y_16be(uint8_t* dst_ptr,
                                        size_t dst_len,
                                        uint8_t* dst_palette_ptr,
                                        size_t dst_palette_len,
                                        const uint8_t* src_ptr,
                                        size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len3 < src_len2) ? dst_len3 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint8_t s0 = s[0];
    d[0] = s0;
    d[1] = s0;
    d[2] = s0;

    s += 1 * 2;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxx__ya_nonpremul__src(uint8_t* dst_ptr,
                                                   size_t dst_len,
                                                   uint8_t* dst_palette_ptr,
                                                   size_t dst_palette_len,
                                                   const uint8_t* src_ptr,
                                                   size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len3 < src_len2) ? dst_len3 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    wuffs_base__poke_u24le__no_bounds_check(
        d + (0 * 3),
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(s0));

    s += 1 * 2;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxx__ya_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len3 = dst_len / 3;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len3 < src_len2) ? dst_len3 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 =
        wuffs_base__peek_u24le__no_bounds_check(d + (0 * 3)) | 0xFF000000;
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    wuffs_base__poke_u24le__no_bounds_check(
        d + (0 * 3),
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 2;
    d += 1 * 3;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_xxxx__index__src(uint8_t* dst_ptr,
                                             size_t dst_len,
                                             uint8_t* dst_palette_ptr,
                                             size_t dst_palette_len,
                                             const uint8_t* src_ptr,
                                             size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len4 = dst_len / 4;
  size_t len = (dst_len4 < src_len) ? dst_len4 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  const size_t loop_unroll_count = 4;

  while (n >= loop_unroll_count) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[0] * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (1 * 4), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[1] * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (2 * 4), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[2] * 4)));
    wuffs_base__poke_u32le__no_bounds_check(
        d + (3 * 4), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[3] * 4)));

    s += loop_unroll_count * 1;
    d += loop_unroll_count * 4;
    n -= loop_unroll_count;
  }

  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4), wuffs_base__peek_u32le__no_bounds_check(
                         dst_palette_ptr + ((size_t)s[0] * 4)));

    s += 1 * 1;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxxx__index_binary_alpha__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len4 = dst_len / 4;
  size_t len = (dst_len4 < src_len) ? dst_len4 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  const size_t loop_unroll_count = 4;

  while (n >= loop_unroll_count) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    if (s0) {
      wuffs_base__poke_u32le__no_bounds_check(d + (0 * 4), s0);
    }
    uint32_t s1 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[1] * 4));
    if (s1) {
      wuffs_base__poke_u32le__no_bounds_check(d + (1 * 4), s1);
    }
    uint32_t s2 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[2] * 4));
    if (s2) {
      wuffs_base__poke_u32le__no_bounds_check(d + (2 * 4), s2);
    }
    uint32_t s3 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[3] * 4));
    if (s3) {
      wuffs_base__poke_u32le__no_bounds_check(d + (3 * 4), s3);
    }

    s += loop_unroll_count * 1;
    d += loop_unroll_count * 4;
    n -= loop_unroll_count;
  }

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    if (s0) {
      wuffs_base__poke_u32le__no_bounds_check(d + (0 * 4), s0);
    }

    s += 1 * 1;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

// ‼ WUFFS MULTI-FILE SECTION +x86_sse42
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2")
static uint64_t  //
wuffs_private_impl__swizzle_xxxx__y__x86_sse42(uint8_t* dst_ptr,
                                               size_t dst_len,
                                               uint8_t* dst_palette_ptr,
                                               size_t dst_palette_len,
                                               const uint8_t* src_ptr,
                                               size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t len = (dst_len4 < src_len) ? dst_len4 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  __m128i shuffle = _mm_set_epi8(+0x03, +0x03, +0x03, +0x03,  //
                                 +0x02, +0x02, +0x02, +0x02,  //
                                 +0x01, +0x01, +0x01, +0x01,  //
                                 +0x00, +0x00, +0x00, +0x00);
  __m128i or_ff = _mm_set_epi8(-0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00,  //
                               -0x01, +0x00, +0x00, +0x00);

  while (n >= 4) {
    __m128i x;
    x = _mm_cvtsi32_si128((int)(wuffs_base__peek_u32le__no_bounds_check(s)));
    x = _mm_shuffle_epi8(x, shuffle);
    x = _mm_or_si128(x, or_ff);
    _mm_storeu_si128((__m128i*)(void*)d, x);

    s += 4 * 1;
    d += 4 * 4;
    n -= 4;
  }

  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4), 0xFF000000 | (0x010101 * (uint32_t)s[0]));

    s += 1 * 1;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
// ‼ WUFFS MULTI-FILE SECTION -x86_sse42

static uint64_t  //
wuffs_private_impl__swizzle_xxxx__y(uint8_t* dst_ptr,
                                    size_t dst_len,
                                    uint8_t* dst_palette_ptr,
                                    size_t dst_palette_len,
                                    const uint8_t* src_ptr,
                                    size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t len = (dst_len4 < src_len) ? dst_len4 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4), 0xFF000000 | (0x010101 * (uint32_t)s[0]));

    s += 1 * 1;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxxx__y_16be(uint8_t* dst_ptr,
                                         size_t dst_len,
                                         uint8_t* dst_palette_ptr,
                                         size_t dst_palette_len,
                                         const uint8_t* src_ptr,
                                         size_t src_len) {
  size_t dst_len4 = dst_len / 4;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len4 < src_len2) ? dst_len4 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    wuffs_base__poke_u32le__no_bounds_check(
        d + (0 * 4), 0xFF000000 | (0x010101 * (uint32_t)s[0]));

    s += 1 * 2;
    d += 1 * 4;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_xxxxxxxx__index__src(uint8_t* dst_ptr,
                                                 size_t dst_len,
                                                 uint8_t* dst_palette_ptr,
                                                 size_t dst_palette_len,
                                                 const uint8_t* src_ptr,
                                                 size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len8 = dst_len / 8;
  size_t len = (dst_len8 < src_len) ? dst_len8 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8), wuffs_base__color_u32__as__color_u64(
                         wuffs_base__peek_u32le__no_bounds_check(
                             dst_palette_ptr + ((size_t)s[0] * 4))));

    s += 1 * 1;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxxxxxxx__index_binary_alpha__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t dst_len8 = dst_len / 8;
  size_t len = (dst_len8 < src_len) ? dst_len8 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    if (s0) {
      wuffs_base__poke_u64le__no_bounds_check(
          d + (0 * 8), wuffs_base__color_u32__as__color_u64(s0));
    }

    s += 1 * 1;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxxxxxxx__y(uint8_t* dst_ptr,
                                        size_t dst_len,
                                        uint8_t* dst_palette_ptr,
                                        size_t dst_palette_len,
                                        const uint8_t* src_ptr,
                                        size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t len = (dst_len8 < src_len) ? dst_len8 : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8), 0xFFFF000000000000 | (0x010101010101 * (uint64_t)s[0]));

    s += 1 * 1;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_xxxxxxxx__y_16be(uint8_t* dst_ptr,
                                             size_t dst_len,
                                             uint8_t* dst_palette_ptr,
                                             size_t dst_palette_len,
                                             const uint8_t* src_ptr,
                                             size_t src_len) {
  size_t dst_len8 = dst_len / 8;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len8 < src_len2) ? dst_len8 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint64_t s0 =
        ((uint64_t)(wuffs_base__peek_u16be__no_bounds_check(s + (0 * 2))));
    wuffs_base__poke_u64le__no_bounds_check(
        d + (0 * 8), 0xFFFF000000000000 | (0x000100010001 * s0));

    s += 1 * 2;
    d += 1 * 8;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_y__bgr(uint8_t* dst_ptr,
                                   size_t dst_len,
                                   uint8_t* dst_palette_ptr,
                                   size_t dst_palette_len,
                                   const uint8_t* src_ptr,
                                   size_t src_len) {
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len < src_len3) ? dst_len : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 =
        0xFF000000 | wuffs_base__peek_u24le__no_bounds_check(s + (0 * 3));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0);

    s += 1 * 3;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__bgr_565(uint8_t* dst_ptr,
                                       size_t dst_len,
                                       uint8_t* dst_palette_ptr,
                                       size_t dst_palette_len,
                                       const uint8_t* src_ptr,
                                       size_t src_len) {
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len < src_len2) ? dst_len : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_base__color_u16_rgb_565__as__color_u32_argb_premul(
        wuffs_base__peek_u16le__no_bounds_check(s + (0 * 2)));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0);

    s += 1 * 2;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__bgra_nonpremul__src(uint8_t* dst_ptr,
                                                   size_t dst_len,
                                                   uint8_t* dst_palette_ptr,
                                                   size_t dst_palette_len,
                                                   const uint8_t* src_ptr,
                                                   size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 =
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
            wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0);

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = 0xFF000000 | (0x00010101 * ((uint32_t)(d[0])));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__bgra_nonpremul_4x16le__src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len < src_len8) ? dst_len : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    d[0] = wuffs_base__color_u64_argb_nonpremul__as__color_u8_gray(
        wuffs_base__peek_u64le__no_bounds_check(s + (0 * 8)));

    s += 1 * 8;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__bgra_nonpremul_4x16le__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t src_len8 = src_len / 8;
  size_t len = (dst_len < src_len8) ? dst_len : src_len8;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t dr = 0x101 * ((uint32_t)d[0]);
    uint32_t dg = 0x101 * ((uint32_t)d[0]);
    uint32_t db = 0x101 * ((uint32_t)d[0]);
    uint32_t sa = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 6));
    uint32_t sr = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 4));
    uint32_t sg = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 2));
    uint32_t sb = ((uint32_t)wuffs_base__peek_u16le__no_bounds_check(s + 0));

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (nonpremul) over dst (premul).
    dr = ((sr * sa) + (dr * ia)) / 0xFFFF;
    dg = ((sg * sa) + (dg * ia)) / 0xFFFF;
    db = ((sb * sa) + (db * ia)) / 0xFFFF;

    // Convert to 16-bit color to 8-bit gray.
    uint32_t weighted_average =
        (19595 * dr) + (38470 * dg) + (7471 * db) + 32768;
    d[0] = (uint8_t)(weighted_average >> 24);

    s += 1 * 8;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__bgra_premul__src(uint8_t* dst_ptr,
                                                size_t dst_len,
                                                uint8_t* dst_palette_ptr,
                                                size_t dst_palette_len,
                                                const uint8_t* src_ptr,
                                                size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0);

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__bgra_premul__src_over(uint8_t* dst_ptr,
                                                     size_t dst_len,
                                                     uint8_t* dst_palette_ptr,
                                                     size_t dst_palette_len,
                                                     const uint8_t* src_ptr,
                                                     size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    // Extract 16-bit color components.
    uint32_t dr = 0x101 * ((uint32_t)d[0]);
    uint32_t dg = 0x101 * ((uint32_t)d[0]);
    uint32_t db = 0x101 * ((uint32_t)d[0]);
    uint32_t sa = 0x101 * ((uint32_t)s[3]);
    uint32_t sr = 0x101 * ((uint32_t)s[2]);
    uint32_t sg = 0x101 * ((uint32_t)s[1]);
    uint32_t sb = 0x101 * ((uint32_t)s[0]);

    // Calculate the inverse of the src-alpha: how much of the dst to keep.
    uint32_t ia = 0xFFFF - sa;

    // Composite src (premul) over dst (premul).
    dr = sr + ((dr * ia) / 0xFFFF);
    dg = sg + ((dg * ia) / 0xFFFF);
    db = sb + ((db * ia) / 0xFFFF);

    // Convert to 16-bit color to 8-bit gray.
    uint32_t weighted_average =
        (19595 * dr) + (38470 * dg) + (7471 * db) + 32768;
    d[0] = (uint8_t)(weighted_average >> 24);

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__bgrx(uint8_t* dst_ptr,
                                    size_t dst_len,
                                    uint8_t* dst_palette_ptr,
                                    size_t dst_palette_len,
                                    const uint8_t* src_ptr,
                                    size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 =
        0xFF000000 | wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0);

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__rgb(uint8_t* dst_ptr,
                                   size_t dst_len,
                                   uint8_t* dst_palette_ptr,
                                   size_t dst_palette_len,
                                   const uint8_t* src_ptr,
                                   size_t src_len) {
  size_t src_len3 = src_len / 3;
  size_t len = (dst_len < src_len3) ? dst_len : src_len3;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 =
        0xFF000000 | wuffs_base__peek_u24be__no_bounds_check(s + (0 * 3));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0);

    s += 1 * 3;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__rgba_nonpremul__src(uint8_t* dst_ptr,
                                                   size_t dst_len,
                                                   uint8_t* dst_palette_ptr,
                                                   size_t dst_palette_len,
                                                   const uint8_t* src_ptr,
                                                   size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(
            wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4))));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0);

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__rgba_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = 0xFF000000 | (0x00010101 * ((uint32_t)(d[0])));
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__rgba_premul__src(uint8_t* dst_ptr,
                                                size_t dst_len,
                                                uint8_t* dst_palette_ptr,
                                                size_t dst_palette_len,
                                                const uint8_t* src_ptr,
                                                size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(s0);

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__rgba_premul__src_over(uint8_t* dst_ptr,
                                                     size_t dst_len,
                                                     uint8_t* dst_palette_ptr,
                                                     size_t dst_palette_len,
                                                     const uint8_t* src_ptr,
                                                     size_t src_len) {
  size_t src_len4 = src_len / 4;
  size_t len = (dst_len < src_len4) ? dst_len : src_len4;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = 0xFF000000 | (0x00010101 * ((uint32_t)(d[0])));
    uint32_t s0 = wuffs_private_impl__swap_u32_argb_abgr(
        wuffs_base__peek_u32le__no_bounds_check(s + (0 * 4)));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(
        wuffs_private_impl__composite_premul_premul_u32_axxx(d0, s0));

    s += 1 * 4;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__y_16be(uint8_t* dst_ptr,
                                      size_t dst_len,
                                      uint8_t* dst_palette_ptr,
                                      size_t dst_palette_len,
                                      const uint8_t* src_ptr,
                                      size_t src_len) {
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len < src_len2) ? dst_len : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    d[0] = s[0];

    s += 1 * 2;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__ya_nonpremul__src(uint8_t* dst_ptr,
                                                 size_t dst_len,
                                                 uint8_t* dst_palette_ptr,
                                                 size_t dst_palette_len,
                                                 const uint8_t* src_ptr,
                                                 size_t src_len) {
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len < src_len2) ? dst_len : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    d[0] = (uint8_t)
        wuffs_base__color_u32_argb_nonpremul__as__color_u32_argb_premul(s0);

    s += 1 * 2;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__ya_nonpremul__src_over(uint8_t* dst_ptr,
                                                      size_t dst_len,
                                                      uint8_t* dst_palette_ptr,
                                                      size_t dst_palette_len,
                                                      const uint8_t* src_ptr,
                                                      size_t src_len) {
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len < src_len2) ? dst_len : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = 0xFF000000 | ((uint32_t)(d[0]) * 0x010101);
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    d[0] = (uint8_t)wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0,
                                                                            s0);

    s += 1 * 2;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__index__src(uint8_t* dst_ptr,
                                          size_t dst_len,
                                          uint8_t* dst_palette_ptr,
                                          size_t dst_palette_len,
                                          const uint8_t* src_ptr,
                                          size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t len = (dst_len < src_len) ? dst_len : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    d[0] = dst_palette_ptr[(size_t)s[0] * 4];

    s += 1 * 1;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__index_bgra_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t len = (dst_len < src_len) ? dst_len : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t d0 = 0xFF000000 | (0x00010101 * ((uint32_t)(d[0])));
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    d[0] = wuffs_base__color_u32_argb_premul__as__color_u8_gray(
        wuffs_private_impl__composite_premul_nonpremul_u32_axxx(d0, s0));

    s += 1 * 1;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

static uint64_t  //
wuffs_private_impl__swizzle_y__index_binary_alpha__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  if (dst_palette_len !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
    return 0;
  }
  size_t len = (dst_len < src_len) ? dst_len : src_len;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  // TODO: unroll.

  while (n >= 1) {
    uint32_t s0 = wuffs_base__peek_u32le__no_bounds_check(dst_palette_ptr +
                                                          ((size_t)s[0] * 4));
    if (s0) {
      d[0] = (uint8_t)s0;
    }

    s += 1 * 1;
    d += 1 * 1;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_y_16le__y_16be(uint8_t* dst_ptr,
                                           size_t dst_len,
                                           uint8_t* dst_palette_ptr,
                                           size_t dst_palette_len,
                                           const uint8_t* src_ptr,
                                           size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len2 < src_len2) ? dst_len2 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint8_t s0 = s[0];
    uint8_t s1 = s[1];
    d[0] = s1;
    d[1] = s0;

    s += 1 * 2;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_ya_nonpremul__ya_nonpremul__src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    const uint8_t* src_ptr,
    size_t src_len) {
  size_t dst_len2 = dst_len / 2;
  size_t src_len2 = src_len / 2;
  size_t len = (dst_len2 < src_len2) ? dst_len2 : src_len2;
  uint8_t* d = dst_ptr;
  const uint8_t* s = src_ptr;
  size_t n = len;

  while (n >= 1) {
    uint32_t d0 = ((uint32_t)(d[1]) << 24) | ((uint32_t)(d[0]) * 0x010101);
    uint32_t s0 = ((uint32_t)(s[1]) << 24) | ((uint32_t)(s[0]) * 0x010101);
    uint32_t c0 =
        wuffs_private_impl__composite_nonpremul_nonpremul_u32_axxx(d0, s0);
    wuffs_base__poke_u16le__no_bounds_check(d + (0 * 2), (uint16_t)(c0 >> 16));

    s += 1 * 2;
    d += 1 * 2;
    n -= 1;
  }

  return len;
}

// --------

static uint64_t  //
wuffs_private_impl__swizzle_transparent_black_src(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    uint64_t num_pixels,
    uint32_t dst_pixfmt_bytes_per_pixel) {
  uint64_t n = ((uint64_t)dst_len) / dst_pixfmt_bytes_per_pixel;
  if (n > num_pixels) {
    n = num_pixels;
  }
  memset(dst_ptr, 0, ((size_t)(n * dst_pixfmt_bytes_per_pixel)));
  return n;
}

static uint64_t  //
wuffs_private_impl__swizzle_transparent_black_src_over(
    uint8_t* dst_ptr,
    size_t dst_len,
    uint8_t* dst_palette_ptr,
    size_t dst_palette_len,
    uint64_t num_pixels,
    uint32_t dst_pixfmt_bytes_per_pixel) {
  uint64_t n = ((uint64_t)dst_len) / dst_pixfmt_bytes_per_pixel;
  if (n > num_pixels) {
    n = num_pixels;
  }
  return n;
}

// --------

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__y(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      return wuffs_private_impl__swizzle_copy_1_1;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      return wuffs_private_impl__swizzle_bgr_565__y;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      return wuffs_private_impl__swizzle_xxx__y;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
      if (wuffs_base__cpu_arch__have_x86_sse42()) {
        return wuffs_private_impl__swizzle_xxxx__y__x86_sse42;
      }
#endif
      return wuffs_private_impl__swizzle_xxxx__y;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL_4X16LE:
      return wuffs_private_impl__swizzle_xxxxxxxx__y;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__y_16be(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      return wuffs_private_impl__swizzle_y__y_16be;

    case WUFFS_BASE__PIXEL_FORMAT__Y_16LE:
      return wuffs_private_impl__swizzle_y_16le__y_16be;

    case WUFFS_BASE__PIXEL_FORMAT__Y_16BE:
      return wuffs_private_impl__swizzle_copy_2_2;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      return wuffs_private_impl__swizzle_bgr_565__y_16be;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      return wuffs_private_impl__swizzle_xxx__y_16be;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      return wuffs_private_impl__swizzle_xxxx__y_16be;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL_4X16LE:
      return wuffs_private_impl__swizzle_xxxxxxxx__y_16be;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__ya_nonpremul(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_y__ya_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_y__ya_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_2_2;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_ya_nonpremul__ya_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr_565__ya_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr_565__ya_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxx__ya_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_xxx__ya_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul__ya_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__ya_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_premul__ya_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__ya_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL_4X16LE:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__ya_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__ya_nonpremul__src_over;
      }
      return NULL;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__indexed__bgra_nonpremul(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          if (wuffs_private_impl__swizzle_squash_align4_y_8888(
                  dst_palette.ptr, dst_palette.len, src_palette.ptr,
                  src_palette.len, true) !=
              (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_y__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                            src_palette) !=
              WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_y__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL:
      if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                        src_palette) !=
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_1_1;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          if (wuffs_private_impl__swizzle_squash_align4_bgr_565_8888(
                  dst_palette.ptr, dst_palette.len, src_palette.ptr,
                  src_palette.len, true) !=
              (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_bgr_565__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                            src_palette) !=
              WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_bgr_565__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          if (wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul__src(
                  dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
                  src_palette.len) !=
              (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_xxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                            src_palette) !=
              WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_xxx__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                        src_palette) !=
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                        src_palette) !=
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxxxxxxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          if (wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul__src(
                  dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
                  src_palette.len) !=
              (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_xxxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                            src_palette) !=
              WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_bgra_premul__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          if (wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul__src(
                  dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
                  src_palette.len) !=
              (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_xxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          if (wuffs_private_impl__swizzle_swap_rgbx_bgrx(
                  dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
                  src_palette.len) !=
              (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_xxx__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      if (wuffs_private_impl__swizzle_swap_rgbx_bgrx(
              dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
              src_palette.len) !=
          (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          if (wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul__src(
                  dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
                  src_palette.len) !=
              (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_xxxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          if (wuffs_private_impl__swizzle_swap_rgbx_bgrx(
                  dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
                  src_palette.len) !=
              (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
            return NULL;
          }
          return wuffs_private_impl__swizzle_bgra_premul__index_bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      // TODO.
      break;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__indexed__bgra_binary(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      if (wuffs_private_impl__swizzle_squash_align4_y_8888(
              dst_palette.ptr, dst_palette.len, src_palette.ptr,
              src_palette.len, false) !=
          (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_y__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_y__index_binary_alpha__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY:
      if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                        src_palette) !=
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_1_1;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      if (wuffs_private_impl__swizzle_squash_align4_bgr_565_8888(
              dst_palette.ptr, dst_palette.len, src_palette.ptr,
              src_palette.len, false) !=
          (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr_565__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr_565__index_binary_alpha__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                        src_palette) !=
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_xxx__index_binary_alpha__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
      if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                        src_palette) !=
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_xxxx__index_binary_alpha__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL_4X16LE:
      if (wuffs_private_impl__slice_u8__copy_from_slice(dst_palette,
                                                        src_palette) !=
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxxxxxxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_xxxxxxxx__index_binary_alpha__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      if (wuffs_private_impl__swizzle_swap_rgbx_bgrx(
              dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
              src_palette.len) !=
          (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_xxx__index_binary_alpha__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
      if (wuffs_private_impl__swizzle_swap_rgbx_bgrx(
              dst_palette.ptr, dst_palette.len, NULL, 0, src_palette.ptr,
              src_palette.len) !=
          (WUFFS_BASE__PIXEL_FORMAT__INDEXED__PALETTE_BYTE_LENGTH / 4)) {
        return NULL;
      }
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_xxxx__index__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_xxxx__index_binary_alpha__src_over;
      }
      return NULL;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__bgr_565(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      return wuffs_private_impl__swizzle_y__bgr_565;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      return wuffs_private_impl__swizzle_copy_2_2;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      return wuffs_private_impl__swizzle_bgr__bgr_565;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      return wuffs_private_impl__swizzle_bgrw__bgr_565;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL_4X16LE:
      return wuffs_private_impl__swizzle_bgrw_4x16le__bgr_565;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      return wuffs_private_impl__swizzle_rgb__bgr_565;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      return wuffs_private_impl__swizzle_rgbw__bgr_565;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__bgr(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      return wuffs_private_impl__swizzle_y__bgr;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      return wuffs_private_impl__swizzle_bgr_565__bgr;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      return wuffs_private_impl__swizzle_copy_3_3;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
      if (wuffs_base__cpu_arch__have_x86_sse42()) {
        return wuffs_private_impl__swizzle_bgrw__bgr__x86_sse42;
      }
#endif
      return wuffs_private_impl__swizzle_bgrw__bgr;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL_4X16LE:
      return wuffs_private_impl__swizzle_bgrw_4x16le__bgr;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      return wuffs_private_impl__swizzle_swap_rgb_bgr;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
      if (wuffs_base__cpu_arch__have_x86_sse42()) {
        return wuffs_private_impl__swizzle_bgrw__rgb__x86_sse42;
      }
#endif
      return wuffs_private_impl__swizzle_bgrw__rgb;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__bgra_nonpremul(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_y__bgra_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_y__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr_565__bgra_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr_565__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__bgra_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_4_4;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      // TODO.
      break;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__rgba_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
          if (wuffs_base__cpu_arch__have_x86_sse42()) {
            return wuffs_private_impl__swizzle_swap_rgbx_bgrx__x86_sse42;
          }
#endif
          return wuffs_private_impl__swizzle_swap_rgbx_bgrx;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      // TODO.
      break;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__bgra_nonpremul_4x16le(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_y__bgra_nonpremul_4x16le__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_y__bgra_nonpremul_4x16le__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr_565__bgra_nonpremul_4x16le__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr_565__bgra_nonpremul_4x16le__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__bgra_nonpremul_4x16le__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__bgra_nonpremul_4x16le__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_nonpremul_4x16le__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_nonpremul_4x16le__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_8_8;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_nonpremul_4x16le__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul_4x16le__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul_4x16le__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      // TODO.
      break;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__rgba_nonpremul_4x16le__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__rgba_nonpremul_4x16le__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_rgba_nonpremul__bgra_nonpremul_4x16le__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_rgba_nonpremul__bgra_nonpremul_4x16le__src_over;
      }
      break;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul_4x16le__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul_4x16le__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      // TODO.
      break;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__bgra_premul(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_y__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_y__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr_565__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr_565__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_4_4;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__rgba_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul__rgba_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
          if (wuffs_base__cpu_arch__have_x86_sse42()) {
            return wuffs_private_impl__swizzle_swap_rgbx_bgrx__x86_sse42;
          }
#endif
          return wuffs_private_impl__swizzle_swap_rgbx_bgrx;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_premul__src_over;
      }
      return NULL;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__bgra_binary(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_y__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_y__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr_565__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr_565__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_4_4;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_4_4;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__rgba_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
          if (wuffs_base__cpu_arch__have_x86_sse42()) {
            return wuffs_private_impl__swizzle_swap_rgbx_bgrx__x86_sse42;
          }
#endif
          return wuffs_private_impl__swizzle_swap_rgbx_bgrx;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
          if (wuffs_base__cpu_arch__have_x86_sse42()) {
            return wuffs_private_impl__swizzle_swap_rgbx_bgrx__x86_sse42;
          }
#endif
          return wuffs_private_impl__swizzle_swap_rgbx_bgrx;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_premul__src_over;
      }
      return NULL;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__bgrx(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      return wuffs_private_impl__swizzle_y__bgrx;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      return wuffs_private_impl__swizzle_bgr_565__bgrx;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      return wuffs_private_impl__swizzle_xxx__xxxx;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
      return wuffs_private_impl__swizzle_bgrw__bgrx;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      return wuffs_private_impl__swizzle_bgrw_4x16le__bgrx;

    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      return wuffs_private_impl__swizzle_copy_4_4;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      return wuffs_private_impl__swizzle_bgr__rgbx;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      return wuffs_private_impl__swizzle_bgrw__rgbx;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__rgb(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      return wuffs_private_impl__swizzle_y__rgb;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      return wuffs_private_impl__swizzle_bgr_565__rgb;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      return wuffs_private_impl__swizzle_swap_rgb_bgr;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
      if (wuffs_base__cpu_arch__have_x86_sse42()) {
        return wuffs_private_impl__swizzle_bgrw__rgb__x86_sse42;
      }
#endif
      return wuffs_private_impl__swizzle_bgrw__rgb;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      return wuffs_private_impl__swizzle_bgrw_4x16le__rgb;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      return wuffs_private_impl__swizzle_copy_3_3;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
      if (wuffs_base__cpu_arch__have_x86_sse42()) {
        return wuffs_private_impl__swizzle_bgrw__bgr__x86_sse42;
      }
#endif
      return wuffs_private_impl__swizzle_bgrw__bgr;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__rgba_nonpremul(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_y__rgba_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_y__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr_565__rgba_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr_565__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__rgba_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
          if (wuffs_base__cpu_arch__have_x86_sse42()) {
            return wuffs_private_impl__swizzle_swap_rgbx_bgrx__x86_sse42;
          }
#endif
          return wuffs_private_impl__swizzle_swap_rgbx_bgrx;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__rgba_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      // TODO.
      break;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__bgra_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_4_4;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_nonpremul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_BINARY:
    case WUFFS_BASE__PIXEL_FORMAT__RGBX:
      // TODO.
      break;
  }
  return NULL;
}

static inline WUFFS_BASE__FORCE_INLINE wuffs_base__pixel_swizzler__func  //
wuffs_private_impl__pixel_swizzler__prepare__rgba_premul(
    wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_format dst_pixfmt,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src_palette,
    wuffs_base__pixel_blend blend) {
  switch (dst_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_y__rgba_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_y__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr_565__rgba_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr_565__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__rgba_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul__rgba_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__rgba_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul_4x16le__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V2)
          if (wuffs_base__cpu_arch__have_x86_sse42()) {
            return wuffs_private_impl__swizzle_swap_rgbx_bgrx__x86_sse42;
          }
#endif
          return wuffs_private_impl__swizzle_swap_rgbx_bgrx;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__rgba_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgr__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgr__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_premul__src;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_nonpremul__bgra_premul__src_over;
      }
      return NULL;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      switch (blend) {
        case WUFFS_BASE__PIXEL_BLEND__SRC:
          return wuffs_private_impl__swizzle_copy_4_4;
        case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
          return wuffs_private_impl__swizzle_bgra_premul__bgra_premul__src_over;
      }
      return NULL;
  }
  return NULL;
}

// --------

WUFFS_BASE__MAYBE_STATIC wuffs_base__status  //
wuffs_base__pixel_swizzler__prepare(wuffs_base__pixel_swizzler* p,
                                    wuffs_base__pixel_format dst_pixfmt,
                                    wuffs_base__slice_u8 dst_palette,
                                    wuffs_base__pixel_format src_pixfmt,
                                    wuffs_base__slice_u8 src_palette,
                                    wuffs_base__pixel_blend blend) {
  if (!p) {
    return wuffs_base__make_status(wuffs_base__error__bad_receiver);
  }
  p->private_impl.func = NULL;
  p->private_impl.transparent_black_func = NULL;
  p->private_impl.dst_pixfmt_bytes_per_pixel = 0;
  p->private_impl.src_pixfmt_bytes_per_pixel = 0;

  // ----

#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST)
  switch (dst_pixfmt.repr) {
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_Y)
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      break;
#endif
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_BGR_565)
    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      break;
#endif
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_BGR)
    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      break;
#endif
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_BGRA_NONPREMUL)
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      break;
#endif
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_BGRA_NONPREMUL_4X16LE)
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      break;
#endif
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_BGRA_PREMUL)
    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      break;
#endif
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_RGB)
    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      break;
#endif
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_RGBA_NONPREMUL)
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      break;
#endif
#if defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_RGBA_PREMUL)
    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      break;
#endif
    default:
      return wuffs_base__make_status(
          wuffs_base__error__disabled_by_wuffs_config_dst_pixel_format_enable_allowlist);
  }
#endif  // defined(WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST)

  // ----

  wuffs_base__pixel_swizzler__func func = NULL;
  wuffs_base__pixel_swizzler__transparent_black_func transparent_black_func =
      NULL;

  uint32_t dst_pixfmt_bits_per_pixel =
      wuffs_base__pixel_format__bits_per_pixel(&dst_pixfmt);
  if ((dst_pixfmt_bits_per_pixel == 0) ||
      ((dst_pixfmt_bits_per_pixel & 7) != 0)) {
    return wuffs_base__make_status(
        wuffs_base__error__unsupported_pixel_swizzler_option);
  }

  uint32_t src_pixfmt_bits_per_pixel =
      wuffs_base__pixel_format__bits_per_pixel(&src_pixfmt);
  if ((src_pixfmt_bits_per_pixel == 0) ||
      ((src_pixfmt_bits_per_pixel & 7) != 0)) {
    return wuffs_base__make_status(
        wuffs_base__error__unsupported_pixel_swizzler_option);
  }

  // TODO: support many more formats.

  switch (blend) {
    case WUFFS_BASE__PIXEL_BLEND__SRC:
      transparent_black_func =
          wuffs_private_impl__swizzle_transparent_black_src;
      break;

    case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
      transparent_black_func =
          wuffs_private_impl__swizzle_transparent_black_src_over;
      break;
  }

  switch (src_pixfmt.repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
      func = wuffs_private_impl__pixel_swizzler__prepare__y(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__Y_16BE:
      func = wuffs_private_impl__pixel_swizzler__prepare__y_16be(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__YA_NONPREMUL:
      func = wuffs_private_impl__pixel_swizzler__prepare__ya_nonpremul(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_NONPREMUL:
      func =
          wuffs_private_impl__pixel_swizzler__prepare__indexed__bgra_nonpremul(
              p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY:
      func = wuffs_private_impl__pixel_swizzler__prepare__indexed__bgra_binary(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      func = wuffs_private_impl__pixel_swizzler__prepare__bgr_565(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGR:
      func = wuffs_private_impl__pixel_swizzler__prepare__bgr(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      func = wuffs_private_impl__pixel_swizzler__prepare__bgra_nonpremul(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      func = wuffs_private_impl__pixel_swizzler__prepare__bgra_nonpremul_4x16le(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      func = wuffs_private_impl__pixel_swizzler__prepare__bgra_premul(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGRA_BINARY:
      func = wuffs_private_impl__pixel_swizzler__prepare__bgra_binary(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__BGRX:
      func = wuffs_private_impl__pixel_swizzler__prepare__bgrx(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__RGB:
      func = wuffs_private_impl__pixel_swizzler__prepare__rgb(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      func = wuffs_private_impl__pixel_swizzler__prepare__rgba_nonpremul(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;

    case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
      func = wuffs_private_impl__pixel_swizzler__prepare__rgba_premul(
          p, dst_pixfmt, dst_palette, src_palette, blend);
      break;
  }

  p->private_impl.func = func;
  p->private_impl.transparent_black_func = transparent_black_func;
  p->private_impl.dst_pixfmt_bytes_per_pixel = dst_pixfmt_bits_per_pixel / 8;
  p->private_impl.src_pixfmt_bytes_per_pixel = src_pixfmt_bits_per_pixel / 8;
  return wuffs_base__make_status(
      func ? NULL : wuffs_base__error__unsupported_pixel_swizzler_option);
}

WUFFS_BASE__MAYBE_STATIC uint64_t  //
wuffs_base__pixel_swizzler__limited_swizzle_u32_interleaved_from_reader(
    const wuffs_base__pixel_swizzler* p,
    uint32_t up_to_num_pixels,
    wuffs_base__slice_u8 dst,
    wuffs_base__slice_u8 dst_palette,
    const uint8_t** ptr_iop_r,
    const uint8_t* io2_r) {
  if (p && p->private_impl.func) {
    const uint8_t* iop_r = *ptr_iop_r;
    uint64_t src_len = wuffs_base__u64__min(
        ((uint64_t)up_to_num_pixels) *
            ((uint64_t)p->private_impl.src_pixfmt_bytes_per_pixel),
        ((uint64_t)(io2_r - iop_r)));
    uint64_t n =
        (*p->private_impl.func)(dst.ptr, dst.len, dst_palette.ptr,
                                dst_palette.len, iop_r, (size_t)src_len);
    *ptr_iop_r += n * p->private_impl.src_pixfmt_bytes_per_pixel;
    return n;
  }
  return 0;
}

WUFFS_BASE__MAYBE_STATIC uint64_t  //
wuffs_base__pixel_swizzler__swizzle_interleaved_from_reader(
    const wuffs_base__pixel_swizzler* p,
    wuffs_base__slice_u8 dst,
    wuffs_base__slice_u8 dst_palette,
    const uint8_t** ptr_iop_r,
    const uint8_t* io2_r) {
  if (p && p->private_impl.func) {
    const uint8_t* iop_r = *ptr_iop_r;
    uint64_t src_len = ((uint64_t)(io2_r - iop_r));
    uint64_t n =
        (*p->private_impl.func)(dst.ptr, dst.len, dst_palette.ptr,
                                dst_palette.len, iop_r, (size_t)src_len);
    *ptr_iop_r += n * p->private_impl.src_pixfmt_bytes_per_pixel;
    return n;
  }
  return 0;
}

WUFFS_BASE__MAYBE_STATIC uint64_t  //
wuffs_base__pixel_swizzler__swizzle_interleaved_from_slice(
    const wuffs_base__pixel_swizzler* p,
    wuffs_base__slice_u8 dst,
    wuffs_base__slice_u8 dst_palette,
    wuffs_base__slice_u8 src) {
  if (p && p->private_impl.func) {
    return (*p->private_impl.func)(dst.ptr, dst.len, dst_palette.ptr,
                                   dst_palette.len, src.ptr, src.len);
  }
  return 0;
}

WUFFS_BASE__MAYBE_STATIC uint64_t  //
wuffs_base__pixel_swizzler__swizzle_interleaved_transparent_black(
    const wuffs_base__pixel_swizzler* p,
    wuffs_base__slice_u8 dst,
    wuffs_base__slice_u8 dst_palette,
    uint64_t num_pixels) {
  if (p && p->private_impl.transparent_black_func) {
    return (*p->private_impl.transparent_black_func)(
        dst.ptr, dst.len, dst_palette.ptr, dst_palette.len, num_pixels,
        p->private_impl.dst_pixfmt_bytes_per_pixel);
  }
  return 0;
}
