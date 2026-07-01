// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- I/O

static inline uint64_t  //
wuffs_private_impl__io__count_since(uint64_t mark, uint64_t index) {
  if (index >= mark) {
    return index - mark;
  }
  return 0;
}

// TODO: drop the "const" in "const uint8_t* ptr". Some though required about
// the base.io_reader.since method returning a mutable "slice base.u8".
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
static inline wuffs_base__slice_u8  //
wuffs_private_impl__io__since(uint64_t mark,
                              uint64_t index,
                              const uint8_t* ptr) {
  if (index >= mark) {
    return wuffs_base__make_slice_u8(((uint8_t*)ptr) + mark,
                                     ((size_t)(index - mark)));
  }
  return wuffs_base__empty_slice_u8();
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// --------

static inline void  //
wuffs_private_impl__io_reader__limit(const uint8_t** ptr_io2_r,
                                     const uint8_t* iop_r,
                                     uint64_t limit) {
  if (((uint64_t)(*ptr_io2_r - iop_r)) > limit) {
    *ptr_io2_r = iop_r + limit;
  }
}

static inline uint32_t  //
wuffs_private_impl__io_reader__limited_copy_u32_to_slice(
    const uint8_t** ptr_iop_r,
    const uint8_t* io2_r,
    uint32_t length,
    wuffs_base__slice_u8 dst) {
  const uint8_t* iop_r = *ptr_iop_r;
  size_t n = dst.len;
  if (n > length) {
    n = length;
  }
  if (n > ((size_t)(io2_r - iop_r))) {
    n = (size_t)(io2_r - iop_r);
  }
  if (n > 0) {
    memmove(dst.ptr, iop_r, n);
    *ptr_iop_r += n;
  }
  return (uint32_t)(n);
}

// wuffs_private_impl__io_reader__match7 returns whether the io_reader's
// upcoming bytes start with the given prefix (up to 7 bytes long). It is
// peek-like, not read-like, in that there are no side-effects.
//
// The low 3 bits of a hold the prefix length, n.
//
// The high 56 bits of a hold the prefix itself, in little-endian order. The
// first prefix byte is in bits 8..=15, the second prefix byte is in bits
// 16..=23, etc. The high (8 * (7 - n)) bits are ignored.
//
// There are three possible return values:
//  - 0 means success.
//  - 1 means inconclusive, equivalent to "$short read".
//  - 2 means failure.
static inline uint32_t  //
wuffs_private_impl__io_reader__match7(const uint8_t* iop_r,
                                      const uint8_t* io2_r,
                                      wuffs_base__io_buffer* r,
                                      uint64_t a) {
  uint32_t n = a & 7;
  a >>= 8;
  if ((io2_r - iop_r) >= 8) {
    uint64_t x = wuffs_base__peek_u64le__no_bounds_check(iop_r);
    uint32_t shift = 8 * (8 - n);
    return ((a << shift) == (x << shift)) ? 0 : 2;
  }
  for (; n > 0; n--) {
    if (iop_r >= io2_r) {
      return (r && r->meta.closed) ? 2 : 1;
    } else if (*iop_r != ((uint8_t)(a))) {
      return 2;
    }
    iop_r++;
    a >>= 8;
  }
  return 0;
}

static inline wuffs_base__io_buffer*  //
wuffs_private_impl__io_reader__set(wuffs_base__io_buffer* b,
                                   const uint8_t** ptr_iop_r,
                                   const uint8_t** ptr_io0_r,
                                   const uint8_t** ptr_io1_r,
                                   const uint8_t** ptr_io2_r,
                                   wuffs_base__slice_u8 data,
                                   uint64_t history_position) {
  b->data = data;
  b->meta.wi = data.len;
  b->meta.ri = 0;
  b->meta.pos = history_position;
  b->meta.closed = false;

  *ptr_iop_r = data.ptr;
  *ptr_io0_r = data.ptr;
  *ptr_io1_r = data.ptr;
  *ptr_io2_r = data.ptr + data.len;

  return b;
}

// --------

static inline uint64_t  //
wuffs_private_impl__io_writer__copy_from_slice(uint8_t** ptr_iop_w,
                                               uint8_t* io2_w,
                                               wuffs_base__slice_u8 src) {
  uint8_t* iop_w = *ptr_iop_w;
  size_t n = src.len;
  if (n > ((size_t)(io2_w - iop_w))) {
    n = (size_t)(io2_w - iop_w);
  }
  if (n > 0) {
    memmove(iop_w, src.ptr, n);
    *ptr_iop_w += n;
  }
  return (uint64_t)(n);
}

static inline void  //
wuffs_private_impl__io_writer__limit(uint8_t** ptr_io2_w,
                                     uint8_t* iop_w,
                                     uint64_t limit) {
  if (((uint64_t)(*ptr_io2_w - iop_w)) > limit) {
    *ptr_io2_w = iop_w + limit;
  }
}

static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_history(
    uint8_t** ptr_iop_w,
    uint8_t* io0_w,
    uint8_t* io2_w,
    uint32_t length,
    uint32_t distance) {
  if (!distance) {
    return 0;
  }
  uint8_t* p = *ptr_iop_w;
  if ((size_t)(p - io0_w) < (size_t)(distance)) {
    return 0;
  }
  uint8_t* q = p - distance;
  size_t n = (size_t)(io2_w - p);
  if ((size_t)(length) > n) {
    length = (uint32_t)(n);
  } else {
    n = (size_t)(length);
  }
  // TODO: unrolling by 3 seems best for the std/deflate benchmarks, but that
  // is mostly because 3 is the minimum length for the deflate format. This
  // function implementation shouldn't overfit to that one format. Perhaps the
  // limited_copy_u32_from_history Wuffs method should also take an unroll hint
  // argument, and the cgen can look if that argument is the constant
  // expression '3'.
  //
  // See also wuffs_private_impl__io_writer__limited_copy_u32_from_history_fast
  // below.
  for (; n >= 3; n -= 3) {
    *p++ = *q++;
    *p++ = *q++;
    *p++ = *q++;
  }
  for (; n; n--) {
    *p++ = *q++;
  }
  *ptr_iop_w = p;
  return length;
}

// wuffs_private_impl__io_writer__limited_copy_u32_from_history_fast is like
// the wuffs_private_impl__io_writer__limited_copy_u32_from_history function
// above, but has stronger pre-conditions.
//
// The caller needs to prove that:
//  - length   >= 1
//  - length   <= (io2_w      - *ptr_iop_w)
//  - distance >= 1
//  - distance <= (*ptr_iop_w - io0_w)
static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_history_fast(
    uint8_t** ptr_iop_w,
    uint8_t* io0_w,
    uint8_t* io2_w,
    uint32_t length,
    uint32_t distance) {
  uint8_t* p = *ptr_iop_w;
  uint8_t* q = p - distance;
  uint32_t n = length;
  for (; n >= 3; n -= 3) {
    *p++ = *q++;
    *p++ = *q++;
    *p++ = *q++;
  }
  for (; n; n--) {
    *p++ = *q++;
  }
  *ptr_iop_w = p;
  return length;
}

// wuffs_private_impl__io_writer__limited_copy_u32_from_history_fast_return_cusp
// is like the
// wuffs_private_impl__io_writer__limited_copy_u32_from_history_fast function,
// but also returns the cusp: a byte pair (as a u16le) being the last byte of
// and next byte after the copied history.
//
// For example, if history was [10, 11, 12, 13, 14, 15, 16, 17, 18] then:
//  - copying l=3, d=8 produces [11, 12, 13] and the cusp is (13, 14).
//  - copying l=3, d=2 produces [17, 18, 17] and the cusp is (17, 18).
//
// The caller needs to prove that:
//  - length   >= 1
//  - length   <= (io2_w      - *ptr_iop_w)
//  - distance >= 1
//  - distance <= (*ptr_iop_w - io0_w)
static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_history_fast_return_cusp(
    uint8_t** ptr_iop_w,
    uint8_t* io0_w,
    uint8_t* io2_w,
    uint32_t length,
    uint32_t distance) {
  uint8_t* p = *ptr_iop_w;
  uint8_t* q = p - distance;
  uint32_t n = length;
  for (; n >= 3; n -= 3) {
    *p++ = *q++;
    *p++ = *q++;
    *p++ = *q++;
  }
  for (; n; n--) {
    *p++ = *q++;
  }
  *ptr_iop_w = p;
  return (uint32_t)wuffs_base__peek_u16le__no_bounds_check(q - 1);
}

// wuffs_private_impl__io_writer__limited_copy_u32_from_history_8_byte_chunks_distance_1_fast
// copies the previous byte (the one immediately before *ptr_iop_w), copying 8
// byte chunks at a time. Each chunk contains 8 repetitions of the same byte.
//
// In terms of number of bytes copied, length is rounded up to a multiple of 8.
// As a special case, a zero length rounds up to 8 (even though 0 is already a
// multiple of 8), since there is always at least one 8 byte chunk copied.
//
// In terms of advancing *ptr_iop_w, length is not rounded up.
//
// The caller needs to prove that:
//  - length       >= 1
//  - (length + 8) <= (io2_w      - *ptr_iop_w)
//  - distance     == 1
//  - distance     <= (*ptr_iop_w - io0_w)
static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_history_8_byte_chunks_distance_1_fast(
    uint8_t** ptr_iop_w,
    uint8_t* io0_w,
    uint8_t* io2_w,
    uint32_t length,
    uint32_t distance) {
  uint8_t* p = *ptr_iop_w;
  uint64_t x = p[-1];
  x |= x << 8;
  x |= x << 16;
  x |= x << 32;
  uint32_t n = length;
  while (1) {
    wuffs_base__poke_u64le__no_bounds_check(p, x);
    if (n <= 8) {
      p += n;
      break;
    }
    p += 8;
    n -= 8;
  }
  *ptr_iop_w = p;
  return length;
}

// wuffs_private_impl__io_writer__limited_copy_u32_from_history_8_byte_chunks_distance_1_fast_return_cusp
// copies the previous byte (the one immediately before *ptr_iop_w), copying 8
// byte chunks at a time. Each chunk contains 8 repetitions of the same byte.
// It also returns the cusp: a byte pair (as a u16le) being the last byte of
// and next byte after the copied history.
//
// In terms of number of bytes copied, length is rounded up to a multiple of 8.
// As a special case, a zero length rounds up to 8 (even though 0 is already a
// multiple of 8), since there is always at least one 8 byte chunk copied.
//
// In terms of advancing *ptr_iop_w, length is not rounded up.
//
// The caller needs to prove that:
//  - length       >= 1
//  - (length + 8) <= (io2_w      - *ptr_iop_w)
//  - distance     == 1
//  - distance     <= (*ptr_iop_w - io0_w)
static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_history_8_byte_chunks_distance_1_fast_return_cusp(
    uint8_t** ptr_iop_w,
    uint8_t* io0_w,
    uint8_t* io2_w,
    uint32_t length,
    uint32_t distance) {
  uint8_t* p = *ptr_iop_w;
  uint8_t* q = p - distance;
  uint64_t x = p[-1];
  x |= x << 8;
  x |= x << 16;
  x |= x << 32;
  uint32_t n = length;
  while (1) {
    wuffs_base__poke_u64le__no_bounds_check(p, x);
    if (n <= 8) {
      p += n;
      q += n;
      break;
    }
    p += 8;
    q += 8;
    n -= 8;
  }
  *ptr_iop_w = p;
  return (uint32_t)wuffs_base__peek_u16le__no_bounds_check(q - 1);
}

// wuffs_private_impl__io_writer__limited_copy_u32_from_history_8_byte_chunks_fast
// is like the
// wuffs_private_impl__io_writer__limited_copy_u32_from_history_fast function
// above, but copies 8 byte chunks at a time.
//
// In terms of number of bytes copied, length is rounded up to a multiple of 8.
// As a special case, a zero length rounds up to 8 (even though 0 is already a
// multiple of 8), since there is always at least one 8 byte chunk copied.
//
// In terms of advancing *ptr_iop_w, length is not rounded up.
//
// The caller needs to prove that:
//  - length       >= 1
//  - (length + 8) <= (io2_w      - *ptr_iop_w)
//  - distance     >= 8
//  - distance     <= (*ptr_iop_w - io0_w)
static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_history_8_byte_chunks_fast(
    uint8_t** ptr_iop_w,
    uint8_t* io0_w,
    uint8_t* io2_w,
    uint32_t length,
    uint32_t distance) {
  uint8_t* p = *ptr_iop_w;
  uint8_t* q = p - distance;
  uint32_t n = length;
  while (1) {
    memcpy(p, q, 8);
    if (n <= 8) {
      p += n;
      break;
    }
    p += 8;
    q += 8;
    n -= 8;
  }
  *ptr_iop_w = p;
  return length;
}

// wuffs_private_impl__io_writer__limited_copy_u32_from_history_8_byte_chunks_fast_return_cusp
// is like the
// wuffs_private_impl__io_writer__limited_copy_u32_from_history_fast function
// above, but copies 8 byte chunks at a time. It also returns the cusp: a byte
// pair (as a u16le) being the last byte of and next byte after the copied
// history.
//
// In terms of number of bytes copied, length is rounded up to a multiple of 8.
// As a special case, a zero length rounds up to 8 (even though 0 is already a
// multiple of 8), since there is always at least one 8 byte chunk copied.
//
// In terms of advancing *ptr_iop_w, length is not rounded up.
//
// The caller needs to prove that:
//  - length       >= 1
//  - (length + 8) <= (io2_w      - *ptr_iop_w)
//  - distance     >= 8
//  - distance     <= (*ptr_iop_w - io0_w)
static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_history_8_byte_chunks_fast_return_cusp(
    uint8_t** ptr_iop_w,
    uint8_t* io0_w,
    uint8_t* io2_w,
    uint32_t length,
    uint32_t distance) {
  uint8_t* p = *ptr_iop_w;
  uint8_t* q = p - distance;
  uint32_t n = length;
  while (1) {
    memcpy(p, q, 8);
    if (n <= 8) {
      p += n;
      q += n;
      break;
    }
    p += 8;
    q += 8;
    n -= 8;
  }
  *ptr_iop_w = p;
  return (uint32_t)wuffs_base__peek_u16le__no_bounds_check(q - 1);
}

static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_reader(
    uint8_t** ptr_iop_w,
    uint8_t* io2_w,
    uint32_t length,
    const uint8_t** ptr_iop_r,
    const uint8_t* io2_r) {
  uint8_t* iop_w = *ptr_iop_w;
  size_t n = length;
  if (n > ((size_t)(io2_w - iop_w))) {
    n = (size_t)(io2_w - iop_w);
  }
  const uint8_t* iop_r = *ptr_iop_r;
  if (n > ((size_t)(io2_r - iop_r))) {
    n = (size_t)(io2_r - iop_r);
  }
  if (n > 0) {
    memmove(iop_w, iop_r, n);
    *ptr_iop_w += n;
    *ptr_iop_r += n;
  }
  return (uint32_t)(n);
}

static inline uint32_t  //
wuffs_private_impl__io_writer__limited_copy_u32_from_slice(
    uint8_t** ptr_iop_w,
    uint8_t* io2_w,
    uint32_t length,
    wuffs_base__slice_u8 src) {
  uint8_t* iop_w = *ptr_iop_w;
  size_t n = src.len;
  if (n > length) {
    n = length;
  }
  if (n > ((size_t)(io2_w - iop_w))) {
    n = (size_t)(io2_w - iop_w);
  }
  if (n > 0) {
    memmove(iop_w, src.ptr, n);
    *ptr_iop_w += n;
  }
  return (uint32_t)(n);
}

static inline wuffs_base__io_buffer*  //
wuffs_private_impl__io_writer__set(wuffs_base__io_buffer* b,
                                   uint8_t** ptr_iop_w,
                                   uint8_t** ptr_io0_w,
                                   uint8_t** ptr_io1_w,
                                   uint8_t** ptr_io2_w,
                                   wuffs_base__slice_u8 data,
                                   uint64_t history_position) {
  b->data = data;
  b->meta.wi = 0;
  b->meta.ri = 0;
  b->meta.pos = history_position;
  b->meta.closed = false;

  *ptr_iop_w = data.ptr;
  *ptr_io0_w = data.ptr;
  *ptr_io1_w = data.ptr;
  *ptr_io2_w = data.ptr + data.len;

  return b;
}

// ---------------- I/O (Utility)

#define wuffs_base__utility__empty_io_reader wuffs_base__empty_io_reader
#define wuffs_base__utility__empty_io_writer wuffs_base__empty_io_writer
