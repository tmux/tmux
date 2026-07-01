// Copyright 2023 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

/*
toy-aux-image demonstrates using the wuffs_aux::DecodeImage C++ function to
decode an in-memory compressed image. In this example, the compressed image is
hard-coded to a specific image: a JPEG encoding of the first frame of the
test/data/muybridge.gif animated image.

To run:

$CXX toy-aux-image.cc && ./a.out; rm -f a.out

for a C++ compiler $CXX, such as clang++ or g++.

The expected output:

@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@@@@@@@@@@@@X@@@@XX@@@@@@@@@@X
XXXXX@@XXX@@@@@@@II@@@X@X@@@@@
XXXXX@@XX@@X@@@XO+XXX@XX@@@X@@
XXXXXXXX@XX@X@XI=I@@XXI+OXX@XX
XXXXXXXXXXXXXXX+=+OXO+=::OXX@X
XXXXXXXXXXXXXXXXXX=+==:::=XXXX
XXXXXXXXO+:::::+OO+===+OI=+XXX
XXXO::=++:::==+++XI+++X@XXO@XX
XXXO=X@X+::=::::+O++=I@XX@XXXX
XXXXX@XXX=:::::::::=+@XXXX@XXX
XXXXXXXX@O::IXO=::::O@@XXXXXXX
XXXXXXXXO=X+X@@XX::O@@XXXXXXXX
XXXXXXXXXOO=X@X@X+OIXXXXXXXXXX
XXXXXXXXXXX+IIXX+X@OX@XXXXXXXX
XXXXXXXXX@XXOI+IIOOOXXXXXXXXXX
XXXXXXXXXXX@XXXXX@XXXXXXXXXXXX
XXXXXXXXXXXXXXXXX@XXXXXXXXXXXX
OOOOXXXXXXXXXXOXXXXXXXXXXXXOOO
=+++IIIIIIIOOOOOOOOOOIIIIIIII+
*/

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__STATIC_FUNCTIONS macro is optional, but when
// combined with WUFFS_IMPLEMENTATION, it demonstrates making all of Wuffs'
// functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#define WUFFS_CONFIG__STATIC_FUNCTIONS

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__AUX__BASE
#define WUFFS_CONFIG__MODULE__AUX__IMAGE
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__JPEG

// Defining the WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST (and the
// associated ETC__ALLOW_FOO) macros are optional, but can lead to smaller
// programs (in terms of binary size). By default (without these macros),
// Wuffs' standard library can decode images to a variety of pixel formats,
// such as BGR_565, BGRA_PREMUL or RGBA_NONPREMUL. The destination pixel format
// is selectable at runtime. Using these macros essentially makes the selection
// at compile time, by narrowing the list of supported destination pixel
// formats. The FOO in ETC__ALLOW_FOO should match the pixel format passed (as
// part of the wuffs_base__image_config argument) to the decode_frame method.
//
// If using the wuffs_aux C++ API, without overriding the SelectPixfmt method,
// the implicit destination pixel format is BGRA_PREMUL.
#define WUFFS_CONFIG__DST_PIXEL_FORMAT__ENABLE_ALLOWLIST
#define WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_BGRA_PREMUL

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

// g_src_array and g_src_len hold a grayscale JPEG image of a galloping horse.
//
// $ convert 'test/data/muybridge.gif[0]' -colorspace gray x.jpeg
// $ hd x.jpeg
// 00000000  ff d8 ff e0 00 10 4a 46 49 46 00 01 01 00 00 01 |......JFIF......|
// 00000010  00 01 00 00 ff db 00 43 00 03 02 02 02 02 02 03 |.......C........|
// 00000020  02 02 02 03 03 03 03 04 06 04 04 04 04 04 08 06 |................|
// etc
// 00000200  53 04 29 20 5a 26 25 41 d3 44 75 ee fa 37 07 d5 |S.) Z&%A.Du..7..|
// 00000210  fd 40 67 91 55 49 14 49 6c 55 53 af 14 ed e7 fd |.@g.UI.IlUS.....|
// 00000220  6e 3f ff d9                                     |n?..|
// 00000224
uint8_t g_src_array[] = {
    0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x43,
    0x00, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03, 0x02, 0x02, 0x02, 0x03,
    0x03, 0x03, 0x03, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x06,
    0x06, 0x05, 0x06, 0x09, 0x08, 0x0a, 0x0a, 0x09, 0x08, 0x09, 0x09, 0x0a,
    0x0c, 0x0f, 0x0c, 0x0a, 0x0b, 0x0e, 0x0b, 0x09, 0x09, 0x0d, 0x11, 0x0d,
    0x0e, 0x0f, 0x10, 0x10, 0x11, 0x10, 0x0a, 0x0c, 0x12, 0x13, 0x12, 0x10,
    0x13, 0x0f, 0x10, 0x10, 0x10, 0xff, 0xc0, 0x00, 0x0b, 0x08, 0x00, 0x14,
    0x00, 0x1e, 0x01, 0x01, 0x11, 0x00, 0xff, 0xc4, 0x00, 0x18, 0x00, 0x01,
    0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x07, 0x08, 0x03, 0x05, 0x06, 0xff, 0xc4, 0x00, 0x2e,
    0x10, 0x00, 0x02, 0x01, 0x04, 0x00, 0x05, 0x01, 0x05, 0x09, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x11,
    0x00, 0x07, 0x08, 0x12, 0x21, 0x13, 0x22, 0x31, 0x41, 0x51, 0x61, 0x14,
    0x17, 0x23, 0x32, 0x42, 0x52, 0x71, 0x81, 0x91, 0xff, 0xda, 0x00, 0x08,
    0x01, 0x01, 0x00, 0x00, 0x3f, 0x00, 0xbd, 0x63, 0xbc, 0x2a, 0x15, 0x49,
    0x6a, 0x02, 0xbb, 0x0d, 0x80, 0xcc, 0x01, 0x3f, 0x5d, 0x71, 0xc7, 0x5f,
    0x3a, 0x80, 0xe5, 0x85, 0x92, 0x5a, 0xeb, 0x5a, 0xf3, 0x0b, 0x18, 0x9a,
    0xf7, 0x48, 0x1a, 0x35, 0xb5, 0xc9, 0x7c, 0xa5, 0x82, 0x67, 0x9f, 0x5e,
    0xcc, 0x47, 0xbd, 0xc0, 0x52, 0x4e, 0x87, 0x9f, 0x77, 0x06, 0xb5, 0xbd,
    0x58, 0x47, 0x80, 0xd9, 0x5a, 0xd5, 0x96, 0x50, 0x5d, 0x2f, 0x59, 0xf5,
    0x45, 0x63, 0x16, 0xc7, 0x2d, 0xd0, 0x89, 0x7e, 0xcd, 0xea, 0x76, 0xb4,
    0x50, 0xc7, 0x34, 0x60, 0xa4, 0x88, 0x11, 0x97, 0x4c, 0x0b, 0x31, 0x25,
    0x87, 0xc3, 0x41, 0x87, 0x97, 0xfc, 0xce, 0xb3, 0xf3, 0x27, 0x16, 0xa5,
    0xca, 0x6c, 0x8d, 0x50, 0x91, 0xcc, 0x5a, 0x29, 0xe9, 0xaa, 0x17, 0xb2,
    0x7a, 0x49, 0xd7, 0xc4, 0x90, 0xca, 0x9f, 0xa5, 0xd4, 0xf8, 0x23, 0xf8,
    0x23, 0x60, 0x83, 0xc6, 0xb2, 0xed, 0xcb, 0xbc, 0x07, 0x30, 0xba, 0xd0,
    0xdf, 0xb2, 0xbc, 0x46, 0xd7, 0x78, 0xae, 0xb7, 0x43, 0x24, 0x14, 0x72,
    0xd7, 0x53, 0xac, 0xe2, 0x04, 0x90, 0xa9, 0x7e, 0xd5, 0x6d, 0xa8, 0x27,
    0xb4, 0x7b, 0x5a, 0xd8, 0xf3, 0xa3, 0xe4, 0xf1, 0x26, 0xd6, 0xf4, 0x6d,
    0x8b, 0xfd, 0xe3, 0x65, 0x32, 0x64, 0xd9, 0xc5, 0xeb, 0x0b, 0xa3, 0xb9,
    0x91, 0x57, 0x61, 0x92, 0x19, 0x51, 0xe0, 0x49, 0xe5, 0x96, 0x50, 0xca,
    0xd3, 0xb0, 0x1e, 0xaa, 0x84, 0x8d, 0x1b, 0xb1, 0xbd, 0x36, 0x1e, 0xb0,
    0x50, 0x58, 0xaf, 0x73, 0x24, 0x74, 0xbd, 0xca, 0xac, 0xaa, 0xd3, 0x7f,
    0xb8, 0xe6, 0x7c, 0xc6, 0xca, 0xe2, 0xc8, 0xaa, 0x68, 0x63, 0x5a, 0x6c,
    0x76, 0x48, 0xe3, 0x58, 0xc4, 0x74, 0x7a, 0x31, 0x24, 0x92, 0x22, 0x9f,
    0xc3, 0x94, 0xc5, 0x1a, 0xea, 0x26, 0x05, 0x91, 0x5f, 0xcb, 0x16, 0xdf,
    0x6d, 0x1a, 0xec, 0x4b, 0x33, 0xaa, 0x22, 0x17, 0x3d, 0xcc, 0x54, 0x6b,
    0xb8, 0xe8, 0x0d, 0x9f, 0x99, 0xd0, 0x03, 0xfa, 0x1c, 0x4b, 0xb8, 0x47,
    0x51, 0xdc, 0xc1, 0xbc, 0xd2, 0xa4, 0xd5, 0x90, 0xda, 0x36, 0x57, 0x7a,
    0x4a, 0x66, 0x03, 0xf2, 0x83, 0xfb, 0xfe, 0xbc, 0x23, 0xd9, 0xb9, 0xab,
    0x95, 0xdc, 0xed, 0xed, 0x34, 0xed, 0x48, 0xa5, 0x48, 0x20, 0x24, 0x3e,
    0x3e, 0x1f, 0x32, 0x78, 0xc9, 0x72, 0xe6, 0xb6, 0x53, 0x04, 0x29, 0x20,
    0x5a, 0x26, 0x25, 0x41, 0xd3, 0x44, 0x75, 0xee, 0xfa, 0x37, 0x07, 0xd5,
    0xfd, 0x40, 0x67, 0x91, 0x55, 0x49, 0x14, 0x49, 0x6c, 0x55, 0x53, 0xaf,
    0x14, 0xed, 0xe7, 0xfd, 0x6e, 0x3f, 0xff, 0xd9,
};
size_t g_src_len = 0x224;

static std::string  //
decode() {
  // Call wuffs_aux::DecodeImage, which is the entry point to Wuffs' high-level
  // C++ API for decoding images. This API is easier to use than Wuffs'
  // low-level C API but the low-level one (1) handles animation, (2) handles
  // asynchronous I/O, (3) handles metadata and (4) does no dynamic memory
  // allocation, so it can run under a `SECCOMP_MODE_STRICT` sandbox.
  // Obviously, if you don't need any of those features, then these simple
  // lines of code here suffices.
  //
  // This example program doesn't explicitly use Wuffs' low-level C API but, if
  // you're curious to learn more, the wuffs_aux::DecodeImage implementation in
  // internal/cgen/auxiliary/*.cc uses it, as does the example/convert-to-nia C
  // program. There's also documentation at doc/std/image-decoders.md
  //
  // If you also want metadata like EXIF orientation and ICC color profiles,
  // script/print-image-metadata.cc has some example code. It uses Wuffs'
  // low-level API but it's a C++ program to use Wuffs' shorter convenience
  // methods: `decoder->decode_frame_config(NULL, &src)` instead of C's
  // `wuffs_base__image_decoder__decode_frame_config(decoder, NULL, &src)`.
  wuffs_aux::DecodeImageCallbacks callbacks;
  wuffs_aux::sync_io::MemoryInput input(&g_src_array[0], g_src_len);
  wuffs_aux::DecodeImageResult result =
      wuffs_aux::DecodeImage(callbacks, input);
  if (!result.error_message.empty()) {
    return result.error_message;
  }
  // If result.error_message is empty then the DecodeImage call succeeded. The
  // decoded image is held in result.pixbuf, backed by memory that is released
  // when result.pixbuf_mem_owner (a std::unique_ptr) is destroyed. In this
  // example program, this happens at the end of this function.

  // Print result.pixbuf as ASCII art.
  uint32_t w = result.pixbuf.pixcfg.width();
  uint32_t h = result.pixbuf.pixcfg.height();
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      // color is 32 bits (4x8 bits). The blue channel occupies the low 8 bits.
      // The green channel occupies the next 8 bits. Red the next 8 and alpha
      // the high 8 bits. Since our hard-coded JPEG image is grayscale, this
      // example program just prints an ASCII value based on the high 3 bits of
      // the blue channel value.
      //
      // This example program also configures wuffs_aux::DecodeImage to only
      // support JPEG images (and not BMP, PNG, etc). To add support for other
      // image file formats, add e.g. "#define WUFFS_CONFIG__MODULE__BMP" (and
      // any dependencies) next to the "#define WUFFS_CONFIG__MODULE__JPEG"
      // line above. Dependencies are listed at
      // https://github.com/google/wuffs/tree/main/doc/std#modules-and-dependencies
      //
      // Calling the color_u32_at method is simple and easy, but like any
      // one-call-per-pixel approach, it has some performance overhead. An
      // alternative approach calls result.pixbuf.plane instead, to get the
      // table (base pointer, width, height and stride) for the interleaved
      // (not multi-planar) WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL pixel data,
      // and proceeds with pointer arithmetic. See example/imageviewer code.
      wuffs_base__color_u32_argb_premul color =
          result.pixbuf.color_u32_at(x, y);
      putchar("-:=+IOX@"[(color & 0xFF) >> 5]);
    }
    putchar('\n');
  }
  return std::string();
}

int  //
main(int argc, char** argv) {
  std::string status_msg = decode();
  if (!status_msg.empty()) {
    fprintf(stderr, "%s\n", status_msg.c_str());
    return 1;
  }
  return 0;
}
