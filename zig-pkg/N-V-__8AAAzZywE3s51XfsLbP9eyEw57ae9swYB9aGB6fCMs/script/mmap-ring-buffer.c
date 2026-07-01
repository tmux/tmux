// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// This program demonstrates mmap'ing a ring-buffer's N bytes of physical
// memory three times, to be a contiguous block of 3*N bytes. The three
// pointers (base + 0*N + i), (base + 1*N + i) and (base + 2*N + i), which are
// different addresses in virtual memory, all alias the same physical address.
//
// Reading or writing a chunk of length M <= N is therefore a simple memcpy,
// without having to explicitly wrap around the ring-buffer boundaries.
//
// This is similar to the technique discussed in
// https://lo.calho.st/quick-hacks/employing-black-magic-in-the-linux-page-table/
//
// This program differs from that web page's discussion by mapping the physical
// memory three times, not just two. This lets us read or write, implicitly
// wrapping, both forwards (after the middle mapping's end) and backwards
// (before the middle mapping's start). That web page only considers forwards
// reads or writes. Backwards reads are useful when decoding a Lempel-Ziv style
// compression format, copying from history (recently decoded bytes).
//
// Its output should be:
//
// middle[-8]  ==  0x00  ==  0x00  ==  middle[131064]
// middle[-7]  ==  0x00  ==  0x00  ==  middle[131065]
// middle[-6]  ==  0x00  ==  0x00  ==  middle[131066]
// middle[-5]  ==  0x00  ==  0x00  ==  middle[131067]
// middle[-4]  ==  0x00  ==  0x00  ==  middle[131068]
// middle[-3]  ==  0x00  ==  0x00  ==  middle[131069]
// middle[-2]  ==  0x20  ==  0x20  ==  middle[131070]
// middle[-1]  ==  0x21  ==  0x21  ==  middle[131071]
// middle[ 0]  ==  0x22  ==  0x22  ==  middle[131072]
// middle[ 1]  ==  0x23  ==  0x23  ==  middle[131073]
// middle[ 2]  ==  0x12  ==  0x12  ==  middle[131074]
// middle[ 3]  ==  0x13  ==  0x13  ==  middle[131075]
// middle[ 4]  ==  0x30  ==  0x30  ==  middle[131076]
// middle[ 5]  ==  0x31  ==  0x31  ==  middle[131077]
// middle[ 6]  ==  0x32  ==  0x32  ==  middle[131078]
// middle[ 7]  ==  0x17  ==  0x17  ==  middle[131079]

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

// We should be able to do:
//
// #include <sys/memfd.h>
//
// to get the memfd_create function signature, but memfd_create is relatively
// recent. For some reason, this #include hits "No such file or directory" on
// Ubuntu 18.04 (linux 4.15, glibc 2.27), and there's also been problems on
// Debian systems. Instead, we explicitly define our own memfd_create.
static int  //
my_memfd_create(const char* name, unsigned int flags) {
  return syscall(__NR_memfd_create, name, flags);
}

#define N (128 * 1024)

void*  //
make_ring_buffer() {
  int page_size = getpagesize();
  if ((N < page_size) || (page_size <= 0) || ((N % page_size) != 0)) {
    return NULL;
  }

  int memfd = my_memfd_create("ring", 0);
  if (memfd == -1) {
    return NULL;
  }
  if (ftruncate(memfd, N) == -1) {
    return NULL;
  }

  // Have the kernel find a contiguous range of unused address space.
  void* base = mmap(NULL, 3 * N, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (base == MAP_FAILED) {
    return NULL;
  }

  // Map that "ring" file 3 times, filling that range exactly.
  for (int i = 0; i < 3; i++) {
    void* p = mmap(base + (i * N), N, PROT_READ | PROT_WRITE,
                   MAP_FIXED | MAP_SHARED, memfd, 0);
    if (p == MAP_FAILED) {
      return NULL;
    }
  }

  close(memfd);
  return base;
}

int  //
main(int argc, char** argv) {
  uint8_t* base = make_ring_buffer();
  if (!base) {
    fprintf(stderr, "could not make ring buffer\n");
    return 1;
  }

  for (int i = 0; i < 8; i++) {
    base[i] = 0x10 + i;
  }

  memcpy(base + N - 2, "\x20\x21\x22\x23", 4);

  base[(0 * N) + 4] = 0x30;
  base[(1 * N) + 5] = 0x31;
  base[(2 * N) + 6] = 0x32;

  uint8_t* middle = base + N;
  for (int i = -8; i < 8; i++) {
    int j = N + i;
    printf("middle[%2d]  ==  0x%02X  ==  0x%02X  ==  middle[%6d]\n", i,
           middle[i], middle[j], j);
  }

  return 0;
}
