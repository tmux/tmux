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
print-image-metadata prints images' metadata.
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

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../release/c/wuffs-unsupported-snapshot.c"

// ----

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (64 * 1024)
#endif

#ifndef META_BUFFER_ARRAY_SIZE
#define META_BUFFER_ARRAY_SIZE (64 * 1024)
#endif

#ifndef WORKBUF_ARRAY_SIZE
#define WORKBUF_ARRAY_SIZE (256 * 1024 * 1024)
#endif

#define PRINTBUF_ARRAY_SIZE 80

uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE] = {0};
uint8_t g_meta_buffer_array[META_BUFFER_ARRAY_SIZE] = {0};
uint8_t g_workbuf_array[WORKBUF_ARRAY_SIZE] = {0};

uint8_t g_printbuf_array[PRINTBUF_ARRAY_SIZE] = {0};
uint32_t g_printbuf_index = 0;

// ----

#define TRY(error_msg)         \
  do {                         \
    const char* z = error_msg; \
    if (z) {                   \
      return z;                \
    }                          \
  } while (false)

const uint8_t  //
    hexify[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',  //
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',  //
};

const uint8_t  //
    printable_ascii[256] = {
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,  //
        0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,  //
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,  //
        0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,  //

        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,  //
        0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,  //
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,  //
        0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,  //
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,  //
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,  //
        0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,  //
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,  //

        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //

        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
        0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E, 0x2E,  //
};

// ----

const char*  //
read_buffer_from_file(wuffs_base__io_buffer* buf, FILE* f) {
  if (buf->meta.closed) {
    return "main: unexpected end of file";
  }
  buf->compact();
  size_t n = fread(buf->writer_pointer(), 1, buf->writer_length(), f);
  buf->meta.wi += n;
  buf->meta.closed = feof(f);
  return ferror(f) ? "main: error reading file" : nullptr;
}

void  //
print_fourcc(uint32_t fourcc) {
  printf("  %c%c%c%c\n",           //
         (0xFF & (fourcc >> 24)),  //
         (0xFF & (fourcc >> 16)),  //
         (0xFF & (fourcc >> 8)),   //
         (0xFF & (fourcc >> 0)));
}

void  //
flush_hex_dump() {
  if (g_printbuf_index == 0) {
    return;
  }
  puts(static_cast<const char*>(static_cast<const void*>(g_printbuf_array)));
  g_printbuf_index = 0;
}

void  //
print_hex_dump(const uint8_t* ptr, size_t len) {
  while (len--) {
    if (g_printbuf_index == 0) {
      const char* s =
          "    -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --    "
          "----------------";
      size_t n = strlen(s);
      if ((n + 1) > PRINTBUF_ARRAY_SIZE) {
        exit(1);
      }
      memcpy(&g_printbuf_array[0], s, n + 1);
    }
    const uint8_t c = *ptr++;
    g_printbuf_array[(3 * g_printbuf_index) + 4] = hexify[c >> 4];
    g_printbuf_array[(3 * g_printbuf_index) + 5] = hexify[c & 15];
    g_printbuf_array[g_printbuf_index + 55] = printable_ascii[c];
    g_printbuf_index++;
    if (g_printbuf_index == 16) {
      puts(
          static_cast<const char*>(static_cast<const void*>(g_printbuf_array)));
      g_printbuf_index = 0;
    }
  }
}

const char*  //
print_raw_passthrough(wuffs_base__io_buffer* src,
                      FILE* f,
                      wuffs_base__range_ie_u64 r) {
  if (r.is_empty()) {
    return nullptr;
  }

  // Advance src so that its reader_position is r.min_incl.
  if (src->reader_position() > r.min_incl) {
    return "main: unsupported metadata range";
  }
  while (src->reader_position() < r.min_incl) {
    if (src->writer_position() >= r.min_incl) {
      src->meta.ri = r.min_incl - src->meta.pos;
      break;
    }
    src->meta.ri = src->meta.wi;
    TRY(read_buffer_from_file(src, f));
  }

  // Print the passthrough bytes until src's reader_position is r.max_excl.
  while (true) {
    uint64_t n0 = r.max_excl - src->reader_position();
    if (n0 == 0) {
      break;
    }
    uint64_t n1 = src->reader_length();
    while (n1 == 0) {
      TRY(read_buffer_from_file(src, f));
      n1 = src->reader_length();
    }
    uint64_t n = wuffs_base__u64__min(n0, n1);
    print_hex_dump(src->reader_pointer(), n);
    src->meta.ri += n;
  }

  return nullptr;
}

const char*  //
print_metadata(wuffs_base__image_decoder* dec,
               wuffs_base__io_buffer* src,
               FILE* f) {
  bool printed_fourcc = false;
  while (true) {
    auto meta = wuffs_base__ptr_u8__writer(&g_meta_buffer_array[0],
                                           META_BUFFER_ARRAY_SIZE);
    auto minfo = wuffs_base__empty_more_information();
    auto tmm_status = dec->tell_me_more(&meta, &minfo, src);

    if (minfo.flavor) {
      if (!printed_fourcc) {
        printed_fourcc = true;
        print_fourcc(minfo.metadata__fourcc());
      }

      switch (minfo.flavor) {
        case WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_PASSTHROUGH:
          TRY(print_raw_passthrough(src, f,
                                    minfo.metadata_raw_passthrough__range()));
          break;

        case WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_RAW_TRANSFORM:
          print_hex_dump(meta.reader_pointer(), meta.reader_length());
          meta.meta.ri = meta.meta.wi;
          break;

        case WUFFS_BASE__MORE_INFORMATION__FLAVOR__METADATA_PARSED:
          switch (minfo.metadata__fourcc()) {
            case WUFFS_BASE__FOURCC__CHRM:
              for (uint32_t i = 0; i < 8; i++) {
                printf("    %" PRId32 "\n", minfo.metadata_parsed__chrm(i));
              }
              break;
            case WUFFS_BASE__FOURCC__GAMA:
              printf("    %" PRIu32 "\n", minfo.metadata_parsed__gama());
              break;
            case WUFFS_BASE__FOURCC__SRGB:
              printf("    %" PRIu32 "\n", minfo.metadata_parsed__srgb());
              break;
            default:
              return "main: unsupported metadata FourCC";
          }
          break;

        default:
          return "main: unsupported metadata flavor";
      }
    }

    if (tmm_status.is_ok()) {
      break;
    } else if (tmm_status.repr == wuffs_base__suspension__short_read) {
      TRY(read_buffer_from_file(src, f));
      continue;
    } else if (tmm_status.repr == wuffs_base__suspension__short_write) {
      continue;
    } else if (tmm_status.repr !=
               wuffs_base__suspension__even_more_information) {
      return tmm_status.message();
    }
  }
  flush_hex_dump();

  return nullptr;
}

const char*  //
handle_redirect(int32_t* out_fourcc,
                wuffs_base__image_decoder* dec,
                wuffs_base__io_buffer* src,
                FILE* f) {
  auto empty = wuffs_base__empty_io_buffer();
  auto minfo = wuffs_base__empty_more_information();
  auto tmm_status = dec->tell_me_more(&empty, &minfo, src);
  if (tmm_status.repr != NULL) {
    return tmm_status.message();
  } else if (minfo.flavor !=
             WUFFS_BASE__MORE_INFORMATION__FLAVOR__IO_REDIRECT) {
    return "main: unsupported file format";
  }
  *out_fourcc = (int32_t)(minfo.io_redirect__fourcc());
  if (*out_fourcc <= 0) {
    return "main: unsupported file format";
  }

  // Advance src so that its reader_position is r.min_incl.
  auto r = minfo.io_redirect__range();
  if (src->reader_position() > r.min_incl) {
    return "main: unsupported I/O redirect range";
  }
  while (src->reader_position() < r.min_incl) {
    if (src->writer_position() >= r.min_incl) {
      src->meta.ri = r.min_incl - src->meta.pos;
      break;
    }
    src->meta.ri = src->meta.wi;
    TRY(read_buffer_from_file(src, f));
  }
  return nullptr;
}

const char*  //
handle(const char* filename, FILE* f) {
  auto src =
      wuffs_base__ptr_u8__writer(&g_src_buffer_array[0], SRC_BUFFER_ARRAY_SIZE);
  auto work =
      wuffs_base__ptr_u8__writer(&g_workbuf_array[0], WORKBUF_ARRAY_SIZE);

  int32_t fourcc = 0;
  while (true) {
    fourcc = wuffs_base__magic_number_guess_fourcc(src.reader_slice(),
                                                   src.meta.closed);
    if (fourcc > 0) {
      break;
    } else if (fourcc == 0) {
      return "main: unrecognized file format";
    } else {
      TRY(read_buffer_from_file(&src, f));
    }
  }

  bool redirected = false;
redirect:
  do {
    print_fourcc(fourcc);
    wuffs_base__image_decoder::unique_ptr dec;
    switch (fourcc) {
      case WUFFS_BASE__FOURCC__BMP:
        dec = wuffs_bmp__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__ETC2:
        dec = wuffs_etc2__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__GIF:
        dec = wuffs_gif__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__JPEG:
        dec = wuffs_jpeg__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__NIE:
        dec = wuffs_nie__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__NPBM:
        dec = wuffs_netpbm__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__PNG:
        dec = wuffs_png__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__QOI:
        dec = wuffs_qoi__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__TARGA:
        dec = wuffs_targa__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__TH:
        dec = wuffs_thumbhash__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__WBMP:
        dec = wuffs_wbmp__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      case WUFFS_BASE__FOURCC__WEBP:
        dec = wuffs_webp__decoder::alloc_as__wuffs_base__image_decoder();
        break;
      default:
        return "main: unsupported file format";
    }

    dec->set_report_metadata(WUFFS_BASE__FOURCC__CHRM, true);
    dec->set_report_metadata(WUFFS_BASE__FOURCC__EXIF, true);
    dec->set_report_metadata(WUFFS_BASE__FOURCC__GAMA, true);
    dec->set_report_metadata(WUFFS_BASE__FOURCC__ICCP, true);
    dec->set_report_metadata(WUFFS_BASE__FOURCC__KVP, true);
    dec->set_report_metadata(WUFFS_BASE__FOURCC__SRGB, true);
    dec->set_report_metadata(WUFFS_BASE__FOURCC__XMP, true);

    while (true) {
      auto dfc_status = dec->decode_frame_config(NULL, &src);
      if (dfc_status.is_ok()) {
        // No-op.
      } else if (dfc_status.repr == wuffs_base__note__end_of_data) {
        break;
      } else if (dfc_status.repr == wuffs_base__note__metadata_reported) {
        TRY(print_metadata(dec.get(), &src, f));
      } else if (dfc_status.repr == wuffs_base__note__i_o_redirect) {
        if (redirected) {
          return "main: unsupported file format";
        }
        redirected = true;
        TRY(handle_redirect(&fourcc, dec.get(), &src, f));
        goto redirect;
      } else if (dfc_status.repr == wuffs_base__suspension__short_read) {
        TRY(read_buffer_from_file(&src, f));
      } else {
        return dfc_status.message();
      }
    }
  } while (false);

  return nullptr;
}

int  //
main(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    FILE* f = fopen(argv[i], "r");
    if (!f) {
      printf("%s\n  %s\n", argv[i], strerror(errno));
      continue;
    }
    printf("%s\n", argv[i]);
    const char* err = handle(argv[i], f);
    if (err) {
      printf("  %s\n", err);
    }
    fclose(f);
  }
  return 0;
}
