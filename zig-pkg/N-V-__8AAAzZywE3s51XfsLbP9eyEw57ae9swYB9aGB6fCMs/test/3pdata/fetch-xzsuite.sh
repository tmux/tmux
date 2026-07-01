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

# This script fetches xz's tests/files suite.

if [ ! -e ../../wuffs-root-directory.txt ]; then
  echo "$0 should be run from the Wuffs test/3pdata directory."
  exit 1
fi

# Check out a specific commit (from February 2024).
#
# ---
#
# The remaining comment below was written on 2024-04-05.
#
# YYYY-MM-DD dates might appear off by one, due to timezones.
#
# ---
#
# The nigeltao/xz-tests-files repository is a mirror of binary data (test
# files). It combines test files from two git repositories:
#
#  - https://github.com/tukaani-project/xz
#  - https://www.nongnu.org/lzip/
#
# ---
#
# The https://github.com/tukaani-project/xz git repository (a mirror of the
# canonical xz repository) was recently taken offline. It was compromised and
# contains a backdoor:
#
#  - https://www.openwall.com/lists/oss-security/2024/03/29/4
#
# The canonical xz git repository is still online at:
#
#  - https://git.tukaani.org/?p=xz.git
#
# ---
#
# As of nigeltao/xz-tests-files's 6e0194063a6dc205d4db35e307778efe1c9f1875,
# that repo's mirroring of the xz repo's tests/files directory was most
# recently updated by d082a076f3e1da05aa478eac4bbba24744b3a1c0 on 2024-02-13,
# the commit immediately prior to 6e0194063a6dc205d4db35e307778efe1c9f1875:
#
#  - https://github.com/nigeltao/xz-tests-files/commit/d082a076f3e1da05aa478eac4bbba24744b3a1c0
#  - https://github.com/nigeltao/xz-tests-files/commit/6e0194063a6dc205d4db35e307778efe1c9f1875
#
# ---
#
# nigeltao/xz-tests-files at 6e0194063a6dc205d4db35e307778efe1c9f1875 does
# *not* contain the known-to-smuggle-malicious-payloads test files committed by
# Jia Tan on 2024-02-23 and 2024-03-09. It also excludes Jia Tan's possibly
# harmless update to the RISC-V test files (also on 2024-03-09). Specifically,
# nigeltao/xz-tests-files does not contain these five files:
#
#  - tests/files/bad-3-corrupt_lzma2.xz
#  - tests/files/bad-dict_size.lzma
#  - tests/files/good-2cat.xz
#  - tests/files/good-large_compressed.lzma
#  - tests/files/good-small_compressed.lzma
#
# They were added and updated to the xz repo by Jia Tan in:
#
#  - https://git.tukaani.org/?p=xz.git;a=commit;h=cf44e4b7f5dfdbf8c78aef377c10f71e274f63c0
#  - https://git.tukaani.org/?p=xz.git;a=commit;h=6e636819e8f070330d835fce46289a3ff72a7b89
#
# The possibly-harmless RISC-V test files update was:
#
#  - https://git.tukaani.org/?p=xz.git;a=commit;h=0b4ccc91454dbcf0bf521b9bd51aa270581ee23c
#
# ---
#
# nigeltao/xz-tests-files does admittedly contain these two files:
#
#  - tests/files/good-1-riscv-lzma2-1.xz
#  - tests/files/good-1-riscv-lzma2-2.xz
#
# They were added and updated to the xz repo by Jia Tan on 2024-01-22 and
# 2024-01-23 (and 2024-03-09, as mentioned above, but that's excluded):
#
#  - https://git.tukaani.org/?p=xz.git;a=commit;h=e2870db5be1503e6a489fc3d47daf950d6f62723
#  - https://git.tukaani.org/?p=xz.git;a=commit;h=3060e1070b2421b26c0e17794c1307ec5622f11d
#
# These two files (before or after the 0b4ccc91454dbcf0bf521b9bd51aa270581ee23c
# 2024-03-09 excluded update) are not believed to be malicious.
git clone --quiet https://github.com/nigeltao/xz-tests-files.git
cd xz-tests-files
git reset --quiet --hard 6e0194063a6dc205d4db35e307778efe1c9f1875
cd ..

# Trim the git metadata.
rm -rf xzsuite
rm -rf xz-tests-files/.git
mv xz-tests-files xzsuite
