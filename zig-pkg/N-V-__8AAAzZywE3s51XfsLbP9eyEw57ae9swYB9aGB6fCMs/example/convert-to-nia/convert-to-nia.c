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

/*
convert-to-nia converts an image from stdin (e.g. in the BMP, GIF, JPEG or PNG
format) to stdout (in the NIA/NIE format).

See the "const char* g_usage" string below for details.

An equivalent program (using the Chromium image codecs) is at:
https://chromium-review.googlesource.com/c/chromium/src/+/2210331

An equivalent program (using the Skia image codecs) is at:
https://skia-review.googlesource.com/c/skia/+/290618
*/

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__STATIC_FUNCTIONS macro is optional, but when
// combined with WUFFS_IMPLEMENTATION, it demonstrates making all of Wuffs'
// functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#define WUFFS_CONFIG__STATIC_FUNCTIONS

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__BMP
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ETC2
#define WUFFS_CONFIG__MODULE__GIF
#define WUFFS_CONFIG__MODULE__JPEG
#define WUFFS_CONFIG__MODULE__NETPBM
#define WUFFS_CONFIG__MODULE__NIE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__QOI
#define WUFFS_CONFIG__MODULE__TARGA
#define WUFFS_CONFIG__MODULE__THUMBHASH
#define WUFFS_CONFIG__MODULE__VP8
#define WUFFS_CONFIG__MODULE__WBMP
#define WUFFS_CONFIG__MODULE__WEBP
#define WUFFS_CONFIG__MODULE__ZLIB

// Defining the WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST (and the
// associated ETC__ALLOW_FOO) macros are optional, but can lead to smaller
// programs (in terms of binary size). By default (without these macros),
// Wuffs' standard library can decode images to a variety of pixel formats,
// such as BGR_565, BGRA_PREMUL or RGBA_NONPREMUL. The destination pixel format
// is selectable at runtime. Using these macros essentially makes the selection
// at compile time, by narrowing the list of supported destination pixel
// formats. The FOO in ETC__ALLOW_FOO should match the pixel format passed (as
// part of the wuffs_base__image_config argument) to the decode_frame method.
//
// If using the wuffs_aux C++ API, without overriding the SelectPixfmt method,
// the implicit destination pixel format is BGRA_PREMUL.
#define WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST
#define WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_BGRA_NONPREMUL

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

// ----

#if defined(__linux__)
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#define WUFFS_EXAMPLE_USE_SECCOMP
#endif

#define TRY(error_msg)         \
  do {                         \
    const char* z = error_msg; \
    if (z) {                   \
      return z;                \
    }                          \
  } while (false)

static const char* g_usage =
    "Usage: convert-to-nia -flags < src.img > dst.nia\n"
    "\n"
    "Flags:\n"
    "    -1      -first-frame-only\n"
    "    -p      -output-netpbm\n"
    "            -fail-if-unsandboxed\n"
    "\n"
    "convert-to-nia converts an image from stdin (e.g. in the BMP, GIF, JPEG\n"
    "or PNG format) to stdout (in the NIA format, or in the NIE or PPM/PGM\n"
    "format if the -1 or -p flag is given; you cannot specify both flags).\n"
    "\n"
    "NIA/NIE is a trivial animated/still image file format, specified at\n"
    "https://github.com/google/wuffs/blob/main/doc/spec/nie-spec.md\n"
    "\n"
    "PPM (color) and PGM (gray) are also trivial still image file formats.\n"
    "They do not support alpha or animation. Using -p means that this program\n"
    "outputs the same format as /usr/bin/djpeg\n"
    "\n"
    "The -fail-if-unsandboxed flag causes the program to exit if it does not\n"
    "self-impose a sandbox. On Linux, it self-imposes a SECCOMP_MODE_STRICT\n"
    "sandbox, regardless of whether this flag was set.";

// ----

#define STDIN_FD 0
#define STDOUT_FD 1
#define STDERR_FD 2

bool g_sandboxed = false;

wuffs_base__pixel_buffer g_pixbuf = {0};
wuffs_base__io_buffer g_src = {0};

wuffs_base__slice_u8 g_pixbuf_slice = {0};
wuffs_base__slice_u8 g_pixbuf_backup_slice = {0};
wuffs_base__slice_u8 g_workbuf_slice = {0};

wuffs_base__image_config g_image_config = {0};
wuffs_base__frame_config g_frame_config = {0};
int32_t g_fourcc = 0;
uint32_t g_width = 0;
uint32_t g_height = 0;
bool g_pixfmt_is_gray = false;
uint32_t g_num_animation_loops = 0;
uint64_t g_num_printed_frames = 0;

wuffs_base__image_decoder* g_image_decoder = NULL;
union {
  wuffs_bmp__decoder bmp;
  wuffs_etc2__decoder etc2;
  wuffs_gif__decoder gif;
  wuffs_jpeg__decoder jpeg;
  wuffs_netpbm__decoder netpbm;
  wuffs_nie__decoder nie;
  wuffs_png__decoder png;
  wuffs_qoi__decoder qoi;
  wuffs_targa__decoder targa;
  wuffs_thumbhash__decoder thumbhash;
  wuffs_wbmp__decoder wbmp;
  wuffs_webp__decoder webp;
} g_potential_decoders;

wuffs_crc32__ieee_hasher g_digest_hasher;

// ----

#define BYTES_PER_PIXEL 4

#ifndef MAX_DIMENSION
#define MAX_DIMENSION 65535
#endif

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (64 * 1024)
#endif

#ifndef WORKBUF_ARRAY_SIZE
#define WORKBUF_ARRAY_SIZE (256 * 1024 * 1024)
#endif

#ifndef PIXBUF_ARRAY_SIZE
#define PIXBUF_ARRAY_SIZE (256 * 1024 * 1024)
#endif

// Uncomment this #define (or use a C compiler flag) to decode very large
// images like test/3pdata/blinksuite/large-size-image-crash.jpeg which is
// 48010 * 16173 = 776_465730 pixels, roughly 2.9 GiB at 4 bytes per pixel.
//
// This requires calling malloc (before self-imposing a SECCOMP_MODE_STRICT
// sandbox). By default (without this #define), the buffers are statically
// allocated and do not require syscalls.
//
// #define ALLOW_GIGABYTES_OF_PIXEL_BUFFERS 1

uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE] = {0};
#if !defined(ALLOW_GIGABYTES_OF_PIXEL_BUFFERS)
uint8_t g_workbuf_array[WORKBUF_ARRAY_SIZE] = {0};
uint8_t g_pixbuf_array[PIXBUF_ARRAY_SIZE] = {0};
#endif

// ----

struct {
  int remaining_argc;
  char** remaining_argv;

  bool fail_if_unsandboxed;
  bool first_frame_only;
  bool output_crc32_digest;
  bool output_netpbm;
} g_flags = {0};

const char*  //
parse_flags(int argc, char** argv) {
  int c = (argc > 0) ? 1 : 0;  // Skip argv[0], the program name.
  for (; c < argc; c++) {
    char* arg = argv[c];
    if (*arg++ != '-') {
      break;
    }

    // A double-dash "--foo" is equivalent to a single-dash "-foo". As special
    // cases, a bare "-" is not a flag (some programs may interpret it as
    // stdin) and a bare "--" means to stop parsing flags.
    if (*arg == '\x00') {
      break;
    } else if (*arg == '-') {
      arg++;
      if (*arg == '\x00') {
        c++;
        break;
      }
    }

    if (!strcmp(arg, "fail-if-unsandboxed")) {
      g_flags.fail_if_unsandboxed = true;
      continue;
    }
    if (!strcmp(arg, "1") || !strcmp(arg, "first-frame-only")) {
      g_flags.first_frame_only = true;
      continue;
    }
    if (!strcmp(arg, "output-crc32-digest")) {
      if (g_flags.output_crc32_digest || g_flags.output_netpbm) {
        return "main: multiple --output-etc flags";
      }
      g_flags.output_crc32_digest = true;
      continue;
    }
    if (!strcmp(arg, "p") || !strcmp(arg, "output-netpbm")) {
      if (g_flags.output_crc32_digest || g_flags.output_netpbm) {
        return "main: multiple --output-etc flags";
      }
      g_flags.output_netpbm = true;
      continue;
    }

    return g_usage;
  }

  if (g_flags.first_frame_only && g_flags.output_netpbm) {
    return g_usage;
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return NULL;
}

// ----

// ignore_return_value suppresses errors from -Wall -Werror.
static void  //
ignore_return_value(int ignored) {}

ssize_t  //
write_to_stdout(const void* ptr, size_t len) {
  if (len > SSIZE_MAX) {
    return -EFBIG;
  } else if (!g_flags.output_crc32_digest) {
    return write(STDOUT_FD, ptr, len);
  }
  wuffs_crc32__ieee_hasher__update(
      &g_digest_hasher, wuffs_base__make_slice_u8((uint8_t*)ptr, len));
  return (ssize_t)len;
}

const char*  //
read_more_src() {
  if (g_src.meta.closed) {
    return "main: unexpected end of file";
  }
  wuffs_base__io_buffer__compact(&g_src);
  if (g_src.meta.wi == g_src.data.len) {
    return "main: internal error: no I/O progress possible";
  }
  ssize_t n = read(STDIN_FD, g_src.data.ptr + g_src.meta.wi,
                   g_src.data.len - g_src.meta.wi);
  if (n > 0) {
    g_src.meta.wi += n;
  } else if (errno == 0) {
    if (n < 0) {
      return "main: unexpected negative-count read";
    }
    g_src.meta.closed = true;
  } else if (errno != EINTR) {
    return strerror(errno);
  }
  return NULL;
}

const char*  //
load_image_type() {
  g_fourcc = 0;
  while (true) {
    g_fourcc = wuffs_base__magic_number_guess_fourcc(
        wuffs_base__io_buffer__reader_slice(&g_src), g_src.meta.closed);
    if ((g_fourcc >= 0) ||
        (wuffs_base__io_buffer__reader_length(&g_src) == g_src.data.len)) {
      break;
    }
    TRY(read_more_src());
  }
  return NULL;
}

const char*  //
initialize_image_decoder() {
  wuffs_base__status status;
  switch (g_fourcc) {
    case WUFFS_BASE__FOURCC__BMP:
      status = wuffs_bmp__decoder__initialize(
          &g_potential_decoders.bmp, sizeof g_potential_decoders.bmp,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_bmp__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.bmp);
      return NULL;

    case WUFFS_BASE__FOURCC__ETC2:
      status = wuffs_etc2__decoder__initialize(
          &g_potential_decoders.etc2, sizeof g_potential_decoders.etc2,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_etc2__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.etc2);
      return NULL;

    case WUFFS_BASE__FOURCC__GIF:
      status = wuffs_gif__decoder__initialize(
          &g_potential_decoders.gif, sizeof g_potential_decoders.gif,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_gif__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.gif);
      return NULL;

    case WUFFS_BASE__FOURCC__JPEG:
      status = wuffs_jpeg__decoder__initialize(
          &g_potential_decoders.jpeg, sizeof g_potential_decoders.jpeg,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_jpeg__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.jpeg);
      return NULL;

    case WUFFS_BASE__FOURCC__NIE:
      status = wuffs_nie__decoder__initialize(
          &g_potential_decoders.nie, sizeof g_potential_decoders.nie,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_nie__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.nie);
      return NULL;

    case WUFFS_BASE__FOURCC__NPBM:
      status = wuffs_netpbm__decoder__initialize(
          &g_potential_decoders.netpbm, sizeof g_potential_decoders.netpbm,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_netpbm__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.netpbm);
      return NULL;

    case WUFFS_BASE__FOURCC__PNG:
      status = wuffs_png__decoder__initialize(
          &g_potential_decoders.png, sizeof g_potential_decoders.png,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_png__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.png);
      return NULL;

    case WUFFS_BASE__FOURCC__QOI:
      status = wuffs_qoi__decoder__initialize(
          &g_potential_decoders.qoi, sizeof g_potential_decoders.qoi,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_qoi__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.qoi);
      return NULL;

    case WUFFS_BASE__FOURCC__TGA:
      status = wuffs_targa__decoder__initialize(
          &g_potential_decoders.targa, sizeof g_potential_decoders.targa,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_targa__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.targa);
      return NULL;

    case WUFFS_BASE__FOURCC__TH:
      status = wuffs_thumbhash__decoder__initialize(
          &g_potential_decoders.thumbhash,
          sizeof g_potential_decoders.thumbhash, WUFFS_VERSION,
          WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_thumbhash__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.thumbhash);
      return NULL;

    case WUFFS_BASE__FOURCC__WBMP:
      status = wuffs_wbmp__decoder__initialize(
          &g_potential_decoders.wbmp, sizeof g_potential_decoders.wbmp,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_wbmp__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.wbmp);
      return NULL;

    case WUFFS_BASE__FOURCC__WEBP:
      status = wuffs_webp__decoder__initialize(
          &g_potential_decoders.webp, sizeof g_potential_decoders.webp,
          WUFFS_VERSION, WUFFS_INITIALIZE__DEFAULT_OPTIONS);
      TRY(wuffs_base__status__message(&status));
      g_image_decoder =
          wuffs_webp__decoder__upcast_as__wuffs_base__image_decoder(
              &g_potential_decoders.webp);
      return NULL;
  }
  return "main: unsupported file format";
}

const char*  //
advance_for_redirect() {
  wuffs_base__io_buffer empty = wuffs_base__empty_io_buffer();
  wuffs_base__more_information minfo = wuffs_base__empty_more_information();
  wuffs_base__status status = wuffs_base__image_decoder__tell_me_more(
      g_image_decoder, &empty, &minfo, &g_src);
  if (status.repr != NULL) {
    return wuffs_base__status__message(&status);
  } else if (minfo.flavor !=
             WUFFS_BASE__MORE_INFORMATION__FLAVOR__IO_REDIRECT) {
    return "main: unsupported file format";
  }
  g_fourcc =
      (int32_t)(wuffs_base__more_information__io_redirect__fourcc(&minfo));
  if (g_fourcc <= 0) {
    return "main: unsupported file format";
  }

  // Advance g_src's reader_position to pos.
  uint64_t pos =
      wuffs_base__more_information__io_redirect__range(&minfo).min_incl;
  if (pos < wuffs_base__io_buffer__reader_position(&g_src)) {
    // Redirects must go forward.
    return "main: unsupported file format";
  }
  while (true) {
    uint64_t relative_pos =
        pos - wuffs_base__io_buffer__reader_position(&g_src);
    if (relative_pos <= (g_src.meta.wi - g_src.meta.ri)) {
      g_src.meta.ri += relative_pos;
      break;
    }
    g_src.meta.ri = g_src.meta.wi;
    TRY(read_more_src());
  }
  return NULL;
}

const char*  //
load_image_config() {
  bool redirected = false;
redirect:
  TRY(initialize_image_decoder());

  // Decode the wuffs_base__image_config.
  while (true) {
    wuffs_base__status status = wuffs_base__image_decoder__decode_image_config(
        g_image_decoder, &g_image_config, &g_src);
    if (status.repr == NULL) {
      break;
    } else if (status.repr == wuffs_base__note__i_o_redirect) {
      if (redirected) {
        return "main: unsupported file format";
      }
      redirected = true;
      TRY(advance_for_redirect());
      goto redirect;
    } else if (status.repr != wuffs_base__suspension__short_read) {
      return wuffs_base__status__message(&status);
    }
    TRY(read_more_src());
  }

  // Read the dimensions.
  uint32_t w = wuffs_base__pixel_config__width(&g_image_config.pixcfg);
  uint32_t h = wuffs_base__pixel_config__height(&g_image_config.pixcfg);
  if ((w > MAX_DIMENSION) || (h > MAX_DIMENSION)) {
    return "main: image is too large";
  }
  g_width = w;
  g_height = h;
  switch (wuffs_base__pixel_config__pixel_format(&g_image_config.pixcfg).repr) {
    case WUFFS_BASE__PIXEL_FORMAT__Y:
    case WUFFS_BASE__PIXEL_FORMAT__Y_16LE:
    case WUFFS_BASE__PIXEL_FORMAT__Y_16BE:
      g_pixfmt_is_gray = true;
      break;
    default:
      g_pixfmt_is_gray = false;
      break;
  }

  // Override the image's native pixel format to be BGRA_NONPREMUL.
  wuffs_base__pixel_config__set(&g_image_config.pixcfg,
                                WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL,
                                WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);

  // Configure the work buffer.
  uint64_t workbuf_len =
      wuffs_base__image_decoder__workbuf_len(g_image_decoder).max_incl;
  if (g_workbuf_slice.len < workbuf_len) {
    return "main: image is too large (to configure work buffer)";
  }
  g_workbuf_slice.len = workbuf_len;

  // Configure the pixel buffer and (if there's capacity) its backup buffer.
  uint64_t num_pixels = ((uint64_t)w) * ((uint64_t)h);
  if (g_pixbuf_slice.len < (num_pixels * BYTES_PER_PIXEL)) {
    return "main: image is too large (to configure pixel buffer)";
  }
  size_t old_pixbuf_slice_len = g_pixbuf_slice.len;
  g_pixbuf_slice.len = num_pixels * BYTES_PER_PIXEL;
  size_t pixbuf_array_remaining = old_pixbuf_slice_len - g_pixbuf_slice.len;
  if (pixbuf_array_remaining >= g_pixbuf_slice.len) {
    g_pixbuf_backup_slice.ptr = g_pixbuf_slice.ptr + g_pixbuf_slice.len;
    g_pixbuf_backup_slice.len = g_pixbuf_slice.len;
  }

  // Configure the wuffs_base__pixel_buffer struct.
  wuffs_base__status status = wuffs_base__pixel_buffer__set_from_slice(
      &g_pixbuf, &g_image_config.pixcfg, g_pixbuf_slice);
  TRY(wuffs_base__status__message(&status));

  wuffs_base__table_u8 tab = wuffs_base__pixel_buffer__plane(&g_pixbuf, 0);
  if ((tab.width != (g_width * BYTES_PER_PIXEL)) || (tab.height != g_height)) {
    return "main: inconsistent pixel buffer dimensions";
  }

  return NULL;
}

void  //
fill_rectangle(wuffs_base__rect_ie_u32 rect,
               wuffs_base__color_u32_argb_premul color) {
  if (rect.max_excl_x > g_width) {
    rect.max_excl_x = g_width;
  }
  if (rect.max_excl_y > g_height) {
    rect.max_excl_y = g_height;
  }
  uint32_t nonpremul =
      wuffs_base__color_u32_argb_premul__as__color_u32_argb_nonpremul(color);
  wuffs_base__table_u8 tab = wuffs_base__pixel_buffer__plane(&g_pixbuf, 0);

  for (uint32_t y = rect.min_incl_y; y < rect.max_excl_y; y++) {
    uint8_t* p =
        tab.ptr + (y * tab.stride) + (rect.min_incl_x * BYTES_PER_PIXEL);
    for (uint32_t x = rect.min_incl_x; x < rect.max_excl_x; x++) {
      wuffs_base__poke_u32le__no_bounds_check(p, nonpremul);
      p += BYTES_PER_PIXEL;
    }
  }
}

void  //
print_nix_header(uint32_t magic_u32le) {
  static const uint32_t version1_bn4_u32le = 0x346E62FF;
  uint8_t data[16];
  wuffs_base__poke_u32le__no_bounds_check(data + 0x00, magic_u32le);
  wuffs_base__poke_u32le__no_bounds_check(data + 0x04, version1_bn4_u32le);
  wuffs_base__poke_u32le__no_bounds_check(data + 0x08, g_width);
  wuffs_base__poke_u32le__no_bounds_check(data + 0x0C, g_height);
  ignore_return_value(write_to_stdout(&data[0], 16));
}

void  //
print_netpbm_header() {
  char data[256];
  int n = snprintf(data, sizeof(data), "P%c\n%" PRIu32 " %" PRIu32 "\n255\n",
                   (g_pixfmt_is_gray ? '5' : '6'), g_width, g_height);
  ignore_return_value(write_to_stdout(&data[0], n));
}

void  //
print_nia_duration(wuffs_base__flicks duration) {
  uint8_t data[8];
  wuffs_base__poke_u64le__no_bounds_check(data + 0x00, duration);
  ignore_return_value(write_to_stdout(&data[0], 8));
}

void  //
print_nie_frame() {
  g_num_printed_frames++;
  print_nix_header(0x45AFC36E);  // "nïE" as a u32le.
  wuffs_base__table_u8 tab = wuffs_base__pixel_buffer__plane(&g_pixbuf, 0);
  if (tab.width == tab.stride) {
    ignore_return_value(write_to_stdout(tab.ptr, tab.width * tab.height));
  } else {
    for (size_t y = 0; y < tab.height; y++) {
      ignore_return_value(
          write_to_stdout(tab.ptr + (y * tab.stride), tab.width));
    }
  }
}

void  //
print_netpbm_frame() {
  g_num_printed_frames++;
  uint8_t data[4096];
  size_t o = 0;
  const size_t o_increment = g_pixfmt_is_gray ? 1 : 3;
  wuffs_base__table_u8 tab = wuffs_base__pixel_buffer__plane(&g_pixbuf, 0);
  for (size_t y = 0; y < tab.height; y++) {
    const uint8_t* row = tab.ptr + (y * tab.stride);
    for (size_t x = 0; x < tab.width; x += 4) {
      data[o + 0] = row[x + 2];
      data[o + 1] = row[x + 1];
      data[o + 2] = row[x + 0];
      o += o_increment;
      if ((o + 3) > sizeof(data)) {
        ignore_return_value(write_to_stdout(&data[0], o));
        o = 0;
      }
    }
  }
  if (o > 0) {
    ignore_return_value(write_to_stdout(&data[0], o));
    o = 0;
  }
}

void  //
print_nia_padding() {
  if (g_width & g_height & 1) {
    uint8_t data[4];
    wuffs_base__poke_u32le__no_bounds_check(data + 0x00, 0);
    ignore_return_value(write_to_stdout(&data[0], 4));
  }
}

void  //
print_nia_footer() {
  // For still (non-animated) images, the number of animation loops has no
  // practical effect: the pixels on screen do not change over time regardless
  // of its value. In the wire format encoding, there might be no explicit
  // "number of animation loops" value listed in the source bytes. Various
  // codec implementations may therefore choose an implicit default of 0 ("loop
  // forever") or 1 ("loop exactly once"). Either is equally valid.
  //
  // However, when comparing the output of this convert-to-NIA program (backed
  // by Wuffs' image codecs) with other convert-to-NIA programs, it is useful
  // to canonicalize still images' "number of animation loops" to 0.
  uint32_t n = g_num_animation_loops;
  if (g_num_printed_frames <= 1) {
    n = 0;
  }

  uint8_t data[8];
  wuffs_base__poke_u32le__no_bounds_check(data + 0x00, n);
  wuffs_base__poke_u32le__no_bounds_check(data + 0x04, 0x80000000);
  ignore_return_value(write_to_stdout(&data[0], 8));
}

const char*  //
convert_frames() {
  wuffs_base__flicks total_duration = 0;
  while (true) {
    // Decode the wuffs_base__frame_config.
    while (true) {
      wuffs_base__status dfc_status =
          wuffs_base__image_decoder__decode_frame_config(
              g_image_decoder, &g_frame_config, &g_src);
      if (dfc_status.repr == NULL) {
        break;
      } else if (dfc_status.repr == wuffs_base__note__end_of_data) {
        return NULL;
      } else if (dfc_status.repr != wuffs_base__suspension__short_read) {
        return wuffs_base__status__message(&dfc_status);
      }
      TRY(read_more_src());
    }

    wuffs_base__flicks duration =
        wuffs_base__frame_config__duration(&g_frame_config);
    if (duration < 0) {
      return "main: animation frame duration is negative";
    } else if (total_duration > (INT64_MAX - duration)) {
      return "main: animation frame duration overflow";
    }
    total_duration += duration;

    if (wuffs_base__frame_config__index(&g_frame_config) == 0) {
      fill_rectangle(
          wuffs_base__pixel_config__bounds(&g_image_config.pixcfg),
          wuffs_base__frame_config__background_color(&g_frame_config));
    }

    switch (wuffs_base__frame_config__disposal(&g_frame_config)) {
      case WUFFS_BASE__ANIMATION_DISPOSAL__RESTORE_PREVIOUS: {
        if (g_pixbuf_slice.len != g_pixbuf_backup_slice.len) {
          return "main: image is too large (to configure pixel backup buffer)";
        }
        memcpy(g_pixbuf_backup_slice.ptr, g_pixbuf_slice.ptr,
               g_pixbuf_slice.len);
        break;
      }
    }

    // Decode the frame (the pixels).
    wuffs_base__status df_status;
    const char* decode_frame_io_error_message = NULL;
    while (true) {
      df_status = wuffs_base__image_decoder__decode_frame(
          g_image_decoder, &g_pixbuf, &g_src,
          wuffs_base__frame_config__overwrite_instead_of_blend(&g_frame_config)
              ? WUFFS_BASE__PIXEL_BLEND__SRC
              : WUFFS_BASE__PIXEL_BLEND__SRC_OVER,
          g_workbuf_slice, NULL);
      if (df_status.repr != wuffs_base__suspension__short_read) {
        break;
      }
      decode_frame_io_error_message = read_more_src();
      if (decode_frame_io_error_message != NULL) {
        // Neuter the "short read" df_status so that convert_frames returns the
        // I/O error message instead.
        df_status.repr = NULL;
        break;
      }
    }

    // Update g_num_animation_loops. It's rare in practice, but the animation
    // loop count can change over the course of decoding an image file.
    //
    // This program updates the global once per frame (even though the Wuffs
    // API also lets you call wuffs_base__image_decoder__num_animation_loops
    // just once, after the decoding is complete) to more closely match the
    // Chromium web browser. This program emits (via print_nia_footer) the
    // value from the final animation frame's update.
    //
    // Chromium image decoding uses two passes. (Wuffs' API lets you use a
    // single pass but Chromium also wraps other libraries). Its first pass
    // counts the number of animation frames (call it N). The second pass
    // decodes exactly N frames. In particular, if the animation loop count
    // would change between the end of frame N and the end of the file then
    // Chromium's design will not pick up that change, even if it's a valid
    // change in terms of the image file format.
    //
    // Specifically, for the test/data/artificial-gif/multiple-loop-counts.gif
    // file this program emits 31 (0x1F) to match Chromium, even though the
    // file arguably has a 41 (0x29) loop count after a complete decode.
    g_num_animation_loops =
        wuffs_base__image_decoder__num_animation_loops(g_image_decoder);

    // Print a complete NIE frame (and surrounding bytes, for NIA).
    if (g_flags.output_netpbm) {
      print_netpbm_frame();
    } else {
      if (!g_flags.first_frame_only) {
        print_nia_duration(total_duration);
      }
      print_nie_frame();
      if (!g_flags.first_frame_only) {
        print_nia_padding();
      }
    }

    // Return early if there was an error decoding the frame.
    if (df_status.repr != NULL) {
      return wuffs_base__status__message(&df_status);
    } else if (decode_frame_io_error_message != NULL) {
      return decode_frame_io_error_message;
    } else if (g_flags.first_frame_only || g_flags.output_netpbm) {
      return NULL;
    }

    // Dispose the frame.
    switch (wuffs_base__frame_config__disposal(&g_frame_config)) {
      case WUFFS_BASE__ANIMATION_DISPOSAL__RESTORE_BACKGROUND: {
        fill_rectangle(
            wuffs_base__frame_config__bounds(&g_frame_config),
            wuffs_base__frame_config__background_color(&g_frame_config));
        break;
      }
      case WUFFS_BASE__ANIMATION_DISPOSAL__RESTORE_PREVIOUS: {
        if (g_pixbuf_slice.len != g_pixbuf_backup_slice.len) {
          return "main: image is too large (to configure pixel backup buffer)";
        }
        memcpy(g_pixbuf_slice.ptr, g_pixbuf_backup_slice.ptr,
               g_pixbuf_slice.len);
        break;
      }
    }
  }
  return "main: unreachable";
}

const char*  //
main1(int argc, char** argv) {
  TRY(parse_flags(argc, argv));
  if (g_flags.remaining_argc > 0) {
    return "main: bad argument: use \"program < input\", not \"program input\"";
  } else if (g_flags.fail_if_unsandboxed && !g_sandboxed) {
    return "main: unsandboxed";
  }

  if (g_flags.output_crc32_digest) {
    wuffs_base__status status = wuffs_crc32__ieee_hasher__initialize(
        &g_digest_hasher, sizeof g_digest_hasher, WUFFS_VERSION,
        WUFFS_INITIALIZE__DEFAULT_OPTIONS);
    if (status.repr) {
      return wuffs_base__status__message(&status);
    }
  }

  g_src.data.ptr = g_src_buffer_array;
  g_src.data.len = SRC_BUFFER_ARRAY_SIZE;

  TRY(load_image_type());
  TRY(load_image_config());
  if (g_flags.output_netpbm) {
    print_netpbm_header();
  } else if (!g_flags.first_frame_only) {
    print_nix_header(0x41AFC36E);  // "nïA" as a u32le.
  }
  const char* ret = convert_frames();
  if (!g_flags.first_frame_only && !g_flags.output_netpbm) {
    print_nia_footer();
  }
  return ret;
}

int  //
compute_exit_code(const char* status_msg) {
  if (!status_msg) {
    return 0;
  }
  size_t n;
  if (status_msg == g_usage) {
    n = strlen(status_msg);
  } else {
    n = strnlen(status_msg, 2047);
    if (n >= 2047) {
      status_msg = "main: internal error: error message is too long";
      n = strlen(status_msg);
    }
  }
  ignore_return_value(write(STDERR_FD, status_msg, n));
  ignore_return_value(write(STDERR_FD, "\n", 1));
  // Return an exit code of 1 for regular (foreseen) errors, e.g. badly
  // formatted or unsupported input.
  //
  // Return an exit code of 2 for internal (exceptional) errors, e.g. defensive
  // run-time checks found that an internal invariant did not hold.
  //
  // Automated testing, including badly formatted inputs, can therefore
  // discriminate between expected failure (exit code 1) and unexpected failure
  // (other non-zero exit codes). Specifically, exit code 2 for internal
  // invariant violation, exit code 139 (which is 128 + SIGSEGV on x86_64
  // linux) for a segmentation fault (e.g. null pointer dereference).
  return strstr(status_msg, "internal error:") ? 2 : 1;
}

void  //
print_crc32_digest(bool bad) {
  const char* hex = "0123456789abcdef";
  uint32_t hash = wuffs_crc32__ieee_hasher__checksum_u32(&g_digest_hasher);
  char buf[13];
  memcpy(buf + 0, bad ? "BAD " : "OK. ", 4);
  for (int i = 0; i < 8; i++) {
    buf[4 + i] = hex[hash >> 28];
    hash <<= 4;
  }
  buf[12] = '\n';
  const int stdout_fd = 1;
  ignore_return_value(write(stdout_fd, buf, 13));
}

int  //
main(int argc, char** argv) {
#if !defined(ALLOW_GIGABYTES_OF_PIXEL_BUFFERS)
  {
    g_pixbuf_slice =
        wuffs_base__make_slice_u8(&g_pixbuf_array[0], sizeof(g_pixbuf_array));
    g_workbuf_slice =
        wuffs_base__make_slice_u8(&g_workbuf_array[0], sizeof(g_workbuf_array));
  }
#elif !defined(__WORDSIZE) || (__WORDSIZE != 64)
#error "ALLOW_GIGABYTES_OF_PIXEL_BUFFERS requires a word size of 64 bits"
#else
  // Call malloc before we self-impose a SECCOMP_MODE_STRICT sandbox.
  {
    static const size_t four_gigabytes = 0x100000000ul;
    void* p = malloc(four_gigabytes);
    void* w = malloc(four_gigabytes);
    if (!p || !w) {
      return compute_exit_code(
          "could not allocate ALLOW_GIGABYTES_OF_PIXEL_BUFFERS");
    }
    g_pixbuf_slice = wuffs_base__make_slice_u8(p, four_gigabytes);
    g_workbuf_slice = wuffs_base__make_slice_u8(w, four_gigabytes);
  }
#endif  // !defined(ALLOW_GIGABYTES_OF_PIXEL_BUFFERS)

  // ----

#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
  g_sandboxed = true;
#endif

  int exit_code = compute_exit_code(main1(argc, argv));
  if (g_flags.output_crc32_digest) {
    print_crc32_digest(exit_code != 0);
  }

#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  // Call SYS_exit explicitly, instead of calling SYS_exit_group implicitly by
  // either calling _exit or returning from main. SECCOMP_MODE_STRICT allows
  // only SYS_exit.
  syscall(SYS_exit, exit_code);
#endif
  return exit_code;
}
