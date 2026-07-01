// Copyright 2017 The Wuffs Authors.
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
This test program is typically run indirectly, by the "wuffs test" or "wuffs
bench" commands. These commands take an optional "-mimic" flag to check that
Wuffs' output mimics (i.e. exactly matches) other libraries' output, such as
giflib for GIF, libpng for PNG, etc.

To manually run this test:

for CC in clang gcc; do
  $CC -std=c99 -Wall -Werror gif.c && ./a.out
  rm -f a.out
done

Each edition should print "PASS", amongst other information, and exit(0).

Add the "wuffs mimic cflags" (everything after the colon below) to the C
compiler flags (after the .c file) to run the mimic tests.

To manually run the benchmarks, replace "-Wall -Werror" with "-O3" and replace
the first "./a.out" with "./a.out -bench". Combine these changes with the
"wuffs mimic cflags" to run the mimic benchmarks.
*/

// ¿ wuffs mimic cflags: -DWUFFS_MIMIC -lgif

// Wuffs ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define WUFFS_IMPLEMENTATION before #include'ing or
// compiling it.
#define WUFFS_IMPLEMENTATION

// Defining the WUFFS_CONFIG__MODULE* macros are optional, but it lets users of
// release/c/etc.c choose which parts of Wuffs to build. That file contains the
// entire Wuffs standard library, implementing a variety of codecs and file
// formats. Without this macro definition, an optimizing compiler or linker may
// very well discard Wuffs code for unused codecs, but listing the Wuffs
// modules we use makes that process explicit. Preprocessing means that such
// code simply isn't compiled.
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__GIF

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../../../release/c/wuffs-unsupported-snapshot.c"
#include "../testlib/testlib.c"
#ifdef WUFFS_MIMIC
#include "../mimiclib/gif.c"
#endif

// ---------------- Basic Tests

const char*  //
test_basic_bad_receiver() {
  CHECK_FOCUS(__func__);
  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  wuffs_base__status status =
      wuffs_gif__decoder__decode_image_config(NULL, &ic, NULL);
  if (status.repr != wuffs_base__error__bad_receiver) {
    RETURN_FAIL("decode_image_config: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_receiver);
  }
  return NULL;
}

const char*  //
test_basic_bad_sizeof_receiver() {
  CHECK_FOCUS(__func__);
  wuffs_gif__decoder dec;
  wuffs_base__status status = wuffs_gif__decoder__initialize(
      &dec, 0, WUFFS_VERSION,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status.repr != wuffs_base__error__bad_sizeof_receiver) {
    RETURN_FAIL("decode_image_config: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_sizeof_receiver);
  }
  return NULL;
}

const char*  //
test_basic_bad_wuffs_version() {
  CHECK_FOCUS(__func__);
  wuffs_gif__decoder dec;
  wuffs_base__status status = wuffs_gif__decoder__initialize(
      &dec, sizeof dec, WUFFS_VERSION ^ 0x123456789ABC,
      WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED);
  if (status.repr != wuffs_base__error__bad_wuffs_version) {
    RETURN_FAIL("decode_image_config: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_wuffs_version);
  }
  return NULL;
}

const char*  //
test_basic_initialize_not_called() {
  CHECK_FOCUS(__func__);
  wuffs_gif__decoder dec = ((wuffs_gif__decoder){});
  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  wuffs_base__status status =
      wuffs_gif__decoder__decode_image_config(&dec, &ic, NULL);
  if (status.repr != wuffs_base__error__initialize_not_called) {
    RETURN_FAIL("decode_image_config: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__initialize_not_called);
  }
  return NULL;
}

const char*  //
test_basic_status_is_error() {
  CHECK_FOCUS(__func__);
  wuffs_base__status status;
  status.repr = NULL;
  if (wuffs_base__status__is_error(&status)) {
    RETURN_FAIL("is_error(NULL) returned true");
  }
  status.repr = wuffs_base__error__bad_wuffs_version;
  if (!wuffs_base__status__is_error(&status)) {
    RETURN_FAIL("is_error(BAD_WUFFS_VERSION) returned false");
  }
  status.repr = wuffs_base__suspension__short_write;
  if (wuffs_base__status__is_error(&status)) {
    RETURN_FAIL("is_error(SHORT_WRITE) returned true");
  }
  status.repr = wuffs_gif__error__bad_header;
  if (!wuffs_base__status__is_error(&status)) {
    RETURN_FAIL("is_error(BAD_HEADER) returned false");
  }
  return NULL;
}

// ---------------- GIF Tests

const char*  //
test_wuffs_gif_decode_interface_image_decoder() {
  CHECK_FOCUS(__func__);
  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  return do_test__wuffs_base__image_decoder(
      wuffs_gif__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      "test/data/bricks-nodither.gif", 0, SIZE_MAX, 160, 120, 0xFF012463);
}

const char*  //
test_wuffs_gif_decode_truncated_input() {
  CHECK_FOCUS(__func__);

  wuffs_base__io_buffer src =
      wuffs_base__ptr_u8__reader(g_src_array_u8, 0, false);
  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__status status =
      wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
  if (status.repr != wuffs_base__suspension__short_read) {
    RETURN_FAIL("closed=false: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__suspension__short_read);
  }

  src.meta.closed = true;
  status = wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
  if (status.repr != wuffs_gif__error__truncated_input) {
    RETURN_FAIL("closed=true: have \"%s\", want \"%s\"", status.repr,
                wuffs_gif__error__truncated_input);
  }
  return NULL;
}

const char*  //
wuffs_gif_decode(uint64_t* n_bytes_out,
                 wuffs_base__io_buffer* dst,
                 uint32_t wuffs_initialize_flags,
                 wuffs_base__pixel_format pixfmt,
                 uint32_t* quirks_ptr,
                 size_t quirks_len,
                 wuffs_base__io_buffer* src) {
  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(&dec, sizeof dec, WUFFS_VERSION,
                                              wuffs_initialize_flags));
  return do_run__wuffs_base__image_decoder(
      wuffs_gif__decoder__upcast_as__wuffs_base__image_decoder(&dec),
      n_bytes_out, dst, pixfmt, quirks_ptr, quirks_len, src);
}

const char*  //
do_test_wuffs_gif_decode(const char* filename,
                         const char* palette_filename,
                         const char* indexes_filename,
                         uint64_t rlimit,
                         wuffs_base__pixel_format dst_pixfmt) {
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, filename));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});

  {
    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    CHECK_STATUS("decode_image_config",
                 wuffs_gif__decoder__decode_image_config(&dec, &ic, &src));
    if (wuffs_base__pixel_config__pixel_format(&ic.pixcfg).repr !=
        WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY) {
      RETURN_FAIL("pixel_format: have 0x%08" PRIX32 ", want 0x%08" PRIX32,
                  wuffs_base__pixel_config__pixel_format(&ic.pixcfg).repr,
                  WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY);
    }

    // bricks-dither.gif is a 160 × 120, opaque, still (not animated) GIF.
    if (wuffs_base__pixel_config__width(&ic.pixcfg) != 160) {
      RETURN_FAIL("width: have %" PRIu32 ", want 160",
                  wuffs_base__pixel_config__width(&ic.pixcfg));
    }
    if (wuffs_base__pixel_config__height(&ic.pixcfg) != 120) {
      RETURN_FAIL("height: have %" PRIu32 ", want 120",
                  wuffs_base__pixel_config__height(&ic.pixcfg));
    }
    if (!wuffs_base__image_config__first_frame_is_opaque(&ic)) {
      RETURN_FAIL("first_frame_is_opaque: have false, want true");
    }

    wuffs_base__pixel_config__set(&ic.pixcfg, dst_pixfmt.repr,
                                  WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, 160,
                                  120);

    CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                       &pb, &ic.pixcfg, g_pixel_slice_u8));
  }

  int num_iters = 0;
  while (true) {
    num_iters++;
    wuffs_base__io_buffer limited_src = make_limited_reader(src, rlimit);
    size_t old_ri = src.meta.ri;

    wuffs_base__status status =
        wuffs_gif__decoder__decode_frame_config(&dec, &fc, &limited_src);
    src.meta.ri += limited_src.meta.ri;

    if (wuffs_base__status__is_ok(&status)) {
      break;
    }
    if (status.repr != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_frame_config: have \"%s\", want \"%s\"", status.repr,
                  wuffs_base__suspension__short_read);
    }

    if (src.meta.ri < old_ri) {
      RETURN_FAIL("read index src.meta.ri went backwards");
    }
    if (src.meta.ri == old_ri) {
      RETURN_FAIL("no progress was made");
    }
  }

  while (true) {
    num_iters++;
    wuffs_base__io_buffer limited_src = make_limited_reader(src, rlimit);
    size_t old_ri = src.meta.ri;

    wuffs_base__status status = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &limited_src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8,
        NULL);
    src.meta.ri += limited_src.meta.ri;

    if (wuffs_base__status__is_ok(&status)) {
      break;
    }
    if (status.repr != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_frame: have \"%s\", want \"%s\"", status.repr,
                  wuffs_base__suspension__short_read);
    }

    if (src.meta.ri < old_ri) {
      RETURN_FAIL("read index src.meta.ri went backwards");
    }
    if (src.meta.ri == old_ri) {
      RETURN_FAIL("no progress was made");
    }
  }

  CHECK_STRING(copy_to_io_buffer_from_pixel_buffer(
      &have, &pb, wuffs_base__frame_config__bounds(&fc)));

  if (rlimit < UINT64_MAX) {
    if (num_iters <= 2) {
      RETURN_FAIL("num_iters: have %d, want > 2", num_iters);
    }
  } else {
    if (num_iters != 2) {
      RETURN_FAIL("num_iters: have %d, want 2", num_iters);
    }
  }

  uint8_t pal_want_array[1024];
  wuffs_base__io_buffer pal_want = ((wuffs_base__io_buffer){
      .data = ((wuffs_base__slice_u8){
          .ptr = pal_want_array,
          .len = 1024,
      }),
  });
  CHECK_STRING(read_file(&pal_want, palette_filename));
  if (dst_pixfmt.repr == WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY) {
    wuffs_base__io_buffer pal_have = ((wuffs_base__io_buffer){
        .data = wuffs_base__pixel_buffer__palette(&pb),
    });
    pal_have.meta.wi = pal_have.data.len;
    CHECK_STRING(check_io_buffers_equal("palette ", &pal_have, &pal_want));
  }

  wuffs_base__io_buffer ind_want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  CHECK_STRING(read_file(&ind_want, indexes_filename));
  if (dst_pixfmt.repr == WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY) {
    CHECK_STRING(check_io_buffers_equal("indexes ", &have, &ind_want));
  } else {
    wuffs_base__io_buffer expanded_want = ((wuffs_base__io_buffer){
        .data = g_work_slice_u8,
    });
    if (ind_want.meta.wi > (expanded_want.data.len / 4)) {
      RETURN_FAIL("indexes are too long to expand into the work buffer");
    }

    if (dst_pixfmt.repr == WUFFS_BASE__PIXEL_FORMAT__BGR) {
      for (size_t i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        expanded_want.data.ptr[3 * i + 0] = pal_want_array[4 * index + 0];
        expanded_want.data.ptr[3 * i + 1] = pal_want_array[4 * index + 1];
        expanded_want.data.ptr[3 * i + 2] = pal_want_array[4 * index + 2];
      }
      expanded_want.meta.wi = 3 * ind_want.meta.wi;
    } else if (dst_pixfmt.repr == WUFFS_BASE__PIXEL_FORMAT__BGR_565) {
      for (size_t i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        uint32_t b5 = pal_want_array[4 * index + 0] >> 3;
        uint32_t g6 = pal_want_array[4 * index + 1] >> 2;
        uint32_t r5 = pal_want_array[4 * index + 2] >> 3;
        uint32_t bgr = (b5 << 0) | (g6 << 5) | (r5 << 11);
        expanded_want.data.ptr[2 * i + 0] = (uint8_t)(bgr >> 0);
        expanded_want.data.ptr[2 * i + 1] = (uint8_t)(bgr >> 8);
      }
      expanded_want.meta.wi = 2 * ind_want.meta.wi;
    } else if (dst_pixfmt.repr == WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL) {
      for (size_t i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        expanded_want.data.ptr[4 * i + 0] = pal_want_array[4 * index + 0];
        expanded_want.data.ptr[4 * i + 1] = pal_want_array[4 * index + 1];
        expanded_want.data.ptr[4 * i + 2] = pal_want_array[4 * index + 2];
        expanded_want.data.ptr[4 * i + 3] = pal_want_array[4 * index + 3];
      }
      expanded_want.meta.wi = 4 * ind_want.meta.wi;
    } else if (dst_pixfmt.repr == WUFFS_BASE__PIXEL_FORMAT__RGB) {
      for (size_t i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        expanded_want.data.ptr[3 * i + 0] = pal_want_array[4 * index + 2];
        expanded_want.data.ptr[3 * i + 1] = pal_want_array[4 * index + 1];
        expanded_want.data.ptr[3 * i + 2] = pal_want_array[4 * index + 0];
      }
      expanded_want.meta.wi = 3 * ind_want.meta.wi;
    } else if (dst_pixfmt.repr == WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL) {
      for (size_t i = 0; i < ind_want.meta.wi; i++) {
        uint8_t index = ind_want.data.ptr[i];
        expanded_want.data.ptr[4 * i + 0] = pal_want_array[4 * index + 2];
        expanded_want.data.ptr[4 * i + 1] = pal_want_array[4 * index + 1];
        expanded_want.data.ptr[4 * i + 2] = pal_want_array[4 * index + 0];
        expanded_want.data.ptr[4 * i + 3] = pal_want_array[4 * index + 3];
      }
      expanded_want.meta.wi = 4 * ind_want.meta.wi;
    } else {
      return "unsupported pixel format";
    }

    CHECK_STRING(check_io_buffers_equal("pixels ", &have, &expanded_want));
  }

  {
    if (src.meta.ri == src.meta.wi) {
      RETURN_FAIL("decode_frame returned \"ok\" but src was exhausted");
    }
    wuffs_base__status status = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
    if (status.repr != wuffs_base__note__end_of_data) {
      RETURN_FAIL("decode_frame: have \"%s\", want \"%s\"", status.repr,
                  wuffs_base__note__end_of_data);
    }
    if (src.meta.ri != src.meta.wi) {
      RETURN_FAIL(
          "decode_frame returned \"end of data\" but src was not exhausted");
    }
  }

  {
    uint32_t have = wuffs_gif__decoder__num_animation_loops(&dec);
    uint32_t want =
        (wuffs_gif__decoder__num_decoded_frame_configs(&dec) > 1) ? 1 : 0;
    if (have != want) {
      RETURN_FAIL("num_animation_loops: have %" PRIu32 ", want %" PRIu32, have,
                  want);
    }
  }

  return NULL;
}

const char*  //
do_test_wuffs_gif_decode_expecting(wuffs_base__io_buffer src,
                                   uint32_t quirk,
                                   const char* want_status,
                                   bool want_dirty_rect_is_empty) {
  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  if (quirk) {
    wuffs_gif__decoder__set_quirk(&dec, quirk, 1);
  }

  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  CHECK_STATUS("decode_image_config",
               wuffs_gif__decoder__decode_image_config(&dec, &ic, &src));

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                     &pb, &ic.pixcfg, g_pixel_slice_u8));

  {
    wuffs_base__status status = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
    if (status.repr != want_status) {
      RETURN_FAIL("decode_frame #0: have \"%s\", want \"%s\"", status.repr,
                  want_status);
    }
  }

  wuffs_base__rect_ie_u32 r = wuffs_gif__decoder__frame_dirty_rect(&dec);
  bool dirty_rect_is_empty = wuffs_base__rect_ie_u32__is_empty(&r);
  if (dirty_rect_is_empty != want_dirty_rect_is_empty) {
    RETURN_FAIL("dirty_rect.is_empty: have %s, want %s",
                dirty_rect_is_empty ? "true" : "false",
                want_dirty_rect_is_empty ? "true" : "false");
  }

  if (want_status == NULL) {
    wuffs_base__status status = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
    if (status.repr != wuffs_base__note__end_of_data) {
      RETURN_FAIL("decode_frame #1: have \"%s\", want \"%s\"", status.repr,
                  wuffs_base__note__end_of_data);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_gif_call_interleaved() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/bricks-dither.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  {
    wuffs_base__io_buffer limited_src = make_limited_reader(src, 700);
    wuffs_base__status status =
        wuffs_gif__decoder__decode_frame_config(&dec, NULL, &limited_src);
    src.meta.ri += limited_src.meta.ri;
    if (status.repr != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_frame_config: have \"%s\", want \"%s\"", status.repr,
                  wuffs_base__suspension__short_read);
    }
  }

  // Calling another coroutine (decode_image_config) while suspended inside an
  // active coroutine (decode_frame_config) should be an error:
  // wuffs_base__error__interleaved_coroutine_calls.

  {
    wuffs_base__status status =
        wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
    if (status.repr != wuffs_base__error__interleaved_coroutine_calls) {
      RETURN_FAIL("decode_image_config: have \"%s\", want \"%s\"", status.repr,
                  wuffs_base__error__interleaved_coroutine_calls);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_gif_call_sequence() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/bricks-dither.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  CHECK_STATUS("decode_image_config",
               wuffs_gif__decoder__decode_image_config(&dec, NULL, &src));

  wuffs_base__status status =
      wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
  if (status.repr != wuffs_base__error__bad_call_sequence) {
    RETURN_FAIL("decode_image_config: have \"%s\", want \"%s\"", status.repr,
                wuffs_base__error__bad_call_sequence);
  }
  return NULL;
}

const char*  //
do_test_wuffs_gif_decode_animated(
    const char* filename,
    uint32_t want_num_loops,
    uint32_t want_num_frames,
    wuffs_base__rect_ie_u32* want_frame_config_bounds) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, filename));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__image_config ic = ((wuffs_base__image_config){});

  CHECK_STATUS("decode_image_config",
               wuffs_gif__decoder__decode_image_config(&dec, &ic, &src));

  uint32_t have_num_loops = wuffs_gif__decoder__num_animation_loops(&dec);
  if (have_num_loops != want_num_loops) {
    RETURN_FAIL("num_loops: have %" PRIu32 ", want %" PRIu32, have_num_loops,
                want_num_loops);
  }

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                     &pb, &ic.pixcfg, g_pixel_slice_u8));

  for (uint32_t i = 0; i < want_num_frames; i++) {
    wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
    wuffs_base__status status =
        wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("decode_frame_config #%" PRIu32 ": \"%s\"", i, status.repr);
    }

    if (want_frame_config_bounds) {
      wuffs_base__rect_ie_u32 have = wuffs_base__frame_config__bounds(&fc);
      wuffs_base__rect_ie_u32 want = want_frame_config_bounds[i];
      if (!wuffs_base__rect_ie_u32__equals(&have, want)) {
        RETURN_FAIL("decode_frame_config #%" PRIu32 ": bounds: have (%" PRIu32
                    ", %" PRIu32 ")-(%" PRIu32 ", %" PRIu32 "), want (%" PRIu32
                    ", %" PRIu32 ")-(%" PRIu32 ", %" PRIu32 ")",
                    i, have.min_incl_x, have.min_incl_y, have.max_excl_x,
                    have.max_excl_y, want.min_incl_x, want.min_incl_y,
                    want.max_excl_x, want.max_excl_y);
      }
    }

    status = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("decode_frame #%" PRIu32 ": \"%s\"", i, status.repr);
    }

    wuffs_base__rect_ie_u32 frame_rect = wuffs_base__frame_config__bounds(&fc);
    wuffs_base__rect_ie_u32 dirty_rect =
        wuffs_gif__decoder__frame_dirty_rect(&dec);
    if (!wuffs_base__rect_ie_u32__contains_rect(&frame_rect, dirty_rect)) {
      RETURN_FAIL("internal error: frame_rect does not contain dirty_rect");
    }
  }

  // There should be no more frames, no matter how many times we call
  // decode_frame.
  for (uint32_t i = 0; i < 3; i++) {
    wuffs_base__status status = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
    if (status.repr != wuffs_base__note__end_of_data) {
      RETURN_FAIL("decode_frame: have \"%s\", want \"%s\"", status.repr,
                  wuffs_base__note__end_of_data);
    }
  }

  uint64_t have_num_frames = wuffs_gif__decoder__num_decoded_frames(&dec);
  if (have_num_frames != want_num_frames) {
    RETURN_FAIL("frame_count: have %" PRIu64 ", want %" PRIu32, have_num_frames,
                want_num_frames);
  }

  return NULL;
}

const char*  //
test_wuffs_gif_decode_animated_big() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode_animated("test/data/gifplayer-muybridge.gif",
                                           0, 380, NULL);
}

const char*  //
test_wuffs_gif_decode_animated_medium() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode_animated("test/data/muybridge.gif", 0, 15,
                                           NULL);
}

const char*  //
test_wuffs_gif_decode_animated_small() {
  CHECK_FOCUS(__func__);
  // animated-red-blue.gif's num_loops should be 3. The value explicitly in the
  // wire format is 0x0002, but that value means "repeat 2 times after the
  // first play", so the total number of loops is 3.
  const uint32_t want_num_loops = 3;
  wuffs_base__rect_ie_u32 want_frame_config_bounds[4] = {
      make_rect_ie_u32(0, 0, 64, 48),
      make_rect_ie_u32(15, 31, 52, 40),
      make_rect_ie_u32(15, 0, 64, 40),
      make_rect_ie_u32(15, 0, 64, 40),
  };
  return do_test_wuffs_gif_decode_animated(
      "test/data/animated-red-blue.gif", want_num_loops,
      WUFFS_TESTLIB_ARRAY_SIZE(want_frame_config_bounds),
      want_frame_config_bounds);
}

const char*  //
test_wuffs_gif_decode_delay_num_frames_decoded() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/animated-red-blue.gif"));

  // Truncate the final 0x3B trailer byte.
  if (src.meta.wi <= 0) {
    return "src file is too short";
  } else if (src.data.ptr[src.meta.wi - 1] != 0x3B) {
    return "src file does not end with 0x3B";
  }
  src.meta.wi--;

  for (int q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_gif__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    wuffs_gif__decoder__set_quirk(&dec,
                                  WUFFS_GIF__QUIRK_DELAY_NUM_DECODED_FRAMES, q);

    while (true) {
      wuffs_base__status status =
          wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
      if (!wuffs_base__status__is_ok(&status)) {
        break;
      }
    }

    uint64_t have = wuffs_gif__decoder__num_decoded_frames(&dec);
    uint64_t want = q ? 3 : 4;
    if (have != want) {
      RETURN_FAIL("q=%d: num_decoded_frames: have %" PRIu64 ", want %" PRIu64,
                  q, have, want);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_empty_palette() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/artificial-gif/empty-palette.gif"));
  for (int q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_gif__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    wuffs_gif__decoder__set_quirk(&dec, WUFFS_GIF__QUIRK_REJECT_EMPTY_PALETTE,
                                  q);

    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    wuffs_base__status status =
        wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("q=%d: decode_image_config: \"%s\"", q, status.repr);
    }

    wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                      g_pixel_slice_u8);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("q=%d: set_from_slice: \"%s\"", q, status.repr);
    }

    for (int i = 0; i < 2; i++) {
      status = wuffs_gif__decoder__decode_frame(
          &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
      if ((q == 1) && (i == 1)) {
        if (status.repr != wuffs_gif__error__bad_palette) {
          RETURN_FAIL("q=%d: i=%d: decode_frame: have \"%s\", want \"%s\"", q,
                      i, status.repr, wuffs_gif__error__bad_palette);
        }
        break;
      }
      if (!wuffs_base__status__is_ok(&status)) {
        RETURN_FAIL("q=%d: i=%d: decode_frame: \"%s\"", q, i, status.repr);
      }

      wuffs_base__slice_u8 pal = wuffs_base__pixel_buffer__palette(&pb);
      if (pal.len < 4) {
        RETURN_FAIL("q=%d: i=%d: palette length: have %d, want >= 4", q, i,
                    (int)(pal.len));
      }
      uint8_t* p = pal.ptr;
      int blue = i ? 0x00 : 0xFF;
      if ((p[0] != blue) || (p[1] != 0x00) || (p[2] != 0x00) ||
          (p[3] != 0xFF)) {
        RETURN_FAIL(
            "q=%d: i=%d: palette[0] BGRA: have (0x%02X, 0x%02X, 0x%02X, "
            "0x%02X), want (0x%02X, 0x00, 0x00, 0xFF)",
            q, i, (int)(p[0]), (int)(p[1]), (int)(p[2]), (int)(p[3]), blue);
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_background_color() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, "test/data/artificial-gif/background-color.gif"));
  for (int q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_gif__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    wuffs_gif__decoder__set_quirk(&dec, WUFFS_GIF__QUIRK_HONOR_BACKGROUND_COLOR,
                                  q);

    wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
    wuffs_base__status status =
        wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("q=%d: decode_frame_config: \"%s\"", q, status.repr);
    }

    wuffs_base__color_u32_argb_premul have =
        wuffs_base__frame_config__background_color(&fc);
    wuffs_base__color_u32_argb_premul want = q ? 0xFF80C3C3 : 0x00000000;

    if (have != want) {
      RETURN_FAIL("q=%d: have 0x%08" PRIX32 ", want 0x%08" PRIX32, q, have,
                  want);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_first_frame_is_opaque() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, "test/data/artificial-gif/frame-out-of-bounds.gif"));
  for (int q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_gif__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    wuffs_gif__decoder__set_quirk(&dec, WUFFS_GIF__QUIRK_HONOR_BACKGROUND_COLOR,
                                  q);

    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    wuffs_base__status status =
        wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("q=%d: decode_image_config: \"%s\"", q, status.repr);
    }

    bool have = wuffs_base__image_config__first_frame_is_opaque(&ic);
    bool want = q;

    if (have != want) {
      RETURN_FAIL("q=%d: have %s, want %s", q, have ? "true" : "false",
                  want ? "true" : "false");
    }
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_frame_out_of_bounds() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, "test/data/artificial-gif/frame-out-of-bounds.gif"));
  for (int q = 0; q < 2; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_gif__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
    wuffs_gif__decoder__set_quirk(&dec,
                                  WUFFS_GIF__QUIRK_IMAGE_BOUNDS_ARE_STRICT, q);

    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    wuffs_base__status status =
        wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("q=%d: decode_image_config: \"%s\"", q, status.repr);
    }

    // The nominal width and height for the overall image is 2×2, but its first
    // frame extends those bounds to 4×2. See
    // test/data/artificial-gif/frame-out-of-bounds.gif.make-artificial.txt for
    // more discussion.

    const uint32_t width = q ? 2 : 4;
    const uint32_t height = 2;

    if (wuffs_base__pixel_config__width(&ic.pixcfg) != width) {
      RETURN_FAIL("q=%d: width: have %" PRIu32 ", want %" PRIu32, q,
                  wuffs_base__pixel_config__width(&ic.pixcfg), width);
    }
    if (wuffs_base__pixel_config__height(&ic.pixcfg) != height) {
      RETURN_FAIL("q=%d: height: have %" PRIu32 ", want %" PRIu32, q,
                  wuffs_base__pixel_config__height(&ic.pixcfg), height);
    }

    wuffs_base__pixel_config five_by_five = ((wuffs_base__pixel_config){});
    wuffs_base__pixel_config__set(
        &five_by_five, wuffs_base__pixel_config__pixel_format(&ic.pixcfg).repr,
        wuffs_base__pixel_config__pixel_subsampling(&ic.pixcfg).repr, 5, 5);
    wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &five_by_five,
                                                      g_pixel_slice_u8);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("q=%d: set_from_slice: \"%s\"", q, status.repr);
    }

    // See test/data/artificial-gif/frame-out-of-bounds.gif.make-artificial.txt
    // for the want_frame_config_bounds and want_pixel_indexes source.

    wuffs_base__rect_ie_u32 want_frame_config_bounds[4] = {
        q ? make_rect_ie_u32(1, 0, 2, 1) : make_rect_ie_u32(1, 0, 4, 1),
        q ? make_rect_ie_u32(0, 1, 2, 2) : make_rect_ie_u32(0, 1, 2, 2),
        q ? make_rect_ie_u32(0, 2, 1, 2) : make_rect_ie_u32(0, 2, 1, 2),
        q ? make_rect_ie_u32(2, 0, 2, 2) : make_rect_ie_u32(2, 0, 4, 2),
    };

    const char* want_pixel_indexes[4] = {
        q ? ".1.." : ".123....",
        q ? "..89" : "....89..",
        q ? "...." : "........",
        q ? "...." : "..45..89",
    };

    for (uint32_t i = 0; true; i++) {
      wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
      {
        status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
        if (i == WUFFS_TESTLIB_ARRAY_SIZE(want_frame_config_bounds)) {
          if (status.repr != wuffs_base__note__end_of_data) {
            RETURN_FAIL("q=%d: decode_frame_config #%" PRIu32
                        ": have \"%s\", want \"%s\"",
                        q, i, status.repr, wuffs_base__note__end_of_data);
          }
          break;
        }
        if (!wuffs_base__status__is_ok(&status)) {
          RETURN_FAIL("q=%d: decode_frame_config #%" PRIu32 ": \"%s\"", q, i,
                      status.repr);
        }

        wuffs_base__rect_ie_u32 have = wuffs_base__frame_config__bounds(&fc);
        wuffs_base__rect_ie_u32 want = want_frame_config_bounds[i];
        if (!wuffs_base__rect_ie_u32__equals(&have, want)) {
          RETURN_FAIL("q=%d: decode_frame_config #%" PRIu32
                      ": bounds: have (%" PRIu32 ", %" PRIu32 ")-(%" PRIu32
                      ", %" PRIu32 "), want (%" PRIu32 ", %" PRIu32
                      ")-(%" PRIu32 ", %" PRIu32 ")",
                      q, i, have.min_incl_x, have.min_incl_y, have.max_excl_x,
                      have.max_excl_y, want.min_incl_x, want.min_incl_y,
                      want.max_excl_x, want.max_excl_y);
        }
      }

      {
        wuffs_base__table_u8 p = wuffs_base__pixel_buffer__plane(&pb, 0);
        for (uint32_t y = 0; y < 5; y++) {
          for (uint32_t x = 0; x < 5; x++) {
            p.ptr[(y * p.stride) + x] = 0xEE;
          }
        }
        for (uint32_t y = 0; y < height; y++) {
          for (uint32_t x = 0; x < width; x++) {
            p.ptr[(y * p.stride) + x] = 0;
          }
        }

        status = wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                                  WUFFS_BASE__PIXEL_BLEND__SRC,
                                                  g_work_slice_u8, NULL);
        if (!wuffs_base__status__is_ok(&status)) {
          RETURN_FAIL("q=%d: decode_frame #%" PRIu32 ": \"%s\"", q, i,
                      status.repr);
        }

        for (uint32_t y = 0; y < 5; y++) {
          for (uint32_t x = 0; x < 5; x++) {
            if ((x < width) && (y < height)) {
              continue;
            }
            if (p.ptr[(y * p.stride) + x] != 0xEE) {
              RETURN_FAIL("q=%d: decode_frame #%" PRIu32
                          ": dirty pixel (%" PRIu32 ", %" PRIu32
                          ") outside of image bounds",
                          q, i, x, y);
            }
          }
        }

        wuffs_base__rect_ie_u32 frame_rect =
            wuffs_base__frame_config__bounds(&fc);
        wuffs_base__rect_ie_u32 dirty_rect =
            wuffs_gif__decoder__frame_dirty_rect(&dec);
        if (!wuffs_base__rect_ie_u32__contains_rect(&frame_rect, dirty_rect)) {
          RETURN_FAIL(
              "q=%d: internal error: frame_rect does not contain dirty_rect",
              q);
        }

        char have[(width * height) + 1];
        for (uint32_t y = 0; y < height; y++) {
          for (uint32_t x = 0; x < width; x++) {
            uint8_t index = 0x0F & p.ptr[(y * p.stride) + x];
            have[(y * width) + x] = index ? ('0' + index) : '.';
          }
        }
        have[width * height] = 0;

        const char* want = want_pixel_indexes[i];
        if (memcmp(have, want, width * height)) {
          RETURN_FAIL("q=%d: decode_frame #%" PRIu32
                      ": have \"%s\", want \"%s\"",
                      q, i, have, want);
        }
      }
    }
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_zero_width_frame() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, "test/data/artificial-gif/zero-width-frame.gif"));
  for (int q = 0; q < 3; q++) {
    src.meta.ri = 0;

    wuffs_gif__decoder dec;
    CHECK_STATUS("initialize",
                 wuffs_gif__decoder__initialize(
                     &dec, sizeof dec, WUFFS_VERSION,
                     WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

    const char* want = NULL;
    switch (q) {
      case 0:
        want = wuffs_base__error__too_much_data;
        break;
      case 1:
        want = NULL;
        wuffs_gif__decoder__set_quirk(
            &dec, WUFFS_GIF__QUIRK_IGNORE_TOO_MUCH_PIXEL_DATA, 1);
        break;
      case 2:
        want = wuffs_gif__error__bad_frame_size;
        wuffs_gif__decoder__set_quirk(&dec, WUFFS_GIF__QUIRK_REJECT_EMPTY_FRAME,
                                      1);
        break;
    }

    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    wuffs_base__status status =
        wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("q=%d: decode_image_config: \"%s\"", q, status.repr);
    }

    wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
    status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg,
                                                      g_pixel_slice_u8);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("q=%d: set_from_slice: \"%s\"", q, status.repr);
    }

    wuffs_base__status have = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
    if (have.repr != want) {
      RETURN_FAIL("q=%d: decode_frame: have \"%s\", want \"%s\"", q, have.repr,
                  want);
    }
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_pixfmt_bgr() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", UINT64_MAX,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGR));
}

const char*  //
test_wuffs_gif_decode_pixfmt_bgr_565() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", UINT64_MAX,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGR_565));
}

const char*  //
test_wuffs_gif_decode_pixfmt_bgra_nonpremul() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", UINT64_MAX,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL));
}

const char*  //
test_wuffs_gif_decode_pixfmt_rgb() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", UINT64_MAX,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__RGB));
}

const char*  //
test_wuffs_gif_decode_pixfmt_rgba_nonpremul() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", UINT64_MAX,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL));
}

const char*  //
test_wuffs_gif_decode_input_is_a_gif_just_one_read() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", UINT64_MAX,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY));
}

const char*  //
test_wuffs_gif_decode_input_is_a_gif_many_big_reads() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", 4096,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY));
}

const char*  //
test_wuffs_gif_decode_input_is_a_gif_many_medium_reads() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", 787,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY));
  // The magic 787 tickles being in the middle of a decode_extension skip
  // call.
  //
  // TODO: has 787 changed since we decode the image_config separately?
}

const char*  //
test_wuffs_gif_decode_input_is_a_gif_many_small_reads() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode(
      "test/data/bricks-dither.gif", "test/data/bricks-dither.palette",
      "test/data/bricks-dither.indexes", 13,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY));
}

const char*  //
test_wuffs_gif_decode_input_is_a_png() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/bricks-dither.png"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  wuffs_base__image_config ic = ((wuffs_base__image_config){});

  wuffs_base__status status =
      wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
  if (status.repr != wuffs_gif__error__bad_header) {
    RETURN_FAIL("decode_image_config: have \"%s\", want \"%s\"", status.repr,
                wuffs_gif__error__bad_header);
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_interlaced_truncated() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, "test/data/hippopotamus.interlaced.truncated.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  wuffs_base__image_config ic = ((wuffs_base__image_config){});

  CHECK_STATUS("decode_image_config",
               wuffs_gif__decoder__decode_image_config(&dec, &ic, &src));
  if (wuffs_base__pixel_config__width(&ic.pixcfg) != 36) {
    RETURN_FAIL("width: have %" PRIu32 ", want 36",
                wuffs_base__pixel_config__width(&ic.pixcfg));
  }
  if (wuffs_base__pixel_config__height(&ic.pixcfg) != 28) {
    RETURN_FAIL("height: have %" PRIu32 ", want 28",
                wuffs_base__pixel_config__height(&ic.pixcfg));
  }
  if (wuffs_base__pixel_config__pixel_format(&ic.pixcfg).repr !=
      WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY) {
    RETURN_FAIL("pixel_format: have 0x%08" PRIX32 ", want 0x%08" PRIX32,
                wuffs_base__pixel_config__pixel_format(&ic.pixcfg).repr,
                WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY);
  }
  const int num_pixel_indexes = 36 * 28 * 1;

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                     &pb, &ic.pixcfg, g_pixel_slice_u8));
  uint8_t* pixel_ptr = wuffs_base__pixel_buffer__plane(&pb, 0).ptr;
  memset(pixel_ptr, 0xEE, num_pixel_indexes);

  if (pixel_ptr[num_pixel_indexes - 1] != 0xEE) {
    RETURN_FAIL("final pixel index, before: have 0x%02X, want 0x%02X",
                pixel_ptr[num_pixel_indexes - 1], 0xEE);
  }

  wuffs_base__status status = wuffs_gif__decoder__decode_frame(
      &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
  if (status.repr != wuffs_gif__error__truncated_input) {
    RETURN_FAIL("decode_frame: have \"%s\", want \"%s\"", status.repr,
                wuffs_gif__error__truncated_input);
  }

  // Even though the source GIF data was truncated, replicating the interlaced
  // rows should have set the bottom right pixel.
  if (pixel_ptr[num_pixel_indexes - 1] == 0xEE) {
    RETURN_FAIL("final pixel index, after: have 0x%02X, want not 0x%02X",
                pixel_ptr[num_pixel_indexes - 1], 0xEE);
  }

  wuffs_base__rect_ie_u32 r = wuffs_gif__decoder__frame_dirty_rect(&dec);
  if (r.max_excl_y != 28) {
    RETURN_FAIL("frame_dirty_rect max_excl_y: have %" PRIu32 ", want 28",
                r.max_excl_y);
  }
  return NULL;
}

const char*  //
do_test_wuffs_gif_decode_metadata(bool full) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, full ? "test/data/artificial-gif/metadata-full.gif"
                           : "test/data/artificial-gif/metadata-empty.gif"));

  int iccp;
  for (iccp = 0; iccp < 2; iccp++) {
    int xmp;
    for (xmp = 0; xmp < 2; xmp++) {
      bool seen_iccp = false;
      bool seen_xmp = false;

      wuffs_gif__decoder dec;
      CHECK_STATUS("initialize",
                   wuffs_gif__decoder__initialize(
                       &dec, sizeof dec, WUFFS_VERSION,
                       WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

      if (iccp) {
        wuffs_gif__decoder__set_report_metadata(&dec, WUFFS_BASE__FOURCC__ICCP,
                                                true);
      }
      if (xmp) {
        wuffs_gif__decoder__set_report_metadata(&dec, WUFFS_BASE__FOURCC__XMP,
                                                true);
      }

      wuffs_base__image_config ic = ((wuffs_base__image_config){});
      src.meta.ri = 0;

      while (true) {
        wuffs_base__status status =
            wuffs_gif__decoder__decode_image_config(&dec, &ic, &src);
        if (wuffs_base__status__is_ok(&status)) {
          break;
        } else if (status.repr != wuffs_base__note__metadata_reported) {
          RETURN_FAIL(
              "decode_image_config (iccp=%d, xmp=%d): have \"%s\", want \"%s\"",
              iccp, xmp, status.repr, wuffs_base__note__metadata_reported);
        }

        const char* want = "";
        char have_buffer[100];
        int have_length = 0;
        uint32_t have_fourcc = 0;

        while (true) {
          wuffs_base__io_buffer empty = wuffs_base__empty_io_buffer();
          wuffs_base__more_information minfo =
              wuffs_base__empty_more_information();
          wuffs_base__status status =
              wuffs_gif__decoder__tell_me_more(&dec, &empty, &minfo, &src);
          if (wuffs_base__status__is_error(&status)) {
            RETURN_FAIL("tell_me_more (iccp=%d, xmp=%d): \"%s\"", iccp, xmp,
                        status.repr);
          } else if (
              minfo.flavor !=
              WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_PASSTHROUGH) {
            RETURN_FAIL(
                "tell_me_more (iccp=%d, xmp=%d): flavor: have %" PRIu32
                ", want %" PRIu32,
                iccp, xmp, minfo.flavor,
                WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_PASSTHROUGH);
          }

          have_fourcc = wuffs_base__more_information__metadata__fourcc(&minfo);
          switch (have_fourcc) {
            case WUFFS_BASE__FOURCC__ICCP:
              want = full ? "\x16\x26\x36\x46\x56\x76\x86\x96" : "";
              seen_iccp = true;
              break;
            case WUFFS_BASE__FOURCC__XMP:
              want = full ? "\x05\x17\x27\x37\x47\x57\x03\x77\x87\x97" : "";
              seen_xmp = true;
              break;
            default:
              RETURN_FAIL(
                  "metadata_fourcc (iccp=%d, xmp=%d): unexpected FourCC "
                  "0x%08" PRIX32,
                  iccp, xmp, have_fourcc);
          }

          wuffs_base__range_ie_u64 r =
              wuffs_base__more_information__metadata_raw_passthrough__range(
                  &minfo);
          uint64_t n = wuffs_base__range_ie_u64__length(&r);
          if ((n > 100) || (n + have_length > 100)) {
            RETURN_FAIL(
                "metadata chunk length (iccp=%d, xmp=%d): too much "
                "metadata (vs buffer size)",
                iccp, xmp);
          }
          if (n > wuffs_base__io_buffer__reader_length(&src)) {
            RETURN_FAIL(
                "metadata chunk length (iccp=%d, xmp=%d): too much "
                "metadata (vs available)",
                iccp, xmp);
          }
          memcpy(have_buffer + have_length, src.data.ptr + src.meta.ri, n);
          have_length += n;
          src.meta.ri += n;

          if (wuffs_base__status__is_ok(&status)) {
            break;
          } else if (status.repr !=
                     wuffs_base__suspension__even_more_information) {
            RETURN_FAIL(
                "tell_me_more (iccp=%d, xmp=%d): have \"%s\", want \"%s\"",
                iccp, xmp, status.repr,
                wuffs_base__suspension__even_more_information);
          }
        }

        int want_length = strlen(want);
        if ((have_length != want_length) ||
            strncmp(have_buffer, want, want_length)) {
          RETURN_FAIL("metadata (iccp=%d, xmp=%d): fourcc=0x%08" PRIX32
                      ": values differed",
                      iccp, xmp, have_fourcc);
        }
      }

      if (iccp != seen_iccp) {
        RETURN_FAIL("seen_iccp (iccp=%d, xmp=%d): have %d, want %d", iccp, xmp,
                    seen_iccp, iccp);
      }

      if (xmp != seen_xmp) {
        RETURN_FAIL("seen_xmp (iccp=%d, xmp=%d): have %d, want %d", iccp, xmp,
                    seen_xmp, xmp);
      }

      {
        uint64_t have = wuffs_base__image_config__first_frame_io_position(&ic);
        uint64_t want = 25;
        if (have != want) {
          RETURN_FAIL("first_frame_io_position: have %" PRIu64
                      ", want %" PRIu64,
                      have, want);
        }
      }

      {
        uint32_t have = wuffs_gif__decoder__num_animation_loops(&dec);
        uint32_t want = 2001;
        if (have != want) {
          RETURN_FAIL("num_animation_loops: have %" PRIu32 ", want %" PRIu32,
                      have, want);
        }
      }

      {
        wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
        wuffs_base__status status =
            wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
        if (!wuffs_base__status__is_ok(&status)) {
          RETURN_FAIL("decode_frame_config (iccp=%d, xmp=%d): %s", iccp, xmp,
                      status.repr);
        }
        uint32_t have = wuffs_base__frame_config__width(&fc);
        uint32_t want = 1;
        if (have != want) {
          RETURN_FAIL(
              "decode_frame_config (iccp=%d, xmp=%d): width: have %" PRIu32
              ", want %" PRIu32,
              iccp, xmp, have, want);
        }
      }
    }
  }

  return NULL;
}

const char*  //
test_wuffs_gif_decode_metadata_empty() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode_metadata(false);
}

const char*  //
test_wuffs_gif_decode_metadata_full() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_decode_metadata(true);
}

const char*  //
test_wuffs_gif_decode_missing_two_src_bytes() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/pjw-thumbnail.gif"));

  // Trim the final two bytes: the 0x00 end-of-block and the 0x3B trailer.
  if (src.meta.wi < 2) {
    return "src file is too short";
  }
  src.meta.wi -= 2;

  return do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_gif__error__truncated_input, false);
}

const char*  //
test_wuffs_gif_decode_multiple_graphic_controls() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(
      &src, "test/data/artificial-gif/multiple-graphic-controls.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
  CHECK_STATUS("decode_frame_config",
               wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src));

  int have = wuffs_base__frame_config__duration(&fc) /
             WUFFS_BASE__FLICKS_PER_MILLISECOND;
  int want = 300;
  if (have != want) {
    RETURN_FAIL("duration: have %d, want %d", have, want);
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_multiple_loop_counts() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, "test/data/artificial-gif/multiple-loop-counts.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));
  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  CHECK_STATUS("decode_image_config",
               wuffs_gif__decoder__decode_image_config(&dec, &ic, &src));

  // See test/data/artificial-gif/multiple-loop-counts.gif.make-artificial.txt
  // for the want_num_animation_loops source. Note that the GIF wire format's
  // loop counts are the number of animation loops *after* the first play
  // through, and Wuffs' are the number *including* the first play through.

  uint32_t want_num_animation_loops[4] = {0, 1, 51, 31};

  uint32_t i = 0;
  while (true) {
    wuffs_base__status status =
        wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
    if (i == WUFFS_TESTLIB_ARRAY_SIZE(want_num_animation_loops)) {
      if (status.repr != wuffs_base__note__end_of_data) {
        RETURN_FAIL("decode_frame_config #%" PRIu32
                    ": have \"%s\", want \"%s\"",
                    i, status.repr, wuffs_base__note__end_of_data);
      }
      break;
    }
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("decode_frame_config #%" PRIu32 ": \"%s\"", i, status.repr);
    }

    uint32_t have = wuffs_gif__decoder__num_animation_loops(&dec);
    uint32_t want = want_num_animation_loops[i];
    if (have != want) {
      RETURN_FAIL("num_animation_loops #%" PRIu32 ": have %" PRIu32
                  ", want %" PRIu32,
                  i, have, want);
    }
    i++;
  }

  uint32_t have = wuffs_gif__decoder__num_animation_loops(&dec);
  uint32_t want = 41;
  if (have != want) {
    RETURN_FAIL("num_animation_loops #%" PRIu32 ": have %" PRIu32
                ", want %" PRIu32,
                i, have, want);
  }
  return NULL;
}

const char*  //
test_wuffs_gif_decode_nul_byte_trailer() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/pjw-thumbnail.gif"));

  // Change the final 0x3B trailer byte to 0x00.
  if (src.meta.wi <= 0) {
    return "src file is too short";
  } else if (src.data.ptr[src.meta.wi - 1] != 0x3B) {
    return "src file does not end with 0x3B";
  }
  src.data.ptr[src.meta.wi - 1] = 0x00;

  return do_test_wuffs_gif_decode_expecting(src, 0, NULL, false);
}

const char*  //
test_wuffs_gif_decode_pixel_data_none() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/artificial-gif/pixel-data-none.gif"));

  return do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_base__error__not_enough_data, true);
}

const char*  //
test_wuffs_gif_decode_pixel_data_not_enough() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, "test/data/artificial-gif/pixel-data-not-enough.gif"));

  return do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_base__error__not_enough_data, false);
}

const char*  //
test_wuffs_gif_decode_pixel_data_too_much_sans_quirk() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });

  src.meta = wuffs_base__empty_io_buffer_meta();
  CHECK_STRING(read_file(
      &src, "test/data/artificial-gif/pixel-data-too-much-bad-lzw.gif"));
  CHECK_STRING(do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_gif__error__bad_lzw_code, false));

  src.meta = wuffs_base__empty_io_buffer_meta();
  CHECK_STRING(read_file(
      &src, "test/data/artificial-gif/pixel-data-too-much-good-lzw.gif"));
  CHECK_STRING(do_test_wuffs_gif_decode_expecting(
      src, 0, wuffs_base__error__too_much_data, false));

  return NULL;
}

const char*  //
test_wuffs_gif_decode_pixel_data_too_much_with_quirk() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });

  src.meta = wuffs_base__empty_io_buffer_meta();
  CHECK_STRING(read_file(
      &src, "test/data/artificial-gif/pixel-data-too-much-bad-lzw.gif"));
  CHECK_STRING(do_test_wuffs_gif_decode_expecting(
      src, WUFFS_GIF__QUIRK_IGNORE_TOO_MUCH_PIXEL_DATA, NULL, false));

  src.meta = wuffs_base__empty_io_buffer_meta();
  CHECK_STRING(read_file(
      &src, "test/data/artificial-gif/pixel-data-too-much-good-lzw.gif"));
  CHECK_STRING(do_test_wuffs_gif_decode_expecting(
      src, WUFFS_GIF__QUIRK_IGNORE_TOO_MUCH_PIXEL_DATA, NULL, false));

  return NULL;
}

const char*  //
test_wuffs_gif_frame_dirty_rect() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/hippopotamus.interlaced.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  CHECK_STATUS("decode_image_config",
               wuffs_gif__decoder__decode_image_config(&dec, &ic, &src));

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                     &pb, &ic.pixcfg, g_pixel_slice_u8));

  // The hippopotamus.interlaced.gif image is 28 pixels high. As we decode rows
  // of pixels, interlacing means that we decode rows 0, 8, 16, 24, 4, 12, 20,
  // 2, 6, 10, ..., 22, 26, 1, 3, 5, ..., 25, 27. As we progress, the dirty
  // rect's max_excl_y should be one more than the highest decoded row so far,
  // until the row is complete, when it is replicated (to a multiple of 8). If
  // we haven't decoded any rows yet, max_excl_y should be zero.
  uint32_t wants[9] = {0, 1, 8, 9, 16, 17, 24, 25, 28};
  int i = 0;

  while (true) {
    wuffs_base__io_buffer limited_src = make_limited_reader(src, 1);

    wuffs_base__status status = wuffs_gif__decoder__decode_frame(
        &dec, &pb, &limited_src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8,
        NULL);
    src.meta.ri += limited_src.meta.ri;

    wuffs_base__rect_ie_u32 r = wuffs_gif__decoder__frame_dirty_rect(&dec);
    if ((i < WUFFS_TESTLIB_ARRAY_SIZE(wants)) && (wants[i] == r.max_excl_y)) {
      i++;
    }

    if (wuffs_base__status__is_ok(&status)) {
      break;
    } else if (status.repr != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_frame: have \"%s\", want \"%s\"", status.repr,
                  wuffs_base__suspension__short_read);
    }
  }

  if (i != WUFFS_TESTLIB_ARRAY_SIZE(wants)) {
    RETURN_FAIL("i: have %d, want %d", i,
                (int)(WUFFS_TESTLIB_ARRAY_SIZE(wants)));
  }
  return NULL;
}

const char*  //
do_test_wuffs_gif_num_decoded(bool frame_config) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/animated-red-blue.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  if (!frame_config) {
    wuffs_base__image_config ic = ((wuffs_base__image_config){});
    CHECK_STATUS("decode_image_config",
                 wuffs_gif__decoder__decode_image_config(&dec, &ic, &src));

    CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                       &pb, &ic.pixcfg, g_pixel_slice_u8));
  }

  const char* method = frame_config ? "decode_frame_config" : "decode_frame";
  bool end_of_data = false;
  uint64_t want = 0;
  while (true) {
    uint64_t have =
        frame_config
            ? (uint64_t)(wuffs_gif__decoder__num_decoded_frame_configs(&dec))
            : (uint64_t)(wuffs_gif__decoder__num_decoded_frames(&dec));
    if (have != want) {
      RETURN_FAIL("num_%ss: have %" PRIu64 ", want %" PRIu64, method, have,
                  want);
    }

    if (end_of_data) {
      break;
    }

    wuffs_base__status status = wuffs_base__make_status(NULL);
    if (frame_config) {
      status = wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
    } else {
      status = wuffs_gif__decoder__decode_frame(
          &dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, g_work_slice_u8, NULL);
    }

    if (wuffs_base__status__is_ok(&status)) {
      want++;
    } else if (status.repr == wuffs_base__note__end_of_data) {
      end_of_data = true;
    } else {
      RETURN_FAIL("%s: have \"%s\", want \"%s\"", method, status.repr,
                  wuffs_base__note__end_of_data);
    }
  }

  if (want != 4) {
    RETURN_FAIL("%s: have %" PRIu64 ", want 4", method, want);
  }
  return NULL;
}

const char*  //
test_wuffs_gif_num_decoded_frame_configs() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_num_decoded(true);
}

const char*  //
test_wuffs_gif_num_decoded_frames() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_num_decoded(false);
}

const char*  //
do_test_wuffs_gif_io_position(bool chunked) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, "test/data/animated-red-blue.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  if (chunked) {
    if (src.meta.wi < 50) {
      RETURN_FAIL("src is too short");
    }
    size_t saved_wi = src.meta.wi;
    bool saved_closed = src.meta.closed;
    src.meta.wi = 30;
    src.meta.closed = false;

    wuffs_base__status status =
        wuffs_gif__decoder__decode_image_config(&dec, NULL, &src);
    if (status.repr != wuffs_base__suspension__short_read) {
      RETURN_FAIL("decode_image_config (chunked): have \"%s\", want \"%s\"",
                  status.repr, wuffs_base__suspension__short_read);
    }

    src.meta.wi = saved_wi;
    src.meta.closed = saved_closed;

    if (src.meta.pos != 0) {
      RETURN_FAIL("src.meta.pos: have %" PRIu64 ", want zero", src.meta.pos);
    }
    wuffs_base__io_buffer__compact(&src);
    if (src.meta.pos == 0) {
      RETURN_FAIL("src.meta.pos: have %" PRIu64 ", want non-zero",
                  src.meta.pos);
    }
  }

  CHECK_STATUS("decode_image_config",
               wuffs_gif__decoder__decode_image_config(&dec, NULL, &src));

  wuffs_base__frame_config fcs[4];
  uint32_t width_wants[4] = {64, 37, 49, 49};
  uint64_t pos_wants[4] = {781, 2126, 2187, 2542};
  for (int i = 0; i < 4; i++) {
    fcs[i] = ((wuffs_base__frame_config){});
    wuffs_base__status status =
        wuffs_gif__decoder__decode_frame_config(&dec, &fcs[i], &src);
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("decode_frame_config #%d: \"%s\"", i, status.repr);
    }

    uint64_t index_have = wuffs_base__frame_config__index(&fcs[i]);
    uint64_t index_want = i;
    if (index_have != index_want) {
      RETURN_FAIL("index #%d: have %" PRIu64 ", want %" PRIu64, i, index_have,
                  index_want);
    }

    uint32_t width_have = wuffs_base__frame_config__width(&fcs[i]);
    uint32_t width_want = width_wants[i];
    if (width_have != width_want) {
      RETURN_FAIL("width #%d: have %" PRIu32 ", want %" PRIu32, i, width_have,
                  width_want);
    }

    uint64_t pos_have = wuffs_base__frame_config__io_position(&fcs[i]);
    uint64_t pos_want = pos_wants[i];
    if (pos_have != pos_want) {
      RETURN_FAIL("io_position #%d: have %" PRIu64 ", want %" PRIu64, i,
                  pos_have, pos_want);
    }

    // Look for the 0x21 byte that's a GIF's Extension Introducer. Not every
    // GIF's frame_config's I/O position will point to 0x21, as an 0x2C Image
    // Separator is also valid. But for animated-red-blue.gif, it'll be 0x21.
    if (pos_have < src.meta.pos) {
      RETURN_FAIL("io_position #%d: have %" PRIu64 ", was too small", i,
                  pos_have);
    }
    uint64_t src_ptr_offset = pos_have - src.meta.pos;
    if (src_ptr_offset >= src.meta.wi) {
      RETURN_FAIL("io_position #%d: have %" PRIu64 ", was too large", i,
                  pos_have);
    }
    uint8_t x = src.data.ptr[src_ptr_offset];
    if (x != 0x21) {
      RETURN_FAIL("Image Descriptor byte #%d: have 0x%02X, want 0x2C", i,
                  (int)x);
    }
  }

  wuffs_base__status status =
      wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
  if (status.repr != wuffs_base__note__end_of_data) {
    RETURN_FAIL("decode_frame_config EOD: have \"%s\", want \"%s\"",
                status.repr, wuffs_base__note__end_of_data);
  }

  // If we're chunked, we've discarded some source bytes due to an earlier
  // wuffs_base__io_buffer__compact call. We won't bother testing
  // wuffs_gif__decoder__restart_frame in that case.
  if (chunked) {
    return NULL;
  }

  for (int i = 0; i < 4; i++) {
    src.meta.ri = pos_wants[i];

    status = wuffs_gif__decoder__restart_frame(
        &dec, i, wuffs_base__frame_config__io_position(&fcs[i]));
    if (!wuffs_base__status__is_ok(&status)) {
      RETURN_FAIL("restart_frame #%d: \"%s\"", i, status.repr);
    }

    for (int j = i; j < 4; j++) {
      wuffs_base__frame_config fc = ((wuffs_base__frame_config){});
      status = wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src);
      if (!wuffs_base__status__is_ok(&status)) {
        RETURN_FAIL("decode_frame_config #%d, #%d: \"%s\"", i, j, status.repr);
      }

      uint64_t index_have = wuffs_base__frame_config__index(&fc);
      uint64_t index_want = j;
      if (index_have != index_want) {
        RETURN_FAIL("index #%d, #%d: have %" PRIu64 ", want %" PRIu64, i, j,
                    index_have, index_want);
      }

      uint32_t width_have = wuffs_base__frame_config__width(&fc);
      uint32_t width_want = width_wants[j];
      if (width_have != width_want) {
        RETURN_FAIL("width #%d, #%d: have %" PRIu32 ", want %" PRIu32, i, j,
                    width_have, width_want);
      }
    }

    status = wuffs_gif__decoder__decode_frame_config(&dec, NULL, &src);
    if (status.repr != wuffs_base__note__end_of_data) {
      RETURN_FAIL("decode_frame_config #%d: have \"%s\", want \"%s\"", i,
                  status.repr, wuffs_base__note__end_of_data);
    }
  }

  return NULL;
}

const char*  //
test_wuffs_gif_io_position_one_chunk() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_io_position(false);
}

const char*  //
test_wuffs_gif_io_position_two_chunks() {
  CHECK_FOCUS(__func__);
  return do_test_wuffs_gif_io_position(true);
}

const char*  //
test_wuffs_gif_small_frame_interlaced() {
  CHECK_FOCUS(__func__);
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(
      read_file(&src, "test/data/artificial-gif/small-frame-interlaced.gif"));

  wuffs_gif__decoder dec;
  CHECK_STATUS("initialize",
               wuffs_gif__decoder__initialize(
                   &dec, sizeof dec, WUFFS_VERSION,
                   WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED));

  wuffs_base__image_config ic = ((wuffs_base__image_config){});
  CHECK_STATUS("decode_image_config",
               wuffs_gif__decoder__decode_image_config(&dec, &ic, &src));
  if (wuffs_base__pixel_config__width(&ic.pixcfg) != 5) {
    RETURN_FAIL("width: have %" PRIu32 ", want 5",
                wuffs_base__pixel_config__width(&ic.pixcfg));
  }
  if (wuffs_base__pixel_config__height(&ic.pixcfg) != 5) {
    RETURN_FAIL("height: have %" PRIu32 ", want 5",
                wuffs_base__pixel_config__height(&ic.pixcfg));
  }

  wuffs_base__pixel_buffer pb = ((wuffs_base__pixel_buffer){});
  CHECK_STATUS("set_from_slice", wuffs_base__pixel_buffer__set_from_slice(
                                     &pb, &ic.pixcfg, g_pixel_slice_u8));

  wuffs_base__frame_config fc;
  CHECK_STATUS("decode_frame_config",
               wuffs_gif__decoder__decode_frame_config(&dec, &fc, &src));

  wuffs_base__rect_ie_u32 fr = wuffs_base__frame_config__bounds(&fc);
  if (fr.max_excl_y != 3) {
    RETURN_FAIL("frame rect max_excl_y: have %" PRIu32 ", want 3",
                fr.max_excl_y);
  }

  CHECK_STATUS("decode_frame",
               wuffs_gif__decoder__decode_frame(&dec, &pb, &src,
                                                WUFFS_BASE__PIXEL_BLEND__SRC,
                                                g_work_slice_u8, NULL));

  wuffs_base__rect_ie_u32 dr = wuffs_gif__decoder__frame_dirty_rect(&dec);
  if (dr.max_excl_y != 3) {
    RETURN_FAIL("dirty rect max_excl_y: have %" PRIu32 ", want 3",
                dr.max_excl_y);
  }

  return NULL;
}

// ---------------- Mimic Tests

#ifdef WUFFS_MIMIC

const char*  //
do_test_mimic_gif_decode(const char* filename) {
  wuffs_base__io_buffer src = ((wuffs_base__io_buffer){
      .data = g_src_slice_u8,
  });
  CHECK_STRING(read_file(&src, filename));

  src.meta.ri = 0;
  wuffs_base__io_buffer have = ((wuffs_base__io_buffer){
      .data = g_have_slice_u8,
  });
  CHECK_STRING(
      wuffs_gif_decode(NULL, &have, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
                       wuffs_base__make_pixel_format(
                           WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
                       NULL, 0, &src));

  src.meta.ri = 0;
  wuffs_base__io_buffer want = ((wuffs_base__io_buffer){
      .data = g_want_slice_u8,
  });
  CHECK_STRING(
      mimic_gif_decode(NULL, &want, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
                       wuffs_base__make_pixel_format(
                           WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
                       NULL, 0, &src));

  CHECK_STRING(check_io_buffers_equal("", &have, &want));

  // TODO: check the palette RGB values, not just the palette indexes
  // (pixels).

  return NULL;
}

const char*  //
test_mimic_gif_decode_animated_small() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/animated-red-blue.gif");
}

const char*  //
test_mimic_gif_decode_bricks_dither() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/bricks-dither.gif");
}

const char*  //
test_mimic_gif_decode_bricks_gray() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/bricks-gray.gif");
}

const char*  //
test_mimic_gif_decode_bricks_nodither() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/bricks-nodither.gif");
}

const char*  //
test_mimic_gif_decode_gifplayer_muybridge() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/gifplayer-muybridge.gif");
}

const char*  //
test_mimic_gif_decode_harvesters() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/harvesters.gif");
}

const char*  //
test_mimic_gif_decode_hat() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hat.gif");
}

const char*  //
test_mimic_gif_decode_hibiscus_primitive() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hibiscus.primitive.gif");
}

const char*  //
test_mimic_gif_decode_hibiscus_regular() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hibiscus.regular.gif");
}

const char*  //
test_mimic_gif_decode_hippopotamus_interlaced() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hippopotamus.interlaced.gif");
}

const char*  //
test_mimic_gif_decode_hippopotamus_regular() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/hippopotamus.regular.gif");
}

const char*  //
test_mimic_gif_decode_muybridge() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/muybridge.gif");
}

const char*  //
test_mimic_gif_decode_pjw_thumbnail() {
  CHECK_FOCUS(__func__);
  return do_test_mimic_gif_decode("test/data/pjw-thumbnail.gif");
}

#endif  // WUFFS_MIMIC

// ---------------- GIF Benches

const char*  //
bench_wuffs_gif_decode_1k_bw() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/pjw-thumbnail.gif", 0, SIZE_MAX, 2000);
}

const char*  //
bench_wuffs_gif_decode_1k_color_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hippopotamus.regular.gif", 0, SIZE_MAX, 1000);
}

const char*  //
bench_wuffs_gif_decode_1k_color_part_init() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hippopotamus.regular.gif", 0, SIZE_MAX, 1000);
}

const char*  //
bench_wuffs_gif_decode_10k_bgra() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL),
      NULL, 0, "test/data/hat.gif", 0, SIZE_MAX, 100);
}

const char*  //
bench_wuffs_gif_decode_10k_indexed() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hat.gif", 0, SIZE_MAX, 100);
}

const char*  //
bench_wuffs_gif_decode_20k() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/bricks-gray.gif", 0, SIZE_MAX, 50);
}

const char*  //
bench_wuffs_gif_decode_100k_artificial() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hibiscus.primitive.gif", 0, SIZE_MAX, 15);
}

const char*  //
bench_wuffs_gif_decode_100k_realistic() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hibiscus.regular.gif", 0, SIZE_MAX, 10);
}

const char*  //
bench_wuffs_gif_decode_1000k_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__DEFAULT_OPTIONS,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/harvesters.gif", 0, SIZE_MAX, 1);
}

const char*  //
bench_wuffs_gif_decode_1000k_part_init() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/harvesters.gif", 0, SIZE_MAX, 1);
}

const char*  //
bench_wuffs_gif_decode_anim_screencap() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      wuffs_gif_decode, WUFFS_INITIALIZE__LEAVE_INTERNAL_BUFFERS_UNINITIALIZED,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/gifplayer-muybridge.gif", 0, SIZE_MAX, 1);
}

// ---------------- Mimic Benches

#ifdef WUFFS_MIMIC

const char*  //
bench_mimic_gif_decode_1k_bw() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      mimic_gif_decode, 0,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/pjw-thumbnail.gif", 0, SIZE_MAX, 2000);
}

const char*  //
bench_mimic_gif_decode_1k_color_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      mimic_gif_decode, 0,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hippopotamus.regular.gif", 0, SIZE_MAX, 1000);
}

const char*  //
bench_mimic_gif_decode_10k_indexed() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      mimic_gif_decode, 0,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hat.gif", 0, SIZE_MAX, 100);
}

const char*  //
bench_mimic_gif_decode_20k() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      mimic_gif_decode, 0,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/bricks-gray.gif", 0, SIZE_MAX, 50);
}

const char*  //
bench_mimic_gif_decode_100k_artificial() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      mimic_gif_decode, 0,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hibiscus.primitive.gif", 0, SIZE_MAX, 15);
}

const char*  //
bench_mimic_gif_decode_100k_realistic() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      mimic_gif_decode, 0,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/hibiscus.regular.gif", 0, SIZE_MAX, 10);
}

const char*  //
bench_mimic_gif_decode_1000k_full_init() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      mimic_gif_decode, 0,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/harvesters.gif", 0, SIZE_MAX, 1);
}

const char*  //
bench_mimic_gif_decode_anim_screencap() {
  CHECK_FOCUS(__func__);
  return do_bench_image_decode(
      mimic_gif_decode, 0,
      wuffs_base__make_pixel_format(
          WUFFS_BASE__PIXEL_FORMAT__INDEXED__BGRA_BINARY),
      NULL, 0, "test/data/gifplayer-muybridge.gif", 0, SIZE_MAX, 1);
}

#endif  // WUFFS_MIMIC

// ---------------- Manifest

proc g_tests[] = {

    // These basic tests are really testing the Wuffs compiler. They aren't
    // specific to the std/gif code, but putting them here is as good as any
    // other place.
    test_basic_bad_receiver,
    test_basic_bad_sizeof_receiver,
    test_basic_bad_wuffs_version,
    test_basic_initialize_not_called,
    test_basic_status_is_error,

    test_wuffs_gif_call_interleaved,
    test_wuffs_gif_call_sequence,
    test_wuffs_gif_decode_animated_big,
    test_wuffs_gif_decode_animated_medium,
    test_wuffs_gif_decode_animated_small,
    test_wuffs_gif_decode_background_color,
    test_wuffs_gif_decode_delay_num_frames_decoded,
    test_wuffs_gif_decode_empty_palette,
    test_wuffs_gif_decode_first_frame_is_opaque,
    test_wuffs_gif_decode_frame_out_of_bounds,
    test_wuffs_gif_decode_input_is_a_gif_just_one_read,
    test_wuffs_gif_decode_input_is_a_gif_many_big_reads,
    test_wuffs_gif_decode_input_is_a_gif_many_medium_reads,
    test_wuffs_gif_decode_input_is_a_gif_many_small_reads,
    test_wuffs_gif_decode_input_is_a_png,
    test_wuffs_gif_decode_interface_image_decoder,
    test_wuffs_gif_decode_interlaced_truncated,
    test_wuffs_gif_decode_metadata_empty,
    test_wuffs_gif_decode_metadata_full,
    test_wuffs_gif_decode_missing_two_src_bytes,
    test_wuffs_gif_decode_multiple_graphic_controls,
    test_wuffs_gif_decode_multiple_loop_counts,
    test_wuffs_gif_decode_nul_byte_trailer,
    test_wuffs_gif_decode_pixel_data_none,
    test_wuffs_gif_decode_pixel_data_not_enough,
    test_wuffs_gif_decode_pixel_data_too_much_sans_quirk,
    test_wuffs_gif_decode_pixel_data_too_much_with_quirk,
    test_wuffs_gif_decode_pixfmt_bgr,
    test_wuffs_gif_decode_pixfmt_bgr_565,
    test_wuffs_gif_decode_pixfmt_bgra_nonpremul,
    test_wuffs_gif_decode_pixfmt_rgb,
    test_wuffs_gif_decode_pixfmt_rgba_nonpremul,
    test_wuffs_gif_decode_truncated_input,
    test_wuffs_gif_decode_zero_width_frame,
    test_wuffs_gif_frame_dirty_rect,
    test_wuffs_gif_num_decoded_frame_configs,
    test_wuffs_gif_num_decoded_frames,
    test_wuffs_gif_io_position_one_chunk,
    test_wuffs_gif_io_position_two_chunks,
    test_wuffs_gif_small_frame_interlaced,

#ifdef WUFFS_MIMIC

    test_mimic_gif_decode_animated_small,
    test_mimic_gif_decode_bricks_dither,
    test_mimic_gif_decode_bricks_gray,
    test_mimic_gif_decode_bricks_nodither,
    test_mimic_gif_decode_gifplayer_muybridge,
    test_mimic_gif_decode_harvesters,
    test_mimic_gif_decode_hat,
    test_mimic_gif_decode_hibiscus_primitive,
    test_mimic_gif_decode_hibiscus_regular,
    test_mimic_gif_decode_hippopotamus_interlaced,
    test_mimic_gif_decode_hippopotamus_regular,
    test_mimic_gif_decode_muybridge,
    test_mimic_gif_decode_pjw_thumbnail,

#endif  // WUFFS_MIMIC

    NULL,
};

proc g_benches[] = {

    bench_wuffs_gif_decode_1k_bw,
    bench_wuffs_gif_decode_1k_color_full_init,
    bench_wuffs_gif_decode_1k_color_part_init,
    bench_wuffs_gif_decode_10k_bgra,
    bench_wuffs_gif_decode_10k_indexed,
    bench_wuffs_gif_decode_20k,
    bench_wuffs_gif_decode_100k_artificial,
    bench_wuffs_gif_decode_100k_realistic,
    bench_wuffs_gif_decode_1000k_full_init,
    bench_wuffs_gif_decode_1000k_part_init,
    bench_wuffs_gif_decode_anim_screencap,

#ifdef WUFFS_MIMIC

    bench_mimic_gif_decode_1k_bw,
    bench_mimic_gif_decode_1k_color_full_init,
    bench_mimic_gif_decode_10k_indexed,
    bench_mimic_gif_decode_20k,
    bench_mimic_gif_decode_100k_artificial,
    bench_mimic_gif_decode_100k_realistic,
    bench_mimic_gif_decode_1000k_full_init,
    bench_mimic_gif_decode_anim_screencap,

#endif  // WUFFS_MIMIC

    NULL,
};

int  //
main(int argc, char** argv) {
  g_proc_package_name = "std/gif";
  return test_main(argc, argv, g_tests, g_benches);
}
