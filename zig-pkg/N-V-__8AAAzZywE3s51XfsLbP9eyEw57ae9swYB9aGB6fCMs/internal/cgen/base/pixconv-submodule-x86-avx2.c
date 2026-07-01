// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// --------

// ‼ WUFFS MULTI-FILE SECTION +x86_avx2
#if defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V3)
WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2,avx2")
static void  //
wuffs_private_impl__swizzle_ycc__convert_3_bgrx_x86_avx2(
    wuffs_base__pixel_buffer* dst,
    uint32_t x,
    uint32_t x_end,
    uint32_t y,
    const uint8_t* up0,
    const uint8_t* up1,
    const uint8_t* up2) {
  if ((x + 32u) > x_end) {
    wuffs_private_impl__swizzle_ycc__convert_3_bgrx(  //
        dst, x, x_end, y, up0, up1, up2);
    return;
  }

  size_t dst_stride = dst->private_impl.planes[0].stride;
  uint8_t* dst_iter = dst->private_impl.planes[0].ptr +
                      (dst_stride * ((size_t)y)) + (4u * ((size_t)x));

  // u0001 = u16x16 [0x0001 .. 0x0001]
  // u00FF = u16x16 [0x00FF .. 0x00FF]
  // uFF80 = u16x16 [0xFF80 .. 0xFF80]
  // uFFFF = u16x16 [0xFFFF .. 0xFFFF]
  const __m256i u0001 = _mm256_set1_epi16(+0x0001);
  const __m256i u00FF = _mm256_set1_epi16(+0x00FF);
  const __m256i uFF80 = _mm256_set1_epi16(-0x0080);
  const __m256i uFFFF = _mm256_set1_epi16(-0x0001);

  // p8000_p0000 = u16x16 [0x8000 0x0000 .. 0x8000 0x0000]
  const __m256i p8000_p0000 = _mm256_set_epi16(  //
      +0x0000, -0x8000, +0x0000, -0x8000,        //
      +0x0000, -0x8000, +0x0000, -0x8000,        //
      +0x0000, -0x8000, +0x0000, -0x8000,        //
      +0x0000, -0x8000, +0x0000, -0x8000);

  // Per wuffs_base__color_ycc__as__color_u32, the formulae:
  //
  //  R = Y                + 1.40200 * Cr
  //  G = Y - 0.34414 * Cb - 0.71414 * Cr
  //  B = Y + 1.77200 * Cb
  //
  // When scaled by 1<<16:
  //
  //  0.34414 becomes 0x0581A =  22554.
  //  0.71414 becomes 0x0B6D2 =  46802.
  //  1.40200 becomes 0x166E9 =  91881.
  //  1.77200 becomes 0x1C5A2 = 116130.
  //
  // Separate the integer and fractional parts, since we work with signed
  // 16-bit SIMD lanes. The fractional parts range from -0.5 .. +0.5 (as
  // floating-point) which is from -0x8000 .. +0x8000 (as fixed-point).
  //
  //  -0x3A5E = -0x20000 + 0x1C5A2     The B:Cb factor.
  //  +0x66E9 = -0x10000 + 0x166E9     The R:Cr factor.
  //  -0x581A = +0x00000 - 0x0581A     The G:Cb factor.
  //  +0x492E = +0x10000 - 0x0B6D2     The G:Cr factor.
  const __m256i m3A5E = _mm256_set1_epi16(-0x3A5E);
  const __m256i p66E9 = _mm256_set1_epi16(+0x66E9);
  const __m256i m581A_p492E = _mm256_set_epi16(  //
      +0x492E, -0x581A, +0x492E, -0x581A,        //
      +0x492E, -0x581A, +0x492E, -0x581A,        //
      +0x492E, -0x581A, +0x492E, -0x581A,        //
      +0x492E, -0x581A, +0x492E, -0x581A);

  while (x < x_end) {
    // Load chroma values in even and odd columns (the high 8 bits of each
    // u16x16 element are zero) and then subtract 0x0080.
    //
    // cb_all = u8x32  [cb.00 cb.01 cb.02 cb.03 .. cb.1C cb.1D cb.1E cb.1F]
    // cb_eve = i16x16 [cb.00-0x80  cb.02-0x80  .. cb.1C-0x80  cb.1E-0x80 ]
    // cb_odd = i16x16 [cb.01-0x80  cb.03-0x80  .. cb.1D-0x80  cb.1F-0x80 ]
    //
    // Ditto for the cr_xxx Chroma-Red values.
    __m256i cb_all = _mm256_lddqu_si256((const __m256i*)(const void*)up1);
    __m256i cr_all = _mm256_lddqu_si256((const __m256i*)(const void*)up2);
    __m256i cb_eve = _mm256_add_epi16(uFF80, _mm256_and_si256(cb_all, u00FF));
    __m256i cr_eve = _mm256_add_epi16(uFF80, _mm256_and_si256(cr_all, u00FF));
    __m256i cb_odd = _mm256_add_epi16(uFF80, _mm256_srli_epi16(cb_all, 8));
    __m256i cr_odd = _mm256_add_epi16(uFF80, _mm256_srli_epi16(cr_all, 8));

    // ----

    // Calculate:
    //
    //  B-Y = (+1.77200 * Cb)                 as floating-point
    //  R-Y = (+1.40200 * Cr)                 as floating-point
    //
    //  B-Y = ((0x2_0000 - 0x3A5E) * Cb)      as fixed-point
    //  R-Y = ((0x1_0000 + 0x66E9) * Cr)      as fixed-point
    //
    //  B-Y = ((-0x3A5E * Cb) + ("2.0" * Cb))
    //  R-Y = ((+0x66E9 * Cr) + ("1.0" * Cr))

    // Multiply by m3A5E or p66E9, taking the high 16 bits. There's also a
    // doubling (add x to itself), adding-of-1 and halving (shift right by 1).
    // That makes multiply-and-take-high round to nearest (instead of down).
    __m256i tmp_by_eve = _mm256_srai_epi16(
        _mm256_add_epi16(
            _mm256_mulhi_epi16(_mm256_add_epi16(cb_eve, cb_eve), m3A5E), u0001),
        1);
    __m256i tmp_by_odd = _mm256_srai_epi16(
        _mm256_add_epi16(
            _mm256_mulhi_epi16(_mm256_add_epi16(cb_odd, cb_odd), m3A5E), u0001),
        1);
    __m256i tmp_ry_eve = _mm256_srai_epi16(
        _mm256_add_epi16(
            _mm256_mulhi_epi16(_mm256_add_epi16(cr_eve, cr_eve), p66E9), u0001),
        1);
    __m256i tmp_ry_odd = _mm256_srai_epi16(
        _mm256_add_epi16(
            _mm256_mulhi_epi16(_mm256_add_epi16(cr_odd, cr_odd), p66E9), u0001),
        1);

    // Add (2 * Cb) and (1 * Cr).
    __m256i by_eve =
        _mm256_add_epi16(tmp_by_eve, _mm256_add_epi16(cb_eve, cb_eve));
    __m256i by_odd =
        _mm256_add_epi16(tmp_by_odd, _mm256_add_epi16(cb_odd, cb_odd));
    __m256i ry_eve = _mm256_add_epi16(tmp_ry_eve, cr_eve);
    __m256i ry_odd = _mm256_add_epi16(tmp_ry_odd, cr_odd);

    // ----

    // Calculate:
    //
    //  G-Y = (-0.34414 * Cb) +
    //        (-0.71414 * Cr)                 as floating-point
    //
    //  G-Y = ((+0x0_0000 - 0x581A) * Cb) +
    //        ((-0x1_0000 + 0x492E) * Cr)     as fixed-point
    //
    //  G-Y =  (-0x581A * Cb) +
    //         (+0x492E * Cr) - ("1.0" * Cr)

    // Multiply-add to get ((-0x581A * Cb) + (+0x492E * Cr)).
    __m256i tmp0_gy_eve_lo = _mm256_madd_epi16(  //
        _mm256_unpacklo_epi16(cb_eve, cr_eve), m581A_p492E);
    __m256i tmp0_gy_eve_hi = _mm256_madd_epi16(  //
        _mm256_unpackhi_epi16(cb_eve, cr_eve), m581A_p492E);
    __m256i tmp0_gy_odd_lo = _mm256_madd_epi16(  //
        _mm256_unpacklo_epi16(cb_odd, cr_odd), m581A_p492E);
    __m256i tmp0_gy_odd_hi = _mm256_madd_epi16(  //
        _mm256_unpackhi_epi16(cb_odd, cr_odd), m581A_p492E);

    // Divide the i32x8 vectors by (1 << 16), rounding to nearest.
    __m256i tmp1_gy_eve_lo =
        _mm256_srai_epi32(_mm256_add_epi32(tmp0_gy_eve_lo, p8000_p0000), 16);
    __m256i tmp1_gy_eve_hi =
        _mm256_srai_epi32(_mm256_add_epi32(tmp0_gy_eve_hi, p8000_p0000), 16);
    __m256i tmp1_gy_odd_lo =
        _mm256_srai_epi32(_mm256_add_epi32(tmp0_gy_odd_lo, p8000_p0000), 16);
    __m256i tmp1_gy_odd_hi =
        _mm256_srai_epi32(_mm256_add_epi32(tmp0_gy_odd_hi, p8000_p0000), 16);

    // Pack the ((-0x581A * Cb) + (+0x492E * Cr)) as i16x16 and subtract Cr.
    __m256i gy_eve = _mm256_sub_epi16(
        _mm256_packs_epi32(tmp1_gy_eve_lo, tmp1_gy_eve_hi), cr_eve);
    __m256i gy_odd = _mm256_sub_epi16(
        _mm256_packs_epi32(tmp1_gy_odd_lo, tmp1_gy_odd_hi), cr_odd);

    // ----

    // Add Y to (B-Y), (G-Y) and (R-Y) to produce B, G and R.
    //
    // For the resultant packed_x_xxx vectors, only elements 0 ..= 7 and 16 ..=
    // 23 of the 32-element vectors matter (since we'll unpacklo but not
    // unpackhi them). Let … denote 8 ignored consecutive u8 values and let %
    // denote 0xFF. We'll end this section with:
    //
    // packed_b_eve = u8x32 [b00 b02 .. b0C b0E  …  b10 b12 .. b1C b1E  …]
    // packed_b_odd = u8x32 [b01 b03 .. b0D b0F  …  b11 b13 .. b1D b1F  …]
    // packed_g_eve = u8x32 [g00 g02 .. g0C g0E  …  g10 g12 .. g1C g1E  …]
    // packed_g_odd = u8x32 [g01 g03 .. g0D g0F  …  g11 g13 .. g1D g1F  …]
    // packed_r_eve = u8x32 [r00 r02 .. r0C r0E  …  r10 r12 .. r1C r1E  …]
    // packed_r_odd = u8x32 [r01 r03 .. r0D r0F  …  r11 r13 .. r1D r1F  …]
    // uFFFF        = u8x32 [  %   % ..   %   %  …    %   % ..   %   %  …]

    __m256i yy_all = _mm256_lddqu_si256((const __m256i*)(const void*)up0);
    __m256i yy_eve = _mm256_and_si256(yy_all, u00FF);
    __m256i yy_odd = _mm256_srli_epi16(yy_all, 8);

    __m256i loose_b_eve = _mm256_add_epi16(by_eve, yy_eve);
    __m256i loose_b_odd = _mm256_add_epi16(by_odd, yy_odd);
    __m256i packed_b_eve = _mm256_packus_epi16(loose_b_eve, loose_b_eve);
    __m256i packed_b_odd = _mm256_packus_epi16(loose_b_odd, loose_b_odd);

    __m256i loose_g_eve = _mm256_add_epi16(gy_eve, yy_eve);
    __m256i loose_g_odd = _mm256_add_epi16(gy_odd, yy_odd);
    __m256i packed_g_eve = _mm256_packus_epi16(loose_g_eve, loose_g_eve);
    __m256i packed_g_odd = _mm256_packus_epi16(loose_g_odd, loose_g_odd);

    __m256i loose_r_eve = _mm256_add_epi16(ry_eve, yy_eve);
    __m256i loose_r_odd = _mm256_add_epi16(ry_odd, yy_odd);
    __m256i packed_r_eve = _mm256_packus_epi16(loose_r_eve, loose_r_eve);
    __m256i packed_r_odd = _mm256_packus_epi16(loose_r_odd, loose_r_odd);

    // ----

    // Mix those values (unpacking in 8, 16 and then 32 bit units) to get the
    // desired BGRX/RGBX order.
    //
    // From here onwards, all of our __m256i registers are u8x32.

    // mix00 = [b00 g00 b02 g02 .. b0E g0E b10 g10 .. b1C g1C b1E g1E]
    // mix01 = [b01 g01 b03 g03 .. b0F g0F b11 g11 .. b1D g1D b1F g1F]
    // mix02 = [r00   % r02   % .. r0E   % r10   % .. r1C   % r1E   %]
    // mix03 = [r01   % r03   % .. r0F   % r11   % .. r1D   % r1F   %]
    //
    // See also § below.
    __m256i mix00 = _mm256_unpacklo_epi8(packed_b_eve, packed_g_eve);
    __m256i mix01 = _mm256_unpacklo_epi8(packed_b_odd, packed_g_odd);
    __m256i mix02 = _mm256_unpacklo_epi8(packed_r_eve, uFFFF);
    __m256i mix03 = _mm256_unpacklo_epi8(packed_r_odd, uFFFF);

    // mix10 = [b00 g00 r00 %  b02 g02 r02 %  b04 g04 r04 %  b06 g06 r06 %
    //          b10 g10 r10 %  b12 g12 r12 %  b14 g14 r14 %  b16 g16 r16 %]
    // mix11 = [b01 g01 r01 %  b03 g03 r03 %  b05 g05 r05 %  b07 g07 r07 %
    //          b11 g11 r11 %  b13 g13 r13 %  b15 g15 r15 %  b17 g17 r17 %]
    // mix12 = [b08 g08 r08 %  b0A g0A r0A %  b0C g0C r0C %  b0E g0E r0E %
    //          b18 g18 r18 %  b1A g1A r1A %  b1C g1C r1C %  b1E g1E r1E %]
    // mix13 = [b09 g09 r09 %  b0B g0B r0B %  b0D g0D r0D %  b0F g0F r0F %
    //          b19 g19 r19 %  b1B g1B r1B %  b1D g1D r1D %  b1F g1F r1F %]
    __m256i mix10 = _mm256_unpacklo_epi16(mix00, mix02);
    __m256i mix11 = _mm256_unpacklo_epi16(mix01, mix03);
    __m256i mix12 = _mm256_unpackhi_epi16(mix00, mix02);
    __m256i mix13 = _mm256_unpackhi_epi16(mix01, mix03);

    // mix20 = [b00 g00 r00 %  b01 g01 r01 %  b02 g02 r02 %  b03 g03 r03 %
    //          b10 g10 r10 %  b11 g11 r11 %  b12 g12 r12 %  b13 g13 r13 %]
    // mix21 = [b04 g04 r04 %  b05 g05 r05 %  b06 g06 r06 %  b07 g07 r07 %
    //          b14 g14 r14 %  b15 g15 r15 %  b16 g16 r16 %  b17 g17 r17 %]
    // mix22 = [b08 g08 r08 %  b09 g09 r09 %  b0A g0A r0A %  b0B g0B r0B %
    //          b18 g18 r18 %  b19 g19 r19 %  b1A g1A r1A %  b1B g1B r1B %]
    // mix23 = [b0C g0C r0C %  b0D g0D r0D %  b0E g0E r0E %  b0F g0F r0F %
    //          b1C g1C r1C %  b1D g1D r1D %  b1E g1E r1E %  b1F g1F r1F %]
    __m256i mix20 = _mm256_unpacklo_epi32(mix10, mix11);
    __m256i mix21 = _mm256_unpackhi_epi32(mix10, mix11);
    __m256i mix22 = _mm256_unpacklo_epi32(mix12, mix13);
    __m256i mix23 = _mm256_unpackhi_epi32(mix12, mix13);

    // mix30 = [b00 g00 r00 %  b01 g01 r01 %  b02 g02 r02 %  b03 g03 r03 %
    //          b04 g04 r04 %  b05 g05 r05 %  b06 g06 r06 %  b07 g07 r07 %]
    // mix31 = [b08 g08 r08 %  b09 g09 r09 %  b0A g0A r0A %  b0B g0B r0B %
    //          b0C g0C r0C %  b0D g0D r0D %  b0E g0E r0E %  b0F g0F r0F %]
    // mix32 = [b10 g10 r10 %  b11 g11 r11 %  b12 g12 r12 %  b13 g13 r13 %
    //          b14 g14 r14 %  b15 g15 r15 %  b16 g16 r16 %  b17 g17 r17 %]
    // mix33 = [b18 g18 r18 %  b19 g19 r19 %  b1A g1A r1A %  b1B g1B r1B %
    //          b1C g1C r1C %  b1D g1D r1D %  b1E g1E r1E %  b1F g1F r1F %]
    __m256i mix30 = _mm256_permute2x128_si256(mix20, mix21, 0x20);
    __m256i mix31 = _mm256_permute2x128_si256(mix22, mix23, 0x20);
    __m256i mix32 = _mm256_permute2x128_si256(mix20, mix21, 0x31);
    __m256i mix33 = _mm256_permute2x128_si256(mix22, mix23, 0x31);

    // Write out four u8x32 SIMD registers (128 bytes, 32 BGRX/RGBX pixels).
    _mm256_storeu_si256((__m256i*)(void*)(dst_iter + 0x00), mix30);
    _mm256_storeu_si256((__m256i*)(void*)(dst_iter + 0x20), mix31);
    _mm256_storeu_si256((__m256i*)(void*)(dst_iter + 0x40), mix32);
    _mm256_storeu_si256((__m256i*)(void*)(dst_iter + 0x60), mix33);

    // Advance by up to 32 pixels. The first iteration might be smaller than 32
    // so that all of the remaining steps are exactly 32.
    uint32_t n = 32u - (31u & (x - x_end));
    dst_iter += 4u * n;
    up0 += n;
    up1 += n;
    up2 += n;
    x += n;
  }
}

// The rgbx flavor (below) is exactly the same as the bgrx flavor (above)
// except for the lines marked with a § and that comments were stripped.
WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2,avx2")
static void  //
wuffs_private_impl__swizzle_ycc__convert_3_rgbx_x86_avx2(
    wuffs_base__pixel_buffer* dst,
    uint32_t x,
    uint32_t x_end,
    uint32_t y,
    const uint8_t* up0,
    const uint8_t* up1,
    const uint8_t* up2) {
  if ((x + 32u) > x_end) {
    wuffs_private_impl__swizzle_ycc__convert_3_bgrx(  //
        dst, x, x_end, y, up0, up1, up2);
    return;
  }

  size_t dst_stride = dst->private_impl.planes[0].stride;
  uint8_t* dst_iter = dst->private_impl.planes[0].ptr +
                      (dst_stride * ((size_t)y)) + (4u * ((size_t)x));

  const __m256i u0001 = _mm256_set1_epi16(+0x0001);
  const __m256i u00FF = _mm256_set1_epi16(+0x00FF);
  const __m256i uFF80 = _mm256_set1_epi16(-0x0080);
  const __m256i uFFFF = _mm256_set1_epi16(-0x0001);

  const __m256i p8000_p0000 = _mm256_set_epi16(  //
      +0x0000, -0x8000, +0x0000, -0x8000,        //
      +0x0000, -0x8000, +0x0000, -0x8000,        //
      +0x0000, -0x8000, +0x0000, -0x8000,        //
      +0x0000, -0x8000, +0x0000, -0x8000);

  const __m256i m3A5E = _mm256_set1_epi16(-0x3A5E);
  const __m256i p66E9 = _mm256_set1_epi16(+0x66E9);
  const __m256i m581A_p492E = _mm256_set_epi16(  //
      +0x492E, -0x581A, +0x492E, -0x581A,        //
      +0x492E, -0x581A, +0x492E, -0x581A,        //
      +0x492E, -0x581A, +0x492E, -0x581A,        //
      +0x492E, -0x581A, +0x492E, -0x581A);

  while (x < x_end) {
    __m256i cb_all = _mm256_lddqu_si256((const __m256i*)(const void*)up1);
    __m256i cr_all = _mm256_lddqu_si256((const __m256i*)(const void*)up2);
    __m256i cb_eve = _mm256_add_epi16(uFF80, _mm256_and_si256(cb_all, u00FF));
    __m256i cr_eve = _mm256_add_epi16(uFF80, _mm256_and_si256(cr_all, u00FF));
    __m256i cb_odd = _mm256_add_epi16(uFF80, _mm256_srli_epi16(cb_all, 8));
    __m256i cr_odd = _mm256_add_epi16(uFF80, _mm256_srli_epi16(cr_all, 8));

    __m256i tmp_by_eve = _mm256_srai_epi16(
        _mm256_add_epi16(
            _mm256_mulhi_epi16(_mm256_add_epi16(cb_eve, cb_eve), m3A5E), u0001),
        1);
    __m256i tmp_by_odd = _mm256_srai_epi16(
        _mm256_add_epi16(
            _mm256_mulhi_epi16(_mm256_add_epi16(cb_odd, cb_odd), m3A5E), u0001),
        1);
    __m256i tmp_ry_eve = _mm256_srai_epi16(
        _mm256_add_epi16(
            _mm256_mulhi_epi16(_mm256_add_epi16(cr_eve, cr_eve), p66E9), u0001),
        1);
    __m256i tmp_ry_odd = _mm256_srai_epi16(
        _mm256_add_epi16(
            _mm256_mulhi_epi16(_mm256_add_epi16(cr_odd, cr_odd), p66E9), u0001),
        1);

    __m256i by_eve =
        _mm256_add_epi16(tmp_by_eve, _mm256_add_epi16(cb_eve, cb_eve));
    __m256i by_odd =
        _mm256_add_epi16(tmp_by_odd, _mm256_add_epi16(cb_odd, cb_odd));
    __m256i ry_eve = _mm256_add_epi16(tmp_ry_eve, cr_eve);
    __m256i ry_odd = _mm256_add_epi16(tmp_ry_odd, cr_odd);

    __m256i tmp0_gy_eve_lo = _mm256_madd_epi16(  //
        _mm256_unpacklo_epi16(cb_eve, cr_eve), m581A_p492E);
    __m256i tmp0_gy_eve_hi = _mm256_madd_epi16(  //
        _mm256_unpackhi_epi16(cb_eve, cr_eve), m581A_p492E);
    __m256i tmp0_gy_odd_lo = _mm256_madd_epi16(  //
        _mm256_unpacklo_epi16(cb_odd, cr_odd), m581A_p492E);
    __m256i tmp0_gy_odd_hi = _mm256_madd_epi16(  //
        _mm256_unpackhi_epi16(cb_odd, cr_odd), m581A_p492E);

    __m256i tmp1_gy_eve_lo =
        _mm256_srai_epi32(_mm256_add_epi32(tmp0_gy_eve_lo, p8000_p0000), 16);
    __m256i tmp1_gy_eve_hi =
        _mm256_srai_epi32(_mm256_add_epi32(tmp0_gy_eve_hi, p8000_p0000), 16);
    __m256i tmp1_gy_odd_lo =
        _mm256_srai_epi32(_mm256_add_epi32(tmp0_gy_odd_lo, p8000_p0000), 16);
    __m256i tmp1_gy_odd_hi =
        _mm256_srai_epi32(_mm256_add_epi32(tmp0_gy_odd_hi, p8000_p0000), 16);

    __m256i gy_eve = _mm256_sub_epi16(
        _mm256_packs_epi32(tmp1_gy_eve_lo, tmp1_gy_eve_hi), cr_eve);
    __m256i gy_odd = _mm256_sub_epi16(
        _mm256_packs_epi32(tmp1_gy_odd_lo, tmp1_gy_odd_hi), cr_odd);

    __m256i yy_all = _mm256_lddqu_si256((const __m256i*)(const void*)up0);
    __m256i yy_eve = _mm256_and_si256(yy_all, u00FF);
    __m256i yy_odd = _mm256_srli_epi16(yy_all, 8);

    __m256i loose_b_eve = _mm256_add_epi16(by_eve, yy_eve);
    __m256i loose_b_odd = _mm256_add_epi16(by_odd, yy_odd);
    __m256i packed_b_eve = _mm256_packus_epi16(loose_b_eve, loose_b_eve);
    __m256i packed_b_odd = _mm256_packus_epi16(loose_b_odd, loose_b_odd);

    __m256i loose_g_eve = _mm256_add_epi16(gy_eve, yy_eve);
    __m256i loose_g_odd = _mm256_add_epi16(gy_odd, yy_odd);
    __m256i packed_g_eve = _mm256_packus_epi16(loose_g_eve, loose_g_eve);
    __m256i packed_g_odd = _mm256_packus_epi16(loose_g_odd, loose_g_odd);

    __m256i loose_r_eve = _mm256_add_epi16(ry_eve, yy_eve);
    __m256i loose_r_odd = _mm256_add_epi16(ry_odd, yy_odd);
    __m256i packed_r_eve = _mm256_packus_epi16(loose_r_eve, loose_r_eve);
    __m256i packed_r_odd = _mm256_packus_epi16(loose_r_odd, loose_r_odd);

    // § Note the swapped B and R channels.
    __m256i mix00 = _mm256_unpacklo_epi8(packed_r_eve, packed_g_eve);
    __m256i mix01 = _mm256_unpacklo_epi8(packed_r_odd, packed_g_odd);
    __m256i mix02 = _mm256_unpacklo_epi8(packed_b_eve, uFFFF);
    __m256i mix03 = _mm256_unpacklo_epi8(packed_b_odd, uFFFF);

    __m256i mix10 = _mm256_unpacklo_epi16(mix00, mix02);
    __m256i mix11 = _mm256_unpacklo_epi16(mix01, mix03);
    __m256i mix12 = _mm256_unpackhi_epi16(mix00, mix02);
    __m256i mix13 = _mm256_unpackhi_epi16(mix01, mix03);

    __m256i mix20 = _mm256_unpacklo_epi32(mix10, mix11);
    __m256i mix21 = _mm256_unpackhi_epi32(mix10, mix11);
    __m256i mix22 = _mm256_unpacklo_epi32(mix12, mix13);
    __m256i mix23 = _mm256_unpackhi_epi32(mix12, mix13);

    __m256i mix30 = _mm256_permute2x128_si256(mix20, mix21, 0x20);
    __m256i mix31 = _mm256_permute2x128_si256(mix22, mix23, 0x20);
    __m256i mix32 = _mm256_permute2x128_si256(mix20, mix21, 0x31);
    __m256i mix33 = _mm256_permute2x128_si256(mix22, mix23, 0x31);

    _mm256_storeu_si256((__m256i*)(void*)(dst_iter + 0x00), mix30);
    _mm256_storeu_si256((__m256i*)(void*)(dst_iter + 0x20), mix31);
    _mm256_storeu_si256((__m256i*)(void*)(dst_iter + 0x40), mix32);
    _mm256_storeu_si256((__m256i*)(void*)(dst_iter + 0x60), mix33);

    uint32_t n = 32u - (31u & (x - x_end));
    dst_iter += 4u * n;
    up0 += n;
    up1 += n;
    up2 += n;
    x += n;
  }
}

#if defined(__GNUC__) && !defined(__clang__)
// No-op.
#else
WUFFS_BASE__MAYBE_ATTRIBUTE_TARGET("pclmul,popcnt,sse4.2,avx2")
static const uint8_t*  //
wuffs_private_impl__swizzle_ycc__upsample_inv_h2v2_triangle_x86_avx2(
    uint8_t* dst_ptr,
    const uint8_t* src_ptr_major,
    const uint8_t* src_ptr_minor,
    size_t src_len,
    uint32_t h1v2_bias_ignored,
    bool first_column,
    bool last_column) {
  uint8_t* dp = dst_ptr;
  const uint8_t* sp_major = src_ptr_major;
  const uint8_t* sp_minor = src_ptr_minor;

  if (first_column) {
    src_len--;
    if ((src_len <= 0u) && last_column) {
      uint32_t sv = (12u * ((uint32_t)(*sp_major++))) +  //
                    (4u * ((uint32_t)(*sp_minor++)));
      *dp++ = (uint8_t)((sv + 8u) >> 4u);
      *dp++ = (uint8_t)((sv + 7u) >> 4u);
      return dst_ptr;
    }

    uint32_t sv_major_m1 = sp_major[-0];  // Clamp offset to zero.
    uint32_t sv_minor_m1 = sp_minor[-0];  // Clamp offset to zero.
    uint32_t sv_major_p1 = sp_major[+1];
    uint32_t sv_minor_p1 = sp_minor[+1];

    uint32_t sv = (9u * ((uint32_t)(*sp_major++))) +  //
                  (3u * ((uint32_t)(*sp_minor++)));
    *dp++ = (uint8_t)((sv + (3u * sv_major_m1) + (sv_minor_m1) + 8u) >> 4u);
    *dp++ = (uint8_t)((sv + (3u * sv_major_p1) + (sv_minor_p1) + 7u) >> 4u);
    if (src_len <= 0u) {
      return dst_ptr;
    }
  }

  if (last_column) {
    src_len--;
  }

  if (src_len < 32) {
    // This fallback is the same as the non-SIMD-capable code path.
    for (; src_len > 0u; src_len--) {
      uint32_t sv_major_m1 = sp_major[-1];
      uint32_t sv_minor_m1 = sp_minor[-1];
      uint32_t sv_major_p1 = sp_major[+1];
      uint32_t sv_minor_p1 = sp_minor[+1];

      uint32_t sv = (9u * ((uint32_t)(*sp_major++))) +  //
                    (3u * ((uint32_t)(*sp_minor++)));
      *dp++ = (uint8_t)((sv + (3u * sv_major_m1) + (sv_minor_m1) + 8u) >> 4u);
      *dp++ = (uint8_t)((sv + (3u * sv_major_p1) + (sv_minor_p1) + 7u) >> 4u);
    }

  } else {
    while (src_len > 0u) {
      // Load 1+32+1 samples (six u8x32 vectors) from the major (jxx) and minor
      // (nxx) rows.
      //
      // major_p0 = [j00 j01 j02 j03 .. j28 j29 j30 j31]   // p0 = "plus  0"
      // minor_p0 = [n00 n01 n02 n03 .. n28 n29 n30 n31]   // p0 = "plus  0"
      // major_m1 = [jm1 j00 j01 j02 .. j27 j28 j29 j30]   // m1 = "minus 1"
      // minor_m1 = [nm1 n00 n01 n02 .. n27 n28 n29 n30]   // m1 = "minus 1"
      // major_p1 = [j01 j02 j03 j04 .. j29 j30 j31 j32]   // p1 = "plus  1"
      // minor_p1 = [n01 n02 n03 n04 .. n29 n30 n31 n32]   // p1 = "plus  1"
      __m256i major_p0 =
          _mm256_lddqu_si256((const __m256i*)(const void*)(sp_major + 0));
      __m256i minor_p0 =
          _mm256_lddqu_si256((const __m256i*)(const void*)(sp_minor + 0));
      __m256i major_m1 =
          _mm256_lddqu_si256((const __m256i*)(const void*)(sp_major - 1));
      __m256i minor_m1 =
          _mm256_lddqu_si256((const __m256i*)(const void*)(sp_minor - 1));
      __m256i major_p1 =
          _mm256_lddqu_si256((const __m256i*)(const void*)(sp_major + 1));
      __m256i minor_p1 =
          _mm256_lddqu_si256((const __m256i*)(const void*)(sp_minor + 1));

      // Unpack, staying with u8x32 vectors.
      //
      // step1_p0_lo = [j00 n00 j01 n01 .. j07 n07  j16 n16 j17 n17 .. j23 n23]
      // step1_p0_hi = [j08 n08 j09 n09 .. j15 n15  j24 n24 j25 n25 .. j31 n31]
      // step1_m1_lo = [jm1 nm1 j00 n00 .. j06 n06  j15 n15 j16 n16 .. j22 n22]
      // step1_m1_hi = [j07 n07 j08 n08 .. j14 n14  j23 n23 j24 n24 .. j30 n30]
      // step1_p1_lo = [j01 n01 j02 n02 .. j08 n08  j17 n17 j18 n18 .. j24 n24]
      // step1_p1_hi = [j09 n09 j10 n10 .. j16 n16  j25 n25 j26 n26 .. j32 n32]
      __m256i step1_p0_lo = _mm256_unpacklo_epi8(major_p0, minor_p0);
      __m256i step1_p0_hi = _mm256_unpackhi_epi8(major_p0, minor_p0);
      __m256i step1_m1_lo = _mm256_unpacklo_epi8(major_m1, minor_m1);
      __m256i step1_m1_hi = _mm256_unpackhi_epi8(major_m1, minor_m1);
      __m256i step1_p1_lo = _mm256_unpacklo_epi8(major_p1, minor_p1);
      __m256i step1_p1_hi = _mm256_unpackhi_epi8(major_p1, minor_p1);

      // Multiply-add to get u16x16 vectors.
      //
      // step2_p0_lo = [9*j00+3*n00 9*j01+3*n01 .. 9*j23+3*n23]
      // step2_p0_hi = [9*j08+3*n08 9*j09+3*n09 .. 9*j31+3*n31]
      // step2_m1_lo = [3*jm1+1*nm1 3*j00+1*n00 .. 3*j22+1*n22]
      // step2_m1_hi = [3*j07+1*n07 3*j08+1*n08 .. 3*j30+1*n30]
      // step2_p1_lo = [3*j01+1*n01 3*j02+1*n02 .. 3*j24+1*n24]
      // step2_p1_hi = [3*j09+1*n09 3*j10+1*n10 .. 3*j32+1*n32]
      const __m256i k0309 = _mm256_set1_epi16(0x0309);
      const __m256i k0103 = _mm256_set1_epi16(0x0103);
      __m256i step2_p0_lo = _mm256_maddubs_epi16(step1_p0_lo, k0309);
      __m256i step2_p0_hi = _mm256_maddubs_epi16(step1_p0_hi, k0309);
      __m256i step2_m1_lo = _mm256_maddubs_epi16(step1_m1_lo, k0103);
      __m256i step2_m1_hi = _mm256_maddubs_epi16(step1_m1_hi, k0103);
      __m256i step2_p1_lo = _mm256_maddubs_epi16(step1_p1_lo, k0103);
      __m256i step2_p1_hi = _mm256_maddubs_epi16(step1_p1_hi, k0103);

      // Compute the weighted sums of (p0, m1) and (p0, p1). For example:
      //
      // step3_m1_lo[00] = ((9*j00) + (3*n00) + (3*jm1) + (1*nm1)) as u16
      // step3_p1_hi[15] = ((9*j31) + (3*n31) + (3*j32) + (1*n32)) as u16
      __m256i step3_m1_lo = _mm256_add_epi16(step2_p0_lo, step2_m1_lo);
      __m256i step3_m1_hi = _mm256_add_epi16(step2_p0_hi, step2_m1_hi);
      __m256i step3_p1_lo = _mm256_add_epi16(step2_p0_lo, step2_p1_lo);
      __m256i step3_p1_hi = _mm256_add_epi16(step2_p0_hi, step2_p1_hi);

      // Bias by 8 (on the left) or 7 (on the right) and then divide by 16
      // (which is 9+3+3+1) to get a weighted average. On the left (m1), shift
      // the u16 right value by 4. On the right (p1), shift right by 4 and then
      // shift left by 8 so that, when still in the u16x16 little-endian
      // interpretation, we have:
      //  - m1_element =  (etcetera + 8) >> 4
      //  - p1_element = ((etcetera + 7) >> 4) << 8
      //
      // step4_m1_lo = [0x00?? 0x00?? ... 0x00?? 0x00??]
      // step4_p1_lo = [0x??00 0x??00 ... 0x??00 0x??00]
      // step4_m1_hi = [0x00?? 0x00?? ... 0x00?? 0x00??]
      // step4_p1_hi = [0x??00 0x??00 ... 0x??00 0x??00]
      __m256i step4_m1_lo = _mm256_srli_epi16(
          _mm256_add_epi16(step3_m1_lo, _mm256_set1_epi16(8)), 4);
      __m256i step4_p1_lo = _mm256_slli_epi16(
          _mm256_srli_epi16(_mm256_add_epi16(step3_p1_lo, _mm256_set1_epi16(7)),
                            4),
          8);
      __m256i step4_m1_hi = _mm256_srli_epi16(
          _mm256_add_epi16(step3_m1_hi, _mm256_set1_epi16(8)), 4);
      __m256i step4_p1_hi = _mm256_slli_epi16(
          _mm256_srli_epi16(_mm256_add_epi16(step3_p1_hi, _mm256_set1_epi16(7)),
                            4),
          8);

      // Bitwise-or two "0x00"-rich u16x16 vectors to get a u8x32 vector. Do
      // that twice. Once for the low columns and once for the high columns.
      //
      // In terms of jxx (major row) or nxx (minor row) source samples:
      //  - low  columns means ( 0 ..  8; 16 .. 24).
      //  - high columns means ( 8 .. 16; 24 .. 32).
      //
      // In terms of dxx destination samples (there are twice as many):
      //  - low  columns means ( 0 .. 16; 32 .. 48).
      //  - high columns means (16 .. 32; 48 .. 64).
      //
      // step5_lo = [d00 d01 .. d14 d15  d32 d33 .. d46 d47]
      // step5_hi = [d16 d17 .. d30 d31  d48 d49 .. d62 d63]
      //
      // The d00, d02 ... d62 even elements come from (p0, m1) weighted sums.
      // The d01, d03 ... d63 odd  elements come from (p0, p1) weighted sums.
      __m256i step5_lo = _mm256_or_si256(step4_m1_lo, step4_p1_lo);
      __m256i step5_hi = _mm256_or_si256(step4_m1_hi, step4_p1_hi);

      // Permute and store.
      //
      // step6_00_31 = [d00 d01 .. d14 d15  d16 d17 .. d30 d31]
      // step6_32_63 = [d32 d33 .. d46 d47  d48 d49 .. d62 d63]
      __m256i step6_00_31 = _mm256_permute2x128_si256(step5_lo, step5_hi, 0x20);
      __m256i step6_32_63 = _mm256_permute2x128_si256(step5_lo, step5_hi, 0x31);
      _mm256_storeu_si256((__m256i*)(void*)(dp + 0x00), step6_00_31);
      _mm256_storeu_si256((__m256i*)(void*)(dp + 0x20), step6_32_63);

      // Advance by up to 32 source samples (64 destination samples). The first
      // iteration might be smaller than 32 so that all of the remaining steps
      // are exactly 32.
      size_t n = 32u - (31u & (0u - src_len));
      dp += 2u * n;
      sp_major += n;
      sp_minor += n;
      src_len -= n;
    }
  }

  if (last_column) {
    uint32_t sv_major_m1 = sp_major[-1];
    uint32_t sv_minor_m1 = sp_minor[-1];
    uint32_t sv_major_p1 = sp_major[+0];  // Clamp offset to zero.
    uint32_t sv_minor_p1 = sp_minor[+0];  // Clamp offset to zero.

    uint32_t sv = (9u * ((uint32_t)(*sp_major++))) +  //
                  (3u * ((uint32_t)(*sp_minor++)));
    *dp++ = (uint8_t)((sv + (3u * sv_major_m1) + (sv_minor_m1) + 8u) >> 4u);
    *dp++ = (uint8_t)((sv + (3u * sv_major_p1) + (sv_minor_p1) + 7u) >> 4u);
  }

  return dst_ptr;
}
#endif
#endif  // defined(WUFFS_PRIVATE_IMPL__CPU_ARCH__X86_64_V3)
// ‼ WUFFS MULTI-FILE SECTION -x86_avx2
