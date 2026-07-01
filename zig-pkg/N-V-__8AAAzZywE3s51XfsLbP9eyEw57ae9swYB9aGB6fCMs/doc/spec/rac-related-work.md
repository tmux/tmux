# Random Access Compression (RAC) Related Work

This is a companion document to the [Random Access Compression (RAC)
Specification](/doc/spec/rac-spec.md). Its contents aren't necessary for
implementing the RAC file format, but gives further context on what RAC is and
is not.

In general, RAC differs from most other archive or compression formats in a
number of ways. This doesn't mean that RAC is necessarily better or worse than
these other designs, just that different designs make different trade-offs.

For example, a key feature is that RAC supports [shared compression
dictionaries](http://fastcompression.blogspot.com/2018/02/when-to-use-dictionary-compression.html),
embedded within the archive file. This can claw back some of the compression
ratio lost when splitting a source file into independently compressed chunks.
The trade-off is that decompressing a RAC file therefore requires random access
to the compressed file, not just sequential access. Unlike some other formats
discussed below, RAC cannot do streaming (one pass) decoding. Decompression
might not be able to start until the entire compressed file is available: the
root node of the index might be at the end of the compressed file.

Similarly, unlike Gzip or XZ, concatenating two RAC files together does not
produce a valid RAC file. A new root node also has to be appended afterwards,
which is cheap and easy, but not a no-op. Nonetheless, that new root node
allows for incremental, append-only modification of a RAC file: removing,
replacing or inserting bytes in the middle of the decompressed form by only
appending to the compressed form.

A few more differences between RAC and most if not all of the other file
formats or algorithms discussed further below:

  - RAC itself isn't tied to any one underlying compression codec. Obviously,
    "RAC + Zlib" is tied to Zlib, and "RAC + Brotli" is tied to Brotli, but if
    somebody invents a new "MiddleOut" compression codec, one that wasn't
    random access per se, it would be easy to have "RAC + MiddleOut".
  - RAC seek points are identified by numbers (i.e. `DOffset`s), not strings
    (i.e. file names). Some other formats' seek points are positioned only at
    source file boundaries, as they're primarily used to compress part of a
    file system, containing directories and (variable length) files. They
    cannot seek to a relative offset (in what RAC calls `DSpace`) within a
    source file.

These differences apply to established archive formats like
[Tar](https://en.wikipedia.org/wiki/Tar_%28computing%29) and
[Zip](https://en.wikipedia.org/wiki/Zip_%28file_format%29), and newer archive
formats like [Śiva](https://blog.sourced.tech/post/siva/).


## Software Products

[XZ](https://tukaani.org/xz/format.html) supports random access and multiple
underlying compression codecs like RAC. Out of all the formats listed here, it
is the one most similar to RAC, but as one of XZ's explicit goals is to support
streaming decoding, XZ does not support shared dictionaries or incremental
modification, as discussed above. Also, unlike RAC, finding the record that
contains any given `DOffset` requires a linear scan of all previous records,
even if those records don't need decompressing.

[DictZip](http://linuxcommand.org/man_pages/dictzip1.html) supports random
access like RAC, but is tied to the Gzip compression codec (every valid DictZip
file is also a valid Gzip file), and due to size restrictions in the Gzip
extension format, a DictZip file's decompressed size cannot exceed about 1.8
GiB. [CSIO](https://github.com/hoxnox/csio/blob/master/include/csio.h) seems to
do something similar. [Idzip](https://github.com/fidlej/idzip) avoids the 1.8
GiB limitation on the overall size (although retaining the 64 KiB limit on
individual chunks) by not requiring the index to fit inside a single Gzip
extension block.

[BGZF](http://samtools.github.io/hts-specs/SAMv1.pdf) is very similar to
DictZip, but also avoids the 1.8 GiB limitation. It places the uncompressed
size of each chunk together with each chunk, instead of having a single
contiguous index separate from chunk data. Unless the index is also [stored
externally](https://github.com/samtools/htslib/blob/develop/bgzip.1), random
access is achieved through following a linked list of size offsets. Some more
discussion is on [Heng Li's
blog](http://lh3.github.io/2014/07/05/random-access-to-zlib-compressed-files).

[Xflate](https://github.com/dsnet/compress/blob/master/doc/xflate-format.pdf)
is similar to BGZF, but extends the Deflate format instead of Gzip (every valid
Xflate file is also a valid Deflate file). Gzip extends Deflate with metadata
(where BGZF stashes its index information) and a checksum. Deflate has no
capacity for metadata per se, but Xflate exploits the fact that there are many
ways to encode an empty Deflate block (empty meaning that decoding it produces
zero bytes).

[Bzip2](https://sourceware.org/bzip2/) and
[RAZip](https://sourceforge.net/projects/razip/) are other formats based around
the same idea of partitioning the source file into independently compressed
chunks, and for each chunk, recording both its compressed and uncompressed
size. Once again, random access is achieved through following a linked list of
size offsets.

[Riegeli](https://github.com/google/riegeli) is a sequence-of-records format. A
RAC file arguably contains what Riegeli calls records (which are not files, as
they don't have names) and both formats support numerical seek points, and
multiple compression codecs, but Riegeli seeks to a point in what RAC calls
`CSpace`, whereas RAC seeks to a point in `DSpace`.

[QCOW](https://github.com/qemu/qemu/blob/master/docs/interop/qcow2.txt)
supports random access like RAC, but compression does not seem to be the
foremost concern. That specification mentions compression but does not give any
particular codec. A [separate
document](https://people.gnome.org/~markmc/qcow-image-format.html) suggests
that the codec is Zlib (with no other option). Like other RAC-related work,
QCOW does not support shared dictionaries.

The
[`examples/zran.c`](https://github.com/madler/zlib/blob/master/examples/zran.c)
program in the zlib-the-library source code repository (and e.g. the jzran Java
language port) builds an in-memory index, not a persistent (disk-backed) one,
and building the index involves first decompressing the whole file. It also
requires storing (in memory) the entire state of the decompressor, including 32
KiB of history, per seek point. For coarse grained seek points (e.g. once every
1 MiB), that overhead can be acceptable, but for fine grained seek points (e.g.
once every 16 KiB), that overhead is prohibitive.

The
[`examples/dictionaryRandomAccess.c`](https://github.com/lz4/lz4/blob/master/examples/dictionaryRandomAccess.c)
program in the lz4-the-library source code repository is much closer in spirit
to RAC, and unlike the zlib example, does not require saving decompressor
state: its chunks are independently compressed. It supports dictionaries, but
they have to be supplied out-of-band, separate from the compressed file. It is
also tied to the LZ4 compression format, hard-codes what RAC would call the
chunk DRange size to a fixed value, and the file format is endian-dependent.
This is a proof of concept (the implementation's maximum DFileSize is 1MiB),
not a production quality file format.

RAC differs from the [LevelDB
Table](https://github.com/google/leveldb/blob/master/doc/table_format.md)
format, even if the LevelDB Table string keys are re-purposed to encode both
numerical `DOffset` seek points and identifiers for shared compression
dictionaries. A LevelDB Table index is flat, unlike RAC's hierarchical index.
In both cases, a maliciously written index could contain multiple entries for
the same key, and different decoders could therefore produce different output
for the same source file - a potential security vulnerability. For LevelDB
Tables, verifying an index key's uniqueness requires scanning every key in the
file, which can be relatively expensive, and is therefore not done in practice.
In comparison, RAC's "Branch Node Validation" process (see below) ensures that
exactly one `Leaf Node` contains any given byte offset `di` in the decompressed
file, and the RAC index's hierarchical nature places a scalable upper bound on
the scanning cost of verifying that, based on that portion of the index tree
visited for any given request `[di .. dj)`.

[zfp](https://computing.llnl.gov/projects/floating-point-compression) primarily
concerns lossy compression of spatial floating point data.


## Academic Publications

Kerbiriou and Chikhi's ["Parallel decompression of gzip-compressed files and
random access to DNA sequences"](https://arxiv.org/pdf/1905.07224.pdf) does not
modify the Gzip / Deflate file format. Seek points are located in `CSpace`, not
`DSpace`. It uses a heuristic approach to determine block boundaries near an
arbitrary seek point, which is susceptible to false positive matches. The
primary use case is biometric data in the FASTQ file format, limited to ASCII.
Reconstruction can be lossy: "The method is near-exact at low compression
levels, but often a large fraction of sequences contains undetermined
characters at higher compression levels".

Kreft and Navarro's ["LZ77-like Compression with Fast Random
Access"](https://users.dcc.uchile.cl/~gnavarro/ps/dcc10.1.pdf) constrains the
more general Lempel-Ziv approach so that back-reference spans always end at
phrase boundaries. Combining with an efficient data structure for phrase
boundaries, a bitmap with rank and select operations, allows for backwards
reconstruction (starting from a phrase endpoint). However, in terms of
compression ratios, "Our least-space variants take [2.5–4.0 times the space of
p7zip](https://users.dcc.uchile.cl/~gnavarro/ps/tcs12.pdf), and 1.8–2.5 times
the space of lz77".

Fredriksson and Nikitin's ["Simple Random Access
Compression"](http://www.cs.uku.fi/~fredriks/pub/papers/fi09.pdf) provides
random access to short substrings (as short as a single symbol) of the
decompressed file, focusing on natural language source text (where a symbol
corresponds to e.g. an English language word). It is unclear how well their
technique performs on decompressing large substrings of binary data. For
example, while their reported compression ratios appear competitive with gzip
for an English text file, they are substantially worse for XML data.

Zhang and Bockelman's ["Exploring compression techniques for ROOT
IO"](https://arxiv.org/pdf/1704.06976.pdf) has a section literally titled
"Random Access Compression", which appears to apply to structured floating
point data (from the High Energy Physics domain), and the reported compression
ratios are dramatically worse (2.88x) with what they call RAC than without.
This document discusses lossless compression of arbitrary binary data, with a
small (e.g. 1.02x overhead).

Rodler's ["Compression with Fast Random
Access"](https://www.brics.dk/DS/01/9/BRICS-DS-01-9.pdf) sounds relevant based
on its title, but primarily concerns lossy compression of volumetric data.

Robert and Nadarajan's ["New Algorithms For Random Access Text
Compression"](https://www.researchgate.net/publication/4231766_New_Algorithms_For_Random_Access_Text_Compression)
cannot compress arbitrary data, only 7-bit ASCII. The largest document they
consider is also only 17 KiB in size.
