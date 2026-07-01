// Copyright 2019 The Wuffs Authors.
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
#include <string.h>

// See naive-parse.c and wuffs-parse.c for implementations of this function.
uint32_t parse(char*, size_t);

void run(char* p) {
  size_t n = strlen(p) + 1;  // +1 for the trailing NUL that ends a C string.
  uint32_t i = parse(p, n);
  printf("%" PRIu32 "\n", i);
}

int main(int argc, char** argv) {
  run("0");
  run("12");
  run("56789");
  run("4294967295");  // (1<<32) - 1, aka UINT32_MAX.
  run("4294967296");  // (1<<32).
  run("123456789012");
  return 0;
}
