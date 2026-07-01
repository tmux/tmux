#!/bin/bash -eu
# Copyright 2024 The Wuffs Authors.
#
# Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
# https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
# <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
# option. This file may not be copied, modified, or distributed
# except according to those terms.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

# ----------------

# This script fetches libwebp's test data suite.

if [ ! -e ../../wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs test/3pdata directory."
  exit 1
fi

# Check out a specific commit (from July 2024).
git clone --quiet https://github.com/webmproject/libwebp-test-data.git webpsuite
cd webpsuite
git reset --quiet --hard 260804d686f0491e0326a50eef491829dae09e9e
