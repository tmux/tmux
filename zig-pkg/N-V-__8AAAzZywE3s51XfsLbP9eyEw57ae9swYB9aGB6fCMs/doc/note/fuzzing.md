# Fuzzing

Wuffs has been fuzzed (on [OSS-Fuzz](https://google.github.io/oss-fuzz/)) since
[February
2018](https://github.com/google/oss-fuzz/commit/27f374480b1e7c780d161b7a6727c23cc03419c5).

As of November 2019, this has not yet found any memory-safety bugs in Wuffs.
Compile-time safety checks seem to work.

However, it has found a few correctness bugs, based on generating random input.
Wuffs' fuzzer programs will [intentionally
segfault](https://github.com/google/wuffs/blob/7ec252876541ec203659949450fafddc148b606e/fuzz/c/fuzzlib/fuzzlib.c#L61)
if the Wuffs library returns an "internal" error message.

For example, Wuffs [won't let you write `x -=
1`](/doc/note/interval-arithmetic.md#i--i--1-doesnt-compile) unless you can
prove that it won't underflow. The programmer might 'know' that `x` is always
positive at some point in their program, but to satisfy Wuffs' [bounds
checker](/doc/note/bounds-checking.md), they have to explicitly write something
like:

```
if x > 0 {
    x -= 1
} else {
    // Unreachable: x is always positive because blah blah blah.
    return "#internal error: etc"
}
```

Hitting the else branch means that there's a bug: what the programmer 'knew'
was incorrect. These aren't security bugs per se: decoding an image would
produce the wrong pixels, or abort early, instead of leading to RCE (Remote
Code Execution). But fuzzing Wuffs has still been useful.
