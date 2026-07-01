# Adler-32

Adler-32 is a checksum algorithm that hashes byte sequences to 32 bit values.
It is named after its inventor, Mark Adler, who also co-invented the Gzip and
Zlib compressed file formats. Amongst other differences, Gzip uses CRC-32 as
its checksum and Zlib uses Adler-32.

The algorithm, described in [RFC 1950](https://www.ietf.org/rfc/rfc1950.txt),
is simple. Conceptually, there are two unsigned integers `s1` and `s2` of
infinite precision, initialized to `0` and `1`. These two accumulators are
updated for every input byte `src[i]`. At the end of the loop, `s1` is `1` plus
the sum of all source bytes and `s2` is the sum of all (intermediate and final)
`s1` values:

    var s1 = 1;
    var s2 = 0;
    for_each i in_the_range_of src {
        s1 = s1 + src[i];
        s2 = s2 + s1;
    }
    return ((s2 % 65521) << 16) | (s1 % 65521);

The final `uint32_t` hash value is composed of two 16-bit values: `(s1 %
65521)` in the low 16 bits and `(s2 % 65521)` in the high 16 bits. `65521` is
the largest prime number less than `(1 << 16)`.

Infinite precision arithmetic requires arbitrarily large amounts of memory. In
practice, computing the Adler-32 hash instead uses a `uint32_t` typed `s1` and
`s2`, modifying the algorithm to be concious of overflow inside the loop:

    uint32_t s1 = 1;
    uint32_t s2 = 0;
    for_each i in_the_range_of src {
        s1 = (s1 + src[i]) % 65521;
        s2 = (s2 + s1)     % 65521;
    }
    return (s2 << 16) | s1;

The loop can be split into two levels, so that the relatively expensive modulo
operation can be hoisted out of the inner loop:

    uint32_t s1 = 1;
    uint32_t s2 = 0;
    for_each_sub_slice s of_length_up_to M partitioning src {
        for_each i in_the_range_of s {
            s1 = s1 + s[i];
            s2 = s2 + s1;
        }
        s1 = s1 % 65521;
        s2 = s2 % 65521;
    }
    return (s2 << 16) | s1;

We just need to find the largest `M` such that the inner loop cannot overflow.
The worst case scenario is that `s1` and `s2` both start the inner loop at
`65520` and every subsequent `src[i]` byte equals `0xFF`. A simple
[computation](https://play.golang.org/p/wdx6BPDs2-R) finds that the largest
non-overflowing `M` is 5552.

In a happy coincidence, 5552 is an exact multiple of 16, which often works well
with loop unrolling and with SIMD alignment.


## Comparison with CRC-32

Adler-32 is a very simple hashing algorithm. While its output is nominally a
`uint32_t` value, it isn't uniformly distributed across the entire `uint32_t`
range. The `[65521, 65535]` range of each 16-bit half of an Adler-32 checksum
is never touched.

While neither Adler-32 or CRC-32 are cryptographic hash functions, there is
still a stark difference in the patterns (or lack of) in their hash values of
the `N`-byte string consisting entirely of zeroes, as [this Go
program](https://play.golang.org/p/SkPVp0tBnDl) shows:

    N  Adler-32    CRC-32      Input
    0  0x00000001  0x00000000  ""
    1  0x00010001  0xD202EF8D  "\x00"
    2  0x00020001  0x41D912FF  "\x00\x00"
    3  0x00030001  0xFF41D912  "\x00\x00\x00"
    4  0x00040001  0x2144DF1C  "\x00\x00\x00\x00"
    5  0x00050001  0xC622F71D  "\x00\x00\x00\x00\x00"
    6  0x00060001  0xB1C2A1A3  "\x00\x00\x00\x00\x00\x00"
    7  0x00070001  0x9D6CDF7E  "\x00\x00\x00\x00\x00\x00\x00"

Adler-32 is a simpler algorithm than CRC-32. At the time Adler-32 was invented,
it had noticeably higher throughput. With modern SIMD implementations, that
performance difference has largely disappeared.


# Worked Example

A worked example for calculating the Adler-32 hash of the three byte input
"Hi\n", starting from the initial state `(s1 = 1)` and `(s2 = 0)`:

    src[i]  ((s2 << 16) | s1)
    ----    0x00000001
    0x48    0x00490049
    0x69    0x00FB00B2
    0x0A    0x01B700BC
