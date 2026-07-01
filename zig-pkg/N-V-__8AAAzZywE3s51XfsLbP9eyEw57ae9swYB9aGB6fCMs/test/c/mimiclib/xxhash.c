// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "xxhash.h"

uint32_t global_mimiclib_xxhash32_unused_u32;
uint64_t global_mimiclib_xxhash64_unused_u64;

const char*  //
mimic_bench_xxhash32(wuffs_base__io_buffer* dst,
                     wuffs_base__io_buffer* src,
                     uint32_t wuffs_initialize_flags,
                     uint64_t wlimit,
                     uint64_t rlimit) {
  XXH32_state_t* hasher = XXH32_createState();
  if (!hasher) {
    return "libxxhash: XXH32_createState failed";
  } else if (XXH_OK != XXH32_reset(hasher, 0)) {
    return "libxxhash: XXH32_reset failed";
  }

  global_mimiclib_xxhash32_unused_u32 = 0;
  while (src->meta.ri < src->meta.wi) {
    uint8_t* ptr = src->data.ptr + src->meta.ri;
    size_t len = src->meta.wi - src->meta.ri;
    if (len > 0x7FFFFFFF) {
      return "src length is too large";
    } else if (len > rlimit) {
      len = rlimit;
    }
    if (XXH_OK != XXH32_update(hasher, ptr, len)) {
      return "libxxhash: XXH32_update failed";
    }
    src->meta.ri += len;
  }
  global_mimiclib_xxhash32_unused_u32 = XXH32_digest(hasher);

  if (XXH_OK != XXH32_freeState(hasher)) {
    return "libxxhash: XXH32_freeState failed";
  }
  return NULL;
}

const char*  //
mimic_bench_xxhash64(wuffs_base__io_buffer* dst,
                     wuffs_base__io_buffer* src,
                     uint32_t wuffs_initialize_flags,
                     uint64_t wlimit,
                     uint64_t rlimit) {
  XXH64_state_t* hasher = XXH64_createState();
  if (!hasher) {
    return "libxxhash: XXH64_createState failed";
  } else if (XXH_OK != XXH64_reset(hasher, 0)) {
    return "libxxhash: XXH64_reset failed";
  }

  global_mimiclib_xxhash64_unused_u64 = 0;
  while (src->meta.ri < src->meta.wi) {
    uint8_t* ptr = src->data.ptr + src->meta.ri;
    size_t len = src->meta.wi - src->meta.ri;
    if (len > 0x7FFFFFFF) {
      return "src length is too large";
    } else if (len > rlimit) {
      len = rlimit;
    }
    if (XXH_OK != XXH64_update(hasher, ptr, len)) {
      return "libxxhash: XXH64_update failed";
    }
    src->meta.ri += len;
  }
  global_mimiclib_xxhash64_unused_u64 = XXH64_digest(hasher);

  if (XXH_OK != XXH64_freeState(hasher)) {
    return "libxxhash: XXH64_freeState failed";
  }
  return NULL;
}

uint32_t  //
mimic_xxhash32_one_shot_checksum_u32(wuffs_base__slice_u8 data) {
  return XXH32(data.ptr, data.len, 0);
}

uint64_t  //
mimic_xxhash64_one_shot_checksum_u64(wuffs_base__slice_u8 data) {
  return XXH64(data.ptr, data.len, 0);
}
