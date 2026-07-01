# Coroutines

In Wuffs, coroutines are functions that can suspend their execution in the
middle of the function body, yielding (returning) a
[status](/doc/note/statuses.md) that is a suspension. Calling that function
again will resume that function where it left off. Calling any other coroutine
function on the same receiver, whilst it is suspended, will result in an error.

Unlike coroutines in other programming languages, Wuffs coroutines cannot
return other values in addition to that status, and statuses don't carry
additional fields. Wuffs coroutines cannot be used as generators directly,
although they can produce to and consume from buffers as side effects. The
built-in I/O types (`base.io_reader` and `base.io_writer`) have coroutine
methods that are exceptions to this "no additional return value" rule, and are
special-cased by the Wuffs compiler.

Another difference with other programming languages is that Wuffs coroutines'
arguments can change across a suspension and resumption. The mechanism for
resuming a coroutine, for C code that calls into Wuffs-transpiled-to-C, is to
simply call the C function again, with the same `self` receiver (the first
argument), but with potentially different other arguments.

As of Wuffs version 0.2 (December 2019), the language and standard library use
only two suspension status values, both [I/O](/doc/note/io-input-output.md)
related: `"$short read"` and `"$short write"` are used when a source buffer is
empty (and needs re-filling) or a destination buffer is full (and needs
flushing). These I/O related suspensions, together with the I/O buffer
arguments' contents and read/write indexes changing, are how [compression
decoders](/doc/std/compression-decoders.md) are able to decompress from
arbitrarily long inputs to arbitrarily long outputs with fixed sized buffers.

Wuffs coroutines are stackful, in that they can call other coroutines, although
not recursively. When suspended, coroutine state is stored in the receiver
struct. Wuffs has no free standing functions (and therefore no free standing
coroutines), only methods (functions with a receiver).

Wuffs code (as opposed to a C program calling into a Wuffs library) can only
call coroutines from within a method that is also a coroutine. If the callee
suspends, then the caller will also suspend, by default, unless the callee's
status result is assigned via the `=?` operator.

Syntactically, coroutines are marked by a question mark, `?`, at their
definition and at call sites, implying that their return type is a
`base.status`. The `yield` keyword is also always immediately followed by `?`,
so that grepping a function's body for the literal question mark will find all
of the potential suspension points.

As for [facts](/doc/note/facts.md), crossing a potential suspension point drops
any facts involving `this` or `args`. Facts only involving local variables are
preserved.
