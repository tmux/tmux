# Iterate Loops

Iterate loops are syntactic sugar for partitioning a variable-sized input slice
into fixed-sized chunk slices (with possible smaller-sized remainders).
Individual chunks' fixed sizes are typically easier for
[bounds-checking](/doc/note/bounds-checking.md).

For example, with primarily 8 length chunks, a 26 length input can be processed
in five chunks (of length 8, 8, 8, 1 and 1). Accessing `chunk[7]` is trivially
in-bounds if `chunk.length()` is exactly 8.

```
var chunk : slice base.u8
var input : slice base.u8

etc

iterate (chunk = input)(length: 8, unroll: 2) {
    // Within this block, chunk.length() is always 8. For a 26 length input,
    // this block will run three times:
    //  - with "chunk = input[0 .. 8]",
    //  - with "chunk = input[8 .. 16]",
    //  - with "chunk = input[16 .. 24]".
    etc

} else (length: 1, unroll: 1) {
    // Each block of the iterate loop only runs after all previous blocks (in
    // this case the only previous block) are exhausted: the remaining input is
    // shorter than each of their given lengths.
    //
    // Within this block, chunk.length() is always 1. Continuing the previous
    // example, this block will run two times:
    //  - with "chunk = input[24 .. 25]",
    //  - with "chunk = input[25 .. 26]".
    etc
}
```

Chunk processing (i.e. loop bodies) can also be unrolled, which affects
performance but not semantics.
