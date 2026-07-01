#!/bin/bash -eu
# Copyright 2018 The Wuffs Authors.
#
# Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
# https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
# <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
# option. This file may not be copied, modified, or distributed
# except according to those terms.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

# ----------------

# See build-all.sh for commentary.

if [ ! -e wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs root directory."
  exit 1
fi

CC=${CC:-gcc}
CXX=${CXX:-g++}
LDFLAGS=${LDFLAGS:-}

# The "-fdata-sections -ffunction-sections -Wl,--gc-sections" produces smaller
# binaries. See commit 41fce8a8 "Strip examples of unused data and functions".
CFLAGS=${CFLAGS:--O3 -fdata-sections -ffunction-sections -Wl,--gc-sections}
CXXFLAGS=${CXXFLAGS:--O3 -fdata-sections -ffunction-sections -Wl,--gc-sections}

mkdir -p gen/bin

sources=$@
if [ $# -eq 0 ]; then
  sources=fuzz/c/std/*_fuzzer.c*
fi

for f in $sources; do
  f=${f%_fuzzer.c}
  f=${f%_fuzzer.cc}
  f=${f%/}
  f=${f##*/}
  if [ -z $f ]; then
    continue
  fi

  if [   -e fuzz/c/std/${f}_fuzzer.c  ]; then
    echo "Building (C)   gen/bin/fuzz-$f"
    $CC  $CFLAGS   -DWUFFS_CONFIG__FUZZLIB_MAIN fuzz/c/std/${f}_fuzzer.c  \
        $LDFLAGS -o gen/bin/fuzz-$f
  elif [ -e fuzz/c/std/${f}_fuzzer.cc ]; then
    echo "Building (C++) gen/bin/fuzz-$f"
    $CXX $CXXFLAGS -DWUFFS_CONFIG__FUZZLIB_MAIN fuzz/c/std/${f}_fuzzer.cc \
        $LDFLAGS -o gen/bin/fuzz-$f
  fi
done
