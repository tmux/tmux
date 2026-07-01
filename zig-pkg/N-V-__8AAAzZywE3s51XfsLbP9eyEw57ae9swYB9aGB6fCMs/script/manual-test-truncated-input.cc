// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// manual-test-truncated-input tests that decoding truncated versions of
// well-formed files produces a "truncated input" error.
//
// It tests every M-byte prefix of a valid N-byte file, for every positive M
// that satisfies (M < 65536) or ((N - M) <= 1024). The truncation point is
// either within 64 KiB of the start or 1 KiB of the end. It does not test
// every potential M in between, as that would take O(N**2) time.

#include <dirent.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
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
#define WUFFS_CONFIG__MODULE__BZIP2
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ETC2
#define WUFFS_CONFIG__MODULE__GIF
#define WUFFS_CONFIG__MODULE__GZIP
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

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../release/c/wuffs-unsupported-snapshot.c"

// ----

#ifndef DST_BUFFER_ARRAY_SIZE
#define DST_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#endif

#ifndef WORKBUF_ARRAY_SIZE
#define WORKBUF_ARRAY_SIZE (256 * 1024 * 1024)
#endif

uint8_t g_dst_buffer_array[DST_BUFFER_ARRAY_SIZE] = {0};
uint8_t g_workbuf_array[WORKBUF_ARRAY_SIZE] = {0};

static int g_num_files_processed;

static struct {
  char buf[PATH_MAX];
  size_t len;
} g_relative_cwd;

// ----

static const char skipped[] = "skipped";
static const char unsupported_file_format[] = "unsupported file format";

static const char*  //
handle_image_decoder(wuffs_base__io_buffer src,
                     int32_t fourcc,
                     bool full_decode) {
  wuffs_base__image_decoder::unique_ptr dec;
  switch (fourcc) {
    case WUFFS_BASE__FOURCC__BMP:
      dec = wuffs_bmp__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__ETC2:
      dec = wuffs_etc2__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__GIF:
      dec = wuffs_gif__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__JPEG:
      dec = wuffs_jpeg__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__NIE:
      dec = wuffs_nie__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__NPBM:
      dec = wuffs_netpbm__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__PNG:
      dec = wuffs_png__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__QOI:
      dec = wuffs_qoi__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__TARGA:
      dec = wuffs_targa__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__TH:
      dec = wuffs_thumbhash__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__WBMP:
      dec = wuffs_wbmp__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    case WUFFS_BASE__FOURCC__WEBP:
      dec = wuffs_webp__decoder::alloc_as__wuffs_base__image_decoder();
      break;
    default:
      return unsupported_file_format;
  }

  if (!full_decode) {
    while (true) {
      auto dfc_status = dec->decode_frame_config(nullptr, &src);
      if (dfc_status.is_ok()) {
        // No-op.
      } else if (dfc_status.repr == wuffs_base__note__end_of_data) {
        break;
      } else {
        return dfc_status.message();
      }
    }
    return nullptr;
  }

  wuffs_base__pixel_config pixcfg = {0};
  pixcfg.set(WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL,
             WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, 0, 0);

  wuffs_base__pixel_buffer pixbuf = {0};
  auto sfs_status =
      pixbuf.set_from_slice(&pixcfg, wuffs_base__make_slice_u8(nullptr, 0));
  if (!sfs_status.is_ok()) {
    return sfs_status.message();
  }

  while (true) {
    auto df_status = dec->decode_frame(
        &pixbuf, &src, WUFFS_BASE__PIXEL_BLEND__SRC,
        wuffs_base__make_slice_u8(g_workbuf_array, WORKBUF_ARRAY_SIZE),
        nullptr);
    if (df_status.is_ok()) {
      // No-op.
    } else if (df_status.repr == wuffs_base__note__end_of_data) {
      break;
    } else {
      return df_status.message();
    }
  }
  if (src.meta.ri != src.meta.wi) {
    return skipped;
  }
  return nullptr;
}

static const char*  //
handle_io_transformer(wuffs_base__io_buffer src, int32_t fourcc) {
  wuffs_base__io_transformer::unique_ptr dec;
  switch (fourcc) {
    case WUFFS_BASE__FOURCC__BZ2:
      dec = wuffs_bzip2__decoder::alloc_as__wuffs_base__io_transformer();
      break;
    case WUFFS_BASE__FOURCC__GZ:
      dec = wuffs_gzip__decoder::alloc_as__wuffs_base__io_transformer();
      break;
    case WUFFS_BASE__FOURCC__ZLIB:
      dec = wuffs_zlib__decoder::alloc_as__wuffs_base__io_transformer();
      break;
    default:
      return unsupported_file_format;
  }

  while (true) {
    auto dst = wuffs_base__ptr_u8__writer(&g_dst_buffer_array[0],
                                          DST_BUFFER_ARRAY_SIZE);
    auto df_status = dec->transform_io(
        &dst, &src,
        wuffs_base__make_slice_u8(g_workbuf_array, WORKBUF_ARRAY_SIZE));
    if (df_status.is_ok()) {
      break;
    } else if (df_status.repr == wuffs_base__suspension__short_write) {
      // No-op.
    } else {
      return df_status.message();
    }
  }
  return nullptr;
}

static const char*  //
handle_various(wuffs_base__io_buffer src, int32_t fourcc) {
  const char* status_msg;

  status_msg = handle_image_decoder(src, fourcc, false);
  if (status_msg != unsupported_file_format) {
    if ((status_msg != nullptr) && !strstr(status_msg, "truncated input")) {
      return status_msg;
    }
    return handle_image_decoder(src, fourcc, true);
  }

  status_msg = handle_io_transformer(src, fourcc);
  if (status_msg != unsupported_file_format) {
    return status_msg;
  }

  return unsupported_file_format;
}

static const char*  //
handle_range(wuffs_base__io_buffer src,
             int32_t fourcc,
             size_t wi_min_incl,
             size_t wi_max_excl) {
  for (size_t wi = wi_min_incl; wi < wi_max_excl; wi++) {
    src.meta.wi = wi;
    const char* status_msg = handle_various(src, fourcc);
    if ((status_msg != nullptr) && strstr(status_msg, "truncated input")) {
      continue;
    }
    printf("when truncated to %zu bytes: ", wi);
    if (status_msg) {
      return status_msg;
    }
    return "have ok; want \"truncated input\"";
  }
  return nullptr;
}

static const char*  //
handle(const uint8_t* ptr, size_t len) {
  auto src = wuffs_base__ptr_u8__reader(const_cast<uint8_t*>(ptr), len, true);
  int32_t fourcc = wuffs_base__magic_number_guess_fourcc(src.reader_slice(),
                                                         src.meta.closed);
  if (fourcc <= 0) {
    return skipped;
  }

  // Skip any invalid or unsupported input (when decoded in its entirety).
  //
  // Unsupported includes ignoring wuffs_base__note__i_o_redirect. We don't
  // bother testing truncated PNGs-embedded-in-BMPs because we presumably
  // already test truncated PNGs.
  const char* status_msg = handle_various(src, fourcc);
  if (status_msg) {
    return skipped;
  }

  size_t file_len = src.meta.wi;
  if (file_len <= (65536 + 1024)) {
    return handle_range(src, fourcc, 1, file_len);
  }

  status_msg = handle_range(src, fourcc, 1, 65536);
  if (status_msg) {
    return status_msg;
  }
  return handle_range(src, fourcc, file_len - 1024, file_len);
}

// ----

static int  //
visit(char* filename);

static int  //
visit_dir(int fd) {
  int cwd_fd = open(".", O_RDONLY, 0);
  if (fchdir(fd)) {
    printf("failed\n");
    fprintf(stderr, "FAIL: fchdir: %s\n", strerror(errno));
    return 1;
  }

  DIR* d = fdopendir(fd);
  if (!d) {
    printf("failed\n");
    fprintf(stderr, "FAIL: fdopendir: %s\n", strerror(errno));
    return 1;
  }

  printf("dir\n");
  while (true) {
    struct dirent* e = readdir(d);
    if (!e) {
      break;
    }
    if ((e->d_name[0] == '\x00') || (e->d_name[0] == '.')) {
      continue;
    }
    int v = visit(e->d_name);
    if (v) {
      return v;
    }
  }

  if (closedir(d)) {
    fprintf(stderr, "FAIL: closedir: %s\n", strerror(errno));
    return 1;
  }
  if (fchdir(cwd_fd)) {
    fprintf(stderr, "FAIL: fchdir: %s\n", strerror(errno));
    return 1;
  }
  if (close(cwd_fd)) {
    fprintf(stderr, "FAIL: close: %s\n", strerror(errno));
    return 1;
  }
  return 0;
}

static int  //
visit_reg(int fd, off_t size) {
  if ((size < 0) || (0x7FFFFFFF < size)) {
    printf("failed\n");
    fprintf(stderr, "FAIL: file size out of bounds");
    return 1;
  }

  void* data = NULL;
  if (size > 0) {
    data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
      printf("failed\n");
      fprintf(stderr, "FAIL: mmap: %s\n", strerror(errno));
      return 1;
    }
  }

  const char* msg = handle((const uint8_t*)(data), size);
  if (msg) {
    printf("%s\n", msg);
  } else {
    printf("ok\n");
  }

  if ((size > 0) && munmap(data, size)) {
    fprintf(stderr, "FAIL: mmap: %s\n", strerror(errno));
    return 1;
  }
  if (close(fd)) {
    fprintf(stderr, "FAIL: close: %s\n", strerror(errno));
    return 1;
  }
  return (msg && (msg != skipped)) ? 1 : 0;
}

static int  //
visit(char* filename) {
  g_num_files_processed++;
  if (!filename || (filename[0] == '\x00')) {
    fprintf(stderr, "FAIL: invalid filename\n");
    return 1;
  }
  int n = printf("- %s%s", g_relative_cwd.buf, filename);
  printf("%*s", (60 > n) ? (60 - n) : 1, "");
  fflush(stdout);

  struct stat z;
  int fd = open(filename, O_RDONLY, 0);
  if (fd == -1) {
    printf("failed\n");
    fprintf(stderr, "FAIL: open: %s\n", strerror(errno));
    return 1;
  }
  if (fstat(fd, &z)) {
    printf("failed\n");
    fprintf(stderr, "FAIL: fstat: %s\n", strerror(errno));
    return 1;
  }

  if (S_ISREG(z.st_mode)) {
    return visit_reg(fd, z.st_size);
  } else if (!S_ISDIR(z.st_mode)) {
    printf("skipped\n");
    return 0;
  }

  size_t old_len = g_relative_cwd.len;
  size_t filename_len = strlen(filename);
  size_t new_len = old_len + strlen(filename);
  bool slash = filename[filename_len - 1] != '/';
  if (slash) {
    new_len++;
  }
  if ((filename_len >= PATH_MAX) || (new_len >= PATH_MAX)) {
    printf("failed\n");
    fprintf(stderr, "FAIL: path is too long\n");
    return 1;
  }
  memcpy(g_relative_cwd.buf + old_len, filename, filename_len);

  if (slash) {
    g_relative_cwd.buf[new_len - 1] = '/';
  }
  g_relative_cwd.buf[new_len] = '\x00';
  g_relative_cwd.len = new_len;

  int v = visit_dir(fd);

  g_relative_cwd.buf[old_len] = '\x00';
  g_relative_cwd.len = old_len;
  return v;
}

int  //
main(int argc, char** argv) {
  g_num_files_processed = 0;
  g_relative_cwd.len = 0;

  for (int i = 1; i < argc; i++) {
    int v = visit(argv[i]);
    if (v) {
      return v;
    }
  }

  printf("PASS: %d files processed\n", g_num_files_processed);
  return 0;
}
