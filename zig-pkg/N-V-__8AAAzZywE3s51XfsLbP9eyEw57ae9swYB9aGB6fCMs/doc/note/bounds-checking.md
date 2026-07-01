# Bounds Checking

For a slice or array `x`, an expression like `x[i .. j]` is invalid unless the
compiler can prove that `((0 <= i) and (i <= j) and (j <= x.length()))`. For
example, the expression `x[..]` is always valid because the implicit `i` and
`j` values are `0` and `x.length()`, and a slice or array cannot have negative
length.

Similarly, an expression like `x[i]` is invalid unless there is a compile-time
proof that `i` is in-bounds: that `((0 <= i) and (i < x.length()))`. Proofs can
involve natural bounds (e.g. if `i` has type `base.u8` then `(0 <= i)` is
trivially true since a `base.u8` is unsigned),
[refinements](/doc/glossary.md#refinement-type) (e.g. if `i` has type
`base.u32[..= 100]` then 100 is an inclusive upper bound for `i`),
[facts](/doc/note/facts.md) (e.g. an explicit prior check `if i < x.length()`)
and [interval arithmetic](/doc/note/interval-arithmetic.md) (e.g. for an
expression like `x[m + n]`, the upper bound for `m + n` is the sum of the upper
bounds of `m` and `n`).

For example, if `a` is an `array[1024] base.u8`, and `expr` is some expression
of type `base.u32`, then `a[expr & 1023]` is valid (in-bounds). The bitwise-and
means that the overall index expression is in the range `[0 ..= 1023]`,
regardless of `expr`'s range.


## Overflow Checking

Arithmetic overflow checking uses the same mechanisms as bounds checking. For
example, if `m` and `n` both have type `base.u8` (or a refinement thereof) then
the expression `m + n` also has type `base.u8`. Proving that `m + n` evaluates
to something inside the `base.u8` range, `[0 ..= 255]`, is equivalent to
proving that the expression `h[m + n]` is valid (in-bounds) for a hypothetical
`h` array of length `256`.


## `nullptr` Checking

Guarding against `nullptr` dereferences is another case of bounds checking.
Like other programming languages, `nptr T` and `ptr T` are types: nullable and
non-null pointers to some type `T`. In Wuffs, the latter is effectively a
refinement. Assuming that `nullptr` is equivalent to `0`, `ptr T` is
conceptually just `(nptr T)[1 ..= UINTPTR_MAX]`, although that's not actually
valid syntax. Similarly, if `expr` is a pointer-typed expression then checking
that `expr` is not `nullptr` is equivalent to checking that `expr` is in the
range `[1 ..= UINTPTR_MAX]`.
