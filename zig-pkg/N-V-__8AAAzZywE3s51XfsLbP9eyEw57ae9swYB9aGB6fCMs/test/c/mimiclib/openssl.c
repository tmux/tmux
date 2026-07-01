// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "openssl/evp.h"

const char*  //
mimic_bench_sha256(wuffs_base__io_buffer* dst,
                   wuffs_base__io_buffer* src,
                   uint32_t wuffs_initialize_flags,
                   uint64_t wlimit,
                   uint64_t rlimit) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) {
    return "EVP_MD_CTX_new failed";
  } else if (!EVP_DigestInit_ex(ctx, EVP_sha256(), NULL)) {
    return "EVP_DigestInit_ex failed";
  }

  while (src->meta.ri < src->meta.wi) {
    uint8_t* ptr = src->data.ptr + src->meta.ri;
    size_t len = src->meta.wi - src->meta.ri;
    if (len > 0x7FFFFFFF) {
      return "src length is too large";
    } else if (len > rlimit) {
      len = rlimit;
    }
    if (!EVP_DigestUpdate(ctx, ptr, len)) {
      return "EVP_DigestUpdate failed";
    }
    src->meta.ri += len;
  }

  uint8_t results[EVP_MAX_MD_SIZE] = {0};
  if (!EVP_DigestFinal_ex(ctx, results, NULL)) {
    return "EVP_DigestFinal_ex failed";
  }
  return NULL;
}

wuffs_base__bitvec256  //
mimic_sha256_one_shot_checksum_bitvec256(wuffs_base__slice_u8 data) {
  uint8_t results[EVP_MAX_MD_SIZE] = {0};
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (ctx) {
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) &&  //
        EVP_DigestUpdate(ctx, data.ptr, data.len) &&   //
        EVP_DigestFinal_ex(ctx, results, NULL)) {
      // No-op.
    }
    EVP_MD_CTX_free(ctx);
  }

  return wuffs_base__make_bitvec256(
      wuffs_base__peek_u64be__no_bounds_check(results + 0x18),
      wuffs_base__peek_u64be__no_bounds_check(results + 0x10),
      wuffs_base__peek_u64be__no_bounds_check(results + 0x08),
      wuffs_base__peek_u64be__no_bounds_check(results + 0x00));
}
