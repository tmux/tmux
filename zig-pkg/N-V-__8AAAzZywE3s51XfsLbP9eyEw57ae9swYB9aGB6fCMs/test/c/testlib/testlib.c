// Copyright 2017 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define MIMICLIB_SCRATCH_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#define IO_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#define PIXEL_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#define TOKEN_BUFFER_ARRAY_SIZE (128 * 1024)

#define WUFFS_TESTLIB_ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

uint8_t g_have_array_u8[IO_BUFFER_ARRAY_SIZE];
uint8_t g_want_array_u8[IO_BUFFER_ARRAY_SIZE];
uint8_t g_work_array_u8[IO_BUFFER_ARRAY_SIZE];
uint8_t g_src_array_u8[IO_BUFFER_ARRAY_SIZE];

uint8_t g_mimiclib_scratch_array_u8[MIMICLIB_SCRATCH_BUFFER_ARRAY_SIZE];
uint8_t g_pixel_array_u8[PIXEL_BUFFER_ARRAY_SIZE];

wuffs_base__token g_have_array_token[TOKEN_BUFFER_ARRAY_SIZE];
wuffs_base__token g_want_array_token[TOKEN_BUFFER_ARRAY_SIZE];

wuffs_base__slice_u8 g_have_slice_u8;
wuffs_base__slice_u8 g_want_slice_u8;
wuffs_base__slice_u8 g_work_slice_u8;
wuffs_base__slice_u8 g_src_slice_u8;

wuffs_base__slice_u8 g_mimiclib_scratch_slice_u8;
wuffs_base__slice_u8 g_pixel_slice_u8;

wuffs_base__slice_token g_have_slice_token;
wuffs_base__slice_token g_want_slice_token;

void  //
wuffs_testlib__initialize_global_xxx_slices() {
  g_have_slice_u8 = ((wuffs_base__slice_u8){
      .ptr = g_have_array_u8,
      .len = IO_BUFFER_ARRAY_SIZE,
  });
  g_want_slice_u8 = ((wuffs_base__slice_u8){
      .ptr = g_want_array_u8,
      .len = IO_BUFFER_ARRAY_SIZE,
  });
  g_work_slice_u8 = ((wuffs_base__slice_u8){
      .ptr = g_work_array_u8,
      .len = IO_BUFFER_ARRAY_SIZE,
  });
  g_src_slice_u8 = ((wuffs_base__slice_u8){
      .ptr = g_src_array_u8,
      .len = IO_BUFFER_ARRAY_SIZE,
  });
  g_mimiclib_scratch_slice_u8 = ((wuffs_base__slice_u8){
      .ptr = g_mimiclib_scratch_array_u8,
      .len = MIMICLIB_SCRATCH_BUFFER_ARRAY_SIZE,
  });
  g_pixel_slice_u8 = ((wuffs_base__slice_u8){
      .ptr = g_pixel_array_u8,
      .len = PIXEL_BUFFER_ARRAY_SIZE,
  });

  g_have_slice_token = ((wuffs_base__slice_token){
      .ptr = g_have_array_token,
      .len = TOKEN_BUFFER_ARRAY_SIZE,
  });
  g_want_slice_token = ((wuffs_base__slice_token){
      .ptr = g_want_array_token,
      .len = TOKEN_BUFFER_ARRAY_SIZE,
  });
}

uint8_t  //
unhex(uint8_t b) {
  if (('0' <= b) && (b <= '9')) {
    return b - '0';
  }
  if (('A' <= b) && (b <= 'F')) {
    return b + 10 - 'A';
  }
  if (('a' <= b) && (b <= 'f')) {
    return b + 10 - 'a';
  }
  return 0;
}

char g_fail_msg[65536] = {0};

#define RETURN_FAIL(...)                                                \
  return (snprintf(g_fail_msg, sizeof(g_fail_msg), ##__VA_ARGS__) >= 0) \
             ? g_fail_msg                                               \
             : strcpy(g_fail_msg, "unknown failure (snprintf-related)")

#define INCR_FAIL(msg, ...) \
  msg += snprintf(msg, sizeof(g_fail_msg) - (msg - g_fail_msg), ##__VA_ARGS__)

#define CHECK_STATUS(prefix, status)             \
  do {                                           \
    wuffs_base__status z = status;               \
    if (z.repr) {                                \
      RETURN_FAIL("%s: \"%s\"", prefix, z.repr); \
    }                                            \
  } while (0)

#define CHECK_STRING(string) \
  do {                       \
    const char* z = string;  \
    if (z) {                 \
      return z;              \
    }                        \
  } while (0)

int g_tests_run = 0;

struct {
  int remaining_argc;
  char** remaining_argv;

  bool bench;
  const char* focus;
  uint64_t iterscale;
  int reps;
} g_flags = {0};

const char*  //
parse_flags(int argc, char** argv) {
  g_flags.iterscale = 100;
  g_flags.reps = 5;

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

    if (!strcmp(arg, "bench")) {
      g_flags.bench = true;
      continue;
    }

    if (!strncmp(arg, "focus=", 6)) {
      g_flags.focus = arg + 6;
      continue;
    }

    if (!strncmp(arg, "iterscale=", 10)) {
      arg += 10;
      if (!*arg) {
        return "missing -iterscale=N value";
      }
      char* end = NULL;
      long int n = strtol(arg, &end, 10);
      if (*end) {
        return "invalid -iterscale=N value";
      }
      if ((n < 0) || (1000000 < n)) {
        return "out-of-range -iterscale=N value";
      }
      g_flags.iterscale = n;
      continue;
    }

    if (!strncmp(arg, "reps=", 5)) {
      arg += 5;
      if (!*arg) {
        return "missing -reps=N value";
      }
      char* end = NULL;
      long int n = strtol(arg, &end, 10);
      if (*end) {
        return "invalid -reps=N value";
      }
      if ((n < 0) || (1000000 < n)) {
        return "out-of-range -reps=N value";
      }
      g_flags.reps = n;
      continue;
    }

    return "unrecognized flag argument";
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return NULL;
}

const char* g_proc_package_name = "unknown_package_name";
const char* g_proc_func_name = "unknown_func_name";
bool g_in_focus = false;

#define CHECK_FOCUS(func_name)  \
  g_proc_func_name = func_name; \
  g_in_focus = check_focus();   \
  if (!g_in_focus) {            \
    return NULL;                \
  }

bool  //
check_focus() {
  const char* p = g_flags.focus;
  if (!p || !*p) {
    return true;
  }
  size_t n = strlen(g_proc_func_name);

  // On each iteration of the loop, set p and q so that p (inclusive) and q
  // (exclusive) bracket the interesting fragment of the comma-separated
  // elements of focus.
  while (true) {
    const char* r = p;
    const char* q = NULL;
    while ((*r != ',') && (*r != '\x00')) {
      if ((*r == '/') && (q == NULL)) {
        q = r;
      }
      r++;
    }
    if (q == NULL) {
      q = r;
    }
    // At this point, r points to the comma or NUL byte that ends this element
    // of the comma-separated list. q points to the first slash in that
    // element, or if there are no slashes, q equals r.
    //
    // The pointers are named so that (p <= q) && (q <= r).

    if (p == q) {
      // No-op. Skip empty focus targets, which makes it convenient to
      // copy/paste a string with a trailing comma.
    } else {
      // Strip a leading "Benchmark", if present, from the [p, q) string.
      // Idiomatic C function names look like "test_wuffs_gif_lzw_decode_pi"
      // and won't start with "Benchmark". Stripping lets us conveniently
      // copy/paste a string like "Benchmarkwuffs_gif_decode_10k/gcc" from the
      // "wuffs bench std/gif" output.
      if ((q - p >= 9) && !strncmp(p, "Benchmark", 9)) {
        p += 9;
      }

      // See if g_proc_func_name (with or without a "test_" or "bench_" prefix)
      // starts with the [p, q) string.
      if ((n >= q - p) && !strncmp(g_proc_func_name, p, q - p)) {
        return true;
      }
      const char* unprefixed_proc_func_name = NULL;
      size_t unprefixed_n = 0;
      if ((n >= q - p) && !strncmp(g_proc_func_name, "test_", 5)) {
        unprefixed_proc_func_name = g_proc_func_name + 5;
        unprefixed_n = n - 5;
      } else if ((n >= q - p) && !strncmp(g_proc_func_name, "bench_", 6)) {
        unprefixed_proc_func_name = g_proc_func_name + 6;
        unprefixed_n = n - 6;
      }
      if (unprefixed_proc_func_name && (unprefixed_n >= q - p) &&
          !strncmp(unprefixed_proc_func_name, p, q - p)) {
        return true;
      }
    }

    if (*r == '\x00') {
      break;
    }
    p = r + 1;
  }
  return false;
}

// https://www.guyrutenberg.com/2008/12/20/expanding-macros-into-string-constants-in-c/
#define WUFFS_TESTLIB_QUOTE_EXPAND(x) #x
#define WUFFS_TESTLIB_QUOTE(x) WUFFS_TESTLIB_QUOTE_EXPAND(x)

// The order matters here. Clang also defines "__GNUC__".
#if defined(__clang__)
const char* g_cc = "clang" WUFFS_TESTLIB_QUOTE(__clang_major__);
const char* g_cc_version = __clang_version__;
#elif defined(__GNUC__)
const char* g_cc = "gcc" WUFFS_TESTLIB_QUOTE(__GNUC__);
const char* g_cc_version = __VERSION__;
#elif defined(_MSC_VER)
const char* g_cc = "cl";
const char* g_cc_version = "???";
#else
const char* g_cc = "cc";
const char* g_cc_version = "???";
#endif

typedef struct {
  const char* want_filename;
  const char* src_filename;
  size_t src_offset0;
  size_t src_offset1;
} golden_test;

bool g_bench_warm_up;
struct timeval g_bench_start_tv;

void  //
bench_start() {
  gettimeofday(&g_bench_start_tv, NULL);
}

void  //
bench_finish(uint64_t iters, uint64_t n_bytes) {
  struct timeval bench_finish_tv;
  gettimeofday(&bench_finish_tv, NULL);
  int64_t micros =
      (int64_t)(bench_finish_tv.tv_sec - g_bench_start_tv.tv_sec) * 1000000 +
      (int64_t)(bench_finish_tv.tv_usec - g_bench_start_tv.tv_usec);
  uint64_t nanos = 1;
  if (micros > 0) {
    nanos = (uint64_t)(micros)*1000;
  }
  uint64_t kb_per_s = n_bytes * 1000000 / nanos;

  const char* name = g_proc_func_name;
  if ((strlen(name) >= 6) && !strncmp(name, "bench_", 6)) {
    name += 6;
  }
  if (g_bench_warm_up) {
    printf("# (warm up) %s/%s\t%8" PRIu64 ".%06" PRIu64 " seconds\n",  //
           name, g_cc, nanos / 1000000000, (nanos % 1000000000) / 1000);
  } else if (!n_bytes) {
    printf("Benchmark%s/%s\t%8" PRIu64 "\t%8" PRIu64 " ns/op\n",  //
           name, g_cc, iters, nanos / iters);
  } else {
    printf("Benchmark%s/%s\t%8" PRIu64 "\t%8" PRIu64
           " ns/op\t%8d.%03d MB/s\n",         //
           name, g_cc, iters, nanos / iters,  //
           (int)(kb_per_s / 1000), (int)(kb_per_s % 1000));
  }
  // Flush stdout so that "wuffs bench | tee etc" still prints its numbers as
  // soon as they are available.
  fflush(stdout);
}

const char*  //
chdir_to_the_wuffs_root_directory() {
  // Chdir to the Wuffs root directory, assuming that we're starting from
  // somewhere in the Wuffs repository, so we can find the root directory by
  // running chdir("..") a number of times.
  for (int n = 0; n < 64; n++) {
    if (access("wuffs-root-directory.txt", F_OK) == 0) {
      return NULL;
    }

    // If we're at the root "/", chdir("..") won't change anything.
    char cwd_buffer[4];
    char* cwd = getcwd(cwd_buffer, 4);
    if ((cwd != NULL) && (cwd[0] == '/') && (cwd[1] == '\x00')) {
      break;
    }

    if (chdir("..")) {
      return "could not chdir(\"..\")";
    }
  }
  return "could not find Wuffs root directory; chdir there before running this "
         "program";
}

typedef const char* (*proc)();

int  //
test_main(int argc, char** argv, proc* tests, proc* benches) {
  wuffs_testlib__initialize_global_xxx_slices();
  const char* status = chdir_to_the_wuffs_root_directory();
  if (status) {
    fprintf(stderr, "%s\n", status);
    return 1;
  }

  status = parse_flags(argc, argv);
  if (status) {
    fprintf(stderr, "%s\n", status);
    return 1;
  }
  if (g_flags.remaining_argc > 0) {
    fprintf(stderr, "unexpected (non-flag) argument\n");
    return 1;
  }

  int reps = 1;
  proc* procs = tests;
  if (g_flags.bench) {
    reps = g_flags.reps + 1;  // +1 for the warm up run.
    procs = benches;
    printf("# %s\n# %s version %s\n#\n", g_proc_package_name, g_cc,
           g_cc_version);
    printf(
        "# The output format, including the \"Benchmark\" prefixes, is "
        "compatible with the\n"
        "# https://godoc.org/golang.org/x/perf/cmd/benchstat tool. To install "
        "it, first\n"
        "# install Go, then run \"go install "
        "golang.org/x/perf/cmd/benchstat\".\n");
  }

  for (int i = 0; i < reps; i++) {
    g_bench_warm_up = i == 0;
    for (proc* p = procs; *p; p++) {
      g_proc_func_name = "unknown_func_name";
      g_fail_msg[0] = 0;
      g_in_focus = false;
      const char* status = (*p)();
      if (!g_in_focus) {
        continue;
      }
      if (status) {
        printf("%-16s%-8sFAIL %s: %s\n", g_proc_package_name, g_cc,
               g_proc_func_name, status);
        return 1;
      }
      if (i == 0) {
        g_tests_run++;
      }
    }
    if (i != 0) {
      continue;
    }
    if (g_flags.bench) {
      printf("# %d benchmarks, 1+%d reps per benchmark, iterscale=%d\n",
             g_tests_run, g_flags.reps, (int)(g_flags.iterscale));
    } else {
      printf("%-16s%-8sPASS (%d tests)\n", g_proc_package_name, g_cc,
             g_tests_run);
    }
  }
  return 0;
}

wuffs_base__io_buffer  //
make_io_buffer_from_string(const char* ptr, size_t len) {
  return ((wuffs_base__io_buffer){
      .data = ((wuffs_base__slice_u8){
          .ptr = ((uint8_t*)(ptr)),
          .len = len,
      }),
      .meta = ((wuffs_base__io_buffer_meta){
          .wi = len,
          .closed = true,
      }),
  });
}

wuffs_base__rect_ie_u32  //
make_rect_ie_u32(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
  wuffs_base__rect_ie_u32 ret;
  ret.min_incl_x = x0;
  ret.min_incl_y = y0;
  ret.max_excl_x = x1;
  ret.max_excl_y = y1;
  return ret;
}

wuffs_base__io_buffer  //
make_limited_reader(wuffs_base__io_buffer b, uint64_t limit) {
  uint64_t n = b.meta.wi - b.meta.ri;
  bool closed = b.meta.closed;
  if (n > limit) {
    n = limit;
    closed = false;
  }

  wuffs_base__io_buffer ret;
  ret.data.ptr = b.data.ptr + b.meta.ri;
  ret.data.len = n;
  ret.meta.wi = n;
  ret.meta.ri = 0;
  ret.meta.pos = wuffs_base__u64__sat_add(b.meta.pos, b.meta.ri);
  ret.meta.closed = closed;
  return ret;
}

wuffs_base__io_buffer  //
make_limited_writer(wuffs_base__io_buffer b, uint64_t limit) {
  uint64_t n = b.data.len - b.meta.wi;
  if (n > limit) {
    n = limit;
  }

  wuffs_base__io_buffer ret;
  ret.data.ptr = b.data.ptr + b.meta.wi;
  ret.data.len = n;
  ret.meta.wi = 0;
  ret.meta.ri = 0;
  ret.meta.pos = wuffs_base__u64__sat_add(b.meta.pos, b.meta.wi);
  ret.meta.closed = b.meta.closed;
  return ret;
}

wuffs_base__token_buffer  //
make_limited_token_writer(wuffs_base__token_buffer b, uint64_t limit) {
  uint64_t n = b.data.len - b.meta.wi;
  if (n > limit) {
    n = limit;
  }

  wuffs_base__token_buffer ret;
  ret.data.ptr = b.data.ptr + b.meta.wi;
  ret.data.len = n;
  ret.meta.wi = 0;
  ret.meta.ri = 0;
  ret.meta.pos = wuffs_base__u64__sat_add(b.meta.pos, b.meta.wi);
  ret.meta.closed = b.meta.closed;
  return ret;
}

// TODO: we shouldn't need to pass the rect. Instead, pass a subset pixbuf.
const char*  //
copy_to_io_buffer_from_pixel_buffer(wuffs_base__io_buffer* dst,
                                    wuffs_base__pixel_buffer* src,
                                    wuffs_base__rect_ie_u32 r) {
  if (!src) {
    return "copy_to_io_buffer_from_pixel_buffer: NULL src";
  }

  wuffs_base__pixel_format pixfmt =
      wuffs_base__pixel_config__pixel_format(&src->pixcfg);
  if (wuffs_base__pixel_format__is_planar(&pixfmt)) {
    // If we want to support planar pixel buffers, in the future, be concious
    // of pixel subsampling.
    return "copy_to_io_buffer_from_pixel_buffer: cannot copy from planar src";
  }
  uint32_t bits_per_pixel = wuffs_base__pixel_format__bits_per_pixel(&pixfmt);
  if (bits_per_pixel == 0) {
    return "copy_to_io_buffer_from_pixel_buffer: invalid bits_per_pixel";
  } else if ((bits_per_pixel % 8) != 0) {
    return "copy_to_io_buffer_from_pixel_buffer: cannot copy fractional bytes";
  }
  size_t bytes_per_pixel = bits_per_pixel / 8;

  for (uint32_t p = 0; p < 1; p++) {
    wuffs_base__table_u8 tab = wuffs_base__pixel_buffer__plane(src, p);
    for (uint32_t y = r.min_incl_y; y < r.max_excl_y; y++) {
      wuffs_base__slice_u8 row = wuffs_private_impl__table_u8__row_u32(tab, y);
      if ((r.min_incl_x >= r.max_excl_x) ||
          (r.max_excl_x > (row.len / bytes_per_pixel))) {
        break;
      }

      size_t n = r.max_excl_x - r.min_incl_x;
      if (n > (SIZE_MAX / bytes_per_pixel)) {
        return "copy_to_io_buffer_from_pixel_buffer: n is too large";
      }
      n *= bytes_per_pixel;

      if (n > (dst->data.len - dst->meta.wi)) {
        return "copy_to_io_buffer_from_pixel_buffer: dst buffer is too small";
      }
      memmove(dst->data.ptr + dst->meta.wi, row.ptr + r.min_incl_x, n);
      dst->meta.wi += n;
    }
  }
  return NULL;
}

bool  //
skip_read_file_patches(const char** path) {
  const char* p = *path;
  while (*p == '@') {
    p++;
    while (*p != ';') {
      if (*p == '\x00') {
        return false;
      }
      p++;
    }
    p++;
  }
  *path = p;
  return true;
}

bool  //
apply_read_file_patches(wuffs_base__io_buffer* dst,
                        const char* original_path,
                        size_t original_wi) {
  const char* p = original_path;
  while (*p == '@') {
    p++;
    uint64_t offset = 0;
    while (*p != '=') {
      if (*p == '\x00') {
        return false;
      }
      offset = (offset << 4) | unhex(*p);
      p++;
    }
    p++;
    uint8_t before = 0;
    while (*p != '=') {
      if (*p == '\x00') {
        return false;
      }
      before = (before << 4) | unhex(*p);
      p++;
    }
    p++;
    uint8_t after = 0;
    while (*p != ';') {
      if (*p == '\x00') {
        return false;
      }
      after = (after << 4) | unhex(*p);
      p++;
    }
    p++;

    size_t i = original_wi + offset;
    if ((i >= dst->meta.wi) || (dst->data.ptr[i] != before)) {
      return false;
    }
    dst->data.ptr[i] = after;
  }
  return true;
}

// read_file loads path into dst.
//
// The path may be preceded by zero or more "@123=45=67;" sub-strings, which
// denote a one-byte patch at offset 0x123, changing the byte at that offset
// from 0x45 to 0x67.
const char*  //
read_file(wuffs_base__io_buffer* dst, const char* path) {
  if (!dst || !path) {
    RETURN_FAIL("read_file: NULL argument");
  }
  if (dst->meta.closed) {
    RETURN_FAIL("read_file: dst buffer closed for writes");
  }

  const char* original_path = path;
  size_t original_wi = dst->meta.wi;
  if (!skip_read_file_patches(&path)) {
    RETURN_FAIL("read_file(\"%s\"): invalid \"@123=45=67;\" patch",
                original_path);
  }

  FILE* f = fopen(path, "rb");
  if (!f) {
    RETURN_FAIL("read_file(\"%s\"): %s (errno=%d)", path, strerror(errno),
                errno);
  }

  uint8_t placeholder[1];
  uint8_t* ptr = dst->data.ptr + dst->meta.wi;
  size_t len = dst->data.len - dst->meta.wi;
  while (true) {
    if (!len) {
      // We have read all that dst can hold. Check that we have read the full
      // file by trying to read one more byte, which should fail with EOF.
      ptr = placeholder;
      len = 1;
    }
    size_t n = fread(ptr, 1, len, f);
    if (ptr != placeholder) {
      ptr += n;
      len -= n;
      dst->meta.wi += n;
    } else if (n) {
      fclose(f);
      RETURN_FAIL("read_file(\"%s\"): EOF not reached", path);
    }
    if (feof(f)) {
      break;
    }
    int err = ferror(f);
    if (!err) {
      continue;
    }
    if (err == EINTR) {
      clearerr(f);
      continue;
    }
    fclose(f);
    RETURN_FAIL("read_file(\"%s\"): %s (errno=%d)", path, strerror(err), err);
  }
  fclose(f);
  dst->meta.pos = 0;
  dst->meta.closed = true;

  if (!apply_read_file_patches(dst, original_path, original_wi)) {
    RETURN_FAIL("read_file(\"%s\"): invalid \"@123=45=67;\" patch",
                original_path);
  }
  return NULL;
}

const char*  //
read_file_fragment(wuffs_base__io_buffer* dst,
                   const char* path,
                   size_t ri_min,
                   size_t wi_max) {
  CHECK_STRING(read_file(dst, path));
  if (dst->meta.ri < ri_min) {
    dst->meta.ri = ri_min;
  }
  if (dst->meta.wi > wi_max) {
    dst->meta.wi = wi_max;
  }
  if (dst->meta.ri > dst->meta.wi) {
    RETURN_FAIL("read_file_fragment(\"%s\"): ri > wi", path);
  }
  return NULL;
}

char*  //
hex_dump(char* msg, wuffs_base__io_buffer* buf, size_t i) {
  if (!msg || !buf) {
    RETURN_FAIL("hex_dump: NULL argument");
  }
  if (buf->meta.wi == 0) {
    return msg;
  }
  size_t base = i - (i & 15);
  for (int j = -3 * 16; j <= +3 * 16; j += 16) {
    if ((j < 0) && (base < (size_t)(-j))) {
      continue;
    }
    size_t b = base + j;
    if (b >= buf->meta.wi) {
      break;
    }
    size_t n = buf->meta.wi - b;
    INCR_FAIL(msg, "  %06zx:", b);
    for (size_t k = 0; k < 16; k++) {
      if (k % 2 == 0) {
        INCR_FAIL(msg, " ");
      }
      if (k < n) {
        INCR_FAIL(msg, "%02x", buf->data.ptr[b + k]);
      } else {
        INCR_FAIL(msg, "  ");
      }
    }
    INCR_FAIL(msg, "  ");
    for (size_t k = 0; k < 16; k++) {
      char c = ' ';
      if (k < n) {
        c = buf->data.ptr[b + k];
        if ((c < 0x20) || (0x7F <= c)) {
          c = '.';
        }
      }
      INCR_FAIL(msg, "%c", c);
    }
    INCR_FAIL(msg, "\n");
    if (n < 16) {
      break;
    }
  }
  return msg;
}

const char*  //
check_io_buffers_equal(const char* prefix,
                       wuffs_base__io_buffer* have,
                       wuffs_base__io_buffer* want) {
  if (!have || !want) {
    RETURN_FAIL("%sio_buffers_equal: NULL argument", prefix);
  }
  char* msg = g_fail_msg;
  size_t i = 0;
  size_t n = have->meta.wi < want->meta.wi ? have->meta.wi : want->meta.wi;
  while ((i < n) && (have->data.ptr[i] == want->data.ptr[i])) {
    i++;
  }
  if (have->meta.wi != want->meta.wi) {
    INCR_FAIL(msg, "%sio_buffers_equal: wi: have %zu, want %zu.\n", prefix,
              have->meta.wi, want->meta.wi);
  } else if (i < have->meta.wi) {
    INCR_FAIL(msg, "%sio_buffers_equal: wi=%zu:\n", prefix, n);
  } else {
    return NULL;
  }
  INCR_FAIL(msg, "contents differ at byte %zu (in hex: 0x%06zx):\n", i, i);
  msg = hex_dump(msg, have, i);
  INCR_FAIL(msg, "excerpts of have (above) versus want (below):\n");
  msg = hex_dump(msg, want, i);
  return g_fail_msg;
}

bool  //
strings_are_equal(const char* s, const char* t) {
  if (s == t) {
    return true;
  } else if ((s == NULL) || (t == NULL)) {
    return false;
  }
  return strcmp(s, t) == 0;
}

// throughput_counter is whether to count dst or src bytes, or neither, when
// calculating a benchmark's MB/s throughput number.
//
// Decoders typically use tcounter_dst. Encoders and hashes typically use
// tcounter_src.
typedef enum {
  tcounter_neither = 0,
  tcounter_dst = 1,
  tcounter_src = 2,
} throughput_counter;

const char*  //
proc_io_buffers(const char* (*codec_func)(wuffs_base__io_buffer*,
                                          wuffs_base__io_buffer*,
                                          uint32_t,
                                          uint64_t,
                                          uint64_t),
                uint32_t wuffs_initialize_flags,
                throughput_counter tcounter,
                golden_test* gt,
                uint64_t wlimit,
                uint64_t rlimit,
                uint64_t iters,
                bool bench) {
  if (!codec_func) {
    RETURN_FAIL("NULL codec_func");
  }
  if (!gt) {
    RETURN_FAIL("NULL golden_test");
  }

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });

  if (!gt->src_filename) {
    src.meta.closed = true;
  } else {
    const char* status = read_file(&src, gt->src_filename);
    if (status) {
      return status;
    }
  }
  if (gt->src_offset0 || gt->src_offset1) {
    if (gt->src_offset0 > gt->src_offset1) {
      RETURN_FAIL("inconsistent src_offsets");
    }
    if (gt->src_offset1 > src.meta.wi) {
      RETURN_FAIL("src_offset1 too large");
    }
    src.meta.ri = gt->src_offset0;
    src.meta.wi = gt->src_offset1;
  }

  if (bench) {
    bench_start();
  }
  uint64_t n_bytes = 0;
  for (uint64_t i = 0; i < iters; i++) {
    have.meta.wi = 0;
    src.meta.ri = gt->src_offset0;
    const char* status =
        codec_func(&have, &src, wuffs_initialize_flags, wlimit, rlimit);
    if (status) {
      return status;
    }
    switch (tcounter) {
      case tcounter_neither:
        break;
      case tcounter_dst:
        n_bytes += have.meta.wi;
        break;
      case tcounter_src:
        n_bytes += src.meta.ri - gt->src_offset0;
        break;
    }
  }
  if (bench) {
    bench_finish(iters, n_bytes);
    return NULL;
  }

  if (!gt->want_filename) {
    want.meta.closed = true;
  } else {
    const char* status = read_file(&want, gt->want_filename);
    if (status) {
      return status;
    }
  }
  return check_io_buffers_equal("", &have, &want);
}

const char*  //
proc_token_decoder(const char* (*codec_func)(wuffs_base__token_buffer*,
                                             wuffs_base__io_buffer*,
                                             uint32_t,
                                             uint64_t,
                                             uint64_t),
                   uint32_t wuffs_initialize_flags,
                   throughput_counter tcounter,
                   golden_test* gt,
                   uint64_t wlimit,
                   uint64_t rlimit,
                   uint64_t iters,
                   bool bench) {
  if (!codec_func) {
    RETURN_FAIL("NULL codec_func");
  }
  if (!gt) {
    RETURN_FAIL("NULL golden_test");
  }

  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  wuffs_base__token_buffer have = ((wuffs_base__token_buffer){
      .data = g_have_slice_token,
  });

  if (!gt->src_filename) {
    src.meta.closed = true;
  } else {
    const char* status = read_file(&src, gt->src_filename);
    if (status) {
      return status;
    }
  }
  if (gt->src_offset0 || gt->src_offset1) {
    if (gt->src_offset0 > gt->src_offset1) {
      RETURN_FAIL("inconsistent src_offsets");
    }
    if (gt->src_offset1 > src.meta.wi) {
      RETURN_FAIL("src_offset1 too large");
    }
    src.meta.ri = gt->src_offset0;
    src.meta.wi = gt->src_offset1;
  }

  if (bench) {
    bench_start();
  }
  uint64_t n_bytes = 0;
  for (uint64_t i = 0; i < iters; i++) {
    have.meta.wi = 0;
    src.meta.ri = gt->src_offset0;
    const char* status =
        codec_func(&have, &src, wuffs_initialize_flags, wlimit, rlimit);
    if (status) {
      return status;
    }
    switch (tcounter) {
      case tcounter_neither:
        break;
      case tcounter_dst:
        RETURN_FAIL("cannot use tcounter_dst for token decoders");
        break;
      case tcounter_src:
        n_bytes += src.meta.ri - gt->src_offset0;
        break;
    }
  }
  if (bench) {
    bench_finish(iters, n_bytes);
  }
  return NULL;
}

const char*  //
do_bench_io_buffers(const char* (*codec_func)(wuffs_base__io_buffer*,
                                              wuffs_base__io_buffer*,
                                              uint32_t,
                                              uint64_t,
                                              uint64_t),
                    uint32_t wuffs_initialize_flags,
                    throughput_counter tcounter,
                    golden_test* gt,
                    uint64_t wlimit,
                    uint64_t rlimit,
                    uint64_t iters_unscaled) {
  return proc_io_buffers(codec_func, wuffs_initialize_flags, tcounter, gt,
                         wlimit, rlimit, iters_unscaled * g_flags.iterscale,
                         true);
}

const char*  //
do_bench_token_decoder(const char* (*codec_func)(wuffs_base__token_buffer*,
                                                 wuffs_base__io_buffer*,
                                                 uint32_t,
                                                 uint64_t,
                                                 uint64_t),
                       uint32_t wuffs_initialize_flags,
                       throughput_counter tcounter,
                       golden_test* gt,
                       uint64_t wlimit,
                       uint64_t rlimit,
                       uint64_t iters_unscaled) {
  return proc_token_decoder(codec_func, wuffs_initialize_flags, tcounter, gt,
                            wlimit, rlimit, iters_unscaled * g_flags.iterscale,
                            true);
}

const char*  //
do_test_io_buffers(const char* (*codec_func)(wuffs_base__io_buffer*,
                                             wuffs_base__io_buffer*,
                                             uint32_t,
                                             uint64_t,
                                             uint64_t),
                   golden_test* gt,
                   uint64_t wlimit,
                   uint64_t rlimit) {
  return proc_io_buffers(codec_func,
                         WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
                         tcounter_neither, gt, wlimit, rlimit, 1, false);
}

// --------

const char*  //
do_run__wuffs_base__image_decoder(wuffs_base__image_decoder* b,
                                  uint64_t* n_bytes_out,
                                  wuffs_base__io_buffer* dst,
                                  wuffs_base__pixel_format pixfmt,
                                  uint32_t* quirks_ptr,
                                  size_t quirks_len,
                                  wuffs_base__io_buffer* src) {
  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});

  for (size_t i = 0; i < quirks_len; i++) {
    wuffs_base__image_decoder__set_quirk(b, quirks_ptr[i], 1);
  }

  uint32_t bits_per_pixel = wuffs_base__pixel_format__bits_per_pixel(&pixfmt);
  if (bits_per_pixel == 0) {
    return "do_run__wuffs_base__image_decoder: invalid bits_per_pixel";
  } else if ((bits_per_pixel % 8) != 0) {
    return "do_run__wuffs_base__image_decoder: cannot bench fractional bytes";
  }
  uint64_t bytes_per_pixel = bits_per_pixel / 8;

  CHECK_STATUS("decode_image_config",
               wuffs_base__image_decoder__decode_image_config(b, &ic, src));
  wuffs_base__pixel_config__set(&ic.pixcfg, pixfmt.repr,
                                WUFFS_BASE__PIXEL_SUBSAMPLING__NONE,
                                wuffs_base__pixel_config__width(&ic.pixcfg),
                                wuffs_base__pixel_config__height(&ic.pixcfg));
  CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                     &pb, &ic.pixcfg, g_pixel_slice_u8));

  while (true) {
    wuffs_base__status status =
        wuffs_base__image_decoder__decode_frame_config(b, &fc, src);
    if (status.repr == wuffs_base__note__end_of_data) {
      break;
    } else {
      CHECK_STATUS("decode_frame_config", status);
    }
    wuffs_base__pixel_blend blend =
        ((wuffs_base__frame_config__index(&fc) == 0) ||
         wuffs_base__frame_config__overwrite_instead_of_blend(&fc) ||
         wuffs_base__pixel_format__is_indexed(&pixfmt))
            ? WUFFS_BASE__PIXEL_BLEND__SRC
            : WUFFS_BASE__PIXEL_BLEND__SRC_OVER;

    CHECK_STATUS("decode_frame",
                 wuffs_base__image_decoder__decode_frame(
                     b, &pb, src, blend, g_work_slice_u8, NULL));

    if (n_bytes_out) {
      uint64_t frame_width = wuffs_base__frame_config__width(&fc);
      uint64_t frame_height = wuffs_base__frame_config__height(&fc);
      *n_bytes_out += frame_width * frame_height * bytes_per_pixel;
    }
    if (dst) {
      CHECK_STRING(copy_to_io_buffer_from_pixel_buffer(
          dst, &pb, wuffs_base__frame_config__bounds(&fc)));
    }
  }
  return NULL;
}

const char*  //
do_bench_image_decode(
    const char* (*decode_func)(uint64_t* n_bytes_out,
                               wuffs_base__io_buffer* dst,
                               uint32_t wuffs_initialize_flags,
                               wuffs_base__pixel_format pixfmt,
                               uint32_t* quirks_ptr,
                               size_t quirks_len,
                               wuffs_base__io_buffer* src),
    uint32_t wuffs_initialize_flags,
    wuffs_base__pixel_format pixfmt,
    uint32_t* quirks_ptr,
    size_t quirks_len,
    const char* src_filename,
    size_t src_ri,
    size_t src_wi,
    uint64_t iters_unscaled) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file_fragment(&src, src_filename, src_ri, src_wi));

  bench_start();
  uint64_t n_bytes = 0;
  uint64_t iters = iters_unscaled * g_flags.iterscale;
  for (uint64_t i = 0; i < iters; i++) {
    src.meta.ri = src_ri;
    CHECK_STRING((*decode_func)(&n_bytes, NULL, wuffs_initialize_flags, pixfmt,
                                quirks_ptr, quirks_len, &src));
  }
  bench_finish(iters, n_bytes);
  return NULL;
}

// --------

const char*  //
do_test__wuffs_base__hasher_u32(wuffs_base__hasher_u32* b,
                                const char* src_filename,
                                size_t src_ri,
                                size_t src_wi,
                                uint32_t want) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file_fragment(&src, src_filename, src_ri, src_wi));
  uint32_t have = wuffs_base__hasher_u32__update_u32(
      b, ((wuffs_base__slice_u8){
             .ptr = (uint8_t*)(src.data.ptr + src.meta.ri),
             .len = (size_t)(src.meta.wi - src.meta.ri),
         }));
  if (have != want) {
    RETURN_FAIL("have 0x%08" PRIX32 ", want 0x%08" PRIX32, have, want);
  }
  return NULL;
}

const char*  //
do_test__wuffs_base__hasher_u64(wuffs_base__hasher_u64* b,
                                const char* src_filename,
                                size_t src_ri,
                                size_t src_wi,
                                uint64_t want) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file_fragment(&src, src_filename, src_ri, src_wi));
  uint64_t have = wuffs_base__hasher_u64__update_u64(
      b, ((wuffs_base__slice_u8){
             .ptr = (uint8_t*)(src.data.ptr + src.meta.ri),
             .len = (size_t)(src.meta.wi - src.meta.ri),
         }));
  if (have != want) {
    RETURN_FAIL("have 0x%016" PRIX64 ", want 0x%016" PRIX64, have, want);
  }
  return NULL;
}

const char*  //
do_test__wuffs_base__hasher_bitvec256(wuffs_base__hasher_bitvec256* b,
                                      const char* src_filename,
                                      size_t src_ri,
                                      size_t src_wi,
                                      wuffs_base__bitvec256 want) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file_fragment(&src, src_filename, src_ri, src_wi));
  wuffs_base__bitvec256 have = wuffs_base__hasher_bitvec256__update_bitvec256(
      b, ((wuffs_base__slice_u8){
             .ptr = (uint8_t*)(src.data.ptr + src.meta.ri),
             .len = (size_t)(src.meta.wi - src.meta.ri),
         }));
  if ((have.elements_u64[0] != want.elements_u64[0]) ||
      (have.elements_u64[1] != want.elements_u64[1]) ||
      (have.elements_u64[2] != want.elements_u64[2]) ||
      (have.elements_u64[3] != want.elements_u64[3])) {
    RETURN_FAIL("have 0x%016" PRIX64 "_%016" PRIX64          //
                "_%016" PRIX64 "_%016" PRIX64                //
                ", want 0x%016" PRIX64 "_%016" PRIX64        //
                "_%016" PRIX64 "_%016" PRIX64,               //
                have.elements_u64[3], have.elements_u64[2],  //
                have.elements_u64[1], have.elements_u64[0],  //
                want.elements_u64[3], want.elements_u64[2],  //
                want.elements_u64[1], want.elements_u64[0]);
  }
  return NULL;
}

const char*  //
do_test__wuffs_base__image_decoder(
    wuffs_base__image_decoder* b,
    const char* src_filename,
    size_t src_ri,
    size_t src_wi,
    uint32_t want_width,
    uint32_t want_height,
    wuffs_base__color_u32_argb_premul want_final_pixel) {
  if ((want_width > 16384) || (want_height > 16384) ||
      ((want_width * want_height * 4) > PIXEL_BUFFER_ARRAY_SIZE)) {
    return "want dimensions are too large";
  }

  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file_fragment(&src, src_filename, src_ri, src_wi));
  CHECK_STATUS("decode_image_config",
               wuffs_base__image_decoder__decode_image_config(b, &ic, &src));

  uint32_t have_width = wuffs_base__pixel_config__width(&ic.pixcfg);
  if (have_width != want_width) {
    RETURN_FAIL("width: have %" PRIu32 ", want %" PRIu32, have_width,
                want_width);
  }
  uint32_t have_height = wuffs_base__pixel_config__height(&ic.pixcfg);
  if (have_height != want_height) {
    RETURN_FAIL("height: have %" PRIu32 ", want %" PRIu32, have_height,
                want_height);
  }
  wuffs_base__pixel_config__set(
      &ic.pixcfg, WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL,
      WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, want_width, want_height);

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                     &pb, &ic.pixcfg, g_pixel_slice_u8));
  CHECK_STATUS("decode_frame", wuffs_base__image_decoder__decode_frame(
                                   b, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC,
                                   g_work_slice_u8, NULL));

  uint64_t n = wuffs_base__pixel_config__pixbuf_len(&ic.pixcfg);
  if (n < 4) {
    RETURN_FAIL("pixbuf_len too small");
  } else if (n > PIXEL_BUFFER_ARRAY_SIZE) {
    RETURN_FAIL("pixbuf_len too large");
  } else {
    wuffs_base__color_u32_argb_premul have_final_pixel =
        wuffs_base__peek_u32le__no_bounds_check(&g_pixel_array_u8[n - 4]);
    if (have_final_pixel != want_final_pixel) {
      RETURN_FAIL("final pixel: have 0x%08" PRIX32 ", want 0x%08" PRIX32,
                  have_final_pixel, want_final_pixel);
    }
  }

  if ((have_width > 0) && (have_height > 0)) {
    wuffs_base__color_u32_argb_premul have_final_pixel =
        wuffs_base__pixel_buffer__color_u32_at(&pb, have_width - 1,
                                               have_height - 1);
    if (have_final_pixel != want_final_pixel) {
      RETURN_FAIL("final pixel: have 0x%08" PRIX32 ", want 0x%08" PRIX32,
                  have_final_pixel, want_final_pixel);
    }
  }
  return NULL;
}

const char*  //
do_test__wuffs_base__io_transformer(wuffs_base__io_transformer* b,
                                    const char* src_filename,
                                    size_t src_ri,
                                    size_t src_wi,
                                    size_t want_wi,
                                    uint8_t want_final_byte) {
  if (want_wi > IO_BUFFER_ARRAY_SIZE) {
    return "want_wi is too large";
  }
  wuffs_base__range_ii_u64 workbuf_len =
      wuffs_base__io_transformer__workbuf_len(b);
  if (workbuf_len.min_incl > workbuf_len.max_incl) {
    return "inconsistent workbuf_len";
  }
  if (workbuf_len.max_incl > IO_BUFFER_ARRAY_SIZE) {
    return "workbuf_len is too large";
  }

  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file_fragment(&src, src_filename, src_ri, src_wi));
  CHECK_STATUS("transform_io", wuffs_base__io_transformer__transform_io(
                                   b, &have, &src, g_work_slice_u8));
  if (have.meta.wi != want_wi) {
    RETURN_FAIL("dst wi: have %zu, want %zu", have.meta.wi, want_wi);
  }
  if ((have.meta.wi > 0) &&
      (have.data.ptr[have.meta.wi - 1] != want_final_byte)) {
    RETURN_FAIL("final byte: have 0x%02X, want 0x%02X",
                have.data.ptr[have.meta.wi - 1], want_final_byte);
  }
  return NULL;
}

const char*  //
do_test__wuffs_base__token_decoder(wuffs_base__token_decoder* b,
                                   golden_test* gt) {
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  wuffs_base__token_buffer tok = ((wuffs_base__token_buffer){
      .data = g_have_slice_token,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });

  if (gt->src_filename) {
    CHECK_STRING(
        read_file_fragment(&src, gt->src_filename, gt->src_offset0,
                           gt->src_offset1 ? gt->src_offset1 : SIZE_MAX));
  } else {
    src.meta.closed = true;
  }

  CHECK_STATUS("decode_tokens", wuffs_base__token_decoder__decode_tokens(
                                    b, &tok, &src, g_work_slice_u8));

  uint64_t pos = 0;
  while (tok.meta.ri < tok.meta.wi) {
    wuffs_base__token* t = &tok.data.ptr[tok.meta.ri++];
    uint16_t len = wuffs_base__token__length(t);

    if (wuffs_base__token__value(t) != 0) {
      uint16_t con = wuffs_base__token__continued(t) ? 1 : 0;
      int32_t vmajor = wuffs_base__token__value_major(t);

      if ((have.data.len - have.meta.wi) < 16) {
        return "testlib: output is too long";
      }
      // This 16-bytes-per-token debug format is the same one used by
      // `script/print-json-token-debug-format.c`.
      uint8_t* ptr = have.data.ptr + have.meta.wi;

      wuffs_base__poke_u32be__no_bounds_check(ptr + 0x0, (uint32_t)(pos));
      wuffs_base__poke_u16be__no_bounds_check(ptr + 0x4, len);
      wuffs_base__poke_u16be__no_bounds_check(ptr + 0x6, con);
      if (vmajor > 0) {
        wuffs_base__poke_u32be__no_bounds_check(ptr + 0x8, vmajor);
        uint32_t vminor = wuffs_base__token__value_minor(t);
        wuffs_base__poke_u32be__no_bounds_check(ptr + 0xC, vminor);
      } else if (vmajor == 0) {
        uint8_t vbc = wuffs_base__token__value_base_category(t);
        uint32_t vbd = wuffs_base__token__value_base_detail(t);
        wuffs_base__poke_u32be__no_bounds_check(ptr + 0x8, 0);
        wuffs_base__poke_u8__no_bounds_check(ptr + 0x000C, vbc);
        wuffs_base__poke_u24be__no_bounds_check(ptr + 0xD, vbd);
      } else {
        wuffs_base__poke_u8__no_bounds_check(ptr + 0x0008, 0x01);
        wuffs_base__poke_u56be__no_bounds_check(
            ptr + 0x9, wuffs_base__token__value_extension(t));
      }
      have.meta.wi += 16;
    }

    pos += len;
    if (pos > 0xFFFFFFFF) {
      return "testlib: input is too long";
    }
  }

  if (gt->want_filename) {
    CHECK_STRING(read_file(&want, gt->want_filename));
  } else {
    want.meta.closed = true;
  }
  return check_io_buffers_equal("", &have, &want);
}
