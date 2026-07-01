// Copyright 2022 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "bzlib.h"

const char*  //
mimic_bzip2_decode(wuffs_base__io_buffer* dst,
                   wuffs_base__io_buffer* src,
                   uint32_t wuffs_initialize_flags,
                   uint64_t wlimit,
                   uint64_t rlimit) {
  if (wuffs_base__io_buffer__writer_length(dst) > 0x7FFFFFFF) {
    return "dst length is too large";
  } else if (wuffs_base__io_buffer__reader_length(src) > 0x7FFFFFFF) {
    return "src length is too large";
  } else if ((wlimit < UINT64_MAX) || (rlimit < UINT64_MAX)) {
    // Supporting this would probably mean using BZ2_bzDecompress instead of
    // the simpler BZ2_bzBuffToBuffDecompress function.
    return "unsupported I/O limit";
  }
  unsigned int dlen = wuffs_base__io_buffer__writer_length(dst);
  unsigned int slen = wuffs_base__io_buffer__reader_length(src);
  int err = BZ2_bzBuffToBuffDecompress(
      ((char*)wuffs_base__io_buffer__writer_pointer(dst)), &dlen,
      ((char*)wuffs_base__io_buffer__reader_pointer(src)), slen, 0, 0);
  if (err != BZ_OK) {
    return "libbz2: an error occurred";
  } else if (dlen > wuffs_base__io_buffer__writer_length(dst)) {
    return "libbz2: dst buffer overflow";
  }
  dst->meta.wi += dlen;
  return NULL;
}
