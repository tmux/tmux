// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// Silence the nested slash-star warning for the next comment's command line.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomment"

/*
This fuzzer (the fuzz function) is typically run indirectly, by a framework
such as https://github.com/google/oss-fuzz calling LLVMFuzzerTestOneInput.

When working on the fuzz implementation, or as a coherence check, defining
WUFFS_CONFIG__FUZZLIB_MAIN will let you manually run fuzz over a set of files:

gcc -DWUFFS_CONFIG__FUZZLIB_MAIN cbor_fuzzer.c
./a.out ../../../test/data/*.cbor
rm -f ./a.out

It should print "PASS", amongst other information, and exit(0).
*/

#pragma clang diagnostic pop

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

#if defined(WUFFS_CONFIG__FUZZLIB_MAIN)
// Defining the WUFFS_CONFIG__STATIC_FUNCTIONS macro is optional, but when
// combined with WUFFS_IMPLEMENTATION, it demonstrates making all of Wuffs'
// functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#endif  // defined(WUFFS_CONFIG__FUZZLIB_MAIN)

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CBOR

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../fuzzlib/fuzzlib.c"

#define TOK_BUFFER_ARRAY_SIZE 4096
#define STACK_SIZE (WUFFS_CBOR__DECODER_DEPTH_MAX_INCL + 1)

// Wuffs allows either statically or dynamically allocated work buffers. This
// program exercises static allocation.
#define WORK_BUFFER_ARRAY_SIZE \
  WUFFS_CBOR__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
#if WORK_BUFFER_ARRAY_SIZE > 0
uint8_t g_work_buffer_array[WORK_BUFFER_ARRAY_SIZE];
#else
// Not all C/C++ compilers support 0-length arrays.
uint8_t g_work_buffer_array[1];
#endif

// Each stack element is 1 byte. The low 7 bits denote the container:
//  - 0x01 means no container: we are at the top level.
//  - 0x02 means a [] list.
//  - 0x04 means a {} dictionary.
//
// The high 0x80 bit holds the even/odd-ness of the number of elements in that
// container. A valid dictionary contains key-value pairs and should therefore
// contain an even number of elements.
typedef uint8_t stack_element;

bool  //
token_is_cbor_tag(wuffs_base__token t) {
  return (wuffs_base__token__value_major(&t) ==
          WUFFS_CBOR__TOKEN_VALUE_MAJOR) &&
         (wuffs_base__token__value_minor(&t) &
          WUFFS_CBOR__TOKEN_VALUE_MINOR__TAG);
}

const char*  //
fuzz_one_token(wuffs_base__token t,
               wuffs_base__token prev_token,
               wuffs_base__io_buffer* src,
               size_t* ti,
               stack_element* stack,
               size_t* depth) {
  uint64_t len = wuffs_base__token__length(&t);
  if (len > 0xFFFF) {
    return "fuzz: internal error: length too long (vs 0xFFFF)";
  } else if (len > (src->meta.wi - *ti)) {
    return "fuzz: internal error: length too long (vs wi - ti)";
  }
  *ti += len;

  bool is_cbor_tag = token_is_cbor_tag(t);
  if (wuffs_base__token__value_extension(&t) >= 0) {
    if (!wuffs_base__token__continued(&prev_token)) {
      return "fuzz: internal error: extended token not after continued token";
    }
    is_cbor_tag = token_is_cbor_tag(prev_token);
  }

  int64_t vbc = wuffs_base__token__value_base_category(&t);
  uint64_t vbd = wuffs_base__token__value_base_detail(&t);

  switch (vbc) {
    case WUFFS_BASE__TOKEN__VBC__STRUCTURE: {
      bool from_consistent = false;
      if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__FROM_NONE) {
        from_consistent = stack[*depth] & 0x01;
      } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__FROM_LIST) {
        from_consistent = stack[*depth] & 0x02;
      } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__FROM_DICT) {
        from_consistent = stack[*depth] & 0x04;
      }
      if (!from_consistent) {
        return "fuzz: internal error: inconsistent VBD__STRUCTURE__FROM_ETC";
      }

      if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH) {
        (*depth)++;
        if ((*depth >= STACK_SIZE) || (*depth == 0)) {
          return "fuzz: internal error: depth too large";
        }

        if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_NONE) {
          return "fuzz: internal error: push to the 'none' container";
        } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST) {
          stack[*depth] = 0x02;
        } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_DICT) {
          stack[*depth] = 0x04;
        } else {
          return "fuzz: internal error: unrecognized VBD__STRUCTURE__TO_ETC";
        }

      } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__POP) {
        if ((vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__FROM_DICT) &&
            (0 != (0x80 & stack[*depth]))) {
          return "fuzz: internal error: dictionary had an incomplete key/value "
                 "pair";
        }

        if (*depth <= 0) {
          return "fuzz: internal error: depth too small";
        }
        (*depth)--;

        bool to_consistent = false;
        if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_NONE) {
          to_consistent = stack[*depth] & 0x01;
        } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST) {
          to_consistent = stack[*depth] & 0x02;
        } else if (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_DICT) {
          to_consistent = stack[*depth] & 0x04;
        }
        if (!to_consistent) {
          return "fuzz: internal error: inconsistent VBD__STRUCTURE__TO_ETC";
        }

      } else {
        return "fuzz: internal error: unrecognized VBC__STRUCTURE";
      }
      break;
    }

    case WUFFS_BASE__TOKEN__VBC__STRING: {
      if (vbd & WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_1_DST_1_SRC_COPY) {
        wuffs_base__slice_u8 s =
            wuffs_base__make_slice_u8(src->data.ptr + *ti - len, len);
        if ((vbd & WUFFS_BASE__TOKEN__VBD__STRING__DEFINITELY_UTF_8) &&
            (s.len != wuffs_base__utf_8__longest_valid_prefix(s.ptr, s.len))) {
          return "fuzz: internal error: invalid UTF-8";
        }
        if ((vbd & WUFFS_BASE__TOKEN__VBD__STRING__DEFINITELY_ASCII) &&
            (s.len != wuffs_base__ascii__longest_valid_prefix(s.ptr, s.len))) {
          return "fuzz: internal error: invalid ASCII";
        }
      }
      break;
    }

    case WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT: {
      if ((WUFFS_BASE__UNICODE_SURROGATE__MIN_INCL <= vbd) &&
          (vbd <= WUFFS_BASE__UNICODE_SURROGATE__MAX_INCL)) {
        return "fuzz: internal error: invalid Unicode surrogate";
      } else if (WUFFS_BASE__UNICODE_CODE_POINT__MAX_INCL < vbd) {
        return "fuzz: internal error: invalid Unicode code point";
      }
      break;
    }

    default:
      break;
  }

  // After a complete CBOR value, update the parity (even/odd count) of the
  // container.
  if (!wuffs_base__token__continued(&t) &&
      (vbc != WUFFS_BASE__TOKEN__VBC__FILLER) &&
      ((vbc != WUFFS_BASE__TOKEN__VBC__STRUCTURE) ||
       (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__POP)) &&
      !is_cbor_tag) {
    stack[*depth] ^= 0x80;
  }

  return NULL;
}

uint64_t  //
buffer_limit(uint64_t hash, uint64_t min, uint64_t max) {
  hash &= 0x3F;
  uint64_t n;
  if (hash < 0x20) {
    n = min + hash;
  } else {
    n = max - (0x3F - hash);
  }
  if (n < min) {
    return min;
  } else if (n > max) {
    return max;
  }
  return n;
}

const char*  //
fuzz_complex(wuffs_base__io_buffer* full_src, uint64_t hash) {
  uint64_t tok_limit = buffer_limit(
      hash & 0x3F, WUFFS_CBOR__DECODER_DST_TOKEN_BUFFER_LENGTH_MIN_INCL,
      TOK_BUFFER_ARRAY_SIZE);
  hash = wuffs_base__u64__rotate_right(hash, 6);

  uint64_t src_limit = buffer_limit(
      hash & 0x3F, WUFFS_CBOR__DECODER_SRC_IO_BUFFER_LENGTH_MIN_INCL, 4096);
  hash = wuffs_base__u64__rotate_right(hash, 6);

  // ----

  wuffs_cbor__decoder dec;
  wuffs_base__status status = wuffs_cbor__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (!wuffs_base__status__is_ok(&status)) {
    return wuffs_base__status__message(&status);
  }

  wuffs_base__token tok_array[TOK_BUFFER_ARRAY_SIZE];
  wuffs_base__token_buffer tok = ((wuffs_base__token_buffer){
      .data = ((wuffs_base__slice_token){
          .ptr = tok_array,
          .len = (size_t)((tok_limit < TOK_BUFFER_ARRAY_SIZE)
                              ? tok_limit
                              : TOK_BUFFER_ARRAY_SIZE),
      }),
  });

  wuffs_base__token prev_token = wuffs_base__make_token(0);
  uint32_t no_progress_count = 0;

  stack_element stack[STACK_SIZE];
  stack[0] = 0x01;  // We start in the 'none' container.
  size_t depth = 0;

  // ----

  while (true) {  // Outer loop.
    wuffs_base__io_buffer src = make_limited_reader(*full_src, src_limit);

    size_t old_tok_wi = tok.meta.wi;
    size_t old_tok_ri = tok.meta.ri;
    size_t old_src_wi = src.meta.wi;
    size_t old_src_ri = src.meta.ri;
    size_t ti = old_src_ri;

    status = wuffs_cbor__decoder__decode_tokens(
        &dec, &tok, &src,
        wuffs_base__make_slice_u8(g_work_buffer_array, WORK_BUFFER_ARRAY_SIZE));
    if ((tok.data.len < tok.meta.wi) ||  //
        (tok.meta.wi < tok.meta.ri) ||   //
        (tok.meta.ri != old_tok_ri)) {
      return "fuzz: internal error: inconsistent tok indexes";
    } else if ((src.data.len < src.meta.wi) ||  //
               (src.meta.wi < src.meta.ri) ||   //
               (src.meta.wi != old_src_wi)) {
      return "fuzz: internal error: inconsistent src indexes";
    }
    full_src->meta.ri += src.meta.ri - old_src_ri;

    if ((tok.meta.wi > old_tok_wi) || (src.meta.ri > old_src_ri) ||
        !wuffs_base__status__is_suspension(&status)) {
      no_progress_count = 0;
    } else if (no_progress_count < 999) {
      no_progress_count++;
    } else if (!full_src->meta.closed &&
               (status.repr == wuffs_base__suspension__short_read)) {
      return wuffs_base__status__message(&status);
    } else {
      return "fuzz: internal error: no progress";
    }

    // ----

    while (tok.meta.ri < tok.meta.wi) {  // Inner loop.
      wuffs_base__token t = tok.data.ptr[tok.meta.ri++];
      const char* z =
          fuzz_one_token(t, prev_token, &src, &ti, &stack[0], &depth);
      if (z != NULL) {
        return z;
      }
      prev_token = t;
    }  // Inner loop.

    // ----

    // Check that, starting from old_src_ri, summing the token lengths brings
    // us to the new src.meta.ri.
    if (ti != src.meta.ri) {
      return "fuzz: internal error: ti != ri";
    }

    if (status.repr == NULL) {
      break;

    } else if (status.repr == wuffs_base__suspension__short_read) {
      if (src.meta.closed) {
        return "fuzz: internal error: short read on a closed io_reader";
      }
      // We don't compact full_src as it may be mmap'ed read-only.
      continue;

    } else if (status.repr == wuffs_base__suspension__short_write) {
      wuffs_base__token_buffer__compact(&tok);
      continue;
    }

    return wuffs_base__status__message(&status);
  }  // Outer loop.

  // ----

  if (depth != 0) {
    return "fuzz: internal error: decoded OK but final depth was not zero";
  } else if (wuffs_base__token__continued(&prev_token)) {
    return "fuzz: internal error: decoded OK but final token was continued";
  }
  return NULL;
}

const char*  //
fuzz_simple(wuffs_base__io_buffer* full_src) {
  wuffs_cbor__decoder dec;
  wuffs_base__status status =
      wuffs_cbor__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION, 0);
  if (!wuffs_base__status__is_ok(&status)) {
    return wuffs_base__status__message(&status);
  }

  wuffs_base__token tok_array[TOK_BUFFER_ARRAY_SIZE];
  wuffs_base__token_buffer tok = ((wuffs_base__token_buffer){
      .data = ((wuffs_base__slice_token){
          .ptr = tok_array,
          .len = TOK_BUFFER_ARRAY_SIZE,
      }),
  });

  while (true) {
    status = wuffs_cbor__decoder__decode_tokens(
        &dec, &tok, full_src,
        wuffs_base__make_slice_u8(g_work_buffer_array, WORK_BUFFER_ARRAY_SIZE));
    if (status.repr == NULL) {
      break;

    } else if (status.repr == wuffs_base__suspension__short_write) {
      tok.meta.ri = tok.meta.wi;
      wuffs_base__token_buffer__compact(&tok);
      continue;
    }

    return wuffs_base__status__message(&status);
  }

  return NULL;
}

const char*  //
fuzz(wuffs_base__io_buffer* full_src, uint64_t hash) {
  // Send 99.6% of inputs to fuzz_complex and the remainder to fuzz_simple. The
  // 0xA5 constant is arbitrary but non-zero. If the hash function maps the
  // empty input to 0, this still sends the empty input to fuzz_complex.
  //
  // The fuzz_simple implementation shows how easy decoding with Wuffs is when
  // all you want is to run LLVMFuzzerTestOneInput's built-in (Wuffs API
  // independent) checks (e.g. the ASan address sanitizer) and you don't really
  // care what the output is, just that it doesn't crash.
  //
  // The fuzz_complex implementation adds many more Wuffs API specific checks
  // (e.g. that the sum of the tokens' lengths do not exceed the input length).
  if ((hash & 0xFF) != 0xA5) {
    return fuzz_complex(full_src, wuffs_base__u64__rotate_right(hash, 8));
  }
  return fuzz_simple(full_src);
}
