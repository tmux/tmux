// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Fundamentals

// WUFFS_BASE__MAGIC is a magic number to check that initializers are called.
// It's not foolproof, given C doesn't automatically zero memory before use,
// but it should catch 99.99% of cases.
//
// Its (non-zero) value is arbitrary, based on md5sum("wuffs").
#define WUFFS_BASE__MAGIC ((uint32_t)0x3CCB6C71)

// WUFFS_BASE__DISABLED is a magic number to indicate that a non-recoverable
// error was previously encountered.
//
// Its (non-zero) value is arbitrary, based on md5sum("disabled").
#define WUFFS_BASE__DISABLED ((uint32_t)0x075AE3D2)

// Use switch cases for coroutine suspension points, similar to the technique
// in https://www.chiark.greenend.org.uk/~sgtatham/coroutines.html
//
// The implicit fallthrough is intentional.
//
// We use trivial macros instead of an explicit assignment and case statement
// so that clang-format doesn't get confused by the unusual "case"s.
#define WUFFS_BASE__COROUTINE_SUSPENSION_POINT_0 case 0:;
#define WUFFS_BASE__COROUTINE_SUSPENSION_POINT(n) \
  coro_susp_point = n;                            \
  case n:;

#define WUFFS_BASE__COROUTINE_SUSPENSION_POINT_MAYBE_SUSPEND(n) \
  if (!status.repr) {                                           \
    goto ok;                                                    \
  } else if (*status.repr != '$') {                             \
    goto exit;                                                  \
  }                                                             \
  coro_susp_point = n;                                          \
  goto suspend;                                                 \
  case n:;

// The "defined(__clang__)" isn't redundant. While vanilla clang defines
// __GNUC__, clang-cl (which mimics MSVC's cl.exe) does not.
#if defined(__GNUC__) || defined(__clang__)
#define WUFFS_BASE__LIKELY(expr) (__builtin_expect(!!(expr), 1))
#define WUFFS_BASE__UNLIKELY(expr) (__builtin_expect(!!(expr), 0))
#else
#define WUFFS_BASE__LIKELY(expr) (expr)
#define WUFFS_BASE__UNLIKELY(expr) (expr)
#endif

// --------

static inline wuffs_base__empty_struct  //
wuffs_private_impl__ignore_status(wuffs_base__status z) {
  return wuffs_base__make_empty_struct();
}

static inline wuffs_base__status  //
wuffs_private_impl__status__ensure_not_a_suspension(wuffs_base__status z) {
  if (z.repr && (*z.repr == '$')) {
    z.repr = wuffs_base__error__cannot_return_a_suspension;
  }
  return z;
}

// --------

// wuffs_private_impl__iterate_total_advance returns the exclusive
// pointer-offset at which iteration should stop. The overall slice has length
// total_len, each iteration's sub-slice has length iter_len and are placed
// iter_advance apart.
//
// The iter_advance may not be larger than iter_len. The iter_advance may be
// smaller than iter_len, in which case the sub-slices will overlap.
//
// The return value r satisfies ((0 <= r) && (r <= total_len)).
//
// For example, if total_len = 15, iter_len = 5 and iter_advance = 3, there are
// four iterations at offsets 0, 3, 6 and 9. This function returns 12.
//
// 0123456789012345
// [....]
//    [....]
//       [....]
//          [....]
//             $
// 0123456789012345
//
// For example, if total_len = 15, iter_len = 5 and iter_advance = 5, there are
// three iterations at offsets 0, 5 and 10. This function returns 15.
//
// 0123456789012345
// [....]
//      [....]
//           [....]
//                $
// 0123456789012345
static inline size_t  //
wuffs_private_impl__iterate_total_advance(size_t total_len,
                                          size_t iter_len,
                                          size_t iter_advance) {
  if (total_len >= iter_len) {
    size_t n = total_len - iter_len;
    return ((n / iter_advance) * iter_advance) + iter_advance;
  }
  return 0;
}

// ---------------- Numeric Types

extern const uint8_t wuffs_private_impl__low_bits_mask__u8[8];
extern const uint16_t wuffs_private_impl__low_bits_mask__u16[16];
extern const uint32_t wuffs_private_impl__low_bits_mask__u32[32];
extern const uint64_t wuffs_private_impl__low_bits_mask__u64[64];

#define WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U8(n) \
  (wuffs_private_impl__low_bits_mask__u8[n])
#define WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U16(n) \
  (wuffs_private_impl__low_bits_mask__u16[n])
#define WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U32(n) \
  (wuffs_private_impl__low_bits_mask__u32[n])
#define WUFFS_PRIVATE_IMPL__LOW_BITS_MASK__U64(n) \
  (wuffs_private_impl__low_bits_mask__u64[n])

// --------

static inline void  //
wuffs_private_impl__u8__sat_add_indirect(uint8_t* x, uint8_t y) {
  *x = wuffs_base__u8__sat_add(*x, y);
}

static inline void  //
wuffs_private_impl__u8__sat_sub_indirect(uint8_t* x, uint8_t y) {
  *x = wuffs_base__u8__sat_sub(*x, y);
}

static inline void  //
wuffs_private_impl__u16__sat_add_indirect(uint16_t* x, uint16_t y) {
  *x = wuffs_base__u16__sat_add(*x, y);
}

static inline void  //
wuffs_private_impl__u16__sat_sub_indirect(uint16_t* x, uint16_t y) {
  *x = wuffs_base__u16__sat_sub(*x, y);
}

static inline void  //
wuffs_private_impl__u32__sat_add_indirect(uint32_t* x, uint32_t y) {
  *x = wuffs_base__u32__sat_add(*x, y);
}

static inline void  //
wuffs_private_impl__u32__sat_sub_indirect(uint32_t* x, uint32_t y) {
  *x = wuffs_base__u32__sat_sub(*x, y);
}

static inline void  //
wuffs_private_impl__u64__sat_add_indirect(uint64_t* x, uint64_t y) {
  *x = wuffs_base__u64__sat_add(*x, y);
}

static inline void  //
wuffs_private_impl__u64__sat_sub_indirect(uint64_t* x, uint64_t y) {
  *x = wuffs_base__u64__sat_sub(*x, y);
}

// ---------------- Numeric Types (Utility)

#define wuffs_base__utility__i64_divide(a, b) \
  ((uint64_t)(((int64_t)(a)) / ((int64_t)(b))))

#define wuffs_base__utility__sign_extend_convert_u8_u32(a) \
  ((uint32_t)(int32_t)(int8_t)(a))

#define wuffs_base__utility__sign_extend_convert_u8_u64(a) \
  ((uint64_t)(int64_t)(int8_t)(a))

#define wuffs_base__utility__sign_extend_convert_u16_u32(a) \
  ((uint32_t)(int32_t)(int16_t)(a))

#define wuffs_base__utility__sign_extend_convert_u16_u64(a) \
  ((uint64_t)(int64_t)(int16_t)(a))

#define wuffs_base__utility__sign_extend_convert_u32_u64(a) \
  ((uint64_t)(int64_t)(int32_t)(a))

#define wuffs_base__utility__sign_extend_rshift_u32(a, n) \
  ((uint32_t)(((int32_t)(a)) >> (n)))

#define wuffs_base__utility__sign_extend_rshift_u64(a, n) \
  ((uint64_t)(((int64_t)(a)) >> (n)))

#define wuffs_base__utility__make_bitvec256(e00, e01, e02, e03) \
  wuffs_base__make_bitvec256(e00, e01, e02, e03)

#define wuffs_base__utility__make_optional_u63(h, v) \
  wuffs_base__make_optional_u63(h, v)

// ---------------- Slices and Tables

// This function basically returns (ptr + len), except that that expression is
// Undefined Behavior in C (but not C++) when ptr is NULL, even if len is zero.
//
// Precondition: (ptr != NULL) || (len == 0).
static inline const uint8_t*  //
wuffs_private_impl__ptr_u8_plus_len(const uint8_t* ptr, size_t len) {
  return ptr ? (ptr + len) : NULL;
}

// --------

// wuffs_private_impl__slice_u8__prefix returns up to the first up_to bytes of
// s.
static inline wuffs_base__slice_u8  //
wuffs_private_impl__slice_u8__prefix(wuffs_base__slice_u8 s, uint64_t up_to) {
  if (((uint64_t)(s.len)) > up_to) {
    s.len = ((size_t)up_to);
  }
  return s;
}

// wuffs_private_impl__slice_u8__suffix returns up to the last up_to bytes of
// s.
static inline wuffs_base__slice_u8  //
wuffs_private_impl__slice_u8__suffix(wuffs_base__slice_u8 s, uint64_t up_to) {
  if (((uint64_t)(s.len)) > up_to) {
    s.ptr += ((uint64_t)(s.len)) - up_to;
    s.len = ((size_t)up_to);
  }
  return s;
}

// wuffs_private_impl__slice_u8__copy_from_slice calls memmove(dst.ptr,
// src.ptr, len) where len is the minimum of dst.len and src.len.
//
// Passing a wuffs_base__slice_u8 with all fields NULL or zero (a valid, empty
// slice) is valid and results in a no-op.
static inline uint64_t  //
wuffs_private_impl__slice_u8__copy_from_slice(wuffs_base__slice_u8 dst,
                                              wuffs_base__slice_u8 src) {
  size_t len = dst.len < src.len ? dst.len : src.len;
  if (len > 0) {
    memmove(dst.ptr, src.ptr, len);
  }
  return len;
}

static inline wuffs_base__empty_struct  //
wuffs_private_impl__bulk_load_host_endian(void* ptr,
                                          size_t len,
                                          wuffs_base__slice_u8 src) {
  if (len && (len <= src.len)) {
    memmove(ptr, src.ptr, len);
  }
  return wuffs_base__make_empty_struct();
}

static inline wuffs_base__empty_struct  //
wuffs_private_impl__bulk_memset(void* ptr, size_t len, uint8_t byte_value) {
  if (len) {
    memset(ptr, byte_value, len);
  }
  return wuffs_base__make_empty_struct();
}

static inline wuffs_base__empty_struct  //
wuffs_private_impl__bulk_save_host_endian(void* ptr,
                                          size_t len,
                                          wuffs_base__slice_u8 dst) {
  if (len && (len <= dst.len)) {
    memmove(dst.ptr, ptr, len);
  }
  return wuffs_base__make_empty_struct();
}

// --------

static inline wuffs_base__slice_u8  //
wuffs_private_impl__table_u8__row_u32(wuffs_base__table_u8 t, uint32_t y) {
  if (t.ptr && (y < t.height)) {
    return wuffs_base__make_slice_u8(t.ptr + (t.stride * y), t.width);
  }
  return wuffs_base__empty_slice_u8();
}

// ---------------- Slices and Tables (Utility)

#define wuffs_base__utility__empty_slice_u8 wuffs_base__empty_slice_u8
