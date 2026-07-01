#!/bin/bash -eu
# Copyright 2020 The Wuffs Authors.
#
# Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
# https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
# <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
# option. This file may not be copied, modified, or distributed
# except according to those terms.
#
# SPDX-License-Identifier: Apache-2.0 OR MIT

# ----------------

# This script runs a JSON parsing program (e.g. example/jsonptr) over a JSON
# test suite (e.g. https://github.com/nst/JSONTestSuite checked out on local
# disk as $HOME/JSONTestSuite).
#
# It runs that program over every command line argument: if a file then itself,
# if a directory then every [iny]_*.json child. It checks for a 0 exit code for
# y_*.json files, 1 for n_*.json files and either 0 or 1 for i_*.json files.

jsonparser=${JSONPARSER:-gen/bin/example-jsonptr}
if [ ! "$(command -v $jsonparser)" ]; then
  if [ ${JSONPARSER:-} ]; then
    echo "Could not run \$JSONPARSER ($jsonparser)."
  else
    echo "Could not run the default JSON parser (gen/bin/example-jsonptr)."
    echo "Run \"./build-example.sh example/jsonptr\" from the Wuffs root directory."
  fi
  exit 1
fi

sources=$@
if [ $# -eq 0 ]; then
  sources=$HOME/JSONTestSuite/test_parsing
fi

num_fail=0
num_ok=0
num_total=0

# ----

test1() {
  set +e

  $jsonparser < $1 >/dev/null 2>/dev/null
  local x=$?

  local basename=${1##*/}
  local ok=
  if   [[ $basename = i_*.json ]]; then
    if [[ ($x == 0 || $x == 1) ]]; then
      ok=1
    fi
  elif [[ $basename = n_*.json ]]; then
    if [[ ($x == 1) ]]; then
      ok=1
    fi
  elif [[ $basename = y_*.json ]]; then
    if [[ ($x == 0) ]]; then
      ok=1
    fi
  fi

  if [ $ok ]; then
    echo "ok      $1"
    ((num_ok++))
  else
    echo "fail    $1"
    ((num_fail++))
  fi
  ((num_total++))

  set -e
}

# ----

for f in $sources; do
  if [ -f $f ]; then
    test1 $f
  elif [ -d $f ]; then
    for g in ${f%/}/[iny]_*.json; do
      test1 $g
    done
  else
    echo "Could not open $f"
    if [ $# -eq 0 ]; then
      echo "Run \"git clone https://github.com/nst/JSONTestSuite.git\" from your home dir."
    fi
    exit 1
  fi
done

echo "$num_fail fail, $num_ok ok, $num_total total"
if [ $num_fail -ne 0 ]; then
  exit 1
fi
