# Background

Decoding untrusted data, such as images downloaded from across the web, has a
long history of security vulnerabilities. As of 2019, libpng is over 20 years
old, and the [PNG specification is dated 2003](https://www.w3.org/TR/PNG/), but
that well examined C library is still getting [CVE's published in
2019](https://www.cvedetails.com/vulnerability-list/vendor_id-7294/year-2019/Libpng.html).

Sandboxing and fuzzing can mitigate the danger, but they are reactions to C's
fundamental unsafety. Newer programming languages remove entire classes of
potential security bugs. Buffer overflows and null pointer dereferences are
amongst the most well known.

Less well known are integer overflow bugs. Offset-length pairs, defining a
sub-section of a file, are seen in many file formats, such as OpenType fonts
and PDF documents. A conscientious C programmer might think to check that a
section of a file or a buffer is within bounds by writing `if (offset + length
< end)` before processing that section, but that addition can silently
overflow, and a maliciously crafted file might bypass the check.

A variation on this theme is where `offset` is a pointer, exemplified by
[capnproto's
CVE-2017-7892](https://github.com/sandstorm-io/capnproto/blob/master/security-advisories/2017-04-17-0-apple-clang-elides-bounds-check.md)
and [another
example](https://www.blackhat.com/docs/us-14/materials/us-14-Rosenberg-Reflections-on-Trusting-TrustZone.pdf).
For a pointer-typed offset, witnessing such a vulnerability can depend on both
the malicious input itself and the addresses of the memory the software used to
process that input. Those addresses can vary from run to run and from system to
system, e.g. 32-bit versus 64-bit systems and whether dynamically allocated
memory can have sufficiently high address values, and that variability makes it
harder to reproduce and to catch such subtle bugs from fuzzing.

In C, some integer overflow is *undefined behavior*, as per [the C99 spec
section 3.4.3](http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf). In
Go, integer overflow is [silently
ignored](https://golang.org/ref/spec#Integer_overflow). In Rust, integer
overflow is [checked at run time in debug mode and silently ignored in release
mode](http://huonw.github.io/blog/2016/04/myths-and-legends-about-integer-overflow-in-rust/)
by default, as the run time performance penalty was deemed too great. In Swift,
it's a [run time
error](https://developer.apple.com/library/content/documentation/Swift/Conceptual/Swift_Programming_Language/AdvancedOperators.html#//apple_ref/doc/uid/TP40014097-CH27-ID37).
In D, it's [configurable](http://dconf.org/2017/talks/alexandrescu.pdf). Other
languages like Python and Haskell can automatically spill into 'big integers'
larger than 64 bits, but this can have a performance impact when such integers
are used in inner loops.

Even if overflow is checked, it is usually checked at run time. Similarly,
modern languages do their bounds checking at run time. An expression like
`a[i]` is really `if ((0 <= i) && (i < a.length)) { use a[i] } else { throw }`,
in mangled pseudo-code. Compilers for these languages can often eliminate many
of these bounds checks, e.g. if `i` is an iterator index, but not always all of
them.

The run time cost is small, measured in nanoseconds. But if an image decoding
library has to eat this cost per pixel, and you have a megapixel image, then
nanoseconds become milliseconds, and milliseconds can matter.

In comparison, in Wuffs, all bounds checks and arithmetic overflow checks
happen at compile time, with zero run time overhead.
