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

# This script fetches Blink's web_tests/images/resources suite.

if [ ! -e ../../wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs test/3pdata directory."
  exit 1
fi

# Check out a specific commit (from May 2023).
git clone --quiet https://github.com/nigeltao/blink-web-tests-images-resources.git
cd blink-web-tests-images-resources
git reset --quiet --hard 9c5a6768184e5b45eaddad8f83ffd87dc5dc8f15
cd ..

# Copy out the resources directory.
rm -rf blinksuite
mv blink-web-tests-images-resources/resources blinksuite
rm -rf blink-web-tests-images-resources
