# Wuffs' Fuzzer Programs

This directory contains multiple programs to fuzz Wuffs' implementations of
various codecs. For example, `gif_fuzzer.c` is a program to fuzz Wuffs' GIF
implementation.

They are typically run indirectly, by a fuzzing framework such as
[OSS-Fuzz](https://github.com/google/oss-fuzz). That repository's
`projects/wuffs` directory contains the complementary configuration for this
directory's code.

When working on these files, it is possible to run them directly on an explicit
test suite, in order to speed up the edit-compile-run cycle. Look for
`WUFFS_CONFIG__FUZZLIB_MAIN` for more details, and in `seed_corpora.txt` for
suggested test data.


## Building

Running `build-fuzz.sh` from the top level directory will build all of the
fuzzers. To check out and build just one, such as `json_fuzzer`:

    git clone https://github.com/google/wuffs.git
    cd wuffs
    ./build-fuzz.sh fuzz/c/std/json_fuzzer.cc

When re-building, you only need the last of those three lines. To run it:

    gen/bin/fuzz-json test/data/json-things.*
