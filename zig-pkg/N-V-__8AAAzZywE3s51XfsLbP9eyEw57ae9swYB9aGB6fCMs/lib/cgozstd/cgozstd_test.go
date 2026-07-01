// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package cgozstd

import (
	"bytes"
	"io"
	"strings"
	"testing"
)

const (
	// compressedMoreN is 15 bytes of zstd frame:
	//
	// There are two forms, depending on the zstd-the-library version used for
	// compression. They differ only in the WD byte.
	//
	// \x28\xb5\x2f\xfd \x00 \x50 \x31\x00\x00 \x4d\x6f\x72\x65\x21\x0a
	// \x28\xb5\x2f\xfd \x00 \x58 \x31\x00\x00 \x4d\x6f\x72\x65\x21\x0a
	// Magic----------- FHD- WD-- BH---------- BlockData---------------
	//
	// Frame Header Descriptor: no flags set.
	// Window Descriptor: Exponent=10, Mantissa=0, Window_Size=1MiB.
	//                or: Exponent=11, Mantissa=0, Window_Size=2MiB.
	// Block Header: Last_Block=1, Block_Type=0 (Raw_Block), Block_Size=6.
	// Block Data: the literal bytes "More!\n".
	compressedMore0 = "\x28\xb5\x2f\xfd\x00\x50\x31\x00\x00\x4d\x6f\x72\x65\x21\x0a"
	compressedMore1 = "\x28\xb5\x2f\xfd\x00\x58\x31\x00\x00\x4d\x6f\x72\x65\x21\x0a"

	uncompressedMore = "More!\n"
)

func TestRoundTrip(tt *testing.T) {
	if !cgoEnabled {
		tt.Skip("cgo is not enabled")
	}

	wr := &WriterRecycler{}
	defer wr.Close()
	w := &Writer{}
	wr.Bind(w)

	rr := &ReaderRecycler{}
	defer rr.Close()
	r := &Reader{}
	rr.Bind(r)

	for i := 0; i < 3; i++ {
		buf := &bytes.Buffer{}

		// Compress.
		{
			if err := w.Reset(buf, nil, 0); err != nil {
				w.Close()
				tt.Fatalf("i=%d: Reset: %v", i, err)
			}
			if _, err := w.Write([]byte(uncompressedMore)); err != nil {
				w.Close()
				tt.Fatalf("i=%d: Write: %v", i, err)
			}
			if err := w.Close(); err != nil {
				tt.Fatalf("i=%d: Close: %v", i, err)
			}
		}

		compressed := buf.String()
		if (compressed != compressedMore0) && (compressed != compressedMore1) {
			tt.Fatalf("i=%d: compressed\ngot  % 02x\nwant % 02x\nor   % 02x",
				i, compressed, compressedMore0, compressedMore1)
		}

		// Uncompress.
		{
			if err := r.Reset(strings.NewReader(compressed), nil); err != nil {
				r.Close()
				tt.Fatalf("i=%d: Reset: %v", i, err)
			}
			gotBytes, err := io.ReadAll(r)
			if err != nil {
				r.Close()
				tt.Fatalf("i=%d: ReadAll: %v", i, err)
			}
			if got, want := string(gotBytes), uncompressedMore; got != want {
				r.Close()
				tt.Fatalf("i=%d:\ngot  %q\nwant %q", i, got, want)
			}
			if err := r.Close(); err != nil {
				tt.Fatalf("i=%d: Close: %v", i, err)
			}
		}
	}
}

func TestDictionary(tt *testing.T) {
	if !cgoEnabled {
		tt.Skip("cgo is not enabled")
	}

	const (
		abc          = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
		uncompressed = abc + "123"
	)

	for _, withDict := range []bool{false, true} {
		buf := &bytes.Buffer{}
		dictionary, name := []byte(nil), "sans dictionary"
		if withDict {
			dictionary, name = []byte(abc), "with dictionary"
		}

		w := &Writer{}
		if err := w.Reset(buf, dictionary, 0); err != nil {
			w.Close()
			tt.Fatalf("%s: Reset: %v", name, err)
		}
		if _, err := w.Write([]byte(uncompressed)); err != nil {
			w.Close()
			tt.Fatalf("%s: Write: %v", name, err)
		}
		if err := w.Close(); err != nil {
			tt.Fatalf("%s: Close: %v", name, err)
		}

		compressed := buf.String()
		if withDict {
			if n := buf.Len(); n >= 30 {
				tt.Fatalf("%s: compressed length: got %d, want < 30", name, n)
			}
		} else {
			if n := buf.Len(); n < 50 {
				tt.Fatalf("%s: compressed length: got %d, want >= 50", name, n)
			}
		}

		r := &Reader{}
		if err := r.Reset(strings.NewReader(compressed), dictionary); err != nil {
			r.Close()
			tt.Fatalf("%s: Reset: %v", name, err)
		}
		gotBytes, err := io.ReadAll(r)
		if err != nil {
			r.Close()
			tt.Fatalf("%s: ReadAll: %v", name, err)
		}
		if got, want := string(gotBytes), uncompressed; got != want {
			r.Close()
			tt.Fatalf("%s:\ngot  %q\nwant %q", name, got, want)
		}
		if err := r.Close(); err != nil {
			tt.Fatalf("%s: Close: %v", name, err)
		}
	}
}
