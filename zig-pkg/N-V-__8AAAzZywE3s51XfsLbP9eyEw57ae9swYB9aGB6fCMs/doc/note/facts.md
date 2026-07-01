# Facts

Facts are boolean expressions, such as `x < (y + z)`, that are guaranteed to be
true at a given point in a program's execution (or, informally, at a given line
of code). They might not be true at other points, as Wuffs is an imperative
language and its variables can change values (but not change types) over a
program's lifetime.

Types (and their [refinements](/doc/glossary.md#refinement-type)) express
static constraints (on variables, struct fields and function arguments). Facts
express dynamic constraints. They come and go as a program proceeds.

Another difference is that facts can capture the relationships *between*
variables. For example, `i < s.length()` might be a fact involving the two
variables `i` and `s`, and therefore `s[i]` is a valid (in-bounds) index
expression (assuming that we also know that `0 <= i`, e.g. because `i` is
unsigned). Other programming languages use the type system to express these
sorts of concepts, but Wuffs does not have dependent types.


## Bounds Checking

[Interval arithmetic](/doc/note/interval-arithmetic.md) and the type system
provide much of Wuffs' compile-time [bounds
checking](/doc/note/bounds-checking.md), but facts are also used to prove
bounds checks that the type system alone cannot. For example, a variable `x`
might generally have values in the range `[0 ..= 255]`, but in a specific block
of code, `x` might be temporarily restricted to the range `[30 ..= 40]` and
therefore an expression like `a[x - 25]` is guaranteed in-bounds when `a` is an
array of length 16.

Guarding against `nullptr` dereferences is another case of bounds checking.
`nptr T` is the "pointer to `T`" type that allows `nullptr` values. The Wuffs
compiler will reject any method call on a variable (or function argument) with
that type, without the fact that it is not actually a `nullptr`:

```
pub func foo.bar!(ic: nptr base.image_config) {
    etc
    // Cannot call args.ic.set!(etc) here, since the type system doesn't
    // rule out args.ic being nullptr.
    etc

    if args.ic <> nullptr {
        // We now know, as a fact, that "args.ic <> nullptr", so that calling a
        // method on it is valid. Wuffs spells not-equals as "<>".
        args.ic.set!(etc)
    }

    etc
}
```


## Creating Facts

Every fact is true (at its point in the program), but not every truth is a
fact. For example, if `x < 5` is true then `x < 6`, `x < 7`, `x < 8`, etc are
an infinite sequence of truths, but the Wuffs tools only track a finite number
of facts at any given point. Some facts are explicitly created and some facts
are implicitly created. But all facts are created by specific means.

One way to implicitly create a fact is by assignment. Immediately after the
statement `a = rhs`, the boolean expression `a == rhs` is a true fact, provided
that `rhs` is an expression with no [side effects](/doc/note/effects.md). That
assignment also either removes or updates any other fact involving `a`.

TODO: specify exactly when and how facts are removed or updated.

As alluded to above, another way to implicitly create a fact is to explicitly
check if it is true, using an `if` or `while` statement:

```
// "x < (y + z)" isn't necessarily true here...

if x < (y + z) {
    // ...but in here, "x < (y + z)" is a true fact.
    w = 3
    // It remains so after an assignment to an unrelated variable.
    y = 4
    // But "x < (y + z)" is no longer necessarily true after one or more of
    // that boolean expression's variables (in this case, y) might have
    // changed. Still, at this point in the program, the set of facts include
    // "w == 3" and "y == 4".
    etc

} else {
    // At the top of the else branch, and any subsequent branch in the same
    // if-else chain, the inverted expression  "x >= (y + z)" is a true fact.
    etc
}
```

Facts are also explicitly created by compile-time
[assertions](/doc/note/assertions.md), discussed in a separate document.


## Situations and Reconciliation

The situation is defined as the set of facts at a given point in a program.
Multiple situations have to be reconciled when there are multiple paths to a
line of code:

- The separate arms of an if-else chain eventually come back together. Terminal
  arms (e.g. those that end with a `break`, `continue` or `return` statement)
  are not considered during reconciliation.
- The start of a while loop can come from not just its preceding line of code,
  but also from any explicit `continue`s inside that loop, and the implicit
  `continue` at the closing `}` curly brace.
- Similarly, the line of code immediately after the entire while loop can come
  from an explicit `break` or if the while condition fails.

Reconciliation is simply set intersection: a boolean expression is guaranteed
to be true (i.e. it is a fact) at a point in the program only if it is true for
every possible code execution path to that point.


## Loops

Loops are cyclical: the situation at the bottom of the loop depends on the
situation at the top of the loop, but the situation at the top depends on the
bottom (because of the implicit `continue` at the bottom). Instead of relying
on an SMT solver or automated theorem prover (whose exact behavior is hard to
specify, or even reliably reproduce if it can be configured with a timeout) to
infer a solution to this self-referential problem, Wuffs simply requires the
programmer to write exactly what the situation is, before and after the loop:

```
while n_bits < n_extra_bits,
    inv i < 320,
    inv rep_count <= 11,
    post n_bits >= n_extra_bits,
{
    etc  // Loop body.
}
```

The `pre`, `post` and `inv` keywords introduce loop pre-conditions,
post-conditions and invariants (things that are both). The snippet above means
that the Wuffs compiler has to verify all three conditions at every place this
loop exits: at the implicit `break` if the expression `n_bits < n_extra_bits`
evaluates to false (which in this case trivially proves the third condition),
but also at every explicit `break` for that loop within its body.

Similarly, it has to verify only the first two conditions at every place this
loop repeats (at its initial execution, at every explicit `continue` and at the
loop bottom), before the `n_bits < n_extra_bits` condition is checked.

As a consequence of explicitly listing those two invariants (which are also
pre-condtions), the situation at the top of the loop body (immediately after
the `{` curly brace) is precisely those first two conditions, plus the `n_bits
< n_extra_bits` fact from the while condition evaluation. For example, if the
loop body never modifies the `i` local variable, it is trivial to preserve the
fact that `i < 320` throughout the loop body but specifically at every exit
point, and therefore maintain the invariant.

Note that invariants don't have to hold at every point within the loop, only at
every jump for that loop. Such facts can be temporarily lost and later
re-established within that loop body, provided that no `break` or `continue`
for that loop comes in between.

The discussion above focuses on while loops, but also apply to the less common
[iterate loops](/doc/note/iterate-loops.md).


## Debugging Facts

During development, writing down what part of the situation a programmer needs
to preserve across a while loop obviously requires knowing what the situation
is. In the future, a Wuffs-aware IDE could provide that information. Until
then, inserting an `assert false` line into a Wuffs program will fail to
compile, as the compiler can obviously not prove that `false` is true, and the
compilation error message should include a situation listing.
