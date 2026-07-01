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

# This script, build-all.sh, is a simple coherence check that the tests pass
# and the example programs (including fuzz programs) compile.
#
# If you're just looking to get started with Wuffs, running this script isn't
# necessary (as Wuffs doesn't have the "configure; make; make install" dance or
# its equivalent), although studying this script can help you learn how the
# pieces fit together.
#
# For example, this script builds command line tools, such as `wuffs` and
# `wuffs-fmt`, that are used when *developing* Wuffs, but aren't necessary if
# all you want to do is *use* Wuffs as a third party library. In the latter
# case, the only files you need are those under the release/ directory.
#
# Instead of running this script, you should be able to run the example
# programs (except the `example/toy-genlib` special case) out of the box,
# without having to separately configure, build or install a library:
#
# git clone https://github.com/google/wuffs.git
# cd wuffs
# gcc ./example/zcat/zcat.c
# ./a.out < ./test/data/romeo.txt.gz
#
# See BUILD.md for more discussion.

if [ ! -e wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs root directory."
  exit 1
fi

CC=${CC:-gcc}
CXX=${CXX:-g++}

go install github.com/google/wuffs/cmd/...
go test    github.com/google/wuffs/...
wuffs gen

# Compiler warning flags are discussed at
# http://fastcompression.blogspot.com/2019/01/compiler-warnings.html
# with some further flags from
# https://review.coreboot.org/plugins/gitiles/coreboot/+/17848b65c38c32fa9630925ca8a15203a0617788/Makefile.inc#480
WARNING_FLAGS="-Wall -Werror -Wpedantic -Wcast-qual -Wcast-align -Wpointer-arith -Wfloat-equal -Wundef -Wvla -Wconversion -Wshadow -Wredundant-decls -Wunused-const-variable"
C_WARNING_FLAGS="$WARNING_FLAGS -Wstrict-prototypes -Wold-style-definition"
CXX_WARNING_FLAGS="$WARNING_FLAGS"

echo "Checking snapshot compiles cleanly (as C99)"
$CC -c $C_WARNING_FLAGS                          -std=c99 -Wc++-compat \
    release/c/wuffs-unsupported-snapshot.c -o /dev/null
$CC -c $C_WARNING_FLAGS   -DWUFFS_IMPLEMENTATION -std=c99 -Wc++-compat \
    release/c/wuffs-unsupported-snapshot.c -o /dev/null

echo "Checking snapshot compiles cleanly (as C++11)"
$CXX -c $CXX_WARNING_FLAGS                        -std=c++11 -x c++ \
    release/c/wuffs-unsupported-snapshot.c -o /dev/null
$CXX -c $CXX_WARNING_FLAGS -DWUFFS_IMPLEMENTATION -std=c++11 -x c++ \
    release/c/wuffs-unsupported-snapshot.c -o /dev/null

wuffs genlib -skipgen
wuffs test   -skipgen -mimic
wuffs bench  -skipgen -mimic -reps=1 -iterscale=1

./build-example.sh
./build-fuzz.sh

# ----

echo "Running  gen/bin/example-crc32"
JSON_THINGS_CRC32=$(gen/bin/example-crc32 < test/data/json-things.formatted.json)
if [ "$JSON_THINGS_CRC32" != "cdcc7e35" ]; then
  echo "example-crc32 failed on json-things data"
  exit 1
fi

# ----

echo "Running  gen/bin/example-jsonptr"
JSON_THINGS_CRC32=$(gen/bin/example-jsonptr < test/data/json-things.unformatted.json | gen/bin/example-crc32)
if [ "$JSON_THINGS_CRC32" != "cdcc7e35" ]; then
  echo "example-jsonptr failed on json-things data"
  exit 1
fi

# ----

echo "Running  gen/bin/example-convert-to-nia"
set +e
script/print-nia-checksums.sh | \
    diff --unified test/nia-checksums-of-data.txt /dev/stdin
if [ $? != 0 ]; then
  echo "Unexpected change in test/nia-checksums-of-data.txt"
  exit 1
fi
set -e

# ----

echo "Running  gen/bin/example-mzcat"
set +e
script/print-mzcat-checksums.sh | \
    diff --unified test/mzcat-checksums-of-data.txt /dev/stdin
if [ $? != 0 ]; then
  echo "Unexpected change in test/mzcat-checksums-of-data.txt"
  exit 1
fi
set -e

# ----

for f in gen/bin/fuzz-*; do
  echo "Running  $f"
  $f test/data > /dev/null
done

# ----

echo "DONE: build-all.sh"
