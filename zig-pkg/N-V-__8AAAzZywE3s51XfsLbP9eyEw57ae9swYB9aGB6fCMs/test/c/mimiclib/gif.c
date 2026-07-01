// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include "gif_lib.h"

int  //
mimic_gif_read_func(GifFileType* f, GifByteType* ptr, int len) {
  wuffs_base__io_buffer* src = (wuffs_base__io_buffer*)(f->UserData);
  if (len < 0) {
    return 0;
  }
  size_t n = (size_t)(len);
  size_t num_src = src->meta.wi - src->meta.ri;
  if (n > num_src) {
    n = num_src;
  }
  memmove(ptr, src->data.ptr + src->meta.ri, n);
  src->meta.ri += n;
  return n;
}

const char*  //
mimic_gif_decode(uint64_t* n_bytes_out,
                 wuffs_base__io_buffer* dst,
                 uint32_t wuffs_initialize_flags,
                 wuffs_base__pixel_format pixfmt,
                 uint32_t* quirks_ptr,
                 size_t quirks_len,
                 wuffs_base__io_buffer* src) {
  const char* ret = NULL;

  // http://giflib.sourceforge.net/gif_lib.html#compatibility says that "A few
  // changes in behavior were introduced in 5.0:
  //
  // GIF file openers and closers - DGifOpenFileName(), DGifOpenFileHandle(),
  // DGifOpen(), DGifClose(), EGifOpenFileName(), EGifOpenFileHandle(),
  // EGifOpen(), and EGifClose() - all now take a final integer address
  // argument. If non-null, this is used to pass back an error code when the
  // function returns NULL."
#if defined(GIFLIB_MAJOR) && (GIFLIB_MAJOR >= 5)
  GifFileType* f = DGifOpen(src, mimic_gif_read_func, NULL);
#else
  GifFileType* f = DGifOpen(src, mimic_gif_read_func);
#endif
  if (!f) {
    ret = "DGifOpen failed";
    goto cleanup0;
  }
  if (DGifSlurp(f) != GIF_OK) {
    ret = "DGifSlurp failed";
    goto cleanup1;
  }

  for (int i = 0; i < f->ImageCount; i++) {
    // Copy the pixel data from the GifFileType* f to the dst buffer, since the
    // former is free'd at the end of this function.
    //
    // In theory, this mimic_gif_decode function might be faster overall if the
    // DGifSlurp call above decoded the pixel data directly into dst instead of
    // into an intermediate buffer that needed to be malloc'ed and then free'd.
    // In practice, doing so did not seem to show a huge difference. (See
    // commit ab7e0ae "Add a custom gif_mimic_DGifSlurp function.") It also
    // further complicates supporting both versions 4 and 5 of giflib. That
    // commit was therefore rolled back.
    struct SavedImage* si = &f->SavedImages[i];
    size_t num_src =
        (size_t)(si->ImageDesc.Width) * (size_t)(si->ImageDesc.Height);
    if (n_bytes_out) {
      *n_bytes_out += num_src;
    }
    if (dst) {
      size_t num_dst = dst->data.len - dst->meta.wi;
      if (num_dst < num_src) {
        ret = "GIF image's pixel data won't fit in the dst buffer";
        goto cleanup1;
      }
      memmove(dst->data.ptr + dst->meta.wi, si->RasterBits, num_src);
      dst->meta.wi += num_src;
    }
  }

cleanup1:;
#if defined(GIFLIB_MAJOR) && (GIFLIB_MAJOR >= 5)
  int close_status = DGifCloseFile(f, NULL);
#else
  int close_status = DGifCloseFile(f);
#endif
  if ((close_status != GIF_OK) && !ret) {
    ret = "DGifCloseFile failed";
  }
cleanup0:;
  return ret;
}
