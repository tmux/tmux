// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "lzma.h"

const char*  //
mimic_lzma_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  if (wuffs_base__io_buffer__writer_length(dst) > 0x7FFFFFFF) {
    return "dst length is too large";
  } else if (wuffs_base__io_buffer__reader_length(src) > 0x7FFFFFFF) {
    return "src length is too large";
  } else if ((wlimit < UINT64_MAX) || (rlimit < UINT64_MAX)) {
    // It's simpler if we only assume one-shot decompression.
    return "unsupported I/O limit";
  }
  lzma_stream z = LZMA_STREAM_INIT;

  lzma_ret ret = lzma_auto_decoder(&z, UINT64_MAX, 0);
  if (ret != LZMA_OK) {
    return "liblzma: lzma_auto_decoder failed";
  }
  z.next_out = wuffs_base__io_buffer__writer_pointer(dst);
  z.avail_out = wuffs_base__io_buffer__writer_length(dst);
  z.next_in = wuffs_base__io_buffer__reader_pointer(src);
  z.avail_in = wuffs_base__io_buffer__reader_length(src);

  ret = lzma_code(&z, LZMA_RUN);
  dst->meta.wi += (size_t)z.total_out;
  src->meta.ri += (size_t)z.total_in;
  lzma_end(&z);
  if (ret != LZMA_STREAM_END) {
    return "liblzma: lzma_code failed";
  }
  return NULL;
}
