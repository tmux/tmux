# Compression Decoders

Compression decoders read from one input stream (an `io_reader` called `src`)
and write to an output stream (an `io_writer` called `dst`). Wuffs'
implementations have one key method: `transform_io`. It incrementally
decompresses the source data.

This method is a [coroutine](/doc/note/coroutines.md), and does not require
either all of the input or all of the output to fit in a single contiguous
buffer. It can suspend with the `$short_read` or `$short_write`
[statuses](/doc/note/statuses.md) when the `src` buffer needs re-filling or the
`dst` buffer needs flushing. For an example, look at the
[example/zcat](/example/zcat/zcat.c) program, which uses fixed size buffers,
but reads arbitrarily long compressed input from stdin and writes arbitrarily
long decompressed output to stdout.


## Dictionaries

TODO: standardize the [various dictionary
APIs](https://github.com/google/wuffs/issues/73), after Wuffs v0.2 is released.


## API Listing

In Wuffs syntax, the `base.io_transformer` methods are:

- `dst_history_retain_length() u64`
- `get_quirk(key: u32) u64`
- `set_quirk!(key: u32, value: u64) status`
- `transform_io?(dst: io_writer, src: io_reader, workbuf: slice u8)`
- `workbuf_len() range_ii_u64`


## Implementations

- [std/bzip2](/std/bzip2)
- [std/deflate](/std/deflate)
- [std/gzip](/std/gzip)
- [std/lzip](/std/lzip)
- [std/lzma](/std/lzma)
- [std/lzw](/std/lzw)
- [std/xz](/std/xz)
- [std/zlib](/std/zlib)


## Examples

- [example/mzcat](/example/mzcat)
- [example/toy-genlib](/example/toy-genlib)
- [example/zcat](/example/zcat)


## [Quirks](/doc/note/quirks.md)

- [LZMA decoder quirks](/std/lzma/decode_quirks.wuffs)
- [LZW decoder quirks](/std/lzw/decode_quirks.wuffs)
- [XZ decoder quirks](/std/xz/decode_quirks.wuffs)
- [Zlib decoder quirks](/std/zlib/decode_quirks.wuffs)


## Related Documentation

- [I/O (Input / Output)](/doc/note/io-input-output.md)

See also the general remarks on [Wuffs' standard library](/doc/std/README.md).
