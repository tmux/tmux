# LZW (Lempel Ziv Welch) Compression

LZW is a general purpose compression algorithm, not specific to GIF, PDF, TIFF
or even to graphics. In practice, there are two incompatible implementations,
LSB (Least Significant Bits) and MSB (Most Significant Bits) first.

The GIF format uses LSB first. The literal width can vary between 2 and 8 bits,
inclusive. There is no EarlyChange option.

The PDF and TIFF formats use MSB first. The literal width is fixed at 8 bits.
There is an EarlyChange option that PDF sometimes uses (see Table 3.7 in the
[PDF 1.4
spec](https://www.adobe.com/content/dam/acom/en/devnet/pdf/pdfs/pdf_reference_archives/PDFReference.pdf))
and TIFF always uses.


# Codes

An LZW encoding is a stream of codes, each code emitting zero or more bytes.
There are four types of codes: literals, copies, clear and end.

Literal codes emit exactly one byte. For a given literal width L, there are (1
<< L) literal codes. For example, if L is at least 7, then the 0x41 code emits
the ASCII 'A' byte. If L is exactly 7, the highest literal code is 0x7F.

There is exactly one clear code and one end code, whose encoded values are
right after the literal codes: CLEAR = ((1 << L) + 0) and END = ((1 << L) + 1).
For 8 bit wide literals, CLEAR is 0x100 and END is 0x101. The clear code is
discussed below. The end code means no more data. Both emit zero bytes.

Copy codes (with encoded values ranging from (END + 1) to 0xFFF, or
equivalently, up to 4095 or up to 12 bits) emit two or more bytes, bytes that
were previously emitted in the decoded stream. This is essentially a lookup in
a key-value table. For example, the table could state that the 0x200 code emits
"foo" and the 0x300 code emits "bar".

Valid copy codes are in the range [END + 1, N], where N starts at END (i.e. the
table's key range starts empty) and grows over time, up to a maximum value of
0xFFF. A clear code resets N to its starting value, END. In effect, a clear
code clears the key-value table.

There is no explicit way to "increase N" or "add this key-value to the table".
Instead, as long as the table isn't full (i.e. that N < 0xFFF), every literal
or copy code, other than the first one in the stream or the first one after a
clear code, implicitly adds a key-value pair (with a key of N) and then
increments N by 1.

The first literal or copy code in the stream (or after a clear code) merely
increments N to (END + 1) without adding a key-value pair. In practice, it is
equivalent to have that first code set a fake key-value pair for N == END,
provided that table lookup algorithm ensures that "END means an end code" takes
precendence.


## Codes Example

The value in each key-value pair consists of a prefix (being 1 or more bytes)
and a suffix (exactly 1 byte). The prefix equals the output emitted by the
previous code: the one that was processed immediately before the current code.
The suffix is the first byte of the output emitted by the current code.

For example, if a literal T code (which emits "T") is followed by a literal O
(which emits "O"), then the value in the implicit key-value pair is the entire
"T" concatenated with the first byte of "O", so the value is "TO".

For example, if a copy 0x104 code (which emits "BE") is followed by a copy
0x106 code (which emits "OR"), then the value is "BEO".

For example, if a copy 0x10E code (which emits "TOBE") is followed by a literal
Y (which emits "Y"), then the value is "TOBEY".

It is possible for a copy code to equal N, the very key to be implicitly added
to the table. The prefix is, once again, the output emitted by the previous
code. The suffix byte is, once again, the first byte of the output emitted by
the current code, which in this case is the first byte of the prefix.

For example, if the previous code emitted "OTX", then a copy code of N would
both emit "OTXO" and add a (key=N, value="OXTO") pair to the table.

One possible encoding of "TOBEORNOTTOBEORTOBEORNOTXOTXOTXOOTXOOOTXOOOTOBEY" is
in the table below. With dots to punctuate each code's emitted output, that is
"T.O.B.E.O.R.N.O.T.TO.BE.OR.TOB.EO.RN.OT.X.OTX.OTXO.OTXOO.OTXOOO.TOBE.Y".

```
 Code   Emits    Key    Value   Pre1+Suf1  LM1  /Q  %Q   PreQ+SufQ
    T       T
    O       O  0x102       TO      T O       1   0   1      - TO.
    B       B  0x103       OB      O B       1   0   1      - OB.
    E       E  0x104       BE      B E       1   0   1      - BE.
    O       O  0x105       EO      E O       1   0   1      - EO.
    R       R  0x106       OR      O R       1   0   1      - OR.
    N       N  0x107       RN      R N       1   0   1      - RN.
    O       O  0x108       NO      N O       1   0   1      - NO.
    T       T  0x109       OT      O T       1   0   1      - OT.
0x102      TO  0x10A       TT      T T       1   0   1      - TT.
0x104      BE  0x10B      TOB  0x102 B       2   0   2      - TOB
0x106      OR  0x10C      BEO  0x104 O       2   0   2      - BEO
0x10B     TOB  0x10D      ORT  0x106 T       2   0   2      - ORT
0x105      EO  0x10E     TOBE  0x10B E       3   1   0  0x10B E..
0x107      RN  0x10F      EOR  0x105 R       2   0   2      - EOR
0x109      OT  0x110      RNO  0x107 O       2   0   2      - RNO
    X       X  0x111      OTX  0x109 X       2   0   2      - OTX
0x111     OTX  0x112       XO      X O       1   0   1      - XO.
0x113    OTXO  0x113     OTXO  0x111 O       3   1   0  0x111 O..
0x114   OTXOO  0x114    OTXOO  0x113 O       4   1   1  0x111 OO.
0x115  OTXOOO  0x115   OTXOOO  0x114 O       5   1   2  0x111 OOO
0x10E    TOBE  0x116  OTXOOOT  0x115 T       6   2   0  0x115 T..
    Y       Y  0x117    TOBEY  0x10E Y       4   1   1  0x10B EY.
0x101   -end-
```

See `script/print-lzw-example.go` for the code that generated that table,
including three implementations of a simplified core of the LZW algorithm.

The first four columns (Code, Emits, Key, Value) are discussed above. The
remaining columns are discussed below.


## Prefix+Suffix Representation

A naive implementation of the key-value table, with the complete value stored
contiguously for each key, would have quadratic worst case memory requirements,
as each successive value can be up to 1 byte longer than the previous longest.

In contrast, the Pre1+Suf1 columns are a compact (3 bytes per key-value pair
for a uint16 prefix and uint8 suffix) representation of the values. Each value
is the concatenation of a variable length prefix (stored as a table key) and a
1-byte suffix. For example, the value for key 0x10C is "BE" + "O", and "BE" is
the value for the key 0x104.

Note that the Pre1 column is exactly the same as the Code column, shifted
vertically by one row. Note also that the Suf1 column is exactly the same as
the final byte of the Value column.

To reconstruct a code's value, start with the suffix and work backwards,
walking the prefix chain producing one byte at a time. Either reconstruct the
value in an intermediate buffer and then once fully reconstructed (and its
length is known) copy it to the output buffer, or walk the chain twice (the
first pass calculates the value length, the second pass reconstructs the value
directly in the output buffer, skipping a copy) or store each value's length
along with its prefix and suffix (an additional uint16 stored per key-value
pair) and reconstruct the value directly in the output buffer in one pass.

Some of the computations are slightly easier, avoiding multiple "+1"s and "-1"s
in the program, if we store the "length minus 1" of each value, not the length.
This is the LM1 column. Note that the LM1 of all literal codes are 0.


## Multi-Byte Suffixes

It can be faster (albeit with higher memory requirements) to store up-to-Q-byte
suffixes instead of 1-byte suffixes, for some positive integer Q: the quantum
of bytes per read or write. In effect, changing Q (usually at compile time, not
at run time) is a classic performance versus memory trade-off.

On modern CPUs that can do unaligned 32 or 64 bit loads and stores, a Q of 4 or
8 can perform very well. To keep this worked example short and simple, we use a
Q of 3, meaning that we store 1, 2 or 3 byte suffixes per key, and the prefix
length is a multiple of 3. For example:

  - The value "BE" is the "" prefix and the 2-byte "BE" suffix.
  - The value "TOB" is the "" prefix and the 3-byte "TOB" suffix.
  - The value "OTXOOOT" is the "OTXOOO" prefix and the 1-byte "T" suffix.

The number of steps (or equivalently, Q-byte chunks) in the prefix chain is
(LM1 / Q), which is the "/Q" column in the example above.

The suffix length is ((LM1 % Q) + 1). The first part, (LM1 % Q), is the "%Q"
column in the example above. When Q is 8, the "/Q" and "%Q" operations are
simple bitwise operations, such as ">> 3" or "& 7". When Q is 1 (a degenerate
case), the "%Q" column is all zeroes and all suffixes are 1 byte long, which is
unsurprisingly equivalent to the 1-byte suffix representation described above.

When copying the suffix to a buffer (either an intermediate buffer or a final
buffer), it is often unnecessary to copy *exactly* suffix length bytes,
provided that there is enough destination buffer space for all Q bytes. Even if
we write excess bytes, decoding subsequent codes will overwrite the excess, or
else we'll get an end code and the excess will be ignored. Again, on modern
CPUs, it can be faster to write *at least 7* bytes (i.e. *exactly 8*) than to
write *exactly 7*.

Even so, the (LM1 % Q) value is still necessary to calculate where to start
writing those Q-byte chunk. After that, reconstructing the value proceeds in
the same way as in the 1-byte suffix algorithm. It just involves larger chunks.
For example, the value for the key 0x116 is the suffix "T" preceded by the
value for 0x115, which is the suffix "OOO" preceded by the value for 0x111,
which is the suffix "OTX" with no prefix: the value is "OTX"+"OOO"+"T".

The PreQ column is more complicated than the Pre1 column. It is no longer just
a vertically shifted Code column, and can now contain "no prefix" values (or
equivalently, the "/Q" column is 0). When inserting a new key-value pair, we
only set the N'th prefix equal to the previously seen code (the Code column
shifted by one row) if the suffix for that previous code was a full Q bytes (or
equivalently, if the current row's "%Q" value is 0). Otherwise, we copy the
previous code's prefix and suffix, and then extend the suffix with one more
byte. For example, the PreQ+SufQ value for the 0x117 key is based on the
PreQ+SufQ value (the same PreQ, an extended SufQ) of the previously seen code,
namely 0x10E.


# Varying Bit Widths

The description above has discussed an LZW stream as a stream of codes. In the
actual wire format, those codes are packed into a stream of bytes, and a code
can straddle byte boundaries.

Specifically, the next code in the stream takes B bits, where B is the smallest
number of that bits that can distinguish all valid codes: those codes in the
range [0, N]. If N is 0x101, B is 9. If N is 0x3FF, B is 10 bits. If N is
0x400, B is 11. Remember that each successive code increments N, up to a
maximum N of 0xFFF, or 12 bits.

The literal width can also vary in the range [2..8] for LZW as used by GIF. In
that case, B starts off as 1 more than the literal width. For example, with a
literal width of 2, the four literal codes are 0x0, 0x1, 0x2 and 0x3, which are
followed by a clear code of 0x4 and an end code of 0x5, so N is 0x5 and B is 3.


## Early Change

Unfortunately, some implementations misinterpreted the algorithm, and
incremented B *before* instead of *after* emitting a bit-packed code for N ==
((1 << B) - 1). Accordingly, LZW as used in TIFF and sometimes in PDF
(depending on the EarlyChange bit) increment B one iteration earlier. The
maximum value of B is still 12.

This misinterpretation is unfixable due to backwards compatibility, 


## LSB and MSB

The bits are then packed into bytes in a predetermined ordering. For GIF, the
bits are packed Least Significant Bits first. For PDF and TIFF, Most
Significant Bits first. With a 8 bit literal width (and therefore a 9 bit
initial code width), the two literal codes T (0x54) and O (0x4F) followed by an
end code 0x101 would be encoded as 27 bits, or 4 bytes (with 5 padding bits,
which are ignored but are conventionally set to zero).

In LSB first, it would be four bytes, 0x54 0x9E 0x04 0x04:

    offset  xoffset ASCII   hex     binary
    000000  0x0000  T       0x54    0b_0101_0100
    000001  0x0001  .       0x9E    0b_1001_1110
    000002  0x0002  .       0x04    0b_0000_0100
    000003  0x0003  .       0x04    0b_0000_0100

These four bytes contain three 9-bit codes:

    binary                              width   code
    0b_...._...._...._...0_0101_0100     9      0x0054
    0b_...._...._...._..00_1001_111.     9      0x004F
    0b_...._...._...._.100_0000_01..     9      0x0101

In MSB first, it would again be four bytes, 0x2A 0x13 0xE0 0x20:

    offset  xoffset ASCII   hex     binary
    000000  0x0000  *       0x2A    0b_0010_1010
    000001  0x0001  .       0x13    0b_0001_0011
    000002  0x0002  .       0xE0    0b_1110_0000
    000003  0x0003          0x20    0b_0010_0000

Again, these four bytes contain three 9-bit codes:

    binary                              width   code
    0b_0010_1010_0..._...._...._....     9      0x0054
    0b_.001_0011_11.._...._...._....     9      0x004F
    0b_..10_0000_001._...._...._....     9      0x0101

See `std/gif/README.md` for a complete worked example of LSB-first encoded LZW
data, including a varying B width.


# More Wire Format Examples

See `test/data/artificial/gif-*.commentary.txt`
