# Releases (in the C Programming Language)

[Wuffs the Library](/doc/wuffs-the-library.md) ships as a "single file C
library", also known as a "[header file
library](https://github.com/nothings/stb/blob/master/docs/stb_howto.txt)".

To use that library in your C/C++ project, you just need to copy one file from
this directory, or otherwise integrate that one file into your build system.
Typically, pick the latest stable version (but not the unsupported snapshot),
with a filename like `wuffs-vMAJOR.MINOR.c` for version MAJOR.MINOR.

To use that single file as a "foo.c"-like implementation, instead of a
"foo.h"-like header, `#define WUFFS_IMPLEMENTATION` before `#include`'ing or
compiling it.


# Latest Stable Version

**Version 0.3 (April 2023)** is the latest stable version. Stable means that
its API won't change any further, but being a "version 0.x" means that:

- It will not have long term support.
- Newer versions make no promises about compatibility.


# Mirror

Wuffs' canonical repository is located at
[github.com/google/wuffs](https://github.com/google/wuffs). If you just want to
*use* Wuffs-the-library in its C form (instead of *modifying* its Wuffs form or
otherwise needing Wuffs-the-language tools), a mirror of just the `release/c`
directory is located at
[github.com/google/wuffs-mirror-release-c](https://github.com/google/wuffs-mirror-release-c).
By excluding everything else, the mirror is much smaller than and changes much
less frequently than the canonical repository.
