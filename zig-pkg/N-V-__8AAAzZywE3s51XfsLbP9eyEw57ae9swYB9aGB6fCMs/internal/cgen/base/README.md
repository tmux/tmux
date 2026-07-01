# C Base Code

This directory (`base`) and its sibling
([`auxiliary`](/internal/cgen/auxiliary)) contain C and C++ code respectively.
This C/C++ code combines with the transpiled-to-C/C++ form of the Wuffs code in
the top-level [`std`](/std) directory to create the "single file C/C++ library"
form of the Wuffs standard library.

- From Wuffs-the-language's point of view, this directory contains
  implementations of the built-in `base` package types, such as [I/O
  buffers](/doc/note/io-input-output.md), [ranges and
  rects](/doc/note/ranges-and-rects.md) and [statuses](/doc/note/statuses.md).
- From application code's point of view (Wuffs is for writing libraries not
  applications), this directory also contains helper functions for working with
  those `base` types, such as constructing an I/O buffer from a pointer and a
  length, that aren't relevant to Wuffs-the-language code.
- This directory also contains some functionality (e.g. pixel conversion and
  string conversion) that, in the future, could possibly be Wuffs packages
  under `std`. For optimal performance, they sometimes require programming
  language features (e.g. function-pointer typed variables, SIMD acceleration)
  that are not yet available in Wuffs-the-language (or were not available at
  the time they were added). In the short term, it was more practical to
  implement them in carefully hand-written C/C++, in this directory. In the
  long term, they could migrate out of `base` to be under `std`, but any `base`
  API removal would require a Wuffs version number bump.


## Making Changes

After editing source code in this directory, to see those changes when
generating the C/C++ form of Wuffs' standard library:

    go install github.com/google/wuffs/cmd/wuffs-c && wuffs gen base

This should modify the
[`wuffs-unsupported-snapshot.c`](/release/c/wuffs-unsupported-snapshot.c) file.
