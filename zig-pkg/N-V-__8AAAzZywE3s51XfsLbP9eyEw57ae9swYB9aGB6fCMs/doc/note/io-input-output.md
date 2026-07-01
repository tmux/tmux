# I/O (Input / Output)

Wuffs per se doesn't have the ability to read or write to files or network
connections. Recall that Wuffs is a programming language for writing libraries,
not applications, and that having fewer capabilities means that it's trivial to
prove that you can't misuse a capability, even when given malicious input.

Instead, the code that calls into Wuffs libraries is responsible for
interfacing with e.g. the file system or the network system. An `io_buffer` is
the mechanism for transferring data into and out of Wuffs libraries. For
example, when decompressing gzip, there are two `io_buffer`s: the caller fills
a source buffer with e.g. the compressed file's contents and the callee (the
Wuffs library) reads compressed bytes from that source buffer and writes
decompressed bytes to a destination buffer.


## I/O Buffers

An `io_buffer` is a [slice](/doc/note/slices-arrays-and-tables.md) of bytes
(the data, a `ptr` and `len`) with additional fields (the metadata): a read
index (`ri`), a write index (`wi`), a position (`pos`) and whether or not it is
`closed`.


## Read Index and Write Index

Writing to an `io_buffer`, e.g. copying from a file to a buffer, increments
`wi`. The buffer is full for writing (no more can be written) when `wi` equals
`len`. Writing does not have to fill a buffer before further processing.

Reading from an `io_buffer`, e.g. copying from a buffer to a file, increments
`ri`. The buffer is empty for reading (no more can be read) when `ri` equals
`wi`. Reading does not have to empty a buffer before further processing.

An invariant condition is that `((0 <= ri) and (ri <= wi) and (wi <= len))`.

Having separate read and write indexes simplifies connecting a sequence of
filters or processors with `io_buffer`s, similar to connecting Unix processes
with pipes. Each filter reads from the previous buffer and writes to the next
buffer. Each buffer is written to by the previous filter and is read from by
the next filter. There's no need to flip a buffer between reading and writing
modes. Nonetheless, `io_buffer`s are generally not thread-safe.

Continuing the "decompressing gzip" example, the application would write to
the source buffer by copying from e.g. `stdin`. The Wuffs library would read
from the source buffer and write to the destination buffer. The application
would read from the destination buffer by copying to e.g. `stdout`. Buffer
space can be re-used, via compaction (see below), so that neither the source
or destination data needs to be entirely in memory at any point.

For example, an `io_buffer` of length 8 could have 4 bytes available to read
and 1 byte available to write. If 1 byte was written, there would then be 5
bytes available to read. Visually:

```
[.. .. .. .. .. .. .. ..]
 |<- ri ->|           |  |
 |<------- wi ------->|  |
 |<-------- len -------->|
```


## Position

An `io_buffer` is a sliding window into a stream of bytes. Its position (`pos`)
is the number of bytes in the stream prior to the first element of the slice.
The total number of bytes read from and written to the stream are therefore
`(pos + ri)` and `(pos + wi)`.

While every slice element is in-memory, the stream's prior bytes do not
necessarily have to be in-memory now, or have been in-memory in the past. It is
valid to open a file, seek to the 1000'th byte and start copying from there to
an `io_buffer`, provided that `pos` was also initialized to 1000.


## Closed-ness

The `closed` field indicates that no further writes are expected to the
`io_buffer`. When copying from a file to a buffer, `closed` means that we have
reached EOF (End Of File).

For example, decoding a particular file format might, at some point, expect at
least another 4 bytes of data, but only 3 are available to read. If `closed` is
false, this isn't necessarily an error, since an `io_buffer` holds only a
partial view of the underlying data stream, and more data might be forthcoming
but not yet buffered. If `closed` is true, it is definitely an error.


## Undoing Reads and Writes

It is possible to decrement `ri` or `wi`, undoing previous reads or writes,
provided that the invariant `((0 <= ri) and (ri <= wi) and (wi <= len))` holds.
For example, it can be faster on 64 bit (8 byte) systems, if buffer space is
available, to write 8 bytes and then undo 1 byte than to write exactly 7 bytes.

The Wuffs compiler enforces that, during a Wuffs function, `ri` and `wi` will
never be decremented (by an undo operation) to be less than the initial values
at the time of the call. When considering a function as a 'black box', the two
indexes can only travel forward, and it is up to the application code (not
Wuffs library) code to rewind the indexes (e.g. by compaction).

Even though `ri` cannot drop below its initial value, Wuffs code can still
read the contents of the slice before `ri` (in sub-slice notation,
`data[0 .. ri]`) and it should still contain the `(pos + 0)`th, `(pos + 1)`th,
etc. byte of the stream.

The contents of the slice after `wi` (in sub-slice notation, `data[wi .. len]`)
are undefined, and code should not rely on its values. When passing an
`io_buffer` into a function, that function is free to modify anything in
`data[wi .. len]`, for either value of `wi` before or after the function
returns.


## Compaction

Compacting an `io_buffer` moves any written but unread bytes (those in `data[ri
.. wi]`) to the start of the buffer, and updates the metadata fields `ri`, `wi`
and `pos`. Equivalently, it moves the sliding window that is the `io_buffer` as
far forward as possible along the stream.

This generally increases `(len - wi)`, the number of bytes available for
writing, allowing for re-using the allocated buffer memory (the data slice).

Suppose that the underlying data stream's `i`th byte has value `i`, and that we
start with `ri`, `wi` and `pos` were `3`, `7` and `20`. Compaction will
subtract 3 from the first two and add 3 to the last, so that the new `ri`, `wi`
and `pos` are `0`, `4` and `23`. Note that `len`, `(pos + ri)` and `(pos + wi)`
are all unchanged.

Here are two equivalent visualizations of before and after compaction. The `xx`
means a byte whose value is undefined (as it is at or past `wi`).

The first visualization is where the slice is fixed and its contents (its view
of the stream) moves relative to the slice:

```
Before:
[20 21 22 23 24 25 26 xx]
 |<- ri ->|           |  |
 |<------- wi ------->|  |
 |<-------- len -------->|

After:
[23 24 25 26 xx xx xx xx]
 ||          |           |
 |<-- wi --->|           |
 |<-------- len -------->|
```

The second visualization is where the stream (and its contents) is fixed and
the slice (the sliding window) moves relative to the stream:

```
                           pos+ri      pos+wi
                           |           |
Before:          [20 21 22 23 24 25 26 xx]
Stream: ... 18 19 20 21 22 23 24 25 26 27 27 28 29 30 31 ...
After:                    [23 24 25 26 xx xx xx xx]
                           |           |
                           pos+ri      pos+wi
```


## Seeking and I/O Positions

Recall that Wuffs code has limited capabilities, and cannot seek in the
underlying I/O data streams per se. When it needs to seek (e.g. when jumping
between video frames), it will typically provide an "I/O position", a
`uint64_t` value, via some package-specific API. The application (the caller of
the Wuffs code) is then responsible for configuring an `io_buffer` whose
`(pos + ri)` or `(pos + wi)` value, depending on whether we're reading or
writing, is at that "I/O position".

If the underlying file (or equivalent) isn't seekable, e.g. it's `/dev/stdin`
instead of a regular file, then the request cannot be satisfied. The
application should then decide whether that error is recoverable or fatal. This
is the application's responsibility, not the library's, as the application
usually has more context to make that decision.

If that "I/O position" is already within the sliding window, it might not be
necessary to seek in the underlying file, as it may be possible to e.g. simply
decrement `ri` to reach a target `(pos + ri)`, for the reading case. Otherwise,
the typical process is:

1. Set `ri`, `wi` and `pos` to `0`, `0` and that "I/O position". This discards
   any buffered data (but does not free the buffer's memory).
2. Seek in the underlying file to that same "I/O position".
3. Copy from the underlying file to the `io_buffer`, incrementing `wi`.

Whether or not it was necessary to seek and copy from the underlying file, when
calling back into the Wuffs library, it typically checks that the `io_buffer`'s
`(pos + ri)` is now at the expected "I/O position".


## I/O Reader and I/O Writer

An `io_buffer` is the mechanism for transferring data between the application
and the Wuffs library. Application code can manipulate an `io_buffer`'s fields
as it wishes (but is responsible for maintaining the invariant condition).
Wuffs library code places a further restriction that `io_buffer`s are used
exclusively either for reading or for writing, as optimizing incremental access
to an `io_buffer`'s data, while enforcing invariants, is simpler when only one
of `ri` and `wi` can vary.

Wuffs code therefore refers to either a `base.io_reader` or `base.io_writer`,
both of which are essentially the same type (an `io_buffer`) with different
methods. Wuffs code does not reference an `io_buffer` directly.


## Binding

An `io_bind` block temporarily adapts a slice of bytes as an `io_reader` or
`io_writer`. This is typically done to call other functions that take an
`io_reader` or `io_writer` as an argument.

```
var r : base.io_reader
var s : slice base.u8

etc

// Just before the io_bind, r's state is saved.

io_bind (io: r, data: s) {
    // At the top of the block, r's data slice is set to s, and r's metadata is
    // set so that ri = 0, pos = 0 and closed = false.
    //
    // Because r is an io_reader, not an io_writer, the wi metadata field is
    // set to the slice length, not 0.
    //
    // r must be a local variable, but s can be an expression.
    etc
}

// Just after the io_bind, r's state is restored.
```
