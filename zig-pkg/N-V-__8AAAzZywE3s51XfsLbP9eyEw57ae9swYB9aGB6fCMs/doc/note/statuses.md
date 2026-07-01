# Statuses

Wuffs' important functions - ones that do a significant amount of work, often
involving [I/O](/doc/note/io-input-output.md) - return a status value. There
are four categories:

- OK:          the request was completed, successfully.
- Notes:       the request was completed, unsuccessfully.
- Suspensions: the request was not completed, but can be re-tried.
- Errors:      the request was not completed, permanently.

When a [method](/doc/glossary.md#method) returns an error, it is permanent.
Calling that method (or any other status-returning method), on the same
receiver, will return a `"#disabled by previous error"` error.

When a method returns a suspension, the suspended
[coroutine](/doc/note/coroutines.md) can be resumed by calling that method
again. However, calling any other public coroutine method, while already
suspended, will lead to an `"#interleaved coroutine calls"` error.

Otherwise, the call was complete. 'Unsuccessful' (i.e. a note) doesn't
necessarily mean 'bad' or something to avoid, only that something occurred
other than the typical outcome. For example, when decoding an animated image,
without knowing the number of frames beforehand, a call to "decode the next
frame" could return OK, if there was a next frame, or an `"@end of data"` note,
if there wasn't.


## Statuses are Strings

There is only one value in the OK category. In Wuffs code, this is a built-in
literal value called `ok`, without `"` quote marks. For example:

```
return ok
```

The other categories can contain multiple values, each with an ASCII string
message. In Wuffs code, a string literal is synonymous with a status value, as
Wuffs otherwise doesn't use string-typed values, only byte slices, arrays and
buffers:

```
// Return an error status, defined in this package.
return "#bad Huffman code"
```

That status value may be package-qualified. For example, a coroutine could
refer to a status defined in another package, `base`:

```
// Yield a suspension status, defined in the base package.
yield? base."$short read"
```

That string message is human-readable, for programmers, but it is not for end
users. It is not localized, and does not contain additional contextual
information such as a source filename.

The first byte of the string message gives the category. For example, `"#bad
receiver"` is an error and `"$short read"` is a suspension:

- `'@'` means a note.
- `'$'` means a suspension.
- `'#'` means an error.


## C Implementation

In terms of C implementation, a status' `repr` (representation) is just its
string message: a `const char *`, with `ok` being the null pointer. That C
string is statically allocated and should never be `free`d. Status `repr`s can
be compared by the `==` operator and not just by `strcmp`.

The C string's contents has the Wuffs package name inserted by the Wuffs
compiler, just after that first byte. For example, the `std/deflate` package
has this line of Wuffs code, defining an error status:

```
pub status "#bad Huffman code"
```

When that Wuffs code is compiled to C, it produces:

```
const char* wuffs_deflate__error__bad_huffman_code =
    "#deflate: bad Huffman code";
```

When printing a status message, the `wuffs_base__status__message` function will
advance a (non null) pointer by 1 byte, skipping that leading `'@'`, `'#'` or
`'$'`.
