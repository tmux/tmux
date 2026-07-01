#!/bin/bash -eu
# Copyright 2019 The Wuffs Authors.
#
# Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
# https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
# <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
# option. This file may not be copied, modified, or distributed
# except according to those terms.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

# ----------------

# This script measures Wuffs benchmarks over recent commits. For example:
#
# PACKAGE=deflate FOCUS=wuffs_deflate_decode_100k script/bench-history.sh

num_commits=${NUM_COMMITS:-100}
cc=${CC:-gcc}
package=${PACKAGE:-gif}
focus=${FOCUS:-wuffs_gif_decode_1000k_full_init}
iterscale=${ITERSCALE:-50}
reps=${REPS:-20}

# ----

save=`git log -1 --pretty=format:"%H"`

i=0
while [ $i -lt $num_commits ]; do
  set +e
  $cc -O3 -o bench-history.out test/c/std/$package.c
  if [ $? -ne 0 ]; then
    this_metric='compile_failed'
  else
    this_metric=`./bench-history.out -bench -focus=$focus -iterscale=$iterscale -reps=$reps | benchstat /dev/stdin | sed -ne '/^name.*speed$/,$ p' | grep $focus`
    if [ $? -ne 0 ]; then
      this_metric='bench_failed'
    fi
  fi
  set -e

  this_hash=`git log -1 --pretty=format:"%h"`
  echo $this_hash $this_metric
  git checkout --quiet HEAD^
  i=$((i + 1))
done

rm -f ./bench-history.out
git checkout --quiet $save
