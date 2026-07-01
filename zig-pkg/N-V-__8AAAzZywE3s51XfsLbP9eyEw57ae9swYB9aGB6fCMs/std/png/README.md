# PNG

PNG ([Portable Network Graphics](https://www.w3.org/TR/PNG/)) is a lossless
image compression format for still images. APNG ([Animated
PNG](https://wiki.mozilla.org/APNG_Specification)) is an unofficial extension
for animated images.


## File Structure

A PNG file consists of an 8-byte magic identifier and then a series of chunks.
Each chunk is:

  - a 4-byte uint32 payload length `N`.
  - a 4-byte chunk type (e.g. `gAMA` for gamma correction metadata).
  - an `N`-byte payload.
  - a 4-byte uint32 [CRC-32](../crc32/README.md) checksum of the previous `(N +
	4)` bytes, including the chunk type but excluding the payload length.

All multi-byte numbers (including 16-bit depth RGBA colors) are stored
big-endian. The first chunk has an `IHDR` type (whose 13-byte payload contains
the uint32 width and height). The last chunk has an `IEND` type (and a 0-byte
payload).

For example, this 36 × 24 (0x24 × 0x1C), 8 bits-per-pixel PNG file's chunk
sequence starts: `IHDR`, `gAMA`, `cHRM`, `bKGD`, `IDAT`...

    $ hd test/data/hippopotamus.interlaced.png | head
    00000000  89 50 4e 47 0d 0a 1a 0a  00 00 00 0d 49 48 44 52  |.PNG........IHDR|
    00000010  00 00 00 24 00 00 00 1c  08 02 00 00 01 f1 4c ba  |...$..........L.|
    00000020  99 00 00 00 04 67 41 4d  41 00 00 b1 8f 0b fc 61  |.....gAMA......a|
    00000030  05 00 00 00 20 63 48 52  4d 00 00 7a 26 00 00 80  |.... cHRM..z&...|
    00000040  84 00 00 fa 00 00 00 80  e8 00 00 75 30 00 00 ea  |...........u0...|
    00000050  60 00 00 3a 98 00 00 17  70 9c ba 51 3c 00 00 00  |`..:....p..Q<...|
    00000060  06 62 4b 47 44 00 ff 00  ff 00 ff a0 bd a7 93 00  |.bKGD...........|
    00000070  00 09 7a 49 44 41 54 48  c7 65 56 69 90 54 d5 19  |..zIDATH.eVi.T..|
    00000080  bd eb 5b fb 75 bf de a6  a7 e9 9e b5 67 06 74 80  |..[.u.......g.t.|
    00000090  08 a8 50 a8 10 b4 2c 2c  b5 a2 21 d1 44 8d 26 24  |..P...,,..!.D.&$|

The upper / lower case bit of a chunk type's first of four letters denote
critical / ancillary chunks. There are four critical chunk types, which must
occur in this order (although a `PLTE` chunk is optional and there can be more
than one `IDAT` chunk):

  - `IHDR` contains what Wuffs calls the image config (width, height and pixel
	format (e.g. 4 bits per pixel gray, RGB, RGBA_NONPREMUL_4X16BE)) and the
	interlacing bit.
  - `PLTE` contains the color palette.
  - `IDAT` contains the [zlib](../zlib/README.md)-compressed filtered pixel
	data. If there are multiple `IDAT` chunks, their payloads are treated as
	concatenated.
  - `IEND` contains an empty payload.

The PNG specification allows decoders to ignore all ancillary chunks, but when
converting a PNG file to pixels on a screen, high quality decoders should still
process transparency related (`tRNS`) and color space related (`cHRM`, `gAMA`,
`iCCP`, `sBIT` and `sRGB`) chunks.


## Filtering

Encoding a PNG involves subtracting (using modular uint8 arithmetic) a
predicted value from each pixel (before zlib compressing the residuals). The
predicted value of pixel `x` depends on the pixel to the left (`a`), the pixel
above (`b`) and the pixel above-left (`c`). Decoding a PNG involves reversing
that process (after zlib decompression).

Each row of pixels uses one of five prediction algorithms, also called filters:
0=None, 1=Sub, 2=Up, 3=Average and 4=Paeth.

  - Filter 0: `Prediction = 0`.
  - Filter 1: `Prediction = a`.
  - Filter 2: `Prediction = b`.
  - Filter 3: `Prediction = floor((a + b) / 2)`.
  - Filter 4: `Prediction = paeth(a, b, c)`.

The Paeth prediction function is around 10 lines of code and is described in
[the PNG spec](https://www.w3.org/TR/PNG/#9Filter-type-4-Paeth).

Prediction conceptually involves pixels but in practice works on bytes. 8-bit
depth RGB images have 3 bytes per pixel, so the 'pixel to the left' means 3
bytes prior in the zlib-decompressed stream. Low depth images (e.g. bi-level
images have 0.125 bytes per pixel) use a filter distance of 1 byte.

The bytes of the 'pixel to the left' of the first column is implicitly zero.
Likewise for the 'pixel above' the first row.


### Per-Row Filter Byte

Just after zlib decompression when decoding (or just before zlib compression
when encoding), the image is represented as `((ceil(width * bpp) + 1) *
height)` bytes, where `bpp` is the possibly-fractional number of bytes per
pixel (e.g. 3 for RGB, 4 for RGBA). Each of the `height` rows of pixel data
start with an additional byte that denotes the per-row filter.

For example, a 4 pixel wide RGB (3 `bpp`) image  would have 13 bytes per row.
Undoing the filter on this row (whose initial `0x01` byte denotes the Sub
filter) of residuals:

    0x01, 0x57, 0x68, 0x61, 0x74, 0x73, 0x49, 0x6E, 0x41, 0x4E, 0x61, 0x6D, 0x65

reconstitutes the original RGB pixel data for that row:

    ____  0x57, 0x68, 0x61, 0xCB, 0xDB, 0xAA, 0x39, 0x1C, 0xF8, 0x9A, 0x89, 0x5D


## Interlacing

TODO.


# Further Reading

See the [PNG Wikipedia
article](https://en.wikipedia.org/wiki/Portable_Network_Graphics).
