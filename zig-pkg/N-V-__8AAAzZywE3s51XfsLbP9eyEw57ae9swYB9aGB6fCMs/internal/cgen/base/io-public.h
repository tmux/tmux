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
//
// See (/doc/note/io-input-output.md).

// wuffs_base__io_buffer_meta is the metadata for a wuffs_base__io_buffer's
// data.
typedef struct wuffs_base__io_buffer_meta__struct {
  size_t wi;     // Write index. Invariant: wi <= len.
  size_t ri;     // Read  index. Invariant: ri <= wi.
  uint64_t pos;  // Buffer position (relative to the start of stream).
  bool closed;   // No further writes are expected.
} wuffs_base__io_buffer_meta;

// wuffs_base__io_buffer is a 1-dimensional buffer (a pointer and length) plus
// additional metadata.
//
// A value with all fields zero is a valid, empty buffer.
typedef struct wuffs_base__io_buffer__struct {
  wuffs_base__slice_u8 data;
  wuffs_base__io_buffer_meta meta;

#ifdef __cplusplus
  inline bool is_valid() const;
  inline void compact();
  inline void compact_retaining(uint64_t history_retain_length);
  inline size_t reader_length() const;
  inline uint8_t* reader_pointer() const;
  inline uint64_t reader_position() const;
  inline wuffs_base__slice_u8 reader_slice() const;
  inline size_t writer_length() const;
  inline uint8_t* writer_pointer() const;
  inline uint64_t writer_position() const;
  inline wuffs_base__slice_u8 writer_slice() const;
#endif  // __cplusplus

} wuffs_base__io_buffer;

static inline wuffs_base__io_buffer  //
wuffs_base__make_io_buffer(wuffs_base__slice_u8 data,
                           wuffs_base__io_buffer_meta meta) {
  wuffs_base__io_buffer ret;
  ret.data = data;
  ret.meta = meta;
  return ret;
}

static inline wuffs_base__io_buffer_meta  //
wuffs_base__make_io_buffer_meta(size_t wi,
                                size_t ri,
                                uint64_t pos,
                                bool closed) {
  wuffs_base__io_buffer_meta ret;
  ret.wi = wi;
  ret.ri = ri;
  ret.pos = pos;
  ret.closed = closed;
  return ret;
}

static inline wuffs_base__io_buffer  //
wuffs_base__ptr_u8__reader(uint8_t* ptr, size_t len, bool closed) {
  wuffs_base__io_buffer ret;
  ret.data.ptr = ptr;
  ret.data.len = len;
  ret.meta.wi = len;
  ret.meta.ri = 0;
  ret.meta.pos = 0;
  ret.meta.closed = closed;
  return ret;
}

static inline wuffs_base__io_buffer  //
wuffs_base__ptr_u8__writer(uint8_t* ptr, size_t len) {
  wuffs_base__io_buffer ret;
  ret.data.ptr = ptr;
  ret.data.len = len;
  ret.meta.wi = 0;
  ret.meta.ri = 0;
  ret.meta.pos = 0;
  ret.meta.closed = false;
  return ret;
}

static inline wuffs_base__io_buffer  //
wuffs_base__slice_u8__reader(wuffs_base__slice_u8 s, bool closed) {
  wuffs_base__io_buffer ret;
  ret.data.ptr = s.ptr;
  ret.data.len = s.len;
  ret.meta.wi = s.len;
  ret.meta.ri = 0;
  ret.meta.pos = 0;
  ret.meta.closed = closed;
  return ret;
}

static inline wuffs_base__io_buffer  //
wuffs_base__slice_u8__writer(wuffs_base__slice_u8 s) {
  wuffs_base__io_buffer ret;
  ret.data.ptr = s.ptr;
  ret.data.len = s.len;
  ret.meta.wi = 0;
  ret.meta.ri = 0;
  ret.meta.pos = 0;
  ret.meta.closed = false;
  return ret;
}

static inline wuffs_base__io_buffer  //
wuffs_base__empty_io_buffer(void) {
  wuffs_base__io_buffer ret;
  ret.data.ptr = NULL;
  ret.data.len = 0;
  ret.meta.wi = 0;
  ret.meta.ri = 0;
  ret.meta.pos = 0;
  ret.meta.closed = false;
  return ret;
}

static inline wuffs_base__io_buffer_meta  //
wuffs_base__empty_io_buffer_meta(void) {
  wuffs_base__io_buffer_meta ret;
  ret.wi = 0;
  ret.ri = 0;
  ret.pos = 0;
  ret.closed = false;
  return ret;
}

static inline bool  //
wuffs_base__io_buffer__is_valid(const wuffs_base__io_buffer* buf) {
  if (buf) {
    if (buf->data.ptr) {
      return (buf->meta.ri <= buf->meta.wi) && (buf->meta.wi <= buf->data.len);
    } else {
      return (buf->meta.ri == 0) && (buf->meta.wi == 0) && (buf->data.len == 0);
    }
  }
  return false;
}

// wuffs_base__io_buffer__compact moves any written but unread bytes to the
// start of the buffer.
static inline void  //
wuffs_base__io_buffer__compact(wuffs_base__io_buffer* buf) {
  if (!buf || (buf->meta.ri == 0)) {
    return;
  }
  buf->meta.pos = wuffs_base__u64__sat_add(buf->meta.pos, buf->meta.ri);
  size_t new_wi = buf->meta.wi - buf->meta.ri;
  if (new_wi != 0) {
    memmove(buf->data.ptr, buf->data.ptr + buf->meta.ri, new_wi);
  }
  buf->meta.wi = new_wi;
  buf->meta.ri = 0;
}

// wuffs_base__io_buffer__compact_retaining moves any written but unread bytes
// closer to the start of the buffer. It retains H bytes of history (the most
// recently read bytes), where H is min(buf->meta.ri, history_retain_length).
// It is therefore a no-op if history_retain_length is UINT64_MAX. A
// postcondition is that buf->meta.ri == H.
//
// wuffs_base__io_buffer__compact_retaining(0) is equivalent to
// wuffs_base__io_buffer__compact().
//
// For example, if buf started like this:
//
//        +--- ri = 3
//        v
//     abcdefgh??    len = 10, pos = 900
//             ^
//             +--- wi = 8
//
// Then, depending on history_retain_length, the resultant buf would be:
//
// HRL = 0     defgh?????    ri = 0    wi = 5    pos = 903
// HRL = 1     cdefgh????    ri = 1    wi = 6    pos = 902
// HRL = 2     bcdefgh???    ri = 2    wi = 7    pos = 901
// HRL = 3     abcdefgh??    ri = 3    wi = 8    pos = 900
// HRL = 4+    abcdefgh??    ri = 3    wi = 8    pos = 900
static inline void  //
wuffs_base__io_buffer__compact_retaining(wuffs_base__io_buffer* buf,
                                         uint64_t history_retain_length) {
  if (!buf || (buf->meta.ri == 0)) {
    return;
  }
  size_t old_ri = buf->meta.ri;
  size_t new_ri = (size_t)(wuffs_base__u64__min(old_ri, history_retain_length));
  size_t memmove_start = old_ri - new_ri;
  buf->meta.pos = wuffs_base__u64__sat_add(buf->meta.pos, memmove_start);
  size_t new_wi = buf->meta.wi - memmove_start;
  if ((new_wi != 0) && (memmove_start != 0)) {
    memmove(buf->data.ptr, buf->data.ptr + memmove_start, new_wi);
  }
  buf->meta.wi = new_wi;
  buf->meta.ri = new_ri;
}

static inline size_t  //
wuffs_base__io_buffer__reader_length(const wuffs_base__io_buffer* buf) {
  return buf ? buf->meta.wi - buf->meta.ri : 0;
}

static inline uint8_t*  //
wuffs_base__io_buffer__reader_pointer(const wuffs_base__io_buffer* buf) {
  return (buf && buf->data.ptr) ? (buf->data.ptr + buf->meta.ri) : NULL;
}

static inline uint64_t  //
wuffs_base__io_buffer__reader_position(const wuffs_base__io_buffer* buf) {
  return buf ? wuffs_base__u64__sat_add(buf->meta.pos, buf->meta.ri) : 0;
}

static inline wuffs_base__slice_u8  //
wuffs_base__io_buffer__reader_slice(const wuffs_base__io_buffer* buf) {
  return (buf && buf->data.ptr)
             ? wuffs_base__make_slice_u8(buf->data.ptr + buf->meta.ri,
                                         buf->meta.wi - buf->meta.ri)
             : wuffs_base__empty_slice_u8();
}

static inline size_t  //
wuffs_base__io_buffer__writer_length(const wuffs_base__io_buffer* buf) {
  return buf ? buf->data.len - buf->meta.wi : 0;
}

static inline uint8_t*  //
wuffs_base__io_buffer__writer_pointer(const wuffs_base__io_buffer* buf) {
  return (buf && buf->data.ptr) ? (buf->data.ptr + buf->meta.wi) : NULL;
}

static inline uint64_t  //
wuffs_base__io_buffer__writer_position(const wuffs_base__io_buffer* buf) {
  return buf ? wuffs_base__u64__sat_add(buf->meta.pos, buf->meta.wi) : 0;
}

static inline wuffs_base__slice_u8  //
wuffs_base__io_buffer__writer_slice(const wuffs_base__io_buffer* buf) {
  return (buf && buf->data.ptr)
             ? wuffs_base__make_slice_u8(buf->data.ptr + buf->meta.wi,
                                         buf->data.len - buf->meta.wi)
             : wuffs_base__empty_slice_u8();
}

#ifdef __cplusplus

inline bool  //
wuffs_base__io_buffer::is_valid() const {
  return wuffs_base__io_buffer__is_valid(this);
}

inline void  //
wuffs_base__io_buffer::compact() {
  wuffs_base__io_buffer__compact(this);
}

inline void  //
wuffs_base__io_buffer::compact_retaining(uint64_t history_retain_length) {
  wuffs_base__io_buffer__compact_retaining(this, history_retain_length);
}

inline size_t  //
wuffs_base__io_buffer::reader_length() const {
  return wuffs_base__io_buffer__reader_length(this);
}

inline uint8_t*  //
wuffs_base__io_buffer::reader_pointer() const {
  return wuffs_base__io_buffer__reader_pointer(this);
}

inline uint64_t  //
wuffs_base__io_buffer::reader_position() const {
  return wuffs_base__io_buffer__reader_position(this);
}

inline wuffs_base__slice_u8  //
wuffs_base__io_buffer::reader_slice() const {
  return wuffs_base__io_buffer__reader_slice(this);
}

inline size_t  //
wuffs_base__io_buffer::writer_length() const {
  return wuffs_base__io_buffer__writer_length(this);
}

inline uint8_t*  //
wuffs_base__io_buffer::writer_pointer() const {
  return wuffs_base__io_buffer__writer_pointer(this);
}

inline uint64_t  //
wuffs_base__io_buffer::writer_position() const {
  return wuffs_base__io_buffer__writer_position(this);
}

inline wuffs_base__slice_u8  //
wuffs_base__io_buffer::writer_slice() const {
  return wuffs_base__io_buffer__writer_slice(this);
}

#endif  // __cplusplus
