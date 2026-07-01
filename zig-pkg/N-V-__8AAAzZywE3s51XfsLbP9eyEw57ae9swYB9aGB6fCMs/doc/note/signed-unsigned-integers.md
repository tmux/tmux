# Signed and Unsigned Integers

In mathematics, if `i` is an integer then `((i + 1) > i)` is always true, and
similarly `((i - 1) < i)`. In computing, they're almost always true, but for
fixed width integers (e.g. 32-bit integers), they can be false due to overflow
or underflow.

For *signed* 32-bit integers, the discontinuity happens at roughly +2 billion
and -2 billion. For *unsigned* 32-bit integers, it's at roughly +4 billion and
at zero. In practice, many human-scale numbers (such as the number of employees
that a person manages, a domain name's string length or a photo's height in
pixels) are small enough that ±2 billion won't ever be reached, but zero is
obviously common (some employees don't manage anyone). In other programming
languages, using *signed* integers means that, in practice, one can
conveniently forget about the discontinuities, even when adding or subtracting
other human-scale numbers, and this almost always works fine.

Wuffs is concerned about always working, not just almost always working. Wuffs
programs *always* have to consider the discontinuities, wherever they may lie,
so there is less advantage to working with signed integer types, and less
disadvantage to working with unsigned integer types. Unlike in C and similar
programming languages, there is no way to forget that `(i - 1)` might be very
large, when `i` is zero.

The unsigned types are arguably more natural - after all, they are part of what
mathematics calls the natural numbers - and stronger inferences can be made
from them. For example, barring overflow, `(x <= (x + i))` is trivially true if
`i` is non-negative. Similarly, when [bounds
checking](/doc/note/bounds-checking.md) a slice or array expression like
`a[i]`, proving `(i >= 0)` is trivial when `i` has unsigned integer type.

Wuffs programs therefore usually work with unsigned integer types: `base.u32`
instead of `base.i32`. In fact, Wuffs' standard library, as of version 0.2
(December 2019), uses *only* unsigned integer types, yet still implements
full-featured decoders for e.g. the GIF and ZLIB formats.


## Signed Integers in Other Languages

Conversely, using C's or Go's or Java's (signed) `int` type can sometimes be
imperfect. For example, some libraries use `int` for an image's width or height
in pixels. This is undeniably convenient, but might not be able to represent a
3 billion pixel high image. It is possible to craft a PNG image that high, one
that is perfectly valid according to the [PNG
specification](https://www.w3.org/TR/2003/REC-PNG-20031110/#11IHDR).

Similarly, even when using the 'correct' width (width as in fixed width
integer, not as in image pixel width), the difference between signed and
unsigned can matter. For example, as Java doesn't have unsigned integer types,
care must be taken when implementing the [LZ4 compression
format](https://github.com/lz4/lz4/issues/792) to exactly match `uint32_t`
modular arithmetic.
