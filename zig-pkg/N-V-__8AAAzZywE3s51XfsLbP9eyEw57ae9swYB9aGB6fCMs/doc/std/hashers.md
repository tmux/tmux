# Hashers

Hashers take a stream of bytes (of arbitrary length) and produce a fixed length
value - the hash. For example, the CRC-32/IEEE hashing algorithm produces a 32
bit value (a `base.u32`). The MD5 hashing algorithm produces a 128 bit value.

Wuffs' hasher implementations have one core method. For 32 bit hashes, the
method signature is `update_u32!(x: roslice base.u8) base.u32`. It
incrementally updates the hasher object's state with the addition data `x`, and
returns the hash value so far, for all of the data up to and including `x`.

This method is stateful. Calling `update_u32` twice with the same slice of
bytes can produce two different hash values. Conversely, calling `update_u32`
twice with two different slices should be equivalent to calling it once on
their concatenation. [Re-initialize](/doc/note/initialization.md) the object to
reset the state.

The `update_u32!` method is equivalent to calling `update!` and then
`checksum_u32` (which is a pure method). For the rare algorithms where
computing the checksum from internal state is relatively expensive, and when
streaming many relatively-small updates, it might be more efficient to call
`update!` N times and `checksum_u32` only once, instead of calling
`update_u32!` N times.

Wuffs' hasher implementations are not cryptographic. They make no attempt to
resist timing attacks.


## API Listing

In Wuffs syntax, the `base.hasher_u32` methods are:

- `checksum_u32() u32`
- `get_quirk(key: u32) u64`
- `set_quirk!(key: u32, value: u64) status`
- `update!(x: slice u8)`
- `update_u32!(x: slice u8) u32`

The `base.hasher_u64` and `base.hasher_bitvec256` methods are similar, except
for the obvious difference of calculating a 64-bit or 256-bit (not 32-bit)
checksum.


## Implementations

- [std/adler32](/std/adler32)
- [std/crc32](/std/crc32)
- [std/crc64](/std/crc64)
- [std/sha256](/std/sha256)
- [std/xxhash32](/std/xxhash32)
- [std/xxhash64](/std/xxhash64)


## Examples

- [example/crc32](/example/crc32)


## Related Documentation

- [I/O (Input / Output)](/doc/note/io-input-output.md)

See also the general remarks on [Wuffs' standard library](/doc/std/README.md).
