// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Auxiliary - Image

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__AUX__IMAGE)

#include <utility>

namespace wuffs_aux {

DecodeImageResult::DecodeImageResult(MemOwner&& pixbuf_mem_owner0,
                                     wuffs_base__pixel_buffer pixbuf0,
                                     std::string&& error_message0)
    : pixbuf_mem_owner(std::move(pixbuf_mem_owner0)),
      pixbuf(pixbuf0),
      error_message(std::move(error_message0)) {}

DecodeImageResult::DecodeImageResult(std::string&& error_message0)
    : pixbuf_mem_owner(nullptr, &free),
      pixbuf(wuffs_base__null_pixel_buffer()),
      error_message(std::move(error_message0)) {}

DecodeImageCallbacks::~DecodeImageCallbacks() {}

DecodeImageCallbacks::AllocPixbufResult::AllocPixbufResult(
    MemOwner&& mem_owner0,
    wuffs_base__pixel_buffer pixbuf0)
    : mem_owner(std::move(mem_owner0)), pixbuf(pixbuf0), error_message("") {}

DecodeImageCallbacks::AllocPixbufResult::AllocPixbufResult(
    std::string&& error_message0)
    : mem_owner(nullptr, &free),
      pixbuf(wuffs_base__null_pixel_buffer()),
      error_message(std::move(error_message0)) {}

DecodeImageCallbacks::AllocWorkbufResult::AllocWorkbufResult(
    MemOwner&& mem_owner0,
    wuffs_base__slice_u8 workbuf0)
    : mem_owner(std::move(mem_owner0)), workbuf(workbuf0), error_message("") {}

DecodeImageCallbacks::AllocWorkbufResult::AllocWorkbufResult(
    std::string&& error_message0)
    : mem_owner(nullptr, &free),
      workbuf(wuffs_base__empty_slice_u8()),
      error_message(std::move(error_message0)) {}

wuffs_base__image_decoder::unique_ptr  //
DecodeImageCallbacks::SelectDecoder(uint32_t fourcc,
                                    wuffs_base__slice_u8 prefix_data,
                                    bool prefix_closed) {
  switch (fourcc) {
#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BMP)
    case WUFFS_BASE__FOURCC__BMP:
      return wuffs_bmp__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__ETC2)
    case WUFFS_BASE__FOURCC__ETC2:
      return wuffs_etc2__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__GIF)
    case WUFFS_BASE__FOURCC__GIF:
      return wuffs_gif__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__JPEG)
    case WUFFS_BASE__FOURCC__JPEG:
      return wuffs_jpeg__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__NIE)
    case WUFFS_BASE__FOURCC__NIE:
      return wuffs_nie__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__NETPBM)
    case WUFFS_BASE__FOURCC__NPBM:
      return wuffs_netpbm__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__PNG)
    case WUFFS_BASE__FOURCC__PNG: {
      auto dec = wuffs_png__decoder::alloc_as__wuffs_base__image_decoder();
      // Favor faster decodes over rejecting invalid checksums.
      dec->set_quirk(WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, 1);
      return dec;
    }
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__QOI)
    case WUFFS_BASE__FOURCC__QOI:
      return wuffs_qoi__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__TARGA)
    case WUFFS_BASE__FOURCC__TGA:
      return wuffs_targa__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__THUMBHASH)
    case WUFFS_BASE__FOURCC__TH:
      return wuffs_thumbhash__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__WBMP)
    case WUFFS_BASE__FOURCC__WBMP:
      return wuffs_wbmp__decoder::alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__WEBP)
    case WUFFS_BASE__FOURCC__WEBP:
      return wuffs_webp__decoder::alloc_as__wuffs_base__image_decoder();
#endif
  }

  return wuffs_base__image_decoder::unique_ptr(nullptr);
}

std::string  //
DecodeImageCallbacks::HandleMetadata(const wuffs_base__more_information& minfo,
                                     wuffs_base__slice_u8 raw) {
  return "";
}

wuffs_base__pixel_format  //
DecodeImageCallbacks::SelectPixfmt(
    const wuffs_base__image_config& image_config) {
  return wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL);
}

DecodeImageCallbacks::AllocPixbufResult  //
DecodeImageCallbacks::AllocPixbuf(const wuffs_base__image_config& image_config,
                                  bool allow_uninitialized_memory) {
  uint32_t w = image_config.pixcfg.width();
  uint32_t h = image_config.pixcfg.height();
  if ((w == 0) || (h == 0)) {
    return AllocPixbufResult("");
  }
  uint64_t len = image_config.pixcfg.pixbuf_len();
  if ((len == 0) || (SIZE_MAX < len)) {
    return AllocPixbufResult(DecodeImage_UnsupportedPixelConfiguration);
  }
  void* ptr =
      allow_uninitialized_memory ? malloc((size_t)len) : calloc(1, (size_t)len);
  if (!ptr) {
    return AllocPixbufResult(DecodeImage_OutOfMemory);
  }
  wuffs_base__pixel_buffer pixbuf;
  wuffs_base__status status = pixbuf.set_from_slice(
      &image_config.pixcfg,
      wuffs_base__make_slice_u8((uint8_t*)ptr, (size_t)len));
  if (!status.is_ok()) {
    free(ptr);
    return AllocPixbufResult(status.message());
  }
  return AllocPixbufResult(MemOwner(ptr, &free), pixbuf);
}

DecodeImageCallbacks::AllocWorkbufResult  //
DecodeImageCallbacks::AllocWorkbuf(wuffs_base__range_ii_u64 len_range,
                                   bool allow_uninitialized_memory) {
  uint64_t len = len_range.max_incl;
  if (len == 0) {
    return AllocWorkbufResult("");
  } else if (SIZE_MAX < len) {
    return AllocWorkbufResult(DecodeImage_OutOfMemory);
  }
  void* ptr =
      allow_uninitialized_memory ? malloc((size_t)len) : calloc(1, (size_t)len);
  if (!ptr) {
    return AllocWorkbufResult(DecodeImage_OutOfMemory);
  }
  return AllocWorkbufResult(
      MemOwner(ptr, &free),
      wuffs_base__make_slice_u8((uint8_t*)ptr, (size_t)len));
}

void  //
DecodeImageCallbacks::Done(
    DecodeImageResult& result,
    sync_io::Input& input,
    IOBuffer& buffer,
    wuffs_base__image_decoder::unique_ptr image_decoder) {}

const char DecodeImage_BufferIsTooShort[] =  //
    "wuffs_aux::DecodeImage: buffer is too short";
const char DecodeImage_MaxInclDimensionExceeded[] =  //
    "wuffs_aux::DecodeImage: max_incl_dimension exceeded";
const char DecodeImage_MaxInclMetadataLengthExceeded[] =  //
    "wuffs_aux::DecodeImage: max_incl_metadata_length exceeded";
const char DecodeImage_OutOfMemory[] =  //
    "wuffs_aux::DecodeImage: out of memory";
const char DecodeImage_UnexpectedEndOfFile[] =  //
    "wuffs_aux::DecodeImage: unexpected end of file";
const char DecodeImage_UnsupportedImageFormat[] =  //
    "wuffs_aux::DecodeImage: unsupported image format";
const char DecodeImage_UnsupportedMetadata[] =  //
    "wuffs_aux::DecodeImage: unsupported metadata";
const char DecodeImage_UnsupportedPixelBlend[] =  //
    "wuffs_aux::DecodeImage: unsupported pixel blend";
const char DecodeImage_UnsupportedPixelConfiguration[] =  //
    "wuffs_aux::DecodeImage: unsupported pixel configuration";
const char DecodeImage_UnsupportedPixelFormat[] =  //
    "wuffs_aux::DecodeImage: unsupported pixel format";

DecodeImageArgQuirks::DecodeImageArgQuirks(const QuirkKeyValuePair* ptr0,
                                           const size_t len0)
    : ptr(ptr0), len(len0) {}

DecodeImageArgQuirks  //
DecodeImageArgQuirks::DefaultValue() {
  return DecodeImageArgQuirks(nullptr, 0);
}

DecodeImageArgFlags::DecodeImageArgFlags(uint64_t repr0) : repr(repr0) {}

DecodeImageArgFlags  //
DecodeImageArgFlags::DefaultValue() {
  return DecodeImageArgFlags(0);
}

DecodeImageArgPixelBlend::DecodeImageArgPixelBlend(
    wuffs_base__pixel_blend repr0)
    : repr(repr0) {}

DecodeImageArgPixelBlend  //
DecodeImageArgPixelBlend::DefaultValue() {
  return DecodeImageArgPixelBlend(WUFFS_BASE__PIXEL_BLEND__SRC);
}

DecodeImageArgBackgroundColor::DecodeImageArgBackgroundColor(
    wuffs_base__color_u32_argb_premul repr0)
    : repr(repr0) {}

DecodeImageArgBackgroundColor  //
DecodeImageArgBackgroundColor::DefaultValue() {
  return DecodeImageArgBackgroundColor(1);
}

DecodeImageArgMaxInclDimension::DecodeImageArgMaxInclDimension(uint32_t repr0)
    : repr(repr0) {}

DecodeImageArgMaxInclDimension  //
DecodeImageArgMaxInclDimension::DefaultValue() {
  return DecodeImageArgMaxInclDimension(1048575);
}

DecodeImageArgMaxInclMetadataLength::DecodeImageArgMaxInclMetadataLength(
    uint64_t repr0)
    : repr(repr0) {}

DecodeImageArgMaxInclMetadataLength  //
DecodeImageArgMaxInclMetadataLength::DefaultValue() {
  return DecodeImageArgMaxInclMetadataLength(16777215);
}

// --------

namespace {

const private_impl::ErrorMessages DecodeImageErrorMessages = {
    DecodeImage_MaxInclMetadataLengthExceeded,  //
    DecodeImage_OutOfMemory,                    //
    DecodeImage_UnexpectedEndOfFile,            //
    DecodeImage_UnsupportedMetadata,            //
    DecodeImage_UnsupportedImageFormat,         //
};

std::string  //
DecodeImageAdvanceIOBufferTo(sync_io::Input& input,
                             wuffs_base__io_buffer& io_buf,
                             uint64_t absolute_position) {
  return private_impl::AdvanceIOBufferTo(DecodeImageErrorMessages, input,
                                         io_buf, absolute_position);
}

wuffs_base__status  //
DIHM0(void* self,
      wuffs_base__io_buffer* a_dst,
      wuffs_base__more_information* a_minfo,
      wuffs_base__io_buffer* a_src) {
  return wuffs_base__image_decoder__tell_me_more(
      static_cast<wuffs_base__image_decoder*>(self), a_dst, a_minfo, a_src);
}

std::string  //
DIHM1(void* self,
      const wuffs_base__more_information* minfo,
      wuffs_base__slice_u8 raw) {
  return static_cast<DecodeImageCallbacks*>(self)->HandleMetadata(*minfo, raw);
}

std::string  //
DecodeImageHandleMetadata(wuffs_base__image_decoder::unique_ptr& image_decoder,
                          DecodeImageCallbacks& callbacks,
                          sync_io::Input& input,
                          wuffs_base__io_buffer& io_buf,
                          sync_io::DynIOBuffer& raw_metadata_buf) {
  return private_impl::HandleMetadata(DecodeImageErrorMessages, input, io_buf,
                                      raw_metadata_buf, DIHM0,
                                      static_cast<void*>(image_decoder.get()),
                                      DIHM1, static_cast<void*>(&callbacks));
}

DecodeImageResult  //
DecodeImage0(wuffs_base__image_decoder::unique_ptr& image_decoder,
             DecodeImageCallbacks& callbacks,
             sync_io::Input& input,
             wuffs_base__io_buffer& io_buf,
             const QuirkKeyValuePair* quirks_ptr,
             const size_t quirks_len,
             uint64_t flags,
             wuffs_base__pixel_blend pixel_blend,
             wuffs_base__color_u32_argb_premul background_color,
             uint32_t max_incl_dimension,
             uint64_t max_incl_metadata_length) {
  // Check args.
  switch (pixel_blend) {
    case WUFFS_BASE__PIXEL_BLEND__SRC:
    case WUFFS_BASE__PIXEL_BLEND__SRC_OVER:
      break;
    default:
      return DecodeImageResult(DecodeImage_UnsupportedPixelBlend);
  }

  wuffs_base__image_config image_config = wuffs_base__null_image_config();
  sync_io::DynIOBuffer raw_metadata_buf(max_incl_metadata_length);
  uint64_t start_pos = io_buf.reader_position();
  bool interested_in_metadata_after_the_frame = false;
  bool redirected = false;
  int32_t fourcc = 0;
redirect:
  do {
    // Determine the image format.
    if (!redirected) {
      while (true) {
        fourcc = wuffs_base__magic_number_guess_fourcc(io_buf.reader_slice(),
                                                       io_buf.meta.closed);
        if (fourcc > 0) {
          break;
        } else if ((fourcc == 0) && (io_buf.reader_length() >= 64)) {
          // Having (fourcc == 0) means that Wuffs' built in MIME sniffer
          // didn't recognize the image format. Nonetheless, custom callbacks
          // may still be able to do their own MIME sniffing, for exotic image
          // types. We try to give them at least 64 bytes of prefix data when
          // one-shot-calling callbacks.SelectDecoder. There is no mechanism
          // for the callbacks to request a longer prefix.
          break;
        } else if (io_buf.meta.closed || (io_buf.writer_length() == 0)) {
          fourcc = 0;
          break;
        }
        std::string error_message = input.CopyIn(&io_buf);
        if (!error_message.empty()) {
          return DecodeImageResult(std::move(error_message));
        }
      }
    } else {
      wuffs_base__io_buffer empty = wuffs_base__empty_io_buffer();
      wuffs_base__more_information minfo = wuffs_base__empty_more_information();
      wuffs_base__status tmm_status =
          image_decoder->tell_me_more(&empty, &minfo, &io_buf);
      if (tmm_status.repr != nullptr) {
        return DecodeImageResult(tmm_status.message());
      }
      if (minfo.flavor != WUFFS_BASE__MORE_INFORMATION__FLAVOR__IO_REDIRECT) {
        return DecodeImageResult(DecodeImage_UnsupportedImageFormat);
      }
      uint64_t pos = minfo.io_redirect__range().min_incl;
      if (pos <= start_pos) {
        // Redirects must go forward.
        return DecodeImageResult(DecodeImage_UnsupportedImageFormat);
      }
      std::string error_message =
          DecodeImageAdvanceIOBufferTo(input, io_buf, pos);
      if (!error_message.empty()) {
        return DecodeImageResult(std::move(error_message));
      }
      fourcc = (int32_t)(minfo.io_redirect__fourcc());
      if (fourcc == 0) {
        return DecodeImageResult(DecodeImage_UnsupportedImageFormat);
      }
      image_decoder.reset();
    }

    // Select the image decoder.
    image_decoder = callbacks.SelectDecoder(
        (uint32_t)fourcc, io_buf.reader_slice(), io_buf.meta.closed);
    if (!image_decoder) {
      return DecodeImageResult(DecodeImage_UnsupportedImageFormat);
    }

    // Apply quirks.
    for (size_t i = 0; i < quirks_len; i++) {
      image_decoder->set_quirk(quirks_ptr[i].first, quirks_ptr[i].second);
    }

    // Apply flags.
    if (flags != 0) {
      if (flags & DecodeImageArgFlags::REPORT_METADATA_CHRM) {
        image_decoder->set_report_metadata(WUFFS_BASE__FOURCC__CHRM, true);
      }
      if (flags & DecodeImageArgFlags::REPORT_METADATA_EXIF) {
        interested_in_metadata_after_the_frame = true;
        image_decoder->set_report_metadata(WUFFS_BASE__FOURCC__EXIF, true);
      }
      if (flags & DecodeImageArgFlags::REPORT_METADATA_GAMA) {
        image_decoder->set_report_metadata(WUFFS_BASE__FOURCC__GAMA, true);
      }
      if (flags & DecodeImageArgFlags::REPORT_METADATA_ICCP) {
        image_decoder->set_report_metadata(WUFFS_BASE__FOURCC__ICCP, true);
      }
      if (flags & DecodeImageArgFlags::REPORT_METADATA_KVP) {
        interested_in_metadata_after_the_frame = true;
        image_decoder->set_report_metadata(WUFFS_BASE__FOURCC__KVP, true);
      }
      if (flags & DecodeImageArgFlags::REPORT_METADATA_SRGB) {
        image_decoder->set_report_metadata(WUFFS_BASE__FOURCC__SRGB, true);
      }
      if (flags & DecodeImageArgFlags::REPORT_METADATA_XMP) {
        interested_in_metadata_after_the_frame = true;
        image_decoder->set_report_metadata(WUFFS_BASE__FOURCC__XMP, true);
      }
    }

    // Decode the image config.
    while (true) {
      wuffs_base__status id_dic_status =
          image_decoder->decode_image_config(&image_config, &io_buf);
      if (id_dic_status.repr == nullptr) {
        break;
      } else if (id_dic_status.repr == wuffs_base__note__i_o_redirect) {
        if (redirected) {
          return DecodeImageResult(DecodeImage_UnsupportedImageFormat);
        }
        redirected = true;
        goto redirect;
      } else if (id_dic_status.repr == wuffs_base__note__metadata_reported) {
        std::string error_message = DecodeImageHandleMetadata(
            image_decoder, callbacks, input, io_buf, raw_metadata_buf);
        if (!error_message.empty()) {
          return DecodeImageResult(std::move(error_message));
        }
      } else if (id_dic_status.repr != wuffs_base__suspension__short_read) {
        return DecodeImageResult(id_dic_status.message());
      } else if (io_buf.meta.closed) {
        return DecodeImageResult(DecodeImage_UnexpectedEndOfFile);
      } else {
        std::string error_message = input.CopyIn(&io_buf);
        if (!error_message.empty()) {
          return DecodeImageResult(std::move(error_message));
        }
      }
    }
  } while (false);
  if (!interested_in_metadata_after_the_frame) {
    raw_metadata_buf.drop();
  }

  // Select the pixel format.
  uint32_t w = image_config.pixcfg.width();
  uint32_t h = image_config.pixcfg.height();
  if ((w > max_incl_dimension) || (h > max_incl_dimension)) {
    return DecodeImageResult(DecodeImage_MaxInclDimensionExceeded);
  }
  wuffs_base__pixel_format pixel_format = callbacks.SelectPixfmt(image_config);
  if (pixel_format.repr != image_config.pixcfg.pixel_format().repr) {
    switch (pixel_format.repr) {
      case WUFFS_BASE__PIXEL_FORMAT__Y:
      case WUFFS_BASE__PIXEL_FORMAT__BGR_565:
      case WUFFS_BASE__PIXEL_FORMAT__BGR:
      case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL:
      case WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL_4X16LE:
      case WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL:
      case WUFFS_BASE__PIXEL_FORMAT__RGB:
      case WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL:
      case WUFFS_BASE__PIXEL_FORMAT__RGBA_PREMUL:
        break;
      default:
        return DecodeImageResult(DecodeImage_UnsupportedPixelFormat);
    }
    image_config.pixcfg.set(pixel_format.repr,
                            WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);
  }

  // Allocate the pixel buffer.
  bool valid_background_color =
      wuffs_base__color_u32_argb_premul__is_valid(background_color);
  DecodeImageCallbacks::AllocPixbufResult alloc_pixbuf_result =
      callbacks.AllocPixbuf(image_config, valid_background_color);
  if (!alloc_pixbuf_result.error_message.empty()) {
    return DecodeImageResult(std::move(alloc_pixbuf_result.error_message));
  }
  wuffs_base__pixel_buffer pixel_buffer = alloc_pixbuf_result.pixbuf;
  if (valid_background_color) {
    wuffs_base__status pb_scufr_status = pixel_buffer.set_color_u32_fill_rect(
        pixel_buffer.pixcfg.bounds(), background_color);
    if (pb_scufr_status.repr != nullptr) {
      return DecodeImageResult(pb_scufr_status.message());
    }
  }

  // Allocate the work buffer. Wuffs' decoders conventionally assume that this
  // can be uninitialized memory.
  wuffs_base__range_ii_u64 workbuf_len = image_decoder->workbuf_len();
  DecodeImageCallbacks::AllocWorkbufResult alloc_workbuf_result =
      callbacks.AllocWorkbuf(workbuf_len, true);
  if (!alloc_workbuf_result.error_message.empty()) {
    return DecodeImageResult(std::move(alloc_workbuf_result.error_message));
  } else if (alloc_workbuf_result.workbuf.len < workbuf_len.min_incl) {
    return DecodeImageResult(DecodeImage_BufferIsTooShort);
  }

  // Decode the frame config.
  wuffs_base__frame_config frame_config = wuffs_base__null_frame_config();
  while (true) {
    wuffs_base__status id_dfc_status =
        image_decoder->decode_frame_config(&frame_config, &io_buf);
    if (id_dfc_status.repr == nullptr) {
      break;
    } else if (id_dfc_status.repr == wuffs_base__note__metadata_reported) {
      std::string error_message = DecodeImageHandleMetadata(
          image_decoder, callbacks, input, io_buf, raw_metadata_buf);
      if (!error_message.empty()) {
        return DecodeImageResult(std::move(error_message));
      }
    } else if (id_dfc_status.repr != wuffs_base__suspension__short_read) {
      return DecodeImageResult(id_dfc_status.message());
    } else if (io_buf.meta.closed) {
      return DecodeImageResult(DecodeImage_UnexpectedEndOfFile);
    } else {
      std::string error_message = input.CopyIn(&io_buf);
      if (!error_message.empty()) {
        return DecodeImageResult(std::move(error_message));
      }
    }
  }

  // Decode the frame (the pixels).
  //
  // From here on, always returns the pixel_buffer. If we get this far, we can
  // still display a partial image, even if we encounter an error.
  std::string message("");
  if ((pixel_blend == WUFFS_BASE__PIXEL_BLEND__SRC_OVER) &&
      frame_config.overwrite_instead_of_blend()) {
    pixel_blend = WUFFS_BASE__PIXEL_BLEND__SRC;
  }
  while (true) {
    wuffs_base__status id_df_status =
        image_decoder->decode_frame(&pixel_buffer, &io_buf, pixel_blend,
                                    alloc_workbuf_result.workbuf, nullptr);
    if (id_df_status.repr == nullptr) {
      break;
    } else if (id_df_status.repr != wuffs_base__suspension__short_read) {
      message = id_df_status.message();
      break;
    } else if (io_buf.meta.closed) {
      message = DecodeImage_UnexpectedEndOfFile;
      break;
    } else {
      std::string error_message = input.CopyIn(&io_buf);
      if (!error_message.empty()) {
        message = std::move(error_message);
        break;
      }
    }
  }

  // Decode any metadata after the frame.
  if (interested_in_metadata_after_the_frame) {
    while (true) {
      wuffs_base__status id_dfc_status =
          image_decoder->decode_frame_config(NULL, &io_buf);
      if (id_dfc_status.repr == wuffs_base__note__end_of_data) {
        break;
      } else if (id_dfc_status.repr == nullptr) {
        continue;
      } else if (id_dfc_status.repr == wuffs_base__note__metadata_reported) {
        std::string error_message = DecodeImageHandleMetadata(
            image_decoder, callbacks, input, io_buf, raw_metadata_buf);
        if (!error_message.empty()) {
          return DecodeImageResult(std::move(error_message));
        }
      } else if (id_dfc_status.repr != wuffs_base__suspension__short_read) {
        return DecodeImageResult(id_dfc_status.message());
      } else if (io_buf.meta.closed) {
        return DecodeImageResult(DecodeImage_UnexpectedEndOfFile);
      } else {
        std::string error_message = input.CopyIn(&io_buf);
        if (!error_message.empty()) {
          return DecodeImageResult(std::move(error_message));
        }
      }
    }
  }

  return DecodeImageResult(std::move(alloc_pixbuf_result.mem_owner),
                           pixel_buffer, std::move(message));
}

}  // namespace

DecodeImageResult  //
DecodeImage(DecodeImageCallbacks& callbacks,
            sync_io::Input& input,
            DecodeImageArgQuirks quirks,
            DecodeImageArgFlags flags,
            DecodeImageArgPixelBlend pixel_blend,
            DecodeImageArgBackgroundColor background_color,
            DecodeImageArgMaxInclDimension max_incl_dimension,
            DecodeImageArgMaxInclMetadataLength max_incl_metadata_length) {
  wuffs_base__io_buffer* io_buf = input.BringsItsOwnIOBuffer();
  wuffs_base__io_buffer fallback_io_buf = wuffs_base__empty_io_buffer();
  std::unique_ptr<uint8_t[]> fallback_io_array(nullptr);
  if (!io_buf) {
    fallback_io_array = std::unique_ptr<uint8_t[]>(new uint8_t[32768]);
    fallback_io_buf =
        wuffs_base__ptr_u8__writer(fallback_io_array.get(), 32768);
    io_buf = &fallback_io_buf;
  }

  wuffs_base__image_decoder::unique_ptr image_decoder(nullptr);
  DecodeImageResult result = DecodeImage0(
      image_decoder, callbacks, input, *io_buf, quirks.ptr, quirks.len,
      flags.repr, pixel_blend.repr, background_color.repr,
      max_incl_dimension.repr, max_incl_metadata_length.repr);
  callbacks.Done(result, input, *io_buf, std::move(image_decoder));
  return result;
}

}  // namespace wuffs_aux

#endif  // !defined(WUFFS_CONFIG__MODULES) ||
        // defined(WUFFS_CONFIG__MODULE__AUX__IMAGE)
