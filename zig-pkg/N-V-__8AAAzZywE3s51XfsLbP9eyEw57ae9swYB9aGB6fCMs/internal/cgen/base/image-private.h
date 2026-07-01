// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Images

WUFFS_BASE__MAYBE_STATIC uint64_t  //
wuffs_base__pixel_swizzler__limited_swizzle_u32_interleaved_from_reader(
    const wuffs_base__pixel_swizzler* p,
    uint32_t up_to_num_pixels,
    wuffs_base__slice_u8 dst,
    wuffs_base__slice_u8 dst_palette,
    const uint8_t** ptr_iop_r,
    const uint8_t* io2_r);

WUFFS_BASE__MAYBE_STATIC uint64_t  //
wuffs_base__pixel_swizzler__swizzle_interleaved_from_reader(
    const wuffs_base__pixel_swizzler* p,
    wuffs_base__slice_u8 dst,
    wuffs_base__slice_u8 dst_palette,
    const uint8_t** ptr_iop_r,
    const uint8_t* io2_r);

WUFFS_BASE__MAYBE_STATIC uint64_t  //
wuffs_base__pixel_swizzler__swizzle_interleaved_transparent_black(
    const wuffs_base__pixel_swizzler* p,
    wuffs_base__slice_u8 dst,
    wuffs_base__slice_u8 dst_palette,
    uint64_t num_pixels);

WUFFS_BASE__MAYBE_STATIC wuffs_base__status  //
wuffs_base__pixel_swizzler__swizzle_ycck(
    const wuffs_base__pixel_swizzler* p,
    wuffs_base__pixel_buffer* dst,
    wuffs_base__slice_u8 dst_palette,
    uint32_t x_min_incl,
    uint32_t x_max_excl,
    uint32_t y_min_incl,
    uint32_t y_max_excl,
    wuffs_base__slice_u8 src0,
    wuffs_base__slice_u8 src1,
    wuffs_base__slice_u8 src2,
    wuffs_base__slice_u8 src3,
    uint32_t width0,
    uint32_t width1,
    uint32_t width2,
    uint32_t width3,
    uint32_t height0,
    uint32_t height1,
    uint32_t height2,
    uint32_t height3,
    uint32_t stride0,
    uint32_t stride1,
    uint32_t stride2,
    uint32_t stride3,
    uint8_t h0,
    uint8_t h1,
    uint8_t h2,
    uint8_t h3,
    uint8_t v0,
    uint8_t v1,
    uint8_t v2,
    uint8_t v3,
    bool is_rgb_or_cmyk,
    bool triangle_filter_for_2to1,
    wuffs_base__slice_u8 scratch_buffer_2k);

// ---------------- Images (Utility)

#define wuffs_base__utility__make_pixel_format wuffs_base__make_pixel_format
