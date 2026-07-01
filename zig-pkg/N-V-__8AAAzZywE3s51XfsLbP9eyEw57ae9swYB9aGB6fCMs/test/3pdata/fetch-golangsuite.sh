#!/bin/bash -eu
# Copyright 2023 The Wuffs Authors.
#
# Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
# https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
# <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
# option. This file may not be copied, modified, or distributed
# except according to those terms.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

# ----------------

# This script fetches golang/go's src/image/testdata suite.

if [ ! -e ../../wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs test/3pdata directory."
  exit 1
fi

# Check out a specific commit (from May 2023).
git clone --quiet https://github.com/nigeltao/golang-go-src-image-testdata.git
cd golang-go-src-image-testdata
git reset --quiet --hard f2f7970f9054b910148be4e0dbda94b392c58700
cd ..

# Copy out the testdata directory.
rm -rf golangsuite
mv golang-go-src-image-testdata/testdata golangsuite
rm -rf golang-go-src-image-testdata
