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

/*
cbor-to-json reads CBOR (a binary format) from stdin and writes the equivalent
formatted JSON (a text format) to stdout.

See the "const char* g_usage" string below for details.

----

To run:

$CXX cbor-to-json.cc && ./a.out < ../../test/data/json-things.cbor; rm -f a.out

for a C++ compiler $CXX, such as clang++ or g++.
*/

#if defined(__cplusplus) && (__cplusplus < 201103L)
#error "This C++ program requires -std=c++11 or later"
#endif

#include <stdio.h>

#include <string>
#include <vector>

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
#define WUFFS_CONFIG__MODULE__AUX__CBOR
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CBOR

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C++ file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

#define TRY(error_msg)         \
  do {                         \
    std::string z = error_msg; \
    if (!z.empty()) {          \
      return z;                \
    }                          \
  } while (false)

static const char* g_usage =
    "Usage: cbor-to-json -flags input.cbor\n"
    "\n"
    "Flags:\n"
    "    -c      -compact-output\n"
    "    -s=NUM  -spaces=NUM\n"
    "    -t      -tabs\n"
    "            -output-cbor-metadata-as-comments\n"
    "            -output-extra-comma\n"
    "            -output-inf-nan-numbers\n"
    "\n"
    "The input.cbor filename is optional. If absent, it reads from stdin.\n"
    "\n"
    "----\n"
    "\n"
    "cbor-to-json reads CBOR (a binary format) from stdin and writes the\n"
    "equivalent formatted JSON (a text format) to stdout.\n"
    "\n"
    "The output JSON's arrays' and objects' elements are indented, each on\n"
    "its own line. Configure this with the -c / -compact-output, -s=NUM /\n"
    "-spaces=NUM (for NUM ranging from 0 to 8) and -t / -tabs flags.\n"
    "\n"
    "The conversion may be lossy. For example, CBOR metadata such as tags or\n"
    "distinguishing undefined from null are either dropped or, with\n"
    "-output-cbor-metadata-as-comments, converted to \"/*comments*/\". Such\n"
    "comments are non-compliant with the JSON specification but many parsers\n"
    "accept them.\n"
    "\n"
    "The -output-extra-comma flag writes output like \"[1,2,]\", with a comma\n"
    "after the final element of a JSON list or dictionary. Such commas are\n"
    "non-compliant with the JSON specification but many parsers accept them\n"
    "and they can produce simpler line-based diffs. This flag is ignored when\n"
    "-compact-output is set.\n"
    "\n"
    "The -output-inf-nan-numbers flag writes Inf and NaN instead of a\n"
    "substitute null value. Such values are non-compliant with the JSON\n"
    "specification but many parsers accept them.\n"
    "\n"
    "CBOR is more permissive about map keys but JSON only allows strings.\n"
    "When converting from -i=cbor to -o=json, this program rejects keys other\n"
    "than integers and strings (CBOR major types 0, 1, 2 and 3). Integer\n"
    "keys like 123 quoted to be string keys like \"123\".\n"
    "\n"
    "The CBOR specification permits implementations to set their own maximum\n"
    "input depth. This CBOR implementation sets it to 1024.";

// ----

// ascii_escapes was created by script/print-json-ascii-escapes.go.
const uint8_t ascii_escapes[1024] = {
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x30, 0x00,  // 0x00: "\\u0000"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x31, 0x00,  // 0x01: "\\u0001"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x32, 0x00,  // 0x02: "\\u0002"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x33, 0x00,  // 0x03: "\\u0003"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x34, 0x00,  // 0x04: "\\u0004"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x35, 0x00,  // 0x05: "\\u0005"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x36, 0x00,  // 0x06: "\\u0006"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x37, 0x00,  // 0x07: "\\u0007"
    0x02, 0x5C, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x08: "\\b"
    0x02, 0x5C, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x09: "\\t"
    0x02, 0x5C, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x0A: "\\n"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x42, 0x00,  // 0x0B: "\\u000B"
    0x02, 0x5C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x0C: "\\f"
    0x02, 0x5C, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x0D: "\\r"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x45, 0x00,  // 0x0E: "\\u000E"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x30, 0x46, 0x00,  // 0x0F: "\\u000F"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x30, 0x00,  // 0x10: "\\u0010"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x31, 0x00,  // 0x11: "\\u0011"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x32, 0x00,  // 0x12: "\\u0012"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x33, 0x00,  // 0x13: "\\u0013"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x34, 0x00,  // 0x14: "\\u0014"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x35, 0x00,  // 0x15: "\\u0015"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x36, 0x00,  // 0x16: "\\u0016"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x37, 0x00,  // 0x17: "\\u0017"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x38, 0x00,  // 0x18: "\\u0018"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x39, 0x00,  // 0x19: "\\u0019"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x41, 0x00,  // 0x1A: "\\u001A"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x42, 0x00,  // 0x1B: "\\u001B"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x43, 0x00,  // 0x1C: "\\u001C"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x44, 0x00,  // 0x1D: "\\u001D"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x45, 0x00,  // 0x1E: "\\u001E"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x31, 0x46, 0x00,  // 0x1F: "\\u001F"
    0x06, 0x5C, 0x75, 0x30, 0x30, 0x32, 0x30, 0x00,  // 0x20: "\\u0020"
    0x01, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x21: "!"
    0x02, 0x5C, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x22: "\\\""
    0x01, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x23: "#"
    0x01, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x24: "$"
    0x01, 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x25: "%"
    0x01, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x26: "&"
    0x01, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x27: "'"
    0x01, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x28: "("
    0x01, 0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x29: ")"
    0x01, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x2A: "*"
    0x01, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x2B: "+"
    0x01, 0x2C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x2C: ","
    0x01, 0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x2D: "-"
    0x01, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x2E: "."
    0x01, 0x2F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x2F: "/"
    0x01, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x30: "0"
    0x01, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x31: "1"
    0x01, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x32: "2"
    0x01, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x33: "3"
    0x01, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x34: "4"
    0x01, 0x35, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x35: "5"
    0x01, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x36: "6"
    0x01, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x37: "7"
    0x01, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x38: "8"
    0x01, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x39: "9"
    0x01, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x3A: ":"
    0x01, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x3B: ";"
    0x01, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x3C: "<"
    0x01, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x3D: "="
    0x01, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x3E: ">"
    0x01, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x3F: "?"
    0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x40: "@"
    0x01, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x41: "A"
    0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x42: "B"
    0x01, 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x43: "C"
    0x01, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x44: "D"
    0x01, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x45: "E"
    0x01, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x46: "F"
    0x01, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x47: "G"
    0x01, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x48: "H"
    0x01, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x49: "I"
    0x01, 0x4A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x4A: "J"
    0x01, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x4B: "K"
    0x01, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x4C: "L"
    0x01, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x4D: "M"
    0x01, 0x4E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x4E: "N"
    0x01, 0x4F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x4F: "O"
    0x01, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x50: "P"
    0x01, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x51: "Q"
    0x01, 0x52, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x52: "R"
    0x01, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x53: "S"
    0x01, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x54: "T"
    0x01, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x55: "U"
    0x01, 0x56, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x56: "V"
    0x01, 0x57, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x57: "W"
    0x01, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x58: "X"
    0x01, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x59: "Y"
    0x01, 0x5A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x5A: "Z"
    0x01, 0x5B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x5B: "["
    0x02, 0x5C, 0x5C, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x5C: "\\\\"
    0x01, 0x5D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x5D: "]"
    0x01, 0x5E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x5E: "^"
    0x01, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x5F: "_"
    0x01, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x60: "`"
    0x01, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x61: "a"
    0x01, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x62: "b"
    0x01, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x63: "c"
    0x01, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x64: "d"
    0x01, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x65: "e"
    0x01, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x66: "f"
    0x01, 0x67, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x67: "g"
    0x01, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x68: "h"
    0x01, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x69: "i"
    0x01, 0x6A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x6A: "j"
    0x01, 0x6B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x6B: "k"
    0x01, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x6C: "l"
    0x01, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x6D: "m"
    0x01, 0x6E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x6E: "n"
    0x01, 0x6F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x6F: "o"
    0x01, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x70: "p"
    0x01, 0x71, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x71: "q"
    0x01, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x72: "r"
    0x01, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x73: "s"
    0x01, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x74: "t"
    0x01, 0x75, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x75: "u"
    0x01, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x76: "v"
    0x01, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x77: "w"
    0x01, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x78: "x"
    0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x79: "y"
    0x01, 0x7A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x7A: "z"
    0x01, 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x7B: "{"
    0x01, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x7C: "|"
    0x01, 0x7D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x7D: "}"
    0x01, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x7E: "~"
    0x01, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // 0x7F: "<DEL>"
};

#define NEW_LINE_THEN_256_SPACES                                               \
  "\n                                                                        " \
  "                                                                          " \
  "                                                                          " \
  "                                    "
#define NEW_LINE_THEN_256_TABS                                                 \
  "\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"

const char* g_new_line_then_256_indent_bytes;
uint32_t g_bytes_per_indent_depth;

#ifndef DST_BUFFER_ARRAY_SIZE
#define DST_BUFFER_ARRAY_SIZE (32 * 1024)
#endif

uint8_t g_dst_array[DST_BUFFER_ARRAY_SIZE];
wuffs_base__io_buffer g_dst;

uint32_t g_depth;

enum class context {
  none,
  in_list_after_bracket,
  in_list_after_value,
  in_dict_after_brace,
  in_dict_after_key,
  in_dict_after_value,
} g_ctx;

bool g_wrote_to_dst;

std::vector<uint64_t> g_cbor_tags;

struct {
  int remaining_argc;
  char** remaining_argv;

  bool compact_output;
  bool output_cbor_metadata_as_comments;
  bool output_extra_comma;
  bool output_inf_nan_numbers;
  bool tabs;

  uint32_t spaces;
} g_flags = {0};

std::string  //
parse_flags(int argc, char** argv) {
  g_flags.spaces = 4;

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

    if (!strcmp(arg, "c") || !strcmp(arg, "compact-output")) {
      g_flags.compact_output = true;
      continue;
    }
    if (!strcmp(arg, "output-cbor-metadata-as-comments")) {
      g_flags.output_cbor_metadata_as_comments = true;
      continue;
    }
    if (!strcmp(arg, "output-extra-comma")) {
      g_flags.output_extra_comma = true;
      continue;
    }
    if (!strcmp(arg, "output-inf-nan-numbers")) {
      g_flags.output_inf_nan_numbers = true;
      continue;
    }
    if (!strncmp(arg, "s=", 2) || !strncmp(arg, "spaces=", 7)) {
      while (*arg++ != '=') {
      }
      if (('0' <= arg[0]) && (arg[0] <= '8') && (arg[1] == '\x00')) {
        g_flags.spaces = arg[0] - '0';
        continue;
      }
      return g_usage;
    }
    if (!strcmp(arg, "t") || !strcmp(arg, "tabs")) {
      g_flags.tabs = true;
      continue;
    }

    return g_usage;
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return "";
}

// ----

std::string  //
flush_dst() {
  while (true) {
    size_t n = g_dst.reader_length();
    if (n == 0) {
      break;
    }
    size_t i = fwrite(g_dst.reader_pointer(), 1, n, stdout);
    g_dst.meta.ri += i;
    if (i < n) {
      return "main: error writing to stdout";
    }
  }
  g_dst.compact();
  return "";
}

std::string  //
write_dst_slow(const void* s, size_t n) {
  const uint8_t* p = static_cast<const uint8_t*>(s);
  while (n > 0) {
    size_t i = g_dst.writer_length();
    if (i == 0) {
      TRY(flush_dst());
      i = g_dst.writer_length();
      if (i == 0) {
        return "main: g_dst buffer is full";
      }
    }

    if (i > n) {
      i = n;
    }
    memcpy(g_dst.data.ptr + g_dst.meta.wi, p, i);
    g_dst.meta.wi += i;
    p += i;
    n -= i;
    g_wrote_to_dst = true;
  }
  return "";
}

inline std::string  //
write_dst(const void* s, size_t n) {
  if (n <= (DST_BUFFER_ARRAY_SIZE - g_dst.meta.wi)) {
    memcpy(g_dst.data.ptr + g_dst.meta.wi, s, n);
    g_dst.meta.wi += n;
    g_wrote_to_dst = true;
    return "";
  }
  return write_dst_slow(s, n);
}

// ----

class Callbacks : public wuffs_aux::DecodeCborCallbacks {
 public:
  Callbacks() = default;

  std::string WritePreambleAndUpdateContext() {
    // Write preceding punctuation, whitespace and indentation. Update g_ctx.
    do {
      switch (g_ctx) {
        case context::none:
          goto skip_indentation;
        case context::in_list_after_bracket:
          g_ctx = context::in_list_after_value;
          break;
        case context::in_list_after_value:
          TRY(write_dst(",", 1));
          break;
        case context::in_dict_after_brace:
          g_ctx = context::in_dict_after_key;
          break;
        case context::in_dict_after_key:
          TRY(write_dst(": ", g_flags.compact_output ? 1 : 2));
          g_ctx = context::in_dict_after_value;
          goto skip_indentation;
        case context::in_dict_after_value:
          TRY(write_dst(",", 1));
          g_ctx = context::in_dict_after_key;
          break;
      }

      if (!g_flags.compact_output) {
        uint32_t indent = g_depth * g_bytes_per_indent_depth;
        TRY(write_dst(g_new_line_then_256_indent_bytes, 1 + (indent & 0xFF)));
        for (indent >>= 8; indent > 0; indent--) {
          TRY(write_dst(g_new_line_then_256_indent_bytes + 1, 0x100));
        }
      }
    } while (false);
  skip_indentation:

    // Write any CBOR tags.
    if (g_flags.output_cbor_metadata_as_comments) {
      for (const auto& cbor_tag : g_cbor_tags) {
        uint8_t buf[WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL];
        size_t n = wuffs_base__render_number_u64(
            wuffs_base__make_slice_u8(&buf[0],
                                      WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL),
            cbor_tag, WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
        TRY(write_dst("/*cbor:tag", 10));
        TRY(write_dst(&buf[0], n));
        TRY(write_dst("*/", 2));
      }
      g_cbor_tags.clear();
    }

    return "";
  }

  std::string AppendNull() override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      return "main: invalid JSON map key";
    }
    return write_dst("null", 4);
  }

  std::string AppendUndefined() override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      return "main: invalid JSON map key";
    }
    // JSON's closest approximation to "undefined" is "null".
    if (g_flags.output_cbor_metadata_as_comments) {
      return write_dst("/*cbor:undefined*/null", 22);
    }
    return write_dst("null", 4);
  }

  std::string AppendBool(bool val) override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      return "main: invalid JSON map key";
    }
    if (val) {
      return write_dst("true", 4);
    }
    return write_dst("false", 5);
  }

  std::string AppendF64(double val) override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      return "main: invalid JSON map key";
    }

    uint8_t buf[64];
    constexpr uint32_t precision = 0;
    size_t n = wuffs_base__render_number_f64(
        wuffs_base__make_slice_u8(&buf[0], sizeof buf), val, precision,
        WUFFS_BASE__RENDER_NUMBER_FXX__JUST_ENOUGH_PRECISION);
    if (!g_flags.output_inf_nan_numbers) {
      // JSON numbers don't include Infinities or NaNs. For such numbers, their
      // IEEE 754 bit representation's 11 exponent bits are all on.
      uint64_t u =
          wuffs_base__ieee_754_bit_representation__from_f64_to_u64(val);
      if (((u >> 52) & 0x7FF) == 0x7FF) {
        if (g_flags.output_cbor_metadata_as_comments) {
          TRY(write_dst("/*cbor:", 7));
          TRY(write_dst(&buf[0], n));
          TRY(write_dst("*/", 2));
        }
        return write_dst("null", 4);
      }
    }
    return write_dst(&buf[0], n);
  }

  std::string AppendI64(int64_t val) override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      TRY(write_dst("\"", 1));
    }

    uint8_t buf[WUFFS_BASE__I64__BYTE_LENGTH__MAX_INCL];
    size_t n = wuffs_base__render_number_i64(
        wuffs_base__make_slice_u8(&buf[0], sizeof buf), val,
        WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
    TRY(write_dst(&buf[0], n));

    if (g_ctx == context::in_dict_after_key) {
      TRY(write_dst("\"", 1));
    }
    return "";
  }

  std::string AppendU64(uint64_t val) override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      TRY(write_dst("\"", 1));
    }

    uint8_t buf[WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL];
    size_t n = wuffs_base__render_number_u64(
        wuffs_base__make_slice_u8(&buf[0], sizeof buf), val,
        WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
    TRY(write_dst(&buf[0], n));

    if (g_ctx == context::in_dict_after_key) {
      TRY(write_dst("\"", 1));
    }
    return "";
  }

  std::string AppendByteString(std::string&& val) override {
    TRY(WritePreambleAndUpdateContext());
    if (g_flags.output_cbor_metadata_as_comments) {
      TRY(write_dst("/*cbor:base64url*/\"", 19));
    } else {
      TRY(write_dst("\"", 1));
    }

    const uint8_t* ptr =
        static_cast<const uint8_t*>(static_cast<const void*>(val.data()));
    size_t len = val.length();
    while (len > 0) {
      constexpr bool closed = true;
      wuffs_base__transform__output o = wuffs_base__base_64__encode(
          g_dst.writer_slice(),
          wuffs_base__make_slice_u8(const_cast<uint8_t*>(ptr), len), closed,
          WUFFS_BASE__BASE_64__URL_ALPHABET);
      g_dst.meta.wi += o.num_dst;
      ptr += o.num_src;
      len -= o.num_src;
      if (o.status.repr == nullptr) {
        if (len != 0) {
          return "main: internal error: inconsistent base-64 length";
        }
        break;
      } else if (o.status.repr != wuffs_base__suspension__short_write) {
        return o.status.message();
      }
      TRY(flush_dst());
    }

    return write_dst("\"", 1);
  }

  std::string AppendTextString(std::string&& val) override {
    TRY(WritePreambleAndUpdateContext());
    TRY(write_dst("\"", 1));
    const uint8_t* ptr =
        static_cast<const uint8_t*>(static_cast<const void*>(val.data()));
    size_t len = val.length();
  loop:
    if (len > 0) {
      for (size_t i = 0; i < len; i++) {
        uint8_t c = ptr[i];
        if ((c == '"') || (c == '\\') || (c < 0x20)) {
          TRY(write_dst(ptr, i));
          TRY(write_dst(&ascii_escapes[8 * static_cast<size_t>(c) + 1],
                        ascii_escapes[8 * static_cast<size_t>(c)]));
          ptr += i + 1;
          len -= i + 1;
          goto loop;
        }
      }
      TRY(write_dst(ptr, len));
    }
    return write_dst("\"", 1);
  }

  std::string AppendMinus1MinusX(uint64_t val) override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      TRY(write_dst("\"", 1));
    }

    val++;
    if (val == 0) {
      // See the cbor.TOKEN_VALUE_MINOR__MINUS_1_MINUS_X comment re overflow.
      TRY(write_dst("-18446744073709551616", 21));
    } else {
      uint8_t buf[1 + WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL];
      uint8_t* b = &buf[0];
      *b++ = '-';
      size_t n = wuffs_base__render_number_u64(
          wuffs_base__make_slice_u8(b, WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL),
          val, WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
      TRY(write_dst(&buf[0], 1 + n));
    }

    if (g_ctx == context::in_dict_after_key) {
      TRY(write_dst("\"", 1));
    }
    return "";
  }

  std::string AppendCborSimpleValue(uint8_t val) override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      return "main: invalid JSON map key";
    }

    if (!g_flags.output_cbor_metadata_as_comments) {
      return write_dst("null", 4);
    }
    uint8_t buf[WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL];
    size_t n = wuffs_base__render_number_u64(
        wuffs_base__make_slice_u8(&buf[0],
                                  WUFFS_BASE__U64__BYTE_LENGTH__MAX_INCL),
        val, WUFFS_BASE__RENDER_NUMBER_XXX__DEFAULT_OPTIONS);
    TRY(write_dst("/*cbor:simple", 13));
    TRY(write_dst(&buf[0], n));
    return write_dst("*/null", 6);
  }

  std::string AppendCborTag(uint64_t val) override {
    // No call to WritePreambleAndUpdateContext. A CBOR tag isn't a value. It
    // decorates the upcoming value.
    if (g_flags.output_cbor_metadata_as_comments) {
      g_cbor_tags.push_back(val);
    }
    return "";
  }

  std::string Push(uint32_t flags) override {
    TRY(WritePreambleAndUpdateContext());
    if (g_ctx == context::in_dict_after_key) {
      return "main: invalid JSON map key";
    }

    g_depth++;
    g_ctx = (flags & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST)
                ? context::in_list_after_bracket
                : context::in_dict_after_brace;
    return write_dst(
        (flags & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST) ? "[" : "{", 1);
  }

  std::string Pop(uint32_t flags) override {
    // No call to WritePreambleAndUpdateContext. We write the extra comma,
    // outdent, etc. ourselves.
    g_depth--;
    if (g_flags.compact_output) {
      // No-op.
    } else if ((g_ctx != context::in_list_after_bracket) &&
               (g_ctx != context::in_dict_after_brace)) {
      if (g_flags.output_extra_comma) {
        TRY(write_dst(",", 1));
      }
      uint32_t indent = g_depth * g_bytes_per_indent_depth;
      TRY(write_dst(g_new_line_then_256_indent_bytes, 1 + (indent & 0xFF)));
      for (indent >>= 8; indent > 0; indent--) {
        TRY(write_dst(g_new_line_then_256_indent_bytes + 1, 0x100));
      }
    }
    g_ctx = (flags & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST)
                ? context::in_list_after_value
                : context::in_dict_after_value;
    return write_dst(
        (flags & WUFFS_BASE__TOKEN__VBD__STRUCTURE__FROM_LIST) ? "]" : "}", 1);
  }
};

// ----

std::string  //
main1(int argc, char** argv) {
  g_dst = wuffs_base__ptr_u8__writer(&g_dst_array[0], DST_BUFFER_ARRAY_SIZE);
  g_depth = 0;
  g_ctx = context::none;
  g_wrote_to_dst = false;

  TRY(parse_flags(argc, argv));

  g_new_line_then_256_indent_bytes =
      g_flags.tabs ? NEW_LINE_THEN_256_TABS : NEW_LINE_THEN_256_SPACES;
  g_bytes_per_indent_depth = g_flags.tabs ? 1 : g_flags.spaces;

  FILE* in = stdin;
  if (g_flags.remaining_argc > 1) {
    return g_usage;
  } else if (g_flags.remaining_argc == 1) {
    in = fopen(g_flags.remaining_argv[0], "rb");
    if (!in) {
      return std::string("main: cannot read input file");
    }
  }

  Callbacks callbacks;
  wuffs_aux::sync_io::FileInput input(in);
  return wuffs_aux::DecodeCbor(callbacks, input).error_message;
}

// ----

int  //
compute_exit_code(std::string status_msg) {
  if (status_msg.empty()) {
    return 0;
  }
  fputs(status_msg.c_str(), stderr);
  fputc('\n', stderr);
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
  size_t i = status_msg.find('=');
  if (i != std::string::npos) {
    status_msg = status_msg.substr(0, i);
  }
  return (status_msg.find("internal error:") != std::string::npos) ? 2 : 1;
}

int  //
main(int argc, char** argv) {
  std::string z = main1(argc, argv);
  if (g_wrote_to_dst) {
    std::string z1 = write_dst("\n", 1);
    std::string z2 = flush_dst();
    z = !z.empty() ? z : (!z1.empty() ? z1 : z2);
  }
  return compute_exit_code(z);
}
