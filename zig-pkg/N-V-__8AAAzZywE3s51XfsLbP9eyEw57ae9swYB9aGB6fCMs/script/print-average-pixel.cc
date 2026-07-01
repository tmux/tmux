// Copyright 2022 The Wuffs Authors.
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
print-average-pixel prints the average color of an image's pixels (as well as
the image file format, width and height). It's a toy program to demonstrate how
to use the wuffs_aux C++ API to decode an image and iterate over its pixels.
*/

#include <inttypes.h>
#include <stdio.h>

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
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__AUX__BASE
#define WUFFS_CONFIG__MODULE__AUX__IMAGE
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__BMP
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__ETC2
#define WUFFS_CONFIG__MODULE__GIF
#define WUFFS_CONFIG__MODULE__JPEG
#define WUFFS_CONFIG__MODULE__NETPBM
#define WUFFS_CONFIG__MODULE__NIE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__QOI
#define WUFFS_CONFIG__MODULE__TARGA
#define WUFFS_CONFIG__MODULE__THUMBHASH
#define WUFFS_CONFIG__MODULE__WBMP
#define WUFFS_CONFIG__MODULE__ZLIB

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../release/c/wuffs-unsupported-snapshot.c"

class MyCallbacks : public wuffs_aux::DecodeImageCallbacks {
 public:
  MyCallbacks() : m_fourcc(0) {}

  uint32_t m_fourcc;

 private:
  wuffs_base__image_decoder::unique_ptr  //
  SelectDecoder(uint32_t fourcc,
                wuffs_base__slice_u8 prefix_data,
                bool prefix_closed) override {
    // Save the fourcc value (you can think of it as like a 'MIME type' but in
    // uint32_t form) before calling the superclass' implementation.
    //
    // The "if (m_fourcc == 0)" is because SelectDecoder can be called multiple
    // times. Files that are nominally BMP images can contain complete JPEG or
    // PNG images. This program prints the outer file format, the first one
    // encountered, not the inner one.
    if (m_fourcc == 0) {
      m_fourcc = fourcc;
    }
    return wuffs_aux::DecodeImageCallbacks::SelectDecoder(fourcc, prefix_data,
                                                          prefix_closed);
  }

  wuffs_base__pixel_format  //
  SelectPixfmt(const wuffs_base__image_config& image_config) override {
    // This is the same as the superclass' implementation, but makes it
    // explicit that this program uses a single-plane pixel buffer (as opposed
    // to e.g. 3-plane YCbCr) with 4 bytes per pixel (in B, G, R, A order) and
    // premultiplied alpha.
    return wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL);
  }

  AllocPixbufResult  //
  AllocPixbuf(const wuffs_base__image_config& image_config,
              bool allow_uninitialized_memory) override {
    // This just calls the superclass' implementation, but if you wanted more
    // control about how the pixel buffer's memory is allocated and freed,
    // change the code here. For example, if you (the wuffs_aux::DecodeImage
    // caller) want to use an already-allocated buffer, instead of Wuffs (the
    // callee) allocating a new buffer.
    //
    // The example/sdl-imageviewer/sdl-imageviewer.cc file demonstrates
    // overriding the AllocPixbuf method with the "bring your own buffer"
    // approach. In the sdl-imageviewer case, the pixel buffer is allocated by
    // SDL and lent to Wuffs, instead of allocated by Wuffs.
    return wuffs_aux::DecodeImageCallbacks::AllocPixbuf(
        image_config, allow_uninitialized_memory);
  }
};

void  //
handle(const char* filename, FILE* f) {
  MyCallbacks callbacks;
  wuffs_aux::sync_io::FileInput input(f);
  wuffs_aux::DecodeImageResult res = wuffs_aux::DecodeImage(callbacks, input);
  if (!res.error_message.empty()) {
    printf("%-30s %s\n", filename, res.error_message.c_str());
    return;
  } else if (res.pixbuf.pixcfg.pixel_format().repr !=
             WUFFS_BASE__PIXEL_FORMAT__BGRA_PREMUL) {
    printf("%-30s internal error: inconsistent pixel format\n", filename);
    return;
  }

  wuffs_base__table_u8 table = res.pixbuf.plane(0);
  uint32_t w = res.pixbuf.pixcfg.width();
  uint32_t h = res.pixbuf.pixcfg.height();

  uint64_t count = 0;
  uint64_t color_b = 0;
  uint64_t color_g = 0;
  uint64_t color_r = 0;
  uint64_t color_a = 0;
  for (uint32_t y = 0; y < h; y++) {
    const uint8_t* ptr = table.ptr + (y * table.stride);
    for (uint32_t x = 0; x < w; x++) {
      count++;
      color_b += *ptr++;
      color_g += *ptr++;
      color_r += *ptr++;
      color_a += *ptr++;
    }
  }
  if (count > 0) {
    color_b = (color_b + (count / 2)) / count;
    color_g = (color_g + (count / 2)) / count;
    color_r = (color_r + (count / 2)) / count;
    color_a = (color_a + (count / 2)) / count;
  }

  printf("%-30s %c%c%c%c   %5" PRIu32 " x %5" PRIu32
         "   AverageARGB: %02X%02X%02X%02X\n",  //
         filename,                              //
         (0xFF & (callbacks.m_fourcc >> 24)),   //
         (0xFF & (callbacks.m_fourcc >> 16)),   //
         (0xFF & (callbacks.m_fourcc >> 8)),    //
         (0xFF & (callbacks.m_fourcc >> 0)),    //
         w, h,                                  //
         (int)(color_a),                        //
         (int)(color_r),                        //
         (int)(color_g),                        //
         (int)(color_b));

  // While it's valid to deference table.ptr in this function, the end of scope
  // here means that the res.pixbuf_mem_owner destructor will free the backing
  // memory (unless you modified the AllocPixbuf method above).
  //
  // If you wanted to return the pixel buffer to the caller, either return the
  // wuffs_aux::MemOwner (a type alias for "std::unique_ptr<void,
  // decltype(&free)>") too, or call res.pixbuf_mem_owner.release() before
  // returning and manually free the backing memory at an appropriate time.
}

int  //
main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    FILE* f = fopen(argv[i], "r");
    if (!f) {
      printf("%-30s could not open file: %s\n", argv[i], strerror(errno));
      continue;
    }
    handle(argv[i], f);
    fclose(f);
  }
  return 0;
}
