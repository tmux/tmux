# Getting Started

If you just want to **decode an image**:

- [example/toy-aux-image](/example/toy-aux-image/toy-aux-image.cc) demonstrates
  the high-level `wuffs_aux::DecodeImage` C++ function (for decoding still
  images, not animated images) and links to more documentation.
- [example/convert-to-nia](/example/convert-to-nia/convert-to-nia.c)
  demonstrates the low-level C API, which is more complicated but (1) handles
  animation, (2) handles asynchronous I/O, (3) handles metadata and (4) does no
  dynamic memory allocation, so it can run under a `SECCOMP_MODE_STRICT`
  sandbox. Wuffs is already a memory-safe language, but this provides defense
  in depth and can make the security audit trivial.
- [script/print-image-metadata.cc](/script/print-image-metadata.cc)
  demonstrates extracting image metadata like ICC color profiles (in raw form).

In all of these cases, you don't need to first configure or build any Wuffs
code. You can just run a C/C++ compiler, like this (for an unoptimized build):
`g++ example/toy-aux-image/toy-aux-image.cc -o my-toy-aux-image`

If you want a fast (optimized) build, pass `-O3` or equivalent to your C/C++
compiler, or from the Wuffs root directory, run this:
`./build-example.sh example/toy-aux-image`

---

If you're looking to **use the Wuffs standard library** generally, e.g. you
want to safely decode some gzip'ed data in your C program, see the [Wuffs the
Library](/doc/wuffs-the-library.md) document and the [other examples](/example)
instead of this document.

If you're looking to **write your own Wuffs code** outside of its standard
library, e.g. you want to safely decode your own custom file format, see the
[Wuffs the Language](/doc/wuffs-the-language.md) document and the
[/hello-wuffs-c](/hello-wuffs-c) example instead of this document.

If you're looking to **modify the Wuffs language**, it's probably best to ask
the [mailing list](https://groups.google.com/forum/#!forum/wuffs).

If you're looking to **modify the Wuffs standard library**, keep reading.

---

First, install the Go toolchain in order to build the Wuffs tools. To run the
test suite, you might also have to install C compilers like clang and gcc, as
well as C libraries (and their .h files) like libjpeg and libpng, as some tests
compare that Wuffs produces exactly the same output as these other libraries.

Run `git clone https://github.com/google/wuffs.git` to get the latest Wuffs
code, `cd` into its directory and run `go install -v ./cmd/wuffs*` to install
the Wuffs tools. The Wuffs tools that you'll most often use are `wuffsfmt`
(analogous to `clang-format`, `gofmt` or `rustfmt`) and `wuffs` (roughly
analogous to `make`, `go` or `cargo`).

You should now be able to run `wuffs test`. If you only have the `gcc` C
compiler installed, but not `clang`, then you can run `wuffs
test -ccompilers=gcc` instead. If all goes well, you should see some output
containing the word "PASS" multiple times.

Remember to re-run `go install etc` whenever you've modified the Wuffs *tools'*
source code (i.e. `*.go` files) or after a manually issued `git pull`. If
you're only modifying the Wuffs *standard library's* source code (i.e.
`*.wuffs` files), re-running `go install etc` is unnecessary.

If you're modifying just one particular codec in the standard library, such as
the `std/png/*.wuffs` files, then you can exclude unrelated tests by running
`wuffs test std/png`.


## Poking Around

Feel free to edit the `std/lzw/decode_lzw.wuffs` file, which implements the GIF
LZW decoder. After editing, run `wuffs gen std/gif` or `wuffs test std/gif` to
re-generate the C edition of the Wuffs standard library's GIF codec, and
optionally run its tests.

Try deleting an assert statement and re-running `wuffs gen`. The result should
be syntactically valid, but a compile error, as some bounds checks can no
longer be proven.

Find the `var bits : base.u32` line, which declares the bits variable,
implicitly initialized to zero. Try adding `bits -= 1` on a new line of code
after all of the `var` lines. Again, `wuffs gen` should fail, as the
computation can underflow.

Similarly, changing the `4095` in `var prev_code : base.u32[..= 4095]` either
higher or lower should fail.

Try adding `assert false` at various places, which should obviously fail, but
should also cause `wuffs gen` to print what facts the compiler knows at that
point. This can be useful when debugging why Wuffs can't prove something you
think it should be able to.


## Running the Tests

If you've changed any of the tools (i.e. changed any `.go` code), re-run `go
install -v github.com/google/wuffs/cmd/...` and `go test
github.com/google/wuffs/lang/...`.

If you've changed any of the libraries (i.e. changed any `.wuffs` code), run
`wuffs test` or, ideally, `wuffs test -mimic` to also check that Wuffs' output
mimics (i.e. exactly matches) other libraries' output, such as giflib for GIF,
libpng for PNG, etc.

If your library change is an optimization, run `wuffs bench` or `wuffs bench
-mimic` both before and after your change to quantify the improvement. The
mimic benchmark numbers shouldn't change if you're only changing `.wuffs` code,
but seeing zero change in those numbers is a coherence check on any unrelated
system variance, such as software updates or virus checkers running in the
background.
