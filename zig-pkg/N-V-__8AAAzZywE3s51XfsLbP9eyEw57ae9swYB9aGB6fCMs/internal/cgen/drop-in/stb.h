// Copyright 2024 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ---------------- Wuffs' reimplementation of the STB API.
//
// This is a drop-in replacement of that third-party library.
//
// Disabled by default, unless you #define the
// WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB macro beforehand.
//
// For API docs, see https://github.com/nothings/stb

#if defined(WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB)

#ifdef __cplusplus
extern "C" {
#endif

#if defined(WUFFS_CONFIG__STATIC_FUNCTIONS) || defined(STB_IMAGE_STATIC)
#define WUFFS_DROP_IN__STB__MAYBE_STATIC static
#else
#define WUFFS_DROP_IN__STB__MAYBE_STATIC
#endif

enum {
  STBI_default = 0,
  STBI_grey = 1,
  STBI_grey_alpha = 2,
  STBI_rgb = 3,
  STBI_rgb_alpha = 4
};

typedef unsigned char stbi_uc;
typedef unsigned short stbi_us;

typedef struct {
  int (*read)(void* user, char* data, int size);
  void (*skip)(void* user, int n);
  int (*eof)(void* user);
} stbi_io_callbacks;

// --------

WUFFS_DROP_IN__STB__MAYBE_STATIC int  //
stbi_info_from_memory(                //
    stbi_uc const* buffer,            //
    int len,                          //
    int* x,                           //
    int* y,                           //
    int* comp);

WUFFS_DROP_IN__STB__MAYBE_STATIC stbi_uc*  //
stbi_load_from_memory(                     //
    stbi_uc const* buffer,                 //
    int len,                               //
    int* x,                                //
    int* y,                                //
    int* channels_in_file,                 //
    int desired_channels);

WUFFS_DROP_IN__STB__MAYBE_STATIC int  //
stbi_info_from_callbacks(             //
    stbi_io_callbacks const* clbk,    //
    void* user,                       //
    int* x,                           //
    int* y,                           //
    int* comp);

WUFFS_DROP_IN__STB__MAYBE_STATIC stbi_uc*  //
stbi_load_from_callbacks(                  //
    stbi_io_callbacks const* clbk,         //
    void* user,                            //
    int* x,                                //
    int* y,                                //
    int* channels_in_file,                 //
    int desired_channels);

// --------

#if !defined(STBI_NO_STDIO)

WUFFS_DROP_IN__STB__MAYBE_STATIC int  //
stbi_info(                            //
    char const* filename,             //
    int* x,                           //
    int* y,                           //
    int* comp);

WUFFS_DROP_IN__STB__MAYBE_STATIC stbi_uc*  //
stbi_load(                                 //
    char const* filename,                  //
    int* x,                                //
    int* y,                                //
    int* channels_in_file,                 //
    int desired_channels);

WUFFS_DROP_IN__STB__MAYBE_STATIC int  //
stbi_info_from_file(                  //
    FILE* f,                          //
    int* x,                           //
    int* y,                           //
    int* comp);

WUFFS_DROP_IN__STB__MAYBE_STATIC stbi_uc*  //
stbi_load_from_file(                       //
    FILE* f,                               //
    int* x,                                //
    int* y,                                //
    int* channels_in_file,                 //
    int desired_channels);

#endif  // !defined(STBI_NO_STDIO)

// --------

WUFFS_DROP_IN__STB__MAYBE_STATIC void  //
stbi_image_free(                       //
    void* retval_from_stbi_load);

WUFFS_DROP_IN__STB__MAYBE_STATIC const char*  //
stbi_failure_reason(void);

#ifdef __cplusplus
}
#endif

#endif  // defined (WUFFS_CONFIG__ENABLE_DROP_IN_REPLACEMENT__STB)
