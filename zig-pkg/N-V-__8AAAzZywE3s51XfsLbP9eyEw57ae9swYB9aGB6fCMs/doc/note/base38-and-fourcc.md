# Base38 and FourCC Codes

Both of these encode a four-character string such as `"JPEG"` as a `uint32_t`
value. Computers can compare two integer values faster than they can compare
two arbitrary strings.

Both schemes maintain ordering: if two four-character strings `s` and `t`
satisfy `(s < t)`, and those strings have valid numerical encodings, then the
numerical values also satisfy `(encoding(s) < encoding(t))`.


## FourCC

FourCC codes are not specific to Wuffs. For example, the AVI multimedia
container format can hold various sub-formats, such as "H264" or "YV12",
distinguished in the overall file format by their FourCC code.

The FourCC encoding is the straightforward sequence of each character's ASCII
encoding. The FourCC code for `"JPEG"` is `0x4A504547`, since `'J'` is `0x4A`,
`'P'` is `0x50`, etc. This is essentially 8 bits for each character, 32 bits
overall. The big-endian representation of this number is exactly the ASCII (and
UTF-8) string `"JPEG"`.

Other FourCC documentation sometimes use a little-endian convention, so that
the `{0x4A, 0x50, 0x45, 0x47}` bytes on the wire for `"JPEG"` corresponds to
the number `0x4745504A` (little-endian) instead of `0x4A504547` (big-endian).
Wuffs uses the big-endian interpretation, as it maintains ordering.


## Base38

Base38 is a tighter encoding than FourCC, fitting four characters into 21 bits
instead of 32 bits. This is achieved by using a smaller alphabet of 38 possible
values ('.', 0-9, a-z or '~'), so that it cannot distinguish between e.g. an
upper case 'X' and a lower case 'x'. There are then two happy coincidences:

- `38 ** 4 = 0x1FD110 = 2085136` is slightly smaller than `2 ** 21 = 0x200000 =
  2097152`.
- 21 bits is just large enough to hold every valid Unicode code point, so that
  systems and data formats that can hold "one UCP" can be repurposed to
  alternatively hold "one base38-encoded four-character string".

The base38 encoding of `"jpeg"` is `0x1153D3`, which is `1135571`, which is
`((20 * (38 ** 3)) + (26 * (38 ** 2)) + (15 * (38 ** 1)) + (17 * (38 ** 0)))`.

The base38 encoding of `"ping"` is `0x1633BD`, which is `1455037`, which is
`((26 * (38 ** 3)) + (19 * (38 ** 2)) + (24 * (38 ** 1)) + (17 * (38 ** 0)))`.

The minimum base38 value, 0x000000, corresponds to `"...."`. Depending on
context, this can be used as a default, built-in or placeholder value.

The maximum base38 value, 0x1FD10F = 2085135, corresponds to `"~~~~"`.
Depending on context, this can be used as a "matches everything" wildcard.

The maximum 21-bit value, 0x1FFFFF = 2097151, is not a valid base38 value but
can still be used in some contexts as a sentinel or option-not-set value.


### Base38 Namespaces

Using only 21 bits means that we can use base38 values to partition the set of
possible `uint32_t` values into file-format specific enumerations. Each package
(i.e. Wuffs implementation of a specific file format) can define up to 1024
local values in their own four-character namespace, without conflicting with
other packages (assuming that there aren't e.g. two `"jpeg"` Wuffs packages in
the same library). The conventional `uint32_t` packing is:

- Bit         `31`  (1 bit)  is reserved (zero).
- Bits `10 ..= 30` (21 bits) are the base38-encoded namespace, shifted by 10.
- Bits  `0 ..=  9` (10 bits) are the local enumeration value.

For example:

- [Quirk keys](/doc/note/quirks.md), as a `uint32_t`, use this
  `((namespace << 10) | local)` scheme.
- [Tokens](/doc/note/tokens.md) assign 21 out of 64 bits for a base38 value.


### Base38 4/4/4 and 4/4/2

3×21 bits fits snugly into a `int64_t` or `uint64_t` (the sign or high bit is
unused). 63 bits can therefore hold a 12-character or three 4-character strings
(taken from base38's limited alphabet).

For example, in a custom RPC protocol, the namespace/class/method name could be
base38-encoded as a 4/4/4 string like `"net./conn/ping"`. As a number, the
4/4/4 format (instead of a monolithic 12) means that each 4-character fragment
is base-38 encoded independently and the three 21-bit numbers are then combined
(with bitshifting). `"net./conn/ping"` would be `((0x147150 << 42) | (0x0B7324
<< 21) | 0x1633BD)` which is `0x51C5416E_649633BD`. At the wire format level,
this would occupy a fixed-length (8 bytes) and that 64th bit could, for
example, indicate request or response.

A 2-character string can fit in 11 bits, as `38 ** 2 = 0x5A4 = 1444` is smaller
than `2 ** 11 = 0x800 = 2048`. Therefore:

- 32 bits (21 + 11) can hold a 6-character (as 4/2) alpha-numeric-ish string.
  32 bits obviously fits snugly in a `uint32_t`.
- 53 bits (21 + 21 + 11) can hold a 10-character (as 4/4/2) alpha-numeric-ish
  string. 53 bits fits snugly under JavaScript's `Number.MAX_SAFE_INTEGER` so
  these integers can be losslessly stored in a `double` or a JSON value.

[Enumerated Media Types](./enumerated-media-types.txt) uses this base38 4/4/2
encoding, mapping `"image/jpeg"` to the base38 `"imag/jpeg/.."` which is
`((0x106BF7 << 32) | (0x1153D3 << 11) | 0x000)` which is `0x106BF7_8A9E9800`.
Some more examples:

- `0x09CC62_A9E37800 = "appl/octe/.." = "application/octet-stream"`
- `0x09CC62_B0B24000 = "appl/pdf./.." = "application/pdf"`
- `0x09CC62_DACDFC4E = "appl/voao/s." = "application/vnd.oasis.opendocument.spreadsheet"`
- `0x09CC62_DACDFC6C = "appl/voao/st" = "application/vnd.oasis.opendocument.spreadsheet-template"`
- `0x09CC62_DACDFC74 = "appl/voao/t." = "application/vnd.oasis.opendocument.text"`
- `0x09CC62_DADFCC6B = "appl/vopo/ss" = "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"`
- `0x106BF7_8A9E9800 = "imag/jpeg/.." = "image/jpeg"`
- `0x106BF7_B276B000 = "imag/png./.." = "image/png"`
- `0x106BF7_FE887DA3 = "imag/~~~~/~~" = "image/*"`
- `0x197816_7DF74000 = "text/html/.." = "text/html"`
- `0x197816_B215E800 = "text/plai/.." = "text/plain"`


### Base38 Alphabet

Case-insensitive alpha-numeric, roughly speaking.

```
'.' =  0
'0' =  1
'1' =  2
'2' =  3
'3' =  4
'4' =  5
'5' =  6
'6' =  7
'7' =  8
'8' =  9
'9' = 10
'a' = 11
'b' = 12
'c' = 13
'd' = 14
'e' = 15
'f' = 16
'g' = 17
'h' = 18
'i' = 19
'j' = 20
'k' = 21
'l' = 22
'm' = 23
'n' = 24
'o' = 25
'p' = 26
'q' = 27
'r' = 28
's' = 29
't' = 30
'u' = 31
'v' = 32
'w' = 33
'x' = 34
'y' = 35
'z' = 36
'~' = 37
```
