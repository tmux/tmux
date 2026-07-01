# Initialization

Outside of the Wuffs base package, every Wuffs struct has an implicit
`initialize` method, which needs to be called before any other method. In
Wuffs' C language form, the `initialize` function for the `foo` package's `bar`
struct is called `wuffs_foo__bar__initialize`. The function takes four
arguments (or, in C++, the `initialize` method takes three arguments, with the
implicit `this` pointer argument):

- A pointer to the `wuffs_foo__bar` object: the `this` pointer.
- The size (in bytes) of that object. Conceptually, this is
  `sizeof(wuffs_foo__bar{})`, but that expression generally won't compile, as
  `wuffs_foo__bar` is deliberately an incomplete type. Its size is not part of
  the stable ABI. Instead, call the `sizeof__wuffs_foo__bar` *function*, whose
  return value can change across Wuffs versions, even across point releases.
  For an example, look for `sizeof__wuffs_json__decoder` in the
  [example/jsonptr](/example/jsonptr/jsonptr.cc) program.
- The Wuffs library version, `WUFFS_VERSION`.
- Additional flags.

Initialization can fail if the caller and callee disagree on the size or the
Wuffs version, or if unsupported flag bits are passed.

There are no destructor functions. Just free the memory. Wuffs structs don't
store or otherwise own file descriptors, pointers to dynamically allocated
memory or anything else that needs explicit releasing.

As a consequence, to restore a Wuffs object to its initial state (e.g. to
re-use a Wuffs image decoder's memory to decode a different image), just call
the `initialize` function again.


## Flags

The flags are a bitmask of options. Zero (or equivalently,
`WUFFS_INITIALIZE__DEFAULT_OPTIONS`) means that none of the options are set.

- The `WUFFS_INITIALIZE__ALREADY_ZEROED` bit tells the `initialize` function to
  assume that the entire struct has already been zero-initialized. This can
  make the `initialize` call a little faster.
- The `WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED` bit means to
  partially, not fully, zero-initialize the struct. Again, this can make the
  call a little faster. See the "Partial Zero-Initialization" section below for
  details. This bit is ignored if the `WUFFS_INITIALIZE__ALREADY_ZEROED` bit is
  also set.


## Partial Zero-Initialization

Memory-safe programming languages typically initialize their variables and
fields to zero, so that there is no way to read an uninitialized value. By
default, Wuffs does so too, but in Wuffs' C/C++ form, there is the option (the
`WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED` flag bit) to leave
some struct fields uninitialized (although local variables are always
zero-initialized), for performance reasons.

With or without this flag bit set, the Wuffs compiler still enforces bounds and
arithmetic overflow checks. It's just that for potentially-uninitialized struct
fields, the compiler has weaker starting assumptions: their numeric types
cannot be [refined](/doc/glossary.md#refinement-type).

Even with this flag bit set, the Wuffs standard library also considers reading
from an uninitialized buffer to be a bug, and strives to never do so, but
unlike buffer out-of-bounds reads or writes, it is not a bug class that the
Wuffs compiler eliminates.

For those paranoid about security, leave this flag bit unset, so that
`wuffs_foo__bar__initialize` will zero-initialize the entire struct (unless the
`WUFFS_INITIALIZE__ALREADY_ZEROED` flag bit is also set).

Setting this flag bit (avoiding a fixed-size cost) gives a small absolute
improvement on micro-benchmarks, mostly noticeable (in relative terms) only
when the actual work to do (the input) is also small. Look for
`WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED` in the
[benchmarks](/doc/benchmarks.md) for performance numbers.

In Wuffs code, a struct definition has two parts, although the second part's
`()` parentheses may be omitted if empty:

```
pub struct bar?(
    // Fields in the first part are always zero-initialized.
    x : etc,
    y : etc,
)(
    // Fields in the second part are optionally uninitialized, but are still
    // zero-initialized by default.
    //
    // Valid types for these fields are either unrefined numerical types, or
    // arrays of a valid type.
    //  - "base.u8" is ok.
    //  - "array[123] base.u8" is ok.
    //  - "array[123] base.u8[0 ..= 99]" is not, due to the refinement.
    z : etc,
)
```
