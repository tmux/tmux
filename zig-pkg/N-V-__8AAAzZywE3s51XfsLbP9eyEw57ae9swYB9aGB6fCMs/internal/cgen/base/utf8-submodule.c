// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Unicode and UTF-8

WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__utf_8__encode(wuffs_base__slice_u8 dst, uint32_t code_point) {
  if (code_point <= 0x7F) {
    if (dst.len >= 1) {
      dst.ptr[0] = (uint8_t)(code_point);
      return 1;
    }

  } else if (code_point <= 0x07FF) {
    if (dst.len >= 2) {
      dst.ptr[0] = (uint8_t)(0xC0 | ((code_point >> 6)));
      dst.ptr[1] = (uint8_t)(0x80 | ((code_point >> 0) & 0x3F));
      return 2;
    }

  } else if (code_point <= 0xFFFF) {
    if ((dst.len >= 3) && ((code_point < 0xD800) || (0xDFFF < code_point))) {
      dst.ptr[0] = (uint8_t)(0xE0 | ((code_point >> 12)));
      dst.ptr[1] = (uint8_t)(0x80 | ((code_point >> 6) & 0x3F));
      dst.ptr[2] = (uint8_t)(0x80 | ((code_point >> 0) & 0x3F));
      return 3;
    }

  } else if (code_point <= 0x10FFFF) {
    if (dst.len >= 4) {
      dst.ptr[0] = (uint8_t)(0xF0 | ((code_point >> 18)));
      dst.ptr[1] = (uint8_t)(0x80 | ((code_point >> 12) & 0x3F));
      dst.ptr[2] = (uint8_t)(0x80 | ((code_point >> 6) & 0x3F));
      dst.ptr[3] = (uint8_t)(0x80 | ((code_point >> 0) & 0x3F));
      return 4;
    }
  }

  return 0;
}

// wuffs_base__utf_8__byte_length_minus_1 is the byte length (minus 1) of a
// UTF-8 encoded code point, based on the encoding's initial byte.
//  - 0x00 is 1-byte UTF-8 (ASCII).
//  - 0x01 is the start of 2-byte UTF-8.
//  - 0x02 is the start of 3-byte UTF-8.
//  - 0x03 is the start of 4-byte UTF-8.
//  - 0x40 is a UTF-8 tail byte.
//  - 0x80 is invalid UTF-8.
//
// RFC 3629 (UTF-8) gives this grammar for valid UTF-8:
//    UTF8-1      = %x00-7F
//    UTF8-2      = %xC2-DF UTF8-tail
//    UTF8-3      = %xE0 %xA0-BF UTF8-tail / %xE1-EC 2( UTF8-tail ) /
//                  %xED %x80-9F UTF8-tail / %xEE-EF 2( UTF8-tail )
//    UTF8-4      = %xF0 %x90-BF 2( UTF8-tail ) / %xF1-F3 3( UTF8-tail ) /
//                  %xF4 %x80-8F 2( UTF8-tail )
//    UTF8-tail   = %x80-BF
static const uint8_t wuffs_base__utf_8__byte_length_minus_1[256] = {
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x00 ..= 0x07.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x08 ..= 0x0F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x10 ..= 0x17.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x18 ..= 0x1F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x20 ..= 0x27.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x28 ..= 0x2F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x30 ..= 0x37.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x38 ..= 0x3F.

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x40 ..= 0x47.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x48 ..= 0x4F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x50 ..= 0x57.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x58 ..= 0x5F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x60 ..= 0x67.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x68 ..= 0x6F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x70 ..= 0x77.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x78 ..= 0x7F.

    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0x80 ..= 0x87.
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0x88 ..= 0x8F.
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0x90 ..= 0x97.
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0x98 ..= 0x9F.
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0xA0 ..= 0xA7.
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0xA8 ..= 0xAF.
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0xB0 ..= 0xB7.
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  // 0xB8 ..= 0xBF.

    0x80, 0x80, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  // 0xC0 ..= 0xC7.
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  // 0xC8 ..= 0xCF.
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  // 0xD0 ..= 0xD7.
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,  // 0xD8 ..= 0xDF.
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,  // 0xE0 ..= 0xE7.
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,  // 0xE8 ..= 0xEF.
    0x03, 0x03, 0x03, 0x03, 0x03, 0x80, 0x80, 0x80,  // 0xF0 ..= 0xF7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xF8 ..= 0xFF.
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
};

WUFFS_BASE__MAYBE_STATIC wuffs_base__utf_8__next__output  //
wuffs_base__utf_8__next(const uint8_t* s_ptr, size_t s_len) {
  if (s_len == 0) {
    return wuffs_base__make_utf_8__next__output(0, 0);
  }
  uint32_t c = s_ptr[0];
  switch (wuffs_base__utf_8__byte_length_minus_1[c & 0xFF]) {
    case 0:
      return wuffs_base__make_utf_8__next__output(c, 1);

    case 1:
      if (s_len < 2) {
        break;
      }
      c = wuffs_base__peek_u16le__no_bounds_check(s_ptr);
      if ((c & 0xC000) != 0x8000) {
        break;
      }
      c = (0x0007C0 & (c << 6)) | (0x00003F & (c >> 8));
      return wuffs_base__make_utf_8__next__output(c, 2);

    case 2:
      if (s_len < 3) {
        break;
      }
      c = wuffs_base__peek_u24le__no_bounds_check(s_ptr);
      if ((c & 0xC0C000) != 0x808000) {
        break;
      }
      c = (0x00F000 & (c << 12)) | (0x000FC0 & (c >> 2)) |
          (0x00003F & (c >> 16));
      if ((c <= 0x07FF) || ((0xD800 <= c) && (c <= 0xDFFF))) {
        break;
      }
      return wuffs_base__make_utf_8__next__output(c, 3);

    case 3:
      if (s_len < 4) {
        break;
      }
      c = wuffs_base__peek_u32le__no_bounds_check(s_ptr);
      if ((c & 0xC0C0C000) != 0x80808000) {
        break;
      }
      c = (0x1C0000 & (c << 18)) | (0x03F000 & (c << 4)) |
          (0x000FC0 & (c >> 10)) | (0x00003F & (c >> 24));
      if ((c <= 0xFFFF) || (0x110000 <= c)) {
        break;
      }
      return wuffs_base__make_utf_8__next__output(c, 4);
  }

  return wuffs_base__make_utf_8__next__output(
      WUFFS_BASE__UNICODE_REPLACEMENT_CHARACTER, 1);
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__utf_8__next__output  //
wuffs_base__utf_8__next_from_end(const uint8_t* s_ptr, size_t s_len) {
  if (s_len == 0) {
    return wuffs_base__make_utf_8__next__output(0, 0);
  }
  const uint8_t* ptr = &s_ptr[s_len - 1];
  if (*ptr < 0x80) {
    return wuffs_base__make_utf_8__next__output(*ptr, 1);

  } else if (*ptr < 0xC0) {
    const uint8_t* too_far = &s_ptr[(s_len > 4) ? (s_len - 4) : 0];
    uint32_t n = 1;
    while (ptr != too_far) {
      ptr--;
      n++;
      if (*ptr < 0x80) {
        break;
      } else if (*ptr < 0xC0) {
        continue;
      }
      wuffs_base__utf_8__next__output o = wuffs_base__utf_8__next(ptr, n);
      if (o.byte_length != n) {
        break;
      }
      return o;
    }
  }

  return wuffs_base__make_utf_8__next__output(
      WUFFS_BASE__UNICODE_REPLACEMENT_CHARACTER, 1);
}

WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__utf_8__longest_valid_prefix(const uint8_t* s_ptr, size_t s_len) {
  // TODO: possibly optimize the all-ASCII case (4 or 8 bytes at a time).
  //
  // TODO: possibly optimize this by manually inlining the
  // wuffs_base__utf_8__next calls.
  size_t original_len = s_len;
  while (s_len > 0) {
    wuffs_base__utf_8__next__output o = wuffs_base__utf_8__next(s_ptr, s_len);
    if ((o.code_point > 0x7F) && (o.byte_length == 1)) {
      break;
    }
    s_ptr += o.byte_length;
    s_len -= o.byte_length;
  }
  return original_len - s_len;
}

WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__ascii__longest_valid_prefix(const uint8_t* s_ptr, size_t s_len) {
  // TODO: possibly optimize this by checking 4 or 8 bytes at a time.
  const uint8_t* original_ptr = s_ptr;
  const uint8_t* p = s_ptr;
  const uint8_t* q = s_ptr + s_len;
  for (; (p != q) && ((*p & 0x80) == 0); p++) {
  }
  return (size_t)(p - original_ptr);
}
