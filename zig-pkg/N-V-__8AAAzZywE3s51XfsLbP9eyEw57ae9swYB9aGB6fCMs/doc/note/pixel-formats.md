# Pixel Formats

`wuffs_base__pixel_format` is a `uint32_t` that encodes the format of the bytes
that constitute an image frame's pixel data. Its bits:

- Bits `28 ..= 31` encodes color (and channel order, in terms of memory).
- Bits `26 ..= 27` are reserved.
- Bits `24 ..= 25` encodes transparency.
- Bits `21 ..= 23` are reserved.
- Bit         `20` indicates big-endian/MSB-first (instead of little/LSB).
- Bit         `19` indicates floating point (instead of integer).
- Bit         `18` indicates palette-indexed. The number-of-planes (the next
                   field) will be 0, as the format is considered interleaved,
                   but the 8-bit N-BGRA color data is stored in plane 3.
- Bits `16 ..= 17` are the number of planes, minus 1. Zero means interleaved.
- Bits `12 ..= 15` encodes the number of bits (depth) in the 3rd channel.
- Bits  `8 ..= 11` encodes the number of bits (depth) in the 2nd channel.
- Bits  `4 ..=  7` encodes the number of bits (depth) in the 1st channel.
- Bits  `0 ..=  3` encodes the number of bits (depth) in the 0th channel.

The bit fields of a `wuffs_base__pixel_format` are not independent. For
example, the number of planes should not be greater than the number of
channels. Similarly, bits `4 ..= 15` are unused (and should be zero) if bits
`24 ..= 31` (color and transparency) together imply only 1 channel (e.g. gray,
no alpha) and floating point samples should mean a bit depth of 16, 32 or 64.

Formats hold between 1 and 4 channels. For example: Y (1 channel: gray), YA (2
channels: gray and alpha), BGR (3 channels: blue, green, red) or CMYK (4
channels: cyan, magenta, yellow, black). BGRX holds 4 channels even though the
X-padding channel's values are ignored.

For direct formats with N > 1 channels, those channels can be laid out in
either 1 (interleaved) or N (planar) planes. For example, RGBA data is usually
interleaved, but YCbCr data is usually planar, due to [chroma
subsampling](/doc/note/pixel-subsampling.md).

For indexed formats, the palette (always 256 × 4 bytes) holds 8 bits per
channel non-alpha-premultiplied BGRA color data. There is only 1 plane (for the
index), as the format is considered interleaved. Plane 0 holds the per-pixel
indices. Plane 3 is re-purposed to hold the per-index colors.

Color is encoded in 4 bits:

-  0 means          A      (Alpha).
-  2 means Y     or YA     (Gray, Alpha).
-  4 means YCbCr or YCbCrA (Luma, Chroma-blue, Chroma-red, Alpha).
-  5 means          YCbCrK (Luma, Chroma-blue, Chroma-red, Black).
-  6 means YCoCg or YCoCgA (Luma, Chroma-orange, Chroma-green, Alpha).
-  7 means          YCoCgK (Luma, Chroma-orange, Chroma-green, Black).
-  8 means BGR   or BGRA   (Blue, Green, Red, Alpha).
-  9 means          BGRX   (Blue, Green, Red, X-padding).
- 10 means RGB   or RGBA   (Red, Green, Blue, Alpha).
- 11 means          RGBX   (Red, Green, Blue, X-padding).
- 12 means CMY   or CMYA   (Cyan, Magenta, Yellow, Alpha).
- 13 means          CMYK   (Cyan, Magenta, Yellow, Black).
- all other values are reserved.

In Wuffs, channels are given in memory order (also known as byte order),
regardless of endianness, since the C type for the pixel data is an array of
bytes, not an array of `uint32_t`. For example, interleaved BGRA with 8 bits
per channel means that the bytes in memory are always Blue, Green, Red then
Alpha. On big-endian systems, that is the `uint32_t 0xBBGGRRAA`. On
little-endian, `0xAARRGGBB`.

Transparency is encoded in 2 bits:

- 0 means no alpha channel, fully opaque.
- 1 means an alpha channel, other channels are non-premultiplied.
- 2 means an alpha channel, other channels are     premultiplied.
- 3 means an alpha channel, binary alpha.

Binary alpha means that if a color is not completely opaque, it is completely
transparent black. As a source pixel format, it can therefore be treated as
either non-premultiplied or premultiplied.

The zero `wuffs_base__pixel_format` value is an invalid pixel format, as it is
invalid to combine the zero color (alpha only) with the zero transparency.

Bit depth is encoded in 4 bits:

-  0 means the channel or index is unused.
-  x means a bit depth of  x, for x in the range `1 ..= 8`.
-  9 means a bit depth of 10.
- 10 means a bit depth of 12.
- 11 means a bit depth of 16.
- 12 means a bit depth of 24.
- 13 means a bit depth of 32.
- 14 means a bit depth of 48.
- 15 means a bit depth of 64.

For example, the value `wuffs_base__pixel_format` `0x80000565` means BGR with
no alpha or padding, 5/6/5 bits for blue/green/red, interleaved 2 bytes per
pixel, laid out LSB-first in memory order:

```
ptr+0...........  ptr+1...........
MSB          LSB  MSB          LSB
G₂G₁G₀B₄B₃B₂B₁B₀  R₄R₃R₂R₁R₀G₅G₄G₃
```

On little-endian systems (but not big-endian), this Wuffs pixel format value
(`0x40000565`) corresponds to the Cairo library's `CAIRO_FORMAT_RGB16_565`, the
SDL2 (Simple DirectMedia Layer 2) library's `SDL_PIXELFORMAT_RGB565` and the
Skia library's `kRGB_565_SkColorType`. Note BGR in Wuffs versus RGB in the
other libraries.

Regardless of endianness, this Wuffs pixel format value (`0x40000565`)
corresponds to the V4L2 (Video For Linux 2) library's `V4L2_PIX_FMT_RGB565` and
the Wayland-DRM library's `WL_DRM_FORMAT_RGB565`.

Different software libraries name their pixel formats (and especially their
channel order) either according to memory layout or as bits of a native integer
type like `uint32_t`. The two conventions differ because of a system's
endianness. As mentioned earlier, Wuffs pixel formats are always in memory
order. More detail of other software libraries' naming conventions is in
Alexandros Frantzis' [Pixel Format
Guide](https://afrantzis.github.io/pixel-format-guide/).


## API Stability

The `wuffs_base__pixel_format` bit packing is documented for explanation and to
assist in debugging (e.g. `printf`ing the bits in `%x` format). However, do
not manipulate its bits directly; they are private implementation details. Use
functions such as `wuffs_base__pixel_format__num_planes` instead.
