# Glossary

#### Auxiliary Code

Additional library code that is not written in the Wuffs programming language.
See the [auxiliary code](/doc/note/auxiliary-code.md) note for more details.

#### Axiom

A named rule for asserting new facts. See the
[assertions](/doc/note/assertions.md#axioms) note for more details.

#### Assertion

A compile-time directive that introduces a new fact (or fails to compile, if
the assertion cannot be proved). See the [assertions](/doc/note/assertions.md)
note for more details.

#### Call Sequence

An API restriction that e.g. the `foo` method needs to be called before the
`bar` method. Out of order calls may result in a `"#bad call sequence"` error.

#### Coroutine

A function that can suspend execution and, when called again later, resume
where it left off, in the middle of the function body. See the
[coroutines](/doc/note/coroutines.md) note for more details.

#### Dependent Type

A type that depends on another value. For example, a variable `n`'s type might
be "the length of `s`", for some other
[slice](/doc/note/slices-arrays-and-tables.md)-typed variable `s`. Dependent
types are *a* way to implement compile-time [bounds
checking](/doc/note/bounds-checking.md), but they're not the only way, and
there's more to programming languages than their type systems. Wuffs does not
use dependent types.

#### Effect

An extension of the type system, such as purity (freedom from side effects),
applied to functions. See the [effects](/doc/note/effects.md) note for more
details.

Syntactically, effects are the exclamation and question marks, `!` and `?`, at
function definitions and function calls. These denote impure and
[coroutine](/doc/note/coroutines.md) functions.

#### Fact

A boolean expression (e.g. `x > y`) that is guaranteed to be true at a given
point in a program. See the [facts](/doc/note/facts.md) note for more details.

#### Flick

A unit of time. One [flick](https://github.com/OculusVR/Flicks) (frame-tick) is
`1 / 705_600_000` of a second.

#### Hermetic Function

A function that can modify only local state (to store the result of
computation), not global or system state. See the
[hermeticity](/doc/note/hermeticity.md) note for more details.

#### Interval Arithmetic

Arithmetic on numerical ranges. See the [interval
arithmetic](/doc/note/interval-arithmetic.md) note for more details.

#### Method

A function whose first argument (the receiver) has special syntactic support.
For example, an expression like `foo.bar(etc)` might effectively be a function
call whose first argument is `foo` and whose remaining arguments are the `etc`.
It is syntactic sugar for `type_of_foo__bar(self: foo, etc)`. On the callee
(not the caller) side, that implicit argument is often named `self` or `this`,
depending on the language (e.g. Wuffs per se uses `this`, but Wuffs transpiled
to C uses `self` to avoid confusion with C++'s `this`). Wuffs has no free
standing functions, only methods.

#### Modular Arithmetic

Arithmetic that wraps around at a certain modulus, such as `256` for the
`base.u8` type. For example, if `x` is a `base.u8` with value `200`, then `(x +
70)` has value `270` and would overflow, but `(x ~mod+ 70)` has value `14`.

#### Operator Precedence

A ranking of arithmetic operators so that an expression like `a + b * c` is
unambiguous (for the computer), equivalent to exactly one of `(a + b) * c` or,
more conventionally, `a + (b * c)`. While good for brevity, Wuffs does not have
operator precedence: the bare `a + b * c` is an invalid Wuffs expression and
the parentheses must be explicit. The ambiguity (for the human) can be a source
of bugs in other [security-conscious file format
parsers](https://github.com/jbangert/nail/issues/7).

Some binary operators (`+`, `*`, `&`, `|`, `^`, `and`, `or`) are also
associative: `(a + b) + c` and `a + (b + c)` are equivalent, and can be written
in Wuffs as `a + b + c`.

#### Refinement Type

A basic type further constrained to a subset of its natural
[range](/doc/note/ranges-and-rects.md). For example, if `x` has type `base.u8[0
..= 99]` then it is an unsigned 8-bit integer whose value must be less than
`100`. Without the refinement, `x` could be as high as `255`.

Bounds may be omitted, where the base integer type provides the implicit bound.
`base.u8[0 ..= 99]` and `base.u8[..= 99]` are the same type.

`base.u8[0 ..= 99]` and `base.u32[0 ..= 99]` are two different types, as they
have different unrefined types. They can have different run-time
representations.

Non-nullable pointer types can be thought of as a refinement of regular pointer
types, where the refined range excludes the `nullptr` value.

#### Saturating Arithmetic

Arithmetic that stops at certain bounds, such as `0` and `255` for the
`base.u8` type. For example, if `x` is a `base.u8` with value `200`, then `(x +
70)` has value `270` and would overflow, but `(x ~sat+ 70)` has value `255`.

#### Situation

The set of [facts](/doc/note/facts.md) at a given point in a program.

#### Unsafe

A programming language mechanism (e.g. an `unsafe` keyword or package) to
circumvent the language's safety enforcement, typically used for performance or
very low level programming. Unlike some other memory-safe languages, Wuffs
doesn't have such an 'escape hatch'.

#### Utility Type

An empty struct (with no fields) used as a placeholder. Every written-in-Wuffs
function is a method: the receiver is mandatory. A Wuffs utility type's methods
are to its package what C++ or Java's static methods are to its class.

"Utility type" is merely a Wuffs naming convention. For example, Wuffs' `base`
package has a type called `base.utility`, similar to how the `zlib` package has
a type called `zlib.decoder`. Unlike "dependent type" or "refinement type",
"utility type" is not a phrase used in programming language type theory.

#### Work Buffer

Scratch space in addition to the primary destination buffer. For example, in
image decoding, the primary destination buffer holds the decoded pixels, but
while decoding is in progress, a decoder might want to store additonal state
per image column.

Wuffs' codecs can state the (input dependent) size of their work buffer needs
as a range, not just a single value. Callers then have the option to trade off
memory for performance.
