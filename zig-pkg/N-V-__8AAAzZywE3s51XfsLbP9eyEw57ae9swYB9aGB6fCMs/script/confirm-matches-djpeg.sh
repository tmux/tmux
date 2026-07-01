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

# This script checks that Wuffs's JPEG decoder and djpeg (from libjpeg or
# libjpeg-turbo) produce the same output (using djpeg's default configuration,
# with no further command line arguments).

DJPEG_BINARY=${DJPEG_BINARY:-djpeg}

if [ ! -e wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs root directory."
  exit 1
elif [ ! -e gen/bin/example-convert-to-nia ]; then
  echo "Run \"./build-example.sh example/convert-to-nia\" first."
  exit 1
elif [ ! -e gen/bin/example-crc32 ]; then
  echo "Run \"./build-example.sh example/crc32\" first."
  exit 1
elif [ -z $(which $DJPEG_BINARY) ]; then
  echo "Could not find DJPEG_BINARY \"$DJPEG_BINARY\"."
  exit 1
elif [[ ! $($DJPEG_BINARY -version 2>&1 >/dev/null) =~ ^libjpeg ]]; then
  echo "Could not get libjpeg version from DJPEG_BINARY \"$DJPEG_BINARY\"."
  exit 1
fi

djpeg_version=$($DJPEG_BINARY -version 2>&1 >/dev/null)
if [[ $djpeg_version =~ ^libjpeg.*version.2\.0 ||
      $djpeg_version =~ ^libjpeg.*version.2\.1 ||
      $djpeg_version =~ ^libjpeg.*version.3\.0\.0 ]]; then
  # "Wuffs's std/jpeg matches djpeg" needs these libjpeg-turbo commits:
  #  - 6d91e950 Use 5x5 win & 9 AC coeffs when smoothing DC scans  (in 2.1.0)
  #  - dbae5928 Fix interblock smoothing with narrow prog. JPEGs   (in 3.0.1)
  #  - 9b704f96 Fix block smoothing w/vert.-subsampled prog. JPEGs (in 3.0.1)
  # https://github.com/libjpeg-turbo/libjpeg-turbo/commit/6d91e950c871103a11bac2f10c63bf998796c719
  # https://github.com/libjpeg-turbo/libjpeg-turbo/commit/dbae59281fdc6b3a6304a40134e8576d50d662c0
  # https://github.com/libjpeg-turbo/libjpeg-turbo/commit/9b704f96b2dccc54363ad7a2fe8e378fc1a2893b
  echo "\"$djpeg_version\" too old from DJPEG_BINARY \"$DJPEG_BINARY\"."
  exit 1
fi

sources=$@
if [ $# -eq 0 ]; then
  sources=test/data/*.jpeg
fi

# ----

result=0

handle() {
  local want=$($DJPEG_BINARY $1 2>/dev/null | gen/bin/example-crc32)
  if [ "$want" == "00000000" ]; then
    return
  fi

  local have=$(gen/bin/example-convert-to-nia -output-netpbm <$1 2>/dev/null | gen/bin/example-crc32)
  if [ "$want" == "$have" ]; then
    echo "Match           $1"
    return
  fi

  # Image file format specifications cover how to correctly decode complete
  # input but usually do not mandate exactly how to decode truncated input.
  #
  # A short SOS (Start Of Scan) bitstream is similar. Strictly speaking, such
  # JPEG images are invalid. In practice, decoders have some discretion in how
  # many implicit zero bits to produce before giving up. Giving up effectively
  # truncates the input, even if we haven't reached the actual end of file.
  #
  # The ectn (example-convert-to-nia) program runs under a SECCOMP_MODE_STRICT
  # sandbox, which prohibits dynamic memory allocation (calling malloc). Using
  # only statically allocated memory, its maximum supported image size has to
  # be configured at compile time, not probed at runtime. The ectn program can
  # reject some perfectly valid (but large) JPEG images that the djpeg program
  # is happy to process (if run on a system with plentiful RAM).
  #
  # These "image is too large" rejections are a property of the ectn program
  # (and its sandbox), not of the Wuffs library. Wuffs' JPEG decoder can
  # happily decode such images if the library caller gives it enough memory.
  #
  # Print "trunc" vs "sSOSb" vs "large" vs "other" to distinguish these cases.
  local error=$(gen/bin/example-convert-to-nia -output-netpbm <$1 2>&1 >/dev/null)
  if [[ $error =~ ^jpeg:\ truncated\ input$ ]]; then
    echo "Differ (trunc)  $1"
  elif [[ $error =~ ^jpeg:\ short\ SOS\ bitstream$ ]]; then
    echo "Differ (sSOSb)  $1"
  elif [[ $error =~ ^main:\ image\ is\ too\ large ]]; then
    echo "Differ (large)  $1"
  else
    echo "Differ (other)  $1"
    result=1
  fi
}

# ----

for f in $sources; do
  if [ -f $f ]; then
    handle $f
  elif [ -d $f ]; then
    for g in `find $f -type f | LANG=C sort`; do
      handle $g
    done
  else
    echo "Could not open $f"
    exit 1
  fi
done

exit $result
