// Copyright 2018 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WUFFS_INCLUDE_GUARD
#error "Wuffs' .h files need to be included before this file"
#endif

void  //
intentional_segfault() {
  static volatile int* ptr = NULL;
  *ptr = 0;
}

static uint32_t  //
popcount32(uint32_t x) {
  static const uint8_t table[256] = {
      0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,  //
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  //
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  //
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  //
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  //
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  //
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  //
      3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  //
      1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,  //
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  //
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  //
      3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  //
      2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,  //
      3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  //
      3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,  //
      4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,  //
  };
  return table[0xFF & (x >> 0)] + table[0xFF & (x >> 8)] +
         table[0xFF & (x >> 16)] + table[0xFF & (x >> 24)];
}

// jenkins_hash_u32 implements
// https://en.wikipedia.org/wiki/Jenkins_hash_function
static uint32_t  //
jenkins_hash_u32(const uint8_t* data, size_t size) {
  uint32_t hash = 0;
  size_t i = 0;
  while (i != size) {
    hash += data[i++];
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}

// memrandomize is like memcpy or memset but it writes pseudo-random values.
static void*  //
memrandomize(void* dest, uint64_t seed, size_t n) {
  unsigned short xsubi[3];  // See "man 3 nrand48".
  xsubi[0] = (seed >> 0) ^ (seed >> 48);
  xsubi[1] = (seed >> 16);
  xsubi[2] = (seed >> 32);
  for (uint8_t* ptr = (uint8_t*)dest; n--;) {
    *ptr++ = nrand48(xsubi);
  }
  return dest;
}

const char*  //
fuzz(wuffs_base__io_buffer* src, uint64_t hash);

static const char*  //
llvmFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Make a 64-bit hash out of two 32-bit hashes, each on half of the data.
  size_t s2 = size / 2;
  uint32_t hash0 = jenkins_hash_u32(data, s2);
  uint32_t hash1 = jenkins_hash_u32(data + s2, size - s2);
  uint64_t hash = (((uint64_t)hash0) << 32) | ((uint64_t)hash1);

  // The ! means that closed will be true for (size == 0).
  const bool closed = !(1 & popcount32(hash0));

  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader((uint8_t*)data, size, closed);

  const char* msg = fuzz(&src, hash);
  if (msg) {
    if (strnlen(msg, 2047) >= 2047) {
      msg = "fuzzlib: internal error: error message is too long";
    }
    if (closed && strstr(msg, "base: short read")) {
      fprintf(stderr, "short read on a closed io_reader\n");
      intentional_segfault();
    } else if (strstr(msg, "internal error:")) {
      fprintf(stderr, "internal errors shouldn't occur: \"%s\"\n", msg);
      intentional_segfault();
    }
  }
  return msg;
}

#ifdef __cplusplus
extern "C" {
#endif

int  //
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  llvmFuzzerTestOneInput(data, size);
  return 0;
}

#ifdef __cplusplus
}  // extern "C"
#endif

static wuffs_base__io_buffer  //
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

#ifdef WUFFS_CONFIG__FUZZLIB_MAIN

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct {
  int remaining_argc;
  char** remaining_argv;

  bool color;
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

    if (!strcmp(arg, "c") || !strcmp(arg, "color")) {
      g_flags.color = true;
      continue;
    }

    return "main: unrecognized flag argument";
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return NULL;
}

static int g_num_files_processed;

static struct {
  char buf[PATH_MAX];
  size_t len;
} g_relative_cwd;

void  //
errorf(const char* msg) {
  if (g_flags.color) {
    printf("\e[31m%s\e[0m\n", msg);
  } else {
    printf("%s\n", msg);
  }
}

static int  //
visit(char* filename);

static int  //
visit_dir(int fd) {
  int cwd_fd = open(".", O_RDONLY, 0);
  if (fchdir(fd)) {
    errorf("failed");
    fprintf(stderr, "FAIL: fchdir: %s\n", strerror(errno));
    return 1;
  }

  DIR* d = fdopendir(fd);
  if (!d) {
    errorf("failed");
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
    errorf("failed");
    fprintf(stderr, "FAIL: file size out of bounds");
    return 1;
  }

  void* data = NULL;
  if (size > 0) {
    data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
      errorf("failed");
      fprintf(stderr, "FAIL: mmap: %s\n", strerror(errno));
      return 1;
    }
  }

  const char* msg = llvmFuzzerTestOneInput((const uint8_t*)(data), size);
  if (msg) {
    errorf(msg);
  } else if (g_flags.color) {
    printf("\e[32mok\e[0m\n");
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
  return 0;
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
    errorf("failed");
    fprintf(stderr, "FAIL: open: %s\n", strerror(errno));
    return 1;
  }
  if (fstat(fd, &z)) {
    errorf("failed");
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
    errorf("failed");
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

  const char* z = parse_flags(argc, argv);
  if (z) {
    fprintf(stderr, "FAIL: %s\n", z);
    return 1;
  }
  for (int i = 0; i < g_flags.remaining_argc; i++) {
    int v = visit(g_flags.remaining_argv[i]);
    if (v) {
      return v;
    }
  }

  printf("PASS: %d files processed\n", g_num_files_processed);
  return 0;
}

#endif  // WUFFS_CONFIG__FUZZLIB_MAIN
