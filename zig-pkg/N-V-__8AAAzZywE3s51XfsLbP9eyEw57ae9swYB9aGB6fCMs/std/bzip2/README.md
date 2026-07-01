# Bzip2

Bzip2 is a general purpose compression format, using Huffman codes, Run Length
Encoding, the Move To Front Transform and the Burrows Wheeler Transform. There
is no official specification, but Joe Tsai has written [comprehensive
documentation](https://github.com/dsnet/compress/raw/master/doc/bzip2-format.pdf)
of the wire format.

It is a block based format, which means that when decoding each block, the
whole compressed block has to be read before a single byte of the uncompressed
block can be written.

Like zlib (but unlike gzip), bzip2 just holds compressed bytes. It does not
hold additional metadata like the original file's name or modification time.


# Wire Format Worked Example

Let's compress "abraca". Bzip2 compresses poorly on such a short input file,
converting 6 uncompressed bytes to 0x2b = 43 compressed bytes. Nonetheless, the
"abraca" uncompressed text matches the example from the 1994 [BWT technical
report](http://www.hpl.hp.com/techreports/Compaq-DEC/SRC-RR-124.pdf) (by M.
Burrows and D. J. Wheeler) that introduced their eponymous Burrows Wheeler
Transform.

    $ hd abraca.txt
    00000000  61 62 72 61 63 61                                 |abraca|
    00000006
    $ hd abraca.txt.bz2
    00000000  42 5a 68 39 31 41 59 26  53 59 76 a7 09 95 00 00  |BZh91AY&SYv.....|
    00000010  00 81 80 38 00 10 00 20  00 21 9a 68 33 4d 30 91  |...8... .!.h3M0.|
    00000020  e2 ee 48 a7 0a 12 0e d4  e1 32 a0                 |..H......2.|
    0000002b

Here are the bits of those 43 bytes with various bitslices labeled from (a) to
(s). The bzip2 file format works in bits instead of bytes. It reads bits from
MSB (Most Significant Bit) to LSB (Least Significant Bit) order and does not
pad to byte boundaries (except at the end of the file).

    offset  xoffset ASCII   hex     binary          bitslice
    000000  0x0000  B       0x42    0b_0100_0010    aaaa_aaaa
    000001  0x0001  Z       0x5A    0b_0101_1010    aaaa_aaaa
    000002  0x0002  h       0x68    0b_0110_1000    bbbb_bbbb
    000003  0x0003  9       0x39    0b_0011_1001    cccc_cccc
    000004  0x0004  1       0x31    0b_0011_0001    dddd_dddd
    000005  0x0005  A       0x41    0b_0100_0001    dddd_dddd
    000006  0x0006  Y       0x59    0b_0101_1001    dddd_dddd
    000007  0x0007  &       0x26    0b_0010_0110    dddd_dddd
    000008  0x0008  S       0x53    0b_0101_0011    dddd_dddd
    000009  0x0009  Y       0x59    0b_0101_1001    dddd_dddd
    000010  0x000A  v       0x76    0b_0111_0110    eeee_eeee
    000011  0x000B  .       0xA7    0b_1010_0111    eeee_eeee
    000012  0x000C  .       0x09    0b_0000_1001    eeee_eeee
    000013  0x000D  .       0x95    0b_1001_0101    eeee_eeee
    000014  0x000E  .       0x00    0b_0000_0000    fggg_gggg
    000015  0x000F  .       0x00    0b_0000_0000    gggg_gggg
    000016  0x0010  .       0x00    0b_0000_0000    gggg_gggg
    000017  0x0011  .       0x81    0b_1000_0001    ghhh_hhhh    popcount(h) = 2
    000018  0x0012  .       0x80    0b_1000_0000    hhhh_hhhh
    000019  0x0013  8       0x38    0b_0011_1000    hiii_iiii    popcount(i) = 3
    000020  0x0014  .       0x00    0b_0000_0000    iiii_iiii
    000021  0x0015  .       0x10    0b_0001_0000    ijjj_jjjj    popcount(j) = 1
    000022  0x0016  .       0x00    0b_0000_0000    jjjj_jjjj
    000023  0x0017          0x20    0b_0010_0000    jkkk_llll
    000024  0x0018  .       0x00    0b_0000_0000    llll_llll
    000025  0x0019  !       0x21    0b_0010_0001    lllm_nnnn
    000026  0x001A  .       0x9A    0b_1001_1010    nnnn_nnnn
    000027  0x001B  h       0x68    0b_0110_1000    nnnn_nnno
    000028  0x001C  3       0x33    0b_0011_0011    oooo_oooo
    000029  0x001D  M       0x4D    0b_0100_1101    oooo_oooo
    000030  0x001E  0       0x30    0b_0011_0000    oopp_pppp
    000031  0x001F  .       0x91    0b_1001_0001    pppp_pppp
    000032  0x0020  .       0xE2    0b_1110_0010    pppq_qqqq
    000033  0x0021  .       0xEE    0b_1110_1110    qqqq_qqqq
    000034  0x0022  H       0x48    0b_0100_1000    qqqq_qqqq
    000035  0x0023  .       0xA7    0b_1010_0111    qqqq_qqqq
    000036  0x0024  .       0x0A    0b_0000_1010    qqqq_qqqq
    000037  0x0025  .       0x12    0b_0001_0010    qqqq_qqqq
    000038  0x0026  .       0x0E    0b_0000_1110    qqqr_rrrr
    000039  0x0027  .       0xD4    0b_1101_0100    rrrr_rrrr
    000040  0x0028  .       0xE1    0b_1110_0001    rrrr_rrrr
    000041  0x0029  2       0x32    0b_0011_0010    rrrr_rrrr
    000042  0x002A  .       0xA0    0b_1010_0000    rrrs_ssss

(a) A 16-bit magic number "BZ" meaning the start of the bzip2 file.

(b) The 'h' byte means Huffman compression. The original bzip (as in, without
the "2" in "bzip2") used a '0' byte here to indicate arithmetic codes instead
of Huffman codes, but arithmetic/bzip was not widely used because of patent
concerns. This package (and worked example) only considers Huffman/bzip2.

(c) The '9' byte means the uncompressed block size ranges up to 900000
(inclusive).

(d) A 48-bit magic number 0x314159265359 (a homage to π) means the start of a
block.

(e) A 32-bit checksum. Refer to section 2.2.3.1 "BlockHeader" of [Tsai's
document](https://github.com/dsnet/compress/raw/master/doc/bzip2-format.pdf)
which notes, "It is the same [CRC-32] checksum used in GZip (RFC 1952, section
8), but is slightly different due to the bit-packing differences noted in
section 2.1".

(f) A "block randomized" bit, deprecated and not used here.

(g) The 24-bit index of the original string in the sorted list of BWT
rotations. The [BWT technical
report](http://www.hpl.hp.com/techreports/Compaq-DEC/SRC-RR-124.pdf) calls this
number *I*. In this case, the value is 0x000001.

(h) A 16-bit bitmap of which byte values are present in the uncompressed text.
In this case, `0b_0000_0011_0000_0000` means that there are values in the 7th
and 8th rows (with 16 bytes per row). Its popcount (population count, the
number of set bits) is 2, which means there are 2 further 16-bit bitmaps
immediately following.

(i) The 16-bit bitmap for the 7th row in (h). `0b_0111_0000_0000_0000` means
that the `0x61`, `0x62` and `0x63` bytes (ASCII 'a', 'b' and 'c') are present.

(j) The 16-bit bitmap for the 8th row in (h). `0b_0010_0000_0000_0000` means
that the `0x72` byte (ASCII 'r') is present.

(k) The 3-bit number of Huffman codes. In this case, `0b_010 = 2`, the minimum
valid value.

(l) The 15-bit number of sections. Each section is 50 symbols. Each symbol
either produces one byte of uncompressed data (roughly speaking) or are one of
three special symbols: RUNA, RUNB (both Run Length Encoding related) or EOB
(End Of Block). The selected Huffman code can change at section boundaries.

(m) The array of unary encoded Huffman code selectors (one for each section).
In this case, an array of one element, [0]. Applying the MTFT (Move To Front
Transform) produces the same values: [0]. This part of `abraca.txt.bz2` isn't
very enlightening but there's a more interesting example in the "MTFT Reprised"
section below.

(n) The first Huffman code. More details below.

(o) The second Huffman code. For `abraca.txt.bz2`, the second Huffman code is
never referenced by the selectors in the (m) bitslice but, for the record, the
(o) bitslice is identical to the (n) bitslice and therefore produces the same
Huffman code.

(p) The bitstream to feed the Huffman codes. More details below.

(q) A 48-bit magic number 0x177245385090 (a homage to sqrt(π)) means no further
blocks.

(r) A 32-bit checksum. Refer to section 2.2.4 "StreamFooter" of [Tsai's
document](https://github.com/dsnet/compress/raw/master/doc/bzip2-format.pdf).

(s) Padding up to a byte boundary.


## Huffman Codes

The (h), (i) and (j) bitslices collectively state that there are 4 byte values
present in the uncompressed output: 'a', 'b', 'c' and 'r'. The relevant MTFT
uses 1 fewer symbols (m01, m02 and m03) than this count, since an m00 operation
(see below) is redundant with the RUNA and RUNB symbols. Adding the 3 special
symbols (RUNA, RUNB, EOB) means that each Huffman code covers 6 symbols.

Here's the (n) bitslice again:

    offset  xoffset ASCII   hex     binary          bitslice
    000025  0x0019  !       0x21    0b_0010_0001    lllm_nnnn
    000026  0x001A  .       0x9A    0b_1001_1010    nnnn_nnnn
    000027  0x001B  h       0x68    0b_0110_1000    nnnn_nnno

Its bits are "0001100110100110100". Adding spaces gives "00011 0 0 11 0 10 0 11
0 10 0", which represents the sequence [3+0, +0, -1+0, +1+0, -1+0, +1+0].
Adding up the deltas gives the code lengths [3, 3, 2, 3, 2, 3] and so the
[canonical Huffman code](https://en.wikipedia.org/wiki/Canonical_Huffman_code)
(with RUNA and RUNB before the mNN symbols and EOB after) is:

    code  length  symbol
    100   3       RUNA
    101   3       RUNB
    00    2       m01
    110   3       m02
    01    2       m03
    111   3       EOB

Here's the (p) bitslice again.

    offset  xoffset ASCII   hex     binary          bitslice
    000030  0x001E  0       0x30    0b_0011_0000    oopp_pppp
    000031  0x001F  .       0x91    0b_1001_0001    pppp_pppp
    000032  0x0020  .       0xE2    0b_1110_0010    pppq_qqqq

Its bits are "11000010010001111". Adding spaces, giving "110 00 01 00 100 01
111", makes it clearer that applying the Huffman code from the (n) bitslice to
the (p) bitslice produces the symbol sequence [m02, m01, m03, m01, RUNA, m03,
EOB].


## Move To Front Transform

MTFT operates on a byte array (let's call it the "order" buffer) that's
initialized with with the bytes-in-use in their natural order: ['a', 'b', 'c',
'r']. Each mNN symbol emits the N'th element (using 0-based indexing) and also
rearranges the order buffer: moving that element to the start and pushing the N
earlier symbols one to the right.

There is no explicit m00 symbol. Instead, a combination of RUNA and RUNB
symbols produce a number R meaning to repeat the 0th element R times (and leave
the order buffer unchanged). For the `abraca.txt.bz2` example, we have one RUNA
symbol but no RUNB symbols, which is effectively an m00 repeated once. Here's
the production:

    order  symbol  emission
    abcr   m02     c
    cabr   m01     a
    acbr   m03     r
    racb   m01     a
    arcb   RUNA    a
    arcb   m03     b
    barc   EOB

Tsai calls this RUNA / RUNB mechanism RLE2 (and goes into further detail on
their semantics) and calls a later Run Length Encoding mechanism RLE1. Later
when decoding, but when encoding, RLE1 occurs before RLE2.


## Burrows Wheeler Transform

Reading down the emission column gives "caraab", which the [BWT technical
report](http://www.hpl.hp.com/techreports/Compaq-DEC/SRC-RR-124.pdf) calls *L*,
the last column of the sorted list of permutations of the uncompressed text. We
can compute its histogram ('a' occurs 3 times, 'b', 'c' and 'r' all occur 1
time) in a first pass over *L*, and thus the cumulative counts:

    Byte  Cumulative
    a     0
    b     3
    c     4
    r     5

We then compute *T* in a second pass over *L*, by assigning an incrementing
value (relative to each cumulative count) to all the 'a' elements in *L*
(assigning 0, 1 and 2), all the 'b' elements (assigning 3), all the 'c'
elements (assinging 4) and all the 'r' elements (assigning 5).

    Index  L  T
    0      c  4
    1      a  0
    2      r  5
    3      a  1
    4      a  2
    5      b  3

Starting with the *I* value given in the (g) bitslice, which was 1, calculate
*I*, *T[I]*, *T[T[I]]*, *T[T[T[I]]]*, etc to give [1, 0, 4, 2, 5, 3]. Indexing
*L* with these values gives "acarba" and reversing that gives "abraca", the
uncompressed text.


## Alternative BWT Inversion

It's not in the BWT technical report, but instead of walking *I*, *T[I]*,
*T[T[I]]*, etc, an alternative but equivalent algorithm works on *U*, the
inverse of the *T* vector. For example *T[4] == 2* and so *U[2] == 4*.

    Index  L  T  F  U
    0      c  4  a  1
    1      a  0  a  3
    2      r  5  a  4
    3      a  1  b  5
    4      a  2  c  0
    5      b  3  r  2

Calculate *U[I]*, *U[U[I]]*, *U[U[U[I]]]*, etc to give [3, 5, 2, 4, 0, 1].
Indexing *L* with these values gives "abraca", the uncompressed text (without
being reversed).


## Final Run Length Encoding

It's not used in `abraca.txt.bz2` but after BWT inversion, there's a final Run
Length Encoding step (which Tsai calls RLE1), separate from the RUNA and RUNB
ops mentioned above. It's triggered by four consecutive identical bytes and
when decoding, converts strings like "AAAA\x03BBBB\x00CCCD" to
"AAAAAAABBBBCCCD".


# Another Example

Here's a larger file, 942 bytes uncompressed and 568 bytes bzip2-compressed.

    $ cat romeo.txt     | head
    Romeo and Juliet
    Excerpt from Act 2, Scene 2
    
    JULIET
    O Romeo, Romeo! wherefore art thou Romeo?
    Deny thy father and refuse thy name;
    Or, if thou wilt not, be but sworn my love,
    And I'll no longer be a Capulet.
    
    ROMEO
    $ hd  romeo.txt.bz2 | head
    00000000  42 5a 68 39 31 41 59 26  53 59 f8 7d 63 30 00 00  |BZh91AY&SY.}c0..|
    00000010  8c 5f 80 00 10 60 85 10  18 be 77 9e 8a 3f ef df  |._...`....w..?..|
    00000020  f0 40 02 30 e3 bb 00 22  9e a0 68 01 a1 a0 00 00  |.@.0..."..h.....|
    00000030  00 02 26 13 23 4d 21 20  6d 41 a0 d0 d0 d0 f2 80  |..&.#M! mA......|
    00000040  2a 9e f5 4f 50 f5 46 4c  23 13 09 89 82 60 4c 09  |*..OP.FL#....`L.|
    00000050  84 89 1a 9a 05 31 92 27  92 6c 88 68 7a 23 26 83  |.....1.'.l.hz#&.|
    00000060  4a b3 4b 3a 06 9b ec c7  19 bc 9a 88 60 6a 6b 42  |J.K:........`jkB|
    00000070  5a c6 8e ad c7 c3 8f a8  f0 91 d1 db e3 1b 25 b5  |Z.............%.|
    00000080  4e 06 cd f3 08 a9 af e8  82 2d 5b 54 fc 35 a9 0e  |N........-[T.5..|
    00000090  13 01 17 7b 4c 98 47 24  25 d9 bd aa cd 31 da ee  |...{L.G$%....1..|

Where `abraca.txt.bz2` had 3 + 1 = 4 possible uncompressed bytes ('a', 'b', 'c'
and 'r'), `romeo.txt.bz2` has 1 + 5 + 4 + 12 + 7 + 14 + 10 = 53. This is
represented in the wire format by the (h), (i) and (j) bitslices for
`abraca.txt.bz2` and the 8 equivalent bitslices in `romeo.txt.bz2` (where we
have repeatedly alternated the (i) and (j) bitslice labels).

    offset  xoffset ASCII   hex     binary          bitslice
    000017  0x0011  _       0x5F    0b_0101_1111    ghhh_hhhh    popcount(h) =  7
    000018  0x0012  .       0x80    0b_1000_0000    hhhh_hhhh
    000019  0x0013  .       0x00    0b_0000_0000    hiii_iiii    popcount(i) =  1
    000020  0x0014  .       0x10    0b_0001_0000    iiii_iiii
    000021  0x0015  `       0x60    0b_0110_0000    ijjj_jjjj    popcount(j) =  5
    000022  0x0016  .       0x85    0b_1000_0101    jjjj_jjjj
    000023  0x0017  .       0x10    0b_0001_0000    jiii_iiii    popcount(i) =  4
    000024  0x0018  .       0x18    0b_0001_1000    iiii_iiii
    000025  0x0019  .       0xBE    0b_1011_1110    ijjj_jjjj    popcount(j) = 12
    000026  0x001A  w       0x77    0b_0111_0111    jjjj_jjjj
    000027  0x001B  .       0x9E    0b_1001_1110    jiii_iiii    popcount(i) =  7
    000028  0x001C  .       0x8A    0b_1000_1010    iiii_iiii
    000029  0x001D  ?       0x3F    0b_0011_1111    ijjj_jjjj    popcount(j) = 14
    000030  0x001E  .       0xEF    0b_1110_1111    jjjj_jjjj
    000031  0x001F  .       0xDF    0b_1101_1111    jiii_iiii    popcount(i) = 10
    000032  0x0020  .       0xF0    0b_1111_0000    iiii_iiii
    000033  0x0021  @       0x40    0b_0100_0000    ikkk_llll

Similarly, where `abraca.txt.bz2` had 1 section (per the (l) bitslice), each
selecting from 2 possible Huffman codes (per the (k) bitslice), `romeo.txt.bz2`
has 17 sections, each selecting from 4 possible Huffman codes.

    offset  xoffset ASCII   hex     binary          bitslice
    000033  0x0021  @       0x40    0b_0100_0000    ikkk_llll
    000034  0x0022  .       0x02    0b_0000_0010    llll_llll
    000035  0x0023  0       0x30    0b_0011_0000    lllm_mmmm


## MTFT Reprised

Bzip2 uses the [Move To Front
Transform](https://en.wikipedia.org/wiki/Move-to-front_transform) in two
places:

1. for the selectors array encoded in the (m) bitslice and
2. for the conversion from the symbols (the Huffman code's output) to
   generating the *L* string.

Use 2 is discussed above but the Use 1 values for `abraca.txt.bz2` is not very
educational, since the (m) bitslice was literally only one bit long. For
`romeo.txt.bz2`, the equivalent (m) bitslice is 29 bits long and makes for a
more interesting example.

    offset  xoffset ASCII   hex     binary          bitslice
    000035  0x0023  0       0x30    0b_0011_0000    lllm_mmmm
    000036  0x0024  .       0xE3    0b_1110_0011    mmmm_mmmm
    000037  0x0025  .       0xBB    0b_1011_1011    mmmm_mmmm
    000038  0x0026  .       0x00    0b_0000_0000    mmmm_mmmm

The bits are "10000111000111011101100000000" which is the unary encoding of the
17-element selector array (before applying the MTFT): [1, 0, 0, 0, 3, 0, 0, 3,
3, 2, 0, 0, 0, 0, 0, 0, 0].

If we label the 4 Huffman codes to select from as w, x, y and z then the MTFT
converts those 17 elements to "xxxxzzzywzzzzzzzz", per the table below. As
before, each N value (pre-MTFT) emits the N'th element (using 0-based indexing)
and also rearranges the order buffer: moving that element to the start and
pushing the N earlier symbols one to the right.

    order  pre  post
    wxyz   1    x
    xwyz   0    x
    xwyz   0    x
    xwyz   0    x
    xwyz   3    z
    zxwy   0    z
    zxwy   0    z
    zxwy   3    y
    yzxw   3    w
    wyzx   2    z
    zwyx   0    z
    zwyx   0    z
    zwyx   0    z
    zwyx   0    z
    zwyx   0    z
    zwyx   0    z
    zwyx   0    z


# Even More Examples

Appendix A of [Tsai's
document](https://github.com/dsnet/compress/raw/master/doc/bzip2-format.pdf)
gives even more wire format worked examples, presented in great detail.
