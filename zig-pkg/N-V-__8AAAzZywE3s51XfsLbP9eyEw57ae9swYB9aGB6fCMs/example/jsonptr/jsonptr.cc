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

// jsonptr is discussed extensively at
// https://nigeltao.github.io/blog/2020/jsonptr.html

/*
jsonptr is a JSON formatter (pretty-printer) that supports the JSON Pointer
(RFC 6901) query syntax. It reads UTF-8 JSON from stdin and writes
canonicalized, formatted UTF-8 JSON to stdout.

See the "const char* g_usage" string below for details.

----

JSON Pointer (and this program's implementation) is one of many JSON query
languages and JSON tools, such as jq, jql and JMESPath. This one is relatively
simple and fewer-featured compared to those others.

One benefit of simplicity is that this program's JSON and JSON Pointer
implementations do not dynamically allocate or free memory (yet it does not
require that the entire input fits in memory at once). They are therefore
trivially protected against certain bug classes: memory leaks, double-frees and
use-after-frees.

The core JSON implementation is also written in the Wuffs programming language
(and then transpiled to C/C++), which is memory-safe (e.g. array indexing is
bounds-checked) but also guards against integer arithmetic overflows.

For defense in depth, on Linux, this program also self-imposes a
SECCOMP_MODE_STRICT sandbox before reading (or otherwise processing) its input
or writing its output. Under this sandbox, the only permitted system calls are
read, write, exit and sigreturn.

All together, this program aims to safely handle untrusted JSON files without
fear of security bugs such as remote code execution.

----

As of 2020-02-24, this program passes all 318 "test_parsing" cases from the
JSON test suite (https://github.com/nst/JSONTestSuite), an appendix to the
"Parsing JSON is a Minefield" article (http://seriot.ch/parsing_json.php) that
was first published on 2016-10-26 and updated on 2018-03-30.

After modifying this program, run "build-example.sh example/jsonptr/" and then
"script/run-json-test-suite.sh" to catch correctness regressions.

----

This program uses Wuffs' JSON decoder at a relatively low level, processing the
decoder's token-stream output individually. The core loop, in pseudo-code, is
"for_each_token { handle_token(etc); }", where the handle_token function
changes global state (e.g. the `g_depth` and `g_ctx` variables) and prints
output text based on that state and the token's source text. Notably,
handle_token is not recursive, even though JSON values can nest.

This approach is centered around JSON tokens. Each JSON 'thing' (e.g. number,
string, object) comprises one or more JSON tokens.

An alternative, higher-level approach is in the sibling example/jsonfindptrs
program. Neither approach is better or worse per se, but when studying this
program, be aware that there are multiple ways to use Wuffs' JSON decoder.

The two programs, jsonfindptrs and jsonptr, also demonstrate different
trade-offs with regard to JSON object duplicate keys. The JSON spec permits
different implementations to allow or reject duplicate keys. It is not always
clear which approach is safer. Rejecting them is certainly unambiguous, and
security bugs can lurk in ambiguous corners of a file format, if two different
implementations both silently accept a file but differ on how to interpret it.
On the other hand, in the worst case, detecting duplicate keys requires O(N)
memory, where N is the size of the (potentially untrusted) input.

This program (jsonptr) allows duplicate keys and requires only O(1) memory. As
mentioned above, it doesn't dynamically allocate memory at all, and on Linux,
it runs in a SECCOMP_MODE_STRICT sandbox.

----

To run:

$CXX jsonptr.cc && ./a.out < ../../test/data/github-tags.json; rm -f a.out

for a C++ compiler $CXX, such as clang++ or g++.
*/

#if defined(__cplusplus) && (__cplusplus < 201103L)
#error "This C++ program requires -std=c++11 or later"
#endif

#include <errno.h>
#include <fcntl.h>
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
#define WUFFS_CONFIG__MODULE__JSON

// If building this program in an environment that doesn't easily accommodate
// relative includes, you can use the script/inline-c-relative-includes.go
// program to generate a stand-alone C++ file.
#include "../../release/c/wuffs-unsupported-snapshot.c"

#if defined(__linux__)
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#define WUFFS_EXAMPLE_USE_SECCOMP
#endif

#define TRY(error_msg)         \
  do {                         \
    const char* z = error_msg; \
    if (z) {                   \
      return z;                \
    }                          \
  } while (false)

static const char* g_eod = "main: end of data";

static const char* g_usage =
    "Usage: jsonptr -flags input.json\n"
    "\n"
    "Flags:\n"
    "    -c      -compact-output\n"
    "    -d=NUM  -max-output-depth=NUM\n"
    "    -q=STR  -query=STR\n"
    "    -s=NUM  -spaces=NUM\n"
    "    -t      -tabs\n"
    "            -fail-if-unsandboxed\n"
    "            -input-allow-comments\n"
    "            -input-allow-extra-comma\n"
    "            -input-allow-inf-nan-numbers\n"
    "            -input-jwcc\n"
    "            -jwcc\n"
    "            -output-comments\n"
    "            -output-extra-comma\n"
    "            -output-inf-nan-numbers\n"
    "            -strict-json-pointer-syntax\n"
    "\n"
    "The input.json filename is optional. If absent, it reads from stdin.\n"
    "\n"
    "----\n"
    "\n"
    "jsonptr is a JSON formatter (pretty-printer) that supports the JSON\n"
    "Pointer (RFC 6901) query syntax. It reads UTF-8 JSON from stdin and\n"
    "writes canonicalized, formatted UTF-8 JSON to stdout.\n"
    "\n"
    "Canonicalized means that e.g. \"abc\\u000A\\tx\\u0177z\" is re-written\n"
    "as \"abc\\n\\txŷz\". It does not sort object keys, nor does it reject\n"
    "duplicate keys. Canonicalization does not imply Unicode normalization.\n"
    "\n"
    "Formatted means that arrays' and objects' elements are indented, each\n"
    "on its own line. Configure this with the -c / -compact-output, -s=NUM /\n"
    "-spaces=NUM (for NUM ranging from 0 to 8) and -t / -tabs flags.\n"
    "\n"
    "The -input-allow-comments flag allows \"/*slash-star*/\" and\n"
    "\"//slash-slash\" C-style comments within JSON input. Such comments are\n"
    "stripped from the output unless -output-comments was also set.\n"
    "\n"
    "The -input-allow-extra-comma flag allows input like \"[1,2,]\", with a\n"
    "comma after the final element of a JSON list or dictionary.\n"
    "\n"
    "The -input-allow-inf-nan-numbers flag allows non-finite floating point\n"
    "numbers (infinities and not-a-numbers) within JSON input. This flag\n"
    "requires that -output-inf-nan-numbers also be set.\n"
    "\n"
    "The -output-comments flag copies any input comments to the output. It\n"
    "has no effect unless -input-allow-comments was also set. Comments look\n"
    "better after commas than before them, but a closing \"]\" or \"}\" can\n"
    "occur after arbitrarily many comments, so -output-comments also requires\n"
    "that one or both of -compact-output and -output-extra-comma be set.\n"
    "\n"
    "With -output-comments, consecutive blank lines collapse to a single\n"
    "blank line. Without that flag, all blank lines are removed.\n"
    "\n"
    "The -output-extra-comma flag writes output like \"[1,2,]\", with a comma\n"
    "after the final element of a JSON list or dictionary. Such commas are\n"
    "non-compliant with the JSON specification but many parsers accept them\n"
    "and they can produce simpler line-based diffs. This flag is ignored when\n"
    "-compact-output is set.\n"
    "\n"
    "Combining some of those flags results in speaking JWCC (JSON With Commas\n"
    "and Comments), not plain JSON. For convenience, the -input-jwcc or -jwcc\n"
    "flags enables the first two or all four of:\n"
    "            -input-allow-comments\n"
    "            -input-allow-extra-comma\n"
    "            -output-comments\n"
    "            -output-extra-comma\n"
    "\n"
#if defined(WUFFS_EXAMPLE_SPEAK_JWCC_NOT_JSON)
    "This program was configured at compile time to always use -jwcc.\n"
    "\n"
#endif
    "----\n"
    "\n"
    "The -q=STR or -query=STR flag gives an optional JSON Pointer query, to\n"
    "print a subset of the input. For example, given RFC 6901 section 5's\n"
    "sample input (https://tools.ietf.org/rfc/rfc6901.txt), this command:\n"
    "    jsonptr -query=/foo/1 rfc-6901-json-pointer.json\n"
    "will print:\n"
    "    \"baz\"\n"
    "\n"
    "An absent query is equivalent to the empty query, which identifies the\n"
    "entire input (the root value). Unlike a file system, the \"/\" query\n"
    "does not identify the root. Instead, \"\" is the root and \"/\" is the\n"
    "child (the value in a key-value pair) of the root whose key is the empty\n"
    "string. Similarly, \"/xyz\" and \"/xyz/\" are two different nodes.\n"
    "\n"
    "If the query found a valid JSON value, this program will return a zero\n"
    "exit code even if the rest of the input isn't valid JSON. If the query\n"
    "did not find a value, or found an invalid one, this program returns a\n"
    "non-zero exit code, but may still print partial output to stdout.\n"
    "\n"
    "The JSON specification (https://json.org/) permits implementations that\n"
    "allow duplicate keys, as this one does. This JSON Pointer implementation\n"
    "is also greedy, following the first match for each fragment without\n"
    "back-tracking. For example, the \"/foo/bar\" query will fail if the root\n"
    "object has multiple \"foo\" children but the first one doesn't have a\n"
    "\"bar\" child, even if later ones do.\n"
    "\n"
    "The -strict-json-pointer-syntax flag restricts the -query=STR string to\n"
    "exactly RFC 6901, with only two escape sequences: \"~0\" and \"~1\" for\n"
    "\"~\" and \"/\". Without this flag, this program also lets \"~n\",\n"
    "\"~r\" and \"~t\" escape the New Line, Carriage Return and Horizontal\n"
    "Tab ASCII control characters, which can work better with line oriented\n"
    "(and tab separated) Unix tools that assume exactly one record (e.g. one\n"
    "JSON Pointer string) per line.\n"
    "\n"
    "----\n"
    "\n"
    "The -d=NUM or -max-output-depth=NUM flag gives the maximum (inclusive)\n"
    "output depth. JSON containers ([] arrays and {} objects) can hold other\n"
    "containers. When this flag is set, containers at depth NUM are replaced\n"
    "with \"[…]\" or \"{…}\". A bare -d or -max-output-depth is equivalent to\n"
    "-d=1. The flag's absence is equivalent to an unlimited output depth.\n"
    "\n"
    "The -max-output-depth flag only affects the program's output. It doesn't\n"
    "affect whether or not the input is considered valid JSON. The JSON\n"
    "specification permits implementations to set their own maximum input\n"
    "depth. This JSON implementation sets it to 1024.\n"
    "\n"
    "Depth is measured in terms of nested containers. It is unaffected by the\n"
    "number of spaces or tabs used to indent.\n"
    "\n"
    "When both -max-output-depth and -query are set, the output depth is\n"
    "measured from when the query resolves, not from the input root. The\n"
    "input depth (measured from the root) is still limited to 1024.\n"
    "\n"
    "----\n"
    "\n"
    "The -fail-if-unsandboxed flag causes the program to exit if it does not\n"
    "self-impose a sandbox. On Linux, it self-imposes a SECCOMP_MODE_STRICT\n"
    "sandbox, regardless of whether this flag was set.";

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

// Wuffs allows either statically or dynamically allocated work buffers. This
// program exercises static allocation.
#define WORK_BUFFER_ARRAY_SIZE \
  WUFFS_JSON__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE
#if WORK_BUFFER_ARRAY_SIZE > 0
uint8_t g_work_buffer_array[WORK_BUFFER_ARRAY_SIZE];
#else
// Not all C/C++ compilers support 0-length arrays.
uint8_t g_work_buffer_array[1];
#endif

bool g_sandboxed = false;

int g_input_file_descriptor = 0;  // A 0 default means stdin.

#define TWO_NEW_LINES_THEN_256_SPACES                                          \
  "\n\n                                                                      " \
  "                                                                          " \
  "                                                                          " \
  "                                      "
#define TWO_NEW_LINES_THEN_256_TABS                                            \
  "\n\n\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t" \
  "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"

const char* g_two_new_lines_then_256_indent_bytes;
uint32_t g_bytes_per_indent_depth;

#ifndef DST_BUFFER_ARRAY_SIZE
#define DST_BUFFER_ARRAY_SIZE (32 * 1024)
#endif
#ifndef SRC_BUFFER_ARRAY_SIZE
#define SRC_BUFFER_ARRAY_SIZE (32 * 1024)
#endif
// 1 token is 8 bytes. 4Ki tokens is 32KiB.
#ifndef TOKEN_BUFFER_ARRAY_SIZE
#define TOKEN_BUFFER_ARRAY_SIZE (4 * 1024)
#endif

uint8_t g_dst_array[DST_BUFFER_ARRAY_SIZE];
uint8_t g_src_array[SRC_BUFFER_ARRAY_SIZE];
wuffs_base__token g_tok_array[TOKEN_BUFFER_ARRAY_SIZE];

wuffs_base__io_buffer g_dst;
wuffs_base__io_buffer g_src;
wuffs_base__token_buffer g_tok;

// g_cursor_index is the g_src.data.ptr index between the previous and current
// token. An invariant is that (g_cursor_index <= g_src.meta.ri).
size_t g_cursor_index;

uint32_t g_depth;

enum class context {
  none,
  in_list_after_bracket,
  in_list_after_value,
  in_dict_after_brace,
  in_dict_after_key,
  in_dict_after_value,
  end_of_data,
} g_ctx;

bool  //
in_dict_before_key() {
  return (g_ctx == context::in_dict_after_brace) ||
         (g_ctx == context::in_dict_after_value);
}

uint64_t g_num_input_blank_lines;

bool g_is_after_comment;

uint32_t g_suppress_write_dst;
bool g_wrote_to_dst;

wuffs_json__decoder g_dec;

// ----

// Query is a JSON Pointer query. After initializing with a NUL-terminated C
// string, its multiple fragments are consumed as the program walks the JSON
// data from stdin. For example, letting "$" denote a NUL, suppose that we
// started with a query string of "/apple/banana/12/durian" and are currently
// trying to match the second fragment, "banana", so that Query::m_depth is 2:
//
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  / a p p l e / b a n a n a / 1 2 / d u r i a n $
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//                ^           ^
//                m_frag_i    m_frag_k
//
// The two pointers m_frag_i and m_frag_k (abbreviated as mfi and mfk) are the
// start (inclusive) and end (exclusive) of the query fragment. They satisfy
// (mfi <= mfk) and may be equal if the fragment empty (note that "" is a valid
// JSON object key).
//
// The m_frag_j (mfj) pointer moves between these two, or is nullptr. An
// invariant is that (((mfi <= mfj) && (mfj <= mfk)) || (mfj == nullptr)).
//
// Wuffs' JSON tokenizer can portray a single JSON string as multiple Wuffs
// tokens, as backslash-escaped values within that JSON string may each get
// their own token.
//
// At the start of each object key (a JSON string), mfj is set to mfi.
//
// While mfj remains non-nullptr, each token's unescaped contents are then
// compared to that part of the fragment from mfj to mfk. If it is a prefix
// (including the case of an exact match), then mfj is advanced by the
// unescaped length. Otherwise, mfj is set to nullptr.
//
// Comparison accounts for JSON Pointer's escaping notation: "~0" and "~1" in
// the query (not the JSON value) are unescaped to "~" and "/" respectively.
// "~n" and "~r" are also unescaped to "\n" and "\r". The program is
// responsible for calling Query::validate (with a strict_json_pointer_syntax
// argument) before otherwise using this class.
//
// The mfj pointer therefore advances from mfi to mfk, or drops out, as we
// incrementally match the object key with the query fragment. For example, if
// we have already matched the "ban" of "banana", then we would accept any of
// an "ana" token, an "a" token or a "\u0061" token, amongst others. They would
// advance mfj by 3, 1 or 1 bytes respectively.
//
//                      mfj
//                      v
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  / a p p l e / b a n a n a / 1 2 / d u r i a n $
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//                ^           ^
//                mfi         mfk
//
// At the end of each object key (or equivalently, at the start of each object
// value), if mfj is non-nullptr and equal to (but not less than) mfk then we
// have a fragment match: the query fragment equals the object key. If there is
// a next fragment (in this example, "12") we move the frag_etc pointers to its
// start and end and increment Query::m_depth. Otherwise, we have matched the
// complete query, and the upcoming JSON value is the result of that query.
//
// The discussion above centers on object keys. If the query fragment is
// numeric then it can also match as an array index: the string fragment "12"
// will match an array's 13th element (starting counting from zero). See RFC
// 6901 for its precise definition of an "array index" number.
//
// Array index fragment match is represented by the Query::m_array_index field,
// whose type (wuffs_base__result_u64) is a result type. An error result means
// that the fragment is not an array index. A value result holds the number of
// list elements remaining. When matching a query fragment in an array (instead
// of in an object), each element ticks this number down towards zero. At zero,
// the upcoming JSON value is the one that matches the query fragment.
class Query {
 private:
  uint8_t* m_frag_i;
  uint8_t* m_frag_j;
  uint8_t* m_frag_k;

  uint32_t m_depth;

  wuffs_base__result_u64 m_array_index;

 public:
  void reset(char* query_c_string) {
    m_frag_i = (uint8_t*)query_c_string;
    m_frag_j = (uint8_t*)query_c_string;
    m_frag_k = (uint8_t*)query_c_string;
    m_depth = 0;
    m_array_index.status.repr = "#main: not an array index query fragment";
    m_array_index.value = 0;
  }

  void restart_fragment(bool enable) { m_frag_j = enable ? m_frag_i : nullptr; }

  bool is_at(uint32_t depth) { return m_depth == depth; }

  // tick returns whether the fragment is a valid array index whose value is
  // zero. If valid but non-zero, it decrements it and returns false.
  bool tick() {
    if (m_array_index.status.is_ok()) {
      if (m_array_index.value == 0) {
        return true;
      }
      m_array_index.value--;
    }
    return false;
  }

  // next_fragment moves to the next fragment, returning whether it existed.
  bool next_fragment() {
    uint8_t* k = m_frag_k;
    uint32_t d = m_depth;

    this->reset(nullptr);

    if (!k || (*k != '/')) {
      return false;
    }
    k++;

    bool all_digits = true;
    uint8_t* i = k;
    while ((*k != '\x00') && (*k != '/')) {
      all_digits = all_digits && ('0' <= *k) && (*k <= '9');
      k++;
    }
    m_frag_i = i;
    m_frag_j = i;
    m_frag_k = k;
    m_depth = d + 1;
    if (all_digits) {
      // wuffs_base__parse_number_u64 rejects leading zeroes, e.g. "00", "07".
      m_array_index = wuffs_base__parse_number_u64(
          wuffs_base__make_slice_u8(i, k - i),
          WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
    }
    return true;
  }

  bool matched_all() { return m_frag_k == nullptr; }

  bool matched_fragment() { return m_frag_j && (m_frag_j == m_frag_k); }

  void incremental_match_slice(uint8_t* ptr, size_t len) {
    if (!m_frag_j) {
      return;
    }
    uint8_t* j = m_frag_j;
    while (true) {
      if (len == 0) {
        m_frag_j = j;
        return;
      }

      if (*j == '\x00') {
        break;

      } else if (*j == '~') {
        j++;
        if (*j == '0') {
          if (*ptr != '~') {
            break;
          }
        } else if (*j == '1') {
          if (*ptr != '/') {
            break;
          }
        } else if (*j == 'n') {
          if (*ptr != '\n') {
            break;
          }
        } else if (*j == 'r') {
          if (*ptr != '\r') {
            break;
          }
        } else if (*j == 't') {
          if (*ptr != '\t') {
            break;
          }
        } else {
          break;
        }

      } else if (*j != *ptr) {
        break;
      }

      j++;
      ptr++;
      len--;
    }
    m_frag_j = nullptr;
  }

  void incremental_match_code_point(uint32_t code_point) {
    if (!m_frag_j) {
      return;
    }
    uint8_t u[WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL];
    size_t n = wuffs_base__utf_8__encode(
        wuffs_base__make_slice_u8(&u[0],
                                  WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL),
        code_point);
    if (n > 0) {
      this->incremental_match_slice(&u[0], n);
    }
  }

  // validate returns whether the (ptr, len) arguments form a valid JSON
  // Pointer. In particular, it must be valid UTF-8, and either be empty or
  // start with a '/'. Any '~' within must immediately be followed by either
  // '0' or '1'. If strict_json_pointer_syntax is false, a '~' may also be
  // followed by either 'n', 'r' or 't'.
  static bool validate(char* query_c_string,
                       size_t length,
                       bool strict_json_pointer_syntax) {
    if (length <= 0) {
      return true;
    }
    if (query_c_string[0] != '/') {
      return false;
    }
    wuffs_base__slice_u8 s =
        wuffs_base__make_slice_u8((uint8_t*)query_c_string, length);
    bool previous_was_tilde = false;
    while (s.len > 0) {
      wuffs_base__utf_8__next__output o = wuffs_base__utf_8__next(s.ptr, s.len);
      if (!o.is_valid()) {
        return false;
      }

      if (previous_was_tilde) {
        switch (o.code_point) {
          case '0':
          case '1':
            break;
          case 'n':
          case 'r':
          case 't':
            if (strict_json_pointer_syntax) {
              return false;
            }
            break;
          default:
            return false;
        }
      }
      previous_was_tilde = o.code_point == '~';

      s.ptr += o.byte_length;
      s.len -= o.byte_length;
    }
    return !previous_was_tilde;
  }
} g_query;

// ----

struct {
  int remaining_argc;
  char** remaining_argv;

  bool compact_output;
  bool fail_if_unsandboxed;
  bool input_allow_comments;
  bool input_allow_extra_comma;
  bool input_allow_inf_nan_numbers;
  bool output_comments;
  bool output_extra_comma;
  bool output_inf_nan_numbers;
  bool strict_json_pointer_syntax;
  bool tabs;

  uint32_t max_output_depth;
  uint32_t spaces;

  char* query_c_string;
} g_flags = {0};

const char*  //
parse_flags(int argc, char** argv) {
  g_flags.spaces = 4;
  g_flags.max_output_depth = 0xFFFFFFFF;

#if defined(WUFFS_EXAMPLE_SPEAK_JWCC_NOT_JSON)
  g_flags.input_allow_comments = true;
  g_flags.input_allow_extra_comma = true;
  g_flags.output_comments = true;
  g_flags.output_extra_comma = true;
#endif

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
    if (!strcmp(arg, "d") || !strcmp(arg, "max-output-depth")) {
      g_flags.max_output_depth = 1;
      continue;
    } else if (!strncmp(arg, "d=", 2) ||
               !strncmp(arg, "max-output-depth=", 16)) {
      while (*arg++ != '=') {
      }
      wuffs_base__result_u64 u = wuffs_base__parse_number_u64(
          wuffs_base__make_slice_u8((uint8_t*)arg, strlen(arg)),
          WUFFS_BASE__PARSE_NUMBER_XXX__DEFAULT_OPTIONS);
      if (u.status.is_ok() && (u.value <= 0xFFFFFFFF)) {
        g_flags.max_output_depth = (uint32_t)(u.value);
        continue;
      }
      return g_usage;
    }
    if (!strcmp(arg, "fail-if-unsandboxed")) {
      g_flags.fail_if_unsandboxed = true;
      continue;
    }
    if (!strcmp(arg, "input-allow-comments")) {
      g_flags.input_allow_comments = true;
      continue;
    }
    if (!strcmp(arg, "input-allow-extra-comma")) {
      g_flags.input_allow_extra_comma = true;
      continue;
    }
    if (!strcmp(arg, "input-allow-inf-nan-numbers")) {
      g_flags.input_allow_inf_nan_numbers = true;
      continue;
    }
    if (!strcmp(arg, "input-jwcc")) {
      g_flags.input_allow_comments = true;
      g_flags.input_allow_extra_comma = true;
      continue;
    }
    if (!strcmp(arg, "jwcc")) {
      g_flags.input_allow_comments = true;
      g_flags.input_allow_extra_comma = true;
      g_flags.output_comments = true;
      g_flags.output_extra_comma = true;
      continue;
    }
    if (!strcmp(arg, "output-comments")) {
      g_flags.output_comments = true;
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
    if (!strncmp(arg, "q=", 2) || !strncmp(arg, "query=", 6)) {
      while (*arg++ != '=') {
      }
      g_flags.query_c_string = arg;
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
    if (!strcmp(arg, "strict-json-pointer-syntax")) {
      g_flags.strict_json_pointer_syntax = true;
      continue;
    }
    if (!strcmp(arg, "t") || !strcmp(arg, "tabs")) {
      g_flags.tabs = true;
      continue;
    }

    return g_usage;
  }

  if (g_flags.query_c_string &&
      !Query::validate(g_flags.query_c_string, strlen(g_flags.query_c_string),
                       g_flags.strict_json_pointer_syntax)) {
    return "main: bad JSON Pointer (RFC 6901) syntax for the -query=STR flag";
  }

  g_flags.remaining_argc = argc - c;
  g_flags.remaining_argv = argv + c;
  return nullptr;
}

const char*  //
initialize_globals(int argc, char** argv) {
  g_dst = wuffs_base__make_io_buffer(
      wuffs_base__make_slice_u8(g_dst_array, DST_BUFFER_ARRAY_SIZE),
      wuffs_base__empty_io_buffer_meta());

  g_src = wuffs_base__make_io_buffer(
      wuffs_base__make_slice_u8(g_src_array, SRC_BUFFER_ARRAY_SIZE),
      wuffs_base__empty_io_buffer_meta());

  g_tok = wuffs_base__make_token_buffer(
      wuffs_base__make_slice_token(g_tok_array, TOKEN_BUFFER_ARRAY_SIZE),
      wuffs_base__empty_token_buffer_meta());

  g_cursor_index = 0;

  g_depth = 0;

  g_ctx = context::none;

  g_num_input_blank_lines = 0;

  g_is_after_comment = false;

  TRY(parse_flags(argc, argv));
  if (g_flags.fail_if_unsandboxed && !g_sandboxed) {
    return "main: unsandboxed";
  }
  if (g_flags.output_comments && !g_flags.compact_output &&
      !g_flags.output_extra_comma) {
    return "main: -output-comments requires one or both of -compact-output and "
           "-output-extra-comma";
  }
  if (g_flags.input_allow_inf_nan_numbers && !g_flags.output_inf_nan_numbers) {
    return "main: -input-allow-inf-nan-numbers requires "
           "-output-inf-nan-numbers";
  }
  const int stdin_fd = 0;
  if (g_flags.remaining_argc >
      ((g_input_file_descriptor != stdin_fd) ? 1 : 0)) {
    return g_usage;
  }

  g_two_new_lines_then_256_indent_bytes = g_flags.tabs
                                              ? TWO_NEW_LINES_THEN_256_TABS
                                              : TWO_NEW_LINES_THEN_256_SPACES;
  g_bytes_per_indent_depth = g_flags.tabs ? 1 : g_flags.spaces;

  g_query.reset(g_flags.query_c_string);

  // If the query is non-empty, suppress writing to stdout until we've
  // completed the query.
  g_suppress_write_dst = g_query.next_fragment() ? 1 : 0;
  g_wrote_to_dst = false;

  TRY(g_dec.initialize(sizeof__wuffs_json__decoder(), WUFFS_VERSION, 0)
          .message());

  if (g_flags.input_allow_comments) {
    g_dec.set_quirk(WUFFS_JSON__QUIRK_ALLOW_COMMENT_BLOCK, 1);
    g_dec.set_quirk(WUFFS_JSON__QUIRK_ALLOW_COMMENT_LINE, 1);
  }
  if (g_flags.input_allow_extra_comma) {
    g_dec.set_quirk(WUFFS_JSON__QUIRK_ALLOW_EXTRA_COMMA, 1);
  }
  if (g_flags.input_allow_inf_nan_numbers) {
    g_dec.set_quirk(WUFFS_JSON__QUIRK_ALLOW_INF_NAN_NUMBERS, 1);
  }

  // Consume any optional trailing whitespace and comments. This isn't part of
  // the JSON spec, but it works better with line oriented Unix tools (such as
  // "echo 123 | jsonptr" where it's "echo", not "echo -n") or hand-edited JSON
  // files which can accidentally contain trailing whitespace.
  g_dec.set_quirk(WUFFS_JSON__QUIRK_ALLOW_TRAILING_FILLER, 1);

  return nullptr;
}

// ----

// ignore_return_value suppresses errors from -Wall -Werror.
static void  //
ignore_return_value(int ignored) {}

const char*  //
read_src() {
  if (g_src.meta.closed) {
    return "main: internal error: read requested on a closed source";
  }
  g_src.compact();
  if (g_src.meta.wi >= g_src.data.len) {
    return "main: g_src buffer is full";
  }
  while (true) {
    ssize_t n = read(g_input_file_descriptor, g_src.writer_pointer(),
                     g_src.writer_length());
    if (n >= 0) {
      g_src.meta.wi += n;
      g_src.meta.closed = n == 0;
      break;
    } else if (errno != EINTR) {
      return strerror(errno);
    }
  }
  return nullptr;
}

const char*  //
flush_dst() {
  while (true) {
    size_t n = g_dst.reader_length();
    if (n == 0) {
      break;
    }
    const int stdout_fd = 1;
    ssize_t i = write(stdout_fd, g_dst.reader_pointer(), n);
    if (i >= 0) {
      g_dst.meta.ri += i;
    } else if (errno != EINTR) {
      return strerror(errno);
    }
  }
  g_dst.compact();
  return nullptr;
}

const char*  //
write_dst_slow(const void* s, size_t n) {
  const uint8_t* p = static_cast<const uint8_t*>(s);
  while (n > 0) {
    size_t i = g_dst.writer_length();
    if (i == 0) {
      const char* z = flush_dst();
      if (z) {
        return z;
      }
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
  return nullptr;
}

inline const char*  //
write_dst(const void* s, size_t n) {
  if (g_suppress_write_dst > 0) {
    return nullptr;
  } else if (n <= (DST_BUFFER_ARRAY_SIZE - g_dst.meta.wi)) {
    memcpy(g_dst.data.ptr + g_dst.meta.wi, s, n);
    g_dst.meta.wi += n;
    g_wrote_to_dst = true;
    return nullptr;
  }
  return write_dst_slow(s, n);
}

#define TRY_INDENT                                                      \
  do {                                                                  \
    uint32_t adj = (g_num_input_blank_lines > 1) ? 1 : 0;               \
    g_num_input_blank_lines = 0;                                        \
    uint32_t indent = g_depth * g_bytes_per_indent_depth;               \
    TRY(write_dst(g_two_new_lines_then_256_indent_bytes + 1 - adj,      \
                  1 + adj + (indent & 0xFF)));                          \
    for (indent >>= 8; indent > 0; indent--) {                          \
      TRY(write_dst(g_two_new_lines_then_256_indent_bytes + 2, 0x100)); \
    }                                                                   \
  } while (false)

// ----

const char*  //
handle_unicode_code_point(uint32_t ucp) {
  if (ucp < 0x80) {
    return write_dst(&ascii_escapes[8 * ucp + 1], ascii_escapes[8 * ucp]);
  }
  uint8_t u[WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL];
  size_t n = wuffs_base__utf_8__encode(
      wuffs_base__make_slice_u8(&u[0],
                                WUFFS_BASE__UTF_8__BYTE_LENGTH__MAX_INCL),
      ucp);
  if (n == 0) {
    return "main: internal error: unexpected Unicode code point";
  }
  return write_dst(&u[0], n);
}

// ----

inline const char*  //
handle_token(wuffs_base__token t, bool start_of_token_chain) {
  do {
    int64_t vbc = t.value_base_category();
    uint64_t vbd = t.value_base_detail();
    uint64_t token_length = t.length();
    // The "- token_length" is because we incremented g_cursor_index before
    // calling handle_token.
    wuffs_base__slice_u8 tok = wuffs_base__make_slice_u8(
        g_src.data.ptr + g_cursor_index - token_length, token_length);

    // Handle ']' or '}'.
    if ((vbc == WUFFS_BASE__TOKEN__VBC__STRUCTURE) &&
        (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__POP)) {
      if (g_query.is_at(g_depth)) {
        return "main: no match for query";
      }
      if (g_depth <= 0) {
        return "main: internal error: inconsistent g_depth";
      }
      g_depth--;

      if (g_query.matched_all() && (g_depth >= g_flags.max_output_depth)) {
        g_suppress_write_dst--;
        // '…' is U+2026 HORIZONTAL ELLIPSIS, which is 3 UTF-8 bytes.
        TRY(write_dst((vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__FROM_LIST)
                          ? "\"[…]\""
                          : "\"{…}\"",
                      7));
      } else {
        // Write preceding whitespace.
        if ((g_ctx != context::in_list_after_bracket) &&
            (g_ctx != context::in_dict_after_brace) &&
            !g_flags.compact_output) {
          if (g_is_after_comment) {
            TRY_INDENT;
          } else {
            if (g_flags.output_extra_comma) {
              TRY(write_dst(",", 1));
            }
            TRY_INDENT;
          }
        } else {
          g_num_input_blank_lines = 0;
        }

        TRY(write_dst(
            (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__FROM_LIST) ? "]" : "}",
            1));
      }

      g_ctx = (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST)
                  ? context::in_list_after_value
                  : context::in_dict_after_key;
      goto after_value;
    }

    // Write preceding whitespace and punctuation, if it wasn't ']', '}' or a
    // continuation of a multi-token chain.
    if (start_of_token_chain) {
      if (g_is_after_comment) {
        TRY_INDENT;
      } else if (g_ctx == context::in_dict_after_key) {
        TRY(write_dst(": ", g_flags.compact_output ? 1 : 2));
      } else if (g_ctx != context::none) {
        if ((g_ctx != context::in_list_after_bracket) &&
            (g_ctx != context::in_dict_after_brace)) {
          TRY(write_dst(",", 1));
        }
        if (!g_flags.compact_output) {
          TRY_INDENT;
        }
      }

      bool query_matched_fragment = false;
      if (g_query.is_at(g_depth)) {
        switch (g_ctx) {
          case context::in_list_after_bracket:
          case context::in_list_after_value:
            query_matched_fragment = g_query.tick();
            break;
          case context::in_dict_after_key:
            query_matched_fragment = g_query.matched_fragment();
            break;
          default:
            break;
        }
      }
      if (!query_matched_fragment) {
        // No-op.
      } else if (!g_query.next_fragment()) {
        // There is no next fragment. We have matched the complete query, and
        // the upcoming JSON value is the result of that query.
        //
        // Un-suppress writing to stdout and reset the g_ctx and g_depth as if
        // we were about to decode a top-level value. This makes any subsequent
        // indentation be relative to this point, and we will return g_eod
        // after the upcoming JSON value is complete.
        if (g_suppress_write_dst != 1) {
          return "main: internal error: inconsistent g_suppress_write_dst";
        }
        g_suppress_write_dst = 0;
        g_ctx = context::none;
        g_depth = 0;
      } else if ((vbc != WUFFS_BASE__TOKEN__VBC__STRUCTURE) ||
                 !(vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__PUSH)) {
        // The query has moved on to the next fragment but the upcoming JSON
        // value is not a container.
        return "main: no match for query";
      }
    }

    // Handle the token itself: either a container ('[' or '{') or a simple
    // value: string (a chain of raw or escaped parts), literal or number.
    switch (vbc) {
      case WUFFS_BASE__TOKEN__VBC__STRUCTURE:
        if (g_query.matched_all() && (g_depth >= g_flags.max_output_depth)) {
          g_suppress_write_dst++;
        } else {
          TRY(write_dst(
              (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST) ? "[" : "{",
              1));
        }
        g_depth++;
        g_ctx = (vbd & WUFFS_BASE__TOKEN__VBD__STRUCTURE__TO_LIST)
                    ? context::in_list_after_bracket
                    : context::in_dict_after_brace;
        g_num_input_blank_lines = 0;
        return nullptr;

      case WUFFS_BASE__TOKEN__VBC__STRING:
        if (start_of_token_chain) {
          TRY(write_dst("\"", 1));
          g_query.restart_fragment(in_dict_before_key() &&
                                   g_query.is_at(g_depth));
        }

        if (vbd & WUFFS_BASE__TOKEN__VBD__STRING__CONVERT_1_DST_1_SRC_COPY) {
          TRY(write_dst(tok.ptr, tok.len));
          g_query.incremental_match_slice(tok.ptr, tok.len);
        }

        if (t.continued()) {
          return nullptr;
        }
        TRY(write_dst("\"", 1));
        goto after_value;

      case WUFFS_BASE__TOKEN__VBC__UNICODE_CODE_POINT:
        if (!t.continued()) {
          return "main: internal error: unexpected non-continued UCP token";
        }
        TRY(handle_unicode_code_point(vbd));
        g_query.incremental_match_code_point(vbd);
        return nullptr;
    }

    // We have a literal or a number.
    TRY(write_dst(tok.ptr, tok.len));
    goto after_value;
  } while (0);

  // Book-keeping after completing a value (whether a container value or a
  // simple value). Empty parent containers are no longer empty. If the parent
  // container is a "{...}" object, toggle between keys and values.
after_value:
  if (g_depth == 0) {
    return g_eod;
  }
  switch (g_ctx) {
    case context::in_list_after_bracket:
      g_ctx = context::in_list_after_value;
      break;
    case context::in_dict_after_brace:
      g_ctx = context::in_dict_after_key;
      break;
    case context::in_dict_after_key:
      g_ctx = context::in_dict_after_value;
      break;
    case context::in_dict_after_value:
      g_ctx = context::in_dict_after_key;
      break;
    default:
      break;
  }
  return nullptr;
}

const char*  //
main1(int argc, char** argv) {
  TRY(initialize_globals(argc, argv));

  bool start_of_token_chain = true;
  while (true) {
    wuffs_base__status status = g_dec.decode_tokens(
        &g_tok, &g_src,
        wuffs_base__make_slice_u8(g_work_buffer_array, WORK_BUFFER_ARRAY_SIZE));

    while (g_tok.meta.ri < g_tok.meta.wi) {
      wuffs_base__token t = g_tok.data.ptr[g_tok.meta.ri++];
      uint64_t token_length = t.length();
      if ((g_src.meta.ri - g_cursor_index) < token_length) {
        return "main: internal error: inconsistent g_src indexes";
      }
      g_cursor_index += token_length;

      // Handle filler tokens (e.g. whitespace, punctuation and comments).
      // These are skipped, unless -output-comments is enabled.
      if (t.value_base_category() == WUFFS_BASE__TOKEN__VBC__FILLER) {
        if (!g_flags.output_comments) {
          // No-op.
        } else if (t.value_base_detail() &
                   WUFFS_BASE__TOKEN__VBD__FILLER__COMMENT_ANY) {
          if (g_flags.compact_output) {
            TRY(write_dst(g_src.data.ptr + g_cursor_index - token_length,
                          token_length));
            if (!t.continued() &&
                (t.value_base_detail() &
                 WUFFS_BASE__TOKEN__VBD__FILLER__COMMENT_LINE)) {
              TRY(write_dst("\n", 1));
            }

          } else {
            if (start_of_token_chain) {
              if (g_is_after_comment) {
                TRY_INDENT;
              } else if (g_ctx != context::none) {
                if (g_ctx == context::in_dict_after_key) {
                  TRY(write_dst(":", 1));
                } else if ((g_ctx != context::in_list_after_bracket) &&
                           (g_ctx != context::in_dict_after_brace) &&
                           (g_ctx != context::end_of_data)) {
                  TRY(write_dst(",", 1));
                }
                TRY_INDENT;
              }
            }
            TRY(write_dst(g_src.data.ptr + g_cursor_index - token_length,
                          token_length));
            g_is_after_comment = true;
          }
          if (g_ctx == context::in_list_after_bracket) {
            g_ctx = context::in_list_after_value;
          } else if (g_ctx == context::in_dict_after_brace) {
            g_ctx = context::in_dict_after_value;
          }
          g_num_input_blank_lines = 0;

        } else {
          uint8_t* p = g_src.data.ptr + g_cursor_index - token_length;
          uint8_t* q = g_src.data.ptr + g_cursor_index;
          for (; p < q; p++) {
            if (*p == '\n') {
              g_num_input_blank_lines++;
            }
          }
        }

        start_of_token_chain = !t.continued();
        continue;
      }

      const char* z = handle_token(t, start_of_token_chain);
      g_is_after_comment = false;
      start_of_token_chain = !t.continued();
      if (z == nullptr) {
        continue;
      } else if (z != g_eod) {
        return z;
      } else if (g_flags.query_c_string && *g_flags.query_c_string) {
        // With a non-empty g_query, don't try to consume trailing filler or
        // confirm that we've processed all the tokens.
        return nullptr;
      }
      g_ctx = context::end_of_data;
    }

    if (status.repr == nullptr) {
      if (g_ctx != context::end_of_data) {
        return "main: internal error: unexpected end of token stream";
      }
      // Check that we've exhausted the input.
      if ((g_src.meta.ri == g_src.meta.wi) && !g_src.meta.closed) {
        TRY(read_src());
      }
      if ((g_src.meta.ri < g_src.meta.wi) || !g_src.meta.closed) {
        return "main: valid JSON followed by further (unexpected) data";
      }
      // All done.
      return nullptr;
    } else if (status.repr == wuffs_base__suspension__short_read) {
      if (g_cursor_index != g_src.meta.ri) {
        return "main: internal error: inconsistent g_src indexes";
      }
      TRY(read_src());
      g_cursor_index = g_src.meta.ri;
    } else if (status.repr == wuffs_base__suspension__short_write) {
      g_tok.compact();
    } else {
      return status.message();
    }
  }
}

int  //
compute_exit_code(const char* status_msg) {
  if (!status_msg) {
    return 0;
  }
  size_t n;
  if (status_msg == g_usage) {
    n = strlen(status_msg);
  } else {
    n = strnlen(status_msg, 2047);
    if (n >= 2047) {
      status_msg = "main: internal error: error message is too long";
      n = strlen(status_msg);
    }
  }
  const int stderr_fd = 2;
  ignore_return_value(write(stderr_fd, status_msg, n));
  ignore_return_value(write(stderr_fd, "\n", 1));
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
  // Look for an input filename (the first non-flag argument) in argv. If there
  // is one, open it (but do not read from it) before we self-impose a sandbox.
  //
  // Flags start with "-", unless it comes after a bare "--" arg.
  {
    bool dash_dash = false;
    for (int a = 1; a < argc; a++) {
      char* arg = argv[a];
      if ((arg[0] == '-') && !dash_dash) {
        dash_dash = (arg[1] == '-') && (arg[2] == '\x00');
        continue;
      }
      g_input_file_descriptor = open(arg, O_RDONLY);
      if (g_input_file_descriptor < 0) {
        fprintf(stderr, "%s: %s\n", arg, strerror(errno));
        return 1;
      }
      break;
    }
  }

#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);
  g_sandboxed = true;
#endif

  const char* z = main1(argc, argv);
  if (g_wrote_to_dst) {
    const char* z1 = g_is_after_comment ? nullptr : write_dst("\n", 1);
    const char* z2 = flush_dst();
    z = z ? z : (z1 ? z1 : z2);
  }
  int exit_code = compute_exit_code(z);

#if defined(WUFFS_EXAMPLE_USE_SECCOMP)
  // Call SYS_exit explicitly, instead of calling SYS_exit_group implicitly by
  // either calling _exit or returning from main. SECCOMP_MODE_STRICT allows
  // only SYS_exit.
  syscall(SYS_exit, exit_code);
#endif
  return exit_code;
}
