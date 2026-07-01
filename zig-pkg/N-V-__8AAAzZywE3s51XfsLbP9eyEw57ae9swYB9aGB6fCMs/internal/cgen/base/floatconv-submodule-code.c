// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- IEEE 754 Floating Point

WUFFS_BASE__MAYBE_STATIC wuffs_base__lossy_value_u16  //
wuffs_base__ieee_754_bit_representation__from_f64_to_u16_truncate(double f) {
  uint64_t u = 0;
  if (sizeof(uint64_t) == sizeof(double)) {
    memcpy(&u, &f, sizeof(uint64_t));
  }
  uint16_t neg = ((uint16_t)((u >> 63) << 15));
  u &= 0x7FFFFFFFFFFFFFFF;
  uint64_t exp = u >> 52;
  uint64_t man = u & 0x000FFFFFFFFFFFFF;

  if (exp == 0x7FF) {
    if (man == 0) {  // Infinity.
      wuffs_base__lossy_value_u16 ret;
      ret.value = neg | 0x7C00;
      ret.lossy = false;
      return ret;
    }
    // NaN. Shift the 52 mantissa bits to 10 mantissa bits, keeping the most
    // significant mantissa bit (quiet vs signaling NaNs). Also set the low 9
    // bits of ret.value so that the 10-bit mantissa is non-zero.
    wuffs_base__lossy_value_u16 ret;
    ret.value = neg | 0x7DFF | ((uint16_t)(man >> 42));
    ret.lossy = false;
    return ret;

  } else if (exp > 0x40E) {  // Truncate to the largest finite f16.
    wuffs_base__lossy_value_u16 ret;
    ret.value = neg | 0x7BFF;
    ret.lossy = true;
    return ret;

  } else if (exp <= 0x3E6) {  // Truncate to zero.
    wuffs_base__lossy_value_u16 ret;
    ret.value = neg;
    ret.lossy = (u != 0);
    return ret;

  } else if (exp <= 0x3F0) {  // Normal f64, subnormal f16.
    // Convert from a 53-bit mantissa (after realizing the implicit bit) to a
    // 10-bit mantissa and then adjust for the exponent.
    man |= 0x0010000000000000;
    uint32_t shift = ((uint32_t)(1051 - exp));  // 1051 = 0x3F0 + 53 - 10.
    uint64_t shifted_man = man >> shift;
    wuffs_base__lossy_value_u16 ret;
    ret.value = neg | ((uint16_t)shifted_man);
    ret.lossy = (shifted_man << shift) != man;
    return ret;
  }

  // Normal f64, normal f16.

  // Re-bias from 1023 to 15 and shift above f16's 10 mantissa bits.
  exp = (exp - 1008) << 10;  // 1008 = 1023 - 15 = 0x3FF - 0xF.

  // Convert from a 52-bit mantissa (excluding the implicit bit) to a 10-bit
  // mantissa (again excluding the implicit bit). We lose some information if
  // any of the bottom 42 bits are non-zero.
  wuffs_base__lossy_value_u16 ret;
  ret.value = neg | ((uint16_t)exp) | ((uint16_t)(man >> 42));
  ret.lossy = (man << 22) != 0;
  return ret;
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__lossy_value_u32  //
wuffs_base__ieee_754_bit_representation__from_f64_to_u32_truncate(double f) {
  uint64_t u = 0;
  if (sizeof(uint64_t) == sizeof(double)) {
    memcpy(&u, &f, sizeof(uint64_t));
  }
  uint32_t neg = ((uint32_t)(u >> 63)) << 31;
  u &= 0x7FFFFFFFFFFFFFFF;
  uint64_t exp = u >> 52;
  uint64_t man = u & 0x000FFFFFFFFFFFFF;

  if (exp == 0x7FF) {
    if (man == 0) {  // Infinity.
      wuffs_base__lossy_value_u32 ret;
      ret.value = neg | 0x7F800000;
      ret.lossy = false;
      return ret;
    }
    // NaN. Shift the 52 mantissa bits to 23 mantissa bits, keeping the most
    // significant mantissa bit (quiet vs signaling NaNs). Also set the low 22
    // bits of ret.value so that the 23-bit mantissa is non-zero.
    wuffs_base__lossy_value_u32 ret;
    ret.value = neg | 0x7FBFFFFF | ((uint32_t)(man >> 29));
    ret.lossy = false;
    return ret;

  } else if (exp > 0x47E) {  // Truncate to the largest finite f32.
    wuffs_base__lossy_value_u32 ret;
    ret.value = neg | 0x7F7FFFFF;
    ret.lossy = true;
    return ret;

  } else if (exp <= 0x369) {  // Truncate to zero.
    wuffs_base__lossy_value_u32 ret;
    ret.value = neg;
    ret.lossy = (u != 0);
    return ret;

  } else if (exp <= 0x380) {  // Normal f64, subnormal f32.
    // Convert from a 53-bit mantissa (after realizing the implicit bit) to a
    // 23-bit mantissa and then adjust for the exponent.
    man |= 0x0010000000000000;
    uint32_t shift = ((uint32_t)(926 - exp));  // 926 = 0x380 + 53 - 23.
    uint64_t shifted_man = man >> shift;
    wuffs_base__lossy_value_u32 ret;
    ret.value = neg | ((uint32_t)shifted_man);
    ret.lossy = (shifted_man << shift) != man;
    return ret;
  }

  // Normal f64, normal f32.

  // Re-bias from 1023 to 127 and shift above f32's 23 mantissa bits.
  exp = (exp - 896) << 23;  // 896 = 1023 - 127 = 0x3FF - 0x7F.

  // Convert from a 52-bit mantissa (excluding the implicit bit) to a 23-bit
  // mantissa (again excluding the implicit bit). We lose some information if
  // any of the bottom 29 bits are non-zero.
  wuffs_base__lossy_value_u32 ret;
  ret.value = neg | ((uint32_t)exp) | ((uint32_t)(man >> 29));
  ret.lossy = (man << 35) != 0;
  return ret;
}

// --------

#define WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE 2047
#define WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION 800

// WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL is the largest N such that
// ((10 << N) < (1 << 64)).
#define WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL 60

// wuffs_private_impl__high_prec_dec (abbreviated as HPD) is a fixed precision
// floating point decimal number, augmented with ±infinity values, but it
// cannot represent NaN (Not a Number).
//
// "High precision" means that the mantissa holds 800 decimal digits. 800 is
// WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION.
//
// An HPD isn't for general purpose arithmetic, only for conversions to and
// from IEEE 754 double-precision floating point, where the largest and
// smallest positive, finite values are approximately 1.8e+308 and 4.9e-324.
// HPD exponents above +2047 mean infinity, below -2047 mean zero. The ±2047
// bounds are further away from zero than ±(324 + 800), where 800 and 2047 is
// WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION and
// WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE.
//
// digits[.. num_digits] are the number's digits in big-endian order. The
// uint8_t values are in the range [0 ..= 9], not ['0' ..= '9'], where e.g. '7'
// is the ASCII value 0x37.
//
// decimal_point is the index (within digits) of the decimal point. It may be
// negative or be larger than num_digits, in which case the explicit digits are
// padded with implicit zeroes.
//
// For example, if num_digits is 3 and digits is "\x07\x08\x09":
//  - A decimal_point of -2 means ".00789"
//  - A decimal_point of -1 means ".0789"
//  - A decimal_point of +0 means ".789"
//  - A decimal_point of +1 means "7.89"
//  - A decimal_point of +2 means "78.9"
//  - A decimal_point of +3 means "789."
//  - A decimal_point of +4 means "7890."
//  - A decimal_point of +5 means "78900."
//
// As above, a decimal_point higher than +2047 means that the overall value is
// infinity, lower than -2047 means zero.
//
// negative is a sign bit. An HPD can distinguish positive and negative zero.
//
// truncated is whether there are more than
// WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION digits, and at least one of those
// extra digits are non-zero. The existence of long-tail digits can affect
// rounding.
//
// The "all fields are zero" value is valid, and represents the number +0.
typedef struct wuffs_private_impl__high_prec_dec__struct {
  uint32_t num_digits;
  int32_t decimal_point;
  bool negative;
  bool truncated;
  uint8_t digits[WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION];
} wuffs_private_impl__high_prec_dec;

// wuffs_private_impl__high_prec_dec__trim trims trailing zeroes from the
// h->digits[.. h->num_digits] slice. They have no benefit, since we explicitly
// track h->decimal_point.
//
// Preconditions:
//  - h is non-NULL.
static inline void  //
wuffs_private_impl__high_prec_dec__trim(wuffs_private_impl__high_prec_dec* h) {
  while ((h->num_digits > 0) && (h->digits[h->num_digits - 1] == 0)) {
    h->num_digits--;
  }
}

// wuffs_private_impl__high_prec_dec__assign sets h to represent the number x.
//
// Preconditions:
//  - h is non-NULL.
static void  //
wuffs_private_impl__high_prec_dec__assign(wuffs_private_impl__high_prec_dec* h,
                                          uint64_t x,
                                          bool negative) {
  uint32_t n = 0;

  // Set h->digits.
  if (x > 0) {
    // Calculate the digits, working right-to-left. After we determine n (how
    // many digits there are), copy from buf to h->digits.
    //
    // UINT64_MAX, 18446744073709551615, is 20 digits long. It can be faster to
    // copy a constant number of bytes than a variable number (20 instead of
    // n). Make buf large enough (and start writing to it from the middle) so
    // that can we always copy 20 bytes: the slice buf[(20-n) .. (40-n)].
    uint8_t buf[40] = {0};
    uint8_t* ptr = &buf[20];
    do {
      uint64_t remaining = x / 10;
      x -= remaining * 10;
      ptr--;
      *ptr = (uint8_t)x;
      n++;
      x = remaining;
    } while (x > 0);
    memcpy(h->digits, ptr, 20);
  }

  // Set h's other fields.
  h->num_digits = n;
  h->decimal_point = (int32_t)n;
  h->negative = negative;
  h->truncated = false;
  wuffs_private_impl__high_prec_dec__trim(h);
}

static wuffs_base__status  //
wuffs_private_impl__high_prec_dec__parse(wuffs_private_impl__high_prec_dec* h,
                                         wuffs_base__slice_u8 s,
                                         uint32_t options) {
  if (!h) {
    return wuffs_base__make_status(wuffs_base__error__bad_receiver);
  }
  h->num_digits = 0;
  h->decimal_point = 0;
  h->negative = false;
  h->truncated = false;

  uint8_t* p = s.ptr;
  uint8_t* q = s.ptr + s.len;

  if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES) {
    for (;; p++) {
      if (p >= q) {
        return wuffs_base__make_status(wuffs_base__error__bad_argument);
      } else if (*p != '_') {
        break;
      }
    }
  }

  // Parse sign.
  do {
    if (*p == '+') {
      p++;
    } else if (*p == '-') {
      h->negative = true;
      p++;
    } else {
      break;
    }
    if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES) {
      for (;; p++) {
        if (p >= q) {
          return wuffs_base__make_status(wuffs_base__error__bad_argument);
        } else if (*p != '_') {
          break;
        }
      }
    }
  } while (0);

  // Parse digits, up to (and including) a '.', 'E' or 'e'. Examples for each
  // limb in this if-else chain:
  //  - "0.789"
  //  - "1002.789"
  //  - ".789"
  //  - Other (invalid input).
  uint32_t nd = 0;
  int32_t dp = 0;
  bool no_digits_before_separator = false;
  if (('0' == *p) &&
      !(options &
        WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES)) {
    p++;
    for (;; p++) {
      if (p >= q) {
        goto after_all;
      } else if (*p ==
                 ((options &
                   WUFFS_BASE__PARSE_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA)
                      ? ','
                      : '.')) {
        p++;
        goto after_sep;
      } else if ((*p == 'E') || (*p == 'e')) {
        p++;
        goto after_exp;
      } else if ((*p != '_') ||
                 !(options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES)) {
        return wuffs_base__make_status(wuffs_base__error__bad_argument);
      }
    }

  } else if (('0' <= *p) && (*p <= '9')) {
    if (*p == '0') {
      for (; (p < q) && (*p == '0'); p++) {
      }
    } else {
      h->digits[nd++] = (uint8_t)(*p - '0');
      dp = (int32_t)nd;
      p++;
    }

    for (;; p++) {
      if (p >= q) {
        goto after_all;
      } else if (('0' <= *p) && (*p <= '9')) {
        if (nd < WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION) {
          h->digits[nd++] = (uint8_t)(*p - '0');
          dp = (int32_t)nd;
        } else if ('0' != *p) {
          // Long-tail non-zeroes set the truncated bit.
          h->truncated = true;
        }
      } else if (*p ==
                 ((options &
                   WUFFS_BASE__PARSE_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA)
                      ? ','
                      : '.')) {
        p++;
        goto after_sep;
      } else if ((*p == 'E') || (*p == 'e')) {
        p++;
        goto after_exp;
      } else if ((*p != '_') ||
                 !(options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES)) {
        return wuffs_base__make_status(wuffs_base__error__bad_argument);
      }
    }

  } else if (*p == ((options &
                     WUFFS_BASE__PARSE_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA)
                        ? ','
                        : '.')) {
    p++;
    no_digits_before_separator = true;

  } else {
    return wuffs_base__make_status(wuffs_base__error__bad_argument);
  }

after_sep:
  for (;; p++) {
    if (p >= q) {
      goto after_all;
    } else if ('0' == *p) {
      if (nd == 0) {
        // Track leading zeroes implicitly.
        dp--;
      } else if (nd < WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION) {
        h->digits[nd++] = (uint8_t)(*p - '0');
      }
    } else if (('0' < *p) && (*p <= '9')) {
      if (nd < WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION) {
        h->digits[nd++] = (uint8_t)(*p - '0');
      } else {
        // Long-tail non-zeroes set the truncated bit.
        h->truncated = true;
      }
    } else if ((*p == 'E') || (*p == 'e')) {
      p++;
      goto after_exp;
    } else if ((*p != '_') ||
               !(options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES)) {
      return wuffs_base__make_status(wuffs_base__error__bad_argument);
    }
  }

after_exp:
  do {
    if (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES) {
      for (;; p++) {
        if (p >= q) {
          return wuffs_base__make_status(wuffs_base__error__bad_argument);
        } else if (*p != '_') {
          break;
        }
      }
    }

    int32_t exp_sign = +1;
    if (*p == '+') {
      p++;
    } else if (*p == '-') {
      exp_sign = -1;
      p++;
    }

    int32_t exp = 0;
    const int32_t exp_large = WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE +
                              WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION;
    bool saw_exp_digits = false;
    for (; p < q; p++) {
      if ((*p == '_') &&
          (options & WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES)) {
        // No-op.
      } else if (('0' <= *p) && (*p <= '9')) {
        saw_exp_digits = true;
        if (exp < exp_large) {
          exp = (10 * exp) + ((int32_t)(*p - '0'));
        }
      } else {
        break;
      }
    }
    if (!saw_exp_digits) {
      return wuffs_base__make_status(wuffs_base__error__bad_argument);
    }
    dp += exp_sign * exp;
  } while (0);

after_all:
  if (p != q) {
    return wuffs_base__make_status(wuffs_base__error__bad_argument);
  }
  h->num_digits = nd;
  if (nd == 0) {
    if (no_digits_before_separator) {
      return wuffs_base__make_status(wuffs_base__error__bad_argument);
    }
    h->decimal_point = 0;
  } else if (dp < -WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE) {
    h->decimal_point = -WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE - 1;
  } else if (dp > +WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE) {
    h->decimal_point = +WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE + 1;
  } else {
    h->decimal_point = dp;
  }
  wuffs_private_impl__high_prec_dec__trim(h);
  return wuffs_base__make_status(NULL);
}

// --------

// wuffs_private_impl__high_prec_dec__lshift_num_new_digits returns the number
// of additional decimal digits when left-shifting by shift.
//
// See below for preconditions.
static uint32_t  //
wuffs_private_impl__high_prec_dec__lshift_num_new_digits(
    wuffs_private_impl__high_prec_dec* h,
    uint32_t shift) {
  // Masking with 0x3F should be unnecessary (assuming the preconditions) but
  // it's cheap and ensures that we don't overflow the
  // wuffs_private_impl__hpd_left_shift array.
  shift &= 63;

  uint32_t x_a = wuffs_private_impl__hpd_left_shift[shift];
  uint32_t x_b = wuffs_private_impl__hpd_left_shift[shift + 1];
  uint32_t num_new_digits = x_a >> 11;
  uint32_t pow5_a = 0x7FF & x_a;
  uint32_t pow5_b = 0x7FF & x_b;

  const uint8_t* pow5 = &wuffs_private_impl__powers_of_5[pow5_a];
  uint32_t i = 0;
  uint32_t n = pow5_b - pow5_a;
  for (; i < n; i++) {
    if (i >= h->num_digits) {
      return num_new_digits - 1;
    } else if (h->digits[i] == pow5[i]) {
      continue;
    } else if (h->digits[i] < pow5[i]) {
      return num_new_digits - 1;
    } else {
      return num_new_digits;
    }
  }
  return num_new_digits;
}

// --------

// wuffs_private_impl__high_prec_dec__rounded_integer returns the integral
// (non-fractional) part of h, provided that it is 18 or fewer decimal digits.
// For 19 or more digits, it returns UINT64_MAX. Note that:
//  - (1 << 53) is    9007199254740992, which has 16 decimal digits.
//  - (1 << 56) is   72057594037927936, which has 17 decimal digits.
//  - (1 << 59) is  576460752303423488, which has 18 decimal digits.
//  - (1 << 63) is 9223372036854775808, which has 19 decimal digits.
// and that IEEE 754 double precision has 52 mantissa bits.
//
// That integral part is rounded-to-even: rounding 7.5 or 8.5 both give 8.
//
// h's negative bit is ignored: rounding -8.6 returns 9.
//
// See below for preconditions.
static uint64_t  //
wuffs_private_impl__high_prec_dec__rounded_integer(
    wuffs_private_impl__high_prec_dec* h) {
  if ((h->num_digits == 0) || (h->decimal_point < 0)) {
    return 0;
  } else if (h->decimal_point > 18) {
    return UINT64_MAX;
  }

  uint32_t dp = (uint32_t)(h->decimal_point);
  uint64_t n = 0;
  uint32_t i = 0;
  for (; i < dp; i++) {
    n = (10 * n) + ((i < h->num_digits) ? h->digits[i] : 0);
  }

  bool round_up = false;
  if (dp < h->num_digits) {
    round_up = h->digits[dp] >= 5;
    if ((h->digits[dp] == 5) && (dp + 1 == h->num_digits)) {
      // We are exactly halfway. If we're truncated, round up, otherwise round
      // to even.
      round_up = h->truncated ||  //
                 ((dp > 0) && (1 & h->digits[dp - 1]));
    }
  }
  if (round_up) {
    n++;
  }

  return n;
}

// wuffs_private_impl__high_prec_dec__small_xshift shifts h's number (where 'x'
// is 'l' or 'r' for left or right) by a small shift value.
//
// Preconditions:
//  - h is non-NULL.
//  - h->decimal_point is "not extreme".
//  - shift is non-zero.
//  - shift is "a small shift".
//
// "Not extreme" means within ±WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE.
//
// "A small shift" means not more than
// WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL.
//
// wuffs_private_impl__high_prec_dec__rounded_integer and
// wuffs_private_impl__high_prec_dec__lshift_num_new_digits have the same
// preconditions.
//
// wuffs_private_impl__high_prec_dec__lshift keeps the first two preconditions
// but not the last two. Its shift argument is signed and does not need to be
// "small": zero is a no-op, positive means left shift and negative means right
// shift.

static void  //
wuffs_private_impl__high_prec_dec__small_lshift(
    wuffs_private_impl__high_prec_dec* h,
    uint32_t shift) {
  if (h->num_digits == 0) {
    return;
  }
  uint32_t num_new_digits =
      wuffs_private_impl__high_prec_dec__lshift_num_new_digits(h, shift);
  uint32_t rx = h->num_digits - 1;                   // Read  index.
  uint32_t wx = h->num_digits - 1 + num_new_digits;  // Write index.
  uint64_t n = 0;

  // Repeat: pick up a digit, put down a digit, right to left.
  while (((int32_t)rx) >= 0) {
    n += ((uint64_t)(h->digits[rx])) << shift;
    uint64_t quo = n / 10;
    uint64_t rem = n - (10 * quo);
    if (wx < WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION) {
      h->digits[wx] = (uint8_t)rem;
    } else if (rem > 0) {
      h->truncated = true;
    }
    n = quo;
    wx--;
    rx--;
  }

  // Put down leading digits, right to left.
  while (n > 0) {
    uint64_t quo = n / 10;
    uint64_t rem = n - (10 * quo);
    if (wx < WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION) {
      h->digits[wx] = (uint8_t)rem;
    } else if (rem > 0) {
      h->truncated = true;
    }
    n = quo;
    wx--;
  }

  // Finish.
  h->num_digits += num_new_digits;
  if (h->num_digits > WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION) {
    h->num_digits = WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION;
  }
  h->decimal_point += (int32_t)num_new_digits;
  wuffs_private_impl__high_prec_dec__trim(h);
}

static void  //
wuffs_private_impl__high_prec_dec__small_rshift(
    wuffs_private_impl__high_prec_dec* h,
    uint32_t shift) {
  uint32_t rx = 0;  // Read  index.
  uint32_t wx = 0;  // Write index.
  uint64_t n = 0;

  // Pick up enough leading digits to cover the first shift.
  while ((n >> shift) == 0) {
    if (rx < h->num_digits) {
      // Read a digit.
      n = (10 * n) + h->digits[rx++];
    } else if (n == 0) {
      // h's number used to be zero and remains zero.
      return;
    } else {
      // Read sufficient implicit trailing zeroes.
      while ((n >> shift) == 0) {
        n = 10 * n;
        rx++;
      }
      break;
    }
  }
  h->decimal_point -= ((int32_t)(rx - 1));
  if (h->decimal_point < -WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE) {
    // After the shift, h's number is effectively zero.
    h->num_digits = 0;
    h->decimal_point = 0;
    h->truncated = false;
    return;
  }

  // Repeat: pick up a digit, put down a digit, left to right.
  uint64_t mask = (((uint64_t)(1)) << shift) - 1;
  while (rx < h->num_digits) {
    uint8_t new_digit = ((uint8_t)(n >> shift));
    n = (10 * (n & mask)) + h->digits[rx++];
    h->digits[wx++] = new_digit;
  }

  // Put down trailing digits, left to right.
  while (n > 0) {
    uint8_t new_digit = ((uint8_t)(n >> shift));
    n = 10 * (n & mask);
    if (wx < WUFFS_PRIVATE_IMPL__HPD__DIGITS_PRECISION) {
      h->digits[wx++] = new_digit;
    } else if (new_digit > 0) {
      h->truncated = true;
    }
  }

  // Finish.
  h->num_digits = wx;
  wuffs_private_impl__high_prec_dec__trim(h);
}

static void  //
wuffs_private_impl__high_prec_dec__lshift(wuffs_private_impl__high_prec_dec* h,
                                          int32_t shift) {
  if (shift > 0) {
    while (shift > +WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL) {
      wuffs_private_impl__high_prec_dec__small_lshift(
          h, WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL);
      shift -= WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL;
    }
    wuffs_private_impl__high_prec_dec__small_lshift(h, ((uint32_t)(+shift)));
  } else if (shift < 0) {
    while (shift < -WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL) {
      wuffs_private_impl__high_prec_dec__small_rshift(
          h, WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL);
      shift += WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL;
    }
    wuffs_private_impl__high_prec_dec__small_rshift(h, ((uint32_t)(-shift)));
  }
}

// --------

// wuffs_private_impl__high_prec_dec__round_etc rounds h's number. For those
// functions that take an n argument, rounding produces at most n digits (which
// is not necessarily at most n decimal places). Negative n values are ignored,
// as well as any n greater than or equal to h's number of digits. The
// etc__round_just_enough function implicitly chooses an n to implement
// WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION.
//
// Preconditions:
//  - h is non-NULL.
//  - h->decimal_point is "not extreme".
//
// "Not extreme" means within ±WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE.

static void  //
wuffs_private_impl__high_prec_dec__round_down(
    wuffs_private_impl__high_prec_dec* h,
    int32_t n) {
  if ((n < 0) || (h->num_digits <= (uint32_t)n)) {
    return;
  }
  h->num_digits = (uint32_t)(n);
  wuffs_private_impl__high_prec_dec__trim(h);
}

static void  //
wuffs_private_impl__high_prec_dec__round_up(
    wuffs_private_impl__high_prec_dec* h,
    int32_t n) {
  if ((n < 0) || (h->num_digits <= (uint32_t)n)) {
    return;
  }

  for (n--; n >= 0; n--) {
    if (h->digits[n] < 9) {
      h->digits[n]++;
      h->num_digits = (uint32_t)(n + 1);
      return;
    }
  }

  // The number is all 9s. Change to a single 1 and adjust the decimal point.
  h->digits[0] = 1;
  h->num_digits = 1;
  h->decimal_point++;
}

static void  //
wuffs_private_impl__high_prec_dec__round_nearest(
    wuffs_private_impl__high_prec_dec* h,
    int32_t n) {
  if ((n < 0) || (h->num_digits <= (uint32_t)n)) {
    return;
  }
  bool up = h->digits[n] >= 5;
  if ((h->digits[n] == 5) && ((n + 1) == ((int32_t)(h->num_digits)))) {
    up = h->truncated ||  //
         ((n > 0) && ((h->digits[n - 1] & 1) != 0));
  }

  if (up) {
    wuffs_private_impl__high_prec_dec__round_up(h, n);
  } else {
    wuffs_private_impl__high_prec_dec__round_down(h, n);
  }
}

static void  //
wuffs_private_impl__high_prec_dec__round_just_enough(
    wuffs_private_impl__high_prec_dec* h,
    int32_t exp2,
    uint64_t mantissa) {
  // The magic numbers 52 and 53 in this function are because IEEE 754 double
  // precision has 52 mantissa bits.
  //
  // Let f be the floating point number represented by exp2 and mantissa (and
  // also the number in h): the number (mantissa * (2 ** (exp2 - 52))).
  //
  // If f is zero or a small integer, we can return early.
  if ((mantissa == 0) ||
      ((exp2 < 53) && (h->decimal_point >= ((int32_t)(h->num_digits))))) {
    return;
  }

  // The smallest normal f has an exp2 of -1022 and a mantissa of (1 << 52).
  // Subnormal numbers have the same exp2 but a smaller mantissa.
  static const int32_t min_incl_normal_exp2 = -1022;
  static const uint64_t min_incl_normal_mantissa = 0x0010000000000000ul;

  // Compute lower and upper bounds such that any number between them (possibly
  // inclusive) will round to f. First, the lower bound. Our number f is:
  //   ((mantissa + 0)         * (2 ** (  exp2 - 52)))
  //
  // The next lowest floating point number is:
  //   ((mantissa - 1)         * (2 ** (  exp2 - 52)))
  // unless (mantissa - 1) drops the (1 << 52) bit and exp2 is not the
  // min_incl_normal_exp2. Either way, call it:
  //   ((l_mantissa)           * (2 ** (l_exp2 - 52)))
  //
  // The lower bound is halfway between them (noting that 52 became 53):
  //   (((2 * l_mantissa) + 1) * (2 ** (l_exp2 - 53)))
  int32_t l_exp2 = exp2;
  uint64_t l_mantissa = mantissa - 1;
  if ((exp2 > min_incl_normal_exp2) && (mantissa <= min_incl_normal_mantissa)) {
    l_exp2 = exp2 - 1;
    l_mantissa = (2 * mantissa) - 1;
  }
  wuffs_private_impl__high_prec_dec lower;
  wuffs_private_impl__high_prec_dec__assign(&lower, (2 * l_mantissa) + 1,
                                            false);
  wuffs_private_impl__high_prec_dec__lshift(&lower, l_exp2 - 53);

  // Next, the upper bound. Our number f is:
  //   ((mantissa + 0)       * (2 ** (exp2 - 52)))
  //
  // The next highest floating point number is:
  //   ((mantissa + 1)       * (2 ** (exp2 - 52)))
  //
  // The upper bound is halfway between them (noting that 52 became 53):
  //   (((2 * mantissa) + 1) * (2 ** (exp2 - 53)))
  wuffs_private_impl__high_prec_dec upper;
  wuffs_private_impl__high_prec_dec__assign(&upper, (2 * mantissa) + 1, false);
  wuffs_private_impl__high_prec_dec__lshift(&upper, exp2 - 53);

  // The lower and upper bounds are possible outputs only if the original
  // mantissa is even, so that IEEE round-to-even would round to the original
  // mantissa and not its neighbors.
  bool inclusive = (mantissa & 1) == 0;

  // As we walk the digits, we want to know whether rounding up would fall
  // within the upper bound. This is tracked by upper_delta:
  //  - When -1, the digits of h and upper are the same so far.
  //  - When +0, we saw a difference of 1 between h and upper on a previous
  //    digit and subsequently only 9s for h and 0s for upper. Thus, rounding
  //    up may fall outside of the bound if !inclusive.
  //  - When +1, the difference is greater than 1 and we know that rounding up
  //    falls within the bound.
  //
  // This is a state machine with three states. The numerical value for each
  // state (-1, +0 or +1) isn't important, other than their order.
  int upper_delta = -1;

  // We can now figure out the shortest number of digits required. Walk the
  // digits until h has distinguished itself from lower or upper.
  //
  // The zi and zd variables are indexes and digits, for z in l (lower), h (the
  // number) and u (upper).
  //
  // The lower, h and upper numbers may have their decimal points at different
  // places. In this case, upper is the longest, so we iterate ui starting from
  // 0 and iterate li and hi starting from either 0 or -1.
  int32_t ui = 0;
  for (;; ui++) {
    // Calculate hd, the middle number's digit.
    int32_t hi = ui - upper.decimal_point + h->decimal_point;
    if (hi >= ((int32_t)(h->num_digits))) {
      break;
    }
    uint8_t hd = (((uint32_t)hi) < h->num_digits) ? h->digits[hi] : 0;

    // Calculate ld, the lower bound's digit.
    int32_t li = ui - upper.decimal_point + lower.decimal_point;
    uint8_t ld = (((uint32_t)li) < lower.num_digits) ? lower.digits[li] : 0;

    // We can round down (truncate) if lower has a different digit than h or if
    // lower is inclusive and is exactly the result of rounding down (i.e. we
    // have reached the final digit of lower).
    bool can_round_down =
        (ld != hd) ||  //
        (inclusive && ((li + 1) == ((int32_t)(lower.num_digits))));

    // Calculate ud, the upper bound's digit, and update upper_delta.
    uint8_t ud = (((uint32_t)ui) < upper.num_digits) ? upper.digits[ui] : 0;
    if (upper_delta < 0) {
      if ((hd + 1) < ud) {
        // For example:
        // h     = 12345???
        // upper = 12347???
        upper_delta = +1;
      } else if (hd != ud) {
        // For example:
        // h     = 12345???
        // upper = 12346???
        upper_delta = +0;
      }
    } else if (upper_delta == 0) {
      if ((hd != 9) || (ud != 0)) {
        // For example:
        // h     = 1234598?
        // upper = 1234600?
        upper_delta = +1;
      }
    }

    // We can round up if upper has a different digit than h and either upper
    // is inclusive or upper is bigger than the result of rounding up.
    bool can_round_up =
        (upper_delta > 0) ||    //
        ((upper_delta == 0) &&  //
         (inclusive || ((ui + 1) < ((int32_t)(upper.num_digits)))));

    // If we can round either way, round to nearest. If we can round only one
    // way, do it. If we can't round, continue the loop.
    if (can_round_down) {
      if (can_round_up) {
        wuffs_private_impl__high_prec_dec__round_nearest(h, hi + 1);
        return;
      } else {
        wuffs_private_impl__high_prec_dec__round_down(h, hi + 1);
        return;
      }
    } else {
      if (can_round_up) {
        wuffs_private_impl__high_prec_dec__round_up(h, hi + 1);
        return;
      }
    }
  }
}

// --------

// wuffs_private_impl__parse_number_f64_eisel_lemire produces the IEEE 754
// double-precision value for an exact mantissa and base-10 exponent. For
// example:
//  - when parsing "12345.678e+02", man is 12345678 and exp10 is -1.
//  - when parsing "-12", man is 12 and exp10 is 0. Processing the leading
//    minus sign is the responsibility of the caller, not this function.
//
// On success, it returns a non-negative int64_t such that the low 63 bits hold
// the 11-bit exponent and 52-bit mantissa.
//
// On failure, it returns a negative value.
//
// The algorithm is based on an original idea by Michael Eisel that was refined
// by Daniel Lemire. See
// https://lemire.me/blog/2020/03/10/fast-float-parsing-in-practice/
// and
// https://nigeltao.github.io/blog/2020/eisel-lemire.html
//
// Preconditions:
//  - man is non-zero.
//  - exp10 is in the range [-307 ..= 288], the same range of the
//    wuffs_private_impl__powers_of_10 array.
//
// The exp10 range (and the fact that man is in the range [1 ..= UINT64_MAX],
// approximately [1 ..= 1.85e+19]) means that (man * (10 ** exp10)) is in the
// range [1e-307 ..= 1.85e+307]. This is entirely within the range of normal
// (neither subnormal nor non-finite) f64 values: DBL_MIN and DBL_MAX are
// approximately 2.23e–308 and 1.80e+308.
static int64_t  //
wuffs_private_impl__parse_number_f64_eisel_lemire(uint64_t man, int32_t exp10) {
  // Look up the (possibly truncated) base-2 representation of (10 ** exp10).
  // The look-up table was constructed so that it is already normalized: the
  // table entry's mantissa's MSB (most significant bit) is on.
  const uint64_t* po10 = &wuffs_private_impl__powers_of_10[exp10 + 307][0];

  // Normalize the man argument. The (man != 0) precondition means that a
  // non-zero bit exists.
  uint32_t clz = wuffs_base__count_leading_zeroes_u64(man);
  man <<= clz;

  // Calculate the return value's base-2 exponent. We might tweak it by ±1
  // later, but its initial value comes from a linear scaling of exp10,
  // converting from power-of-10 to power-of-2, and adjusting by clz.
  //
  // The magic constants are:
  //  - 1087 = 1023 + 64. The 1023 is the f64 exponent bias. The 64 is because
  //    the look-up table uses 64-bit mantissas.
  //  - 217706 is such that the ratio 217706 / 65536 ≈ 3.321930 is close enough
  //    (over the practical range of exp10) to log(10) / log(2) ≈ 3.321928.
  //  - 65536 = 1<<16 is arbitrary but a power of 2, so division is a shift.
  //
  // Equality of the linearly-scaled value and the actual power-of-2, over the
  // range of exp10 arguments that this function accepts, is confirmed by
  // script/print-mpb-powers-of-10.go
  uint64_t ret_exp2 =
      ((uint64_t)(((217706 * exp10) >> 16) + 1087)) - ((uint64_t)clz);

  // Multiply the two mantissas. Normalization means that both mantissas are at
  // least (1<<63), so the 128-bit product must be at least (1<<126). The high
  // 64 bits of the product, x_hi, must therefore be at least (1<<62).
  //
  // As a consequence, x_hi has either 0 or 1 leading zeroes. Shifting x_hi
  // right by either 9 or 10 bits (depending on x_hi's MSB) will therefore
  // leave the top 10 MSBs (bits 54 ..= 63) off and the 11th MSB (bit 53) on.
  wuffs_base__multiply_u64__output x = wuffs_base__multiply_u64(man, po10[1]);
  uint64_t x_hi = x.hi;
  uint64_t x_lo = x.lo;

  // Before we shift right by at least 9 bits, recall that the look-up table
  // entry was possibly truncated. We have so far only calculated a lower bound
  // for the product (man * e), where e is (10 ** exp10). The upper bound would
  // add a further (man * 1) to the 128-bit product, which overflows the lower
  // 64-bit limb if ((x_lo + man) < man).
  //
  // If overflow occurs, that adds 1 to x_hi. Since we're about to shift right
  // by at least 9 bits, that carried 1 can be ignored unless the higher 64-bit
  // limb's low 9 bits are all on.
  //
  // For example, parsing "9999999999999999999" will take the if-true branch
  // here, since:
  //  - x_hi = 0x4563918244F3FFFF
  //  - x_lo = 0x8000000000000000
  //  - man  = 0x8AC7230489E7FFFF
  if (((x_hi & 0x1FF) == 0x1FF) && ((x_lo + man) < man)) {
    // Refine our calculation of (man * e). Before, our approximation of e used
    // a "low resolution" 64-bit mantissa. Now use a "high resolution" 128-bit
    // mantissa. We've already calculated x = (man * bits_0_to_63_incl_of_e).
    // Now calculate y = (man * bits_64_to_127_incl_of_e).
    wuffs_base__multiply_u64__output y = wuffs_base__multiply_u64(man, po10[0]);
    uint64_t y_hi = y.hi;
    uint64_t y_lo = y.lo;

    // Merge the 128-bit x and 128-bit y, which overlap by 64 bits, to
    // calculate the 192-bit product of the 64-bit man by the 128-bit e.
    // As we exit this if-block, we only care about the high 128 bits
    // (merged_hi and merged_lo) of that 192-bit product.
    //
    // For example, parsing "1.234e-45" will take the if-true branch here,
    // since:
    //  - x_hi = 0x70B7E3696DB29FFF
    //  - x_lo = 0xE040000000000000
    //  - y_hi = 0x33718BBEAB0E0D7A
    //  - y_lo = 0xA880000000000000
    uint64_t merged_hi = x_hi;
    uint64_t merged_lo = x_lo + y_hi;
    if (merged_lo < x_lo) {
      merged_hi++;  // Carry the overflow bit.
    }

    // The "high resolution" approximation of e is still a lower bound. Once
    // again, see if the upper bound is large enough to produce a different
    // result. This time, if it does, give up instead of reaching for an even
    // more precise approximation to e.
    //
    // This three-part check is similar to the two-part check that guarded the
    // if block that we're now in, but it has an extra term for the middle 64
    // bits (checking that adding 1 to merged_lo would overflow).
    //
    // For example, parsing "5.9604644775390625e-8" will take the if-true
    // branch here, since:
    //  - merged_hi = 0x7FFFFFFFFFFFFFFF
    //  - merged_lo = 0xFFFFFFFFFFFFFFFF
    //  - y_lo      = 0x4DB3FFC120988200
    //  - man       = 0xD3C21BCECCEDA100
    if (((merged_hi & 0x1FF) == 0x1FF) && ((merged_lo + 1) == 0) &&
        (y_lo + man < man)) {
      return -1;
    }

    // Replace the 128-bit x with merged.
    x_hi = merged_hi;
    x_lo = merged_lo;
  }

  // As mentioned above, shifting x_hi right by either 9 or 10 bits will leave
  // the top 10 MSBs (bits 54 ..= 63) off and the 11th MSB (bit 53) on. If the
  // MSB (before shifting) was on, adjust ret_exp2 for the larger shift.
  //
  // Having bit 53 on (and higher bits off) means that ret_mantissa is a 54-bit
  // number.
  uint64_t msb = x_hi >> 63;
  uint64_t ret_mantissa = x_hi >> (msb + 9);
  ret_exp2 -= 1 ^ msb;

  // IEEE 754 rounds to-nearest with ties rounded to-even. Rounding to-even can
  // be tricky. If we're half-way between two exactly representable numbers
  // (x's low 73 bits are zero and the next 2 bits that matter are "01"), give
  // up instead of trying to pick the winner.
  //
  // Technically, we could tighten the condition by changing "73" to "73 or 74,
  // depending on msb", but a flat "73" is simpler.
  //
  // For example, parsing "1e+23" will take the if-true branch here, since:
  //  - x_hi          = 0x54B40B1F852BDA00
  //  - ret_mantissa  = 0x002A5A058FC295ED
  if ((x_lo == 0) && ((x_hi & 0x1FF) == 0) && ((ret_mantissa & 3) == 1)) {
    return -1;
  }

  // If we're not halfway then it's rounding to-nearest. Starting with a 54-bit
  // number, carry the lowest bit (bit 0) up if it's on. Regardless of whether
  // it was on or off, shifting right by one then produces a 53-bit number. If
  // carrying up overflowed, shift again.
  ret_mantissa += ret_mantissa & 1;
  ret_mantissa >>= 1;
  // This if block is equivalent to (but benchmarks slightly faster than) the
  // following branchless form:
  //    uint64_t overflow_adjustment = ret_mantissa >> 53;
  //    ret_mantissa >>= overflow_adjustment;
  //    ret_exp2 += overflow_adjustment;
  //
  // For example, parsing "7.2057594037927933e+16" will take the if-true
  // branch here, since:
  //  - x_hi          = 0x7FFFFFFFFFFFFE80
  //  - ret_mantissa  = 0x0020000000000000
  if ((ret_mantissa >> 53) > 0) {
    ret_mantissa >>= 1;
    ret_exp2++;
  }

  // Starting with a 53-bit number, IEEE 754 double-precision normal numbers
  // have an implicit mantissa bit. Mask that away and keep the low 52 bits.
  ret_mantissa &= 0x000FFFFFFFFFFFFF;

  // Pack the bits and return.
  return ((int64_t)(ret_mantissa | (ret_exp2 << 52)));
}

// --------

static wuffs_base__result_f64  //
wuffs_private_impl__parse_number_f64_special(wuffs_base__slice_u8 s,
                                             uint32_t options) {
  do {
    if (options & WUFFS_BASE__PARSE_NUMBER_FXX__REJECT_INF_AND_NAN) {
      goto fail;
    }

    uint8_t* p = s.ptr;
    uint8_t* q = s.ptr + s.len;

    for (; (p < q) && (*p == '_'); p++) {
    }
    if (p >= q) {
      goto fail;
    }

    // Parse sign.
    bool negative = false;
    do {
      if (*p == '+') {
        p++;
      } else if (*p == '-') {
        negative = true;
        p++;
      } else {
        break;
      }
      for (; (p < q) && (*p == '_'); p++) {
      }
    } while (0);
    if (p >= q) {
      goto fail;
    }

    bool nan = false;
    switch (p[0]) {
      case 'I':
      case 'i':
        if (((q - p) < 3) ||                     //
            ((p[1] != 'N') && (p[1] != 'n')) ||  //
            ((p[2] != 'F') && (p[2] != 'f'))) {
          goto fail;
        }
        p += 3;

        if ((p >= q) || (*p == '_')) {
          break;
        } else if (((q - p) < 5) ||                     //
                   ((p[0] != 'I') && (p[0] != 'i')) ||  //
                   ((p[1] != 'N') && (p[1] != 'n')) ||  //
                   ((p[2] != 'I') && (p[2] != 'i')) ||  //
                   ((p[3] != 'T') && (p[3] != 't')) ||  //
                   ((p[4] != 'Y') && (p[4] != 'y'))) {
          goto fail;
        }
        p += 5;

        if ((p >= q) || (*p == '_')) {
          break;
        }
        goto fail;

      case 'N':
      case 'n':
        if (((q - p) < 3) ||                     //
            ((p[1] != 'A') && (p[1] != 'a')) ||  //
            ((p[2] != 'N') && (p[2] != 'n'))) {
          goto fail;
        }
        p += 3;

        if ((p >= q) || (*p == '_')) {
          nan = true;
          break;
        }
        goto fail;

      default:
        goto fail;
    }

    // Finish.
    for (; (p < q) && (*p == '_'); p++) {
    }
    if (p != q) {
      goto fail;
    }
    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
        (nan ? 0x7FFFFFFFFFFFFFFF : 0x7FF0000000000000) |
        (negative ? 0x8000000000000000 : 0));
    return ret;
  } while (0);

fail:
  do {
    wuffs_base__result_f64 ret;
    ret.status.repr = wuffs_base__error__bad_argument;
    ret.value = 0;
    return ret;
  } while (0);
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__result_f64  //
wuffs_private_impl__high_prec_dec__to_f64(wuffs_private_impl__high_prec_dec* h,
                                          uint32_t options) {
  do {
    // powers converts decimal powers of 10 to binary powers of 2. For example,
    // (10000 >> 13) is 1. It stops before the elements exceed 60, also known
    // as WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL.
    //
    // This rounds down (1<<13 is a lower bound for 1e4). Adding 1 to the array
    // element value rounds up (1<<14 is an upper bound for 1e4) while staying
    // at or below WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL.
    //
    // When starting in the range [1e+1 .. 1e+2] (i.e. h->decimal_point == +2),
    // powers[2] == 6 and so:
    //  - Right shifting by 6+0 produces the range [10/64 .. 100/64] =
    //    [0.156250 .. 1.56250]. The resultant h->decimal_point is +0 or +1.
    //  - Right shifting by 6+1 produces the range [10/128 .. 100/128] =
    //    [0.078125 .. 0.78125]. The resultant h->decimal_point is -1 or -0.
    //
    // When starting in the range [1e-3 .. 1e-2] (i.e. h->decimal_point == -2),
    // powers[2] == 6 and so:
    //  - Left shifting by 6+0 produces the range [0.001*64 .. 0.01*64] =
    //    [0.064 .. 0.64]. The resultant h->decimal_point is -1 or -0.
    //  - Left shifting by 6+1 produces the range [0.001*128 .. 0.01*128] =
    //    [0.128 .. 1.28]. The resultant h->decimal_point is +0 or +1.
    //
    // Thus, when targeting h->decimal_point being +0 or +1, use (powers[n]+0)
    // when right shifting but (powers[n]+1) when left shifting.
    static const uint32_t num_powers = 19;
    static const uint8_t powers[19] = {
        0,  3,  6,  9,  13, 16, 19, 23, 26, 29,  //
        33, 36, 39, 43, 46, 49, 53, 56, 59,      //
    };

    // Handle zero and obvious extremes. The largest and smallest positive
    // finite f64 values are approximately 1.8e+308 and 4.9e-324.
    if ((h->num_digits == 0) || (h->decimal_point < -326)) {
      goto zero;
    } else if (h->decimal_point > 310) {
      goto infinity;
    }

    // Try the fast Eisel-Lemire algorithm again. Calculating the (man, exp10)
    // pair from the high_prec_dec h is more correct but slower than the
    // approach taken in wuffs_base__parse_number_f64. The latter is optimized
    // for the common cases (e.g. assuming no underscores or a leading '+'
    // sign) rather than the full set of cases allowed by the Wuffs API.
    //
    // When we have 19 or fewer mantissa digits, run Eisel-Lemire once (trying
    // for an exact result). When we have more than 19 mantissa digits, run it
    // twice to get a lower and upper bound. We still have an exact result
    // (within f64's rounding margin) if both bounds are equal (and valid).
    uint32_t i_max = h->num_digits;
    if (i_max > 19) {
      i_max = 19;
    }
    int32_t exp10 = h->decimal_point - ((int32_t)i_max);
    if ((-307 <= exp10) && (exp10 <= 288)) {
      uint64_t man = 0;
      uint32_t i;
      for (i = 0; i < i_max; i++) {
        man = (10 * man) + h->digits[i];
      }
      while (man != 0) {  // The 'while' is just an 'if' that we can 'break'.
        int64_t r0 =
            wuffs_private_impl__parse_number_f64_eisel_lemire(man + 0, exp10);
        if (r0 < 0) {
          break;
        } else if (h->num_digits > 19) {
          int64_t r1 =
              wuffs_private_impl__parse_number_f64_eisel_lemire(man + 1, exp10);
          if (r1 != r0) {
            break;
          }
        }
        wuffs_base__result_f64 ret;
        ret.status.repr = NULL;
        ret.value = wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
            ((uint64_t)r0) | (((uint64_t)(h->negative)) << 63));
        return ret;
      }
    }

    // When Eisel-Lemire fails, fall back to Simple Decimal Conversion. See
    // https://nigeltao.github.io/blog/2020/parse-number-f64-simple.html
    //
    // Scale by powers of 2 until we're in the range [0.1 .. 10]. Equivalently,
    // that h->decimal_point is +0 or +1.
    //
    // First we shift right while at or above 10...
    const int32_t f64_bias = -1023;
    int32_t exp2 = 0;
    while (h->decimal_point > 1) {
      uint32_t n = (uint32_t)(+h->decimal_point);
      uint32_t shift = (n < num_powers)
                           ? powers[n]
                           : WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL;

      wuffs_private_impl__high_prec_dec__small_rshift(h, shift);
      if (h->decimal_point < -WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE) {
        goto zero;
      }
      exp2 += (int32_t)shift;
    }
    // ...then we shift left while below 0.1.
    while (h->decimal_point < 0) {
      uint32_t shift;
      uint32_t n = (uint32_t)(-h->decimal_point);
      shift = (n < num_powers)
                  // The +1 is per "when targeting h->decimal_point being +0 or
                  // +1... when left shifting" in the powers comment above.
                  ? (powers[n] + 1u)
                  : WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL;

      wuffs_private_impl__high_prec_dec__small_lshift(h, shift);
      if (h->decimal_point > +WUFFS_PRIVATE_IMPL__HPD__DECIMAL_POINT__RANGE) {
        goto infinity;
      }
      exp2 -= (int32_t)shift;
    }

    // To get from "in the range [0.1 .. 10]" to "in the range [1 .. 2]" (which
    // will give us our exponent in base-2), the mantissa's first 3 digits will
    // determine the final left shift, equal to 52 (the number of explicit f64
    // bits) plus an additional adjustment.
    int man3 = (100 * h->digits[0]) +
               ((h->num_digits > 1) ? (10 * h->digits[1]) : 0) +
               ((h->num_digits > 2) ? h->digits[2] : 0);
    int32_t additional_lshift = 0;
    if (h->decimal_point == 0) {  // The value is in [0.1 .. 1].
      if (man3 < 125) {
        additional_lshift = +4;
      } else if (man3 < 250) {
        additional_lshift = +3;
      } else if (man3 < 500) {
        additional_lshift = +2;
      } else {
        additional_lshift = +1;
      }
    } else {  // The value is in [1 .. 10].
      if (man3 < 200) {
        additional_lshift = -0;
      } else if (man3 < 400) {
        additional_lshift = -1;
      } else if (man3 < 800) {
        additional_lshift = -2;
      } else {
        additional_lshift = -3;
      }
    }
    exp2 -= additional_lshift;
    uint32_t final_lshift = (uint32_t)(52 + additional_lshift);

    // The minimum normal exponent is (f64_bias + 1).
    while ((f64_bias + 1) > exp2) {
      uint32_t n = (uint32_t)((f64_bias + 1) - exp2);
      if (n > WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL) {
        n = WUFFS_PRIVATE_IMPL__HPD__SHIFT__MAX_INCL;
      }
      wuffs_private_impl__high_prec_dec__small_rshift(h, n);
      exp2 += (int32_t)n;
    }

    // Check for overflow.
    if ((exp2 - f64_bias) >= 0x07FF) {  // (1 << 11) - 1.
      goto infinity;
    }

    // Extract 53 bits for the mantissa (in base-2).
    wuffs_private_impl__high_prec_dec__small_lshift(h, final_lshift);
    uint64_t man2 = wuffs_private_impl__high_prec_dec__rounded_integer(h);

    // Rounding might have added one bit. If so, shift and re-check overflow.
    if ((man2 >> 53) != 0) {
      man2 >>= 1;
      exp2++;
      if ((exp2 - f64_bias) >= 0x07FF) {  // (1 << 11) - 1.
        goto infinity;
      }
    }

    // Handle subnormal numbers.
    if ((man2 >> 52) == 0) {
      exp2 = f64_bias;
    }

    // Pack the bits and return.
    uint64_t exp2_bits =
        (uint64_t)((exp2 - f64_bias) & 0x07FF);              // (1 << 11) - 1.
    uint64_t bits = (man2 & 0x000FFFFFFFFFFFFF) |            // (1 << 52) - 1.
                    (exp2_bits << 52) |                      //
                    (h->negative ? 0x8000000000000000 : 0);  // (1 << 63).

    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__from_u64_to_f64(bits);
    return ret;
  } while (0);

zero:
  do {
    uint64_t bits = h->negative ? 0x8000000000000000 : 0;

    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__from_u64_to_f64(bits);
    return ret;
  } while (0);

infinity:
  do {
    if (options & WUFFS_BASE__PARSE_NUMBER_FXX__REJECT_INF_AND_NAN) {
      wuffs_base__result_f64 ret;
      ret.status.repr = wuffs_base__error__bad_argument;
      ret.value = 0;
      return ret;
    }

    uint64_t bits = h->negative ? 0xFFF0000000000000 : 0x7FF0000000000000;

    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__from_u64_to_f64(bits);
    return ret;
  } while (0);
}

static inline bool  //
wuffs_private_impl__is_decimal_digit(uint8_t c) {
  return ('0' <= c) && (c <= '9');
}

WUFFS_BASE__MAYBE_STATIC wuffs_base__result_f64  //
wuffs_base__parse_number_f64(wuffs_base__slice_u8 s, uint32_t options) {
  // In practice, almost all "dd.ddddE±xxx" numbers can be represented
  // losslessly by a uint64_t mantissa "dddddd" and an int32_t base-10
  // exponent, adjusting "xxx" for the position (if present) of the decimal
  // separator '.' or ','.
  //
  // This (u64 man, i32 exp10) data structure is superficially similar to the
  // "Do It Yourself Floating Point" type from Loitsch (†), but the exponent
  // here is base-10, not base-2.
  //
  // If s's number fits in a (man, exp10), parse that pair with the
  // Eisel-Lemire algorithm. If not, or if Eisel-Lemire fails, parsing s with
  // the fallback algorithm is slower but comprehensive.
  //
  // † "Printing Floating-Point Numbers Quickly and Accurately with Integers"
  // (https://www.cs.tufts.edu/~nr/cs257/archive/florian-loitsch/printf.pdf).
  // Florian Loitsch is also the primary contributor to
  // https://github.com/google/double-conversion
  do {
    // Calculating that (man, exp10) pair needs to stay within s's bounds.
    // Provided that s isn't extremely long, work on a NUL-terminated copy of
    // s's contents. The NUL byte isn't a valid part of "±dd.ddddE±xxx".
    //
    // As the pointer p walks the contents, it's faster to repeatedly check "is
    // *p a valid digit" than "is p within bounds and *p a valid digit".
    if (s.len >= 256) {
      goto fallback;
    }
    uint8_t z[256];
    memcpy(&z[0], s.ptr, s.len);
    z[s.len] = 0;
    const uint8_t* p = &z[0];

    // Look for a leading minus sign. Technically, we could also look for an
    // optional plus sign, but the "script/process-json-numbers.c with -p"
    // benchmark is noticably slower if we do. It's optional and, in practice,
    // usually absent. Let the fallback catch it.
    bool negative = (*p == '-');
    if (negative) {
      p++;
    }

    // After walking "dd.dddd", comparing p later with p now will produce the
    // number of "d"s and "."s.
    const uint8_t* const start_of_digits_ptr = p;

    // Walk the "d"s before a '.', 'E', NUL byte, etc. If it starts with '0',
    // it must be a single '0'. If it starts with a non-zero decimal digit, it
    // can be a sequence of decimal digits.
    //
    // Update the man variable during the walk. It's OK if man overflows now.
    // We'll detect that later.
    uint64_t man;
    if (*p == '0') {
      man = 0;
      p++;
      if (wuffs_private_impl__is_decimal_digit(*p)) {
        goto fallback;
      }
    } else if (wuffs_private_impl__is_decimal_digit(*p)) {
      man = ((uint8_t)(*p - '0'));
      p++;
      for (; wuffs_private_impl__is_decimal_digit(*p); p++) {
        man = (10 * man) + ((uint8_t)(*p - '0'));
      }
    } else {
      goto fallback;
    }

    // Walk the "d"s after the optional decimal separator ('.' or ','),
    // updating the man and exp10 variables.
    int32_t exp10 = 0;
    if (*p ==
        ((options & WUFFS_BASE__PARSE_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA)
             ? ','
             : '.')) {
      p++;
      const uint8_t* first_after_separator_ptr = p;
      if (!wuffs_private_impl__is_decimal_digit(*p)) {
        goto fallback;
      }
      man = (10 * man) + ((uint8_t)(*p - '0'));
      p++;
      for (; wuffs_private_impl__is_decimal_digit(*p); p++) {
        man = (10 * man) + ((uint8_t)(*p - '0'));
      }
      exp10 = ((int32_t)(first_after_separator_ptr - p));
    }

    // Count the number of digits:
    //  - for an input of "314159",  digit_count is 6.
    //  - for an input of "3.14159", digit_count is 7.
    //
    // This is off-by-one if there is a decimal separator. That's OK for now.
    // We'll correct for that later. The "script/process-json-numbers.c with
    // -p" benchmark is noticably slower if we try to correct for that now.
    uint32_t digit_count = (uint32_t)(p - start_of_digits_ptr);

    // Update exp10 for the optional exponent, starting with 'E' or 'e'.
    if ((*p | 0x20) == 'e') {
      p++;
      int32_t exp_sign = +1;
      if (*p == '-') {
        p++;
        exp_sign = -1;
      } else if (*p == '+') {
        p++;
      }
      if (!wuffs_private_impl__is_decimal_digit(*p)) {
        goto fallback;
      }
      int32_t exp_num = ((uint8_t)(*p - '0'));
      p++;
      // The rest of the exp_num walking has a peculiar control flow but, once
      // again, the "script/process-json-numbers.c with -p" benchmark is
      // sensitive to alternative formulations.
      if (wuffs_private_impl__is_decimal_digit(*p)) {
        exp_num = (10 * exp_num) + ((uint8_t)(*p - '0'));
        p++;
      }
      if (wuffs_private_impl__is_decimal_digit(*p)) {
        exp_num = (10 * exp_num) + ((uint8_t)(*p - '0'));
        p++;
      }
      while (wuffs_private_impl__is_decimal_digit(*p)) {
        if (exp_num > 0x1000000) {
          goto fallback;
        }
        exp_num = (10 * exp_num) + ((uint8_t)(*p - '0'));
        p++;
      }
      exp10 += exp_sign * exp_num;
    }

    // The Wuffs API is that the original slice has no trailing data. It also
    // allows underscores, which we don't catch here but the fallback should.
    if (p != &z[s.len]) {
      goto fallback;
    }

    // Check that the uint64_t typed man variable has not overflowed, based on
    // digit_count.
    //
    // For reference:
    //   - (1 << 63) is  9223372036854775808, which has 19 decimal digits.
    //   - (1 << 64) is 18446744073709551616, which has 20 decimal digits.
    //   - 19 nines,  9999999999999999999, is  0x8AC7230489E7FFFF, which has 64
    //     bits and 16 hexadecimal digits.
    //   - 20 nines, 99999999999999999999, is 0x56BC75E2D630FFFFF, which has 67
    //     bits and 17 hexadecimal digits.
    if (digit_count > 19) {
      // Even if we have more than 19 pseudo-digits, it's not yet definitely an
      // overflow. Recall that digit_count might be off-by-one (too large) if
      // there's a decimal separator. It will also over-report the number of
      // meaningful digits if the input looks something like "0.000dddExxx".
      //
      // We adjust by the number of leading '0's and '.'s and re-compare to 19.
      // Once again, technically, we could skip ','s too, but that perturbs the
      // "script/process-json-numbers.c with -p" benchmark.
      const uint8_t* q = start_of_digits_ptr;
      for (; (*q == '0') || (*q == '.'); q++) {
      }
      digit_count -= (uint32_t)(q - start_of_digits_ptr);
      if (digit_count > 19) {
        goto fallback;
      }
    }

    // The wuffs_private_impl__parse_number_f64_eisel_lemire preconditions
    // include that exp10 is in the range [-307 ..= 288].
    if ((exp10 < -307) || (288 < exp10)) {
      goto fallback;
    }

    // If both man and (10 ** exp10) are exactly representable by a double, we
    // don't need to run the Eisel-Lemire algorithm.
    if ((-22 <= exp10) && (exp10 <= 22) && ((man >> 53) == 0)) {
      double d = (double)man;
      if (exp10 >= 0) {
        d *= wuffs_private_impl__f64_powers_of_10[+exp10];
      } else {
        d /= wuffs_private_impl__f64_powers_of_10[-exp10];
      }
      wuffs_base__result_f64 ret;
      ret.status.repr = NULL;
      ret.value = negative ? -d : +d;
      return ret;
    }

    // The wuffs_private_impl__parse_number_f64_eisel_lemire preconditions
    // include that man is non-zero. Parsing "0" should be caught by the "If
    // both man and (10 ** exp10)" above, but "0e99" might not.
    if (man == 0) {
      goto fallback;
    }

    // Our man and exp10 are in range. Run the Eisel-Lemire algorithm.
    int64_t r = wuffs_private_impl__parse_number_f64_eisel_lemire(man, exp10);
    if (r < 0) {
      goto fallback;
    }
    wuffs_base__result_f64 ret;
    ret.status.repr = NULL;
    ret.value = wuffs_base__ieee_754_bit_representation__from_u64_to_f64(
        ((uint64_t)r) | (((uint64_t)negative) << 63));
    return ret;
  } while (0);

fallback:
  do {
    wuffs_private_impl__high_prec_dec h;
    wuffs_base__status status =
        wuffs_private_impl__high_prec_dec__parse(&h, s, options);
    if (status.repr) {
      return wuffs_private_impl__parse_number_f64_special(s, options);
    }
    return wuffs_private_impl__high_prec_dec__to_f64(&h, options);
  } while (0);
}

// --------

static inline size_t  //
wuffs_private_impl__render_inf(wuffs_base__slice_u8 dst,
                               bool neg,
                               uint32_t options) {
  if (neg) {
    if (dst.len < 4) {
      return 0;
    }
    wuffs_base__poke_u32le__no_bounds_check(dst.ptr, 0x666E492D);  // '-Inf'le.
    return 4;
  }

  if (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN) {
    if (dst.len < 4) {
      return 0;
    }
    wuffs_base__poke_u32le__no_bounds_check(dst.ptr, 0x666E492B);  // '+Inf'le.
    return 4;
  }

  if (dst.len < 3) {
    return 0;
  }
  wuffs_base__poke_u24le__no_bounds_check(dst.ptr, 0x666E49);  // 'Inf'le.
  return 3;
}

static inline size_t  //
wuffs_private_impl__render_nan(wuffs_base__slice_u8 dst) {
  if (dst.len < 3) {
    return 0;
  }
  wuffs_base__poke_u24le__no_bounds_check(dst.ptr, 0x4E614E);  // 'NaN'le.
  return 3;
}

static size_t  //
wuffs_private_impl__high_prec_dec__render_exponent_absent(
    wuffs_base__slice_u8 dst,
    wuffs_private_impl__high_prec_dec* h,
    uint32_t precision,
    uint32_t options) {
  size_t n = (h->negative ||
              (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN))
                 ? 1
                 : 0;
  if (h->decimal_point <= 0) {
    n += 1;
  } else {
    n += (size_t)(h->decimal_point);
  }
  if (precision > 0) {
    n += precision + 1;  // +1 for the '.'.
  }

  // Don't modify dst if the formatted number won't fit.
  if (n > dst.len) {
    return 0;
  }

  // Align-left or align-right.
  uint8_t* ptr = (options & WUFFS_BASE__RENDER_NUMBER_XXX__ALIGN_RIGHT)
                     ? &dst.ptr[dst.len - n]
                     : &dst.ptr[0];

  // Leading "±".
  if (h->negative) {
    *ptr++ = '-';
  } else if (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN) {
    *ptr++ = '+';
  }

  // Integral digits.
  if (h->decimal_point <= 0) {
    *ptr++ = '0';
  } else {
    uint32_t m =
        wuffs_base__u32__min(h->num_digits, (uint32_t)(h->decimal_point));
    uint32_t i = 0;
    for (; i < m; i++) {
      *ptr++ = (uint8_t)('0' | h->digits[i]);
    }
    for (; i < (uint32_t)(h->decimal_point); i++) {
      *ptr++ = '0';
    }
  }

  // Separator and then fractional digits.
  if (precision > 0) {
    *ptr++ =
        (options & WUFFS_BASE__RENDER_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA)
            ? ','
            : '.';
    uint32_t i = 0;
    for (; i < precision; i++) {
      uint32_t j = ((uint32_t)(h->decimal_point)) + i;
      *ptr++ = (uint8_t)('0' | ((j < h->num_digits) ? h->digits[j] : 0));
    }
  }

  return n;
}

static size_t  //
wuffs_private_impl__high_prec_dec__render_exponent_present(
    wuffs_base__slice_u8 dst,
    wuffs_private_impl__high_prec_dec* h,
    uint32_t precision,
    uint32_t options) {
  int32_t exp = 0;
  if (h->num_digits > 0) {
    exp = h->decimal_point - 1;
  }
  bool negative_exp = exp < 0;
  if (negative_exp) {
    exp = -exp;
  }

  size_t n = (h->negative ||
              (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN))
                 ? 4
                 : 3;  // Mininum 3 bytes: first digit and then "e±".
  if (precision > 0) {
    n += precision + 1;  // +1 for the '.'.
  }
  n += (exp < 100) ? 2 : 3;

  // Don't modify dst if the formatted number won't fit.
  if (n > dst.len) {
    return 0;
  }

  // Align-left or align-right.
  uint8_t* ptr = (options & WUFFS_BASE__RENDER_NUMBER_XXX__ALIGN_RIGHT)
                     ? &dst.ptr[dst.len - n]
                     : &dst.ptr[0];

  // Leading "±".
  if (h->negative) {
    *ptr++ = '-';
  } else if (options & WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN) {
    *ptr++ = '+';
  }

  // Integral digit.
  if (h->num_digits > 0) {
    *ptr++ = (uint8_t)('0' | h->digits[0]);
  } else {
    *ptr++ = '0';
  }

  // Separator and then fractional digits.
  if (precision > 0) {
    *ptr++ =
        (options & WUFFS_BASE__RENDER_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA)
            ? ','
            : '.';
    uint32_t i = 1;
    uint32_t j = wuffs_base__u32__min(h->num_digits, precision + 1);
    for (; i < j; i++) {
      *ptr++ = (uint8_t)('0' | h->digits[i]);
    }
    for (; i <= precision; i++) {
      *ptr++ = '0';
    }
  }

  // Exponent: "e±" and then 2 or 3 digits.
  *ptr++ = 'e';
  *ptr++ = negative_exp ? '-' : '+';
  if (exp < 10) {
    *ptr++ = '0';
    *ptr++ = (uint8_t)('0' | exp);
  } else if (exp < 100) {
    *ptr++ = (uint8_t)('0' | (exp / 10));
    *ptr++ = (uint8_t)('0' | (exp % 10));
  } else {
    int32_t e = exp / 100;
    exp -= e * 100;
    *ptr++ = (uint8_t)('0' | e);
    *ptr++ = (uint8_t)('0' | (exp / 10));
    *ptr++ = (uint8_t)('0' | (exp % 10));
  }

  return n;
}

WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__render_number_f64(wuffs_base__slice_u8 dst,
                              double x,
                              uint32_t precision,
                              uint32_t options) {
  // Decompose x (64 bits) into negativity (1 bit), base-2 exponent (11 bits
  // with a -1023 bias) and mantissa (52 bits).
  uint64_t bits = wuffs_base__ieee_754_bit_representation__from_f64_to_u64(x);
  bool neg = (bits >> 63) != 0;
  int32_t exp2 = ((int32_t)(bits >> 52)) & 0x7FF;
  uint64_t man = bits & 0x000FFFFFFFFFFFFFul;

  // Apply the exponent bias and set the implicit top bit of the mantissa,
  // unless x is subnormal. Also take care of Inf and NaN.
  if (exp2 == 0x7FF) {
    if (man != 0) {
      return wuffs_private_impl__render_nan(dst);
    }
    return wuffs_private_impl__render_inf(dst, neg, options);
  } else if (exp2 == 0) {
    exp2 = -1022;
  } else {
    exp2 -= 1023;
    man |= 0x0010000000000000ul;
  }

  // Ensure that precision isn't too large.
  if (precision > 4095) {
    precision = 4095;
  }

  // Convert from the (neg, exp2, man) tuple to an HPD.
  wuffs_private_impl__high_prec_dec h;
  wuffs_private_impl__high_prec_dec__assign(&h, man, neg);
  if (h.num_digits > 0) {
    wuffs_private_impl__high_prec_dec__lshift(&h,
                                              exp2 - 52);  // 52 mantissa bits.
  }

  // Handle the "%e" and "%f" formats.
  switch (options & (WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT |
                     WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_PRESENT)) {
    case WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT:  // The "%"f" format.
      if (options & WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION) {
        wuffs_private_impl__high_prec_dec__round_just_enough(&h, exp2, man);
        int32_t p = ((int32_t)(h.num_digits)) - h.decimal_point;
        precision = ((uint32_t)(wuffs_base__i32__max(0, p)));
      } else {
        wuffs_private_impl__high_prec_dec__round_nearest(
            &h, ((int32_t)precision) + h.decimal_point);
      }
      return wuffs_private_impl__high_prec_dec__render_exponent_absent(
          dst, &h, precision, options);

    case WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_PRESENT:  // The "%e" format.
      if (options & WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION) {
        wuffs_private_impl__high_prec_dec__round_just_enough(&h, exp2, man);
        precision = (h.num_digits > 0) ? (h.num_digits - 1) : 0;
      } else {
        wuffs_private_impl__high_prec_dec__round_nearest(
            &h, ((int32_t)precision) + 1);
      }
      return wuffs_private_impl__high_prec_dec__render_exponent_present(
          dst, &h, precision, options);
  }

  // We have the "%g" format and so precision means the number of significant
  // digits, not the number of digits after the decimal separator. Perform
  // rounding and determine whether to use "%e" or "%f".
  int32_t e_threshold = 0;
  if (options & WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION) {
    wuffs_private_impl__high_prec_dec__round_just_enough(&h, exp2, man);
    precision = h.num_digits;
    e_threshold = 6;
  } else {
    if (precision == 0) {
      precision = 1;
    }
    wuffs_private_impl__high_prec_dec__round_nearest(&h, ((int32_t)precision));
    e_threshold = ((int32_t)precision);
    int32_t nd = ((int32_t)(h.num_digits));
    if ((e_threshold > nd) && (nd >= h.decimal_point)) {
      e_threshold = nd;
    }
  }

  // Use the "%e" format if the exponent is large.
  int32_t e = h.decimal_point - 1;
  if ((e < -4) || (e_threshold <= e)) {
    uint32_t p = wuffs_base__u32__min(precision, h.num_digits);
    return wuffs_private_impl__high_prec_dec__render_exponent_present(
        dst, &h, (p > 0) ? (p - 1) : 0, options);
  }

  // Use the "%f" format otherwise.
  int32_t p = ((int32_t)precision);
  if (p > h.decimal_point) {
    p = ((int32_t)(h.num_digits));
  }
  precision = ((uint32_t)(wuffs_base__i32__max(0, p - h.decimal_point)));
  return wuffs_private_impl__high_prec_dec__render_exponent_absent(
      dst, &h, precision, options);
}
