# Naïve Image Formats (NIE, NII, NIA) Related Work

This is a companion document to the [Naïve Image Formats (NIE, NII, NIA)
Specification](/doc/spec/nie-spec.md). Its contents aren't necessary for
implementing the NIE file format, but gives further context on what NIE is and
is not.

In general, being uncompressed, NIE/NII/NIA is not intended for long term
storage or for delivery over a network. Alternative lossless image formats like
[PNG](https://www.w3.org/TR/2003/REC-PNG-20031110/) or [WebP
Lossless](https://developers.google.com/speed/webp/docs/riff_container) are far
more suitable.

Amongst uncompressed formats, [Farbfeld](https://tools.suckless.org/farbfeld/)
is a very similar design. The main difference is that NIE is little-endian (the
same as the vast majority of CPUs) where Farbfeld is big-endian. NIE/NII/NIA
also allows animated images, not just still images, and is also configurable:
non-premultiplied versus premultiplied alpha and 8- versus 16-bit channels.
Configurability is admittedly a trade-off. One person's configuration parameter
is another person's unnecessary bloat. We're not saying that Farbfeld is a bad
design, just a different design that has chosen different trade-offs.

[PAM](http://netpbm.sourceforge.net/doc/pam.html) (Portable Arbitrary Map) and
its related formats (PBM, PGM, PPM) are also very similar. The main difference
is that their headers are ASCII text and of variable length. Such headers are
easier for humans to read but harder for computers to parse. Again, NIE/NII/NIA
also allows animated images. Some programs process concatenated streams of PAM
images as an animation, but that doesn't contain explicit timing information.

[VIPS](https://jcupitt.github.io/libvips/API/current/file-format.html) is the
intermediate format for the [libvips](https://libvips.github.io/libvips/) image
processing library. Pixel data is stored in a straightforward fashion, but
metadata is stored as XML, which is not simple, and is a potential security
concern.

Amongst flexible formats,
[BMP](https://www.fileformat.info/format/bmp/egff.htm),
[PNG](https://www.w3.org/TR/2003/REC-PNG-20031110/) and its animated variant
[APNG](https://wiki.mozilla.org/APNG_Specification),
[Targa](http://tfc.duke.free.fr/coding/tga_specs.pdf) and
[TIFF](http://partners.adobe.com/public/developer/en/tiff/TIFF6.pdf) all have
their built-in compression being optional. Some of those uncompressed forms
have been used as simple interchange formats. Compared to those, NIE has fewer
features, which is obviously unfavorable if you want such features, but can be
favorable if you prefer simplicity and code auditability. A NIE file is still
simpler than an uncompressed BMP file and a NIA file is still simpler than an
uncompressed APNG file.

For example, when using a sandboxed worker process to convert to an
intermediate format, we don't want to use "all possible valid TIFF images" as
the intermediate format, because a compromised worker process could generate a
malicious intermediate image that provoked a security bug in an unsandboxed
TIFF decoder. Instead, we would want only a very simple subset of TIFF, but it
can be easier (in terms of implementation and in terms of a code audit) to
write an (almost trivial) NIE parser from scratch instead of whittling down a
full-featured TIFF library and hoping that you've eliminated all the potential
security vulnerabilities.
