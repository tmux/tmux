// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- String Conversions

// Options (bitwise or'ed together) for wuffs_base__parse_number_xxx
// functions. The XXX options apply to both integer and floating point. The FXX
// options apply only to floating point.

#define WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS ((uint32_t)0x00000000)

// WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES means to accept
// inputs like "00", "0644" and "00.7". By default, they are rejected.
#define WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_MULTIPLE_LEADING_ZEROES \
  ((uint32_t)0x00000001)

// WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES means to accept inputs like
// "1__2" and "_3.141_592". By default, they are rejected.
#define WUFFS_BASE__PARSE_NUMBER_XXX__ALLOW_UNDERSCORES ((uint32_t)0x00000002)

// WUFFS_BASE__PARSE_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA means to accept
// "1,5" and not "1.5" as one-and-a-half.
//
// If the caller wants to accept either, it is responsible for canonicalizing
// the input before calling wuffs_base__parse_number_fxx. The caller also has
// more context on e.g. exactly how to treat something like "$1,234".
#define WUFFS_BASE__PARSE_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA \
  ((uint32_t)0x00000010)

// WUFFS_BASE__PARSE_NUMBER_FXX__REJECT_INF_AND_NAN means to reject inputs that
// would lead to infinite or Not-a-Number floating point values. By default,
// they are accepted.
//
// This affects the literal "inf" as input, but also affects inputs like
// "1e999" that would overflow double-precision floating point.
#define WUFFS_BASE__PARSE_NUMBER_FXX__REJECT_INF_AND_NAN ((uint32_t)0x00000020)

// --------

// Options (bitwise or'ed together) for wuffs_base__render_number_xxx
// functions. The XXX options apply to both integer and floating point. The FXX
// options apply only to floating point.

#define WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS ((uint32_t)0x00000000)

// WUFFS_BASE__RENDER_NUMBER_XXX__ALIGN_RIGHT means to render to the right side
// (higher indexes) of the destination slice, leaving any untouched bytes on
// the left side (lower indexes). The default is vice versa: rendering on the
// left with slack on the right.
#define WUFFS_BASE__RENDER_NUMBER_XXX__ALIGN_RIGHT ((uint32_t)0x00000100)

// WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN means to render the leading
// "+" for non-negative numbers: "+0" and "+12.3" instead of "0" and "12.3".
#define WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN ((uint32_t)0x00000200)

// WUFFS_BASE__RENDER_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA means to render
// one-and-a-half as "1,5" instead of "1.5".
#define WUFFS_BASE__RENDER_NUMBER_FXX__DECIMAL_SEPARATOR_IS_A_COMMA \
  ((uint32_t)0x00001000)

// WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ETC means whether to never
// (EXPONENT_ABSENT, equivalent to printf's "%f") or to always
// (EXPONENT_PRESENT, equivalent to printf's "%e") render a floating point
// number as "1.23e+05" instead of "123000".
//
// Having both bits set is the same has having neither bit set, where the
// notation used depends on whether the exponent is sufficiently large: "0.5"
// is preferred over "5e-01" but "5e-09" is preferred over "0.000000005".
#define WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT ((uint32_t)0x00002000)
#define WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_PRESENT ((uint32_t)0x00004000)

// WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION means to render the
// smallest number of digits so that parsing the resultant string will recover
// the same double-precision floating point number.
//
// For example, double-precision cannot distinguish between 0.3 and
// 0.299999999999999988897769753748434595763683319091796875, so when this bit
// is set, rendering the latter will produce "0.3" but rendering
// 0.3000000000000000444089209850062616169452667236328125 will produce
// "0.30000000000000004".
#define WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION \
  ((uint32_t)0x00008000)

// ---------------- IEEE 754 Floating Point

// wuffs_base__ieee_754_bit_representation__etc converts between a double
// precision numerical value and its IEEE 754 representations:
//  - 16-bit: 1 sign bit,  5 exponent bits, 10 explicit significand bits.
//  - 32-bit: 1 sign bit,  8 exponent bits, 23 explicit significand bits.
//  - 64-bit: 1 sign bit, 11 exponent bits, 52 explicit significand bits.
//
// For example, it converts between:
//  - +1.0 and 0x3C00, 0x3F80_0000 or 0x3FF0_0000_0000_0000.
//  - +5.5 and 0x4580, 0x40B0_0000 or 0x4016_0000_0000_0000.
//  - -inf and 0xFC00, 0xFF80_0000 or 0xFFF0_0000_0000_0000.
//
// Converting from f64 to shorter formats (f16 or f32, represented in C as
// uint16_t and uint32_t) may be lossy. Such functions have names that look
// like etc_truncate, as converting finite numbers produce equal or smaller
// (closer-to-zero) finite numbers. For example, 1048576.0 is a perfectly valid
// f64 number, but converting it to a f16 (with truncation) produces 65504.0,
// the largest finite f16 number. Truncating a f64-typed value d to f32 does
// not always produce the same result as the C-style cast ((float)d), as
// casting can convert from finite numbers to infinite ones.
//
// Converting infinities or NaNs produces infinities or NaNs and always report
// no loss, even though there a multiple NaN representations so that round-
// tripping a f64-typed NaN may produce a different 64 bits. Nonetheless, the
// etc_truncate functions preserve a NaN's "quiet vs signaling" bit.
//
// See https://en.wikipedia.org/wiki/Double-precision_floating-point_format

typedef struct wuffs_base__lossy_value_u16__struct {
  uint16_t value;
  bool lossy;
} wuffs_base__lossy_value_u16;

typedef struct wuffs_base__lossy_value_u32__struct {
  uint32_t value;
  bool lossy;
} wuffs_base__lossy_value_u32;

WUFFS_BASE__MAYBE_STATIC wuffs_base__lossy_value_u16  //
wuffs_base__ieee_754_bit_representation__from_f64_to_u16_truncate(double f);

WUFFS_BASE__MAYBE_STATIC wuffs_base__lossy_value_u32  //
wuffs_base__ieee_754_bit_representation__from_f64_to_u32_truncate(double f);

static inline uint64_t  //
wuffs_base__ieee_754_bit_representation__from_f64_to_u64(double f) {
  uint64_t u = 0;
  if (sizeof(uint64_t) == sizeof(double)) {
    memcpy(&u, &f, sizeof(uint64_t));
  }
  return u;
}

static inline double  //
wuffs_base__ieee_754_bit_representation__from_u16_to_f64(uint16_t u) {
  uint64_t v = ((uint64_t)(u & 0x8000)) << 48;

  do {
    uint64_t exp = (u >> 10) & 0x1F;
    uint64_t man = u & 0x3FF;
    if (exp == 0x1F) {  // Infinity or NaN.
      exp = 2047;
    } else if (exp != 0) {  // Normal.
      exp += 1008;          // 1008 = 1023 - 15, the difference in biases.
    } else if (man != 0) {  // Subnormal but non-zero.
      uint32_t clz = wuffs_base__count_leading_zeroes_u64(man);
      exp = 1062 - clz;  // 1062 = 1008 + 64 - 10.
      man = 0x3FF & (man << (clz - 53));
    } else {  // Zero.
      break;
    }
    v |= (exp << 52) | (man << 42);
  } while (0);

  double f = 0;
  if (sizeof(uint64_t) == sizeof(double)) {
    memcpy(&f, &v, sizeof(uint64_t));
  }
  return f;
}

static inline double  //
wuffs_base__ieee_754_bit_representation__from_u32_to_f64(uint32_t u) {
  float f = 0;
  if (sizeof(uint32_t) == sizeof(float)) {
    memcpy(&f, &u, sizeof(uint32_t));
  }
  return (double)f;
}

static inline double  //
wuffs_base__ieee_754_bit_representation__from_u64_to_f64(uint64_t u) {
  double f = 0;
  if (sizeof(uint64_t) == sizeof(double)) {
    memcpy(&f, &u, sizeof(uint64_t));
  }
  return f;
}

// ---------------- Parsing and Rendering Numbers

// wuffs_base__parse_number_f64 parses the floating point number in s. For
// example, if s contains the bytes "1.5" then it will return the double 1.5.
//
// It returns an error if s does not contain a floating point number.
//
// It does not necessarily return an error if the conversion is lossy, e.g. if
// s is "0.3", which double-precision floating point cannot represent exactly.
//
// Similarly, the returned value may be infinite (and no error returned) even
// if s was not "inf", when the input is nominally finite but sufficiently
// larger than DBL_MAX, about 1.8e+308.
//
// It is similar to the C standard library's strtod function, but:
//  - Errors are returned in-band (in a result type), not out-of-band (errno).
//  - It takes a slice (a pointer and length), not a NUL-terminated C string.
//  - It does not take an optional endptr argument. It does not allow a partial
//    parse: it returns an error unless all of s is consumed.
//  - It does not allow whitespace, leading or otherwise.
//  - It does not allow hexadecimal floating point numbers.
//  - It is not affected by i18n / l10n settings such as environment variables.
//
// The options argument can change these, but by default, it:
//  - Allows "inf", "+Infinity" and "-NAN", case insensitive. Similarly,
//    without an explicit opt-out, it would successfully parse "1e999" as
//    infinity, even though it overflows double-precision floating point.
//  - Rejects underscores. With an explicit opt-in, "_3.141_592" would
//    successfully parse as an approximation to π.
//  - Rejects unnecessary leading zeroes: "00", "0644" and "00.7".
//  - Uses a dot '1.5' instead of a comma '1,5' for the decimal separator.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__FLOATCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__result_f64  //
wuffs_base__parse_number_f64(wuffs_base__slice_u8 s, uint32_t options);

// wuffs_base__parse_number_i64 parses the ASCII integer in s. For example, if
// s contains the bytes "-123" then it will return the int64_t -123.
//
// It returns an error if s does not contain an integer or if the integer
// within would overflow an int64_t.
//
// It is similar to wuffs_base__parse_number_u64 but it returns a signed
// integer, not an unsigned integer. It also allows a leading '+' or '-'.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__result_i64  //
wuffs_base__parse_number_i64(wuffs_base__slice_u8 s, uint32_t options);

// wuffs_base__parse_number_u64 parses the ASCII integer in s. For example, if
// s contains the bytes "123" then it will return the uint64_t 123.
//
// It returns an error if s does not contain an integer or if the integer
// within would overflow a uint64_t.
//
// It is similar to the C standard library's strtoull function, but:
//  - Errors are returned in-band (in a result type), not out-of-band (errno).
//  - It takes a slice (a pointer and length), not a NUL-terminated C string.
//  - It does not take an optional endptr argument. It does not allow a partial
//    parse: it returns an error unless all of s is consumed.
//  - It does not allow whitespace, leading or otherwise.
//  - It does not allow a leading '+' or '-'.
//  - It does not take a base argument (e.g. base 10 vs base 16). Instead, it
//    always accepts both decimal (e.g "1234", "0d5678") and hexadecimal (e.g.
//    "0x9aBC"). The caller is responsible for prior filtering of e.g. hex
//    numbers if they are unwanted. For example, Wuffs' JSON decoder will only
//    produce a wuffs_base__token for decimal numbers, not hexadecimal.
//  - It is not affected by i18n / l10n settings such as environment variables.
//
// The options argument can change these, but by default, it:
//  - Rejects underscores. With an explicit opt-in, "__0D_1_002" would
//    successfully parse as "one thousand and two". Underscores are still
//    rejected inside the optional 2-byte opening "0d" or "0X" that denotes
//    base-10 or base-16.
//  - Rejects unnecessary leading zeroes: "00" and "0644".
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__result_u64  //
wuffs_base__parse_number_u64(wuffs_base__slice_u8 s, uint32_t options);

// --------

// WUFFS_BASE__I64__BYTE_LENGTH__MAX_INCL is the string length of
// "-9223372036854775808" and "+9223372036854775807", INT64_MIN and INT64_MAX.
#define WUFFS_BASE__I64__BYTE_LENGTH__MAX_INCL 20

// WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL is the string length of
// "+18446744073709551615", UINT64_MAX.
#define WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL 21

// wuffs_base__render_number_f64 writes the decimal encoding of x to dst and
// returns the number of bytes written. If dst is shorter than the entire
// encoding, it returns 0 (and no bytes are written).
//
// For those familiar with C's printf or Go's fmt.Printf functions:
//  - "%e" means the WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_PRESENT option.
//  - "%f" means the WUFFS_BASE__RENDER_NUMBER_FXX__EXPONENT_ABSENT  option.
//  - "%g" means neither or both bits are set.
//
// The precision argument controls the number of digits rendered, excluding the
// exponent (the "e+05" in "1.23e+05"):
//  - for "%e" and "%f" it is the number of digits after the decimal separator,
//  - for "%g" it is the number of significant digits (and trailing zeroes are
//    removed).
//
// A precision of 6 gives similar output to printf's defaults.
//
// A precision greater than 4095 is equivalent to 4095.
//
// The precision argument is ignored when the
// WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION option is set. This is
// similar to Go's strconv.FormatFloat with a negative (i.e. non-sensical)
// precision, but there is no corresponding feature in C's printf.
//
// Extreme values of x will be rendered as "NaN", "Inf" (or "+Inf" if the
// WUFFS_BASE__RENDER_NUMBER_XXX__LEADING_PLUS_SIGN option is set) or "-Inf".
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__FLOATCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__render_number_f64(wuffs_base__slice_u8 dst,
                              double x,
                              uint32_t precision,
                              uint32_t options);

// wuffs_base__render_number_i64 writes the decimal encoding of x to dst and
// returns the number of bytes written. If dst is shorter than the entire
// encoding, it returns 0 (and no bytes are written).
//
// dst will never be too short if its length is at least 20, also known as
// WUFFS_BASE__I64__BYTE_LENGTH__MAX_INCL.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__render_number_i64(wuffs_base__slice_u8 dst,
                              int64_t x,
                              uint32_t options);

// wuffs_base__render_number_u64 writes the decimal encoding of x to dst and
// returns the number of bytes written. If dst is shorter than the entire
// encoding, it returns 0 (and no bytes are written).
//
// dst will never be too short if its length is at least 21, also known as
// WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__render_number_u64(wuffs_base__slice_u8 dst,
                              uint64_t x,
                              uint32_t options);

// ---------------- Base-16

// Options (bitwise or'ed together) for wuffs_base__base_16__xxx functions.

#define WUFFS_BASE__BASE_16__DEFAULT_OPTIONS ((uint32_t)0x00000000)

// wuffs_base__base_16__decode2 converts "6A6b" to "jk", where e.g. 'j' is
// U+006A. There are 2 src bytes for every dst byte.
//
// It assumes that the src bytes are two hexadecimal digits (0-9, A-F, a-f),
// repeated. It may write nonsense bytes if not, although it will not read or
// write out of bounds.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_16__decode2(wuffs_base__slice_u8 dst,
                             wuffs_base__slice_u8 src,
                             bool src_closed,
                             uint32_t options);

// wuffs_base__base_16__decode4 converts both "\\x6A\\x6b" and "??6a??6B" to
// "jk", where e.g. 'j' is U+006A. There are 4 src bytes for every dst byte.
//
// It assumes that the src bytes are two ignored bytes and then two hexadecimal
// digits (0-9, A-F, a-f), repeated. It may write nonsense bytes if not,
// although it will not read or write out of bounds.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_16__decode4(wuffs_base__slice_u8 dst,
                             wuffs_base__slice_u8 src,
                             bool src_closed,
                             uint32_t options);

// wuffs_base__base_16__encode2 converts "jk" to "6A6B", where e.g. 'j' is
// U+006A. There are 2 dst bytes for every src byte.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_16__encode2(wuffs_base__slice_u8 dst,
                             wuffs_base__slice_u8 src,
                             bool src_closed,
                             uint32_t options);

// wuffs_base__base_16__encode4 converts "jk" to "\\x6A\\x6B", where e.g. 'j'
// is U+006A. There are 4 dst bytes for every src byte.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_16__encode4(wuffs_base__slice_u8 dst,
                             wuffs_base__slice_u8 src,
                             bool src_closed,
                             uint32_t options);

// ---------------- Base-64

// Options (bitwise or'ed together) for wuffs_base__base_64__xxx functions.

#define WUFFS_BASE__BASE_64__DEFAULT_OPTIONS ((uint32_t)0x00000000)

// WUFFS_BASE__BASE_64__DECODE_ALLOW_PADDING means that, when decoding base-64,
// the input may (but does not need to) be padded with '=' bytes so that the
// overall encoded length in bytes is a multiple of 4. A successful decoding
// will return a num_src that includes those padding bytes.
//
// Excess padding (e.g. three final '='s) will be rejected as bad data.
#define WUFFS_BASE__BASE_64__DECODE_ALLOW_PADDING ((uint32_t)0x00000001)

// WUFFS_BASE__BASE_64__ENCODE_EMIT_PADDING means that, when encoding base-64,
// the output will be padded with '=' bytes so that the overall encoded length
// in bytes is a multiple of 4.
#define WUFFS_BASE__BASE_64__ENCODE_EMIT_PADDING ((uint32_t)0x00000002)

// WUFFS_BASE__BASE_64__URL_ALPHABET means that, for base-64, the URL-friendly
// and file-name-friendly alphabet be used, as per RFC 4648 section 5. When
// this option bit is off, the standard alphabet from section 4 is used.
#define WUFFS_BASE__BASE_64__URL_ALPHABET ((uint32_t)0x00000100)

// wuffs_base__base_64__decode transforms base-64 encoded bytes from src to
// arbitrary bytes in dst.
//
// It will not permit line breaks or other whitespace in src. Filtering those
// out is the responsibility of the caller.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_64__decode(wuffs_base__slice_u8 dst,
                            wuffs_base__slice_u8 src,
                            bool src_closed,
                            uint32_t options);

// wuffs_base__base_64__encode transforms arbitrary bytes from src to base-64
// encoded bytes in dst.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__INTCONV sub-module, not
// just WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__transform__output  //
wuffs_base__base_64__encode(wuffs_base__slice_u8 dst,
                            wuffs_base__slice_u8 src,
                            bool src_closed,
                            uint32_t options);

// ---------------- Unicode and UTF-8

#define WUFFS_BASE__UNICODE_CODE_POINT__MIN_INCL 0x00000000
#define WUFFS_BASE__UNICODE_CODE_POINT__MAX_INCL 0x0010FFFF

#define WUFFS_BASE__UNICODE_REPLACEMENT_CHARACTER 0x0000FFFD

#define WUFFS_BASE__UNICODE_SURROGATE__MIN_INCL 0x0000D800
#define WUFFS_BASE__UNICODE_SURROGATE__MAX_INCL 0x0000DFFF

#define WUFFS_BASE__ASCII__MIN_INCL 0x00
#define WUFFS_BASE__ASCII__MAX_INCL 0x7F

#define WUFFS_BASE__UTF_8__BYTE_LENGTH__MIN_INCL 1
#define WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL 4

#define WUFFS_BASE__UTF_8__BYTE_LENGTH_1__CODE_POINT__MIN_INCL 0x00000000
#define WUFFS_BASE__UTF_8__BYTE_LENGTH_1__CODE_POINT__MAX_INCL 0x0000007F
#define WUFFS_BASE__UTF_8__BYTE_LENGTH_2__CODE_POINT__MIN_INCL 0x00000080
#define WUFFS_BASE__UTF_8__BYTE_LENGTH_2__CODE_POINT__MAX_INCL 0x000007FF
#define WUFFS_BASE__UTF_8__BYTE_LENGTH_3__CODE_POINT__MIN_INCL 0x00000800
#define WUFFS_BASE__UTF_8__BYTE_LENGTH_3__CODE_POINT__MAX_INCL 0x0000FFFF
#define WUFFS_BASE__UTF_8__BYTE_LENGTH_4__CODE_POINT__MIN_INCL 0x00010000
#define WUFFS_BASE__UTF_8__BYTE_LENGTH_4__CODE_POINT__MAX_INCL 0x0010FFFF

// --------

// wuffs_base__utf_8__next__output is the type returned by
// wuffs_base__utf_8__next.
typedef struct wuffs_base__utf_8__next__output__struct {
  uint32_t code_point;
  uint32_t byte_length;

#ifdef __cplusplus
  inline bool is_valid() const;
#endif  // __cplusplus

} wuffs_base__utf_8__next__output;

static inline wuffs_base__utf_8__next__output  //
wuffs_base__make_utf_8__next__output(uint32_t code_point,
                                     uint32_t byte_length) {
  wuffs_base__utf_8__next__output ret;
  ret.code_point = code_point;
  ret.byte_length = byte_length;
  return ret;
}

static inline bool  //
wuffs_base__utf_8__next__output__is_valid(
    const wuffs_base__utf_8__next__output* o) {
  if (o) {
    uint32_t cp = o->code_point;
    switch (o->byte_length) {
      case 1:
        return (cp <= 0x7F);
      case 2:
        return (0x080 <= cp) && (cp <= 0x7FF);
      case 3:
        // Avoid the 0xD800 ..= 0xDFFF surrogate range.
        return ((0x0800 <= cp) && (cp <= 0xD7FF)) ||
               ((0xE000 <= cp) && (cp <= 0xFFFF));
      case 4:
        return (0x00010000 <= cp) && (cp <= 0x0010FFFF);
    }
  }
  return false;
}

#ifdef __cplusplus

inline bool  //
wuffs_base__utf_8__next__output::is_valid() const {
  return wuffs_base__utf_8__next__output__is_valid(this);
}

#endif  // __cplusplus

// --------

// wuffs_base__utf_8__encode writes the UTF-8 encoding of code_point to s and
// returns the number of bytes written. If code_point is invalid, or if s is
// shorter than the entire encoding, it returns 0 (and no bytes are written).
//
// s will never be too short if its length is at least 4, also known as
// WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__UTF8 sub-module, not just
// WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__utf_8__encode(wuffs_base__slice_u8 dst, uint32_t code_point);

// wuffs_base__utf_8__next returns the next UTF-8 code point (and that code
// point's byte length) at the start of the read-only slice (s_ptr, s_len).
//
// There are exactly two cases in which this function returns something where
// wuffs_base__utf_8__next__output__is_valid is false:
//  - If s is empty then it returns {.code_point=0, .byte_length=0}.
//  - If s is non-empty and starts with invalid UTF-8 then it returns
//    {.code_point=WUFFS_BASE__UNICODE_REPLACEMENT_CHARACTER, .byte_length=1}.
//
// Otherwise, it returns something where
// wuffs_base__utf_8__next__output__is_valid is true.
//
// In any case, it always returns an output that satisfies both of:
//  - (output.code_point  <= WUFFS_BASE__UNICODE_CODE_POINT__MAX_INCL).
//  - (output.byte_length <= s_len).
//
// If s is a sub-slice of a larger slice of valid UTF-8, but that sub-slice
// boundary occurs in the middle of a multi-byte UTF-8 encoding of a single
// code point, then this function may return something invalid. It is the
// caller's responsibility to split on or otherwise manage UTF-8 boundaries.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__UTF8 sub-module, not just
// WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__utf_8__next__output  //
wuffs_base__utf_8__next(const uint8_t* s_ptr, size_t s_len);

// wuffs_base__utf_8__next_from_end is like wuffs_base__utf_8__next except that
// it looks at the end of (s_ptr, s_len) instead of the start.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__UTF8 sub-module, not just
// WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC wuffs_base__utf_8__next__output  //
wuffs_base__utf_8__next_from_end(const uint8_t* s_ptr, size_t s_len);

// wuffs_base__utf_8__longest_valid_prefix returns the largest n such that the
// sub-slice s[..n] is valid UTF-8, where s is the read-only slice (s_ptr,
// s_len).
//
// In particular, it returns s_len if and only if all of s is valid UTF-8.
//
// If s is a sub-slice of a larger slice of valid UTF-8, but that sub-slice
// boundary occurs in the middle of a multi-byte UTF-8 encoding of a single
// code point, then this function will return less than s_len. It is the
// caller's responsibility to split on or otherwise manage UTF-8 boundaries.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__UTF8 sub-module, not just
// WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__utf_8__longest_valid_prefix(const uint8_t* s_ptr, size_t s_len);

// wuffs_base__ascii__longest_valid_prefix returns the largest n such that the
// sub-slice s[..n] is valid ASCII, where s is the read-only slice (s_ptr,
// s_len).
//
// In particular, it returns s_len if and only if all of s is valid ASCII.
// Equivalently, when none of the bytes in s have the 0x80 high bit set.
//
// For modular builds that divide the base module into sub-modules, using this
// function requires the WUFFS_CONFIG__MODULE__BASE__UTF8 sub-module, not just
// WUFFS_CONFIG__MODULE__BASE__CORE.
WUFFS_BASE__MAYBE_STATIC size_t  //
wuffs_base__ascii__longest_valid_prefix(const uint8_t* s_ptr, size_t s_len);
