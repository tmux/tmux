// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Integer

// wuffs_base__parse_number__foo_digits entries are 0x00 for invalid digits,
// and (0x80 | v) for valid digits, where v is the 4 bit value.

static const uint8_t wuffs_base__parse_number__decimal_digits[256] = {
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x00 ..= 0x07.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x08 ..= 0x0F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x10 ..= 0x17.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x18 ..= 0x1F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x20 ..= 0x27.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x28 ..= 0x2F.
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  // 0x30 ..= 0x37. '0'-'7'.
    0x88, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x38 ..= 0x3F. '8'-'9'.

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x40 ..= 0x47.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x48 ..= 0x4F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x50 ..= 0x57.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x58 ..= 0x5F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x60 ..= 0x67.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x68 ..= 0x6F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x70 ..= 0x77.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x78 ..= 0x7F.

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x80 ..= 0x87.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x88 ..= 0x8F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x90 ..= 0x97.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x98 ..= 0x9F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xA0 ..= 0xA7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xA8 ..= 0xAF.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xB0 ..= 0xB7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xB8 ..= 0xBF.

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xC0 ..= 0xC7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xC8 ..= 0xCF.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xD0 ..= 0xD7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xD8 ..= 0xDF.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xE0 ..= 0xE7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xE8 ..= 0xEF.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xF0 ..= 0xF7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xF8 ..= 0xFF.
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
};

static const uint8_t wuffs_base__parse_number__hexadecimal_digits[256] = {
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x00 ..= 0x07.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x08 ..= 0x0F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x10 ..= 0x17.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x18 ..= 0x1F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x20 ..= 0x27.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x28 ..= 0x2F.
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,  // 0x30 ..= 0x37. '0'-'7'.
    0x88, 0x89, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x38 ..= 0x3F. '8'-'9'.

    0x00, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x00,  // 0x40 ..= 0x47. 'A'-'F'.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x48 ..= 0x4F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x50 ..= 0x57.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x58 ..= 0x5F.
    0x00, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x00,  // 0x60 ..= 0x67. 'a'-'f'.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x68 ..= 0x6F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x70 ..= 0x77.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x78 ..= 0x7F.

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x80 ..= 0x87.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x88 ..= 0x8F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x90 ..= 0x97.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x98 ..= 0x9F.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xA0 ..= 0xA7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xA8 ..= 0xAF.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xB0 ..= 0xB7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xB8 ..= 0xBF.

    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xC0 ..= 0xC7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xC8 ..= 0xCF.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xD0 ..= 0xD7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xD8 ..= 0xDF.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xE0 ..= 0xE7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xE8 ..= 0xEF.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xF0 ..= 0xF7.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0xF8 ..= 0xFF.
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
};

static const uint8_t wuffs_private_impl__encode_base16[16] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  // 0x00 ..= 0x07.
    0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,  // 0x08 ..= 0x0F.
};

// --------

WUFFS_BASE__MAYBE_STATIC wuffs_base__result_i64  //
wuffs_base__parse_number_i64(wuffs_base__slice_u8 s, uint32_t options) {
  uint8_t* p = s.ptr;
  uint8_t* q = s.ptr + s.len;

  if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES) {
    for (; (p < q) && (*p == '_'); p++) {
    }
  }

  bool negative = false;
  if (p >= q) {
    goto fail_bad_argument;
  } else if (*p == '-') {
    p++;
    negative = true;
  } else if (*p == '+') {
    p++;
  }

  do {
    wuffs_base__result_u64 r = wuffs_base__parse_number_u64(
        wuffs_base__make_slice_u8(p, (size_t)(q - p)), options);
    if (r.status.repr != NULL) {
      wuffs_base__result_i64 ret;
      ret.status.repr = r.status.repr;
      ret.value = 0;
      return ret;
    } else if (negative) {
      if (r.value < 0x8000000000000000) {
        wuffs_base__result_i64 ret;
        ret.status.repr = NULL;
        ret.value = -(int64_t)(r.value);
        return ret;
      } else if (r.value == 0x8000000000000000) {
        wuffs_base__result_i64 ret;
        ret.status.repr = NULL;
        ret.value = INT64_MIN;
        return ret;
      }
      goto fail_out_of_bounds;
    } else if (r.value > 0x7FFFFFFFFFFFFFFF) {
      goto fail_out_of_bounds;
    } else {
      wuffs_base__result_i64 ret;
      ret.status.repr = NULL;
      ret.value = +(int64_t)(r.value);
      return ret;
    }
  } while (0);

fail_bad_argument:
  do {
    wuffs_base__result_i64 ret;
    ret.status.repr = wuffs_base__error__bad_argument;
    ret.value = 0;
    return ret;
  } while (0);

fail_out_of_bounds:
  do {
    wuffs_base__result_i64 ret;
    ret.status.repr = wuffs_base__error__out_of_bounds;
    ret.value = 0;
    return ret;
  } while (0);
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__result_u64  //
wuffs_base__parse_number_u64(wuffs_base__slice_u8 s, uint32_t options) {
  uint8_t* p = s.ptr;
  uint8_t* q = s.ptr + s.len;

  if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES) {
    for (; (p < q) && (*p == '_'); p++) {
    }
  }

  if (p >= q) {
    goto fail_bad_argument;

  } else if (*p == '0') {
    p++;
    if (p >= q) {
      goto ok_zero;
    }
    if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES) {
      if (*p == '_') {
        p++;
        for (; p < q; p++) {
          if (*p != '_') {
            if (options &
                WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES) {
              goto decimal;
            }
            goto fail_bad_argument;
          }
        }
        goto ok_zero;
      }
    }

    if ((*p == 'x') || (*p == 'X')) {
      p++;
      if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES) {
        for (; (p < q) && (*p == '_'); p++) {
        }
      }
      if (p < q) {
        goto hexadecimal;
      }

    } else if ((*p == 'd') || (*p == 'D')) {
      p++;
      if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES) {
        for (; (p < q) && (*p == '_'); p++) {
        }
      }
      if (p < q) {
        goto decimal;
      }
    }

    if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES) {
      goto decimal;
    }
    goto fail_bad_argument;
  }

decimal:
  do {
    uint64_t v = wuffs_base__parse_number__decimal_digits[*p++];
    if (v == 0) {
      goto fail_bad_argument;
    }
    v &= 0x0F;

    // UINT64_MAX is 18446744073709551615, which is ((10 * max10) + max1).
    const uint64_t max10 = 1844674407370955161u;
    const uint8_t max1 = 5;

    for (; p < q; p++) {
      if ((*p == '_') &&
          (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES)) {
        continue;
      }
      uint8_t digit = wuffs_base__parse_number__decimal_digits[*p];
      if (digit == 0) {
        goto fail_bad_argument;
      }
      digit &= 0x0F;
      if ((v > max10) || ((v == max10) && (digit > max1))) {
        goto fail_out_of_bounds;
      }
      v = (10 * v) + ((uint64_t)(digit));
    }

    wuffs_base__result_u64 ret;
    ret.status.repr = NULL;
    ret.value = v;
    return ret;
  } while (0);

hexadecimal:
  do {
    uint64_t v = wuffs_base__parse_number__hexadecimal_digits[*p++];
    if (v == 0) {
      goto fail_bad_argument;
    }
    v &= 0x0F;

    for (; p < q; p++) {
      if ((*p == '_') &&
          (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES)) {
        continue;
      }
      uint8_t digit = wuffs_base__parse_number__hexadecimal_digits[*p];
      if (digit == 0) {
        goto fail_bad_argument;
      }
      digit &= 0x0F;
      if ((v >> 60) != 0) {
        goto fail_out_of_bounds;
      }
      v = (v << 4) | ((uint64_t)(digit));
    }

    wuffs_base__result_u64 ret;
    ret.status.repr = NULL;
    ret.value = v;
    return ret;
  } while (0);

ok_zero:
  do {
    wuffs_base__result_u64 ret;
    ret.status.repr = NULL;
    ret.value = 0;
    return ret;
  } while (0);

fail_bad_argument:
  do {
    wuffs_base__result_u64 ret;
    ret.status.repr = wuffs_base__error__bad_argument;
    ret.value = 0;
    return ret;
  } while (0);

fail_out_of_bounds:
  do {
    wuffs_base__result_u64 ret;
    ret.status.repr = wuffs_base__error__out_of_bounds;
    ret.value = 0;
    return ret;
  } while (0);
}

// --------

// wuffs_base__render_number__first_hundred contains the decimal encodings of
// the first one hundred numbers [0 ..= 99].
static const uint8_t wuffs_base__render_number__first_hundred[200] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4',  //
    '0', '5', '0', '6', '0', '7', '0', '8', '0', '9',  //
    '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',  //
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9',  //
    '2', '0', '2', '1', '2', '2', '2', '3', '2', '4',  //
    '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',  //
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4',  //
    '3', '5', '3', '6', '3', '7', '3', '8', '3', '9',  //
    '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',  //
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9',  //
    '5', '0', '5', '1', '5', '2', '5', '3', '5', '4',  //
    '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',  //
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4',  //
    '6', '5', '6', '6', '6', '7', '6', '8', '6', '9',  //
    '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',  //
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9',  //
    '8', '0', '8', '1', '8', '2', '8', '3', '8', '4',  //
    '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',  //
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4',  //
    '9', '5', '9', '6', '9', '7', '9', '8', '9', '9',  //
};

static size_t  //
wuffs_private_impl__render_number_u64(wuffs_base__slice_u8 dst,
                                      uint64_t x,
                                      uint32_t options,
                                      bool neg) {
  uint8_t buf[WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL];
  uint8_t* ptr = &buf[0] + sizeof(buf);

  while (x >= 100) {
    size_t index = ((size_t)((x % 100) * 2));
    x /= 100;
    uint8_t s0 = wuffs_base__render_number__first_hundred[index + 0];
    uint8_t s1 = wuffs_base__render_number__first_hundred[index + 1];
    ptr -= 2;
    ptr[0] = s0;
    ptr[1] = s1;
  }

  if (x < 10) {
    ptr -= 1;
    ptr[0] = (uint8_t)('0' + x);
  } else {
    size_t index = ((size_t)(x * 2));
    uint8_t s0 = wuffs_base__render_number__first_hundred[index + 0];
    uint8_t s1 = wuffs_base__render_number__first_hundred[index + 1];
    ptr -= 2;
    ptr[0] = s0;
    ptr[1] = s1;
  }

  if (neg) {
    ptr -= 1;
    ptr[0] = '-';
  } else if (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN) {
    ptr -= 1;
    ptr[0] = '+';
  }

  size_t n = sizeof(buf) - ((size_t)(ptr - &buf[0]));
  if (n > dst.len) {
    return 0;
  }
  memcpy(dst.ptr + ((options & WUFFS_BASE__RENDER_NUMBER_XXX__ALIGN_RIGHT)
                        ? (dst.len - n)
                        : 0),
         ptr, n);
  return n;
}

WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__render_number_i64(wuffs_base__slice_u8 dst,
                              int64_t x,
                              uint32_t options) {
  uint64_t u = (uint64_t)x;
  bool neg = x < 0;
  if (neg) {
    u = 1 + ~u;
  }
  return wuffs_private_impl__render_number_u64(dst, u, options, neg);
}

WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__render_number_u64(wuffs_base__slice_u8 dst,
                              uint64_t x,
                              uint32_t options) {
  return wuffs_private_impl__render_number_u64(dst, x, options, false);
}

// ---------------- Base-16

WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_16__decode2(wuffs_base__slice_u8 dst,
                             wuffs_base__slice_u8 src,
                             bool src_closed,
                             uint32_t options) {
  wuffs_base__transform__output o;
  size_t src_len2 = src.len / 2;
  size_t len;
  if (dst.len < src_len2) {
    len = dst.len;
    o.status.repr = wuffs_base__suspension__short_write;
  } else {
    len = src_len2;
    if (!src_closed) {
      o.status.repr = wuffs_base__suspension__short_read;
    } else if (src.len & 1) {
      o.status.repr = wuffs_base__error__bad_data;
    } else {
      o.status.repr = NULL;
    }
  }

  uint8_t* d = dst.ptr;
  uint8_t* s = src.ptr;
  size_t n = len;

  while (n--) {
    *d = (uint8_t)((wuffs_base__parse_number__hexadecimal_digits[s[0]] << 4) |
                   (wuffs_base__parse_number__hexadecimal_digits[s[1]] & 0x0F));
    d += 1;
    s += 2;
  }

  o.num_dst = len;
  o.num_src = len * 2;
  return o;
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_16__decode4(wuffs_base__slice_u8 dst,
                             wuffs_base__slice_u8 src,
                             bool src_closed,
                             uint32_t options) {
  wuffs_base__transform__output o;
  size_t src_len4 = src.len / 4;
  size_t len = dst.len < src_len4 ? dst.len : src_len4;
  if (dst.len < src_len4) {
    len = dst.len;
    o.status.repr = wuffs_base__suspension__short_write;
  } else {
    len = src_len4;
    if (!src_closed) {
      o.status.repr = wuffs_base__suspension__short_read;
    } else if (src.len & 1) {
      o.status.repr = wuffs_base__error__bad_data;
    } else {
      o.status.repr = NULL;
    }
  }

  uint8_t* d = dst.ptr;
  uint8_t* s = src.ptr;
  size_t n = len;

  while (n--) {
    *d = (uint8_t)((wuffs_base__parse_number__hexadecimal_digits[s[2]] << 4) |
                   (wuffs_base__parse_number__hexadecimal_digits[s[3]] & 0x0F));
    d += 1;
    s += 4;
  }

  o.num_dst = len;
  o.num_src = len * 4;
  return o;
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_16__encode2(wuffs_base__slice_u8 dst,
                             wuffs_base__slice_u8 src,
                             bool src_closed,
                             uint32_t options) {
  wuffs_base__transform__output o;
  size_t dst_len2 = dst.len / 2;
  size_t len;
  if (dst_len2 < src.len) {
    len = dst_len2;
    o.status.repr = wuffs_base__suspension__short_write;
  } else {
    len = src.len;
    if (!src_closed) {
      o.status.repr = wuffs_base__suspension__short_read;
    } else {
      o.status.repr = NULL;
    }
  }

  uint8_t* d = dst.ptr;
  uint8_t* s = src.ptr;
  size_t n = len;

  while (n--) {
    uint8_t c = *s;
    d[0] = wuffs_private_impl__encode_base16[c >> 4];
    d[1] = wuffs_private_impl__encode_base16[c & 0x0F];
    d += 2;
    s += 1;
  }

  o.num_dst = len * 2;
  o.num_src = len;
  return o;
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_16__encode4(wuffs_base__slice_u8 dst,
                             wuffs_base__slice_u8 src,
                             bool src_closed,
                             uint32_t options) {
  wuffs_base__transform__output o;
  size_t dst_len4 = dst.len / 4;
  size_t len;
  if (dst_len4 < src.len) {
    len = dst_len4;
    o.status.repr = wuffs_base__suspension__short_write;
  } else {
    len = src.len;
    if (!src_closed) {
      o.status.repr = wuffs_base__suspension__short_read;
    } else {
      o.status.repr = NULL;
    }
  }

  uint8_t* d = dst.ptr;
  uint8_t* s = src.ptr;
  size_t n = len;

  while (n--) {
    uint8_t c = *s;
    d[0] = '\\';
    d[1] = 'x';
    d[2] = wuffs_private_impl__encode_base16[c >> 4];
    d[3] = wuffs_private_impl__encode_base16[c & 0x0F];
    d += 4;
    s += 1;
  }

  o.num_dst = len * 4;
  o.num_src = len;
  return o;
}

// ---------------- Base-64

// The two base-64 alphabets, std and url, differ only in the last two codes.
//  - std: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
//  - url: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"

static const uint8_t wuffs_base__base_64__decode_std[256] = {
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x00 ..= 0x07.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x08 ..= 0x0F.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x10 ..= 0x17.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x18 ..= 0x1F.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x20 ..= 0x27.
    0x80, 0x80, 0x80, 0x3E, 0x80, 0x80, 0x80, 0x3F,  // 0x28 ..= 0x2F.
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,  // 0x30 ..= 0x37.
    0x3C, 0x3D, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x38 ..= 0x3F.

    0x80, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // 0x40 ..= 0x47.
    0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,  // 0x48 ..= 0x4F.
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,  // 0x50 ..= 0x57.
    0x17, 0x18, 0x19, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x58 ..= 0x5F.
    0x80, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,  // 0x60 ..= 0x67.
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,  // 0x68 ..= 0x6F.
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,  // 0x70 ..= 0x77.
    0x31, 0x32, 0x33, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x78 ..= 0x7F.

    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x80 ..= 0x87.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x88 ..= 0x8F.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x90 ..= 0x97.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x98 ..= 0x9F.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xA0 ..= 0xA7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xA8 ..= 0xAF.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xB0 ..= 0xB7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xB8 ..= 0xBF.

    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xC0 ..= 0xC7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xC8 ..= 0xCF.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xD0 ..= 0xD7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xD8 ..= 0xDF.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xE0 ..= 0xE7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xE8 ..= 0xEF.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xF0 ..= 0xF7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xF8 ..= 0xFF.
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
};

static const uint8_t wuffs_base__base_64__decode_url[256] = {
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x00 ..= 0x07.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x08 ..= 0x0F.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x10 ..= 0x17.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x18 ..= 0x1F.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x20 ..= 0x27.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x3E, 0x80, 0x80,  // 0x28 ..= 0x2F.
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,  // 0x30 ..= 0x37.
    0x3C, 0x3D, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x38 ..= 0x3F.

    0x80, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,  // 0x40 ..= 0x47.
    0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,  // 0x48 ..= 0x4F.
    0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,  // 0x50 ..= 0x57.
    0x17, 0x18, 0x19, 0x80, 0x80, 0x80, 0x80, 0x3F,  // 0x58 ..= 0x5F.
    0x80, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,  // 0x60 ..= 0x67.
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,  // 0x68 ..= 0x6F.
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,  // 0x70 ..= 0x77.
    0x31, 0x32, 0x33, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x78 ..= 0x7F.

    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x80 ..= 0x87.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x88 ..= 0x8F.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x90 ..= 0x97.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0x98 ..= 0x9F.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xA0 ..= 0xA7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xA8 ..= 0xAF.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xB0 ..= 0xB7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xB8 ..= 0xBF.

    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xC0 ..= 0xC7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xC8 ..= 0xCF.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xD0 ..= 0xD7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xD8 ..= 0xDF.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xE0 ..= 0xE7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xE8 ..= 0xEF.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xF0 ..= 0xF7.
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  // 0xF8 ..= 0xFF.
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
};

static const uint8_t wuffs_base__base_64__encode_std[64] = {
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,  // 0x00 ..= 0x07.
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,  // 0x08 ..= 0x0F.
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,  // 0x10 ..= 0x17.
    0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,  // 0x18 ..= 0x1F.
    0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,  // 0x20 ..= 0x27.
    0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,  // 0x28 ..= 0x2F.
    0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,  // 0x30 ..= 0x37.
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x2F,  // 0x38 ..= 0x3F.
};

static const uint8_t wuffs_base__base_64__encode_url[64] = {
    // 0     1     2     3     4     5     6     7
    // 8     9     A     B     C     D     E     F
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,  // 0x00 ..= 0x07.
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,  // 0x08 ..= 0x0F.
    0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,  // 0x10 ..= 0x17.
    0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,  // 0x18 ..= 0x1F.
    0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,  // 0x20 ..= 0x27.
    0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,  // 0x28 ..= 0x2F.
    0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,  // 0x30 ..= 0x37.
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2D, 0x5F,  // 0x38 ..= 0x3F.
};

// --------

WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_64__decode(wuffs_base__slice_u8 dst,
                            wuffs_base__slice_u8 src,
                            bool src_closed,
                            uint32_t options) {
  const uint8_t* alphabet = (options & WUFFS_BASE__BASE_64__URL_ALPHABET)
                                ? wuffs_base__base_64__decode_url
                                : wuffs_base__base_64__decode_std;
  wuffs_base__transform__output o;
  uint8_t* d_ptr = dst.ptr;
  size_t d_len = dst.len;
  const uint8_t* s_ptr = src.ptr;
  size_t s_len = src.len;
  bool pad = false;

  while (s_len >= 4) {
    uint32_t s = wuffs_base__peek_u32le__no_bounds_check(s_ptr);
    uint32_t s0 = alphabet[0xFF & (s >> 0)];
    uint32_t s1 = alphabet[0xFF & (s >> 8)];
    uint32_t s2 = alphabet[0xFF & (s >> 16)];
    uint32_t s3 = alphabet[0xFF & (s >> 24)];

    if (((s0 | s1 | s2 | s3) & 0xC0) != 0) {
      if (s_len > 4) {
        o.status.repr = wuffs_base__error__bad_data;
        goto done;
      } else if (!src_closed) {
        o.status.repr = wuffs_base__suspension__short_read;
        goto done;
      } else if ((options & WUFFS_BASE__BASE_64__DECODE_ALLOW_PADDING) &&
                 (s_ptr[3] == '=')) {
        pad = true;
        if (s_ptr[2] == '=') {
          goto src2;
        }
        goto src3;
      }
      o.status.repr = wuffs_base__error__bad_data;
      goto done;
    }

    if (d_len < 3) {
      o.status.repr = wuffs_base__suspension__short_write;
      goto done;
    }

    s_ptr += 4;
    s_len -= 4;
    s = (s0 << 18) | (s1 << 12) | (s2 << 6) | (s3 << 0);
    *d_ptr++ = (uint8_t)(s >> 16);
    *d_ptr++ = (uint8_t)(s >> 8);
    *d_ptr++ = (uint8_t)(s >> 0);
    d_len -= 3;
  }

  if (!src_closed) {
    o.status.repr = wuffs_base__suspension__short_read;
    goto done;
  }

  if (s_len == 0) {
    o.status.repr = NULL;
    goto done;
  } else if (s_len == 1) {
    o.status.repr = wuffs_base__error__bad_data;
    goto done;
  } else if (s_len == 2) {
    goto src2;
  }

src3:
  do {
    uint32_t s = wuffs_base__peek_u24le__no_bounds_check(s_ptr);
    uint32_t s0 = alphabet[0xFF & (s >> 0)];
    uint32_t s1 = alphabet[0xFF & (s >> 8)];
    uint32_t s2 = alphabet[0xFF & (s >> 16)];
    if ((s0 & 0xC0) || (s1 & 0xC0) || (s2 & 0xC3)) {
      o.status.repr = wuffs_base__error__bad_data;
      goto done;
    }
    if (d_len < 2) {
      o.status.repr = wuffs_base__suspension__short_write;
      goto done;
    }
    s_ptr += pad ? 4 : 3;
    s = (s0 << 18) | (s1 << 12) | (s2 << 6);
    *d_ptr++ = (uint8_t)(s >> 16);
    *d_ptr++ = (uint8_t)(s >> 8);
    o.status.repr = NULL;
    goto done;
  } while (0);

src2:
  do {
    uint32_t s = wuffs_base__peek_u16le__no_bounds_check(s_ptr);
    uint32_t s0 = alphabet[0xFF & (s >> 0)];
    uint32_t s1 = alphabet[0xFF & (s >> 8)];
    if ((s0 & 0xC0) || (s1 & 0xCF)) {
      o.status.repr = wuffs_base__error__bad_data;
      goto done;
    }
    if (d_len < 1) {
      o.status.repr = wuffs_base__suspension__short_write;
      goto done;
    }
    s_ptr += pad ? 4 : 2;
    s = (s0 << 18) | (s1 << 12);
    *d_ptr++ = (uint8_t)(s >> 16);
    o.status.repr = NULL;
    goto done;
  } while (0);

done:
  o.num_dst = (size_t)(d_ptr - dst.ptr);
  o.num_src = (size_t)(s_ptr - src.ptr);
  return o;
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_64__encode(wuffs_base__slice_u8 dst,
                            wuffs_base__slice_u8 src,
                            bool src_closed,
                            uint32_t options) {
  const uint8_t* alphabet = (options & WUFFS_BASE__BASE_64__URL_ALPHABET)
                                ? wuffs_base__base_64__encode_url
                                : wuffs_base__base_64__encode_std;
  wuffs_base__transform__output o;
  uint8_t* d_ptr = dst.ptr;
  size_t d_len = dst.len;
  const uint8_t* s_ptr = src.ptr;
  size_t s_len = src.len;

  do {
    while (s_len >= 3) {
      if (d_len < 4) {
        o.status.repr = wuffs_base__suspension__short_write;
        goto done;
      }
      uint32_t s = wuffs_base__peek_u24be__no_bounds_check(s_ptr);
      s_ptr += 3;
      s_len -= 3;
      *d_ptr++ = alphabet[0x3F & (s >> 18)];
      *d_ptr++ = alphabet[0x3F & (s >> 12)];
      *d_ptr++ = alphabet[0x3F & (s >> 6)];
      *d_ptr++ = alphabet[0x3F & (s >> 0)];
      d_len -= 4;
    }

    if (!src_closed) {
      o.status.repr = wuffs_base__suspension__short_read;
      goto done;
    }

    if (s_len == 2) {
      if (d_len <
          ((options & WUFFS_BASE__BASE_64__ENCODE_EMIT_PADDING) ? 4 : 3)) {
        o.status.repr = wuffs_base__suspension__short_write;
        goto done;
      }
      uint32_t s = ((uint32_t)(wuffs_base__peek_u16be__no_bounds_check(s_ptr)))
                   << 8;
      s_ptr += 2;
      *d_ptr++ = alphabet[0x3F & (s >> 18)];
      *d_ptr++ = alphabet[0x3F & (s >> 12)];
      *d_ptr++ = alphabet[0x3F & (s >> 6)];
      if (options & WUFFS_BASE__BASE_64__ENCODE_EMIT_PADDING) {
        *d_ptr++ = '=';
      }
      o.status.repr = NULL;
      goto done;

    } else if (s_len == 1) {
      if (d_len <
          ((options & WUFFS_BASE__BASE_64__ENCODE_EMIT_PADDING) ? 4 : 2)) {
        o.status.repr = wuffs_base__suspension__short_write;
        goto done;
      }
      uint32_t s = ((uint32_t)(wuffs_base__peek_u8__no_bounds_check(s_ptr)))
                   << 16;
      s_ptr += 1;
      *d_ptr++ = alphabet[0x3F & (s >> 18)];
      *d_ptr++ = alphabet[0x3F & (s >> 12)];
      if (options & WUFFS_BASE__BASE_64__ENCODE_EMIT_PADDING) {
        *d_ptr++ = '=';
        *d_ptr++ = '=';
      }
      o.status.repr = NULL;
      goto done;

    } else {
      o.status.repr = NULL;
      goto done;
    }
  } while (0);

done:
  o.num_dst = (size_t)(d_ptr - dst.ptr);
  o.num_src = (size_t)(s_ptr - src.ptr);
  return o;
}
