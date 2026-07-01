// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#ifndef WUFFS_INCLUDE_GUARD
#error "Wuffs' .h files need to be included before this file"
#endif

static const char*  //
fuzz_image_decoder(wuffs_base__io_buffer* src,
                   uint64_t hash,
                   wuffs_base__image_decoder* dec) {
  const char* ret = NULL;
  wuffs_base__slice_u8 pixbuf = ((wuffs_base__slice_u8){});
  wuffs_base__slice_u8 workbuf = ((wuffs_base__slice_u8){});

  // Use a {} code block so that "goto exit" doesn't trigger "jump bypasses
  // variable initialization" warnings.
  {
    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    wuffs_base__status status =
        wuffs_base__image_decoder__decode_image_config(dec, &ic, src);
    if (!wuffs_base__status__is_ok(&status)) {
      ret = wuffs_base__status__message(&status);
      goto exit;
    }
    if (!wuffs_base__image_config__is_valid(&ic)) {
      ret = "invalid image_config";
      goto exit;
    }

    // 50% of the time, choose BGRA_PREMUL instead of the native pixel config.
    if (hash & 1) {
      wuffs_base__pixel_config__set(
          &ic.pixcfg, WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL,
          WUFFS_BASE__PIXEL_SUBSAMPLING__NONE,
          wuffs_base__pixel_config__width(&ic.pixcfg),
          wuffs_base__pixel_config__height(&ic.pixcfg));
    }
    hash >>= 1;

    // Wuffs allows either statically or dynamically allocated work buffers.
    // This program exercises dynamic allocation.
    uint64_t n = wuffs_base__image_decoder__workbuf_len(dec).max_incl;
    if (n > 64 * 1024 * 1024) {  // Don't allocate more than 64 MiB.
      ret = "image too large";
      goto exit;
    }
    if (n > 0) {
      workbuf = wuffs_base__malloc_slice_u8(malloc, n);
      if (!workbuf.ptr) {
        ret = "out of memory";
        goto exit;
      }
    }

    n = wuffs_base__pixel_config__pixbuf_len(&ic.pixcfg);
    if (n > 64 * 1024 * 1024) {  // Don't allocate more than 64 MiB.
      ret = "image too large";
      goto exit;
    }
    if (n > 0) {
      pixbuf = wuffs_base__malloc_slice_u8(malloc, n);
      if (!pixbuf.ptr) {
        ret = "out of memory";
        goto exit;
      }
    }

    wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg, pixbuf);
    if (!wuffs_base__status__is_ok(&status)) {
      ret = wuffs_base__status__message(&status);
      goto exit;
    }

    bool seen_ok = false;
    while (true) {
      wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
      status = wuffs_base__image_decoder__decode_frame_config(dec, &fc, src);
      if (!wuffs_base__status__is_ok(&status)) {
        if ((status.repr != wuffs_base__note__end_of_data) || !seen_ok) {
          ret = wuffs_base__status__message(&status);
        }
        goto exit;
      }

      status = wuffs_base__image_decoder__decode_frame(
          dec, &pb, src, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf, NULL);

      wuffs_base__rect_ie_u32 frame_rect =
          wuffs_base__frame_config__bounds(&fc);
      wuffs_base__rect_ie_u32 dirty_rect =
          wuffs_base__image_decoder__frame_dirty_rect(dec);
      if (!wuffs_base__rect_ie_u32__contains_rect(&frame_rect, dirty_rect)) {
        ret = "internal error: frame_rect does not contain dirty_rect";
        goto exit;
      }

      if (!wuffs_base__status__is_ok(&status)) {
        if ((status.repr != wuffs_base__note__end_of_data) || !seen_ok) {
          ret = wuffs_base__status__message(&status);
        }
        goto exit;
      }
      seen_ok = true;

      if (!wuffs_base__rect_ie_u32__equals(&frame_rect, dirty_rect)) {
        ret = "internal error: frame_rect does not equal dirty_rect";
        goto exit;
      }
    }
  }

exit:
  free(workbuf.ptr);
  free(pixbuf.ptr);
  return ret;
}
