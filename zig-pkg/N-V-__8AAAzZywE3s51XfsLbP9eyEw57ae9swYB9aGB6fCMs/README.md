Wuffs is a **memory-safe programming language** (and a **standard library**
written in that language) for **Wrangling Untrusted File Formats Safely**.
Wrangling includes parsing, decoding and encoding. Example file formats include
images, audio, video, fonts and compressed archives.

It is [**"ridiculously
fast"**](https://twitter.com/richgel999/status/1481027198530248714).

![Screenshot of a tweet saying "ridiculously
fast"](./test/data/ridiculously-fast.png)

Per its [benchmarks](/doc/benchmarks.md) and other linked-to blog posts:

- It can decode bzip2 **[1.3x faster than `/usr/bin/bzcat`
  (C)](https://nigeltao.github.io/blog/2022/wuffs-bzip2-decoder.html)**.
- It can decode deflate up to **1.4x faster than zlib-the-library (C)**.
- It can decode GIF **2x-6x faster than "giflib" (C), "image/gif" (Go) and
  "gif" (Rust)**.
- It can decode PNG **[1.2x-2.7x faster than "libpng" (C), "image/png" (Go) and
  "png"
  (Rust)](https://nigeltao.github.io/blog/2021/fastest-safest-png-decoder.html)**.


## Goals and Non-Goals

Wuffs' goal is to produce software libraries that are as safe as Go or Rust,
roughly speaking, but as fast as C, and that can be used anywhere C libraries
are used. This includes very large C/C++ projects, such as popular web browsers
and operating systems (using that term to include desktop and mobile user
interfaces, not just the kernel).

[Wuffs the Library](/doc/wuffs-the-library.md) is [available](/release/c) as
transpiled C code. Other C/C++ projects can **use that library without
requiring the [Wuffs the Language](/doc/wuffs-the-language.md) toolchain**.
Those projects can use Wuffs the Library like using any other third party C
library. It's just not hand-written C.

However, unlike hand-written C, Wuffs the Language is safe with respect to
buffer overflows, integer arithmetic overflows and null pointer dereferences. A
key difference between Wuffs and other memory-safe languages is that **all such
checks are done at compile time, not at run time**. If it compiles, it is safe,
with respect to those three bug classes.

The trade-off in aiming for both safety and speed is that Wuffs programs take
longer for a programmer to write, as they have to **explicitly annotate their
programs with proofs of safety**. A statement like `x += 1` unsurprisingly
means to increment the variable `x` by `1`. However, in Wuffs, such a statement
is a compile time error unless the compiler can also prove that `x` is not the
maximal value of `x`'s type (e.g. `x` is not `255` if `x` is a `base.u8`), as
the increment would otherwise overflow. Similarly, an integer arithmetic
expression like `x / y` is a compile time error unless the compiler can also
prove that `y` is not zero.


## Hermeticity

Wuffs is not a general purpose programming language. **It is for writing
libraries, not programs**. Wuffs code is [hermetic](/doc/note/hermeticity.md)
and can only compute (e.g. convert "compressed bytes" to "decompressed bytes").
**It cannot make any syscalls** (e.g. it has no ambient authority to read your
files), implying that it cannot allocate or free memory (and is therefore
trivially safe against things like memory leaks, use-after-frees and
double-frees).

It produces [Sans I/O style](https://sans-io.readthedocs.io/) libraries (but C
libraries, not Python), meaning that they are agnostic to ['function
colors'](https://journal.stuffwithstuff.com/2015/02/01/what-color-is-your-function/).
They can be combined with synchronous or asynchronous I/O, as the library
caller (not library implementation) is responsible for the actual I/O.

The idea isn't to write your whole program in Wuffs, **only the parts that are
both performance-conscious and security-conscious**. For example, while
technically possible, it is unlikely that a Wuffs compiler would be worth
writing entirely in Wuffs.


## What Does Wuffs Code Look Like?

The [`/std/lzw/decode_lzw.wuffs`](/std/lzw/decode_lzw.wuffs) file is a good
example. The [Wuffs the Language](/doc/wuffs-the-language.md) document has more
information on how it differs from other languages in the C family.


## What Does Compile Time Checking Look Like?

For example, making this one-line edit to the LZW codec leads to a compile time
error. `wuffs gen` fails to generate the C code, i.e. fails to compile
(transpile) the Wuffs code to C code:

```diff
diff --git a/std/lzw/decode_lzw.wuffs b/std/lzw/decode_lzw.wuffs
index f878c5e..f10dcee 100644
--- a/std/lzw/decode_lzw.wuffs
+++ b/std/lzw/decode_lzw.wuffs
@@ -98,7 +98,7 @@ pub func lzw_decoder.decode?(dst ptr buf1, src ptr buf1, src_final bool)() {
                        in.dst.write?(x:s)

                        if use_save_code {
-                               this.suffixes[save_code] = c as u8
+                               this.suffixes[save_code] = (c + 1) as u8
                                this.prefixes[save_code] = prev_code as u16
                        }
```

```
$ wuffs gen std/gif
check: expression "(c + 1) as u8" bounds [1 ..= 256] is not within bounds [0 ..= 255] at
/home/n/go/src/github.com/google/wuffs/std/lzw/decode_lzw.wuffs:101. Facts:
    n_bits < 8
    c < 256
    this.stack[s] == (c as u8)
    use_save_code
```

In comparison, this two-line edit will compile (but the "does it decode GIF
correctly" tests then fail):

```diff
diff --git a/std/lzw/decode_lzw.wuffs b/std/lzw/decode_lzw.wuffs
index f878c5e..b43443d 100644
--- a/std/lzw/decode_lzw.wuffs
+++ b/std/lzw/decode_lzw.wuffs
@@ -97,8 +97,8 @@ pub func lzw_decoder.decode?(dst ptr buf1, src ptr buf1, src_final bool)() {
                        // type checking, bounds checking and code generation for it).
                        in.dst.write?(x:s)

-                       if use_save_code {
-                               this.suffixes[save_code] = c as u8
+                       if use_save_code and (c < 200) {
+                               this.suffixes[save_code] = (c + 1) as u8
                                this.prefixes[save_code] = prev_code as u16
                        }
```

```
$ wuffs gen std/gif
gen wrote:      /home/n/go/src/github.com/google/wuffs/gen/c/gif.c
gen unchanged:  /home/n/go/src/github.com/google/wuffs/gen/h/gif.h
$ wuffs test std/gif
gen unchanged:  /home/n/go/src/github.com/google/wuffs/gen/c/gif.c
gen unchanged:  /home/n/go/src/github.com/google/wuffs/gen/h/gif.h
test:           /home/n/go/src/github.com/google/wuffs/test/c/gif
gif/basic.c     clang   PASS (8 tests run)
gif/basic.c     gcc     PASS (8 tests run)
gif/gif.c       clang   FAIL test_lzw_decode: bufs1_equal: wi: got 19311, want 19200.
contents differ at byte 3 (in hex: 0x000003):
  000000: dcdc dc00 00d9 f5f9 f6df dc5f 393a 3a3a  ..........._9:::
  000010: 3a3b 618e c8e4 e4e4 e5e4 e600 00e4 bbbb  :;a.............
  000020: eded 8f91 9191 9090 9090 9190 9192 9192  ................
  000030: 9191 9292 9191 9293 93f0 f0f0 f1f1 f2f2  ................
excerpts of got (above) versus want (below):
  000000: dcdc dcdc dcd9 f5f9 f6df dc5f 393a 3a3a  ..........._9:::
  000010: 3a3a 618e c8e4 e4e4 e5e4 e6e4 e4e4 bbbb  ::a.............
  000020: eded 8f91 9191 9090 9090 9090 9191 9191  ................
  000030: 9191 9191 9191 9193 93f0 f0f0 f1f1 f2f2  ................

gif/gif.c       gcc     FAIL test_lzw_decode: bufs1_equal: wi: got 19311, want 19200.
contents differ at byte 3 (in hex: 0x000003):
  000000: dcdc dc00 00d9 f5f9 f6df dc5f 393a 3a3a  ..........._9:::
  000010: 3a3b 618e c8e4 e4e4 e5e4 e600 00e4 bbbb  :;a.............
  000020: eded 8f91 9191 9090 9090 9190 9192 9192  ................
  000030: 9191 9292 9191 9293 93f0 f0f0 f1f1 f2f2  ................
excerpts of got (above) versus want (below):
  000000: dcdc dcdc dcd9 f5f9 f6df dc5f 393a 3a3a  ..........._9:::
  000010: 3a3a 618e c8e4 e4e4 e5e4 e6e4 e4e4 bbbb  ::a.............
  000020: eded 8f91 9191 9090 9090 9090 9191 9191  ................
  000030: 9191 9191 9191 9193 93f0 f0f0 f1f1 f2f2  ................

wuffs-test-c: some tests failed
wuffs test: some tests failed
```

# Directory Layout

- `lang` holds the Go libraries that implement Wuffs the Language: tokenizer,
  AST, parser, renderer, etc. The Wuffs tools are written in Go, but as
  mentioned above, Wuffs transpiles to C code, and Go is not necessarily
  involved if all you want is to use the C edition of Wuffs.
- `lib` holds other Go libraries, not specific to Wuffs the Language per se.
- `internal` holds internal implementation details, as per Go's [internal
  packages](https://golang.org/s/go14internal) convention.
- `cmd` holds Wuffs the Language' command line tools, also written in Go.
- `std` holds Wuffs the Library's code.
- `release` holds the releases (e.g. in their C form) of Wuffs the Library.
- `test` holds the regular tests for Wuffs the Library.
- `fuzz` holds the fuzz tests for Wuffs the Library.
- `script` holds miscellaneous utility programs.
- `doc` holds documentation.
- `example` holds example programs for Wuffs the Library.
- `hello-wuffs-c` holds an example program for Wuffs the Language.


# Building

See the [BUILD](/BUILD.md) instructions.


# Documentation

- [Getting Started](/doc/getting-started.md). **Start here** if you want to
  play but aren't sure how (and [BUILD](/BUILD.md) doesn't help).
- [Background](/doc/background.md).
- [Benchmarks](/doc/benchmarks.md).
- [Binary Size](/doc/binary-size.md).
- [Changelog](/doc/changelog.md).
- [Glossary](/doc/glossary.md).
- [Related Work](/doc/related-work.md).
- [Roadmap](/doc/roadmap.md).
- [Wuffs the Language](/doc/wuffs-the-language.md) overview.
- [Wuffs the Library](/doc/wuffs-the-library.md) overview and see also [API
  categories](/doc/std).

The [Note](/doc/note) directory also contains various short articles.


# Non-C/C++ Languages

- [dev0x13/pywuffs](https://github.com/dev0x13/pywuffs) holds Python bindings
  for Wuffs the Library.
- Bindings for Go, Rust and other languages are tracked as [issue
  #38](https://github.com/google/wuffs/issues/38).


# Status

Version 0.3 (April 2023) is the latest stable version. Stable means that
its API won't change any further, but being a "version 0.x" means that:

- It will not have long term support.
- Newer versions make no promises about compatibility.

The compiler undoubtedly has bugs. Assertion checking needs more rigor,
especially around side effects and aliasing, and being sufficiently well
specified to allow alternative implementations. Lots of detail needs work, but
the broad brushstrokes are there.

Nonetheless, Wuffs' GIF decoder has shipped in the Google Chrome web browser
[since June
2021](https://chromium-review.googlesource.com/c/chromium/src/+/2940044)
(milestone M93). See also the ["ridiculously
fast"](https://twitter.com/richgel999/status/1481027198530248714) tweet already
mentioned above.


# Discussion

The mailing list is at
[https://groups.google.com/forum/#!forum/wuffs](https://groups.google.com/forum/#!forum/wuffs).


# Contributing

The [CONTRIBUTING.md](/CONTRIBUTING.md) file contains instructions on how to
file the Contributor License Agreement before sending any pull requests (PRs).
Of course, if you're new to the project, it's usually best to discuss any
proposals and reach consensus before sending your first PR.

Source code is [auto-formatted](/doc/note/auto-formatting.md).


# License

This software is distributed under the terms of both the MIT license and the
Apache License (Version 2.0).

See LICENSE for details.


# Disclaimer

This is not an official Google product, it is just code that happens to be
owned by Google.


# Mascot

Tony is an arse-kicking wombat who loves playing
[full-forward](https://en.wikipedia.org/wiki/Full-forward) and hates buffer
overflows.

![WUFFS Logo](./doc/logo/wuffs-acronym-logo-1536x1024.png)


---

Updated on November 2023.
