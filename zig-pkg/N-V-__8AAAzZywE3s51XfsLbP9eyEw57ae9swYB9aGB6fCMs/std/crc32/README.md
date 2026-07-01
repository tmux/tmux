# CRC-32

CRC-32 (Cyclic Redundancy Check 32) is a checksum algorithm that hashes byte
sequences to 32 bit values.

The algorithm is based on polynomial division. In theory, a variety of
polynomials can be used. In practice, only two are widely used. The IEEE
polynomial is used by Bzip2, Ethernet (IEEE 802.3), Gzip, MPEG-2, PNG, SATA,
Zip and other formats. The Castagnoli polynomial is used by Btrfs, Ext4, iSCSI,
SCTP and other formats.


# Polynomial Division

In arithmetic, two numbers `A` and `B` can be divided to yield a quotient `Q`
and a remainder `R`, so that `A = (B × Q) + R`. Two polynomials can similarly
be divided, yielding quotient and remainder polynomials. This remains true when
restricting polynomial coefficients to the [Galois Field
GF(2)](https://en.wikipedia.org/wiki/GF(2)), equivalent to the integers modulo
2, i.e. the integers 0 and 1. Addition and multiplication are equivalent to XOR
and AND, so that `foo + foo = 0` and `foo + 0 = foo`. For example,

```
  ((x⁴ + x¹ + x⁰) × (x⁴ + x⁰)) + (x¹ + x⁰)
=  (x⁸ + x⁵ + x⁴) + (x⁴ + x¹ + x⁰) + (x¹ + x⁰)
=   x⁸ + x⁵ + (x⁴ + x⁴) + (x¹ + x¹) + (x⁰ + x⁰)
=   x⁸ + x⁵ + 0 + 0 + 0
=   x⁸ + x⁵
```

Therefore, dividing `A(x) = x⁸ + x⁵` by `B(x) = x⁴ + x¹ + x⁰` yields a
remainder of `R(x) = x¹ + x⁰`. Such polynomials can be represented as binary
numbers: `A` is `0b100100000`, `B` is `0b10011` and `R` is `0b11`.


## Bitwise Operations

[Polynomial division](https://en.wikipedia.org/wiki/Polynomial_long_division)
over GF(2) can be efficiently implemented as a sequence of XORs and shifts.
Extending the example above (with a divisor polynomial `B` of degree `N` equal
to 4), an input `0b10010` is first padded with `N` trailing zeroes to give the
initial state: the `A` value `0b100100000`.

The algorithm loops, alternating between shifting a copy of the `B` polynomial
so that its left-most 1 bit aligns with the left-most 1 bit of the state, and
XOR-ing those two values, producing a new state whose left-most 1 bit is
further to right. The loop continues until no such shifted `B` can be found, or
in other words, all of the state's bits to the left of the right-most `N` bits
are all zero. What remains is the remainder polynomial, `R`.

```
input   00010010
pad     00010010 0000
divisor    10011
xor     00000001 0000
divisor        1 0011
xor     00000000 0011
remain           0011
```

This `B` divisor polynomial, `0b10011`, is also known as the CRC-4-ITU
polynomial. The `4` in "CRC-4-ITU" is the `N` in "an N bit CRC". The polynomial
has `N+1` bits, and the hash value or remainder `0b0011` (what remains after
dividing a longer padded-input polynomial by the shorter hash polynomial) has
`N` bits.


## Normal (MSB) and Reversed (LSB) Representation

Consider that CRC-4-ITU polynomial again: (x⁴ + x¹ + x⁰) or `0b10011` in
binary. The high bit of an `N`-degree polynomial is always 1, so an equivalent
representation of that polynomial is `N=4, MSB, BITS=0b0011`. This is the
"normal" or Most Significant Bit first order. Another equivalent representation
is to visit the bits right-to-left: `N=4, LSB, BITS=0b1100`. This is the
"reversed" or Least Significant Bit first order. The binary number `0b1100` in
hexadecimal is `0xC`, so yet another equivalent representation of that
polynomial is `N=4, LSB, BITS=0xC`. If `N` and `LSB`-ness is agreed beforehand,
`0xC` is all you need to specify the polynomial.

For 32 bit CRCs, the two popular polynomials (presented in `LSB` order) are
`0xEDB8_8320` and `0x82F6_3B78`, also called the IEEE and Castagnoli
polynomials, also called CRC-32 and CRC-32C. For example, the bit string
representation of the IEEE polynomial `0xEDB8_8320`, un-reversed and with the
implicit high bit, is `0b1_00000100_11000001_00011101_10110111 =
0x1_04C1_1DB7`.


# Worked Example

A worked example for calculating the CRC-32 hash of the three byte input "Hi\n"
(equivalently, "\x48\x69\x0A" but note in the work below's input that the bits
within each byte are reversed to be LSB-first) proceeds similarly to the
simpler worked example for the CRC-4-ITU hash, above, with two additional
inversion steps, described below.

Both the CRC-4-ITU and the CRC-32 worked examples are output by the
`script/print-crc32-example.go` program.

```
input   00010010 10010110 01010000
pad     00010010 10010110 01010000 00000000 00000000 00000000 00000000
invert  11101101 01101001 10101111 11111111 00000000 00000000 00000000
divisor 10000010 01100000 10001110 11011011 1
xor     01101111 00001001 00100001 00100100 10000000 00000000 00000000
divisor  1000001 00110000 01000111 01101101 11
xor     00101110 00111001 01100110 01001001 01000000 00000000 00000000
divisor   100000 10011000 00100011 10110110 111
xor     00001110 10100001 01000101 11111111 10100000 00000000 00000000
divisor     1000 00100110 00001000 11101101 10111
xor     00000110 10000111 01001101 00010010 00011000 00000000 00000000
divisor      100 00010011 00000100 01110110 110111
xor     00000010 10010100 01001001 01100100 11000100 00000000 00000000
divisor       10 00001001 10000010 00111011 0110111
xor     00000000 10011101 11001011 01011111 10101010 00000000 00000000
divisor          10000010 01100000 10001110 11011011 1
xor     00000000 00011111 10101011 11010001 01110001 10000000 00000000
divisor             10000 01001100 00010001 11011011 0111
xor     00000000 00001111 11100111 11000000 10101010 11110000 00000000
divisor              1000 00100110 00001000 11101101 10111
xor     00000000 00000111 11000001 11001000 01000111 01001000 00000000
divisor               100 00010011 00000100 01110110 110111
xor     00000000 00000011 11010010 11001100 00110001 10010100 00000000
divisor                10 00001001 10000010 00111011 0110111
xor     00000000 00000001 11011011 01001110 00001010 11111010 00000000
divisor                 1 00000100 11000001 00011101 10110111
xor     00000000 00000000 11011111 10001111 00010111 01001101 00000000
divisor                   10000010 01100000 10001110 11011011 1
xor     00000000 00000000 01011101 11101111 10011001 10010110 10000000
divisor                    1000001 00110000 01000111 01101101 11
xor     00000000 00000000 00011100 11011111 11011110 11111011 01000000
divisor                      10000 01001100 00010001 11011011 0111
xor     00000000 00000000 00001100 10010011 11001111 00100000 00110000
divisor                       1000 00100110 00001000 11101101 10111
xor     00000000 00000000 00000100 10110101 11000111 11001101 10001000
divisor                        100 00010011 00000100 01110110 110111
xor     00000000 00000000 00000000 10100110 11000011 10111011 01010100
remain                             10100110 11000011 10111011 01010100
invert                             01011001 00111100 01000100 10101011
hex                                   A   9    C   3    2   2    5   D
```

The final line says that the CRC-32 checksum of "Hi\n" is `0xD522_3C9A`. This
can be verified by running the `/usr/bin/crc32` program:

```
$ echo Hi | hd    /dev/stdin
00000000  48 69 0a                                          |Hi.|
00000003
$ echo Hi | crc32 /dev/stdin
d5223c9a
```


## Inversion

An `A` value of `0b100100000` as a mathematical concept (whether as a binary
number of as a polynomial) is unchanged by adding leading 0 bits. Thus, the CRC
algorithm in its basic form will compute the same hash value for both some
string `s` and another string that is multiple "\x00" bytes followed by `s`.

It's easy for network errors or other corruption to introduce multiple "\x00"
bytes, and a good checksum should be able detect that. To do so, CRC as used in
practice inverts (applies a bitwise NOT to) the first `N` bits of the padded
input, just before the divisor-xor loop of the algorithm.

Similarly but independently of that, the same issue (in a more limited way) can
occur with trailing "\x00" bytes. The same trick addresses that, this time
inverting the last `N` bits, just after the divisor-xor loop.

While the two inversions are notionally independent, and it would be possible
to implement a CRC flavor that inverted only one side, inverting on both sides
results in a nice decomposability property. Calculating the CRC hash of the
concatenation of two strings, `s+t`, can be computed by first hashing `s`, then
hashing `t` starting with an initial state equal to that hash instead of equal
to zero. The second inversion of the `s` computation cancels out the first
inversion of the `t` computation.


# Fast Computation

In theory, the mathematics of the CRC algorithm works in terms of bit streams.
In practice, the computation of the CRC hash value works with byte streams: 8
(or more) bits at a time. It is faster to do so because CPU and RAM
fundamentally work with bytes (or words) instead of bits, and because the
bit-by-bit loop (over 8 successive 0 or 1 input bits) can be implemented as a
byte-by-byte loop involving a single lookup into a 256-entry table, yielding a
32-bit value to XOR with the cumulative state. For well known polynomials, such
as the IEEE and Castagnoli polynomials, these lookup tables can be calculated
beforehand, and hard-coded at compile time. A uint32 `state` variable can be
updated by the simple algorithm (with `invert` meaning `bitwise_not`):

```
state = invert(state)
for each input byte x {
    state = table[x ^ (state & 0xFF)] ^ (state >> 8)
}
state = invert(state)
```


## Multi-Byte Lookup Tables

Better performance can be obtained by processing M bytes at a time, e.g. for an
M of 4, 8 or 16. Even at only 4 bytes, a naive implementation would require a
more-than-4-billion (256 × 256 × 256 × 256) entry lookup table, which is
impractical. A cleverer algorithm can process 4 bytes at a time using (256 +
256 + 256 + 256) entries. See [A Systematic Approach to Building High
Performance, Software-based, CRC
Generators](https://web.archive.org/web/20060515024705/http://www.intel.com/technology/comms/perfnet/download/CRC_generators.pdf)
by Kounavis and Berry of Intel Corporation. [Stephan Brumme's CRC-32
page](https://create.stephan-brumme.com/crc32/#slicing-by-16-overview) also has
some more discussion and code examples.

This slicing-by-M algorithm is still applicable even when the input isn't an
exact multiple of M bytes. The bulk of the input is processed M bytes at a
time, and the remainder is then processed 1 byte at a time.

For M greater than 4, the first 4 bytes of each slice are treated in one way,
since the state (a uint32) is 4 bytes long. The remaining M-4 bytes are treated
a second way. See [the actual code](./common_crc32.wuffs) for details.


## SIMD Implementations

Even better performance can be obtained through CPU-specific SIMD instructions.
See [Fast CRC Computation for Generic Polynomials Using PCLMULQDQ
Instruction](https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/fast-crc-computation-generic-polynomials-pclmulqdq-paper.pdf)
by Gopal, Ozturk, Guilford, Wolrich, Feghali and Dixon of Intel Corporation and
Karakoyunlu of the Worcester Polytechnic Institute.


# Further Reading

See a couple of Wikipedia articles:

- [CRC](https://en.wikipedia.org/wiki/Cyclic_redundancy_check),
- [Computation of
  CRCs](https://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks).
