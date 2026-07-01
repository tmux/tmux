// Copyright 2021 The Wuffs Authors.
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
sdl-imageviewer is a simple GUI program for viewing an image. To run:

$CXX sdl-imageviewer.cc -lSDL2 -lSDL2_image && \
  ./a.out ../../test/data/bricks-color.png; rm -f a.out

for a C++ compiler $CXX, such as clang++ or g++.

The Tab key switches between decoding the image via Wuffs or via SDL2_image.
There should be no difference unless you uncomment the § line of code below.

The Escape key quits.

----

This program (in Wuffs' example directory) is like example/imageviewer but with
fewer features. It focuses on showing how to integrate the Wuffs image decoders
with SDL (as an alternative to the SDL_image extension).

While SDL is cross-platform, this program is not as good as example/imageviewer
for general use. SDL (which is designed for full-screen games) uses a noticable
amount of CPU (and therefore power) polling for events even when the program
isn't otherwise doing anything.
*/

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

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
#define WUFFS_CONFIG__MODULE__VP8
#define WUFFS_CONFIG__MODULE__WBMP
#define WUFFS_CONFIG__MODULE__WEBP
#define WUFFS_CONFIG__MODULE__ZLIB

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
#define WUFFS_CONFIG__DST_PIXEL_FORMAT__ALLOW_BGRA_NONPREMUL

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

// ----------------

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_render.h>

// --------

class Wuffs_Load_RW_Callbacks : public wuffs_aux::DecodeImageCallbacks {
 public:
  Wuffs_Load_RW_Callbacks() : m_surface(NULL) {}

  ~Wuffs_Load_RW_Callbacks() {
    if (m_surface) {
      SDL_UnlockSurface(m_surface);
      SDL_FreeSurface(m_surface);
      m_surface = NULL;
    }
  }

  SDL_Surface*  //
  TakeSurface() {
    if (!m_surface) {
      return NULL;
    }
    SDL_UnlockSurface(m_surface);
    SDL_Surface* ret = m_surface;
    m_surface = NULL;
    return ret;
  }

 private:
  wuffs_base__pixel_format  //
  SelectPixfmt(const wuffs_base__image_config& image_config) override {
    // Regardless of endianness, SDL_PIXELFORMAT_BGRA32 (from a few lines
    // below) is equivalent to WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL.
    return wuffs_base__make_pixel_format(
        WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL);
  }

  AllocPixbufResult  //
  AllocPixbuf(const wuffs_base__image_config& image_config,
              bool allow_uninitialized_memory) override {
    if (m_surface) {
      SDL_UnlockSurface(m_surface);
      SDL_FreeSurface(m_surface);
      m_surface = NULL;
    }
    uint32_t w = image_config.pixcfg.width();
    uint32_t h = image_config.pixcfg.height();
    if ((w > 0xFFFFFF) || (h > 0xFFFFFF)) {
      return AllocPixbufResult("Wuffs_Load_RW_Callbacks: image is too large");
    }
    uint32_t sdl_pixelformat = SDL_PIXELFORMAT_BGRA32;

    // (§) Uncomment this line of code to invert the BGRA/RGBA color order.
    // This isn't a generally useful feature for an image viewer, but it should
    // make it obvious, when pressing the TAB key, whether you're using the
    // Wuffs (inverted) or SDL_image (correct) decoder.
    //
    // sdl_pixelformat = SDL_PIXELFORMAT_RGBA32;

    m_surface = SDL_CreateRGBSurfaceWithFormat(
        0, static_cast<int>(w), static_cast<int>(h), 32, sdl_pixelformat);
    if (!m_surface) {
      return AllocPixbufResult(
          "Wuffs_Load_RW_Callbacks: SDL_CreateRGBSurface failed");
    }
    SDL_LockSurface(m_surface);
    wuffs_base__pixel_buffer pixbuf;
    wuffs_base__status status = pixbuf.set_interleaved(
        &image_config.pixcfg,
        wuffs_base__make_table_u8(static_cast<uint8_t*>(m_surface->pixels),
                                  m_surface->w * 4, m_surface->h,
                                  m_surface->pitch),
        wuffs_base__empty_slice_u8());
    if (!status.is_ok()) {
      SDL_UnlockSurface(m_surface);
      SDL_FreeSurface(m_surface);
      m_surface = NULL;
      return AllocPixbufResult(status.message());
    }
    return AllocPixbufResult(wuffs_aux::MemOwner(NULL, &free), pixbuf);
  }

  SDL_Surface* m_surface;
};

// --------

class Wuffs_Load_RW_Input : public wuffs_aux::sync_io::Input {
 public:
  Wuffs_Load_RW_Input(SDL_RWops* rw, bool take_ownership_of_rw)
      : m_rw(rw), m_ownership(take_ownership_of_rw) {}

  ~Wuffs_Load_RW_Input() {
    if (m_rw && m_ownership) {
      m_rw->close(m_rw);
    }
  }

 private:
  std::string  //
  CopyIn(wuffs_aux::IOBuffer* dst) override {
    if (!m_rw) {
      return "Wuffs_Load_RW_Input: NULL SDL_RWops";
    } else if (!dst) {
      return "Wuffs_Load_RW_Input: NULL IOBuffer";
    } else if (dst->meta.closed) {
      return "Wuffs_Load_RW_Input: end of file";
    }
    dst->compact();
    if (dst->writer_length() == 0) {
      return "Wuffs_Load_RW_Input: full IOBuffer";
    }
    size_t n = m_rw->read(m_rw, dst->writer_pointer(), 1, dst->writer_length());
    dst->meta.wi += n;
    return std::string();
  }

  SDL_RWops* m_rw;
  const bool m_ownership;
};

// --------

// Wuffs_Load_RW loads the image from the input rw. It is like SDL_image's
// IMG_Load_RW function but it returns any error in-band (as a std::string)
// instead of separately (global state accessible via SDL_GetError).
//
// On success, the SDL_Surface* returned will be non-NULL and the caller owns
// it. Ownership means that they are responsible for calling SDL_FreeSurface on
// it when done.
std::pair<SDL_Surface*, std::string>  //
Wuffs_Load_RW(SDL_RWops* rw, bool take_ownership_of_rw) {
  Wuffs_Load_RW_Callbacks callbacks;
  Wuffs_Load_RW_Input input(rw, take_ownership_of_rw);
  wuffs_aux::DecodeImageResult res = wuffs_aux::DecodeImage(callbacks, input);
  if (!res.error_message.empty()) {
    return std::make_pair<SDL_Surface*, std::string>(
        NULL, std::move(res.error_message));
  }
  return std::make_pair<SDL_Surface*, std::string>(callbacks.TakeSurface(),
                                                   std::string());
}

// ----------------

SDL_Surface* g_image = NULL;
bool g_load_via_sdl_image = false;

bool  //
draw(SDL_Window* window) {
  SDL_Surface* ws = SDL_GetWindowSurface(window);
  if (!ws) {
    // Use an indirect approach, for exotic window pixel formats (e.g. X.org 10
    // bits per RGB channel), when we can't get the window surface directly.
    //
    // See https://github.com/google/wuffs/issues/51
    SDL_Renderer* r = SDL_CreateRenderer(window, -1, 0);
    if (!r) {
      fprintf(stderr, "main: SDL_CreateRenderer: %s\n", SDL_GetError());
      return false;
    }
    SDL_RenderClear(r);
    if (g_image) {
      SDL_Texture* texture = SDL_CreateTextureFromSurface(r, g_image);
      SDL_RenderCopy(r, texture, NULL, &g_image->clip_rect);
      SDL_DestroyTexture(texture);
    }
    SDL_RenderPresent(r);
    SDL_DestroyRenderer(r);
    return true;
  }

  // Use a direct approach.
  SDL_FillRect(ws, NULL, SDL_MapRGB(ws->format, 0x00, 0x00, 0x00));
  if (g_image) {
    SDL_BlitSurface(g_image, NULL, ws, NULL);
  }
  SDL_UpdateWindowSurface(window);
  return true;
}

bool  //
load_image(const char* filename) {
  if (g_image) {
    SDL_FreeSurface(g_image);
    g_image = NULL;
  }

  SDL_RWops* rw = SDL_RWFromFile(filename, "rb");
  if (!rw) {
    fprintf(stderr, "main: SDL_RWFromFile(\"%s\"): %s\n", filename,
            SDL_GetError());
    return false;
  }

  constexpr bool take_ownership_of_rw = true;
  if (g_load_via_sdl_image) {
    g_image = IMG_Load_RW(rw, take_ownership_of_rw);
    if (!g_image) {
      fprintf(stderr, "main: IMG_Load_RW(\"%s\"): %s\n", filename,
              SDL_GetError());
      return false;
    }
  } else {
    std::pair<SDL_Surface*, std::string> p =
        Wuffs_Load_RW(rw, take_ownership_of_rw);
    if (!p.second.empty()) {
      fprintf(stderr, "main: Wuffs_Load_RW(\"%s\"): %s\n", filename,
              p.second.c_str());
      return false;
    }
    g_image = p.first;
  }

  return true;
}

int  //
main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: %s filename\n", argv[0]);
    return 1;
  }
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "main: SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window* window =
      SDL_CreateWindow("sdl-imageviewer", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_SHOWN);
  if (!window) {
    fprintf(stderr, "main: SDL_CreateWindow: %s\n", SDL_GetError());
    return 1;
  }

  if (!load_image(argv[1])) {
    return 1;
  }

  while (true) {
    SDL_Event event;
    if (!SDL_WaitEvent(&event)) {
      fprintf(stderr, "main: SDL_WaitEvent: %s\n", SDL_GetError());
      return 1;
    }

    switch (event.type) {
      case SDL_QUIT:
        goto cleanup;

      case SDL_WINDOWEVENT:
        switch (event.window.event) {
          case SDL_WINDOWEVENT_EXPOSED:
            if (!draw(window)) {
              return 1;
            }
            break;
        }
        break;

      case SDL_KEYDOWN:
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
            goto cleanup;
          case SDLK_TAB:
            g_load_via_sdl_image = !g_load_via_sdl_image;
            printf("Switched to %s.\n",
                   g_load_via_sdl_image ? "SDL_image" : "Wuffs");
            if (!load_image(argv[1]) || !draw(window)) {
              return 1;
            }
            break;
        }
        break;
    }
  }

cleanup:
  if (g_image) {
    SDL_FreeSurface(g_image);
    g_image = NULL;
  }
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
