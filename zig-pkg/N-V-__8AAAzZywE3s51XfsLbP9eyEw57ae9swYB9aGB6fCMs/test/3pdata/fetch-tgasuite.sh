#!/bin/bash -eu
# Copyright 2022 The Wuffs Authors.
#
# Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
# https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
# <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
# option. This file may not be copied, modified, or distributed
# except according to those terms.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

# ----------------

# This script fetches the TrueVision TGA test suite.

if [ ! -e ../../wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs test/3pdata directory."
  exit 1
fi

# Check out a specific commit (from January 2022).
git clone --quiet https://github.com/image-rs/image.git
cd image
git reset --quiet --hard 2e1d2e5928077eae7a92f8b5cac83ca1ae0db0cd
cd ..

# Copy out the *.tga files.
mkdir -p tgasuite
cp image/tests/images/tga/testsuite/*.tga  tgasuite
cp image/tests/regression/tga/*.tga        tgasuite
rm -rf image
