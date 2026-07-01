// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package zlibcut_test

import (
	"bytes"
	"compress/zlib"
	"fmt"
	"io"
	"log"
	"strings"

	"github.com/google/wuffs/lib/zlibcut"
)

func ExampleCut() {
	const sonnet18 = "" +
		"Shall I compare thee to a summer’s day?\n" +
		"Thou art more lovely and more temperate.\n" +
		"Rough winds do shake the darling buds of May,\n" +
		"And summer’s lease hath all too short a date.\n" +
		"Sometime too hot the eye of heaven shines,\n" +
		"And often is his gold complexion dimmed;\n" +
		"And every fair from fair sometime declines,\n" +
		"By chance, or nature’s changing course, untrimmed;\n" +
		"But thy eternal summer shall not fade,\n" +
		"Nor lose possession of that fair thou ow’st,\n" +
		"Nor shall death brag thou wand'rest in his shade,\n" +
		"When in eternal lines to Time thou grow'st.\n" +
		"So long as men can breathe, or eyes can see,\n" +
		"So long lives this, and this gives life to thee.\n"

	if n := len(sonnet18); n != 632 {
		log.Fatalf("len(sonnet18): got %d, want 632", n)
	}

	// Compress the input text, sonnet18.
	buffer := &bytes.Buffer{}
	w := zlib.NewWriter(buffer)
	w.Write([]byte(sonnet18))
	w.Close()
	compressed := buffer.Bytes()

	// The exact length of the zlib-compressed form of sonnet18 depends on the
	// compression algorithm used, which can change from version to version of
	// the Go standard library. Nonetheless, for a 632 byte input, we expect
	// the compressed form to be between 300 and 500 bytes.
	if n := len(compressed); (n < 300) || (500 < n) {
		log.Fatalf("len(compressed): got %d, want something in [300, 500]", n)
	}

	// Cut the 300-or-more bytes to be 200.
	encodedLen, decodedLen, err := zlibcut.Cut(nil, compressed, 200)
	if err != nil {
		log.Fatalf("Cut: %v", err)
	}

	// The encodedLen should be equal to or just under the requested 200.
	cut := compressed[:encodedLen]
	if n := len(cut); (n < 190) || (200 < n) {
		log.Fatalf("len(cut): got %d, want something in [190, 200]", n)
	}

	// At this point, a real program would write that cut slice somewhere. The
	// rest of this example verifies that the cut data has the properties we
	// expect, given the semantics of zlibcut.Cut.

	// Uncompress the cut data. It should be a valid zlib-compressed stream, so
	// no errors should be encountered.
	r, err := zlib.NewReader(bytes.NewReader(cut))
	if err != nil {
		log.Fatalf("NewReader: %v", err)
	}
	uncompressed, err := io.ReadAll(r)
	if err != nil {
		log.Fatalf("ReadAll: %v", err)
	}
	err = r.Close()
	if err != nil {
		log.Fatalf("Close: %v", err)
	}

	// The uncompressed form of the cut data should be a prefix (of length
	// decodedLen) of the original input, sonnet18. Again, the exact length
	// depends on the zlib compression algorithm, but uncompressing 200 or so
	// bytes should give between 250 and 400 bytes.
	if n := len(uncompressed); n != decodedLen {
		log.Fatalf("len(uncompressed): got %d, want %d", n, decodedLen)
	} else if (n < 250) || (400 < n) {
		log.Fatalf("len(uncompressed): got %d, want something in [250, 400]", n)
	} else if !strings.HasPrefix(sonnet18, string(uncompressed)) {
		log.Fatalf("uncompressed was not a prefix of the original input")
	}

	// The first two lines of the sonnet take 83 bytes.
	fmt.Println(string(uncompressed[:83]))
	// Output:
	// Shall I compare thee to a summer’s day?
	// Thou art more lovely and more temperate.
}
