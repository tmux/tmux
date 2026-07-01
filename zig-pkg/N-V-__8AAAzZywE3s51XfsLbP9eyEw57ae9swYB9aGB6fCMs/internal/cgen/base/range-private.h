// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Ranges and Rects

static inline uint32_t  //
wuffs_private_impl__range_ii_u32__get_min_incl(
    const wuffs_base__range_ii_u32* r) {
  return r->min_incl;
}

static inline uint32_t  //
wuffs_private_impl__range_ii_u32__get_max_incl(
    const wuffs_base__range_ii_u32* r) {
  return r->max_incl;
}

static inline uint32_t  //
wuffs_private_impl__range_ie_u32__get_min_incl(
    const wuffs_base__range_ie_u32* r) {
  return r->min_incl;
}

static inline uint32_t  //
wuffs_private_impl__range_ie_u32__get_max_excl(
    const wuffs_base__range_ie_u32* r) {
  return r->max_excl;
}

static inline uint64_t  //
wuffs_private_impl__range_ii_u64__get_min_incl(
    const wuffs_base__range_ii_u64* r) {
  return r->min_incl;
}

static inline uint64_t  //
wuffs_private_impl__range_ii_u64__get_max_incl(
    const wuffs_base__range_ii_u64* r) {
  return r->max_incl;
}

static inline uint64_t  //
wuffs_private_impl__range_ie_u64__get_min_incl(
    const wuffs_base__range_ie_u64* r) {
  return r->min_incl;
}

static inline uint64_t  //
wuffs_private_impl__range_ie_u64__get_max_excl(
    const wuffs_base__range_ie_u64* r) {
  return r->max_excl;
}

// ---------------- Ranges and Rects (Utility)

#define wuffs_base__utility__empty_range_ii_u32 wuffs_base__empty_range_ii_u32
#define wuffs_base__utility__empty_range_ie_u32 wuffs_base__empty_range_ie_u32
#define wuffs_base__utility__empty_range_ii_u64 wuffs_base__empty_range_ii_u64
#define wuffs_base__utility__empty_range_ie_u64 wuffs_base__empty_range_ie_u64
#define wuffs_base__utility__empty_rect_ii_u32 wuffs_base__empty_rect_ii_u32
#define wuffs_base__utility__empty_rect_ie_u32 wuffs_base__empty_rect_ie_u32
#define wuffs_base__utility__make_range_ii_u32 wuffs_base__make_range_ii_u32
#define wuffs_base__utility__make_range_ie_u32 wuffs_base__make_range_ie_u32
#define wuffs_base__utility__make_range_ii_u64 wuffs_base__make_range_ii_u64
#define wuffs_base__utility__make_range_ie_u64 wuffs_base__make_range_ie_u64
#define wuffs_base__utility__make_rect_ii_u32 wuffs_base__make_rect_ii_u32
#define wuffs_base__utility__make_rect_ie_u32 wuffs_base__make_rect_ie_u32
