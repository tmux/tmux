# Standard Library

Wuffs' standard library consists of multiple packages, each implementing a
particular file format or algorithm. Those packages can be grouped into several
categories:

- [Compression Decoders](/doc/std/compression-decoders.md).
- [Hashers](/doc/std/hashers.md).
- [Image Decoders](/doc/std/image-decoders.md).

The general pattern is that a package `foo` (e.g. `jpeg`, `png`) contains a
struct `bar` (e.g. `hasher`, `decoder`, etc) that implements a
package-independent interface. For example, every compression decoder struct
would have a `transform_io` method. In C, this would be invoked as

```
// Allocate and initialize the struct.
wuffs_foo__decoder* dec = etc;

// Do the work. Error checking is not shown, for brevity.
wuffs_base__status status = wuffs_foo__decoder__transform_io(dec, etc);
```

When that C library is used as C++, that last line can be shortened:

```
wuffs_base__status status = dec->transform_io(etc);
```

See also the [glossary](/doc/glossary.md), as well as the notes on:

- [Coroutines](/doc/note/coroutines.md).
- [Initialization](/doc/note/initialization.md).
- [Statuses](/doc/note/statuses.md).


## Modules and Dependencies

By default, building Wuffs' standard library builds the entire thing (provided
that you've defined the `WUFFS_IMPLEMENTATION` C macro), implementing a variety
of codecs and file formats.

Packages can be individually allow-listed, for smaller binaries and faster
compiles. Opt in by also defining `WUFFS_CONFIG__MODULES` and then also
`WUFFS_CONFIG__MODULE__ETC` for each `ETC` (and its dependencies, listed below)
to enable.

- `ADLER32:   BASE`
- `BMP:       BASE`
- `BZIP2:     BASE`
- `CBOR:      BASE`
- `CRC32:     BASE`
- `CRC64:     BASE`
- `DEFLATE:   BASE`
- `ETC2:      BASE`
- `GIF:       BASE`
- `GZIP:      BASE, CRC32, DEFLATE`
- `JPEG:      BASE`
- `JSON:      BASE`
- `LZIP:      BASE, CRC32, LZMA`
- `LZMA:      BASE`
- `LZW:       BASE`
- `NETPBM:    BASE`
- `NIE:       BASE`
- `PNG:       BASE, ADLER32, CRC32, DEFLATE, ZLIB`
- `QOI:       BASE`
- `SHA256:    BASE`
- `TGA:       BASE`
- `THUMBHASH: BASE`
- `VP8:       BASE`
- `WBMP:      BASE`
- `WEBP:      BASE, VP8`
- `XXHASH32:  BASE`
- `XXHASH64:  BASE`
- `XZ:        BASE, CRC32, CRC64, LZMA, SHA256`
- `ZLIB:      BASE, ADLER32, DEFLATE`

For the [auxiliary modules](/doc/note/auxiliary-code.md):

- `AUX_CBOR:  AUX_BASE, BASE, CBOR`
- `AUX_IMAGE: AUX_BASE, BASE` and whichever image-related modules (and their
  dependencies) you want, e.g. `GIF`, `PNG`, etc.
- `AUX_JSON:  AUX_BASE, BASE, JSON`
