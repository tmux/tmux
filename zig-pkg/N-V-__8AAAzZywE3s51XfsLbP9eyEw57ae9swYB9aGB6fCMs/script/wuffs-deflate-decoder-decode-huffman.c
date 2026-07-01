// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// This file contains a hand-written C implementation of
// release/c/wuffs-unsupported-snapshot.c's generated
// wuffs_deflate__decoder__decode_huffman_fast function.
//
// It is not intended to be used in production settings, on untrusted data. Its
// purpose is to give a rough upper bound on how fast Wuffs' generated C code
// can be, with a sufficiently smart Wuffs compiler.
//
// To repeat, substituting in this C implementation is NOT SAFE, and may result
// in buffer overflows. This code exists only to aid optimization of the (safe)
// std/deflate/*.wuffs code and the Wuffs compiler's code generation.

// ----------------

// Having said that, to generate the benchmark numbers with this hand-written C
// implementation, edit release/c/wuffs-unsupported-snapshot.c and find the
// lines that say
//
// // ‼ WUFFS C HEADER ENDS HERE.
// #ifdef WUFFS_IMPLEMENTATION
//
// After that, add this line:
//
// #include "../../script/wuffs-deflate-decoder-decode-huffman.c"
//
// Then find the calls to wuffs_deflate__decoder__decode_huffman_fastxx. They
// should be inside the wuffs_deflate__decoder__decode_blocks function body,
// and the lines of code should look something like
//   v_status = wuffs_deflate__decoder__decode_huffman_fast32(etc);
// and
//   v_status = wuffs_deflate__decoder__decode_huffman_fast64(etc);
//
// Change the "wuffs_etc_fastxx" to "c_wuffs_etc_fast", i.e. add a "c_" prefix
// and drop the "32" or "64" suffix. The net result should look something like:
//
// clang-format off
/*

$ git diff release/c/wuffs-unsupported-snapshot.c
diff --git a/release/c/wuffs-unsupported-snapshot.c b/release/c/wuffs-unsupported-snapshot.c
index d255a788..2a112728 100644
--- a/release/c/wuffs-unsupported-snapshot.c
+++ b/release/c/wuffs-unsupported-snapshot.c
@@ -9758,6 +9758,8 @@ DecodeJson(DecodeJsonCallbacks& callbacks,
 // ‼ WUFFS C HEADER ENDS HERE.
 #ifdef WUFFS_IMPLEMENTATION

+#include "../../script/wuffs-deflate-decoder-decode-huffman.c"
+
 #ifdef __cplusplus
 extern "C" {
 #endif
@@ -25326,7 +25328,7 @@ wuffs_deflate__decoder__decode_blocks(
           if (a_src) {
             a_src->meta.ri = ((size_t)(iop_a_src - a_src->data.ptr));
           }
-          v_status = wuffs_deflate__decoder__decode_huffman_fast32(self, a_dst, a_src);
+          v_status = c_wuffs_deflate__decoder__decode_huffman_fast(self, a_dst, a_src);
           if (a_src) {
             iop_a_src = a_src->data.ptr + a_src->meta.ri;
           }
@@ -25334,7 +25336,7 @@ wuffs_deflate__decoder__decode_blocks(
           if (a_src) {
             a_src->meta.ri = ((size_t)(iop_a_src - a_src->data.ptr));
           }
-          v_status = wuffs_deflate__decoder__decode_huffman_fast64(self, a_dst, a_src);
+          v_status = c_wuffs_deflate__decoder__decode_huffman_fast(self, a_dst, a_src);
           if (a_src) {
             iop_a_src = a_src->data.ptr + a_src->meta.ri;
           }

*/
// clang-format on
//
// That concludes the edits to release/c/wuffs-unsupported-snapshot.c. Run the
// tests and benchmarks with the "-skipgen" flag, otherwise the "wuffs" tool
// will re-generate the C code and override your edits:
//
// wuffs test  -skipgen std/deflate
// wuffs bench -skipgen std/deflate
//
// You may also want to focus on one specific test, e.g.:
//
// wuffs test  -skipgen -focus=wuffs_deflate_decode_midsummer std/deflate
// wuffs bench -skipgen -focus=wuffs_deflate_decode_100k_just std/deflate

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>  // For manual printf debugging.
#include <string.h>

extern const char wuffs_deflate__error__internal_error_inconsistent_distance[];
extern const char
    wuffs_deflate__error__internal_error_inconsistent_huffman_decoder_state[];

// ----------------

// Define this macro to make this file compilable standalone, instead of
// hooking it into release/c/wuffs-unsupported-snapshot.c per the instructions
// above. Standalone means that you can run a "does it compile" check by
//
// gcc -c script/wuffs-deflate-decoder-decode-huffman.c -o /dev/null
//
// and also drop this file into disassembly tools like https://godbolt.org/

//#define MAKE_THIS_FILE_COMPILABLE_STANDALONE

#ifdef MAKE_THIS_FILE_COMPILABLE_STANDALONE

typedef struct {
  const char* repr;
} wuffs_base__status;

static inline wuffs_base__status  //
wuffs_base__make_status(const char* repr) {
  wuffs_base__status z;
  z.repr = repr;
  return z;
}

typedef struct {
  uint8_t* ptr;
  size_t len;
} wuffs_base__slice_u8;
typedef struct {
  size_t wi;
  size_t ri;
  uint64_t pos;
  bool closed;
} wuffs_base__io_buffer_meta;
typedef struct {
  wuffs_base__slice_u8 data;
  wuffs_base__io_buffer_meta meta;
} wuffs_base__io_buffer;
typedef struct wuffs_deflate__decoder__struct {
  struct {
    uint32_t magic;
    uint32_t f_bits;
    uint32_t f_n_bits;
    uint32_t f_history_index;
    uint32_t f_n_huffs_bits[2];
    bool f_end_of_block;
  } private_impl;
  struct {
    // 1024 is huffs_table_size in std/deflate/decode_deflate.wuffs.
    uint32_t f_huffs[2][1024];
    uint8_t f_history[32768];
    uint8_t f_code_lengths[320];
  } private_data;
} wuffs_deflate__decoder;

const char wuffs_base__error__bad_argument[] = "?base: bad argument";
const char wuffs_deflate__error__bad_distance[] = "?deflate: bad distance";
const char wuffs_deflate__error__internal_error_inconsistent_distance[] =
    "?deflate: internal error: inconsistent distance";
const char
    wuffs_deflate__error__internal_error_inconsistent_huffman_decoder_state[] =
        "?deflate: internal error: inconsistent Huffman decoder state";

static wuffs_base__status  //
wuffs_deflate__decoder__decode_huffman_fast(wuffs_deflate__decoder* self,
                                            wuffs_base__io_buffer* a_dst,
                                            wuffs_base__io_buffer* a_src) {
  return wuffs_base__make_status(NULL);
}

#endif  // MAKE_THIS_FILE_COMPILABLE_STANDALONE

// ----------------

// Define this macro to further speed up this C implementation, using two
// techniques that are not used by the zlib-the-library C implementation (as of
// version 1.2.11, current as of November 2017). Doing so gives data for "how
// fast can Wuffs' zlib-the-format implementation be" instead of "is Wuffs'
// generated C code as fast as zlib-the-library's hand-written C code".
//
// Whether the same techniques could apply to zlib-the-library is discussed at
// https://github.com/madler/zlib/pull/292
#ifdef __x86_64__
//#define WUFFS_DEFLATE__HAVE_64_BIT_UNALIGNED_LITTLE_ENDIAN_LOADS
#endif

// wuffs_base__width_to_mask_table is the width_to_mask_table from
// https://fgiesen.wordpress.com/2018/02/19/reading-bits-in-far-too-many-ways-part-1/
//
// Look for "It may feel ridiculous" on that page for the rationale.
static const uint32_t wuffs_base__width_to_mask_table[33] = {
    0x00000000u,

    0x00000001u, 0x00000003u, 0x00000007u, 0x0000000Fu,
    0x0000001Fu, 0x0000003Fu, 0x0000007Fu, 0x000000FFu,

    0x000001FFu, 0x000003FFu, 0x000007FFu, 0x00000FFFu,
    0x00001FFFu, 0x00003FFFu, 0x00007FFFu, 0x0000FFFFu,

    0x0001FFFFu, 0x0003FFFFu, 0x0007FFFFu, 0x000FFFFFu,
    0x001FFFFFu, 0x003FFFFFu, 0x007FFFFFu, 0x00FFFFFFu,

    0x01FFFFFFu, 0x03FFFFFFu, 0x07FFFFFFu, 0x0FFFFFFFu,
    0x1FFFFFFFu, 0x3FFFFFFFu, 0x7FFFFFFFu, 0xFFFFFFFFu,
};

// Uncomment exactly one of these. Choosing between them lets us measure the
// effect of the table-based mask.
//#define MASK(n) ((((uint32_t)1) << (n)) - 1)
#define MASK(n) wuffs_base__width_to_mask_table[n]

// ----------------

// These are the generated functions that we are explicitly overriding. Note
// that the function name is "wuffs_etc", not "c_wuffs_etc".
static wuffs_base__status  //
wuffs_deflate__decoder__decode_huffman_fast32(wuffs_deflate__decoder* self,
                                              wuffs_base__io_buffer* a_dst,
                                              wuffs_base__io_buffer* a_src);
static wuffs_base__status  //
wuffs_deflate__decoder__decode_huffman_fast64(wuffs_deflate__decoder* self,
                                              wuffs_base__io_buffer* a_dst,
                                              wuffs_base__io_buffer* a_src);

// This is the overriding implementation.
wuffs_base__status  //
c_wuffs_deflate__decoder__decode_huffman_fast(wuffs_deflate__decoder* self,
                                              wuffs_base__io_buffer* a_dst,
                                              wuffs_base__io_buffer* a_src) {
  // Avoid the -Werror=unused-function warning for the now-unused
  // overridden wuffs_deflate__decoder__decode_huffman_fastxx functions.
  (void)(wuffs_deflate__decoder__decode_huffman_fast32);
  (void)(wuffs_deflate__decoder__decode_huffman_fast64);

  if (!a_dst || !a_src) {
    return wuffs_base__make_status(wuffs_base__error__bad_argument);
  }
  wuffs_base__status status = wuffs_base__make_status(NULL);

  // Load contextual state. Prepare to check that pdst and psrc remain within
  // a_dst's and a_src's bounds.

  uint8_t* pdst = a_dst->data.ptr + a_dst->meta.wi;
  uint8_t* qdst = a_dst->data.ptr + a_dst->data.len;
  if ((qdst - pdst) < 258) {
    return wuffs_base__make_status(NULL);
  } else {
    qdst -= 258;
  }

  uint8_t* psrc = a_src->data.ptr + a_src->meta.ri;
  uint8_t* qsrc = a_src->data.ptr + a_src->meta.wi;
  if ((qsrc - psrc) < 12) {
    return wuffs_base__make_status(NULL);
  } else {
    qsrc -= 12;
  }

#if defined(WUFFS_DEFLATE__HAVE_64_BIT_UNALIGNED_LITTLE_ENDIAN_LOADS)
  uint64_t bits = self->private_impl.f_bits;
#else
  uint32_t bits = self->private_impl.f_bits;
#endif
  uint32_t n_bits = self->private_impl.f_n_bits;

  // Initialize other local variables.
  uint8_t* pdst_mark = a_dst->data.ptr;
  uint32_t lmask = MASK(self->private_impl.f_n_huffs_bits[0]);
  uint32_t dmask = MASK(self->private_impl.f_n_huffs_bits[1]);

outer_loop:
  while ((pdst <= qdst) && (psrc <= qsrc)) {
#if defined(WUFFS_DEFLATE__HAVE_64_BIT_UNALIGNED_LITTLE_ENDIAN_LOADS)
    // Ensure that we have at least 56 bits of input.
    //
    // This is "Variant 4" of
    // https://fgiesen.wordpress.com/2018/02/20/reading-bits-in-far-too-many-ways-part-2/
    //
    // 56, the number of bits in 7 bytes, is a property of that "Variant 4"
    // bit-reading technique, and not related to the DEFLATE format per se.
    //
    // Specifically for DEFLATE, we need only up to 48 bits per outer_loop
    // iteration. The maximum input bits used by a length/distance pair is 15
    // bits for the length code, 5 bits for the length extra, 15 bits for the
    // distance code, and 13 bits for the distance extra. This totals 48 bits.
    //
    // The fact that the 48 we need is less than the 56 we get is a happy
    // coincidence. It lets us eliminate any other loads in the loop body.
    uint64_t src_u64;
    memcpy(&src_u64, psrc, 8);
    bits |= src_u64 << n_bits;
    psrc += (63 - n_bits) >> 3;
    n_bits |= 56;
#else
    // Ensure that we have at least 15 bits of input.
    if (n_bits < 15) {
      bits |= ((uint32_t)(*psrc++)) << n_bits;
      n_bits += 8;
      bits |= ((uint32_t)(*psrc++)) << n_bits;
      n_bits += 8;
    }
#endif

    // Decode an lcode symbol from H-L.
    uint32_t table_entry = self->private_data.f_huffs[0][bits & lmask];
    while (true) {
      uint32_t n = table_entry & 0x0F;
      bits >>= n;
      n_bits -= n;
      if ((table_entry >> 31) != 0) {
        // Literal.
        *pdst++ = (uint8_t)(table_entry >> 8);
        goto outer_loop;
      }
      if ((table_entry >> 30) != 0) {
        // Back-reference; length = base_number + extra_bits.
        break;
      }
      if ((table_entry >> 29) != 0) {
        // End of block.
        self->private_impl.f_end_of_block = true;
        goto end;
      }
      if ((table_entry >> 24) != 0x10) {
        status = wuffs_base__make_status(
            wuffs_deflate__error__internal_error_inconsistent_huffman_decoder_state);
        goto end;
      }
      uint32_t top = (table_entry >> 8) & 0xFFFF;
      uint32_t mask = MASK((table_entry >> 4) & 0x0F);
      table_entry = self->private_data.f_huffs[0][top + (bits & mask)];
    }

    // length = base_number_minus_3 + 3 + extra_bits.
    uint32_t length = ((table_entry >> 8) & 0xFF) + 3;
    {
      uint32_t n = (table_entry >> 4) & 0x0F;
      if (n) {
#if !defined(WUFFS_DEFLATE__HAVE_64_BIT_UNALIGNED_LITTLE_ENDIAN_LOADS)
        if (n_bits < n) {
          bits |= ((uint32_t)(*psrc++)) << n_bits;
          n_bits += 8;
        }
#endif
        length += bits & MASK(n);
        bits >>= n;
        n_bits -= n;
      }
    }

#if !defined(WUFFS_DEFLATE__HAVE_64_BIT_UNALIGNED_LITTLE_ENDIAN_LOADS)
    // Ensure that we have at least 15 bits of input.
    if (n_bits < 15) {
      bits |= ((uint32_t)(*psrc++)) << n_bits;
      n_bits += 8;
      bits |= ((uint32_t)(*psrc++)) << n_bits;
      n_bits += 8;
    }
#endif

    // Decode a dcode symbol from H-D.
    table_entry = self->private_data.f_huffs[1][bits & dmask];
    while (true) {
      uint32_t n = table_entry & 0x0F;
      bits >>= n;
      n_bits -= n;
      if ((table_entry >> 30) != 0) {
        // Back-reference; distance = base_number + extra_bits.
        break;
      }
      if ((table_entry >> 24) != 0x10) {
        status = wuffs_base__make_status(
            wuffs_deflate__error__internal_error_inconsistent_huffman_decoder_state);
        goto end;
      }
      uint32_t top = (table_entry >> 8) & 0xFFFF;
      uint32_t mask = MASK((table_entry >> 4) & 0x0F);
      table_entry = self->private_data.f_huffs[1][top + (bits & mask)];
    }

    // dist_minus_1 = base_number_minus_1 + extra_bits.
    // distance     = dist_minus_1 + 1.
    uint32_t dist_minus_1 = (table_entry >> 8) & 0xFFFF;
    {
      uint32_t n = (table_entry >> 4) & 0x0F;
#if !defined(WUFFS_DEFLATE__HAVE_64_BIT_UNALIGNED_LITTLE_ENDIAN_LOADS)
      // Ensure that we have at least n bits of input.
      if (n_bits < n) {
        bits |= ((uint32_t)(*psrc++)) << n_bits;
        n_bits += 8;
        bits |= ((uint32_t)(*psrc++)) << n_bits;
        n_bits += 8;
      }
#endif
      dist_minus_1 += bits & MASK(n);
      bits >>= n;
      n_bits -= n;
    }

    // Copy from the history buffer, if necessary.
    if ((ptrdiff_t)(dist_minus_1 + 1) > (pdst - pdst_mark)) {
      // Set (hlen, hdist) to be the length-distance pair to copy from
      // this.history, and (length, distance) to be the remaining
      // length-distance pair to copy from args.dst.
      uint32_t hlen = 0;
      uint32_t hdist = ((dist_minus_1 + 1) - (pdst - pdst_mark));
      if (length > hdist) {
        length -= hdist;
        hlen = hdist;
      } else {
        hlen = length;
        length = 0;
      }
      if (self->private_impl.f_history_index < hdist) {
        status = wuffs_base__make_status(wuffs_deflate__error__bad_distance);
        goto end;
      }

      // Copy up to the right edge of the f_history array.
      uint32_t history_index =
          (self->private_impl.f_history_index - hdist) & 0x7FFF;
      uint32_t available = 0x8000 - history_index;
      uint32_t n_copied = (hlen < available) ? hlen : available;
      memmove(pdst, self->private_data.f_history + history_index, n_copied);
      pdst += n_copied;

      // Copy from the left edge of the f_history array.
      if (hlen > n_copied) {
        hlen -= n_copied;
        memmove(pdst, self->private_data.f_history, hlen);
        pdst += hlen;
      }

      if (length == 0) {
        goto outer_loop;
      }

      if ((ptrdiff_t)(dist_minus_1 + 1) > (pdst - pdst_mark)) {
        status = wuffs_base__make_status(
            wuffs_deflate__error__internal_error_inconsistent_distance);
        goto end;
      }
    }

    uint8_t* pback = pdst - (dist_minus_1 + 1);

#if defined(WUFFS_DEFLATE__HAVE_64_BIT_UNALIGNED_LITTLE_ENDIAN_LOADS)
    // Back-copy fast path, copying 8 instead of 1 bytes at a time.
    //
    // This always copies 8*N bytes (where N is the smallest integer such that
    // 8*N >= length, i.e. we round length up to a multiple of 8), instead of
    // only length bytes, but that's OK, as subsequent iterations will fix up
    // the overrun.
    if (dist_minus_1 + 1 >= 8) {
      while (1) {
        memcpy(pdst, pback, 8);
        if (length <= 8) {
          pdst += length;
          break;
        }
        pdst += 8;
        pback += 8;
        length -= 8;
      }
      continue;
    }
#endif

    // Back-copy slow path.
    for (; length >= 3; length -= 3) {
      *pdst++ = *pback++;
      *pdst++ = *pback++;
      *pdst++ = *pback++;
    }
    for (; length; length--) {
      *pdst++ = *pback++;
    }
  }

end:
  // Return unused input bytes.
  for (; n_bits >= 8; n_bits -= 8) {
    psrc--;
  }
  bits &= MASK(n_bits);

  // Save contextual state.
  a_dst->meta.wi = pdst - a_dst->data.ptr;
  a_src->meta.ri = psrc - a_src->data.ptr;
  self->private_impl.f_bits = bits;
  self->private_impl.f_n_bits = n_bits;

  return status;
}
