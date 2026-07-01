// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

#include <stdio.h>

// TODO: this 'rough edge' shouldn't be necessary. See
// https://github.com/google/wuffs/issues/24
#define WUFFS_CONFIG__MODULE__BASE

#define WUFFS_IMPLEMENTATION
#include "./parse.c"

uint32_t parse(char* p, size_t n) {
  wuffs_base__status status;

  // This next line of code allocates a wuffs_demo__parser on the stack. Stack
  // allocation in C means uninitialized memory, so we need to call
  // wuffs_demo__parser__initialize afterwards.
  //
  // An alternative, allocating on the heap and initializing in a single
  // function call, is to say:
  //
  //  wuffs_demo__parser* parser = wuffs_demo__parser__alloc();
  //  if (!parser) {
  //    // Out of memory.
  //    return 0;
  //  }
  //  // No need to call wuffs_demo__parser__initialize, but don't forget to
  //  // free(parser) before this function returns, taking extra care if this
  //  // function has multiple return points. Wuffs has no destructor functions
  //  // and Wuffs types never hold or own any resources in the RAII sense.
  //  // Just free the memory.
  //
  // For stack allocation, the C compiler needs to know
  // sizeof(wuffs_demo__parser), but that compile-time value isn't guaranteed
  // to be stable across Wuffs versions. Stack allocation is therefore only
  // valid when the C file also #define's WUFFS_IMPLEMENTATION. When linking
  // against a separately built Wuffs library (and the Wuffs types in this
  // compilation are incomplete types), you'll have to use heap allocation.
  wuffs_demo__parser parser;

  // Initialize (and check status). An error here means that bad arguments were
  // passed to wuffs_demo__parser__initialize.
  //
  // There are two other categories of not-OK status values, notes and
  // suspensions, but they won't be encountered in this example.
  status = wuffs_demo__parser__initialize(&parser, sizeof__wuffs_demo__parser(),
                                          WUFFS_VERSION, 0);
  if (!wuffs_base__status__is_ok(&status)) {
    printf("initialize: %s\n", wuffs_base__status__message(&status));
    return 0;
  }

  // True means that the wuffs_base__io_buffer is closed - we are at the end of
  // the input. False would mean that there might be additional data in the
  // byte stream (that this buffer is not large enough to hold all at once).
  //
  // In general, Wuffs' coroutine and suspension status mechanisms let it parse
  // arbitrarily large data streams using fixed sized buffers, but that won't
  // be encountered in this example.
  wuffs_base__io_buffer iobuf =
      wuffs_base__ptr_u8__reader((uint8_t*)p, n, true);

  // Parse (and check status). An error here means that we had invalid input
  // (i.e. "#not a digit" or "#too large").
  //
  // There are two other categories of not-OK status values, notes and
  // suspensions, but they won't be encountered in this example.
  status = wuffs_demo__parser__parse(&parser, &iobuf);
  if (!wuffs_base__status__is_ok(&status)) {
    printf("parse: %s\n", wuffs_base__status__message(&status));
    return 0;
  }

  return wuffs_demo__parser__value(&parser);
}
