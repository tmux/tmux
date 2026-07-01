# Tokens

Some Wuffs codecs transform between byte streams and token streams. For
example, a JSON decoder might transform `[1,true,"abc\txyz"]` as

```
Bytes    [  1  ,  t  r  u  e  ,  "  a  b  c  \  t  x  y  z  "  ]
Tokens   0  1  2  3.........  4  5  6......  7...  8......  9  10
Chains   -  -  -  ----------  -  ----------------------------  -
```

The tokens partition the bytes. Each byte belongs to exactly one token. Each
token spans zero or more bytes.

Just as [I/O buffers](/doc/note/io-input-output.md) and
[coroutines](/doc/note/coroutines.md) mean that a byte stream doesn't need to
be entirely in memory at any point, token buffers similarly allow a token
stream to be produced and consumed incrementally.

Nonetheless, conceptually, if the byte stream was a single contiguous slice
then each token would correspond to a sub-slice, one whose length was the token
length and whose position was the sum of all previous tokens' lengths.

Consecutive tokens can also form token chains, which capture higher level
concepts. For example, the JSON string `"abc\txyz"` can correspond to multiple
tokens. One reason is that the maximum token length is `65535` bytes, but JSON
strings can be longer. Another reason is that different parts of the encoded
string are treated differently when reconstructing the decoded string:

- Decoding the first or last `"` is a no-op.
- Decoding `abc` or `xyz` is a `memcpy`.
- Decoding the backslash-escaped `\t` should produce a tab character.


## Representation

A token is just a `uint64_t`. The broad divisions are:

- Bits `17 ..= 63` (47 bits) are the value.
- Bit         `16`  (1 bit)  is the `continued` bit.
- Bits  `0 ..= 15` (16 bits) are the length.

The `continued` bit is whether that the token chain for this token also
contains the next token. The final token in a token chain, including
stand-alone tokens, will have the `continued` bit set to zero.

```
+-----+-------------+-------+-------------+-----+-----------+
|  1  |      21     |   4   |      21     |  1  |     16    |
+-----+-------------+-------+-------------+-----+-----------+
[..................value..................] con     length
[..1..|..........~value_extension.........]
[..0..|.value_major.|.....value_minor.....]
[..0..|......0......|..VBC..|.....VBD.....]
```

The value bits can be sub-divided in multiple ways. First, the high bit:

- Bit         `63`  (1 bit)  denotes an extended (1) or simple (0) token.


### Extended Tokens

- Bits `17 ..= 62` (46 bits) are the bitwise-not (~) of the `value_extension`.

Extended tokens are typically part of a multi-token chain whose first token is
a simple token that provides the semantics for each `value_extension`.


### Simple Tokens

- Bits `42 ..= 62` (21 bits) are the `value_major`.
- Bits `17 ..= 41` (25 bits) are the `value_minor`.

The `value_major` is a 21-bit [base38](/doc/note/base38-and-fourcc.md) number.
For example, an HTML tokenizer might produce a combination of "base" tokens
(see below) and tokens whose `value_major` is `0x0FBEE8`, the base38 encoding
of `"html"`. The `value_major` forms a namespace that distinguishes e.g.
HTML-specific tokens from JSON-specific tokens.

If `value_major` is non-zero then `value_minor` has whatever meaning the
tokenizer's package assigns to it. For example, combining its low 18 bits with
a follow-up extended token's 46 `value_extension` bits could form a 64-bit
overall value, whose semnatics depend on the `value_minor`'s high 7 bits.


### VBCs and VBDs

A zero `value_major` is reserved for Wuffs' built-in "base" package. The
`value_minor` is further sub-divided:

- Bits `38 ..= 41`  (4 bits) are the `VBC` (`value_base_category`).
- Bits `17 ..= 37` (21 bits) are the `VBD` (`value_base_detail`).

The high 47 bits (bits `17 ..= 63`) only have `VBC` and `VBD` semantics when
the high 22 bits (the `extended` and `value_major` parts) are all zero. An
equivalent test is that the high 26 bits (the notional `VBC`), as either an
unsigned integer or a sign-extended integer, is in the range `0 ..= 15`.

These `VBC`s organize tokens into broad groups, generally applicable (as
opposed to being e.g. HTML-specific or JSON-specific). For example, strings and
numbers are two `VBC`s. Structure is another, for container boundaries like the
start and end of HTML elements and JSON arrays.

Filler is yet another `VBC`. Such tokens can generally be ignored (other than
accumulating their length). Filler is most often encountered as whitespace, but
also includes JSON commas (which are [structurally
inessential](https://www.tbray.org/ongoing/When/201x/2016/08/20/Fixing-JSON#p-1))
and comments.

The `VBD` semantics depend on the `VBC`. For example, at 21 bits, the `VBD` can
hold every valid Unicode code point, up to U+10FFFF. A `\t` or `\u2603` in a
JSON string can each be represented by a single `VBC__UNICODE_CODE_POINT` token
whose `VBD` is `0x0009` or `0x2603`, meaning the Unicode code points U+0009
CHARACTER TABULATION (the ASCII tab character) or U+2603 SNOWMAN.

More details on the `VBC` and `VBD` bit assignments are in the [`source
code`](/internal/cgen/base/token-public.h).


## SAX/Pull versus DOM/Push

For file formats that conceptually decode into a node tree, such as HTML or
JSON, Wuffs typically provides a
[SAX](https://en.wikipedia.org/wiki/Simple_API_for_XML)-like pull parser, not a
[DOM](https://en.wikipedia.org/wiki/Document_Object_Model)-like push parser.
There are general reasons for [favoring pull
parsers](https://github.com/raphlinus/pulldown-cmark/blob/master/README.md#why-a-pull-parser),
but also, Wuffs code cannot [dynamically allocate
memory](/doc/note/memory-safety.md), nor does Wuffs have an `unsafe` keyword or
a foreign function interface, so a caller cannot pass arbitrary callbacks into
Wuffs code. Instead, Wuffs just outputs tokens and tokens are just `uint64_t`s.

The [example/jsonfindptrs](/example/jsonfindptrs/jsonfindptrs.cc) program
demonstrates creating a traditional DOM-like node tree from a SAX-like token
stream. The [example/jsonptr](/example/jsonptr/jsonptr.cc) program demonstrates
a lower-level approach that works directly on tokens, where the entire program
(not just the Wuffs library) never calls `malloc`.


## Example Token Stream

```
$ gcc script/print-json-token-debug-format.c -o pjtdf
$ ./pjtdf -all-tokens -human-readable < test/data/json-things.formatted.json
pos=0x00000000  len=0x0001  con=0  vbc=1:Structure........  vbd=0x004011
pos=0x00000001  len=0x0005  con=0  vbc=0:Filler...........  vbd=0x000000
pos=0x00000006  len=0x0001  con=1  vbc=2:String...........  vbd=0x000113
pos=0x00000007  len=0x0002  con=1  vbc=2:String...........  vbd=0x000203
pos=0x00000009  len=0x0001  con=0  vbc=2:String...........  vbd=0x000113
etc
pos=0x00000090  len=0x0002  con=1  vbc=3:UnicodeCodePoint.  vbd=0x00000A
pos=0x00000092  len=0x0002  con=1  vbc=3:UnicodeCodePoint.  vbd=0x00000A
pos=0x00000094  len=0x0001  con=0  vbc=2:String...........  vbd=0x000113
pos=0x00000095  len=0x0001  con=0  vbc=0:Filler...........  vbd=0x000000
pos=0x00000096  len=0x0001  con=0  vbc=1:Structure........  vbd=0x001042
```
