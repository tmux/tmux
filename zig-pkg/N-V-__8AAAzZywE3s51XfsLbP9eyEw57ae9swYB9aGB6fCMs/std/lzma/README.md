# LZMA

Lempel–Ziv–Markov chain algorithm (LZMA) is a general purpose compression
format, using range coding and Lempel-Ziv style length-distance
back-references. Various descriptions of the file format are at:

- [LZMA-SDK reference implementation](https://raw.githubusercontent.com/jljusten/LZMA-SDK/781863cdf592da3e97420f50de5dac056ad352a5/CPP/7zip/Bundles/LzmaSpec/LzmaSpec.cpp)
- [LZMA-SDK specification](https://raw.githubusercontent.com/jljusten/LZMA-SDK/781863cdf592da3e97420f50de5dac056ad352a5/DOC/lzma-specification.txt)
- [Lzip Manual - Stream Format](https://www.nongnu.org/lzip/manual/lzip_manual.html#Stream-format)
- [Wikipedia (LZMA)](https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Markov_chain_algorithm)


## Byms and Packets

The compressed stream is a stream of "byms" (binary symbols, either 0 or 1;
yeah, it's an awkward term, but less ambiguous than using "bits" for both
compressed and uncompressed things). Byms are conceptually very similar to bits
(binary digits, ubiquitous in computing, a 0.5 KiB file holds 512 bytes, which
is 4096 bits), but are encoded using an adaptive binary range-coder so that N
byms usually compresses to fewer than N bits.

The stream is divided into packets, each packet describing either a single
byte, or an LZ77 sequence with its length and distance implicitly or explicitly
encoded. Each part of each packet is modeled with independent contexts, so the
probability predictions for each bym are correlated with the values of that bym
(and related byms from the same field) in previous packets of the same type.

There are 7 types of packets:

    Symbols                  Meaning
    0         ,literal       LITERAL byte (8_byms)
    1,0       ,len ,dist     MATCH
    1,1,0,0                  SHORTREP   len = 1, dist =     Most Recently Used
    1,1,0,1   ,len           LONGREP[0]          dist =     Most Recently Used
    1,1,1,0   ,len           LONGREP[1]          dist = 2nd Most Recently Used
    1,1,1,1,0 ,len           LONGREP[2]          dist = 3rd Most Recently Used
    1,1,1,1,1 ,len           LONGREP[3]          dist = 4th Most Recently Used

The length encoding:

    Symbols         Value
    0   ,3_byms     Ranges from  2 ..=   9
    1,0 ,3_byms     Ranges from 10 ..=  17
    1,1 ,8_byms     Ranges from 18 ..= 273

The distance encoding starts with a 6-bym "Slot" value, which determines how
many further byms are needed. For small Slot values, there are up to 5 extra
byms, all context-encoded like the rest of LZMA. For large Slot values, there
are N extra byms. The first (N - 4) of them are encoded with a fixed 50%
probability and the remaining 4 byms are context-encoded. The largest encodable
distance-biased-by-1 is `0xFFFF_FFFF`, a (2 + 26 + 4) bit number.

    Slot (decimal)   Distance (binary), biased by 1        Extra byms
    0                0                                      0
    1                1                                      0
    2                10                                     0
    3                11                                     0
    4                10 x                                   1
    5                11 x                                   1
    6                10 xx                                  2
    7                11 xx                                  2
    8                10 xxx                                 3
    9                11 xxx                                 4
    10               10 xxxx                                5
    11               11 xxxx                                4
    12               10 xxxxx                               5
    13               11 xxxxx                               5
    14               10 yy zzzz                             2+4
    15               11 yy zzzz                             2+4
    16               10 yyy zzzz                            3+4
    17               11 yyy zzzz                            3+4
    18               10 yyyy zzzz                           4+4
    ...              ...                                   ...
    61               11 yyyyyyyyyyyyyyyyyyyyyyyyy zzzz     25+4
    62               10 yyyyyyyyyyyyyyyyyyyyyyyyyy zzzz    26+4
    63               11 yyyyyyyyyyyyyyyyyyyyyyyyyy zzzz    26+4

"xxxx" means up-to-5 byms are encoded with a "reverse" binary tree. Each Slot
has its own binary tree probabilities.

"yyyy" means up-to-26 byms. Each has a fixed 50% probability.

"zzzz" means four byms encoded with a "reverse" binary tree. All Slots use the
same "align" binary tree probabilities.

"Biased by 1" means that slot=2 implies biasedDistance=2 so distance=3. A
biasedDistance of `0xFFFF_FFFF` means the End of Stream. Otherwise, the
corrected (unbiased) distance ranges in `[1 ..= 0xFFFF_FFFF]`.

Distance is also capped by the dictionary size, stated in the LZMA header.


## State

In addition to the hundreds of contextual probabilities (see below), there's
also a "State" variable with 12 possible values. The initial State is zero.
Each packet causes a state transtition. The left-most column is the current
State, the other columns hold the next State, depending on the packet:

    State     LITERAL   MATCH     LONGREP   SHORTREP
    0         0         7         8         9
    1         0         7         8         9
    2         0         7         8         9
    3         0         7         8         9
    4         1         7         8         9
    5         2         7         8         9
    6         3         7         8         9
    7         4         10        11        11
    8         5         10        11        11
    9         6         10        11        11
    10        4         10        11        11
    11        5         10        11        11

Equivalently, but looking backwards instead of forwards, each State embodies
the 1st, 2nd, 3rd and 4th MRUP (Most Recently Used Packet). The ? question mark
means every possible packet. Some States (2, 5 and 11) have two rows - multiple
possible histories could lead to that State:

    State     1MRUP         2MRUP         3MRUP         4MRUP
    0         LITERAL       LITERAL       LITERAL       ?
    1         LITERAL       LITERAL       MATCH         ?
    2a        LITERAL       LITERAL       LONGREP       ?
    2b        LITERAL       LITERAL       SHORTREP      NON-LITERAL
    3         LITERAL       LITERAL       SHORTREP      LITERAL
    4         LITERAL       MATCH         ?             ?
    5a        LITERAL       LONGREP       ?             ?
    5b        LITERAL       SHORTREP      NON-LITERAL   ?
    6         LITERAL       SHORTREP      LITERAL       ?
    7         MATCH         LITERAL       ?             ?
    8         LONGREP       LITERAL       ?             ?
    9         SHORTREP      LITERAL       ?             ?
    10        MATCH         NON-LITERAL   ?             ?
    11a       LONGREP       NON-LITERAL   ?             ?
    11b       SHORTREP      NON-LITERAL   ?             ?

A State's 12 possible values can also be aggregated into whether the State is
less than or at least 7: whether the last packet was a LITERAL or a NON-LITERAL
(a Lempel-Ziv copy-from-history). When decoding a LITERAL after a NON-LITERAL
(as opposed to a LITERAL after a LITERAL, i.e. having `decodeLiteral()` branch
on `State < 7`), there is information in the next historical byte after the
previous NON-LITERAL packet's copy-source. That byte almost always does not
equal the about-to-be-decoded literal byte, because if it did equal, the copy
would have been longer.


## Algorithm Overview

The core loop's body (with AlgOveNN line numbers for others to reference) looks
conceptually like this:

    AlgOve00    if decodeTheNextBym() == 0 {
    AlgOve01        // Decode a LITERAL.
    AlgOve02        literal = decodeLiteral()
    AlgOve03        emitLiteral(literal)
    AlgOve04        continue
    AlgOve..
    AlgOve20    } else if decodeTheNextBym() == 0 {
    AlgOve21        // Decode a MATCH.
    AlgOve22        len = decodeLen()
    AlgOve23        slot = decodeSlot(min(len-2, 3))
    AlgOve24        distBiasedBy1 = decodeDistBiasedBy1(slot)
    AlgOve25        if distBiasedBy1 == 0xFFFF_FFFF {
    AlgOve26            break  // End of Stream.
    AlgOve27        }
    AlgOve28        mrud = (1 + distBiasedBy1, mrud[0], mrud[1], mrud[2])
    AlgOve29        goto labelDoTheLZCopy
    AlgOve..
    AlgOve40    } else if decodeTheNextBym() == 0 {
    AlgOve41        if decodeTheNextBym() == 0 {
    AlgOve42            // Decode a SHORTREP.
    AlgOve43            len = 1
    AlgOve44            goto labelDoTheLZCopy
    AlgOve45        }
    AlgOve46        // Decode a LONGREP[0].
    AlgOve..
    AlgOve60    } else if decodeTheNextBym() == 0 {
    AlgOve61        // Decode a LONGREP[1].
    AlgOve62        mrud = (mrud[1], mrud[0], mrud[2], mrud[3])
    AlgOve63    } else if decodeTheNextBym() == 0 {
    AlgOve64        // Decode a LONGREP[2].
    AlgOve65        mrud = (mrud[2], mrud[0], mrud[1], mrud[3])
    AlgOve66    } else {
    AlgOve67        // Decode a LONGREP[3].
    AlgOve68        mrud = (mrud[3], mrud[0], mrud[1], mrud[2])
    AlgOve69    }
    AlgOve..
    AlgOve80    len = decodeLen()
    AlgOve..
    AlgOve90    labelDoTheLZCopy:
    AlgOve91    // mrud[0] has been set to what will be (after the emitCopy
    AlgOve92    // call) the most recently used distance. mrud[1] is the 2nd
    AlgOve93    // most recently used, mrud[2] is the 3rd, mrud[3] is the 4th.
    AlgOve94    emitCopy(len, mrud[0])

Conceptually means that some details have been elided. The `State` variable
needs updating after each iteration, but that's not shown. `decodeTheNextBym()`
also depends not only on the `State` but also which line number you're at. The
probabilities used for "is it a LITERAL or NON-LITERAL" are different from the
probabilities used for "is it a LONGREP[2] or LONGREP[3]".

`decodeLen` expands to:

    AlgOveNN.0  if decodeTheNextBym() == 0 {
    AlgOveNN.1      // Decode a low length.
    AlgOveNN.2      len = decodeMultipleByms(3) + 2
    AlgOveNN.3  } else if decodeTheNextBym() == 0 {
    AlgOveNN.4      // Decode a middle length.
    AlgOveNN.5      len = decodeMultipleByms(3) + 10
    AlgOveNN.6  } else {
    AlgOveNN.7      // Decode a high length.
    AlgOveNN.8      len = decodeMultipleByms(8) + 18
    AlgOveNN.9  }


## Range Coding and `decodeTheNextBym`

The codec also tracks two `uint32` variables, called `bits` and `range`, that
represent an interval (a position and width) on the number line between zero
and one. The literal bitstream (the 0s and 1s in the file, read in MSB to LSB
order) is translated into a `bym` stream, where each `bym` is either 0 or 1. If
both `bym` values have equal (50%) probability then the `bym` stream is
basically the same as the bitstream. If symbols have unequal probability then
we can achieve compression: the literal bitstream can be shorter (in terms of
number of bits) then the `bym` stream.

Specifically, if `prob` is the probability (scaled so that `(1 << 11) = 2048`
means 100% probability and so `1024` means 50% probability) that the next `bym`
is 0, and `range` is at least `(1 << 24)`, then (to over-simplify for now)
`decodeTheNextBym()` is:

    threshold = (range >> 11) * prob
    if bits < threshold {
        bym = 0
    } else {
        bym = 1
    }

The probability `prob` depends on the `State` variable as well as additional
context, such as the high bits of the most recent decoded byte and the low bits
of the number of decoded bytes. Implementations maintain an array with hundreds
of contextual probabilities (each a `uint16` in the range `1 ..= 2047`),
indexed by a `prob_index` variable (calculated from the `State` and other
factors). Expanding (marked by ¶) the over-simplified pseudo-code above:

    prob = probs_array[prob_index]             // ¶
    threshold = (range >> 11) * prob
    if bits < threshold {
        bym = 0
    } else {
        bym = 1
    }

LZMA probabilties also adapt. Producing a 0 (or 1) `bym` increases (or
decreases) the relevant contextual probability: the prediction that, the next
time we're in the same context, that next `bym` is 0. Expanding again:

    prob = probs_array[prob_index]
    threshold = (range >> 11) * prob
    if bits < threshold {
        bym = 0
        prob += (2048 - prob) >> 5             // ¶
        probs_array[prob_index] = prob         // ¶
    } else {
        bym = 1
        prob -= prob >> 5                      // ¶
        probs_array[prob_index] = prob         // ¶
    }

We also need to update the `bits` and `range`. The `range` shrinks every time
we decode a `bym` and, if small enough, then grows by reading the next source
byte. Expanding one last time, `decodeTheNextBym()` (which also updates `bits`,
`range` and the relevant contextual probability) is:

    prob = probs_array[prob_index]
    threshold = (range >> 11) * prob
    if bits < threshold {
        bym = 0
        range = threshold                      // ¶
        prob += (2048 - prob) >> 5
        probs_array[prob_index] = prob
    } else {
        bym = 1
        bits -= threshold                      // ¶
        range -= threshold                     // ¶
        prob -= prob >> 5
        probs_array[prob_index] = prob
    }
    if (range >> 24) == 0 {                    // ¶
        bits = (bits << 8) | src.read_byte()   // ¶
        range <<= 8                            // ¶
    }                                          // ¶


## Binary Trees and `decodeMultipleByms`

Decoding N byms (e.g. decoding a literal, a length, a slot or a distance) is
basically N sequential calls to `decodeTheNextBym`, using `((1 << N) - 1)`
different probabilties. This can be visualized as a balanced binary tree with
`((1 << N) - 1)` branch nodes and `(1 << N)` leaf nodes. Each branch node has
its own probability for taking one or the other of its two child nodes.

- There's 1 probability for the first bym.
- There's 2 probabilities for the second bym. The one that is used depends on
  the value of the first bym.
- There's 4 probabilities for the third bym. The one that is used depends on
  the values of the first two byms.
- Etc.
- There's `(1 << (N - 1))` probabilities for the Nth bym. The one that is used
  depends on the values of all previous byms.
- There's `(1 << N)` leaf nodes, one for each possible N-bit value.

These probabilities can be flattened to a `(1 << N)` element array, whose first
element is unused (or available for repurposing, such as for what the reference
implementation calls the "choice" probabilities). For example, if N is 3 then
the 8-element array could be labeled as `u1223333`, where `u` is unused, `1`
marks the probability for the first bym, `2` marks the probabilities for the
second bym, etc.

Algorithmically, we loop N times, walking the tree from the root to a leaf. The
final N-bit value is captured in the tree node number (after applying a bit
mask or, equivalently, a subtraction):

    tree_node = 1
    while tree_node < (1 << N) {
        bym = decodeTheNextBym()  // Using prob_index = tree_node.
        tree_node = (tree_node << 1) | bym
    }
    tree_node &= ((1 << N) - 1)  // Equivalent to "tree_node -= (1 << N)".

That's a "forward" binary tree, where the bits occur in MSB to LSB order. A
"reverse" binary tree uses the opposite order.


## Further Reading

A longer discussion of LZMA compression, including range coding and the XZ
container format, is at [XZ/LZMA Worked
Example](https://nigeltao.github.io/blog/2024/xz-lzma-part-1-range-coding.html).
