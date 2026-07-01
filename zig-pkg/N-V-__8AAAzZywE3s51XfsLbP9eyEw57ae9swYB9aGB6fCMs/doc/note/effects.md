# Effects

Wuffs functions have one of three effects: pure, impure and coroutine. Pure
means that the function has no side effects - it does not change any observable
state. For example, the body of a pure function (with a `this` receiver) cannot
assign to a `this.foo` field. The other two categories may have side effects,
with [coroutines](/doc/note/coroutines.md) also being able to suspend and
resume.

Impure functions are marked with a `!` at their definition and at call sites.
Coroutines are similarly marked, with a `?`. Pure functions have no mark.

For those used to C/C++ syntax, in Wuffs, the unary not operator is spelled
`not` instead of `!`, and Wuffs has no ternary operator like C/C++'s `?:`.

Sub-expressions in Wuffs must be pure. Only the outermost function call can
have a `!` or `?` mark. You can't write:

```
foo!(bar(), baz!(), qux)
```

Instead, you have to explicitly split it.

```
b = baz!()
foo!(bar(), b, qux)
```

For similar reasons, `x += 1` is a statement in Wuffs, not an expression. This
avoids the ambiguous order of execution in C/C++'s `x = x++`, which is actually
undefined behavior.
