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

// See https://github.com/google/wuffs/blob/main/doc/note/ranges-and-rects.md

typedef struct wuffs_base__range_ii_u32__struct {
  uint32_t min_incl;
  uint32_t max_incl;

#ifdef __cplusplus
  inline bool is_empty() const;
  inline bool equals(wuffs_base__range_ii_u32__struct s) const;
  inline wuffs_base__range_ii_u32__struct intersect(
      wuffs_base__range_ii_u32__struct s) const;
  inline wuffs_base__range_ii_u32__struct unite(
      wuffs_base__range_ii_u32__struct s) const;
  inline bool contains(uint32_t x) const;
  inline bool contains_range(wuffs_base__range_ii_u32__struct s) const;
#endif  // __cplusplus

} wuffs_base__range_ii_u32;

static inline wuffs_base__range_ii_u32  //
wuffs_base__empty_range_ii_u32(void) {
  wuffs_base__range_ii_u32 ret;
  ret.min_incl = 0;
  ret.max_incl = 0;
  return ret;
}

static inline wuffs_base__range_ii_u32  //
wuffs_base__make_range_ii_u32(uint32_t min_incl, uint32_t max_incl) {
  wuffs_base__range_ii_u32 ret;
  ret.min_incl = min_incl;
  ret.max_incl = max_incl;
  return ret;
}

static inline bool  //
wuffs_base__range_ii_u32__is_empty(const wuffs_base__range_ii_u32* r) {
  return r->min_incl > r->max_incl;
}

static inline bool  //
wuffs_base__range_ii_u32__equals(const wuffs_base__range_ii_u32* r,
                                 wuffs_base__range_ii_u32 s) {
  return (r->min_incl == s.min_incl && r->max_incl == s.max_incl) ||
         (wuffs_base__range_ii_u32__is_empty(r) &&
          wuffs_base__range_ii_u32__is_empty(&s));
}

static inline wuffs_base__range_ii_u32  //
wuffs_base__range_ii_u32__intersect(const wuffs_base__range_ii_u32* r,
                                    wuffs_base__range_ii_u32 s) {
  wuffs_base__range_ii_u32 t;
  t.min_incl = wuffs_base__u32__max(r->min_incl, s.min_incl);
  t.max_incl = wuffs_base__u32__min(r->max_incl, s.max_incl);
  return t;
}

static inline wuffs_base__range_ii_u32  //
wuffs_base__range_ii_u32__unite(const wuffs_base__range_ii_u32* r,
                                wuffs_base__range_ii_u32 s) {
  if (wuffs_base__range_ii_u32__is_empty(r)) {
    return s;
  }
  if (wuffs_base__range_ii_u32__is_empty(&s)) {
    return *r;
  }
  wuffs_base__range_ii_u32 t;
  t.min_incl = wuffs_base__u32__min(r->min_incl, s.min_incl);
  t.max_incl = wuffs_base__u32__max(r->max_incl, s.max_incl);
  return t;
}

static inline bool  //
wuffs_base__range_ii_u32__contains(const wuffs_base__range_ii_u32* r,
                                   uint32_t x) {
  return (r->min_incl <= x) && (x <= r->max_incl);
}

static inline bool  //
wuffs_base__range_ii_u32__contains_range(const wuffs_base__range_ii_u32* r,
                                         wuffs_base__range_ii_u32 s) {
  return wuffs_base__range_ii_u32__equals(
      &s, wuffs_base__range_ii_u32__intersect(r, s));
}

#ifdef __cplusplus

inline bool  //
wuffs_base__range_ii_u32::is_empty() const {
  return wuffs_base__range_ii_u32__is_empty(this);
}

inline bool  //
wuffs_base__range_ii_u32::equals(wuffs_base__range_ii_u32 s) const {
  return wuffs_base__range_ii_u32__equals(this, s);
}

inline wuffs_base__range_ii_u32  //
wuffs_base__range_ii_u32::intersect(wuffs_base__range_ii_u32 s) const {
  return wuffs_base__range_ii_u32__intersect(this, s);
}

inline wuffs_base__range_ii_u32  //
wuffs_base__range_ii_u32::unite(wuffs_base__range_ii_u32 s) const {
  return wuffs_base__range_ii_u32__unite(this, s);
}

inline bool  //
wuffs_base__range_ii_u32::contains(uint32_t x) const {
  return wuffs_base__range_ii_u32__contains(this, x);
}

inline bool  //
wuffs_base__range_ii_u32::contains_range(wuffs_base__range_ii_u32 s) const {
  return wuffs_base__range_ii_u32__contains_range(this, s);
}

#endif  // __cplusplus

// --------

typedef struct wuffs_base__range_ie_u32__struct {
  uint32_t min_incl;
  uint32_t max_excl;

#ifdef __cplusplus
  inline bool is_empty() const;
  inline bool equals(wuffs_base__range_ie_u32__struct s) const;
  inline wuffs_base__range_ie_u32__struct intersect(
      wuffs_base__range_ie_u32__struct s) const;
  inline wuffs_base__range_ie_u32__struct unite(
      wuffs_base__range_ie_u32__struct s) const;
  inline bool contains(uint32_t x) const;
  inline bool contains_range(wuffs_base__range_ie_u32__struct s) const;
  inline uint32_t length() const;
#endif  // __cplusplus

} wuffs_base__range_ie_u32;

static inline wuffs_base__range_ie_u32  //
wuffs_base__empty_range_ie_u32(void) {
  wuffs_base__range_ie_u32 ret;
  ret.min_incl = 0;
  ret.max_excl = 0;
  return ret;
}

static inline wuffs_base__range_ie_u32  //
wuffs_base__make_range_ie_u32(uint32_t min_incl, uint32_t max_excl) {
  wuffs_base__range_ie_u32 ret;
  ret.min_incl = min_incl;
  ret.max_excl = max_excl;
  return ret;
}

static inline bool  //
wuffs_base__range_ie_u32__is_empty(const wuffs_base__range_ie_u32* r) {
  return r->min_incl >= r->max_excl;
}

static inline bool  //
wuffs_base__range_ie_u32__equals(const wuffs_base__range_ie_u32* r,
                                 wuffs_base__range_ie_u32 s) {
  return (r->min_incl == s.min_incl && r->max_excl == s.max_excl) ||
         (wuffs_base__range_ie_u32__is_empty(r) &&
          wuffs_base__range_ie_u32__is_empty(&s));
}

static inline wuffs_base__range_ie_u32  //
wuffs_base__range_ie_u32__intersect(const wuffs_base__range_ie_u32* r,
                                    wuffs_base__range_ie_u32 s) {
  wuffs_base__range_ie_u32 t;
  t.min_incl = wuffs_base__u32__max(r->min_incl, s.min_incl);
  t.max_excl = wuffs_base__u32__min(r->max_excl, s.max_excl);
  return t;
}

static inline wuffs_base__range_ie_u32  //
wuffs_base__range_ie_u32__unite(const wuffs_base__range_ie_u32* r,
                                wuffs_base__range_ie_u32 s) {
  if (wuffs_base__range_ie_u32__is_empty(r)) {
    return s;
  }
  if (wuffs_base__range_ie_u32__is_empty(&s)) {
    return *r;
  }
  wuffs_base__range_ie_u32 t;
  t.min_incl = wuffs_base__u32__min(r->min_incl, s.min_incl);
  t.max_excl = wuffs_base__u32__max(r->max_excl, s.max_excl);
  return t;
}

static inline bool  //
wuffs_base__range_ie_u32__contains(const wuffs_base__range_ie_u32* r,
                                   uint32_t x) {
  return (r->min_incl <= x) && (x < r->max_excl);
}

static inline bool  //
wuffs_base__range_ie_u32__contains_range(const wuffs_base__range_ie_u32* r,
                                         wuffs_base__range_ie_u32 s) {
  return wuffs_base__range_ie_u32__equals(
      &s, wuffs_base__range_ie_u32__intersect(r, s));
}

static inline uint32_t  //
wuffs_base__range_ie_u32__length(const wuffs_base__range_ie_u32* r) {
  return wuffs_base__u32__sat_sub(r->max_excl, r->min_incl);
}

#ifdef __cplusplus

inline bool  //
wuffs_base__range_ie_u32::is_empty() const {
  return wuffs_base__range_ie_u32__is_empty(this);
}

inline bool  //
wuffs_base__range_ie_u32::equals(wuffs_base__range_ie_u32 s) const {
  return wuffs_base__range_ie_u32__equals(this, s);
}

inline wuffs_base__range_ie_u32  //
wuffs_base__range_ie_u32::intersect(wuffs_base__range_ie_u32 s) const {
  return wuffs_base__range_ie_u32__intersect(this, s);
}

inline wuffs_base__range_ie_u32  //
wuffs_base__range_ie_u32::unite(wuffs_base__range_ie_u32 s) const {
  return wuffs_base__range_ie_u32__unite(this, s);
}

inline bool  //
wuffs_base__range_ie_u32::contains(uint32_t x) const {
  return wuffs_base__range_ie_u32__contains(this, x);
}

inline bool  //
wuffs_base__range_ie_u32::contains_range(wuffs_base__range_ie_u32 s) const {
  return wuffs_base__range_ie_u32__contains_range(this, s);
}

inline uint32_t  //
wuffs_base__range_ie_u32::length() const {
  return wuffs_base__range_ie_u32__length(this);
}

#endif  // __cplusplus

// --------

typedef struct wuffs_base__range_ii_u64__struct {
  uint64_t min_incl;
  uint64_t max_incl;

#ifdef __cplusplus
  inline bool is_empty() const;
  inline bool equals(wuffs_base__range_ii_u64__struct s) const;
  inline wuffs_base__range_ii_u64__struct intersect(
      wuffs_base__range_ii_u64__struct s) const;
  inline wuffs_base__range_ii_u64__struct unite(
      wuffs_base__range_ii_u64__struct s) const;
  inline bool contains(uint64_t x) const;
  inline bool contains_range(wuffs_base__range_ii_u64__struct s) const;
#endif  // __cplusplus

} wuffs_base__range_ii_u64;

static inline wuffs_base__range_ii_u64  //
wuffs_base__empty_range_ii_u64(void) {
  wuffs_base__range_ii_u64 ret;
  ret.min_incl = 0;
  ret.max_incl = 0;
  return ret;
}

static inline wuffs_base__range_ii_u64  //
wuffs_base__make_range_ii_u64(uint64_t min_incl, uint64_t max_incl) {
  wuffs_base__range_ii_u64 ret;
  ret.min_incl = min_incl;
  ret.max_incl = max_incl;
  return ret;
}

static inline bool  //
wuffs_base__range_ii_u64__is_empty(const wuffs_base__range_ii_u64* r) {
  return r->min_incl > r->max_incl;
}

static inline bool  //
wuffs_base__range_ii_u64__equals(const wuffs_base__range_ii_u64* r,
                                 wuffs_base__range_ii_u64 s) {
  return (r->min_incl == s.min_incl && r->max_incl == s.max_incl) ||
         (wuffs_base__range_ii_u64__is_empty(r) &&
          wuffs_base__range_ii_u64__is_empty(&s));
}

static inline wuffs_base__range_ii_u64  //
wuffs_base__range_ii_u64__intersect(const wuffs_base__range_ii_u64* r,
                                    wuffs_base__range_ii_u64 s) {
  wuffs_base__range_ii_u64 t;
  t.min_incl = wuffs_base__u64__max(r->min_incl, s.min_incl);
  t.max_incl = wuffs_base__u64__min(r->max_incl, s.max_incl);
  return t;
}

static inline wuffs_base__range_ii_u64  //
wuffs_base__range_ii_u64__unite(const wuffs_base__range_ii_u64* r,
                                wuffs_base__range_ii_u64 s) {
  if (wuffs_base__range_ii_u64__is_empty(r)) {
    return s;
  }
  if (wuffs_base__range_ii_u64__is_empty(&s)) {
    return *r;
  }
  wuffs_base__range_ii_u64 t;
  t.min_incl = wuffs_base__u64__min(r->min_incl, s.min_incl);
  t.max_incl = wuffs_base__u64__max(r->max_incl, s.max_incl);
  return t;
}

static inline bool  //
wuffs_base__range_ii_u64__contains(const wuffs_base__range_ii_u64* r,
                                   uint64_t x) {
  return (r->min_incl <= x) && (x <= r->max_incl);
}

static inline bool  //
wuffs_base__range_ii_u64__contains_range(const wuffs_base__range_ii_u64* r,
                                         wuffs_base__range_ii_u64 s) {
  return wuffs_base__range_ii_u64__equals(
      &s, wuffs_base__range_ii_u64__intersect(r, s));
}

#ifdef __cplusplus

inline bool  //
wuffs_base__range_ii_u64::is_empty() const {
  return wuffs_base__range_ii_u64__is_empty(this);
}

inline bool  //
wuffs_base__range_ii_u64::equals(wuffs_base__range_ii_u64 s) const {
  return wuffs_base__range_ii_u64__equals(this, s);
}

inline wuffs_base__range_ii_u64  //
wuffs_base__range_ii_u64::intersect(wuffs_base__range_ii_u64 s) const {
  return wuffs_base__range_ii_u64__intersect(this, s);
}

inline wuffs_base__range_ii_u64  //
wuffs_base__range_ii_u64::unite(wuffs_base__range_ii_u64 s) const {
  return wuffs_base__range_ii_u64__unite(this, s);
}

inline bool  //
wuffs_base__range_ii_u64::contains(uint64_t x) const {
  return wuffs_base__range_ii_u64__contains(this, x);
}

inline bool  //
wuffs_base__range_ii_u64::contains_range(wuffs_base__range_ii_u64 s) const {
  return wuffs_base__range_ii_u64__contains_range(this, s);
}

#endif  // __cplusplus

// --------

typedef struct wuffs_base__range_ie_u64__struct {
  uint64_t min_incl;
  uint64_t max_excl;

#ifdef __cplusplus
  inline bool is_empty() const;
  inline bool equals(wuffs_base__range_ie_u64__struct s) const;
  inline wuffs_base__range_ie_u64__struct intersect(
      wuffs_base__range_ie_u64__struct s) const;
  inline wuffs_base__range_ie_u64__struct unite(
      wuffs_base__range_ie_u64__struct s) const;
  inline bool contains(uint64_t x) const;
  inline bool contains_range(wuffs_base__range_ie_u64__struct s) const;
  inline uint64_t length() const;
#endif  // __cplusplus

} wuffs_base__range_ie_u64;

static inline wuffs_base__range_ie_u64  //
wuffs_base__empty_range_ie_u64(void) {
  wuffs_base__range_ie_u64 ret;
  ret.min_incl = 0;
  ret.max_excl = 0;
  return ret;
}

static inline wuffs_base__range_ie_u64  //
wuffs_base__make_range_ie_u64(uint64_t min_incl, uint64_t max_excl) {
  wuffs_base__range_ie_u64 ret;
  ret.min_incl = min_incl;
  ret.max_excl = max_excl;
  return ret;
}

static inline bool  //
wuffs_base__range_ie_u64__is_empty(const wuffs_base__range_ie_u64* r) {
  return r->min_incl >= r->max_excl;
}

static inline bool  //
wuffs_base__range_ie_u64__equals(const wuffs_base__range_ie_u64* r,
                                 wuffs_base__range_ie_u64 s) {
  return (r->min_incl == s.min_incl && r->max_excl == s.max_excl) ||
         (wuffs_base__range_ie_u64__is_empty(r) &&
          wuffs_base__range_ie_u64__is_empty(&s));
}

static inline wuffs_base__range_ie_u64  //
wuffs_base__range_ie_u64__intersect(const wuffs_base__range_ie_u64* r,
                                    wuffs_base__range_ie_u64 s) {
  wuffs_base__range_ie_u64 t;
  t.min_incl = wuffs_base__u64__max(r->min_incl, s.min_incl);
  t.max_excl = wuffs_base__u64__min(r->max_excl, s.max_excl);
  return t;
}

static inline wuffs_base__range_ie_u64  //
wuffs_base__range_ie_u64__unite(const wuffs_base__range_ie_u64* r,
                                wuffs_base__range_ie_u64 s) {
  if (wuffs_base__range_ie_u64__is_empty(r)) {
    return s;
  }
  if (wuffs_base__range_ie_u64__is_empty(&s)) {
    return *r;
  }
  wuffs_base__range_ie_u64 t;
  t.min_incl = wuffs_base__u64__min(r->min_incl, s.min_incl);
  t.max_excl = wuffs_base__u64__max(r->max_excl, s.max_excl);
  return t;
}

static inline bool  //
wuffs_base__range_ie_u64__contains(const wuffs_base__range_ie_u64* r,
                                   uint64_t x) {
  return (r->min_incl <= x) && (x < r->max_excl);
}

static inline bool  //
wuffs_base__range_ie_u64__contains_range(const wuffs_base__range_ie_u64* r,
                                         wuffs_base__range_ie_u64 s) {
  return wuffs_base__range_ie_u64__equals(
      &s, wuffs_base__range_ie_u64__intersect(r, s));
}

static inline uint64_t  //
wuffs_base__range_ie_u64__length(const wuffs_base__range_ie_u64* r) {
  return wuffs_base__u64__sat_sub(r->max_excl, r->min_incl);
}

#ifdef __cplusplus

inline bool  //
wuffs_base__range_ie_u64::is_empty() const {
  return wuffs_base__range_ie_u64__is_empty(this);
}

inline bool  //
wuffs_base__range_ie_u64::equals(wuffs_base__range_ie_u64 s) const {
  return wuffs_base__range_ie_u64__equals(this, s);
}

inline wuffs_base__range_ie_u64  //
wuffs_base__range_ie_u64::intersect(wuffs_base__range_ie_u64 s) const {
  return wuffs_base__range_ie_u64__intersect(this, s);
}

inline wuffs_base__range_ie_u64  //
wuffs_base__range_ie_u64::unite(wuffs_base__range_ie_u64 s) const {
  return wuffs_base__range_ie_u64__unite(this, s);
}

inline bool  //
wuffs_base__range_ie_u64::contains(uint64_t x) const {
  return wuffs_base__range_ie_u64__contains(this, x);
}

inline bool  //
wuffs_base__range_ie_u64::contains_range(wuffs_base__range_ie_u64 s) const {
  return wuffs_base__range_ie_u64__contains_range(this, s);
}

inline uint64_t  //
wuffs_base__range_ie_u64::length() const {
  return wuffs_base__range_ie_u64__length(this);
}

#endif  // __cplusplus

// --------

typedef struct wuffs_base__rect_ii_u32__struct {
  uint32_t min_incl_x;
  uint32_t min_incl_y;
  uint32_t max_incl_x;
  uint32_t max_incl_y;

#ifdef __cplusplus
  inline bool is_empty() const;
  inline bool equals(wuffs_base__rect_ii_u32__struct s) const;
  inline wuffs_base__rect_ii_u32__struct intersect(
      wuffs_base__rect_ii_u32__struct s) const;
  inline wuffs_base__rect_ii_u32__struct unite(
      wuffs_base__rect_ii_u32__struct s) const;
  inline bool contains(uint32_t x, uint32_t y) const;
  inline bool contains_rect(wuffs_base__rect_ii_u32__struct s) const;
#endif  // __cplusplus

} wuffs_base__rect_ii_u32;

static inline wuffs_base__rect_ii_u32  //
wuffs_base__empty_rect_ii_u32(void) {
  wuffs_base__rect_ii_u32 ret;
  ret.min_incl_x = 0;
  ret.min_incl_y = 0;
  ret.max_incl_x = 0;
  ret.max_incl_y = 0;
  return ret;
}

static inline wuffs_base__rect_ii_u32  //
wuffs_base__make_rect_ii_u32(uint32_t min_incl_x,
                             uint32_t min_incl_y,
                             uint32_t max_incl_x,
                             uint32_t max_incl_y) {
  wuffs_base__rect_ii_u32 ret;
  ret.min_incl_x = min_incl_x;
  ret.min_incl_y = min_incl_y;
  ret.max_incl_x = max_incl_x;
  ret.max_incl_y = max_incl_y;
  return ret;
}

static inline bool  //
wuffs_base__rect_ii_u32__is_empty(const wuffs_base__rect_ii_u32* r) {
  return (r->min_incl_x > r->max_incl_x) || (r->min_incl_y > r->max_incl_y);
}

static inline bool  //
wuffs_base__rect_ii_u32__equals(const wuffs_base__rect_ii_u32* r,
                                wuffs_base__rect_ii_u32 s) {
  return (r->min_incl_x == s.min_incl_x && r->min_incl_y == s.min_incl_y &&
          r->max_incl_x == s.max_incl_x && r->max_incl_y == s.max_incl_y) ||
         (wuffs_base__rect_ii_u32__is_empty(r) &&
          wuffs_base__rect_ii_u32__is_empty(&s));
}

static inline wuffs_base__rect_ii_u32  //
wuffs_base__rect_ii_u32__intersect(const wuffs_base__rect_ii_u32* r,
                                   wuffs_base__rect_ii_u32 s) {
  wuffs_base__rect_ii_u32 t;
  t.min_incl_x = wuffs_base__u32__max(r->min_incl_x, s.min_incl_x);
  t.min_incl_y = wuffs_base__u32__max(r->min_incl_y, s.min_incl_y);
  t.max_incl_x = wuffs_base__u32__min(r->max_incl_x, s.max_incl_x);
  t.max_incl_y = wuffs_base__u32__min(r->max_incl_y, s.max_incl_y);
  return t;
}

static inline wuffs_base__rect_ii_u32  //
wuffs_base__rect_ii_u32__unite(const wuffs_base__rect_ii_u32* r,
                               wuffs_base__rect_ii_u32 s) {
  if (wuffs_base__rect_ii_u32__is_empty(r)) {
    return s;
  }
  if (wuffs_base__rect_ii_u32__is_empty(&s)) {
    return *r;
  }
  wuffs_base__rect_ii_u32 t;
  t.min_incl_x = wuffs_base__u32__min(r->min_incl_x, s.min_incl_x);
  t.min_incl_y = wuffs_base__u32__min(r->min_incl_y, s.min_incl_y);
  t.max_incl_x = wuffs_base__u32__max(r->max_incl_x, s.max_incl_x);
  t.max_incl_y = wuffs_base__u32__max(r->max_incl_y, s.max_incl_y);
  return t;
}

static inline bool  //
wuffs_base__rect_ii_u32__contains(const wuffs_base__rect_ii_u32* r,
                                  uint32_t x,
                                  uint32_t y) {
  return (r->min_incl_x <= x) && (x <= r->max_incl_x) && (r->min_incl_y <= y) &&
         (y <= r->max_incl_y);
}

static inline bool  //
wuffs_base__rect_ii_u32__contains_rect(const wuffs_base__rect_ii_u32* r,
                                       wuffs_base__rect_ii_u32 s) {
  return wuffs_base__rect_ii_u32__equals(
      &s, wuffs_base__rect_ii_u32__intersect(r, s));
}

#ifdef __cplusplus

inline bool  //
wuffs_base__rect_ii_u32::is_empty() const {
  return wuffs_base__rect_ii_u32__is_empty(this);
}

inline bool  //
wuffs_base__rect_ii_u32::equals(wuffs_base__rect_ii_u32 s) const {
  return wuffs_base__rect_ii_u32__equals(this, s);
}

inline wuffs_base__rect_ii_u32  //
wuffs_base__rect_ii_u32::intersect(wuffs_base__rect_ii_u32 s) const {
  return wuffs_base__rect_ii_u32__intersect(this, s);
}

inline wuffs_base__rect_ii_u32  //
wuffs_base__rect_ii_u32::unite(wuffs_base__rect_ii_u32 s) const {
  return wuffs_base__rect_ii_u32__unite(this, s);
}

inline bool  //
wuffs_base__rect_ii_u32::contains(uint32_t x, uint32_t y) const {
  return wuffs_base__rect_ii_u32__contains(this, x, y);
}

inline bool  //
wuffs_base__rect_ii_u32::contains_rect(wuffs_base__rect_ii_u32 s) const {
  return wuffs_base__rect_ii_u32__contains_rect(this, s);
}

#endif  // __cplusplus

// --------

typedef struct wuffs_base__rect_ie_u32__struct {
  uint32_t min_incl_x;
  uint32_t min_incl_y;
  uint32_t max_excl_x;
  uint32_t max_excl_y;

#ifdef __cplusplus
  inline bool is_empty() const;
  inline bool equals(wuffs_base__rect_ie_u32__struct s) const;
  inline wuffs_base__rect_ie_u32__struct intersect(
      wuffs_base__rect_ie_u32__struct s) const;
  inline wuffs_base__rect_ie_u32__struct unite(
      wuffs_base__rect_ie_u32__struct s) const;
  inline bool contains(uint32_t x, uint32_t y) const;
  inline bool contains_rect(wuffs_base__rect_ie_u32__struct s) const;
  inline uint32_t width() const;
  inline uint32_t height() const;
#endif  // __cplusplus

} wuffs_base__rect_ie_u32;

static inline wuffs_base__rect_ie_u32  //
wuffs_base__empty_rect_ie_u32(void) {
  wuffs_base__rect_ie_u32 ret;
  ret.min_incl_x = 0;
  ret.min_incl_y = 0;
  ret.max_excl_x = 0;
  ret.max_excl_y = 0;
  return ret;
}

static inline wuffs_base__rect_ie_u32  //
wuffs_base__make_rect_ie_u32(uint32_t min_incl_x,
                             uint32_t min_incl_y,
                             uint32_t max_excl_x,
                             uint32_t max_excl_y) {
  wuffs_base__rect_ie_u32 ret;
  ret.min_incl_x = min_incl_x;
  ret.min_incl_y = min_incl_y;
  ret.max_excl_x = max_excl_x;
  ret.max_excl_y = max_excl_y;
  return ret;
}

static inline bool  //
wuffs_base__rect_ie_u32__is_empty(const wuffs_base__rect_ie_u32* r) {
  return (r->min_incl_x >= r->max_excl_x) || (r->min_incl_y >= r->max_excl_y);
}

static inline bool  //
wuffs_base__rect_ie_u32__equals(const wuffs_base__rect_ie_u32* r,
                                wuffs_base__rect_ie_u32 s) {
  return (r->min_incl_x == s.min_incl_x && r->min_incl_y == s.min_incl_y &&
          r->max_excl_x == s.max_excl_x && r->max_excl_y == s.max_excl_y) ||
         (wuffs_base__rect_ie_u32__is_empty(r) &&
          wuffs_base__rect_ie_u32__is_empty(&s));
}

static inline wuffs_base__rect_ie_u32  //
wuffs_base__rect_ie_u32__intersect(const wuffs_base__rect_ie_u32* r,
                                   wuffs_base__rect_ie_u32 s) {
  wuffs_base__rect_ie_u32 t;
  t.min_incl_x = wuffs_base__u32__max(r->min_incl_x, s.min_incl_x);
  t.min_incl_y = wuffs_base__u32__max(r->min_incl_y, s.min_incl_y);
  t.max_excl_x = wuffs_base__u32__min(r->max_excl_x, s.max_excl_x);
  t.max_excl_y = wuffs_base__u32__min(r->max_excl_y, s.max_excl_y);
  return t;
}

static inline wuffs_base__rect_ie_u32  //
wuffs_base__rect_ie_u32__unite(const wuffs_base__rect_ie_u32* r,
                               wuffs_base__rect_ie_u32 s) {
  if (wuffs_base__rect_ie_u32__is_empty(r)) {
    return s;
  }
  if (wuffs_base__rect_ie_u32__is_empty(&s)) {
    return *r;
  }
  wuffs_base__rect_ie_u32 t;
  t.min_incl_x = wuffs_base__u32__min(r->min_incl_x, s.min_incl_x);
  t.min_incl_y = wuffs_base__u32__min(r->min_incl_y, s.min_incl_y);
  t.max_excl_x = wuffs_base__u32__max(r->max_excl_x, s.max_excl_x);
  t.max_excl_y = wuffs_base__u32__max(r->max_excl_y, s.max_excl_y);
  return t;
}

static inline bool  //
wuffs_base__rect_ie_u32__contains(const wuffs_base__rect_ie_u32* r,
                                  uint32_t x,
                                  uint32_t y) {
  return (r->min_incl_x <= x) && (x < r->max_excl_x) && (r->min_incl_y <= y) &&
         (y < r->max_excl_y);
}

static inline bool  //
wuffs_base__rect_ie_u32__contains_rect(const wuffs_base__rect_ie_u32* r,
                                       wuffs_base__rect_ie_u32 s) {
  return wuffs_base__rect_ie_u32__equals(
      &s, wuffs_base__rect_ie_u32__intersect(r, s));
}

static inline uint32_t  //
wuffs_base__rect_ie_u32__width(const wuffs_base__rect_ie_u32* r) {
  return wuffs_base__u32__sat_sub(r->max_excl_x, r->min_incl_x);
}

static inline uint32_t  //
wuffs_base__rect_ie_u32__height(const wuffs_base__rect_ie_u32* r) {
  return wuffs_base__u32__sat_sub(r->max_excl_y, r->min_incl_y);
}

#ifdef __cplusplus

inline bool  //
wuffs_base__rect_ie_u32::is_empty() const {
  return wuffs_base__rect_ie_u32__is_empty(this);
}

inline bool  //
wuffs_base__rect_ie_u32::equals(wuffs_base__rect_ie_u32 s) const {
  return wuffs_base__rect_ie_u32__equals(this, s);
}

inline wuffs_base__rect_ie_u32  //
wuffs_base__rect_ie_u32::intersect(wuffs_base__rect_ie_u32 s) const {
  return wuffs_base__rect_ie_u32__intersect(this, s);
}

inline wuffs_base__rect_ie_u32  //
wuffs_base__rect_ie_u32::unite(wuffs_base__rect_ie_u32 s) const {
  return wuffs_base__rect_ie_u32__unite(this, s);
}

inline bool  //
wuffs_base__rect_ie_u32::contains(uint32_t x, uint32_t y) const {
  return wuffs_base__rect_ie_u32__contains(this, x, y);
}

inline bool  //
wuffs_base__rect_ie_u32::contains_rect(wuffs_base__rect_ie_u32 s) const {
  return wuffs_base__rect_ie_u32__contains_rect(this, s);
}

inline uint32_t  //
wuffs_base__rect_ie_u32::width() const {
  return wuffs_base__rect_ie_u32__width(this);
}

inline uint32_t  //
wuffs_base__rect_ie_u32::height() const {
  return wuffs_base__rect_ie_u32__height(this);
}

#endif  // __cplusplus

// ---------------- More Information

// wuffs_base__more_information holds additional fields, typically when a Wuffs
// method returns a [note status](/doc/note/statuses.md).
//
// The flavor field follows the base38 namespace
// convention](/doc/note/base38-and-fourcc.md). The other fields' semantics
// depends on the flavor.
typedef struct wuffs_base__more_information__struct {
  uint32_t flavor;
  uint32_t w;
  uint64_t x;
  uint64_t y;
  uint64_t z;

#ifdef __cplusplus
  inline void set(uint32_t flavor_arg,
                  uint32_t w_arg,
                  uint64_t x_arg,
                  uint64_t y_arg,
                  uint64_t z_arg);
  inline uint32_t io_redirect__fourcc() const;
  inline wuffs_base__range_ie_u64 io_redirect__range() const;
  inline uint64_t io_seek__position() const;
  inline uint32_t metadata__fourcc() const;
  inline wuffs_base__range_ie_u64 metadata_raw_passthrough__range() const;
  inline int32_t metadata_parsed__chrm(uint32_t component) const;
  inline uint32_t metadata_parsed__gama() const;
  inline uint32_t metadata_parsed__srgb() const;
#endif  // __cplusplus

} wuffs_base__more_information;

#define WUFFS_BASE__MORE_INFORMATION__FLAVOR__IO_REDIRECT 1
#define WUFFS_BASE__MORE_INFORMATION__FLAVOR__IO_SEEK 2
#define WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_PASSTHROUGH 3
#define WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_TRANSFORM 4
#define WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_PARSED 5

static inline wuffs_base__more_information  //
wuffs_base__empty_more_information(void) {
  wuffs_base__more_information ret;
  ret.flavor = 0;
  ret.w = 0;
  ret.x = 0;
  ret.y = 0;
  ret.z = 0;
  return ret;
}

static inline void  //
wuffs_base__more_information__set(wuffs_base__more_information* m,
                                  uint32_t flavor,
                                  uint32_t w,
                                  uint64_t x,
                                  uint64_t y,
                                  uint64_t z) {
  if (!m) {
    return;
  }
  m->flavor = flavor;
  m->w = w;
  m->x = x;
  m->y = y;
  m->z = z;
}

static inline uint32_t  //
wuffs_base__more_information__io_redirect__fourcc(
    const wuffs_base__more_information* m) {
  return m->w;
}

static inline wuffs_base__range_ie_u64  //
wuffs_base__more_information__io_redirect__range(
    const wuffs_base__more_information* m) {
  wuffs_base__range_ie_u64 ret;
  ret.min_incl = m->y;
  ret.max_excl = m->z;
  return ret;
}

static inline uint64_t  //
wuffs_base__more_information__io_seek__position(
    const wuffs_base__more_information* m) {
  return m->x;
}

static inline uint32_t  //
wuffs_base__more_information__metadata__fourcc(
    const wuffs_base__more_information* m) {
  return m->w;
}

static inline wuffs_base__range_ie_u64  //
wuffs_base__more_information__metadata_raw_passthrough__range(
    const wuffs_base__more_information* m) {
  wuffs_base__range_ie_u64 ret;
  ret.min_incl = m->y;
  ret.max_excl = m->z;
  return ret;
}

#define WUFFS_BASE__MORE_INFORMATION__METADATA_PARSED__CHRM__WHITE_X 0
#define WUFFS_BASE__MORE_INFORMATION__METADATA_PARSED__CHRM__WHITE_Y 1
#define WUFFS_BASE__MORE_INFORMATION__METADATA_PARSED__CHRM__RED_X 2
#define WUFFS_BASE__MORE_INFORMATION__METADATA_PARSED__CHRM__RED_Y 3
#define WUFFS_BASE__MORE_INFORMATION__METADATA_PARSED__CHRM__GREEN_X 4
#define WUFFS_BASE__MORE_INFORMATION__METADATA_PARSED__CHRM__GREEN_Y 5
#define WUFFS_BASE__MORE_INFORMATION__METADATA_PARSED__CHRM__BLUE_X 6
#define WUFFS_BASE__MORE_INFORMATION__METADATA_PARSED__CHRM__BLUE_Y 7

// wuffs_base__more_information__metadata_parsed__chrm returns chromaticity
// values (scaled by 100000) like the PNG "cHRM" chunk. For example, the sRGB
// color space corresponds to:
//  - ETC__CHRM__WHITE_X 31270
//  - ETC__CHRM__WHITE_Y 32900
//  - ETC__CHRM__RED_X   64000
//  - ETC__CHRM__RED_Y   33000
//  - ETC__CHRM__GREEN_X 30000
//  - ETC__CHRM__GREEN_Y 60000
//  - ETC__CHRM__BLUE_X  15000
//  - ETC__CHRM__BLUE_Y   6000
//
// See
// https://ciechanow.ski/color-spaces/#chromaticity-and-white-point-coordinates
static inline int32_t  //
wuffs_base__more_information__metadata_parsed__chrm(
    const wuffs_base__more_information* m,
    uint32_t component) {
  // After the flavor and the w field (holding a FourCC), a
  // wuffs_base__more_information holds 24 bytes of data in three uint64_t
  // typed fields (x, y and z). We pack the eight chromaticity values (wx, wy,
  // rx, ..., by), basically int24_t values, into 24 bytes like this:
  //  -    LSB                 MSB
  //  - x: wx wx wx wy wy wy rx rx
  //  - y: rx ry ry ry gx gx gx gy
  //  - z: gy gy bx bx bx by by by
  uint32_t u = 0;
  switch (component & 7) {
    case 0:
      u = ((uint32_t)(m->x >> 0));
      break;
    case 1:
      u = ((uint32_t)(m->x >> 24));
      break;
    case 2:
      u = ((uint32_t)((m->x >> 48) | (m->y << 16)));
      break;
    case 3:
      u = ((uint32_t)(m->y >> 8));
      break;
    case 4:
      u = ((uint32_t)(m->y >> 32));
      break;
    case 5:
      u = ((uint32_t)((m->y >> 56) | (m->z << 8)));
      break;
    case 6:
      u = ((uint32_t)(m->z >> 16));
      break;
    case 7:
      u = ((uint32_t)(m->z >> 40));
      break;
  }
  // The left-right shifts sign-extend from 24-bit to 32-bit integers.
  return ((int32_t)(u << 8)) >> 8;
}

// wuffs_base__more_information__metadata_parsed__gama returns inverse gamma
// correction values (scaled by 100000) like the PNG "gAMA" chunk. For example,
// for gamma = 2.2, this returns 45455 (approximating 100000 / 2.2).
static inline uint32_t  //
wuffs_base__more_information__metadata_parsed__gama(
    const wuffs_base__more_information* m) {
  return ((uint32_t)(m->x));
}

#define WUFFS_BASE__SRGB_RENDERING_INTENT__PERCEPTUAL 0
#define WUFFS_BASE__SRGB_RENDERING_INTENT__RELATIVE_COLORIMETRIC 1
#define WUFFS_BASE__SRGB_RENDERING_INTENT__SATURATION 2
#define WUFFS_BASE__SRGB_RENDERING_INTENT__ABSOLUTE_COLORIMETRIC 3

// wuffs_base__more_information__metadata_parsed__srgb returns the sRGB
// rendering intent like the PNG "sRGB" chunk.
static inline uint32_t  //
wuffs_base__more_information__metadata_parsed__srgb(
    const wuffs_base__more_information* m) {
  return m->x & 3;
}

#ifdef __cplusplus

inline void  //
wuffs_base__more_information::set(uint32_t flavor_arg,
                                  uint32_t w_arg,
                                  uint64_t x_arg,
                                  uint64_t y_arg,
                                  uint64_t z_arg) {
  wuffs_base__more_information__set(this, flavor_arg, w_arg, x_arg, y_arg,
                                    z_arg);
}

inline uint32_t  //
wuffs_base__more_information::io_redirect__fourcc() const {
  return wuffs_base__more_information__io_redirect__fourcc(this);
}

inline wuffs_base__range_ie_u64  //
wuffs_base__more_information::io_redirect__range() const {
  return wuffs_base__more_information__io_redirect__range(this);
}

inline uint64_t  //
wuffs_base__more_information::io_seek__position() const {
  return wuffs_base__more_information__io_seek__position(this);
}

inline uint32_t  //
wuffs_base__more_information::metadata__fourcc() const {
  return wuffs_base__more_information__metadata__fourcc(this);
}

inline wuffs_base__range_ie_u64  //
wuffs_base__more_information::metadata_raw_passthrough__range() const {
  return wuffs_base__more_information__metadata_raw_passthrough__range(this);
}

inline int32_t  //
wuffs_base__more_information::metadata_parsed__chrm(uint32_t component) const {
  return wuffs_base__more_information__metadata_parsed__chrm(this, component);
}

inline uint32_t  //
wuffs_base__more_information::metadata_parsed__gama() const {
  return wuffs_base__more_information__metadata_parsed__gama(this);
}

inline uint32_t  //
wuffs_base__more_information::metadata_parsed__srgb() const {
  return wuffs_base__more_information__metadata_parsed__srgb(this);
}

#endif  // __cplusplus
