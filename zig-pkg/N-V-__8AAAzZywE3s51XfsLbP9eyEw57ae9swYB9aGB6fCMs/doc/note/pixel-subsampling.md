# Pixel Subsampling

`wuffs_base__pixel_subsampling` is a `uint32_t` that encodes whether samples
cover one pixel or cover multiple pixels. RGBA or BGRA pixel formats are
typically one pixel per sample. For YCbCr pixel formats (i.e. the native
formats of JPEG and WebP lossy images), the luma channel (Y) is also typically
one pixel per sample but the chroma channels (Cb, Cr) are often 2 or 4 pixels
per sample. In JPEG terminology, 4:4:4, 4:2:2 or 4:2:0 subsampling correspond
to 1, 2 or 4 pixels per [chroma
sample](https://en.wikipedia.org/wiki/Chroma_subsampling).

Equivalently, `wuffs_base__pixel_subsampling` encodes the mapping of pixel
space coordinates `(x, y)` to sample space coordinates `(i, j)`, also known as
pixel buffer indices. That mapping can differ for each plane `p`. A Wuffs image
can have up to 4 planes or channels. For a depth of 8 bits (1 byte), the `p`'th
plane's sample starts at `(planes[p].ptr + (j * planes[p].stride) + i)`.

For interleaved pixel formats, which are always one pixel per sample, the
mapping is trivial: `i = x` and `j = y`. For planar pixel formats, the mapping
can differ due to chroma subsampling. For example, consider a three plane YCbCr
pixel format with 4:2:2 subsampling. For the luma (Y) channel, there is one
sample for every pixel, but for the chroma (Cb, Cr) channels, there is one
sample for every two pixels: pairs of horizontally adjacent pixels form one
macropixel, `i = x / 2` and `j == y`. In general, for a given `p`:

- `i = (x + bias_x) / denominator_x`.
- `j = (y + bias_y) / denominator_y`.

where biases and denominators are in the range `0 ..= 3` and `1 ..= 4`
respectively.

In general, the biases will be zero after decoding an image. However, making
a sub-image may change the bias, since the `(x, y)` coordinates are relative
to the sub-image's top-left origin, but the backing pixel buffers were
created relative to the original image's origin.

For example, consider a 10×3 image with what JPEG calls 4:1:1 subsampling,
where each Chroma sample covers a macropixel (a block of pixels) 4 wide and 1
high. For a Chroma channel, there are 30 pixels and 9 samples. `bias_x = 0` and
`denominator_x = 2`, so that the 6th pixel column (`x = 5`) maps to the 2nd
sample column (`i = 1`).

```
Pixel space:
+---------------------------------------+
|l00 l01 l02 l03 m04 m05 m06 m07 n08 n09|
|                                       |
|p10 p11 p12 p13 q14 q15 q16 q17 r18 r19|
|           +-----------+               |
|t21 t21 t22|t23 u24 u25|u26 u27 v28 v29|
+-----------+-----------+---------------+

Chroma sample space:
+-----+
|l m n|
|     |
|p q r|
|     |
|t u v|
+-----+
```

Now consider the 3×1 sub-image shown above. Even though its pixel width (3) is
less than the macropixel width (4 columns per sample), as that sub-image shares
the sample buffer with its containing image, it still straddles 2 sample
columns (`t` and `u`). `denominator_x = 2`, the same as before, but now `bias_x
= 3`.

```
Pixel space:
+-----------+
|t23 u24 u25|
+-----------+

Chroma sample space:
+---+
|t u|
+---+
```


## Bit Packing

For each plane `p`, the adjusted denominator is defined to be one less than the
denominator. The plane's four numbers (two biases and two adjusted
denominators) are each in the range `0 ..= 3`, and are each encodable in two
bits. Four groups of two bits combine to form an 8 bit unsigned integer:

```
e_p = (bias_x << 6) | (adj_denom_x << 4) | (bias_y << 2) | (adj_denom_y << 0)
```


Those `e_p` values (`e_0` for the first plane, `e_1` for the second plane, etc)
combine to form a `wuffs_base__pixel_subsampling` value:

```
pixsub = (e_3 << 24) | (e_2 << 16) | (e_1 << 8) | (e_0 << 0)
```

For example, visualizing 4:2:2 YCbCr (with no bias):

```
Pixel space:
+-------------------+
|l00 l01 m02 m03 n04|
|                   |
|p10 p11 q12 q13 r14|
+-------------------+

Chroma sample space:
+-----+
|l m n|
|     |
|p q r|
+-----+
```

This corresponds to a `denominator_x` of 1, 2 and 2 for the three planes (Y,
Cb, Cr) and a `denominator_y` of 1, 1 and 1. The `uint32_t` encoding is
`0x101000`.


## API Stability

The `wuffs_base__pixel_subsampling` bit packing is documented for explanation
and to assist in debugging (e.g. `printf`ing the bits in `%x` format).
However, do not manipulate its bits directly; they are private implementation
details. Use functions such as `wuffs_base__pixel_subsampling__bias_x` instead.
