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

// Uncomment one of these #define lines to test and bench alternative mimic
// libraries (libdeflate or miniz) instead of zlib-the-library.
//
// These are collectively referred to as
// WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_ZLIB.
//
// #define WUFFS_MIMICLIB_USE_LIBDEFLATE_INSTEAD_OF_ZLIB 1
// #define WUFFS_MIMICLIB_USE_MINIZ_INSTEAD_OF_ZLIB 1

// -------------------------------- WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_ZLIB
#if defined(WUFFS_MIMICLIB_USE_LIBDEFLATE_INSTEAD_OF_ZLIB)
#include "libdeflate.h"

#define WUFFS_MIMICLIB_DEFLATE_DOES_NOT_SUPPORT_STREAMING 1

#define WUFFS_MIMICLIB_ZLIB_DOES_NOT_SUPPORT_DICTIONARIES 1

uint32_t global_mimiclib_deflate_unused_u32;

typedef enum libdeflate_result (*libdeflate_decompress_func)(
    struct libdeflate_decompressor* decompressor,
    const void* in,
    size_t in_nbytes,
    void* out,
    size_t out_nbytes_avail,
    size_t* actual_in_nbytes_ret,
    size_t* actual_out_nbytes_ret);

const char*  //
libdeflate_result_as_const_char_star(enum libdeflate_result r) {
  switch (r) {
    case LIBDEFLATE_SUCCESS:
      return NULL;
    case LIBDEFLATE_BAD_DATA:
      return "libdeflate: bad data";
    case LIBDEFLATE_SHORT_OUTPUT:
      return "libdeflate: short output";
    case LIBDEFLATE_INSUFFICIENT_SPACE:
      return "libdeflate: insufficient space";
  }
  return "libdeflate: unknown error";
}

const char*  //
mimic_bench_adler32(wuffs_base__io_buffer* dst,
                    wuffs_base__io_buffer* src,
                    uint32_t wuffs_initialize_flags,
                    uint64_t wlimit,
                    uint64_t rlimit) {
  global_mimiclib_deflate_unused_u32 = 1;
  while (src->meta.ri < src->meta.wi) {
    uint8_t* ptr = src->data.ptr + src->meta.ri;
    size_t len = src->meta.wi - src->meta.ri;
    if (len > 0x7FFFFFFF) {
      return "src length is too large";
    } else if (len > rlimit) {
      len = rlimit;
    }
    global_mimiclib_deflate_unused_u32 =
        libdeflate_adler32(global_mimiclib_deflate_unused_u32, ptr, len);
    src->meta.ri += len;
  }
  return NULL;
}

const char*  //
mimic_bench_crc32_ieee(wuffs_base__io_buffer* dst,
                       wuffs_base__io_buffer* src,
                       uint32_t wuffs_initialize_flags,
                       uint64_t wlimit,
                       uint64_t rlimit) {
  global_mimiclib_deflate_unused_u32 = 0;
  while (src->meta.ri < src->meta.wi) {
    uint8_t* ptr = src->data.ptr + src->meta.ri;
    size_t len = src->meta.wi - src->meta.ri;
    if (len > 0x7FFFFFFF) {
      return "src length is too large";
    } else if (len > rlimit) {
      len = rlimit;
    }
    global_mimiclib_deflate_unused_u32 =
        libdeflate_crc32(global_mimiclib_deflate_unused_u32, ptr, len);
    src->meta.ri += len;
  }
  return NULL;
}

const char*  //
mimic_deflate_gzip_zlib_decode(wuffs_base__io_buffer* dst,
                               wuffs_base__io_buffer* src,
                               uint32_t wuffs_initialize_flags,
                               uint64_t wlimit,
                               uint64_t rlimit,
                               libdeflate_decompress_func func) {
  struct libdeflate_decompressor* dec = libdeflate_alloc_decompressor();
  if (!dec) {
    return "libdeflate: alloc failed";
  }
  size_t n_dst = 0;
  size_t n_src = 0;
  enum libdeflate_result res =
      (*func)(dec, wuffs_base__io_buffer__reader_pointer(src),
              ((size_t)wuffs_base__u64__min(
                  rlimit, wuffs_base__io_buffer__reader_length(src))),
              wuffs_base__io_buffer__writer_pointer(dst),
              ((size_t)wuffs_base__u64__min(
                  wlimit, wuffs_base__io_buffer__writer_length(dst))),
              &n_src, &n_dst);
  if (res == LIBDEFLATE_SUCCESS) {
    dst->meta.wi += n_dst;
    src->meta.ri += n_src;
  }
  libdeflate_free_decompressor(dec);
  return libdeflate_result_as_const_char_star(res);
}

const char*  //
mimic_deflate_decode(wuffs_base__io_buffer* dst,
                     wuffs_base__io_buffer* src,
                     uint32_t wuffs_initialize_flags,
                     uint64_t wlimit,
                     uint64_t rlimit) {
  return mimic_deflate_gzip_zlib_decode(dst, src, wuffs_initialize_flags,
                                        wlimit, rlimit,
                                        &libdeflate_deflate_decompress_ex);
}

const char*  //
mimic_gzip_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  return mimic_deflate_gzip_zlib_decode(dst, src, wuffs_initialize_flags,
                                        wlimit, rlimit,
                                        &libdeflate_gzip_decompress_ex);
}

const char*  //
mimic_zlib_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  return mimic_deflate_gzip_zlib_decode(dst, src, wuffs_initialize_flags,
                                        wlimit, rlimit,
                                        &libdeflate_zlib_decompress_ex);
}

const char*  //
mimic_zlib_decode_with_dictionary(wuffs_base__io_buffer* dst,
                                  wuffs_base__io_buffer* src,
                                  wuffs_base__slice_u8 dictionary) {
  return "libdeflate does not implement zlib dictionaries";
}

// -------------------------------- WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_ZLIB
#elif defined(WUFFS_MIMICLIB_USE_MINIZ_INSTEAD_OF_ZLIB)
#include "/path/to/your/copy/of/github.com/richgel999/miniz/miniz_tinfl.c"

// We deliberately do not define the
// WUFFS_MIMICLIB_DEFLATE_DOES_NOT_SUPPORT_STREAMING macro.

#define WUFFS_MIMICLIB_ZLIB_DOES_NOT_SUPPORT_DICTIONARIES 1

const char*  //
mimic_bench_adler32(wuffs_base__io_buffer* dst,
                    wuffs_base__io_buffer* src,
                    uint32_t wuffs_initialize_flags,
                    uint64_t wlimit,
                    uint64_t rlimit) {
  return "miniz does not independently compute Adler32";
}

const char*  //
mimic_bench_crc32_ieee(wuffs_base__io_buffer* dst,
                       wuffs_base__io_buffer* src,
                       uint32_t wuffs_initialize_flags,
                       uint64_t wlimit,
                       uint64_t rlimit) {
  return "miniz does not implement CRC32/IEEE";
}

const char*  //
mimic_deflate_zlib_decode(wuffs_base__io_buffer* dst,
                          wuffs_base__io_buffer* src,
                          uint32_t wuffs_initialize_flags,
                          uint64_t wlimit,
                          uint64_t rlimit,
                          bool deflate_instead_of_zlib) {
  if ((wlimit < UINT64_MAX) || (rlimit < UINT64_MAX)) {
    // Supporting this would probably mean using tinfl_decompress instead of
    // the simpler tinfl_decompress_mem_to_mem function.
    return "unsupported I/O limit";
  }
  int flags = 0;
  if (!deflate_instead_of_zlib) {
    flags |= TINFL_FLAG_PARSE_ZLIB_HEADER;
  }
  size_t n = tinfl_decompress_mem_to_mem(
      dst->data.ptr + dst->meta.wi, dst->data.len - dst->meta.wi,
      src->data.ptr + src->meta.ri, src->meta.wi - src->meta.ri, flags);
  if (n == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED) {
    return "TINFL_DECOMPRESS_MEM_TO_MEM_FAILED";
  }
  dst->meta.wi += n;
  src->meta.ri = src->meta.wi;
  return NULL;
}

const char*  //
mimic_deflate_decode(wuffs_base__io_buffer* dst,
                     wuffs_base__io_buffer* src,
                     uint32_t wuffs_initialize_flags,
                     uint64_t wlimit,
                     uint64_t rlimit) {
  return mimic_deflate_zlib_decode(dst, src, wuffs_initialize_flags, wlimit,
                                   rlimit, true);
}

const char*  //
mimic_gzip_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  return "miniz does not implement gzip";
}

const char*  //
mimic_zlib_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  return mimic_deflate_zlib_decode(dst, src, wuffs_initialize_flags, wlimit,
                                   rlimit, false);
}

const char*  //
mimic_zlib_decode_with_dictionary(wuffs_base__io_buffer* dst,
                                  wuffs_base__io_buffer* src,
                                  wuffs_base__slice_u8 dictionary) {
  return "miniz does not implement zlib dictionaries";
}

// -------------------------------- WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_ZLIB
#else
#include "zlib.h"

// We deliberately do not define the
// WUFFS_MIMICLIB_DEFLATE_DOES_NOT_SUPPORT_STREAMING macro.

// We deliberately do not define the
// WUFFS_MIMICLIB_ZLIB_DOES_NOT_SUPPORT_DICTIONARIES macro.

uint32_t global_mimiclib_deflate_unused_u32;

const char*  //
mimic_bench_adler32(wuffs_base__io_buffer* dst,
                    wuffs_base__io_buffer* src,
                    uint32_t wuffs_initialize_flags,
                    uint64_t wlimit,
                    uint64_t rlimit) {
  global_mimiclib_deflate_unused_u32 = 0;
  while (src->meta.ri < src->meta.wi) {
    uint8_t* ptr = src->data.ptr + src->meta.ri;
    size_t len = src->meta.wi - src->meta.ri;
    if (len > 0x7FFFFFFF) {
      return "src length is too large";
    } else if (len > rlimit) {
      len = rlimit;
    }
    global_mimiclib_deflate_unused_u32 =
        adler32(global_mimiclib_deflate_unused_u32, ptr, len);
    src->meta.ri += len;
  }
  return NULL;
}

const char*  //
mimic_bench_crc32_ieee(wuffs_base__io_buffer* dst,
                       wuffs_base__io_buffer* src,
                       uint32_t wuffs_initialize_flags,
                       uint64_t wlimit,
                       uint64_t rlimit) {
  global_mimiclib_deflate_unused_u32 = 0;
  while (src->meta.ri < src->meta.wi) {
    uint8_t* ptr = src->data.ptr + src->meta.ri;
    size_t len = src->meta.wi - src->meta.ri;
    if (len > 0x7FFFFFFF) {
      return "src length is too large";
    } else if (len > rlimit) {
      len = rlimit;
    }
    global_mimiclib_deflate_unused_u32 =
        crc32(global_mimiclib_deflate_unused_u32, ptr, len);
    src->meta.ri += len;
  }
  return NULL;
}

typedef enum {
  zlib_flavor_raw,
  zlib_flavor_gzip,
  zlib_flavor_zlib,
} zlib_flavor;

const char*  //
mimic_deflate_gzip_zlib_decode(wuffs_base__io_buffer* dst,
                               wuffs_base__io_buffer* src,
                               wuffs_base__slice_u8* dictionary,
                               uint64_t wlimit,
                               uint64_t rlimit,
                               zlib_flavor flavor) {
  const char* ret = NULL;
  if (dst->data.len > UINT_MAX) {
    ret = "dst length is too large";
    goto cleanup0;
  }
  if (src->data.len > UINT_MAX) {
    ret = "src length is too large";
    goto cleanup0;
  }

  // See inflateInit2 in the zlib manual, or in zlib.h, for details about how
  // the window_bits int also encodes the wire format wrapper.
  int window_bits = 0;
  switch (flavor) {
    case zlib_flavor_raw:
      window_bits = -15;
      break;
    case zlib_flavor_gzip:
      window_bits = +15 | 16;
      break;
    case zlib_flavor_zlib:
      window_bits = +15;
      break;
    default:
      ret = "invalid zlib_flavor";
      goto cleanup0;
  }
  z_stream z = {0};
  int ii2_err = inflateInit2(&z, window_bits);
  if (ii2_err != Z_OK) {
    ret = "inflateInit2 failed";
    goto cleanup0;
  }

  while (true) {
    z.next_in = src->data.ptr + src->meta.ri;
    z.avail_in = src->meta.wi - src->meta.ri;
    if (z.avail_in > rlimit) {
      z.avail_in = rlimit;
    }
    uInt initial_avail_in = z.avail_in;

    z.next_out = dst->data.ptr + dst->meta.wi;
    z.avail_out = dst->data.len - dst->meta.wi;
    if (z.avail_out > wlimit) {
      z.avail_out = wlimit;
    }
    uInt initial_avail_out = z.avail_out;

    // TODO: s/Z_NO_FLUSH/Z_SYNC_FLUSH/ more closely matches Wuffs' behavior.
    int i_err = inflate(&z, Z_NO_FLUSH);

    if (initial_avail_in < z.avail_in) {
      ret = "inconsistent avail_in";
      goto cleanup1;
    }
    src->meta.ri += initial_avail_in - z.avail_in;

    if (initial_avail_out < z.avail_out) {
      ret = "inconsistent avail_out";
      goto cleanup1;
    }
    dst->meta.wi += initial_avail_out - z.avail_out;

    if (i_err == Z_STREAM_END) {
      break;
    } else if (i_err == Z_NEED_DICT) {
      if (dictionary) {
        int isd_err =
            inflateSetDictionary(&z, dictionary->ptr, dictionary->len);
        if (isd_err != Z_OK) {
          ret = "inflateSetDictionary failed";
          goto cleanup1;
        }
        dictionary = NULL;
        continue;
      }
      ret = "inflate failed (need dict)";
      goto cleanup1;
    } else if (i_err == Z_DATA_ERROR) {
      ret = "inflate failed (data error)";
      goto cleanup1;
    } else if (i_err != Z_OK) {
      ret = "inflate failed";
      goto cleanup1;
    }
  }

cleanup1:;
  int ie_err = inflateEnd(&z);
  if ((ie_err != Z_OK) && !ret) {
    ret = "inflateEnd failed";
  }

cleanup0:;
  return ret;
}

const char*  //
mimic_deflate_decode(wuffs_base__io_buffer* dst,
                     wuffs_base__io_buffer* src,
                     uint32_t wuffs_initialize_flags,
                     uint64_t wlimit,
                     uint64_t rlimit) {
  return mimic_deflate_gzip_zlib_decode(dst, src, NULL, wlimit, rlimit,
                                        zlib_flavor_raw);
}

const char*  //
mimic_gzip_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  return mimic_deflate_gzip_zlib_decode(dst, src, NULL, wlimit, rlimit,
                                        zlib_flavor_gzip);
}

const char*  //
mimic_zlib_decode(wuffs_base__io_buffer* dst,
                  wuffs_base__io_buffer* src,
                  uint32_t wuffs_initialize_flags,
                  uint64_t wlimit,
                  uint64_t rlimit) {
  return mimic_deflate_gzip_zlib_decode(dst, src, NULL, wlimit, rlimit,
                                        zlib_flavor_zlib);
}

const char*  //
mimic_zlib_decode_with_dictionary(wuffs_base__io_buffer* dst,
                                  wuffs_base__io_buffer* src,
                                  wuffs_base__slice_u8 dictionary) {
  return mimic_deflate_gzip_zlib_decode(dst, src, &dictionary, UINT64_MAX,
                                        UINT64_MAX, zlib_flavor_zlib);
}

#endif
// -------------------------------- WUFFS_MIMICLIB_USE_XXX_INSTEAD_OF_ZLIB
