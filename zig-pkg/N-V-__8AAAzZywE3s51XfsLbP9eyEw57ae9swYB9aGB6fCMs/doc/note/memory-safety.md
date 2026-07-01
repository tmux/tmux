# Memory-Safety

Wuffs is a memory-safe programming language, with compiler-enforced [bounds
checking](/doc/note/bounds-checking.md) against buffer overflows and `nullptr`
dereferences. By default, it also prevents uninitialized reads, although some
[zero-initialization is
optional](/doc/note/initialization.md#partial-zero-initialization), for
additional performance.

A deliberate constraint in the Wuffs language is that struct fields cannot
store any pointers to dynamically allocated memory, only to statically
allocated error messages (a `base.status`). When a Wuffs decompressor reads
from or writes to a buffer, that buffer has to be passed on every call, instead
of being passed once during initialization and saved to a field. There is no
ambiguity about whether the caller or callee should free some memory, even when
encountering an error. It's always caller-owned and callee-borrowed.

Wuffs is also trivially safe against things like memory leaks, use-after-frees
and double-frees because Wuffs code doesn't even have the *capability* to
allocate or free memory, other than its transpiled-to-C form providing
`wuffs_foo__bar__alloc` convenience functions to set up before processing a
single byte of input. Wuffs is certainly less powerful than other programming
languages, but **with less power comes easier proof of safety**. Still, Wuffs
is not a toy language and the [example programs](/example) do real work.

Wuffs is a language for writing libraries, and complete programs require
combining Wuffs with another programming language. The obvious example is the
C/C++ programming language, but it could be something else. For argument's
sake, let's call it "PL".

If "PL" is not memory-safe, then clearly "Wuffs + PL" is also not memory-safe
overall. Nonetheless, "Wuffs + PL" is still safer than "just PL", especially if
the Wuffs parts are the only ones that are in direct contact with untrusted
(and possibly maliciously crafted) inputs. There is value in structuring a
program so that the bulk of the computation happens in Wuffs (memory-safe even
for performant, relatively complicated code) and the memory-unsafe
configuration, outside of inner loops, can be in relatively simple code that
prioritizes readability over raw performance.


## Allocation-Free APIs

The inability for Wuffs libraries to allocate memory means that their APIs may
look unfamiliar at first. For example, there isn't a single "decode a JPEG"
function. Instead, [image decoding](/doc/std/image-decoders.md) typically
involves multiple steps:

1. Call into Wuffs with the compressed image input to discover the image
   configuration: its width, height, color model, etc. Only some of the input
   will be consumed.
2. The caller then allocates a sufficiently large pixel buffer for that
   configuration or otherwise issues a "too large to decode" error.
3. Call into Wuffs again, passing that pixel buffer and the remaining input.

For [compression decoding](/doc/std/compression-decoders.md), a fixed size
buffer can be re-used, combining [I/O buffers](/doc/note/io-input-output.md)
with [coroutines](/doc/note/coroutines.md). The
[example/zcat](/example/zcat/zcat.c) program can decompress arbitrarily large
input using static buffers, without ever calling `malloc`. Indeed, on Linux,
that program self-imposes a `SECCOMP_MODE_STRICT` sandbox, prohibiting dynamic
memory allocation (amongst many other powerful but dangerous things) at the
kernel level. The [example/jsonptr](/example/jsonptr/jsonptr.cc) program, which
parses and filters [JSON](https://www.json.org/), is another example that does
useful work within a strict sandbox.

Alternatively, Wuffs-the-library's [auxiliary
code](/doc/note/auxiliary-code.md), augmenting the Wuffs-the-language code with
hand-written C++ helpers, provides higher level (allocating) APIs, albeit with
several trade-offs.


## Thread-Safety

Unless otherwise noted, a Wuffs object is not thread-safe, but Wuffs code also
lacks the capability to create, destroy or otherwise manage threads. There is
also no global mutable state, so two separate Wuffs objects are safe to use
from two separate threads.
