# Build Instructions

Answering "how do I build Wuffs" depends on what exactly you mean by "Wuffs":

1. The Wuffs library (e.g. image decoders) in C/C++ form.
2. Its example programs (e.g. jsonptr).
3. The Wuffs toolchain (e.g. Wuffs-to-C compiler/proof-checker).
4. The Wuffs library in Wuffs form (converting it to C/C++ code).
5. Everything.


## Quick Start

The rest of this document assumes that you've already checked out and moved
into the Wuffs repository's directory, like this:

```
git clone https://github.com/google/wuffs.git
cd wuffs
```

If you just want to kick the metaphorical tyres:

```
./build-example.sh example/jsonptr
gen/bin/example-jsonptr test/data/rfc-6901-json-pointer.json
```


## Building the Wuffs Library (in C/C++ Form)

There's no build step, in that there's no "configure and make" step needed
before moving on to building the example programs.

To elaborate, transpiling (converting Wuffs-the-library from `*.wuffs` form
into a single `*.c` file) isn't done by whoever *checks out* the Wuffs
repository. It's done by whoever *checks in* code changes.

Wuffs-the-library is provided as [single file C
library](https://github.com/nothings/stb/blob/master/docs/stb_howto.txt). The
example programs just `#include` that file directly.

For your own projects, just copy `release/c/wuffs-$VERSION.c` to your directory
and add that file to your pre-existing build system, or compile an `*.o` object
file directly like below. Remember to define the `WUFFS_IMPLEMENTATION` macro
to compile all of the C code in that single file, not just the "header" part.

```
# Most developers won't have to do this. It just demonstrates how to produce
# wuffs-v0.3.o directly from a C/C++ compiler.
gcc -c -DWUFFS_IMPLEMENTATION -O3 release/c/wuffs-v0.3.c
```


## Building the Example Programs

Just run your favorite C/C++ compiler (e.g. `gcc` or `g++`) on the
`example/foo/*.{c,cc}` file. Pass `-O3` or equivalent for an optimized build:

```
gcc -O3 example/mzcat/mzcat.c -o my-mzcat
./my-mzcat < test/data/romeo.txt.bz2
```

Some example programs require additional libraries:

```
g++ example/imageviewer/imageviewer.cc -lxcb -lxcb-image -lxcb-render -lxcb-render-util

g++ example/sdl-imageviewer/sdl-imageviewer.cc -lSDL2 -lSDL2_image
```

The `build-example.sh` script (an alternative to running your favorite C/C++
compiler directly) takes care of having to remember those additional libraries.

```
./build-example.sh example/sdl-imageviewer
gen/bin/example-sdl-imageviewer test/data/hat.jpeg
```

Building the fuzzers are similar, using `build-fuzz.sh` instead of
`build-example.sh`.


## Building the Wuffs Toolchain

Most developers won't have to do this. See the sections above instead.

But after editing `lang/check/*.go` files, do this:

```
go install ./cmd/wuffs*
```


## Building the Wuffs library (from its Wuffs Form)

Most developers won't have to do this. See the sections above instead.

But after editing `std/jpeg/*.wuffs` files, do this:

```
wuffs gen std/...
```


## Building Everything (All of the Above, Plus Tests, Benchmarks, Etc)

Most developers won't have to do this. See the sections above instead.

But before sending a Pull Request, do this:

```
./build-all.sh
```
