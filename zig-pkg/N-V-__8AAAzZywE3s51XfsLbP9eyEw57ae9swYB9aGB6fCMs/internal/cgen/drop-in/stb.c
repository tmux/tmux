// Copyright 2024 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Wuffs' reimplementation of the STB API.
//
// This is a drop-in replacement of that third-party library.
//
// Disabled by default, unless you #define the
// WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB macro beforehand.
//
// For API docs, see https://github.com/nothings/stb

#if defined(WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB)

#include <limits.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------

#if defined(__GNUC__)
__thread const char*  //
    wuffs_drop_in__stb__g_failure_reason = NULL;
#elif defined(_MSC_VER)
__declspec(thread) const char*  //
    wuffs_drop_in__stb__g_failure_reason = NULL;
#else
const char*  //
    wuffs_drop_in__stb__g_failure_reason = NULL;
#endif

// --------

static void                         //
wuffs_drop_in__stb__read(           //
    wuffs_base__io_buffer* srcbuf,  //
    stbi_io_callbacks const* clbk,  //
    void* user) {
  uint8_t* ptr = wuffs_base__io_buffer__writer_pointer(srcbuf);
  size_t len = wuffs_base__io_buffer__writer_length(srcbuf);
  if (len > INT_MAX) {
    len = INT_MAX;
  }
  int n = clbk->read(user, (char*)ptr, (int)len);
  if (n > 0) {
    srcbuf->meta.wi += (size_t)n;
  } else {
    srcbuf->meta.closed = clbk->eof(user);
  }
}

static wuffs_base__image_decoder*   //
wuffs_drop_in__stb__make_decoder(   //
    wuffs_base__io_buffer* srcbuf,  //
    stbi_io_callbacks const* clbk,  //
    void* user) {
  while (1) {
    int32_t fourcc = wuffs_base__magic_number_guess_fourcc(
        wuffs_base__io_buffer__reader_slice(srcbuf), srcbuf->meta.closed);
    if (fourcc < 0) {
      if (srcbuf->meta.closed || !clbk) {
        break;
      }
      wuffs_drop_in__stb__read(srcbuf, clbk, user);
      continue;
    }

    switch (fourcc) {
#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__BMP)
      case WUFFS_BASE__FOURCC__BMP:
        return wuffs_bmp__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__ETC2)
      case WUFFS_BASE__FOURCC__ETC2:
        return wuffs_etc2__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__GIF)
      case WUFFS_BASE__FOURCC__GIF:
        return wuffs_gif__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__JPEG)
      case WUFFS_BASE__FOURCC__JPEG:
        return wuffs_jpeg__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__NIE)
      case WUFFS_BASE__FOURCC__NIE:
        return wuffs_nie__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__NETPBM)
      case WUFFS_BASE__FOURCC__NPBM:
        return wuffs_netpbm__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__PNG)
      case WUFFS_BASE__FOURCC__PNG:
        return wuffs_png__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__QOI)
      case WUFFS_BASE__FOURCC__QOI:
        return wuffs_qoi__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__TARGA)
      case WUFFS_BASE__FOURCC__TGA:
        return wuffs_targa__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__THUMBHASH)
      case WUFFS_BASE__FOURCC__TH:
        return wuffs_thumbhash__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__WBMP)
      case WUFFS_BASE__FOURCC__WBMP:
        return wuffs_wbmp__decoder__alloc_as__wuffs_base__image_decoder();
#endif

#if !defined(WUFFS_CONFIG__MODULES) || defined(WUFFS_CONFIG__MODULE__WEBP)
      case WUFFS_BASE__FOURCC__WEBP:
        return wuffs_webp__decoder__alloc_as__wuffs_base__image_decoder();
#endif
    }

    wuffs_drop_in__stb__g_failure_reason = "unknown image type";
    break;
  }
  return NULL;
}

// --------

static void*                         //
wuffs_drop_in__stb__load1(           //
    wuffs_base__io_buffer* srcbuf,   //
    stbi_io_callbacks const* clbk,   //
    void* user,                      //
    wuffs_base__image_decoder* dec,  //
    wuffs_base__image_config* ic,    //
    uint32_t dst_pixfmt,             //
    int desired_channels,            //
    int info_only) {
  // Favor faster decodes over rejecting invalid checksums.
  wuffs_base__image_decoder__set_quirk(dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM,
                                       1);

  while (1) {
    wuffs_base__status status =
        wuffs_base__image_decoder__decode_image_config(dec, ic, srcbuf);
    if (status.repr == NULL) {
      break;
    } else if ((status.repr != wuffs_base__suspension__short_read) || !clbk) {
      wuffs_drop_in__stb__g_failure_reason = status.repr;
      return NULL;
    }

    size_t wl = wuffs_base__io_buffer__writer_length(srcbuf);
    wuffs_base__io_buffer__compact(srcbuf);
    if (wl >= wuffs_base__io_buffer__writer_length(srcbuf)) {
      wuffs_drop_in__stb__g_failure_reason = "I/O buffer is too small";
      return NULL;
    }
    wuffs_drop_in__stb__read(srcbuf, clbk, user);
  }

  uint32_t w = wuffs_base__pixel_config__width(&ic->pixcfg);
  uint32_t h = wuffs_base__pixel_config__height(&ic->pixcfg);
  if ((w > 0xFFFFFF) || (h > 0xFFFFFF)) {
    wuffs_drop_in__stb__g_failure_reason = "image is too large";
    return NULL;
  } else if (info_only) {
    return NULL;
  }

  uint64_t pixbuf_len = (uint64_t)w * (uint64_t)h * (uint64_t)desired_channels;
  uint64_t workbuf_len = wuffs_base__image_decoder__workbuf_len(dec).max_incl;
#if SIZE_MAX < 0xFFFFFFFFFFFFFFFFull
  if ((pixbuf_len > ((uint64_t)SIZE_MAX)) ||
      (workbuf_len > ((uint64_t)SIZE_MAX))) {
    wuffs_drop_in__stb__g_failure_reason = "image is too large";
    return NULL;
  }
#endif
  void* pixbuf_ptr = malloc((size_t)pixbuf_len);
  if (!pixbuf_ptr) {
    wuffs_drop_in__stb__g_failure_reason = "out of memory";
    return NULL;
  }
  void* workbuf_ptr = malloc((size_t)workbuf_len);
  if (!workbuf_ptr) {
    free(pixbuf_ptr);
    wuffs_drop_in__stb__g_failure_reason = "out of memory";
    return NULL;
  }
  wuffs_base__slice_u8 workbuf =
      wuffs_base__make_slice_u8(workbuf_ptr, (size_t)workbuf_len);

  wuffs_base__pixel_config pc = ((wuffs_base__pixel_config){});
  wuffs_base__pixel_config__set(&pc, dst_pixfmt,
                                WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  {
    wuffs_base__status status = wuffs_base__pixel_buffer__set_from_slice(
        &pb, &pc, wuffs_base__make_slice_u8(pixbuf_ptr, (size_t)pixbuf_len));
    if (status.repr) {
      free(workbuf_ptr);
      free(pixbuf_ptr);
      wuffs_drop_in__stb__g_failure_reason = status.repr;
      return NULL;
    }
  }

  while (1) {
    wuffs_base__status status = wuffs_base__image_decoder__decode_frame(
        dec, &pb, srcbuf, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf, NULL);
    if (status.repr == NULL) {
      break;
    } else if ((status.repr != wuffs_base__suspension__short_read) || !clbk) {
      free(workbuf_ptr);
      free(pixbuf_ptr);
      wuffs_drop_in__stb__g_failure_reason = status.repr;
      return NULL;
    }

    size_t wl = wuffs_base__io_buffer__writer_length(srcbuf);
    wuffs_base__io_buffer__compact(srcbuf);
    if (wl >= wuffs_base__io_buffer__writer_length(srcbuf)) {
      free(workbuf_ptr);
      free(pixbuf_ptr);
      wuffs_drop_in__stb__g_failure_reason = "I/O buffer is too small";
      return NULL;
    }
    wuffs_drop_in__stb__read(srcbuf, clbk, user);
  }

  free(workbuf_ptr);
  return pixbuf_ptr;
}

static void*                        //
wuffs_drop_in__stb__load0(          //
    wuffs_base__io_buffer* srcbuf,  //
    stbi_io_callbacks const* clbk,  //
    void* user,                     //
    int* x,                         //
    int* y,                         //
    int* channels_in_file,          //
    int desired_channels,           //
    int info_only) {
  uint32_t dst_pixfmt = 0;
  switch (desired_channels) {
    case 1:
      dst_pixfmt = WUFFS_BASE__PIXEL_FORMAT__Y;
      break;
    case 3:
      dst_pixfmt = WUFFS_BASE__PIXEL_FORMAT__RGB;
      break;
    case 4:
      dst_pixfmt = WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL;
      break;
    default:
      wuffs_drop_in__stb__g_failure_reason = "unsupported format conversion";
      return NULL;
  }

  wuffs_base__image_decoder* dec =
      wuffs_drop_in__stb__make_decoder(srcbuf, clbk, user);
  if (!dec) {
    if (wuffs_drop_in__stb__g_failure_reason == NULL) {
      wuffs_drop_in__stb__g_failure_reason = "couldn't allocate image decoder";
    }
    return NULL;
  }

  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  stbi_uc* ret = wuffs_drop_in__stb__load1(
      srcbuf, clbk, user, dec, &ic, dst_pixfmt, desired_channels, info_only);
  free(dec);

  if (!info_only && !ret) {
    return NULL;
  }

  if (x) {
    *x = (int)wuffs_base__pixel_config__width(&ic.pixcfg);
  }
  if (y) {
    *y = (int)wuffs_base__pixel_config__height(&ic.pixcfg);
  }
  if (channels_in_file) {
    wuffs_base__pixel_format src_pixfmt =
        wuffs_base__pixel_config__pixel_format(&ic.pixcfg);
    uint32_t n_color = wuffs_base__pixel_format__coloration(&src_pixfmt);
    uint32_t n_alpha = wuffs_base__pixel_format__transparency(&src_pixfmt) !=
                       WUFFS_BASE__PIXEL_ALPHA_TRANSPARENCY__OPAQUE;
    *channels_in_file = (int)(n_color + n_alpha);
  }

  return ret;
}

// --------

WUFFS_DROP_IN__STB__MAYBE_STATIC int  //
stbi_info_from_memory(                //
    stbi_uc const* buffer,            //
    int len,                          //
    int* x,                           //
    int* y,                           //
    int* comp) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  if (len < 0) {
    wuffs_drop_in__stb__g_failure_reason = "negative buffer length";
    return 0;
  } else if (len == 0) {
    wuffs_drop_in__stb__g_failure_reason = "empty buffer";
    return 0;
  }
  wuffs_base__io_buffer srcbuf =
      wuffs_base__ptr_u8__reader((uint8_t*)(stbi_uc*)buffer, (size_t)len, true);
  wuffs_drop_in__stb__load0(&srcbuf, NULL, NULL, x, y, comp, 1, 1);
  return wuffs_drop_in__stb__g_failure_reason == NULL;
}

WUFFS_DROP_IN__STB__MAYBE_STATIC stbi_uc*  //
stbi_load_from_memory(                     //
    stbi_uc const* buffer,                 //
    int len,                               //
    int* x,                                //
    int* y,                                //
    int* channels_in_file,                 //
    int desired_channels) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  if (len < 0) {
    wuffs_drop_in__stb__g_failure_reason = "negative buffer length";
    return NULL;
  } else if (len == 0) {
    wuffs_drop_in__stb__g_failure_reason = "empty buffer";
    return NULL;
  }
  wuffs_base__io_buffer srcbuf =
      wuffs_base__ptr_u8__reader((uint8_t*)(stbi_uc*)buffer, (size_t)len, true);
  return wuffs_drop_in__stb__load0(&srcbuf, NULL, NULL, x, y, channels_in_file,
                                   desired_channels, 0);
}

WUFFS_DROP_IN__STB__MAYBE_STATIC int  //
stbi_info_from_callbacks(             //
    stbi_io_callbacks const* clbk,    //
    void* user,                       //
    int* x,                           //
    int* y,                           //
    int* comp) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  void* iobuf_ptr = malloc(65536u);
  if (!iobuf_ptr) {
    wuffs_drop_in__stb__g_failure_reason = "out of memory";
    return 0;
  }
  wuffs_base__io_buffer srcbuf =
      wuffs_base__ptr_u8__writer((uint8_t*)iobuf_ptr, 65536u);
  wuffs_drop_in__stb__load0(&srcbuf, clbk, user, x, y, comp, 1, 1);
  free(iobuf_ptr);
  return wuffs_drop_in__stb__g_failure_reason == NULL;
}

WUFFS_DROP_IN__STB__MAYBE_STATIC stbi_uc*  //
stbi_load_from_callbacks(                  //
    stbi_io_callbacks const* clbk,         //
    void* user,                            //
    int* x,                                //
    int* y,                                //
    int* channels_in_file,                 //
    int desired_channels) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  void* iobuf_ptr = malloc(65536u);
  if (!iobuf_ptr) {
    wuffs_drop_in__stb__g_failure_reason = "out of memory";
    return NULL;
  }
  wuffs_base__io_buffer srcbuf =
      wuffs_base__ptr_u8__writer((uint8_t*)iobuf_ptr, 65536u);
  stbi_uc* ret = wuffs_drop_in__stb__load0(
      &srcbuf, clbk, user, x, y, channels_in_file, desired_channels, 0);
  free(iobuf_ptr);
  return ret;
}

WUFFS_DROP_IN__STB__MAYBE_STATIC void  //
stbi_image_free(                       //
    void* retval_from_stbi_load) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  free(retval_from_stbi_load);
}

WUFFS_DROP_IN__STB__MAYBE_STATIC const char*  //
stbi_failure_reason(void) {
  return wuffs_drop_in__stb__g_failure_reason
             ? wuffs_drop_in__stb__g_failure_reason
             : "ok";
}

// --------

#if !defined(STBI_NO_STDIO)

#include <stdio.h>

// TODO: retry after EINTR?

static int                                 //
wuffs_drop_in__stb__file_callbacks__read(  //
    void* user,                            //
    char* data,                            //
    int size) {
  return (int)fread(data, 1u, (size_t)size, (FILE*)user);
}

static void                                //
wuffs_drop_in__stb__file_callbacks__skip(  //
    void* user,                            //
    int n) {
  fseek((FILE*)user, (long)n, SEEK_CUR);
}

static int                                //
wuffs_drop_in__stb__file_callbacks__eof(  //
    void* user) {
  return feof((FILE*)user);
}

WUFFS_DROP_IN__STB__MAYBE_STATIC int  //
stbi_info(                            //
    char const* filename,             //
    int* x,                           //
    int* y,                           //
    int* comp) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  FILE* f = fopen(filename, "rb");
  if (!f) {
    wuffs_drop_in__stb__g_failure_reason = "could not open file";
    return 0;
  }
  int ret = stbi_info_from_file(f, x, y, comp);
  fclose(f);
  return ret;
}

WUFFS_DROP_IN__STB__MAYBE_STATIC stbi_uc*  //
stbi_load(                                 //
    char const* filename,                  //
    int* x,                                //
    int* y,                                //
    int* channels_in_file,                 //
    int desired_channels) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  FILE* f = fopen(filename, "rb");
  if (!f) {
    wuffs_drop_in__stb__g_failure_reason = "could not open file";
    return NULL;
  }
  stbi_uc* ret =
      stbi_load_from_file(f, x, y, channels_in_file, desired_channels);
  fclose(f);
  return ret;
}

WUFFS_DROP_IN__STB__MAYBE_STATIC int  //
stbi_info_from_file(                  //
    FILE* f,                          //
    int* x,                           //
    int* y,                           //
    int* comp) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  void* iobuf_ptr = malloc(65536u);
  if (!iobuf_ptr) {
    wuffs_drop_in__stb__g_failure_reason = "out of memory";
    return 0;
  }
  wuffs_base__io_buffer srcbuf =
      wuffs_base__ptr_u8__writer((uint8_t*)iobuf_ptr, 65536u);
  stbi_io_callbacks clbk;
  clbk.read = &wuffs_drop_in__stb__file_callbacks__read;
  clbk.skip = &wuffs_drop_in__stb__file_callbacks__skip;
  clbk.eof = &wuffs_drop_in__stb__file_callbacks__eof;
  wuffs_drop_in__stb__load0(&srcbuf, &clbk, f, x, y, comp, 1, 1);
  free(iobuf_ptr);
  return wuffs_drop_in__stb__g_failure_reason == NULL;
}

WUFFS_DROP_IN__STB__MAYBE_STATIC stbi_uc*  //
stbi_load_from_file(                       //
    FILE* f,                               //
    int* x,                                //
    int* y,                                //
    int* channels_in_file,                 //
    int desired_channels) {
  wuffs_drop_in__stb__g_failure_reason = NULL;
  void* iobuf_ptr = malloc(65536u);
  if (!iobuf_ptr) {
    wuffs_drop_in__stb__g_failure_reason = "out of memory";
    return NULL;
  }
  wuffs_base__io_buffer srcbuf =
      wuffs_base__ptr_u8__writer((uint8_t*)iobuf_ptr, 65536u);
  stbi_io_callbacks clbk;
  clbk.read = &wuffs_drop_in__stb__file_callbacks__read;
  clbk.skip = &wuffs_drop_in__stb__file_callbacks__skip;
  clbk.eof = &wuffs_drop_in__stb__file_callbacks__eof;
  stbi_uc* ret = wuffs_drop_in__stb__load0(
      &srcbuf, &clbk, f, x, y, channels_in_file, desired_channels, 0);
  free(iobuf_ptr);
  return ret;
}

#endif  // !defined(STBI_NO_STDIO)

// --------

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // defined (WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB)
