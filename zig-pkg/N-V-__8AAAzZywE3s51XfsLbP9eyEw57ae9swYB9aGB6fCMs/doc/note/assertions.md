# Assertions

Assertions are statements that introduce new compile-time
[facts](/doc/note/facts.md). They are not comments, as removing them can
prevent the code from compiling, but unlike other programming languages, Wuffs'
assertions have no run-time effect at all, not even in a "debug, not release"
configuration. Compiling an assert will fail unless it can be proven.

The basic form is `assert some_boolean_expression`, which creates
`some_boolean_expression` as a fact. That expression must be free of
side-effects: any function calls within must be [pure](/doc/note/effects.md).

Arithmetic inside assertions is performed in ideal integer math, working in the
integer ring ℤ. An expression like `x + y` in an assertion never overflows,
even if `x` and `y` have a realized (non-ideal) integer type like `u32`.

Some assertions can be proven by the compiler with no further guidance. For
example, if `x == 1` is already a fact, then `x < 5` can be automatically
proved. Adding a seemingly redundant fact can be useful when reconciling
multiple arms of an if-else chain, as
[reconciliation](/doc/note/facts.md#situations-and-reconciliation) requires the
facts in each arm's final situation to match exactly:

```
if x < 5 {
    // No further action is required. "x < 5" is a fact in the 'if' arm.

} else {
    x = 1
    // At this point, "x == 1" is a fact, but "x < 5" is not.

    // This assertion creates the "x < 5" fact.
    assert x < 5
}

// Here, "x < 5" is still a fact, since the exact boolean expression "x < 5"
// was a fact at the end of every arm of the if-else chain.
```

TODO: specify what can be proved automatically, without naming an axiom.


## Axioms

Wuffs' assertion system is a proof checker, not an SMT solver or automated
theorem prover. It verifies explicit proof targets instead of the more
open-ended task of searching for implicit ones. This involves more explicit
work by the programmer, but compile times matter, so the Wuffs compiler is fast
(and dumb) instead of smart (and slow).

The Wuffs syntax is regular (and unlike C++, does not require a symbol table to
parse), so it should be straightforward to transform Wuffs code to and from
file formats used by more sophisticated proof engines. Nonetheless, that is out
of scope of this respository and the Wuffs compiler per se.

Again for compilation speed, not every inference rule is applied after every
line of code. Some assertions require explicit guidance, naming the rule that
proves the assertion. These names are simply strings that resemble mathematical
statements. They are axiomatic, in that these rules are assumed, not proved, by
the Wuffs toolchain. They are typically at a higher level than e.g. Peano
axioms, as Wuffs emphasizes practicality over theoretical minimalism. As they
are axiomatic, they endeavour to only encode 'obvious' mathematical rules.

For example, the axiom named `"a < b: a < c; c <= b"` is about transitivity:
the assertion `a < b` is proved if both `a < c` and `c <= b` are true, for some
(pure) expression `c`. Terms like `a`, `b` and `c` here are all integers in ℤ,
they do not encompass floating point concepts like negative zero, `NaN`s or
rounding. The axiom is invoked by extending an `assert` with the `via` keyword:

```
assert n_bits < 12 via "a < b: a < c; c <= b"(c: width)
```

This proves `n_bits < 12` by applying that transitivity axiom, where `a` is
`n_bits`, `b` is `12` and `c` is `width`. Compiling this assertion requires
proving both `n_bits < width` and `width <= 12`, from existing facts or from
the type system, e.g. `width` is a `base.u32[..= 12]`.

The trailing `(c: width)` syntax is deliberately similar to a function call
(recall that when calling a Wuffs function, each argument must be named), but
the `"a < b: a < c; c <= b"` named axiom is not a function-typed expression.

The [compiler's built-in axioms](/lang/check/axioms.md) are listed separately.
