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

# This script fetches the BMPSuite collection of BMP images.

if [ ! -e ../../wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs test/3pdata directory."
  exit 1
fi

curl --silent -O http://entropymine.com/jason/bmpsuite/releases/bmpsuite-2.7.zip
unzip bmpsuite-2.7.zip
mv bmpsuite-2.7 bmpsuite
rm bmpsuite-2.7.zip
