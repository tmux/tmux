// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package raczlib_test

import (
	"bytes"
	"fmt"
	"io"
	"log"

	"github.com/google/wuffs/lib/rac"
	"github.com/google/wuffs/lib/raczlib"
)

// Example_roundTrip demonstrates compressing (using a rac.Writer and a
// raczlib.CodecWriter) and decompressing (using a rac.Reader and a
// raczlib.CodecReader). This includes decompressing an excerpt of the original
// data, exercising the "random access" part of RAC.
func Example_roundTrip() {
	// Create some test data.
	oBuf := &bytes.Buffer{}
	for i := 99; i > 0; i-- {
		fmt.Fprintf(oBuf, "%d bottles of beer on the wall, %d bottles of beer.\n"+
			"Take one down, pass it around, %d bottles of beer on the wall.\n",
			i, i, i-1)
	}
	original := oBuf.Bytes()

	// Create the RAC file.
	cBuf := &bytes.Buffer{}
	w := &rac.Writer{
		Writer:      cBuf,
		CodecWriter: &raczlib.CodecWriter{},
		// It's not necessary to explicitly declare the DChunkSize. The zero
		// value implies a reasonable default. Nonetheless, using a 1 KiB
		// DChunkSize (which is relatively small) makes for a more interesting
		// test, as the resultant RAC file then contains more than one chunk.
		DChunkSize: 1024,
		// We also use the default IndexLocation value, which makes for a
		// simpler example, but if you're copy/pasting this code, note that
		// using an explicit IndexLocationAtStart can result in slightly more
		// efficient RAC files, at the cost of using more memory to encode.
	}
	if _, err := w.Write(original); err != nil {
		log.Fatalf("Write: %v", err)
	}
	if err := w.Close(); err != nil {
		log.Fatalf("Close: %v", err)
	}
	compressed := cBuf.Bytes()

	// The exact compression ratio depends on the zlib encoder's algorithm,
	// which can change across Go standard library releases, but it should be
	// at least a 4x ratio. It'd be larger if we didn't specify an explicit
	// (but relatively small) DChunkSize.
	if ratio := len(original) / len(compressed); ratio < 4 {
		log.Fatalf("compression ratio (%dx) was too small", ratio)
	}

	// Prepare to decompress.
	r := &rac.Reader{
		ReadSeeker:     bytes.NewReader(compressed),
		CompressedSize: int64(len(compressed)),
		CodecReaders:   []rac.CodecReader{&raczlib.CodecReader{}},
	}
	defer r.Close()

	// Read the whole file.
	wBuf := &bytes.Buffer{}
	if _, err := io.Copy(wBuf, r); err != nil {
		log.Fatal(err)
	}
	wholeFile := wBuf.Bytes()
	if !bytes.Equal(wholeFile, original) {
		log.Fatal("round trip did not preserve whole file")
	} else {
		fmt.Printf("Whole file preserved (%d bytes).\n", len(wholeFile))
	}

	// Read an excerpt.
	const offset, length = 3000, 1200
	want := original[offset : offset+length]
	got := make([]byte, length)
	if _, err := r.Seek(offset, io.SeekStart); err != nil {
		log.Fatalf("Seek: %v", err)
	}
	if _, err := io.ReadFull(r, got); err != nil {
		log.Fatalf("ReadFull: %v", err)
	}
	if !bytes.Equal(got, want) {
		log.Fatal("round trip did not preserve excerpt")
	} else {
		fmt.Printf("Excerpt    preserved  (%d bytes).\n", len(got))
	}

	// Output:
	// Whole file preserved (11357 bytes).
	// Excerpt    preserved  (1200 bytes).
}
