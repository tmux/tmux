# Wuffs' Example Programs

This directory contains example programs that use [Wuffs the
Library](/doc/wuffs-the-library.md). See [Hello `wuffs-c`](/hello-wuffs-c) for
an example program that uses [Wuffs the Language](/doc/wuffs-the-language.md)
and its `wuffs-c` compiler.

Other than the `toy-*` examples, all of these programs do real work. They're
not just programming language toys at the "calculate the Fibonacci sequence"
level of triviality. For example, the [example/crc32](/example/crc32/crc32.cc)
and [example/zcat](/example/zcat/zcat.c) programs are roughly equivalent to
Debian Linux's `/usr/bin/crc32` and `/bin/zcat` programs.


## Building

Running `build-example.sh` from the top level directory will build all of the
examples. To check out and build just one, such as `jsonptr`:

    git clone https://github.com/google/wuffs.git
    cd wuffs
    ./build-example.sh example/jsonptr

When re-building, you only need the last of those three lines. To run it:

    gen/bin/example-jsonptr test/data/json-things.unformatted.json

See also the top-level [BUILD](/BUILD.md) instructions.
