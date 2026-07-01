# Deflate

Deflate is a general purpose compression format, using Huffman codes and
Lempel-Ziv style length-distance back-references. It is specified in [RFC
1951](https://www.ietf.org/rfc/rfc1951.txt).

Gzip ([RFC 1952](https://www.ietf.org/rfc/rfc1952.txt)) and zlib ([RFC
1950](https://www.ietf.org/rfc/rfc1950.txt)) are two thin wrappers (with
similar purpose, but different wire formats) around the raw deflate encoding.
Gzip (the file format) is used by the `gzip` and `tar` command line tools and
by the HTTP network protocol. Zlib is used by the ELF executable and PNG image
file formats.

[Zip](https://support.pkware.com/display/PKZIP/APPNOTE) (also known as PKZIP)
is another wrapper, similar to combining gzip with tar, that can compress
multiple files into a single archive. Zip is widely used by the ECMA Office
Open XML format, the OASIS Open Document Format for Office Applications and the
Java JAR format.

Wrangling those formats that build on deflate (gzip, zip and zlib) is not
provided by this package. For zlib, look at the `std/zlib` package instead. The
other formats are TODO.

For example, look at `test/data/romeo.txt*`. First, the uncompressed text:

    $ hd test/data/romeo.txt
    00000000  52 6f 6d 65 6f 20 61 6e  64 20 4a 75 6c 69 65 74  |Romeo and Juliet|
    00000010  0a 45 78 63 65 72 70 74  20 66 72 6f 6d 20 41 63  |.Excerpt from Ac|
    etc
    00000390  74 0a 53 6f 20 73 74 75  6d 62 6c 65 73 74 20 6f  |t.So stumblest o|
    000003a0  6e 20 6d 79 20 63 6f 75  6e 73 65 6c 3f 0a        |n my counsel?.|
    000003ae

The raw deflate encoding:

    $ hd test/data/romeo.txt.deflate
    00000000  4d 53 c1 6e db 30 0c bd  f3 2b d8 53 2e 46 0e 3d  |MS.n.0...+.S.F.=|
    00000010  2e 87 20 ed 02 34 c5 ba  00 49 86 1e 86 1d 14 9b  |.. ..4...I......|
    etc
    00000200  7d 13 8f fd b9 a3 24 bb  68 f4 eb 30 7a 59 61 0d  |}.....$.h..0zYa.|
    00000210  7f 01                                             |..|
    00000212

The gzip format wraps a variable length header (in this case, 20 bytes) and 8
byte footer around the raw deflate data. The header contains the NUL-terminated
C string name of the original, uncompressed file, "romeo.txt", amongst other
data. The footer contains a 4 byte CRC32 checksum and a 4 byte length of the
uncompressed file (0x3ae = 942 bytes).

    $ hd test/data/romeo.txt.gz
    00000000  1f 8b 08 08 26 d8 5d 59  00 03 72 6f 6d 65 6f 2e  |....&.]Y..romeo.|
    00000010  74 78 74 00 4d 53 c1 6e  db 30 0c bd f3 2b d8 53  |txt.MS.n.0...+.S|
    etc
    00000210  de 5d 2c 67 7d 13 8f fd  b9 a3 24 bb 68 f4 eb 30  |.],g}.....$.h..0|
    00000220  7a 59 61 0d 7f 01 ef 07  e5 ab ae 03 00 00        |zYa...........|
    0000022e

The zlib format wraps a 2 byte header and 4 byte footer around the raw deflate
data. The footer contains a 4 byte Adler32 checksum. TODO: move this to
std/zlib/README.md.

    $ hd test/data/romeo.txt.zlib
    00000000  78 9c 4d 53 c1 6e db 30  0c bd f3 2b d8 53 2e 46  |x.MS.n.0...+.S.F|
    00000010  0e 3d 2e 87 20 ed 02 34  c5 ba 00 49 86 1e 86 1d  |.=.. ..4...I....|
    etc
    00000200  2c 67 7d 13 8f fd b9 a3  24 bb 68 f4 eb 30 7a 59  |,g}.....$.h..0zY|
    00000210  61 0d 7f 01 57 bb 3e de                           |a...W.>.|
    00000218


# Wire Format Worked Example

Consider `test/data/romeo.txt.deflate`. The relevant spec is RFC 1951.

    offset  xoffset ASCII   hex     binary
    000000  0x0000  M       0x4D    0b_0100_1101
    000001  0x0001  S       0x53    0b_0101_0011
    000002  0x0002  .       0xC1    0b_1100_0001
    000003  0x0003  n       0x6E    0b_0110_1110
    000004  0x0004  .       0xDB    0b_1101_1011
    000005  0x0005  0       0x30    0b_0011_0000
    000006  0x0006  .       0x0C    0b_0000_1100
    000007  0x0007  .       0xBD    0b_1011_1101
    000008  0x0008  .       0xF3    0b_1111_0011
    000009  0x0009  +       0x2B    0b_0010_1011
    000010  0x000A  .       0xD8    0b_1101_1000
    000011  0x000B  S       0x53    0b_0101_0011
    000012  0x000C  .       0x2E    0b_0010_1110
    000013  0x000D  F       0x46    0b_0100_0110
    000014  0x000E  .       0x0E    0b_0000_1110
    000015  0x000F  =       0x3D    0b_0011_1101
    000016  0x0010  .       0x2E    0b_0010_1110
    000017  0x0011  .       0x87    0b_1000_0111
    000018  0x0012          0x20    0b_0010_0000
    000019  0x0013  .       0xED    0b_1110_1101
    etc
    000522  0x020A  .       0xEB    0b_1110_1011
    000523  0x020B  0       0x30    0b_0011_0000
    000524  0x020C  z       0x7A    0b_0111_1010
    000525  0x020D  Y       0x59    0b_0101_1001
    000526  0x020E  a       0x61    0b_0110_0001
    000527  0x020F  .       0x0D    0b_0000_1101
    000528  0x0210  .       0x7F    0b_0111_1111
    000529  0x0211  .       0x01    0b_0000_0001

Starting at the top:

    offset  xoffset ASCII   hex     binary
    000000  0x0000  M       0x4D    0b_0100_1101

As per the RFC section 3.2.3. Details of block format:

  - BFINAL is the first (LSB) bit, 0b1. This block is the final block.
  - BTYPE is the next two bits in natural (MSB to LSB) order, 0b10, which means
    a block that's compressed with dynamic Huffman codes.

There are 3 block types: uncompressed, compressed with fixed Huffman codes and
compressed with dynamic Huffman codes. The first type is straightforward:
containing a uint16 length `N` (and its complement, for error detection) and
then `N` literal bytes. The second type is the same as the third type but with
built-in `lcode` and `dcode` tables (see below). The third type is the
interesting part of the deflate format, and its deconstruction continues below.


## Dynamic Huffman Blocks

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000000  0x0000  M       0x4D    0b_0100_1...
    000001  0x0001  S       0x53    0b_0101_0011
    000002  0x0002  .       0xC1    0b_1100_0001

As per the RFC section 3.2.7. Compression with dynamic Huffman codes, three
variables (5, 5 and 4 bits) are read in the same natural (MSB to LSB) order:

  - HLIT  = 0b01001 =  9
  - HDIST = 0b10011 = 19
  - HCLEN =  0b1010 = 10

Adding the implicit biases give:

    nlit  =  9 + 257 = 266
    ndist = 19 +   1 =  20
    nclen = 10 +   4 =  14


## Code Length Code Lengths

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000002  0x0002  .       0xC1    0b_1100_000.
    000003  0x0003  n       0x6E    0b_0110_1110
    000004  0x0004  .       0xDB    0b_1101_1011
    000005  0x0005  0       0x30    0b_0011_0000
    000006  0x0006  .       0x0C    0b_0000_1100
    000007  0x0007  .       0xBD    0b_1011_1101

There are 19 possible code length code lengths (this is not a typo), but the
wire format order is shuffled (in the idiosyncratic order: 16, 17, 18, 0, 8,
..., 15) so that later elements are more likely to be zero (and hence
compressible). There are `nclen` (in this case, 14) explicit code length code
lengths, 3 bits each.

    clcode_lengths[16] = 0b000 = 0
    clcode_lengths[17] = 0b100 = 4
    clcode_lengths[18] = 0b101 = 5
    clcode_lengths[ 0] = 0b011 = 3
    clcode_lengths[ 8] = 0b011 = 3
    clcode_lengths[ 7] = 0b011 = 3
    clcode_lengths[ 9] = 0b011 = 3
    clcode_lengths[ 6] = 0b011 = 3
    clcode_lengths[10] = 0b000 = 0
    clcode_lengths[ 5] = 0b011 = 3
    clcode_lengths[11] = 0b000 = 0
    clcode_lengths[ 4] = 0b011 = 3
    clcode_lengths[12] = 0b000 = 0
    clcode_lengths[ 3] = 0b101 = 5

The remaining (19 - `nclen`) = 5 entries are implicitly zero:

    clcode_lengths[13] = 0
    clcode_lengths[ 2] = 0
    clcode_lengths[14] = 0
    clcode_lengths[ 1] = 0
    clcode_lengths[15] = 0

Undoing the shuffle gives:

    clcode_lengths[ 0] = 3
    clcode_lengths[ 1] = 0
    clcode_lengths[ 2] = 0
    clcode_lengths[ 3] = 5
    clcode_lengths[ 4] = 3
    clcode_lengths[ 5] = 3
    clcode_lengths[ 6] = 3
    clcode_lengths[ 7] = 3
    clcode_lengths[ 8] = 3
    clcode_lengths[ 9] = 3
    clcode_lengths[10] = 0
    clcode_lengths[11] = 0
    clcode_lengths[12] = 0
    clcode_lengths[13] = 0
    clcode_lengths[14] = 0
    clcode_lengths[15] = 0
    clcode_lengths[16] = 0
    clcode_lengths[17] = 4
    clcode_lengths[18] = 5

For `clcode`s, there are 7 + 1 + 2 = 10 non-zero entries: 7 3-bit codes, 1
4-bit code and 2 5-bit codes. The canonical Huffman encoding's map from bits to
`clcode`s is:

    bits        clcode
    0b000        0
    0b001        4
    0b010        5
    0b011        6
    0b100        7
    0b101        8
    0b110        9
    0b1110      17
    0b11110      3
    0b11111     18

Call that Huffman table `H-CL`.


## Lcodes and Dcodes

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000007  0x0007  .       0xBD    0b_1011_1...
    000008  0x0008  .       0xF3    0b_1111_0011
    000009  0x0009  +       0x2B    0b_0010_1011
    000010  0x000A  .       0xD8    0b_1101_1000
    000011  0x000B  S       0x53    0b_0101_0011
    000012  0x000C  .       0x2E    0b_0010_1110
    000013  0x000D  F       0x46    0b_0100_0110

When decoding via a Huffman table, bits are read in LSB to MSB order,
right-to-left in this debugging output. Extra bits, if any, are then read in
the other, natural order (MSB to LSB). Decoding via `H-CL` gives:

    bits        clcode
    0b1110      17, 3 extra bits = 0b111 = 7
    0b001        4
    0b11111     18, 7 extra bits = 0b0001010 = 10
    0b001        4
    0b101        8
    0b1110      17, 3 extra bits = 0b010 = 2
    0b100        7
    0b1110      17, 3 extra bits = 0b001 = 1
    0b011        6
    0b000        0

Still in the RFC section 3.2.7., this means:

    lcode_lengths has 3 + 7 = 10 consecutive zeroes
    lcode_lengths[ 10] = 4
    lcode_lengths has 11 + 10 = 21 consecutive zeroes
    lcode_lengths[ 32] = 4
    lcode_lengths[ 33] = 8
    lcode_lengths has 3 + 2 = 5 consecutive zeroes
    lcode_lengths[ 39] = 7
    lcode_lengths has 3 + 1 = 4 consecutive zeroes
    lcode_lengths[ 44] = 6
    lcode_lengths[ 45] = 0

After decoding the first 10 code lengths, the bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000013  0x000D  F       0x46    0b_01.._....

Continuing to read all `nlit` (266) `lcode` lengths and `ndist` (20) `dcode`
lengths (zeroes are not shown here):

    lcode_lengths[ 10] = 4
    lcode_lengths[ 32] = 4
    lcode_lengths[ 33] = 8
    lcode_lengths[ 39] = 7
    lcode_lengths[ 44] = 6
    lcode_lengths[ 46] = 7
    lcode_lengths[ 50] = 8
    lcode_lengths[ 58] = 9
    lcode_lengths[ 59] = 7
    lcode_lengths[ 63] = 7
    etc
    lcode_lengths[264] = 9
    lcode_lengths[265] = 9

and

    dcode_lengths[ 5] = 6
    dcode_lengths[ 6] = 5
    dcode_lengths[ 8] = 4
    dcode_lengths[ 9] = 5
    dcode_lengths[10] = 3
    etc
    dcode_lengths[18] = 3
    dcode_lengths[19] = 6

The `H-CL` table is no longer used from here onwards.

For `lcode`s, there are 6 + 9 + 10 + 16 + 10 + 12 = 63 non-zero entries: 6
4-bit codes, 9 5-bit codes, 10 6-bit codes, 16 7-bit codes, 10 8-bit codes and
12 9-bit codes. The canonical Huffman encoding's map from bits to `lcode`
values is:

    bits        lcode
    0b0000       10
    0b0001       32
    0b0010      101
    0b0011      116
    0b0100      257
    0b0101      259
    0b01100      97
    0b01101     104
    0b01110     105
    0b01111     110
    0b10000     111
    0b10001     114
    0b10010     115
    0b10011     258
    0b10100     260
    0b101010     44
    0b101011     99
    0b101100    100
    0b101101    102
    0b101110    108
    0b101111    109
    0b110000    112
    0b110001    117
    0b110010    119
    0b110011    121
    0b1101000    39
    0b1101001    46
    0b1101010    59
    0b1101011    63
    0b1101100    65
    0b1101101    69
    0b1101110    73
    0b1101111    79
    0b1110000    82
    0b1110001    83
    0b1110010    84
    0b1110011    98
    0b1110100   103
    0b1110101   261
    0b1110110   262
    0b1110111   263
    0b11110000   33
    0b11110001   50
    0b11110010   66
    0b11110011   67
    0b11110100   72
    0b11110101   74
    0b11110110   77
    0b11110111   87
    0b11111000  107
    0b11111001  118
    0b111110100  58
    0b111110101  68
    0b111110110  76
    0b111110111  78
    0b111111000  85
    0b111111001  91
    0b111111010  93
    0b111111011 120
    0b111111100 122
    0b111111101 256
    0b111111110 264
    0b111111111 265

Call that Huffman table `H-L`.

For `dcode`s, there are 5 + 4 + 3 + 2 = 14 non-zero entries: 5 3-bit codes, 4
4-bit codes, 3 5-bit codes and 2 6-bit codes. The canonical Huffman encoding's
map from bits to `dcode` values is:

    bits        dcode
    0b000       10
    0b001       14
    0b010       16
    0b011       17
    0b100       18
    0b1010       8
    0b1011      12
    0b1100      13
    0b1101      15
    0b11100      6
    0b11101      9
    0b11110     11
    0b111110     5
    0b111111    19

Call that Huffman table `H-D`.


## Decoding Literals

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000052  0x0034  .       0xE7    0b_1..._....
    000053  0x0035  C       0x43    0b_0100_0011
    000054  0x0036  .       0xE8    0b_1110_1000
    000055  0x0037  )       0x29    0b_0010_1001
    000056  0x0038  .       0xA0    0b_1010_0000
    000057  0x0039  .       0xF1    0b_1111_0001

Decoding from that bit stream via `H-L` gives some literal bytes (where the
`lcode` value is < 256):

    bits        lcode
    0b1110000    82 (ASCII 'R')
    0b10000     111 (ASCII 'o')
    0b101111    109 (ASCII 'm')
    0b0010      101 (ASCII 'e')
    0b10000     111 (ASCII 'o')
    0b0001       32 (ASCII ' ')
    0b01100      97 (ASCII 'a')
    0b01111     110 (ASCII 'n')


## Decoding Back-References

This continues, up until we get to the "O Romeo, Romeo! wherefore art thou
Romeo?" line.

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000089  0x0059  .       0xC1    0b_11.._....
    000090  0x005A  .       0x1E    0b_0001_1110
    000091  0x005B  .       0x0F    0b_0000_1111
    000092  0x005C  .       0xF9    0b_1111_1001

Decoding from that bit stream via `H-L` gives:

    bits        lcode
    0b1101111    79 (ASCII 'O')
    0b0001       32 (ASCII ' ')
    0b1110000    82 (ASCII 'R')
    0b10011     258

This 258 is our first non-literal `lcode`, the start of a length-distance
back-reference. We first need to decode the length. The RFC section 3.2.5.
Compressed blocks (length and distance codes) says that an `lcode` of 258 means
that length=4 with no extra bits.

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000092  0x005C  .       0xF9    0b_111._....
    000093  0x005D  Y       0x59    0b_0101_1001

We next decode via `H-D` instead of `H-L`.

    bits        dcode
    0b11110     11

Again, from section 3.2.5., a `dcode` of 11 means a distance in the range [49,
64], 49 plus 4 extra bits. The next 4 bits are 0b0110=6, so the distance is 55.
We therefore copy length=4 bytes from distance=55 bytes ago: "omeo".

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000093  0x005D  Y       0x59    0b_01.._....
    000094  0x005E  U       0x55    0b_0101_0101
    000095  0x005F  >       0x3E    0b_0011_1110

We go back to decoding via `H-L`, which gives

    bits        lcode
    0b101010     44 (ASCII ',')
    0b10100     260

This is another non-literal. Section 3.2.5. says that an `lcode` of 260 means
length=6 with no extra bits.

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000095  0x005F  >       0x3E    0b_0011_111.

Decode with `H-D`.

    bits        dcode
    0b111110     5

Again, from section 3.2.5., a `dcode` of 5 means a distance in the range [7,
8], 7 plus 1 extra bit. The next 1 bits are 0b0=0, so the distance is 7. We
therefore copy length=6 bytes from distance=7 bytes ago: " Romeo".

The bit stream is now at:

    offset  xoffset ASCII   hex     binary
    000096  0x0060  .       0x0F    0b_0000_1111

We go back to decoding via `H-L`, which gives

    bits        lcode
    0b11110000   33 (ASCII '!')

And on we go.


## End of Block

The block finishes with the bit stream at:

    offset  xoffset ASCII   hex     binary
    000522  0x020A  .       0xEB    0b_1110_101.
    000523  0x020B  0       0x30    0b_0011_0000
    000524  0x020C  z       0x7A    0b_0111_1010
    000525  0x020D  Y       0x59    0b_0101_1001
    000526  0x020E  a       0x61    0b_0110_0001
    000527  0x020F  .       0x0D    0b_0000_1101
    000528  0x0210  .       0x7F    0b_0111_1111
    000529  0x0211  .       0x01    0b_0000_0001

The decoding is:

    bits        lcode
    0b101011     99 (ASCII 'c')
    0b10000     111 (ASCII 'o')
    0b110001    117 (ASCII 'u')
    0b01111     110 (ASCII 'n')
    0b0100      257 (length=3 + 0_extra_bits...)

    bits        dcode
    0b1101      15  (distance=193 + 6_extra_bits, 0b000010 in MSB to LSB order;
                     length=3, distance=195: copy "sel")

    bits        lcode
    0b1101011    63 (ASCII '?')
    0b0000       10 (ASCII '\n')
    0b111111101 256 (end of block)

In other words, the block ends with "counsel?\n".


# More Wire Format Examples

See `test/data/artificial/deflate-*.commentary.txt`
