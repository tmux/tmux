# Naïve Image Formats: NIE, NII, NIA

Status: Draft (as of November 2021). There is no compatibility guarantee yet.

A companion document has further discussion of [NIE related
work](/doc/spec/nie-related-work.md).


## NIE: Still Images

NIE is an easily parsed, uncompressed, lossless format for still (single frame)
images. The 16 byte header:

- 4 bytes of 'magic': \[0x6E, 0xC3, 0xAF, 0x45\], the UTF-8 encoding of "nïE".
- 4 bytes of version-and-configuration: \[0xFF, 0x62, 0x6E or 0x70, 0x34 or
  0x38\].
  - The first byte denotes the overall NIE/NII/NIA format version. 0xFF (which
    is not valid UTF-8) denotes version 1. There are no other valid versions at
    this time.
  - The second byte must be an ASCII 'b'. This denotes that the payload is in
    BGRA order (not RGBA), in terms of the wire format, independent of CPU
    endianness.
  - The third byte, either an ASCII 'n' or an ASCII 'p', denotes whether the
    payload contains non-premultiplied or premultiplied alpha.
  - The fourth byte, either an ASCII '4' or an ASCII '8', denotes whether there
    are 4 or 8 bytes per pixel.
  - Future format versions may allow other byte values, but in version 1, it
    must be '\xFF', then 'b', then 'n' or 'p', then '4' or '8'.
- 4 bytes little-endian `uint32` width. The high bit must not be set.
- 4 bytes little-endian `uint32` height. The high bit must not be set.

The payload:

- 4 or 8 bytes per pixel. W×H pixels in row-major order (horizontally adjacent
  pixels are adjacent in memory). For example, with 8 bytes per pixel, those
  bytes are \[B₀, B₁, G₀, G₁, R₀, R₁, A₀, A₁\]. The ₀ and ₁ subscripts denote
  the low and high bytes of each little-endian `uint16`.

That's it.


### Example NIE File

This still image is 3 pixels wide and 2 pixels high. It is a crude
approximation to the French flag, being three columns: blue, white and red.

    00000000  6e c3 af 45 ff 62 6e 34  03 00 00 00 02 00 00 00  |n..E.bn4........|
    00000010  ff 00 00 ff ff ff ff ff  00 00 ff ff ff 00 00 ff  |................|
    00000020  ff ff ff ff 00 00 ff ff                           |........|


## NII: Animated Images, Timing Index Only, Out-of-Band Frames

NII is an index for animated (multiple frame) images. In video compression
terminology, every NII frame is an I-frame, also known as a keyframe. A NII
file doesn't contain the images per se, only the duration that each frame
should be shown.

The per-frame images, not part of a NII file, may be NIE images, but they may
also be in other formats, such as PNG or WebP or a heterogenous mixture. They
may be static files (possibly with systematic filenames such as
`frame000000.png`, `frame000001.png`, etc.) or dynamically generated. That is
for each application to decide, and out of scope of this specification.

A NII file consists of a 16 byte header, a variable sized payload and an 8 byte
footer.


### NII Header

The 16 byte NII header:

- 4 bytes of 'magic': \[0x6E, 0xC3, 0xAF, 0x49\]. The final byte differs from
  NIE: an ASCII 'I' instead of an ASCII 'E'.
- 4 bytes of version-and-padding, all 0xFF.
- 4 bytes little-endian `uint32` width. The high bit must not be set.
- 4 bytes little-endian `uint32` height. The high bit must not be set.


### NII Payload

The payload is a sequence of 0 or more frames, exactly 8 bytes (a little-endian
`uint64`) per frame:

- The most significant bit is 0.
- The low 63 bits are the cumulative display duration (CDD). This is the amount
  of time, relative to the start of the animation, at which display should
  proceed to the next frame.

Every frame's CDD must be greater than or equal to the previous frame's CDD (or
for the first frame, greater than or equal to zero, which will always be true).

For example, if an animation has four frames, to be displayed for 1 second, 2
seconds, 0 seconds and finally 4.5 seconds, then the CDD's are 1s, 3s, 3s and
7.5s. NII's unit of time is [flicks](https://github.com/OculusVR/Flicks): one
flick (frame-tick) is 1 / 705\_600\_000 of a second. Continuing our example,
the CDDs (in decimal and then hexadecimal) are:

- 705\_600\_000 × 1.0 = 0\_705\_600\_000 = 0x0000\_0000\_2A0E\_9A00.
- 705\_600\_000 × 3.0 = 2\_116\_800\_000 = 0x0000\_0000\_7E2B\_CE00.
- 705\_600\_000 × 3.0 = 2\_116\_800\_000 = 0x0000\_0000\_7E2B\_CE00.
- 705\_600\_000 × 7.5 = 5\_292\_000\_000 = 0x0000\_0001\_3B6D\_8300.

Animations lasting `(1<<63)` or more flicks, more than 400 years, are not
representable in the NII format.


### NII Footer

The 8 byte NII footer:

- 4 bytes little-endian `uint32` LoopCount.
- 4 bytes: \[0x00, 0x00, 0x00, 0x80\].

A zero LoopCount means that the animation loops forever. Non-zero means that
the animation is played LoopCount times and then stops. This is the
[APNG](https://wiki.mozilla.org/APNG_Specification) meaning, not the
[GIF](https://www.w3.org/Graphics/GIF/spec-gif89a.txt) meaning (the number of
times to repeat the loop _after_ the first play). The two meanings differ by 1.


### Example NII File

This animated image is 3 pixels wide and 2 pixels high. It consists of 20
frames, being 10 loops of 2 frames. The total animation time of a single loop
is 3 seconds, so the 10 loops will take 30 seconds. The first frame is shown
for 1 second. The next frame is shown for (3 - 1) seconds (i.e., 2 seconds).
The actual pixel data per frame is stored elsewhere.

    00000000  6e c3 af 49 ff ff ff ff  03 00 00 00 02 00 00 00  |n..I............|
    00000010  00 9a 0e 2a 00 00 00 00  00 ce 2b 7e 00 00 00 00  |...*......+~....|
    00000020  0a 00 00 00 00 00 00 80                           |........|


## NIA: Animated Images, In-band Frames

NIA is like a NII file where the per-frame still images are NIE files
interleaved between the NII payload values.

The NIA header is the same as the 16 byte NII header, except that the 4 byte
'magic' ends in an ASCII 'A' instead of an ASCII 'I', and the 5th to 8th bytes
are version-and-configuration (the same as for NIE), instead of NII's
version-and-padding. The range of valid version-and-configuration bytes is the
same for NIA as it is for NIE.

The NIA footer is the same as the 8 byte NII footer.

The payload is a sequence of 0 or more frames. Each frame is:

- 8 bytes little-endian `uint64` value, the same meaning and constraints as a
  NII payload value.
- A complete NIE image: header and payload. The outer NIA and inner NIE
  must have the same 12 bytes of version-and-configuration, width and height.
- Either 0 or 4 bytes of padding. If present, it must be all zeroes. The
  padding ensures that the size of the padded NIE image a multiple of 8 bytes,
  so that every CDD field is 8 byte aligned. The padding size is 4 if and only
  if there are 4 (not 8) bytes per pixel and both the width and height are odd.
  A C programming language expression for its presence is `((bytes_per_pixel ==
  4) && (width & height & 1))`.


### Example NIA File

This animated image is 3 pixels wide and 2 pixels high. It consists of 20
frames, being 10 loops of 2 frames. The total animation time of a single loop
is 3 seconds, so the 10 loops will take 30 seconds. The first frame is a crude
approximation to the French flag (blue, white and red) and is shown for 1
second. The next frame is a crude approximation to the Italian flag (green,
white and red) and is shown for (3 - 1) seconds (i.e., 2 seconds).

    00000000  6e c3 af 41 ff 62 6e 34  03 00 00 00 02 00 00 00  |n..A.bn4........|
    00000010  00 9a 0e 2a 00 00 00 00  6e c3 af 45 ff 62 6e 34  |...*....n..E.bn4|
    00000020  03 00 00 00 02 00 00 00  ff 00 00 ff ff ff ff ff  |................|
    00000030  00 00 ff ff ff 00 00 ff  ff ff ff ff 00 00 ff ff  |................|
    00000040  00 ce 2b 7e 00 00 00 00  6e c3 af 45 ff 62 6e 34  |..+~....n..E.bn4|
    00000050  03 00 00 00 02 00 00 00  00 ff 00 ff ff ff ff ff  |................|
    00000060  00 00 ff ff 00 ff 00 ff  ff ff ff ff 00 00 ff ff  |................|
    00000070  0a 00 00 00 00 00 00 80                           |........|


# Commentary


## Motivation

One motivating example is securely decoding untrusted images, perhaps uploaded
from potentially malicious actors. Codec libraries have been a rich source of
software security vulnerabilities in the past. One response is to split off
such code into a separate, sandboxed process that reads the compressed image
and writes the equivalent NIE/NIA image, perhaps through pipes or shared
memory. The untrusted codec library processes the untrusted data within the
sandboxed worker process. The unsandboxed manager process only needs to handle
the much simpler NIE/NIA format. That format can be further simplified by the
manager mandating a fixed version-and-configuration, such as v1-"bn4".

Another example is connecting a series of independent image manipulation
programs, each component reading, transforming and then writing a NIE/NIA
image. Such filters can be written in simple programming languages and
connected with Unix-style pipes.

Another example is storing 'golden images' (or their hashes) for codec
development. Given a corpus of test images in a compressed format (e.g. a
corpus of PNG files), it is useful to store their expected decodings for
comparison, but those golden test files should be encoded in an alternative
format, such as NIE/NIA.


## Magic

The 4 bytes of 'magic' are the UTF-8 encoding of the non-ASCII strings "nïE",
"nïI" and "nïA". The unusual capitalization lessens the chance of plain text
data accidentally matching these magic bytes.


## Alpha Premultiplication

For premultiplied alpha, it is valid for a pixel's blue, green or red values to
be greater than its alpha value. Interpretation of such super-saturated colors
is out of scope of this specification.

A program that simply extracts a subset of a NIA's frames as a new NIA
animation is not required to examine or re-encode every payload byte in order
to always output valid NIA data.


## Random Access

Given a NIA animation's bytes per pixel, width and height _B_, _W_ and _H_, the
offset and length of the _i_'th frame's NIE data within that NIA is a simple
computation (but remember to check for overflow):

- length = roundup8(_B_ × _W_ × _H_) + 16
- offset = ((length + 8) × _i_) + 24

The roundup8 function rounds its argument up to the nearest multiple of 8.

This is random access by frame index (the "_i_" in "the _i_'th frame"), not by
time, as different frames can have different display durations.


## Degenerate Animations

A still image is, in some sense, an animated image with a single frame, albeit
without an explicit display duration or looping behavior. Some animated image
formats also support zero frames, just like the empty string being a valid
string. For these degenerate (0 or 1 frame) cases, when converting to NII or
NIA, the convention (but not requirement) is a zero CDD and a zero LoopCount.


## Cumulative Display Duration

Other animation formats, like
[APNG](https://wiki.mozilla.org/APNG_Specification) and
[GIF](https://www.w3.org/Graphics/GIF/spec-gif89a.txt), provide display
durations relative to the previous frame, not relative to the initial frame.
The two schemes are equivalent, in that from a complete stream, either one can
be derived from the other. NII / NIA frames report the cumulative number so
that random access by time can be implemented as a binary search, given random
access by frame index.

Suppose we are given a time _t_ ≥ 0 and want to find the frame to show at that
time. First, there may be no such frame, if the animation contains no frames.

Otherwise, let _o_ be the final frame's CDD, so that _o_ ≥ 0. If _o_ is zero,
the frame to show is the final frame of the animation, and no further
computation is necessary.

Otherwise, calculate the number of loops that would complete by time t: _n_ =
_t_ / _o_, rounding down to the nearest integer. If the LoopCount is non-zero
(as zero means loop forever) and _n_ ≥ LoopCount then the frame to show is the
final frame.

Otherwise, calculate _t′_ = _t_ - (_n_ × _o_), the time 'modulo' _o_. Binary
search to find the smallest _i_ ≥ 0 such that both CDD(_i_) > _t′_ and the
_i_th frame is non-instantaneous. CDD(_i_) is the cumulative display duration
for frame _i_. The first frame is instantaneous if its CDD is zero. Any other
frame is instantaneous if its CDD equals its previous frame's CDD.


## Integer Arithmetic Overflow

Parsing NIE data is almost trivial, but care should be taken to avoid integer
arithmetic overflow when calculating the pixel buffer size from fields in the
NIE header. For example, a C programming language statement like `size_t
row_size = bytes_per_pixel * width;` is incorrect without additional prior
checks. A careful C implementation is:

```c
#include <stdbool.h>
#include <stdint.h>

// nie_payload_size calculates the size in bytes of a NIE payload, given the
// metadata from the NIE header: bytes_per_pixel, width and height. The max
// argument, not defined in the metadata, is the caller's maximum acceptable
// payload size. For example, pass SIZE_MAX for max, or pass a smaller value if
// you wish to limit the memory required to decode an arbitrary NIE file (and
// reject otherwise valid NIE files that would require more memory).
//
// That size is essentially (bytes_per_pixel * width * height), but this
// function checks for integer arithmetic overflow. It also checks that the
// result pointer is non-NULL, the result (the calculated payload size) is less
// than or equal to max, and that the bytes_per_pixel is either 4 or 8.
//
// The bool return value is whether all checks pass. On success, it sets
// *result to the payload size.
bool nie_payload_size(size_t* result,
                      size_t max,
                      uint32_t bytes_per_pixel,
                      uint32_t width,
                      uint32_t height) {
  if ((result == NULL) || ((bytes_per_pixel != 4) && (bytes_per_pixel != 8))) {
    return false;
  }
  uint64_t n = ((uint64_t)width) * ((uint64_t)height);

  // bpp_shift is 2 or 3, depending on bytes_per_pixel being 4 or 8.
  uint32_t bpp_shift = 2 + (bytes_per_pixel >> 3);
  if (n > (max >> bpp_shift)) {
    return false;
  }
  n <<= bpp_shift;

  *result = (size_t)n;
  return true;
}
```


## Color Management and Other Metadata

There is no facility for describing color spaces, gamma, palettes or other
metadata such as EXIF information. For example, when using a sandboxed worker
process to convert from a PNG image (with an embedded color profile) to a NIE
image, the target color space should be provided to the worker out-of-band.


## YUV Color

There is no facility for explicitly describing YUV or Y'CbCr color. Converting
between NIE/NIA and formats such as
[JPEG](http://www.w3.org/Graphics/JPEG/itu-t81.pdf) or [WebP
Lossy](https://developers.google.com/speed/webp/docs/riff_container) is a lossy
process, although JPEG and WebP Lossy are lossy formats to begin with.


## Bytes versus Octets

It was not always the case, historically, but in this specification, `byte` is
synonymous with `octet` and `uint8`.


# Filename Extensions and MIME Types

The recommended filename extensions are `.nie`, `.nii` and `.nia`.

The recommended MIME types are `image/x-nie`, `image/x-nii` and `image/x-nia`.


## Why NIE and not NIF (for Naïve Image Format)?

The `.nif` filename extension is already used by the NetImmerse / Gamebryo game
engine. Instead, you can think of `.nie` as derived from the word "naïve".


## Pronunciation

I pronounce "NIE", "NII" and "NIA" as /naɪˈiː/, /naɪˈaɪ/  and /naɪˈeɪ/, ending
in a long "E", "I" or "A" sound. It's definitely a hard "N", not a soft one.


---

Updated on January 2022.
