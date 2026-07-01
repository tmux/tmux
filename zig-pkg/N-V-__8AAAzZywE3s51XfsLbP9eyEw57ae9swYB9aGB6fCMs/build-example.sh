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

# See BUILD.md and build-all.sh for more discussion.

if [ ! -e wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs root directory."
  exit 1
fi

CC=${CC:-gcc}
CXX=${CXX:-g++}
LDFLAGS=${LDFLAGS:-}

# The "-fdata-sections -ffunction-sections -Wl,--gc-sections" produces smaller
# binaries. See commit 41fce8a8 "Strip examples of unused data and functions".
CFLAGS=${CFLAGS:--O3 -fdata-sections -ffunction-sections -Wall -Wl,--gc-sections}
CXXFLAGS=${CXXFLAGS:--O3 -fdata-sections -ffunction-sections -Wall -Wl,--gc-sections}

mkdir -p gen/bin

sources=$@
if [ $# -eq 0 ]; then
  sources=example/*
fi

for f in $sources; do
  f=${f%/}
  f=${f##*/}
  if [ -z $f ]; then
    continue
  fi

  if [ $f = imageviewer ]; then
    # example/imageviewer is unusual in that needs additional libraries.
    echo "Building (C++) gen/bin/example-$f"
    $CXX $CXXFLAGS example/$f/*.cc \
        $LDFLAGS -lxcb -lxcb-image -lxcb-render -lxcb-render-util \
        -o gen/bin/example-$f
  elif [ $f = "sdl-imageviewer" ]; then
    # example/sdl-imageviewer is unusual in that needs additional libraries.
    echo "Building (C++) gen/bin/example-$f"
    $CXX $CXXFLAGS example/$f/*.cc \
        $LDFLAGS -lSDL2 -lSDL2_image \
        -o gen/bin/example-$f
  elif [ $f = "toy-genlib" ]; then
    # example/toy-genlib is unusual in that it uses separately compiled
    # libraries (built by "wuffs genlib", e.g. by running build-all.sh) instead
    # of directly #include'ing Wuffs' .c files.
    if [ -e gen/lib/c/$CC-static/libwuffs.a ]; then
      echo "Building (C)   gen/bin/example-$f"
      $CC $CFLAGS -static -I.. example/$f/*.c gen/lib/c/$CC-static/libwuffs.a \
          $LDFLAGS -o gen/bin/example-$f
    else
      echo "Skipping gen/bin/example-$f; run \"wuffs genlib\" first"
    fi
  elif [ -e example/$f/*.c ]; then
    echo "Building (C)   gen/bin/example-$f"
    $CC  $CFLAGS              example/$f/*.c  $LDFLAGS -o gen/bin/example-$f
  elif [ $f = "jsonfindptrs" ]; then
    echo "Building (C++) gen/bin/example-$f"
    $CXX $CXXFLAGS -std=c++17 example/$f/*.cc $LDFLAGS -o gen/bin/example-$f
  elif [ -e example/$f/*.cc ]; then
    echo "Building (C++) gen/bin/example-$f"
    $CXX $CXXFLAGS            example/$f/*.cc $LDFLAGS -o gen/bin/example-$f
  fi
done
