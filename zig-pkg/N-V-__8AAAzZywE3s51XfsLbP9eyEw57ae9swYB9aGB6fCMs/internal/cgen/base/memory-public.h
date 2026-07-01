// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Memory Allocation

// The memory allocation related functions in this section aren't used by Wuffs
// per se, but they may be helpful to the code that uses Wuffs.

// wuffs_base__malloc_slice_uxx wraps calling a malloc-like function, except
// that it takes a uint64_t number of elements instead of a size_t size in
// bytes, and it returns a slice (a pointer and a length) instead of just a
// pointer.
//
// You can pass the C stdlib's malloc as the malloc_func.
//
// It returns an empty slice (containing a NULL ptr field) if num_uxx is zero
// or if (num_uxx * sizeof(uintxx_t)) would overflow SIZE_MAX.

static inline wuffs_base__slice_u8  //
wuffs_base__malloc_slice_u8(void* (*malloc_func)(size_t), uint64_t num_u8) {
  if (malloc_func && num_u8 && (num_u8 <= (SIZE_MAX / sizeof(uint8_t)))) {
    void* p = (*malloc_func)((size_t)(num_u8 * sizeof(uint8_t)));
    if (p) {
      return wuffs_base__make_slice_u8((uint8_t*)(p), (size_t)num_u8);
    }
  }
  return wuffs_base__empty_slice_u8();
}

static inline wuffs_base__slice_u16  //
wuffs_base__malloc_slice_u16(void* (*malloc_func)(size_t), uint64_t num_u16) {
  if (malloc_func && num_u16 && (num_u16 <= (SIZE_MAX / sizeof(uint16_t)))) {
    void* p = (*malloc_func)((size_t)(num_u16 * sizeof(uint16_t)));
    if (p) {
      return wuffs_base__make_slice_u16((uint16_t*)(p), (size_t)num_u16);
    }
  }
  return wuffs_base__empty_slice_u16();
}

static inline wuffs_base__slice_u32  //
wuffs_base__malloc_slice_u32(void* (*malloc_func)(size_t), uint64_t num_u32) {
  if (malloc_func && num_u32 && (num_u32 <= (SIZE_MAX / sizeof(uint32_t)))) {
    void* p = (*malloc_func)((size_t)(num_u32 * sizeof(uint32_t)));
    if (p) {
      return wuffs_base__make_slice_u32((uint32_t*)(p), (size_t)num_u32);
    }
  }
  return wuffs_base__empty_slice_u32();
}

static inline wuffs_base__slice_u64  //
wuffs_base__malloc_slice_u64(void* (*malloc_func)(size_t), uint64_t num_u64) {
  if (malloc_func && num_u64 && (num_u64 <= (SIZE_MAX / sizeof(uint64_t)))) {
    void* p = (*malloc_func)((size_t)(num_u64 * sizeof(uint64_t)));
    if (p) {
      return wuffs_base__make_slice_u64((uint64_t*)(p), (size_t)num_u64);
    }
  }
  return wuffs_base__empty_slice_u64();
}
