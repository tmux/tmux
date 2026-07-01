# GIF

GIF (Graphics Interchange Format) is an image compression format for paletted
still and animated images. It is specified in [the GIF89a
specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt).


# Wire Format Worked Example

Consider `test/data/bricks-nodither.gif`.

    offset  xoffset ASCII   hex     binary
    000000  0x0000  G       0x47    0b_0100_0111
    000001  0x0001  I       0x49    0b_0100_1001
    000002  0x0002  F       0x46    0b_0100_0110
    000003  0x0003  8       0x38    0b_0011_1000
    000004  0x0004  9       0x39    0b_0011_1001
    000005  0x0005  a       0x61    0b_0110_0001
    000006  0x0006  .       0xA0    0b_1010_0000
    000007  0x0007  .       0x00    0b_0000_0000
    000008  0x0008  x       0x78    0b_0111_1000
    etc
    014230  0x3796  .       0xD3    0b_1101_0011
    014231  0x3797  .       0x02    0b_0000_0010
    014232  0x3798  .       0x06    0b_0000_0110
    014233  0x3799  .       0x04    0b_0000_0100
    014234  0x379A  .       0x00    0b_0000_0000
    014235  0x379B  ;       0x3B    0b_0011_1011

Starting at the top, a magic identifier, either "GIF87a" or "GIF89a". In
theory, some decoders can only handle the 1987 flavor. In practice, support for
the 1989 flavor is ubiquitous. Anyway, the magic ID is:

    offset  xoffset ASCII   hex     binary
    000000  0x0000  G       0x47    0b_0100_0111
    000001  0x0001  I       0x49    0b_0100_1001
    000002  0x0002  F       0x46    0b_0100_0110
    000003  0x0003  8       0x38    0b_0011_1000
    000004  0x0004  9       0x39    0b_0011_1001
    000005  0x0005  a       0x61    0b_0110_0001


## Logical Screen Descriptor

The Logical Screen Descriptor is at least 7 bytes long:

    offset  xoffset ASCII   hex     binary
    000006  0x0006  .       0xA0    0b_1010_0000
    000007  0x0007  .       0x00    0b_0000_0000
    000008  0x0008  x       0x78    0b_0111_1000
    000009  0x0009  .       0x00    0b_0000_0000
    000010  0x000A  .       0xF7    0b_1111_0111
    000011  0x000B  .       0x00    0b_0000_0000
    000012  0x000C  .       0x00    0b_0000_0000

The image size is 0x00A0 by 0x0078 pixels, or 160 by 120. The fifth byte is
flags, the sixth is the background color index, the seventh is ignored.

The high bit (0x80) and low three bits (0x07) of the flags indicate that we
have a GCT (Global Color Table) and that it contains 1<<(7+1), or 256,
elements. Global means that it applies to each frame in the possibly animated
image unless a per-frame Local Color Table overrides it, and Color Table means
a palette of up to 256 RGB (Red, Green, Blue) triples. That GCT follows
immediately:

    offset  xoffset ASCII   hex     binary
    000013  0x000D  .       0x01    0b_0000_0001
    000014  0x000E  .       0x03    0b_0000_0011
    000015  0x000F  .       0x04    0b_0000_0100
    000016  0x0010  .       0x00    0b_0000_0000
    000017  0x0011  .       0x0C    0b_0000_1100
    000018  0x0012  .       0x03    0b_0000_0011
    etc
    000250  0x00FA  .       0x01    0b_0000_0001
    000251  0x00FB  $       0x24    0b_0010_0100
    000252  0x00FC  c       0x63    0b_0110_0011
    etc
    000673  0x02A1  .       0x0F    0b_0000_1111
    000674  0x02A2  .       0xA7    0b_1010_0111
    000675  0x02A3  .       0xF6    0b_1111_0110
    etc
    000778  0x030A  .       0x9F    0b_1001_1111
    000779  0x030B  .       0xB8    0b_1011_1000
    000780  0x030C  .       0xCD    0b_1100_1101

Thus, color 0 in the palette is the RGB triple {0x01, 0x03, 0x04}, color 1 is
{0x00, 0x0C, 0x03}, etc, color 79 is {0x01, 0x24, 0x63}, etc, color 220 is
{0x0F, 0xA7, 0xF6}, etc, color 255 is {0x9F, 0xB8, 0xCD}.


## Graphic Control Extension

For this particular GIF, next comes an Extension Introducer byte (0x21) for a
GCE (Graphic Control Extension) (0xF9), followed by one or more data blocks:

    offset  xoffset ASCII   hex     binary
    000781  0x030D  !       0x21    0b_0010_0001
    000782  0x030E  .       0xF9    0b_1111_1001
    000783  0x030F  .       0x04    0b_0000_0100
    000784  0x0310  .       0x00    0b_0000_0000
    000785  0x0311  .       0x00    0b_0000_0000
    000786  0x0312  .       0x00    0b_0000_0000
    000787  0x0313  .       0x00    0b_0000_0000
    000788  0x0314  .       0x00    0b_0000_0000

The first block has 0x04 payload bytes. After those four bytes, the second
block has 0x00 payload bytes, which marks the end of the data blocks. The
payload bytes are flags, animation timing and transparency information that,
for this simple, opaque, single-frame image, happen to be all zero.


## Image Descriptor

Image Descriptors form the bulk of a GIF image. A still image will have one
Image Descriptor. An animated image will have many. Image Descriptors begin
with an 0x2C byte, followed by at least nine more bytes for the {left, top,
width, height} of this frame relative to the overall (potentially animated)
image, plus a flags byte:

    offset  xoffset ASCII   hex     binary
    000789  0x0315  ,       0x2C    0b_0010_1100
    000790  0x0316  .       0x00    0b_0000_0000
    000791  0x0317  .       0x00    0b_0000_0000
    000792  0x0318  .       0x00    0b_0000_0000
    000793  0x0319  .       0x00    0b_0000_0000
    000794  0x031A  .       0xA0    0b_1010_0000
    000795  0x031B  .       0x00    0b_0000_0000
    000796  0x031C  x       0x78    0b_0111_1000
    000797  0x031D  .       0x00    0b_0000_0000
    000798  0x031E  .       0x00    0b_0000_0000

The frame's origin is {0x0000, 0x0000} and its extent is {0x00A0, 0x0078}. The
flags byte indicates a non-interlaced frame and that no LCT (Local Color Table)
overrides the GCT. After the (lack of) LCT comes the LZW (Lempel Ziv Welch)
compression's `log2(literal_width)`, discussed further below.

    offset  xoffset ASCII   hex     binary
    000799  0x031F  .       0x08    0b_0000_1000


## Pixel Data

Pixel data from the bulk of an Image Descriptor. Just like the GCE format, the
payload is framed as a sequence of blocks, each block starting with a single
byte count of the remaining bytes in the block, and the final block containing
only a 0x00 byte.

    offset  xoffset ASCII   hex     binary
    000800  0x0320  .       0xFE    0b_1111_1110
    000801  0x0321  .       0x00    0b_0000_0000
    000802  0x0322  .       0xB9    0b_1011_1001
    000803  0x0323  .       0x09    0b_0000_1001
    000804  0x0324  .       0x14    0b_0001_0100
    000805  0x0325  .       0x98    0b_1001_1000
    etc
    001053  0x041D  6       0x36    0b_0011_0110
    001054  0x041E  .       0x1E    0b_0001_1110
    001055  0x041F  .       0xFE    0b_1111_1110
    001056  0x0420  J       0x4A    0b_0100_1010
    001057  0x0421  .       0xA4    0b_1010_0100
    etc
    001308  0x051C  .       0x1F    0b_0001_1111
    001309  0x051D  .       0x81    0b_1000_0001
    001310  0x051E  .       0xFE    0b_1111_1110
    001311  0x051F  5       0x35    0b_0011_0101
    001312  0x0520  ]       0x5D    0b_0101_1101
    etc
    etc
    etc
    013803  0x35EB  n       0x6E    0b_0110_1110
    013804  0x35EC  .       0xD7    0b_1101_0111
    013805  0x35ED  .       0xFE    0b_1111_1110
    013806  0x35EE  .       0x17    0b_0001_0111
    013807  0x35EF  .       0xB4    0b_1011_0100
    etc
    014058  0x36EA  .       0xDB    0b_1101_1011
    014059  0x36EB  g       0x67    0b_0110_0111
    014060  0x36EC  .       0xAD    0b_1010_1101
    014061  0x36ED  .       0x81    0b_1000_0001
    014062  0x36EE  .       0xD6    0b_1101_0110
    etc
    014232  0x3798  .       0x06    0b_0000_0110
    014233  0x3799  .       0x04    0b_0000_0100
    014234  0x379A  .       0x00    0b_0000_0000

The specification allows for the block count to be 0xFF, but for whatever
reason, the ImageMagick encoder and its command-line `convert` tool generated
slightly shorter blocks, with lengths 0xFE, 0xFE, 0xFE, ..., 0xFE, 0xAD, 0x00.

The payload wrapped by these block counts are LZW compressed, discussed below.


## Trailer

The final byte is 0x3B.

    offset  xoffset ASCII   hex     binary
    014235  0x379B  ;       0x3B    0b_0011_1011


# LZW (Lempel Ziv Welch) Compression

See `std/lzw/README.md` for a description of LZW: a general purpose compression
algorithm. Below is a detailed decoding of the LZW data from the Wire Format
Worked Example, deconstructed above.

It is not an official format, but the
`test/data/bricks-nodither.indexes.giflzw` file contains the extracted "Pixel
Data" payload from `test/data/bricks-nodither.gif`, preceded by the one byte
`log2(literal_width)`. Deriving a separate file, as a separate, contiguous
stream, makes it easier to discuss the LZW compression format.

    offset  xoffset ASCII   hex     binary
    000000  0x0000  .       0x08    0b_0000_1000
    000001  0x0001  .       0x00    0b_0000_0000
    000002  0x0002  .       0xB9    0b_1011_1001
    000003  0x0003  .       0x09    0b_0000_1001
    000004  0x0004  .       0x14    0b_0001_0100
    000005  0x0005  .       0x98    0b_1001_1000
    000006  0x0006  .       0xAD    0b_1010_1101
    000007  0x0007  ^       0x5E    0b_0101_1110
    000008  0x0008  >       0x3E    0b_0011_1110
    000009  0x0009  {       0x7B    0b_0111_1011
    000010  0x000A  .       0xDF    0b_1101_1111
    000011  0x000B  .       0xB8    0b_1011_1000
    000012  0x000C  }       0x7D    0b_0111_1101
    000013  0x000D  .       0xC9    0b_1100_1001
    000014  0x000E  .       0xA1    0b_1010_0001
    000015  0x000F  .       0xA3    0b_1010_0011
    000016  0x0010  a       0x61    0b_0110_0001
    000017  0x0011  .       0xC3    0b_1100_0011
    000018  0x0012  0       0x30    0b_0011_0000
    etc
    000253  0x00FD  6       0x36    0b_0011_0110
    000254  0x00FE  .       0x1E    0b_0001_1110
    000255  0x00FF  J       0x4A    0b_0100_1010
    000256  0x0100  .       0xA4    0b_1010_0100
    etc
    000507  0x01FB  .       0x1F    0b_0001_1111
    000508  0x01FC  .       0x81    0b_1000_0001
    000509  0x01FD  5       0x35    0b_0011_0101
    000510  0x01FE  ]       0x5D    0b_0101_1101
    etc
    etc
    etc
    012953  0x3299  n       0x6E    0b_0110_1110
    012954  0x329A  .       0xD7    0b_1101_0111
    012955  0x329B  .       0x17    0b_0001_0111
    012956  0x329C  .       0xB4    0b_1011_0100
    etc
    013207  0x3397  .       0xDB    0b_1101_1011
    013208  0x3398  g       0x67    0b_0110_0111
    013209  0x3399  .       0x81    0b_1000_0001
    013210  0x339A  .       0xD6    0b_1101_0110
    etc
    013380  0x3444  .       0x06    0b_0000_0110
    013381  0x3445  .       0x04    0b_0000_0100


## Codes

LZW consists of a sequence of code values, in the range `[0, max]` inclusive.
Each code decodes to either a literal value (one byte), a back-reference
(multiple bytes), a 'clear code' (discussed below), or indicates the end of the
data stream.

The first `1<<l2lw` codes are literal codes, where `l2lw` is the
`log2(literal_width)` mentioned above. For GIF's flavor of LZW, valid `l2lw`
values are between 2 and 8 inclusive.

For example, if `l2lw == 8`, then the first 256 codes are literal codes: the 0
code decodes one 0x00 byte, the 1 code decodes one 0x01 byte, etc. The next
code (256) is the clear code and the next code after that (257) is the end
code. Any code higher than that denotes a back-reference, but any code above
the current `max` value is invalid.

`max`'s initial value is the same as the end code, and it increases by 1 with
each code consumed, up until `max == 4095`. A clear code resets `max` to its
initial value, and clears the meaning of higher back-reference codes.

Codes take up a variable number of bits in the input stream, the `width`,
depending on `max`. For example, if `max` is in the range [256, 511] then the
next code takes 9 bits, if `max` is in the range [512, 1023] then `width ==
10`, and so on. For GIF, when a code spans multiple bytes, codes are formed
Least Significant Bits first. For the
`test/data/bricks-nodither.indexes.giflzw` example, `l2lw == 8` and so `width`
starts at 9 bits. Printing the bits on byte boundaries give:

    offset  xoffset ASCII   hex     binary
    000001  0x0001  .       0x00    0b_0000_0000
    000002  0x0002  .       0xB9    0b_1011_1001
    000003  0x0003  .       0x09    0b_0000_1001
    000004  0x0004  .       0x14    0b_0001_0100
    000005  0x0005  .       0x98    0b_1001_1000
    000006  0x0006  .       0xAD    0b_1010_1101
    000007  0x0007  ^       0x5E    0b_0101_1110
    000008  0x0008  >       0x3E    0b_0011_1110
    000009  0x0009  {       0x7B    0b_0111_1011
    000010  0x000A  .       0xDF    0b_1101_1111
    000011  0x000B  .       0xB8    0b_1011_1000
    000012  0x000C  }       0x7D    0b_0111_1101
    000013  0x000D  .       0xC9    0b_1100_1001
    000014  0x000E  .       0xA1    0b_1010_0001
    000015  0x000F  .       0xA3    0b_1010_0011
    000016  0x0010  a       0x61    0b_0110_0001
    000017  0x0011  .       0xC3    0b_1100_0011
    000018  0x0012  0       0x30    0b_0011_0000
    etc
    000147  0x0093  >       0x3E    0b_0011_1110
    000148  0x0094  .       0xC4    0b_1100_0100
    etc
    000286  0x011E  _       0x5F    0b_0101_1111
    000287  0x011F  .       0xEA    0b_1110_1010
    000288  0x0120  .       0x9C    0b_1001_1100
    000289  0x0121  .       0xFC    0b_1111_1100
    000290  0x0122  .       0xD4    0b_1101_0100
    000291  0x0123  .       0xA4    0b_1010_0100
    etc
    005407  0x151F  .       0xBB    0b_1011_1011
    005408  0x1520  .       0xFB    0b_1111_1011
    005409  0x1521  .       0x00    0b_0000_0000
    005410  0x1522  .       0xE1    0b_1110_0001
    005411  0x1523  .       0xA1    0b_1010_0001
    005412  0x1524  .       0xC3    0b_1100_0011
    005413  0x1525  @       0x40    0b_0100_0000
    etc
    013378  0x3442  .       0xD3    0b_1101_0011
    013379  0x3443  .       0x02    0b_0000_0010
    013380  0x3444  .       0x06    0b_0000_0110
    013381  0x3445  .       0x04    0b_0000_0100


and on code boundaries give:

    binary                              width   code
    0b_...._...._...._...1_0000_0000     9      0x0100
    0b_...._...._...._..01_1011_100.     9      0x00DC
    0b_...._...._...._.100_0000_10..     9      0x0102
    0b_...._...._...._1000_0001_0...     9      0x0102
    0b_...._...._...0_1101_1001_....     9      0x00D9
    0b_...._...._..01_1110_101._....     9      0x00F5
    0b_...._...._.011_1110_01.._....     9      0x00F9
    0b_...._...._0111_1011_0..._....     9      0x00F6
    0b_...._...._...._...0_1101_1111     9      0x00DF
    0b_...._...._...._..01_1011_100.     9      0x00DC
    0b_...._...._...._.001_0111_11..     9      0x005F
    0b_...._...._...._0001_1100_1...     9      0x0039
    0b_...._...._...0_0011_1010_....     9      0x003A
    0b_...._...._..10_0001_101._....     9      0x010D
    0b_...._...._.100_0011_01.._....     9      0x010D
    0b_...._...._0011_0000_1..._....     9      0x0061
    etc
    0b_...._...._...._.100_0011_11..     9      0x010F
    etc
    0b_...._...._.110_1010_01.._....     9      0x01A9
    0b_...._...._1001_1100_1..._....     9      0x0139
    0b_...._...._...._..00_1111_1100    10      0x00FC  (implicit width increase)
    0b_...._...._...._0100_1101_01..    10      0x0135
    etc
    0b_...._...._1111_1011_1011_....    12      0x0FBB
    0b_...._...._...._0001_0000_0000    12      0x0100  (code == 0x0100 means clear)
    0b_...._...._...0_0001_1110_....     9      0x001E  (explicit width reset)
    0b_...._...._..00_0011_101._....     9      0x001D
    0b_...._...._.100_0000_11.._....     9      0x0103
    etc
    0b_...._..10_0000_0010_11.._....    12      0x080B
    0b_...._...._..00_0100_0000_01..    12      0x0101

The implicit `width` increase is because `max` ticked over from 0x01FF to
0x200. Processing these codes give:

    code      meaning   max     output
    0x0100    clear     0x0101  .
    0x00DC    literal   0x0101  0xDC
    0x0102    back-ref  0x0102  0xDC 0xDC
    0x0102    back-ref  0x0103  0xDC 0xDC
    0x00D9    literal   0x0104  0xD9
    0x00F5    literal   0x0105  0xF5
    0x00F9    literal   0x0106  0xF9
    0x00F6    literal   0x0107  0xF6
    0x00DF    literal   0x0108  0xDF
    0x00DC    literal   0x0109  0xDC
    0x005F    literal   0x010A  0x5F
    0x0039    literal   0x010B  0x39
    0x003A    literal   0x010C  0x3A
    0x010D    back-ref  0x010D  0x3A 0x3A
    0x010D    back-ref  0x010E  0x3A 0x3A
    0x0061    literal   0x010F  0x61
    etc
    0x010F    back-ref  0x0182  0x3A 0x3A 0x61
    etc
    0x01A9    back-ref  0x01FE  0xFB 0xFB 0xFB 0xFB
    0x0139    back-ref  0x01FF  0xFB 0xFC
    0x00FC    literal   0x0200  0xFC
    0x0135    back-ref  0x0201  0xFA 0xFA
    etc
    0x0FBB    back-ref  0x0FFF  0xC1 0xC1 0xC1 0xC1 0xC1 0xC1 0xDF
    0x0100    clear     0x0FFF  .
    0x001E    literal   0x0101  0x1E
    0x001D    literal   0x0102  0x1D
    0x0103    back-ref  0x0103  0x1D 0x1D
    etc
    0x080B    back-ref  0x0896  0x4F 0x4F 0x4F 0x4F 0x4F
    0x0101    end       0x0897  .

The `max` column here is the `max` code allowed when processing that row's
code. The initial clear code is redundant (as the initial value of `max` is
already 0x0101 here), but some encoders produce them because the specification
says that "encoders should output a Clear code as the first code of each image
data stream".

Other than clear and end codes, each code produces some output: a variable
length prefix and a one byte suffix. For literal codes, the prefix is empty and
the suffix is the literal.

When `max` is greater than the end code (e.g. greater than 0x0101), the row
defines a new back-reference, and that definition persists up until the next
clear code. For example, the third, fourth, fifth, etc. rows define what the
0x0102, 0x0103, 0x0104, etc. codes output. That code's output (prefix + suffix)
is defined by the prefix being the previous row's output and the suffix being
the first byte of the current row's output.

For example, the 15th and 16th rows are:

    code      meaning   max     output
    0x010D    back-ref  0x010E  0x3A 0x3A
    0x0061    literal   0x010F  0x61

which defines the 0x010F code to output "0x3A 0x3A 0x61". Later on, an 0x010F
code is encountered, and its output is exactly that.

A row's code can be the code that that row itself defines, in which case the
prefix is the previous row and the suffix is the first byte of the previous
row. Consider the opening few codes:

    code      meaning   max     output
    0x0100    clear     0x0101  .
    0x00DC    literal   0x0101  0xDC
    0x0102    back-ref  0x0102  0xDC 0xDC
    0x0102    back-ref  0x0103  0xDC 0xDC
    0x00D9    literal   0x0104  0xD9

The third row's code is 0x0102, which that row also defines. Its output is
therefore the concatenation of "0xDC" and the first byte of "0xDC", both of
those strings being the previous row's output. The fourth row executes the same
0x0102 code (and outputs "0xDC 0xDC" again) and defines the 0x0103 code. The
next code is a literal 0xD9, and hence the decoded output starts with five 0xDC
bytes and then an 0xD9.


## Actual Pixel Indexes

It is also not an official format, but for reference, the
`test/data/bricks-nodither.indexes` contains the decoded
`test/data/bricks-nodither.indexes.giflzw` data:

    offset  xoffset ASCII   hex     binary
    000000  0x0000  .       0xDC    0b_1101_1100
    000001  0x0001  .       0xDC    0b_1101_1100
    000002  0x0002  .       0xDC    0b_1101_1100
    000003  0x0003  .       0xDC    0b_1101_1100
    000004  0x0004  .       0xDC    0b_1101_1100
    000005  0x0005  .       0xD9    0b_1101_1001
    000006  0x0006  .       0xF5    0b_1111_0101
    000007  0x0007  .       0xF9    0b_1111_1001
    000008  0x0008  .       0xF6    0b_1111_0110
    000009  0x0009  .       0xDF    0b_1101_1111
    000010  0x000A  .       0xDC    0b_1101_1100
    000011  0x000B  _       0x5F    0b_0101_1111
    000012  0x000C  9       0x39    0b_0011_1001
    000013  0x000D  :       0x3A    0b_0011_1010
    000014  0x000E  :       0x3A    0b_0011_1010
    000015  0x000F  :       0x3A    0b_0011_1010
    000016  0x0010  :       0x3A    0b_0011_1010
    000017  0x0011  :       0x3A    0b_0011_1010
    000018  0x0012  a       0x61    0b_0110_0001
    etc
    019195  0x4AFB  O       0x4F    0b_0100_1111
    019196  0x4AFC  O       0x4F    0b_0100_1111
    019197  0x4AFD  O       0x4F    0b_0100_1111
    019198  0x4AFE  O       0x4F    0b_0100_1111
    019199  0x4AFF  O       0x4F    0b_0100_1111

We have 19200 = 160 × 120 pixels. The top row starts with five pixels with RGB
values indexed by 0xDC (or 220 in decimal, and recall that the GCT maps color
220 to {0x0F, 0xA7, 0xF6}), the bottom row ends with index 0x4F.

Indeed, opening `test/data/bricks-nodither.gif` in an image editor should
verify that the top left pixel's RGB is {0x0F, 0xA7, 0xF6}, and likewise the
bottom right pixel's RGB is {0x01, 0x24, 0x63}.


# More Wire Format Examples

See `test/data/artificial/gif-*.commentary.txt`
