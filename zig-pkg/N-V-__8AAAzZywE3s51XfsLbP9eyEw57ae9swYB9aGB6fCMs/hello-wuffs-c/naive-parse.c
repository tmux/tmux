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
#include <stdlib.h>

uint32_t parse(char* p, size_t n) {
  uint32_t ret = 0;
  for (size_t i = 0; (i < n) && p[i]; i++) {
    ret = (10 * ret) + (p[i] - '0');
  }
  return ret;
}
