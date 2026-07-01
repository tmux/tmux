As per [the PngSuite page](http://www.schaik.com/pngsuite/#corrupted), every
`x*.png` file in the test suite is invalid PNG.

Nonetheless, `nia-checksums-of-pngsuite.txt` contains an entry for
`xcsn0g01.png`, meaning that `convert-to-nia` decoded *some* pixels for it.
This is working as intended, because the `xcsn0g01.png` invalidity is in its
`IDAT` chunk's checksum (compared to `xhdn0g08.png`'s corruption in its `IHDR`
chunk's checksum).

The `convert-to-nia` semantics are that, provided that it can decode a complete
image configuration (width, height, pixel format, etc which, for PNG, comes
from the `IHDR` chunk), it outputs *as many pixels as possible* (even if that's
zero pixels) so that it gives a 'best effort' decoding for truncated images.
Indeed, the NIA conversion for `xcsn0g01.png` holds all-zero pixels (and a
"png: bad checksum" error message on stderr) but also a zero exit code.

```
$ gen/bin/example-convert-to-nia < test/3pdata/pngsuite/xcsn0g01.png | hd
00000000  6e c3 af 41 ff 62 6e 34  20 00 00 00 20 00 00 00  |n..A.bn4 ... ...|
00000010  00 00 00 00 00 00 00 00  6e c3 af 45 ff 62 6e 34  |........n..E.bn4|
00000020  20 00 00 00 20 00 00 00  00 00 00 00 00 00 00 00  | ... ...........|
00000030  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00001020  00 00 00 00 00 00 00 00                           |........|
00001028
png: bad checksum
$ echo $?
0
```
