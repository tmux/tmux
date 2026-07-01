// Copyright 2018 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// This file contains a hand-written C benchmark of different strategies for
// decoding PNG data.
//
// For a PNG image with width W and height H, the H rows can be decompressed
// one-at-a-time or all-at-once. Roughly speaking, this corresponds to H versus
// 1 call into the zlib decoder. The former (call it "fragmented dst") requires
// less scratch-space memory than the latter ("full dst"): 2 * bytes_per_row
// instead of H * bytes_per row, but the latter can be faster.
//
// The zlib-compressed data can be split into multiple IDAT chunks. Similarly,
// these chunks can be decompressed separately ("fragmented IDAT") or together
// ("full IDAT"), again providing a memory vs speed trade-off.
//
// This program reports the speed of combining the independent frag/full dst
// and frag/full IDAT techniques.
//
// For example, with gcc 7.3 (and -O3) as of January 2019:
//
// On ../test/data/hat.png (90 × 112 pixels):
// name                 time/op     relative
// FragDstFragIDAT/gcc  289µs ± 1%  1.00x
// FragDstFullIDAT/gcc  288µs ± 0%  1.00x
// FullDstFragIDAT/gcc  149µs ± 1%  1.93x
// FullDstFullIDAT/gcc  148µs ± 1%  1.95x
//
// On ../test/data/hibiscus.regular.png (312 × 442 pixels):
// name                 time/op      relative
// FragDstFragIDAT/gcc  2.49ms ± 0%  1.00x
// FragDstFullIDAT/gcc  2.49ms ± 0%  1.00x
// FullDstFragIDAT/gcc  2.08ms ± 0%  1.20x
// FullDstFullIDAT/gcc  2.02ms ± 1%  1.23x
//
// On ../test/data/harvesters.png (1165 × 859 pixels):
// name                 time/op      relative
// FragDstFragIDAT/gcc  15.6ms ± 2%  1.00x
// FragDstFullIDAT/gcc  15.4ms ± 0%  1.01x
// FullDstFragIDAT/gcc  14.4ms ± 0%  1.08x
// FullDstFullIDAT/gcc  14.1ms ± 0%  1.10x

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
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

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../release/c/wuffs-unsupported-snapshot.c"

// The order matters here. Clang also defines "__GNUC__".
#if defined(__clang__)
const char* g_cc = "clang";
const char* g_cc_version = __clang_version__;
#elif defined(__GNUC__)
const char* g_cc = "gcc";
const char* g_cc_version = __VERSION__;
#elif defined(_MSC_VER)
const char* g_cc = "cl";
const char* g_cc_version = "???";
#else
const char* g_cc = "cc";
const char* g_cc_version = "???";
#endif

static inline uint32_t  //
load_u32be(uint8_t* p) {
  return ((uint32_t)(p[0]) << 24) | ((uint32_t)(p[1]) << 16) |
         ((uint32_t)(p[2]) << 8) | ((uint32_t)(p[3]) << 0);
}

// Limit the input PNG image (and therefore its IDAT data) to (64 MiB - 1 byte)
// compressed, in up to 1024 IDAT chunks, and 256 MiB and 16384 × 16384 pixels
// uncompressed. This is a limitation of this program (which uses the Wuffs
// standard library), not a limitation of Wuffs per se.
#define DST_BUFFER_ARRAY_SIZE (256 * 1024 * 1024)
#define SRC_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#define MAX_DIMENSION (16384)
#define MAX_IDAT_CHUNKS (1024)

uint8_t g_dst_buffer_array[DST_BUFFER_ARRAY_SIZE] = {0};
size_t g_dst_len = 0;
uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE] = {0};
size_t g_src_len = 0;
uint8_t g_idat_buffer_array[SRC_BUFFER_ARRAY_SIZE] = {0};
// The n'th IDAT chunk data (where n is a zero-based count) is in
// g_idat_buffer_array[i:j], where i = g_idat_splits[n+0] and j =
// g_idat_splits[n+1].
size_t g_idat_splits[MAX_IDAT_CHUNKS + 1] = {0};
uint32_t g_num_idat_chunks = 0;

#define WORK_BUFFER_ARRAY_SIZE \
  WUFFS_ZLIB__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
#if WORK_BUFFER_ARRAY_SIZE > 0
uint8_t g_work_buffer_array[WORK_BUFFER_ARRAY_SIZE];
#else
// Not all C/C++ compilers support 0-length arrays.
uint8_t g_work_buffer_array[1];
#endif

uint32_t g_width = 0;
uint32_t g_height = 0;
uint64_t g_bytes_per_pixel = 0;
uint64_t g_bytes_per_row = 0;
uint64_t g_bytes_per_frame = 0;

const char*  //
read_stdin() {
  while (g_src_len < SRC_BUFFER_ARRAY_SIZE) {
    const int stdin_fd = 0;
    ssize_t n = read(stdin_fd, g_src_buffer_array + g_src_len,
                     SRC_BUFFER_ARRAY_SIZE - g_src_len);
    if (n > 0) {
      g_src_len += n;
    } else if (n == 0) {
      return NULL;
    } else if (errno == EINTR) {
      // No-op.
    } else {
      return strerror(errno);
    }
  }
  return "input is too large";
}

const char*  //
process_png_chunks(uint8_t* p, size_t n) {
  while (n > 0) {
    // Process the 8 byte chunk header.
    if (n < 8) {
      return "invalid PNG chunk";
    }
    uint32_t chunk_len = load_u32be(p + 0);
    uint32_t chunk_type = load_u32be(p + 4);
    p += 8;
    n -= 8;

    // Process the chunk payload.
    if (n < chunk_len) {
      return "short PNG chunk data";
    }
    switch (chunk_type) {
      case 0x49484452:  // "IHDR"
        if (chunk_len != 13) {
          return "invalid PNG IDAT chunk";
        }
        g_width = load_u32be(p + 0);
        g_height = load_u32be(p + 4);
        if ((g_width == 0) || (g_height == 0)) {
          return "image dimensions are too small";
        }
        if ((g_width > MAX_DIMENSION) || (g_height > MAX_DIMENSION)) {
          return "image dimensions are too large";
        }
        if (p[8] != 8) {
          return "unsupported PNG bit depth";
        }
        if (g_bytes_per_pixel != 0) {
          return "duplicate PNG IHDR chunk";
        }
        // Process the color type, as per the PNG spec table 11.1.
        switch (p[9]) {
          case 0:
            g_bytes_per_pixel = 1;
            break;
          case 2:
            g_bytes_per_pixel = 3;
            break;
          case 3:
            g_bytes_per_pixel = 1;
            break;
          case 4:
            g_bytes_per_pixel = 2;
            break;
          case 6:
            g_bytes_per_pixel = 4;
            break;
          default:
            return "unsupported PNG color type";
        }
        if (p[12] != 0) {
          return "unsupported PNG interlacing";
        }
        break;

      case 0x49444154:  // "IDAT"
        if (g_num_idat_chunks == MAX_IDAT_CHUNKS - 1) {
          return "too many IDAT chunks";
        }
        memcpy(g_idat_buffer_array + g_idat_splits[g_num_idat_chunks], p,
               chunk_len);
        g_idat_splits[g_num_idat_chunks + 1] =
            g_idat_splits[g_num_idat_chunks] + chunk_len;
        g_num_idat_chunks++;
        break;
    }
    p += chunk_len;
    n -= chunk_len;

    // Process (and ignore) the 4 byte chunk footer (a checksum).
    if (n < 4) {
      return "invalid PNG chunk";
    }
    p += 4;
    n -= 4;
  }
  return NULL;
}

const char*  //
decode_once(bool frag_dst, bool frag_idat) {
  wuffs_zlib__decoder dec;
  wuffs_base__status status =
      wuffs_zlib__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION, 0);
  if (!wuffs_base__status__is_ok(&status)) {
    return wuffs_base__status__message(&status);
  }

  wuffs_base__io_buffer dst = ((wuffs_base__io_buffer){
      .data = ((wuffs_base__slice_u8){
          .ptr = g_dst_buffer_array,
          .len = g_bytes_per_frame,
      }),
  });
  wuffs_base__io_buffer idat = ((wuffs_base__io_buffer){
      .data = ((wuffs_base__slice_u8){
          .ptr = g_idat_buffer_array,
          .len = SRC_BUFFER_ARRAY_SIZE,
      }),
      .meta = ((wuffs_base__io_buffer_meta){
          .wi = g_idat_splits[g_num_idat_chunks],
          .ri = 0,
          .pos = 0,
          .closed = true,
      }),
  });

  uint32_t i = 0;  // Number of dst fragments processed, if frag_dst.
  if (frag_dst) {
    dst.data.len = g_bytes_per_row;
  }

  uint32_t j = 0;  // Number of IDAT fragments processed, if frag_idat.
  if (frag_idat) {
    idat.meta.wi = g_idat_splits[1];
    idat.meta.closed = (g_num_idat_chunks == 1);
  }

  while (true) {
    status =
        wuffs_zlib__decoder__transform_io(&dec, &dst, &idat,
                                          ((wuffs_base__slice_u8){
                                              .ptr = g_work_buffer_array,
                                              .len = WORK_BUFFER_ARRAY_SIZE,
                                          }));

    if (wuffs_base__status__is_ok(&status)) {
      break;
    }
    if ((status.repr == wuffs_base__suspension__short_write) && frag_dst &&
        (i < g_height - 1)) {
      i++;
      dst.data.len = g_bytes_per_row * (i + 1);
      continue;
    }
    if ((status.repr == wuffs_base__suspension__short_read) && frag_idat &&
        (j < g_num_idat_chunks - 1)) {
      j++;
      idat.meta.wi = g_idat_splits[j + 1];
      idat.meta.closed = (g_num_idat_chunks == j + 1);
      continue;
    }
    return wuffs_base__status__message(&status);
  }

  if (dst.meta.wi != g_bytes_per_frame) {
    return "unexpected number of bytes decoded";
  }
  return NULL;
}

const char*  //
decode(bool frag_dst, bool frag_idat) {
  int reps;
  if (g_bytes_per_frame < 100000) {
    reps = 1000;
  } else if (g_bytes_per_frame < 1000000) {
    reps = 100;
  } else if (g_bytes_per_frame < 10000000) {
    reps = 10;
  } else {
    reps = 1;
  }

  struct timeval bench_start_tv;
  gettimeofday(&bench_start_tv, NULL);

  for (int i = 0; i < reps; i++) {
    const char* msg = decode_once(frag_dst, frag_idat);
    if (msg) {
      return msg;
    }
  }

  struct timeval bench_finish_tv;
  gettimeofday(&bench_finish_tv, NULL);
  int64_t micros =
      (int64_t)(bench_finish_tv.tv_sec - bench_start_tv.tv_sec) * 1000000 +
      (int64_t)(bench_finish_tv.tv_usec - bench_start_tv.tv_usec);
  uint64_t nanos = 1;
  if (micros > 0) {
    nanos = (uint64_t)(micros)*1000;
  }

  printf("Benchmark%sDst%sIDAT/%s\t%8d\t%8" PRIu64 " ns/op\n",
         frag_dst ? "Frag" : "Full",   //
         frag_idat ? "Frag" : "Full",  //
         g_cc, reps, nanos / reps);

  return NULL;
}

int  //
fail(const char* msg) {
  const int stderr_fd = 2;
  write(stderr_fd, msg, strnlen(msg, 4095));
  write(stderr_fd, "\n", 1);
  return 1;
}

int  //
main(int argc, char** argv) {
  const char* msg = read_stdin();
  if (msg) {
    return fail(msg);
  }
  if ((g_src_len < 8) || strncmp((const char*)(g_src_buffer_array),
                                 "\x89PNG\x0D\x0A\x1A\x0A", 8)) {
    return fail("invalid PNG");
  }
  msg = process_png_chunks(g_src_buffer_array + 8, g_src_len - 8);
  if (msg) {
    return fail(msg);
  }
  if (g_bytes_per_pixel == 0) {
    return fail("missing PNG IHDR chunk");
  }
  if (g_num_idat_chunks == 0) {
    return fail("missing PNG IDAT chunk");
  }
  // The +1 here is for the per-row filter byte.
  g_bytes_per_row = (uint64_t)g_width * g_bytes_per_pixel + 1;
  g_bytes_per_frame = (uint64_t)g_height * g_bytes_per_row;
  if (g_bytes_per_frame > DST_BUFFER_ARRAY_SIZE) {
    return fail("decompressed data is too large");
  }

  printf("# %s version %s\n#\n", g_cc, g_cc_version);
  printf(
      "# The output format, including the \"Benchmark\" prefixes, is "
      "compatible with the\n"
      "# https://godoc.org/golang.org/x/perf/cmd/benchstat tool. To install "
      "it, first\n"
      "# install Go, then run \"go install golang.org/x/perf/cmd/benchstat\".\n");

  for (int i = 0; i < 5; i++) {
    msg = decode(true, true);
    if (msg) {
      return fail(msg);
    }
    msg = decode(true, false);
    if (msg) {
      return fail(msg);
    }
    msg = decode(false, true);
    if (msg) {
      return fail(msg);
    }
    msg = decode(false, false);
    if (msg) {
      return fail(msg);
    }
  }

  return 0;
}
