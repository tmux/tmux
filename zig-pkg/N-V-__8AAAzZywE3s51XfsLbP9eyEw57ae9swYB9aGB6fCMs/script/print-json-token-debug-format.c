// Copyright 2020 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// print-json-token-debug-format.c parses JSON from stdin and prints the
// resulting token stream, eliding any non-essential (e.g. whitespace) tokens.
//
// The output format is only for debugging or regression testing, and certainly
// not for long term storage. It isn't guaranteed to be stable between versions
// of this program and of the Wuffs standard library.
//
// It prints 16 bytes (128 bits) per token, containing big-endian numbers:
//
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
// |               |       |       |   |      VALUE_EXTENSION      |
// |      POS      |  LEN  |  CON  |EXT|VALUE_MAJOR|  VALUE_MINOR  |
// |               |       |       |   |     0     |VBC|    VBD    |
// +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
//
//  - POS (4 bytes) is the position: the sum of all previous tokens' lengths,
//                  including elided tokens.
//  - LEN (2 bytes) is the length.
//  - CON (2 bytes) is the continued bit
//  - EXT (1 bytes) is 1 for extended and 0 for simple tokens.
//
// Extended tokens have a VALUE_EXTENSION (7 bytes).
//
// Simple tokens have a VALUE_MAJOR (3 bytes) and then either 4 bytes
// VALUE_MINOR (when VALUE_MAJOR is non-zero) or (1 + 3) bytes
// VALUE_BASE_CATEGORY and VALUE_BASE_DETAIL (when VALUE_MAJOR is zero).
//
// ----
//
// Together with the hexadecimal WUFFS_BASE__TOKEN__ETC constants defined in
// token-public.h, this format is somewhat human-readable when piped through a
// hex-dump program (such as /usr/bin/hd), printing one token per line.
// Alternatively, pass the -h (--human-readable) flag to this program.
//
// Pass -a (--all-tokens) to print all tokens, including whitespace.
//
// Pass -i=cbor (--input-format=cbor) to parse CBOR instead of JSON.
//
// Pass -q (--quirks) to enable all of a pre-defined set of various parser
// quirks. For example, allowing JSON comments or JSON "inf" numbers.
//
// If the input or output is larger than the program's buffers (64 MiB and
// 131072 tokens by default), there may be multiple valid tokenizations of any
// given input. For example, if a source string "abcde" straddles an I/O
// boundary, it may be tokenized as single (not continued) 5-length string or
// as a 3-length continued string followed by a 2-length string.
//
// A Wuffs token stream, in general, can support inputs more than 0xFFFF_FFFF
// bytes long, but this program can not, as it tracks the tokens' cumulative
// position as a uint32.

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CBOR
#define WUFFS_CONFIG__MODULE__JSON

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C file.
#include "../release/c/wuffs-unsupported-snapshot.c"

// Wuffs allows either statically or dynamically allocated work buffers. This
// program exercises static allocation.
#if WUFFS_CBOR__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE > \
    WUFFS_JSON__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
#define WORK_BUFFER_ARRAY_SIZE \
  WUFFS_CBOR__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
#else
#define WORK_BUFFER_ARRAY_SIZE \
  WUFFS_JSON__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
#endif
#if WORK_BUFFER_ARRAY_SIZE > 0
uint8_t g_work_buffer_array[WORK_BUFFER_ARRAY_SIZE];
#else
// Not all C/C++ compilers support 0-length arrays.
uint8_t g_work_buffer_array[1];
#endif

#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (64 * 1024 * 1024)
#endif
#ifndef TOKEN_BUFFER_ARRAY_SIZE
#define TOKEN_BUFFER_ARRAY_SIZE (128 * 1024)
#endif

uint8_t g_src_buffer_array[SRC_BUFFER_ARRAY_SIZE];
wuffs_base__token g_tok_buffer_array[TOKEN_BUFFER_ARRAY_SIZE];

wuffs_base__io_buffer g_src;
wuffs_base__token_buffer g_tok;

wuffs_cbor__decoder g_cbor_decoder;
wuffs_json__decoder g_json_decoder;
wuffs_base__token_decoder* g_dec;
wuffs_base__status g_dec_status;

#define TRY(error_msg)         \
  do {                         \
    const char* z = error_msg; \
    if (z) {                   \
      return z;                \
    }                          \
  } while (false)

// ignore_return_value suppresses errors from -Wall -Werror.
static void  //
ignore_return_value(int ignored) {}

const char*  //
read_src() {
  if (g_src.meta.closed) {
    return "main: internal error: read requested on a closed source";
  }
  wuffs_base__io_buffer__compact(&g_src);
  if (g_src.meta.wi >= g_src.data.len) {
    return "main: g_src buffer is full";
  }
  size_t n = fread(g_src.data.ptr + g_src.meta.wi, sizeof(uint8_t),
                   g_src.data.len - g_src.meta.wi, stdin);
  g_src.meta.wi += n;
  g_src.meta.closed = feof(stdin);
  if ((n == 0) && !g_src.meta.closed) {
    return "main: read error";
  }
  return NULL;
}

// ----

typedef enum file_format_enum {
  FILE_FORMAT_JSON,
  FILE_FORMAT_CBOR,
} file_format;

struct {
  int remaining_argc;
  char** remaining_argv;

  bool all_tokens;
  bool human_readable;
  file_format input_format;
  bool quirks;
} g_flags = {0};

const char*  //
parse_flags(int argc, char** argv) {
  int c = (argc > 0) ? 1 : 0;  // Skip argv[0], the program name.
  for (; c < argc; c++) {
    char* arg = argv[c];
    if (*arg++ != '-') {
      break;
    }

    // A double-dash "--foo" is equivalent to a single-dash "-foo". As special
    // cases, a bare "-" is not a flag (some programs may interpret it as
    // stdin) and a bare "--" means to stop parsing flags.
    if (*arg == '\x00') {
      break;
    } else if (*arg == '-') {
      arg++;
      if (*arg == '\x00') {
        c++;
        break;
      }
    }

    if (!strcmp(arg, "a") || !strcmp(arg, "all-tokens")) {
      g_flags.all_tokens = true;
      continue;
    }
    if (!strcmp(arg, "h") || !strcmp(arg, "human-readable")) {
      g_flags.human_readable = true;
      continue;
    }
    if (!strcmp(arg, "i=cbor") || !strcmp(arg, "input-format=cbor")) {
      g_flags.input_format = FILE_FORMAT_CBOR;
      continue;
    }
    if (!strcmp(arg, "i=json") || !strcmp(arg, "input-format=json")) {
      g_flags.input_format = FILE_FORMAT_JSON;
      continue;
    }
    if (!strcmp(arg, "q") || !strcmp(arg, "quirks")) {
      g_flags.quirks = true;
      continue;
    }

    return "main: unrecognized flag argument";
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return NULL;
}

const char* g_vbc_names[16] = {
    "0:Filler...........",  //
    "1:Structure........",  //
    "2:String...........",  //
    "3:UnicodeCodePoint.",  //
    "4:Literal..........",  //
    "5:Number...........",  //
    "6:InlineIntSigned..",  //
    "7:InlineIntUnsigned",  //
    "8:Reserved.........",  //
    "9:Reserved.........",  //
    "A:Reserved.........",  //
    "B:Reserved.........",  //
    "C:Reserved.........",  //
    "D:Reserved.........",  //
    "E:Reserved.........",  //
    "F:Reserved.........",  //
};

const int g_base38_decode[38] = {
    '.', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',                 //
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',       //
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '~',  //
};

const char*  //
main1(int argc, char** argv) {
  TRY(parse_flags(argc, argv));
  if (g_flags.remaining_argc > 0) {
    return "main: bad argument: use \"program < input\", not \"program input\"";
  }

  g_src = wuffs_base__make_io_buffer(
      wuffs_base__make_slice_u8(g_src_buffer_array, SRC_BUFFER_ARRAY_SIZE),
      wuffs_base__empty_io_buffer_meta());

  g_tok = wuffs_base__make_token_buffer(
      wuffs_base__make_slice_token(g_tok_buffer_array, TOKEN_BUFFER_ARRAY_SIZE),
      wuffs_base__empty_token_buffer_meta());

  if (g_flags.input_format == FILE_FORMAT_JSON) {
    wuffs_base__status init_status = wuffs_json__decoder__initialize(
        &g_json_decoder, sizeof__wuffs_json__decoder(), WUFFS_VERSION, 0);
    if (!wuffs_base__status__is_ok(&init_status)) {
      return wuffs_base__status__message(&init_status);
    }
    g_dec = wuffs_json__decoder__upcast_as__wuffs_base__token_decoder(
        &g_json_decoder);
  } else {
    wuffs_base__status init_status = wuffs_cbor__decoder__initialize(
        &g_cbor_decoder, sizeof__wuffs_cbor__decoder(), WUFFS_VERSION, 0);
    if (!wuffs_base__status__is_ok(&init_status)) {
      return wuffs_base__status__message(&init_status);
    }
    g_dec = wuffs_cbor__decoder__upcast_as__wuffs_base__token_decoder(
        &g_cbor_decoder);
  }

  if (g_flags.quirks) {
    uint32_t quirks[] = {
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_A,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_CAPITAL_U,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_E,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_QUESTION_MARK,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_SINGLE_QUOTE,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_V,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_X_AS_CODE_POINTS,
        WUFFS_JSON__QUIRK_ALLOW_BACKSLASH_ZERO,
        WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK,
        WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE,
        WUFFS_JSON__QUIRK_ALLOW_EXTRA_COMMA,
        WUFFS_JSON__QUIRK_ALLOW_INF_NAN_NUMBERS,
        WUFFS_JSON__QUIRK_ALLOW_LEADING_ASCII_RECORD_SEPARATOR,
        WUFFS_JSON__QUIRK_ALLOW_LEADING_UNICODE_BYTE_ORDER_MARK,
        WUFFS_JSON__QUIRK_ALLOW_TRAILING_FILLER,
        WUFFS_JSON__QUIRK_REPLACE_INVALID_UNICODE,
        0,
    };
    for (uint32_t i = 0; quirks[i]; i++) {
      wuffs_base__token_decoder__set_quirk(g_dec, quirks[i], 1);
    }
  }

  uint64_t pos = 0;
  while (true) {
    wuffs_base__status status = wuffs_base__token_decoder__decode_tokens(
        g_dec, &g_tok, &g_src,
        wuffs_base__make_slice_u8(g_work_buffer_array, WORK_BUFFER_ARRAY_SIZE));

    while (g_tok.meta.ri < g_tok.meta.wi) {
      wuffs_base__token* t = &g_tok.data.ptr[g_tok.meta.ri++];
      uint16_t len = wuffs_base__token__length(t);

      if (g_flags.all_tokens || (wuffs_base__token__value(t) != 0)) {
        uint16_t con = wuffs_base__token__continued(t) ? 1 : 0;
        int32_t vmajor = wuffs_base__token__value_major(t);
        uint32_t vminor = wuffs_base__token__value_minor(t);
        uint8_t vbc = wuffs_base__token__value_base_category(t);
        uint32_t vbd = wuffs_base__token__value_base_detail(t);

        if (g_flags.human_readable) {
          printf("pos=0x%08" PRIX32 "  len=0x%04" PRIX16 "  con=%" PRId16 "  ",
                 (uint32_t)(pos), len, con);

          if (vmajor > 0) {
            char vmajor_name[5];
            vmajor_name[0] = '?';
            vmajor_name[1] = '?';
            vmajor_name[2] = '?';
            vmajor_name[3] = '?';
            vmajor_name[4] = '\x00';
            uint32_t m = vmajor;
            if (m < 38 * 38 * 38 * 38) {
              uint32_t m0 = m / (38 * 38 * 38);
              m -= m0 * (38 * 38 * 38);
              vmajor_name[0] = g_base38_decode[m0];

              uint32_t m1 = m / (38 * 38);
              m -= m1 * (38 * 38);
              vmajor_name[1] = g_base38_decode[m1];

              uint32_t m2 = m / (38);
              m -= m2 * (38);
              vmajor_name[2] = g_base38_decode[m2];

              uint32_t m3 = m;
              vmajor_name[3] = g_base38_decode[m3];
            }

            printf("vmajor=0x%06" PRIX32 "=%s vminor=0x%07" PRIX32 "\n", vmajor,
                   vmajor_name, vminor);
          } else if (vmajor == 0) {
            printf("vbc=%s  vbd=0x%06" PRIX32 "\n", g_vbc_names[vbc & 15], vbd);
          } else {
            printf("extended... vextension=0x%012" PRIX64 "\n",
                   wuffs_base__token__value_extension(t));
          }

        } else {
          uint8_t buf[16];
          wuffs_base__poke_u32be__no_bounds_check(&buf[0x0], (uint32_t)(pos));
          wuffs_base__poke_u16be__no_bounds_check(&buf[0x4], len);
          wuffs_base__poke_u16be__no_bounds_check(&buf[0x6], con);
          if (vmajor > 0) {
            wuffs_base__poke_u32be__no_bounds_check(&buf[0x8], vmajor);
            wuffs_base__poke_u32be__no_bounds_check(&buf[0xC], vminor);
          } else if (vmajor == 0) {
            wuffs_base__poke_u32be__no_bounds_check(&buf[0x8], 0);
            wuffs_base__poke_u8__no_bounds_check(&buf[0x000C], vbc);
            wuffs_base__poke_u24be__no_bounds_check(&buf[0xD], vbd);
          } else {
            wuffs_base__poke_u8__no_bounds_check(&buf[0x0008], 0x01);
            wuffs_base__poke_u56be__no_bounds_check(
                &buf[0x9], wuffs_base__token__value_extension(t));
          }
          const int stdout_fd = 1;
          ignore_return_value(write(stdout_fd, &buf[0], 16));
        }
      }

      pos += len;
      if (pos > 0xFFFFFFFF) {
        return "main: input is too long";
      }
    }

    if (status.repr == NULL) {
      return NULL;
    } else if (status.repr == wuffs_base__suspension__short_read) {
      TRY(read_src());
    } else if (status.repr == wuffs_base__suspension__short_write) {
      wuffs_base__token_buffer__compact(&g_tok);
    } else {
      return wuffs_base__status__message(&status);
    }
  }
}

// ----

int  //
compute_exit_code(const char* status_msg) {
  if (!status_msg) {
    return 0;
  }
  size_t n = strnlen(status_msg, 2047);
  if (n >= 2047) {
    status_msg = "main: internal error: error message is too long";
    n = strlen(status_msg);
  }
  fprintf(stderr, "%s\n", status_msg);
  // Return an exit code of 1 for regular (foreseen) errors, e.g. badly
  // formatted or unsupported input.
  //
  // Return an exit code of 2 for internal (exceptional) errors, e.g. defensive
  // run-time checks found that an internal invariant did not hold.
  //
  // Automated testing, including badly formatted inputs, can therefore
  // discriminate between expected failure (exit code 1) and unexpected failure
  // (other non-zero exit codes). Specifically, exit code 2 for internal
  // invariant violation, exit code 139 (which is 128 + SIGSEGV on x86_64
  // linux) for a segmentation fault (e.g. null pointer dereference).
  return strstr(status_msg, "internal error:") ? 2 : 1;
}

int  //
main(int argc, char** argv) {
  const char* z = main1(argc, argv);
  int exit_code = compute_exit_code(z);
  return exit_code;
}
