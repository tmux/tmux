# Hermeticity

Hermetic functions (or hermetic methods) are pieces of code that can modify
only local state (including its arguments) to store the result of computation.
They cannot modify global state (including thread-related state like mutexes)
or make actual or virtual system calls (including allocating memory or querying
timers). They can call other hermetic functions but any recursion must be
declaratively finite (so that a worst case stack size requirement can be
computed at compile time) and they cannot invoke user-supplied callbacks.


## Purity

Hermeticity is similar to
[purity](https://en.wikipedia.org/wiki/Pure_function), although the exact
definition of purity [depends on who you
ask](https://twitter.com/radokirov/status/1097325661658570752). While pure
functions are usually defined as without side effects, hermetic functions can
modify its arguments' state (not just modify local variables).

For example, in Go notation, `func pureSort(original []int) (sorted []int)`
could take a slice of `int`s and return *a new slice* containing the elements
in sorted order - the `original` slice is not modified. In comparison, `func
hermeticSort(dst []int, src []int) (dstWasLongEnough bool)` could copy `src`'s
elements (in sorted order) to modify *a caller-supplied slice* `dst`, provided
that `dst` was long enough.

This distinction is somewhat fuzzy if you think of the pass-a-`dst`-slice
mechanism as merely a performance optimization for 'returning' large values via
an out parameter. Still, hermetic *methods* are allowed to modify the receiver
argument's state, which is separate from return values or out parameters.

For example, `func (otp *oneTimePad) Transform(dst []byte, src []byte, atEOF
bool) (nDst int, nSrc int, err error)` could combine a [one-time
pad](https://en.wikipedia.org/wiki/One-time_pad) with an input stream, writing
the result to an output stream. Calling `Transform` performs some compute but
it also updates the receiver `otp`'s state to track how much of the one-time
pad remains after successive calls during streaming I/O. This
[`Transform`](https://pkg.go.dev/golang.org/x/text/transform?tab=doc#Transformer)
method can be hermetic without being pure.


## In-Process Sandboxing

Hermeticity is similar to process-level sandboxing (like Linux's
`SECCOMP_MODE_STRICT` sandbox) but for hermetic functions, both caller and
callee are in the same process. The motivation is the same - to compute on
untrusted data without security-compromising surprises, even if there are bugs
in the computation code - but the mechanism is different.

For `seccomp`, the operating system enforces the restriction (at run-time) and
communication (literally IPC or Inter-Process Communication) is heavyweight,
asynchronous and complex.

For Wuffs, the compiler enforces [memory-safety](/doc/note/memory-safety.md)
(at compile time) and communication (a function call) is lightweight,
synchronous and simple. [Wuffs the Language](/doc/wuffs-the-language.md) is
deliberately unpowerful. There are:

- no mutable global variables,
- no mutable TLS (thread-local storage) variables,
- no `unsafe` keyword,
- no FFI (Foreign Function Interface),
- no user-supplied callbacks,
- no system calls,
- no allocation or de-allocation of memory and
- no panicking or throwing exceptions (including for out-of-memory).

Wuffs is designed for writing hermetic, secure libraries, not complete
programs, and **with less power comes easier proof of safety**.

In comparison to compiling C/C++ code with WebAssembly (which can be restricted
to only compute), both Wuffs and Wasm allow for hermetic libraries within
larger (memory-unsafe) C/C++ programs. The difference is that Wasm lets you
re-use existing C/C++ libraries (albeit with a run-time [performance
penalty](https://kripken.github.io/blog/wasm/2020/07/27/wasmboxc.html)) but
Wuffs performs [on par with C
code](https://github.com/google/wuffs/blob/main/doc/benchmarks.md) (albeit
requiring re-writing those libraries).


## Halting

Wuffs functions are not guaranteed to halt. We do not solve the [Halting
Problem](https://en.wikipedia.org/wiki/Halting_problem). Furthermore, while
there is a difference in theory between a function that doesn't halt and one
that halts in 99 years, in practice there is not. Systems that already need to
cope with the latter can also cope with the former.


## Timing and Other Attacks

Technically, 'hermetic' code can still affect other threads or processes
indirectly: modifying what's in hardware caches, OS page tables or that
spending CPU quota leads to a scheduler switch. Wuffs does not guarantee that
code is safe from timing or speculative execution attacks. Nonetheless, Wuffs
code does not have access to any clocks. Any Wuffs computation on "the current
time" requires the caller to explicitly pass in that information.
