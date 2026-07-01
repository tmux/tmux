# Random Access Compression: RAC

Status: Draft (as of September 2019). There is no compatibility guarantee yet.


## Overview

RAC is a *compressed* file format that allows *random access* (not just
sequential access) to the decompressed contents. In comparison to some other
popular compression formats, all four of the
[Zlib](https://www.ietf.org/rfc/rfc1950.txt),
[Brotli](https://www.ietf.org/rfc/rfc7932.txt),
[LZ4](https://github.com/lz4/lz4/blob/master/doc/lz4_Frame_format.md) and
[Zstandard](https://www.ietf.org/rfc/rfc8478.txt) specifications explicitly
contain the identical phrase: "the data format defined by this specification
does not attempt to allow random access to compressed data".

Compression means that the derived file is typically smaller than the original
file. Random access means that, starting from the compressed file, it is
possible to reconstruct the half-open byte range `[di .. dj)` of the
decompressed file without always having to first decompress all of `[0 .. di)`.

Conceptually, the decompressed file is partitioned into non-overlapping chunks.
Each compressed chunk can be decompressed independently (although possibly
sharing additional context, such as a LZ77 prefix dictionary). A RAC file also
contains a hierarchical index of those chunks.

RAC is a container format, and while it supports common compression codecs like
Zlib, Brotli, LZ4 and Zstandard, it is not tied to any particular compression
codec.

Non-goals for version 1 include:

  - Filesystem metadata like file names, file sizes and modification times.
  - Multiple source files.

There is the capability (see attributes and reserved `TTag`s, below) but no
promise to address these in a future RAC version. There might not be a need to,
as other designs such as [SquashFS](http://squashfs.sourceforge.net/) and EROFS
([Extendable Read-Only File System](https://lkml.org/lkml/2018/5/31/306))
already exist.

Non-goals in general include:

  - Encryption.
  - Streaming decompression.

A companion document has further discussion of [RAC related
work](/doc/spec/rac-related-work.md).


### Glossary

  - `CBias` is the delta added to a `CPointer` to produce a `COffset`.
  - `DBias` is the delta added to a `DPointer` to produce a `DOffset`.
  - `CFile` is the compressed file.
  - `DFile` is the decompressed file.
  - `CFileSize` is the size of the `CFile`.
  - `DFileSize` is the size of the `DFile`.
  - `COffset` is a byte offset in `CSpace`.
  - `DOffset` is a byte offset in `DSpace`.
  - `CPointer` is a relative `COffset`, prior to bias-correction.
  - `DPointer` is a relative `DOffset`, prior to bias-correction.
  - `CRange` is a `Range` in `CSpace`.
  - `DRange` is a `Range` in `DSpace`.
  - `CSpace` means that byte offsets refer to the `CFile`.
  - `DSpace` means that byte offsets refer to the `DFile`.

`Range` is a pair of byte offsets `[i .. j)`, in either `CSpace` or `DSpace`.
It is half-open, containing every byte offset `x` such that `(i <= x)` and `(x
< j)`. It is invalid to have `(i > j)`. The size of a `Range` equals `(j - i)`.

All bytes are 8 bits and unless explicitly specified otherwise, all fixed-size
unsigned integers (e.g. `uint32_t`, `uint64_t`) are encoded little-endian.
Within those unsigned integers, bit 0 is the least significant bit and e.g. bit
31 is the most significant bit of a `uint32_t`.

The maximum supported `CFileSize` and the maximum supported `DFileSize` are the
same number: `0x0000_FFFF_FFFF_FFFF`, which is `((1 << 48) - 1)`.


## File Structure

A RAC file (the `CFile`) must be at least 32 bytes long, and start with the 3
byte `Magic` (see below), so that no valid RAC file can also be e.g. a valid
JPEG file.

The `CFile` contains a tree of `Node`s. Each `Node` is either a `Branch Node`
(pointing to between 1 and 255 child `Node`s) or a `Leaf Node`. There must be
at least one `Branch Node`, called the `Root Node`. Parsing a `CFile` requires
knowing the `CFileSize` in order to identify the `Root Node`, which is either
at the start or the end of the `CFile`.

Each `Node` has a `DRange`, which can be empty. `Branch Node`s can also have
attributes: elements that aren't child `Node`s, which must have an empty
`DRange`. Empty elements contain metadata or other decompression context such
as a shared dictionary.

Each `Leaf Node` also has 3 `CRange`s (`Primary`, `Secondary` and `Tertiary`),
any or all of which may be empty. The contents of the `CFile`, within those
`CRange`s, are decompressed according to the `Codec` (see below) to reconstruct
that part of the `DFile` within the `Leaf Node`'s `DRange`.


## Branch Nodes

A `Branch Node`'s encoding in the `CFile` has a variable byte size, between 32
and 4096 inclusive, depending on its number of children. Specifically, it
occupies `((Arity * 16) + 16)` bytes, grouped into 8 byte segments (but not
necessarily 8 byte aligned), starting at a `COffset` called its `Branch
COffset`:

    +-+-+-+-+-+-+-+-+
    |0|1|2|3|4|5|6|7|
    +-+-+-+-+-+-+-+-+
    |Magic|A|Che|0|T|  Magic, Arity, Checksum,     Reserved (0),  TTag[0]
    | DPtr[1]   |0|T|  DPtr[1],                    Reserved (0),  TTag[1]
    | DPtr[2]   |0|T|  DPtr[2],                    Reserved (0),  TTag[2]
    | ...       |0|.|  ...,                        Reserved (0),  ...
    | DPtr[A-2] |0|T|  DPtr[Arity-2],              Reserved (0),  TTag[Arity-2]
    | DPtr[A-1] |0|T|  DPtr[Arity-1],              Reserved (0),  TTag[Arity-1]
    | DPtr[A]   |0|C|  DPtr[Arity] a.k.a. DPtrMax, Reserved (0),  CodecByte
    | CPtr[0]   |L|S|  CPtr[0],                    CLen[0],       STag[0]
    | CPtr[1]   |L|S|  CPtr[1],                    CLen[1],       STag[1]
    | CPtr[2]   |L|S|  CPtr[2],                    CLen[2],       STag[2]
    | ...       |L|.|  ...,                        ...,           ...
    | CPtr[A-2] |L|S|  CPtr[Arity-2],              CLen[Arity-2], STag[Arity-2]
    | CPtr[A-1] |L|S|  CPtr[Arity-1],              CLen[Arity-1], STag[Arity-1]
    | CPtr[A]   |V|A|  CPtr[Arity] a.k.a. CPtrMax, Version,       Arity
    +-+-+-+-+-+-+-+-+

See the "Examples" section below for example RAC files and, in particular,
example `Branch Node`s.

For the `(XPtr | Other6 | Other7)` 8 byte fields, the `XPtr` occupies the low
48 bits (as a little-endian `uint64_t`) and the `Other` fields occupy the high
16 bits.

The `CPtr` and `DPtr` values are what is explicitly written in the `CFile`'s
bytes. These are added to a `Branch Node`'s implicit `Branch CBias` and `Branch
DBias` values to give the implicit `COff` and `DOff` values: `COff[i]` and
`DOff[i]` are defined to be `(Branch_CBias + CPtr[i])` and `(Branch_DBias +
DPtr[i])`.

`CPtrMax` is another name for `CPtr[Arity]`, and `COffMax` is defined to be
`(Branch_CBias + CPtrMax)`. Likewise for `DPtrMax` and `DOffMax`.

The `DPtr[0]` value is implicit, and always equals zero, so that `DOff[0]`
always equals the `Branch DBias`.

  - For the `Root Node`, the `DPtrMax` also sets the `DFileSize`. The `Branch
    CBias` and `Branch DBias` are both zero. The `Branch COffset` is determined
    by the "Root Node" section below.
  - For a child `Branch Node`, the `Branch COffset`, `Branch CBias` and `Branch
    DBias` are given by the parent `Branch Node`. See the "Search Within a
    Branch Node" section below.


### Magic

`Magic` is the three bytes `"\x72\xC3\x63"`, which is invalid UTF-8 but is
`"rÃc"` in ISO 8859-1. The tilde isn't particularly meaningful, other than
`"rÃc"` being a nonsensical word (with nonsensical capitalization) that is
unlikely to appear in other files.

Every `Branch Node` must start with these `Magic` bytes, not just a `Branch
Node` positioned at the start of the `CFile`.


### Arity

`Arity` is the `Branch Node`'s number of elements: the number of child `Node`s
plus the number of attributes. Having zero children is invalid.

The `Arity` byte is given twice: the fourth byte and the final byte of the
`Branch Node`. The two values must match.

The repetition lets a RAC reader determine the size of the `Branch Node` data
(as the size depends on the `Arity`), given either its start or its end offset
in `CSpace`. For almost all `Branch Node`s, we will know its start offset (its
`Branch COffset`), but for a `Root Node` at the end of a `CFile`, we will only
know its end offset.


### Checksum

`Checksum` is a checksum of the `Branch Node`'s bytes. It is not a checksum of
the `CFile` or `DFile` contents pointed to by a `Branch Node`. Content
checksums are a `Codec`-specific consideration.

The little-endian `uint16_t` `Checksum` value is the low 16 bits XOR'ed with
the high 16 bits of the `uint32_t` CRC-32 IEEE checksum of the `((Arity * 16) +
10)` bytes immediately after the `Checksum`. The 4 bytes immediately before the
`Checksum` are not considered: the `Magic` bytes have only one valid value and
the `Arity` byte near the start is replicated by the `Arity` byte at the end.


### Reserved (0)

The `Reserved (0)` bytes must have the value `0x00`.


### COffs and DOffs, STags and TTags

For every `a` in the half-open range `[0 .. Arity)`, the `a`'th element has two
tags, `STag[a]` and `TTag[a]`, and a `DRange` of `[DOff[a] .. DOff[a+1])`. The
`DOff` values must be non-decreasing: see the "Branch Node Validation" section
below.

A `TTag[a]` of `0xFE` means that that element is a `Branch Node` child.

A `TTag[a]` of `0xFD` means that there is no child, but is instead a `Codec
Element` attribute, whose `DRange` must be empty, and the rest of this section
does not apply: the `STag` is ignored.

A `TTag[a]` in the half-open range `[0xC0 .. 0xFD)` is reserved. Otherwise, the
element is a `Leaf Node` child.

A child `Branch Node`'s `SubBranch COffset` is defined to be `COff[a]`. Its
`SubBranch DBias` and `SubBranch DOffMax` are defined to be `DOff[a]` and
`DOff[a+1]`.

  - When `(STag[a] < Arity)`, it is a `CBiasing Branch Node`. The `SubBranch
    CBias` is defined to be `(Branch_CBias + CPtr[STag[a]])`. This expression
    is equivalent to `COff[STag[a]]`.
  - When `(STag[a] >= Arity)`, it is a `CNeutral Branch Node`. The `SubBranch
    CBias` is defined to be `(Branch_CBias)`.

A child `Leaf Node`'s `STag[a]` and `TTag[a]` values are also called its `Leaf
STag` and `Leaf TTag`. It also has:

  - A `Primary CRange`, equal to `MakeCRange(a)`.
  - A `Secondary CRange`, equal to `MakeCRange(STag[a])`.
  - A `Tertiary CRange`, equal to `MakeCRange(TTag[a])`.

The `MakeCRange(i)` function defines a `CRange`. If `(i >= Arity)` then that
`CRange` is the empty range `[COffMax .. COffMax)`. Otherwise, the lower bound
is `COff[i]` and the upper bound is:

  - `COffMax` when `CLen[i]` is zero.
  - The minimum of `COffMax` and `(COff[i] + (CLen[i] * 1024))` when `CLen[i]`
    is non-zero.

In other words, the `COffMax` value clamps the `CRange` upper bound. The `CLen`
value, if non-zero, combines with the `COff` value to apply another clamp. The
`CLen` is given in units of 1024 bytes, but the `(COff[i] + (CLen[i] * 1024))`
value is not necessarily quantized to 1024 byte boundaries.

Note that, since `Arity` is at most 255, an `STag[a]` of `0xFF` always results
in a `CNeutral Branch Node` or an empty `Secondary CRange`. Likewise, a
`TTag[a]` of `0xFF` always results in an empty `Tertiary CRange`.


### COffMax

`COffMax` is an inclusive upper bound on every `COff` in a `Branch Node` and in
its descendent `Branch Node`s. A child `Branch Node` must not have a larger
`COffMax` than the parent `Branch Node`'s `COffMax`, and the `Root Node`'s
`COffMax` must equal the `CFileSize`. See the "Branch Node Validation" section
below.

A RAC file can therefore be incrementally modified, if the RAC writer only
appends new `CFile` bytes and does not re-write existing `CFile` bytes, so that
the `CFileSize` increases. Even if the old (smaller) RAC file's `Root Node` was
at the `CFile` start, the new (larger) `CFileSize` means that those starting
bytes are an obsolete `Root Node` (but still a valid `Branch Node`). The new
`Root Node` is therefore located at the end of the new RAC file.

Concatenating RAC files (concatenating in `DSpace`) involves concatenating the
RAC files in `CSpace` and then appending a new `Root Node` with `CBiasing
Branch Node`s pointing to each source RAC file's `Root Node`.


### Version

`Version` must have the value `0x01`, indicating version 1 of the RAC format.
The `0x00` value is reserved, although future editions may use other positive
values.


### Codec

`Codec`s define specializations of RAC, such as "RAC + Zlib" or "RAC + Brotli".
It is valid for a "RAC + Zstandard" only decoder to reject a "RAC + Brotli"
file, even if it is a valid RAC file. Recall that RAC is just a container, and
not tied to any particular compression codec.

There are two categories of `Codec`s: the high `0x80` bit of the `Codec Byte`
being `0` or `1` denotes a `Short` or `Long` codec respectively. `Short Codec`s
are represented by a single byte (the `Codec Byte`). `Long Codec`s use that
`Codec Byte` to locate 7 additional bytes: a `Codec Element`.

For both `Short Codec`s and `Long Codec`s, the second-highest `0x40` bit of the
`Codec Byte` is the `Mix Bit`. A `Mix Bit` of `0` means that all of the
`Node`'s descendents have exactly the same `Codec` (not just the same `Codec
Byte`; `Short Codec`s and `Long Codec`s are not considered exactly the same
even if they represent the same compression algorithm). A `Mix Bit` of `1`
means that descendents may have a different `Codec`.

For `Short Codec`s, the remaining low 6 bits correspond to a specific
compression algorithm:

  - `0x00` means "RAC + Zeroes".
  - `0x01` means "RAC + Zlib".
  - `0x02` means "RAC + LZ4".
  - `0x03` means "RAC + Zstandard".
  - All other values are reserved.

For `Long Codec`s, the remaining low 6 bits of the `Codec Byte` define a number
`c64`. The lowest index `i` out of `(c64 + (64 * 0))`, `(c64 + (64 * 1))`,
`(c64 + (64 * 2))` and `(c64 + (64 * 3))` such that `TTag[i]` is `0xFD` locates
the 7 bytes that identifies the `Codec`. The location is what would otherwise
occupy the `i`th element's `CPtr | CLen` space. It is invalid for no such `i`
to exist.

`Long Codec` values are not reserved by this specification, other than 7 NUL
bytes also means "RAC + Zeroes". Users may define their own values for
arbitrary compression algorithms. Maintaining a centralized registry mapping
values to algorithms is out of scope of this specification, although we suggest
a convention that the 7 bytes form a human-readable (ASCII) string, padded with
trailing NUL bytes. For example, a hypothetical "Middle Out version 2"
compression algorithm that typically used the ".mdo2" file extension might be
represented by the 7 bytes `"mdo2\x00\x00\x00"`.


### Branch Node Validation

The first time that a RAC reader visits any particular `Branch Node`, it must
check that the `Magic` matches, the two `Arity` values match and are non-zero,
there is at least one child `Node` (not just non-`Node` attributes), the
computed checksum matches the listed `Checksum` and that the RAC reader accepts
the `Version`. For `Long Codec`s, there must exist an `0xFD` `TTag` as per the
previous section.

It must also check that all of its `DOff` values are sorted: `(DOff[a] <=
DOff[a+1])` for every `a` in the half-open range `[0 .. Arity)`. By induction,
this means that all of its `DOff` values do not exceed `DOffMax`, and again by
induction, therefore do not exceed `DFileSize`.

It must also check that, other than `Codec Element` attributes, all of its
`COff` values do not exceed `COffMax` (and again by induction, therefore do not
exceed `CFileSize`). Other than that, `COff` values do not have to be sorted:
successive `Node`s (in `DSpace`) can be out of order (in `CSpace`), allowing
for incrementally modified RAC files.

For the `Root Node`, its `COffMax` must equal the `CFileSize`. Recall that
parsing a `CFile` requires knowing the `CFileSize`, and also that a `Root
Node`'s `Branch CBias` is zero, so its `COffMax` equals its `CPtrMax`.

For a child `Branch Node`, if the parent's `Codec` does not have the `Mix Bit`
set then the child's `Codec` must equal its parent's. Furthermore, its
`Version` must be less than or equal to its parent's `Version`, its `COffMax`
must be less than or equal to its parent's `COffMax`, and its `DOffMax` must
equal its parent's `SubBranch DOffMax`. The `DOffMax` condition is equivalent
to checking that the parent and child agree on the child's size in `DSpace`.
The parent states that it is its `(DPtr[a+1] - DPtr[a])` and the child states
that it is its `DPtrMax`.

One conservative way to check `Branch Node`s' validity on first visit is to
check them on every visit, as validating any particular `Branch Node` is
idempotent, but other ways are acceptable.


## Root Node

The `Root Node` might be at the start of the `CFile`, as this might optimize
alignment of `Branch Node`s and of `CRange`s. All `Branch Node`s' sizes are
multiples of 16 bytes, and a maximal `Branch Node` is exactly 4096 bytes.

The `Root Node` might be at the end of the `CFile`, as this allows one-pass
(streaming) encoding of a RAC file. It also allows appending to, concatenating
or incrementally modifying existing RAC files relatively cheaply.

To find the `Root Node`, first look for a valid `Root Node` at the `CFile`
start. If and only if that fails, look at the `CFile` end. If that also fails,
it is not a valid RAC file.


### Root Node at the CFile Start

The fourth byte of the `CFile` gives the `Arity`, assuming the `Root Node` is
at the `CFile` start. If it is zero, fail over to the `CFile` end. A RAC writer
that does not want to place the `Root Node` at the `CFile` start should
intentionally write a zero to the fourth byte.

Otherwise, that `Arity` defines the size in bytes of any starting `Root Node`:
`((Arity * 16) + 16)`. If the `CFileSize` is less than this size, fail over to
the `CFile` end.

If those starting bytes form a valid `Root Node` (as per the "Branch Node
Validation" section), including having a `CPtrMax` equal to the `CFileSize`,
then we have indeed found our `Root Node`: its `Branch COffset` is zero.
Otherwise, fail over to the `CFile` end.


### Root Node at the CFile End

If there is no valid `Root Node` at the `CFile` start then the last byte of the
`CFile` gives the `Root Node`'s `Arity`. This does not necessarily need to
match the fourth byte of the `CFile`.

As before, that `Arity` defines the size in bytes of any ending `Root Node`:
`((Arity * 16) + 16)`. If the `CFileSize` is less than this size, it is not a
valid RAC file.

If those ending bytes form a valid `Root Node` (as per the "Branch Node
Validation" section), including having a `CPtrMax` equal to the `CFileSize`,
then we have indeed found our `Root Node`: its `Branch COffset` is the
`CFileSize` minus the size implied by the `Arity`. Otherwise, it is not a valid
RAC file.


## DRange Reconstruction

To reconstruct the `DRange` `[di .. dj)`, if that `DRange` is empty then the
request is trivially satisfied.

Otherwise, if `(dj > DFileSize)` then reject the request.

Otherwise, start at the `Root Node` and continue to the next section to find
the `Leaf Node` containing the `DOffset` `di`.


### Search Within a Branch Node

Load (and validate) the `Branch Node` given its `Branch COffset`, `Branch
CBias` and `Branch DBias`.

Find the largest child index `a` such that `(a < Arity)` and `(DOff[a] <= di)`
and `(DOff[a+1] > di)`, then examine `TTag[a]` to see if the child is a `Leaf
Node`. If so, skip to the next section.

For a `Branch Node` child, let `CRemaining` equal this `Branch Node`'s (the
parents') `COffMax` minus the `SubBranch COffset`. It invalid for `CRemaining`
to be less than 4, or to be less than the child's size implied by the child's
`Arity` byte at a `COffset` equal to `(SubBranch_COffset + 3)`.

The `SubBranch COffset`, `SubBranch CBias` and `SubBranch DBias` from the
parent `Branch Node` become the `Branch COffset`, `Branch CBias` and `Branch
DBias` of the child `Branch Node`. In order to rule out infinite loops, at
least one of these two conditions must hold:

  - The child's `Branch COffset` is less than the parent's `Branch COffset`.
  - The child's `DPtrMax` is less than the parent's `DPtrMax`.

It is valid for one of those conditions to hold between a parent-child pair and
the other condition to hold between the next parent-child pair.

Repeat this "Search Within a Branch Node" section with the child `Branch Node`.


### Decompressing a Leaf Node

If a `Leaf Node`'s `DRange` is empty, decompression is a no-op and skip the
rest of this section. Specifically, a low-level library that iterates over a
RAC file's chunks, without actually performing decompression, should skip over
empty chunks instead of yielding them to its caller.

Otherwise, decompression combines the `Primary CRange`, `Secondary CRange` and
`Tertiary CRange` slices of the `CFile`, and the `Leaf STag` and `Leaf TTag`
values, in a `Codec`-specific way to produce decompressed data.

There are two general principles, although specific `Codec`s can overrule:

  - The `Codec` may produce fewer bytes than the `DRange` size. In that case,
    the remaining bytes (in `DSpace`) are set to NUL (`memset` to zero).
  - The `Codec` may consume fewer bytes than each `CRange` size, and the
    compressed data typically contains an explicit "end of data" marker. In
    that case, the remaining bytes (in `CSpace`) are ignored. Padding allows
    `COff` values to optionally be aligned.

It is invalid to produce more bytes than the `DRange` size or to consume more
bytes than each `CRange` size.


### Continuing the Reconstruction

If decompressing that `Leaf Node` did not fully reconstruct `[di .. dj)`,
advance through the `Node` tree in depth first search order, decompressing
every `Leaf Node` along the way, until we have gone up to or beyond `dj`.

During that traversal, `Node`s with an empty `DRange` should be skipped, even
if they are `Branch Node`s.


# Specific Codecs


## Common Dictionary Format

Unless otherwise noted, codecs use this common dictionary format.

If a `Leaf Node`'s `Secondary CRange` is empty then there is no dictionary.
Otherwise, the `Secondary CRange` must be at least 8 bytes long:

  - 4 byte little-endian `uint32_t` `Dictionary Length`. The high 2 bits are
    reserved and must be zero. The maximum (inclusive) `Dictionary Length` is
    therefore `((1 << 30) - 1)`, or `1_073_741_823` bytes.
  - `Dictionary Length` bytes `Dictionary`.
  - 4 byte little-endian `uint32_t` `Dictionary Checksum`, the Checksum` is
    CRC-32 IEEE checksum over the `Dictionary`'s bytes (excluding both the 4
    byte prefix and the 4 byte suffix).
  - Padding (ignored).

The `Leaf TTag` must be `0xFF`. All other `Leaf TTag` values (below `0xC0`) are
reserved. The empty `Tertiary CRange` is ignored. The `Leaf STag` value is also
ignored, other than deriving the `Secondary CRange`.


## RAC + Zeroes

The `CRange`s are ignored. The `DRange` is filled with NUL bytes (`memset` to
zero).


## RAC + Zlib

The `CFile` data in the `Leaf Node`'s `Primary CRange` is decompressed as Zlib
(RFC 1950), possibly referencing a LZ77 prefix dictionary (what the RFC calls a
"preset dictionary") wrapped in RAC's common dictionary format, described
above.


## RAC + LZ4

TODO.


## RAC + Zstandard

The `CFile` data in the `Leaf Node`'s `Primary CRange` is decompressed as
Zstandard (RFC 8478), possibly referencing a dictionary wrapped in RAC's common
dictionary format, described above. After unwrapping, the dictionary's bytes
can be either a "raw" or "trained" dictionary, as per RFC 8478 section 5.


# Examples

These examples display RAC files in the format of the `hexdump -C` command line
tool. They are deliberately very short, for ease of understanding. Realistic
RAC files, with larger chunk sizes, would typically exhibit much better
compression ratios.


## Zlib Example (Root Node at End)

The first example is relatively simple. The root node (located at the CFile
end) only has one child: a leaf node whose compressed contents starts at
position `0x04`. Decompressing that chunk produces the 6 bytes
["More!\n"](https://play.golang.org/p/ZSYmQLgv1RR).

    00000000  72 c3 63 00 78 9c 01 06  00 f9 ff 4d 6f 72 65 21  |r.c.x......More!|
    00000010  0a 07 42 01 bf 72 c3 63  01 65 a9 00 ff 06 00 00  |..B..r.c.e......|
    00000020  00 00 00 00 01 04 00 00  00 00 00 01 ff 35 00 00  |.............5..|
    00000030  00 00 00 01 01                                    |.....|


## Zlib Example (Root Node at Start)

The second example consists of a root node with four children: one metadata
node (a shared dictionary) and three data nodes. The shared dictionary,
"\x20sheep.\n" is `0x00000008` bytes long and its CRC-32 IEEE checksum is
`0x477A8DD0`. The third child's (the second data node)'s compressed contents
starts at position `0x75`. Decompressing that chunk, together with that shared
dictionary, produces the 11 bytes ["Two
sheep.\n"](https://play.golang.org/p/Jh9Wyp6PLID). The complete decoding of all
three data chunks is "One sheep.\nTwo sheep.\nThree sheep.\n".

    00000000  72 c3 63 04 37 39 00 ff  00 00 00 00 00 00 00 ff  |r.c.79..........|
    00000010  0b 00 00 00 00 00 00 ff  16 00 00 00 00 00 00 ff  |................|
    00000020  23 00 00 00 00 00 00 01  50 00 00 00 00 00 01 ff  |#.......P.......|
    00000030  60 00 00 00 00 00 01 00  75 00 00 00 00 00 01 00  |`.......u.......|
    00000040  8a 00 00 00 00 00 01 00  a1 00 00 00 00 00 01 04  |................|
    00000050  08 00 00 00 20 73 68 65  65 70 2e 0a d0 8d 7a 47  |.... sheep....zG|
    00000060  78 f9 0b e0 02 6e f2 cf  4b 85 31 01 01 00 00 ff  |x....n..K.1.....|
    00000070  ff 17 21 03 90 78 f9 0b  e0 02 6e 0a 29 cf 87 31  |..!..x....n.)..1|
    00000080  01 01 00 00 ff ff 18 0c  03 a8 78 f9 0b e0 02 6e  |..........x....n|
    00000090  0a c9 28 4a 4d 85 71 00  01 00 00 ff ff 21 6e 04  |..(JM.q......!n.|
    000000a0  66                                                |f|


## Zlib Example (Concatenation)

The third example demonstrates concatenating two RAC files: the two examples
above. The decompressed form of the resultant RAC file is the concatenation of
the two decompressed forms: "One sheep.\nTwo sheep.\nThree sheep.\nMore!\n".
Its 41 decompressed bytes consists of the "sheep" RAC file's 35 bytes and then
the "more" RAC file's 6 bytes. In hexadecimal, `0x29 = 0x23 + 0x06`, and the
`0x29` and `0x23` numbers can be seen in the compressed form's bytes.

The compressed form (what's shown in `hexdump -C` format below) is the
concatenation of the two compressed forms, plus a new root node (64 bytes
starting at position `0xD6`). Even though the overall RAC file starts with the
"sheep" RAC file, whose root node was at its start, those opening bytes are no
longer a valid root node for the larger file.

That new root node has 3 children: 1 metadata node and 2 branch nodes. The
metadata node (one whose `DRange` is empty) is required because one of the
original RAC files' root node is not located at its start. Walking to that
child branch node needs two `COffset` values: one for the embedded RAC file's
start and one for the embedded RAC file's root node.

    00000000  72 c3 63 04 37 39 00 ff  00 00 00 00 00 00 00 ff  |r.c.79..........|
    00000010  0b 00 00 00 00 00 00 ff  16 00 00 00 00 00 00 ff  |................|
    00000020  23 00 00 00 00 00 00 01  50 00 00 00 00 00 01 ff  |#.......P.......|
    00000030  60 00 00 00 00 00 01 00  75 00 00 00 00 00 01 00  |`.......u.......|
    00000040  8a 00 00 00 00 00 01 00  a1 00 00 00 00 00 01 04  |................|
    00000050  08 00 00 00 20 73 68 65  65 70 2e 0a d0 8d 7a 47  |.... sheep....zG|
    00000060  78 f9 0b e0 02 6e f2 cf  4b 85 31 01 01 00 00 ff  |x....n..K.1.....|
    00000070  ff 17 21 03 90 78 f9 0b  e0 02 6e 0a 29 cf 87 31  |..!..x....n.)..1|
    00000080  01 01 00 00 ff ff 18 0c  03 a8 78 f9 0b e0 02 6e  |..........x....n|
    00000090  0a c9 28 4a 4d 85 71 00  01 00 00 ff ff 21 6e 04  |..(JM.q......!n.|
    000000a0  66 72 c3 63 00 78 9c 01  06 00 f9 ff 4d 6f 72 65  |fr.c.x......More|
    000000b0  21 0a 07 42 01 bf 72 c3  63 01 65 a9 00 ff 06 00  |!..B..r.c.e.....|
    000000c0  00 00 00 00 00 01 04 00  00 00 00 00 01 ff 35 00  |..............5.|
    000000d0  00 00 00 00 01 01 72 c3  63 03 83 16 00 ff 00 00  |......r.c.......|
    000000e0  00 00 00 00 00 fe 23 00  00 00 00 00 00 fe 29 00  |......#.......).|
    000000f0  00 00 00 00 00 01 a1 00  00 00 00 00 00 ff 00 00  |................|
    00000100  00 00 00 00 04 01 b6 00  00 00 00 00 04 00 16 01  |................|
    00000110  00 00 00 00 01 03                                 |......|


Focusing on that new root node:

    000000d0                    72 c3  63 03 83 16 00 ff 00 00
    000000e0  00 00 00 00 00 fe 23 00  00 00 00 00 00 fe 29 00
    000000f0  00 00 00 00 00 01 a1 00  00 00 00 00 00 ff 00 00
    00000100  00 00 00 00 04 01 b6 00  00 00 00 00 04 00 16 01
    00000110  00 00 00 00 01 03

Re-formatting to highlight the groups-of-8-bytes structure and reprise the
"Branch Nodes" section's diagram:

    000000d6    72 c3 63 03 83 16 00 ff    |Magic|A|Che|0|T|
    000000de    00 00 00 00 00 00 00 fe    | DPtr[1]   |0|T|
    000000e6    23 00 00 00 00 00 00 fe    | DPtr[2]   |0|T|
    000000ee    29 00 00 00 00 00 00 01    | DPtr[A]   |0|C|
    000000f6    a1 00 00 00 00 00 00 ff    | CPtr[0]   |L|S|
    000000fe    00 00 00 00 00 00 04 01    | CPtr[1]   |L|S|
    00000106    b6 00 00 00 00 00 04 00    | CPtr[2]   |L|S|
    0000010e    16 01 00 00 00 00 01 03    | CPtr[A]   |V|A|

Its `CodecByte` (and therefore its `Short Codec`) is `0x01`, "RAC + Zlib", its
Version is `0x01` and its `Arity` is `0x03`. The `DPtr` values are `0x0000`
(implicit), `0x0000`, `0x0023` and `0x0029`. The `CPtr` values are `0x00A1`,
`0x0000`, `0x00B6` and `0x0116` (the size of the whole RAC file). Note that the
`CPtr` values are not sorted. The last two children's `TTag`s are `0xFE` and
`0xFE` and their `STag`s are `0x01` and `0x00`, which means that they are both
`CBiasing Branch Node`s.


# Reference Implementation

C programming language libraries:

  - TODO. Follow [this GitHub issue](https://github.com/google/wuffs/issues/22)
    for updates.

Go programming language libraries:

  - [RAC](https://godoc.org/github.com/google/wuffs/lib/rac)
  - [RAC + Zlib](https://godoc.org/github.com/google/wuffs/lib/raczlib)

Command line tool, installable via `go install
github.com/google/wuffs/cmd/ractool`:

  - [ractool](https://godoc.org/github.com/google/wuffs/cmd/ractool)


# Acknowledgements

I (Nigel Tao) thank Robert Obryk, Sanjay Ghemawat and Sean Klein for their
review.


---

Updated on September 2019.
