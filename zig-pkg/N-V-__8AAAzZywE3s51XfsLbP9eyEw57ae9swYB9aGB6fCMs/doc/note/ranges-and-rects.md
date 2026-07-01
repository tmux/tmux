# Ranges and Rects

Ranges are finite numerical intervals, e.g. "all integers `i` such that `(m <=
i)` and `(i < n)`". The high end bound is sometimes exclusive, `(i < n)`, and
sometimes inclusive, `(i <= n)`.

In Wuffs syntax, similar to Rust syntax, the exclusive range is `m .. n` and
the inclusive range is `m ..= n`. The conventional mathematical syntax is `[m,
n)` or `[m, n[` for exclusive and `[m, n]` for inclusive, but Wuffs is a
programming language, and programming language tools prefer brackets to always
be balanced.

In Wuffs' C form, the exclusive range is `wuffs_base__range_ie_T` and the
inclusive range is `wuffs_base__range_ii_T`. The `ie` means inclusive on the
low end, exclusive on the high end. The `T` is a numerical type like `u32` or
`u64`.

Both of the `ii` and `ie` flavors are useful in practice: `ii` or `m ..= n` is
more convenient when computing [interval
arithmetic](/doc/note/interval-arithmetic.md), `ie` or `m .. n` is more
convenient when working with [slices](/doc/note/slices-arrays-and-tables.md).
The `ei` and `ee` flavors also exist in theory, but aren't widely used. In
Wuffs, the low end is always inclusive.


## Inclusive-Exclusive

The `ie` (half-open) flavor is recommended by Dijkstra's ["Why numbering should
start at zero"](http://www.cs.utexas.edu/users/EWD/ewd08xx/EWD831.PDF) and see
also a further discussion of [half-open
intervals](https://www.quora.com/Why-are-Python-ranges-half-open-exclusive-instead-of-closed-inclusive).

For example, with `ie`, the number of elements in "`uint32_t` values in the
half-open interval `m .. n`" is equal to `max(0, n - m)`. Furthermore, that
number of elements (in one dimension, a length, in two dimensions, a width or
height) is itself representable as a `uint32_t` without overflow, again for
`uint32_t` values `m` and `n`. In the contrasting `ii` flavor, the size of the
closed interval `0 ..= ((1<<32) - 1)` is `1<<32`, which cannot be represented
as a `uint32_t`.

In Wuffs' C form, because of this potential overflow, the `ie` flavor has
length / width / height methods, but the `ii` flavor does not.


## Inclusive-Inclusive

The `ii` (closed) flavor is useful when refining e.g. "the set of all
`uint32_t` values" to a contiguous subset: "`uint32_t` values in the closed
interval `m ..= n`", for `uint32_t` values `m` and `n`. An unrefined type (in
other words, the set of all `uint32_t` values) is not representable in the `ie`
flavor because if `n` equals `((1<<32) - 1)` then `(n + 1)` will overflow.


## Empty Ranges

It is valid for `m >= n` (for the `ie` case) or for `m > n` (for the `ii`
case), in which case the range is empty. There are multiple valid
representations of an empty range: `(m=1, n=0)` and `(m=99, n=77)` are
equivalent.


## Rectangles

Rects are just the 2-dimensional form of (1-dimensional) ranges. For example,
`wuffs_base__rect_ii_u32` is a rectangle on the integer grid, containing all
points `(x, y)` such that `(min_incl_x <= x)` and `(x <= max_incl_x)`, and
likewise for `y`.

Once again, it is valid for `min > max`, and there are multiple valid
representations of an empty rectangle.

When rects are used in graphics, the X and Y axes increase right and down.
