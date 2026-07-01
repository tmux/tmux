# Interval Arithmetic

Regular arithmetic says things like: if `x` has the value `3` and `y` has the
value `20`, then the expression `(x + y)` has the value `23`.

Interval arithmetic says things like: if `x`'s value is in the
[range](/doc/note/ranges-and-rects.md) `3 ..= 5` and `y`'s value is in the
range `20 ..= 63`, then the expression `(x + y)`'s value is in the range `23
..= 68`.

With integer interval arithmetic, addition is trivial: the lower and upper
(inclusive) bounds of a sum is the sum of the lower and upper bounds.
Multiplication is still straightforward, with care needed when possibly
multiplying by a negative number: `((x * 2) > x)` is false when `x` is
negative. Other arithmetic operations, like divisions, shifts and even bitwise
operations ('and' and 'or') are still computable, although more complicated.


## Bounds / Overflow Checking

In Wuffs, integer interval arithmetic is used for bounds and overflow checking.
For example, this program snippet:

```
var a : array[256] base.32
var x : base.u32[0 ..= 2]
var y : base.u32[0 ..= 100]

etcetera

return a[(x * y) + 80]
```

fails to compile, because the range of the expression `((x * y) + 80)` is `[0
..= 280]`, part of which falls outside of the array `a`'s acceptable indexes:
the range `[0 ..= 255]`.


## `i = i + 1` Doesn't Compile

Similarly, this program snippet:

```
var i : base.u32[0 ..= 10]

etcetera

i = i + 1
```

fails the compile-time bounds/overflow checker, because the assignment's RHS's
(Right Hand Side's) range, `[1 ..= 11]`, is not wholly contained by the LHS
(Left Hand Side) variable's range: `i` has range `[0 ..= 10]`. If `i` had an
unrefined type, such as `base.u32`, then this is essentially an overflow check.


## Masking Array Indexes

A common pattern is for the array length to be a power of 2, e.g. `256` is
`0x100`, and the index expression to be and-ed with a mask value one less than
that length, e.g. `255` is `0xFF`. Continuing the example above:

```
return a[((x * y) + 80) & 0xFF]
```

will compile, since the range of `(((x * y) + 80) & 0xFF)` is `[0 ..= 255]`.
This is equal to and therefore trivially wholly contained by `a`'s acceptable
index range.


## Interaction with Facts

For an expression like `(i + 1)`, the relevant interval for the sub-expression
`i` starts with `i`'s possibly-refined type, `base.u32[0 ..= 10]`. This is a
static concept - a variable's type cannot change - but can be further narrowed
by dynamic constraints, or [facts](/doc/note/facts.md). For example, while a
bare `i = i + 1` will never pass the bounds/overflow checker, making that
assignment conditional can pass:

```
var i : base.u32[0 ..= 10]

etcetera

if i <= 4 {
    i = i + 1
}
```

Inside that if-block, up until the assignment to `i`, the range of possible `i`
values are the intersection of two ranges. The first range comes from the type:
`[0 ..= 10]`. The second range comes from the if-condition: `[-∞ ..= 4]`. The
intersection is `[0 ..= 4]`, and the range of possible values for the
expression `(i + 1)` is therefore `[1 ..= 5]`. Since `[1 ..= 5]` is wholly
contained by the LHS variable's type's range, `[0 ..= 10]`, the assignment is
valid.


## Slices and Arrays

Recall the difference between [slices and
arrays](/doc/note/slices-arrays-and-tables.md): slices have dynamic (run-time
determined) length, arrays have static (compile-time determined) length. For an
index expresssion like `s[expr]` or `a[expr]`, bounds-checking an array-index
can be done by interval arithmetic and refinement types alone, but
bounds-checking a slice-index can only be done by an if-condition or
while-condition (or, less frequently, an [iterate
loop](/doc/note/iterate-loops.md)), introducing a dynamic fact:

```
var s : slice base.u8
var x : base.u8

etcetera

// The "0 <= expr" can be omitted if the compiler can prove that the
// range of expr's possible values does not contain any negative values.
// This is trivial if the expression expr is just a variable that has an
// unsigned integer type.
while (0 <= expr) and (expr < s.length()) {
    x = s[expr]
    etcetera
}
```


## Go Programming Language Library

For those programmers wishing to work with integer interval arithmetic, the
[`github.com/google/wuffs/lib/interval`](https://godoc.org/github.com/google/wuffs/lib/interval)
package is usable without requiring anything else Wuffs-related. Specifically,
that package depends only on the standard `math/big` package.
