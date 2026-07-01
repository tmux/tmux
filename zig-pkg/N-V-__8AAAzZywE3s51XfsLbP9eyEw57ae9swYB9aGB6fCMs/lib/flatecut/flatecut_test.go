// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package flatecut

import (
	"bytes"
	"compress/flate"
	"io"
	"testing"

	"github.com/google/wuffs/internal/testcut"
)

func TestCut(tt *testing.T) {
	testcut.Test(tt, SmallestValidMaxEncodedLen, Cut, newReader, []string{
		"artificial-deflate/backref-crosses-blocks.deflate",
		"artificial-deflate/distance-32768.deflate",
		"romeo.txt.deflate",
		"romeo.txt.fixed-huff.deflate",
	})
}

func BenchmarkCut(b *testing.B) {
	testcut.Benchmark(b, SmallestValidMaxEncodedLen, Cut, newReader,
		"pi.txt.zlib", 2, 4, 100003)
}

func newReader(r io.Reader) (io.ReadCloser, error) {
	return flate.NewReader(r), nil
}

func decodeFlate(src []byte) string {
	dst, err := io.ReadAll(flate.NewReader(bytes.NewReader(src)))
	if err != nil {
		return err.Error()
	}
	return string(dst)
}

func TestHuffmanDecode(tt *testing.T) {
	// This exercises the "ABCDEFGH" example from RFC 1951 section 3.2.2,
	// discussed in the "type huffman" doc comment.

	const src = "HEADFACE"
	codes := []string{
		'A': " 010",
		'B': " 011",
		'C': " 100",
		'D': " 101",
		'E': " 110",
		'F': "  00",
		'G': "1110",
		'H': "1111",
	}

	encoded := []byte(nil)
	encBits := uint32(0)
	encNBits := uint32(0)
	for _, s := range src {
		for _, c := range codes[s] {
			if c == ' ' {
				continue
			}

			if c == '1' {
				encBits |= 1 << encNBits
			}
			encNBits++

			if encNBits == 8 {
				encoded = append(encoded, uint8(encBits))
				encBits = 0
				encNBits = 0
			}
		}
	}
	if encNBits > 0 {
		encoded = append(encoded, uint8(encBits))
	}

	h := huffman{
		counts: [maxCodeBits + 1]uint32{
			2: 1, 3: 5, 4: 2,
		},
		symbols: [maxNumCodes]int32{
			0: 'F', 1: 'A', 2: 'B', 3: 'C', 4: 'D', 5: 'E', 6: 'G', 7: 'H',
		},
	}
	h.constructLookUpTable()

	b := &bitstream{
		bytes: encoded,
	}

	decoded := []byte(nil)
	for _ = range src {
		c := h.decode(b)
		if c < 0 {
			decoded = append(decoded, '!')
			break
		}
		decoded = append(decoded, uint8(c))
	}

	if got, want := string(decoded), src; got != want {
		tt.Fatalf("got %q, want %q", got, want)
	}
}

// A DEFLATE encoding of the first 64 bytes of the AUTHORS file in the
// repository root directory.
//
// The encoding uses a Dynamic Huffman block, but one whose H-D Huffman
// distance tree is degenerate (there's a mapping for the "0" code but no
// mapping for the "1" code) and unused.
//
// The first 25-30 bytes contain the encoded H-L and H-D Huffman trees. The
// last 30-35 bytes contain the actual payload, encoded with H-L.
//
// The high 7 bits of the final byte is unused padding.
const (
	degenerateHuffmanDec = "# This is the official list of Wuffs authors for copyright purpo"
	degenerateHuffmanEnc = "" +
		"\x05\xc0\xc1\x09\xc5\x30\x0c\x03\xd0\x55\x04\x7f\xa5\x0f\x3d\x87" +
		"\x50\xd5\x82\x80\x82\xed\x1c\xba\x7d\xdf\x0f\xff\x50\x41\x85\x8e" +
		"\x1b\x26\x35\x35\x16\x96\xaa\x61\xe2\x3a\x64\x61\x9c\x0e\x67\x81" +
		"\x4e\x4c\xef\x37\xf5\x44\x63\x9f\xdc\xfe\x00"
)

func TestReplaceHuffmanWithStored(tt *testing.T) {
	const dec = degenerateHuffmanDec
	const enc = degenerateHuffmanEnc
	if (len(dec) != 64) || (len(enc) != 59) {
		panic("inconsistent const string lengths")
	}
	if got, want := decodeFlate([]byte(enc)), dec; got != want {
		tt.Fatalf("before Cut: got %q, want %q", got, want)
	}

	for i := 4; i <= 59; i += 5 {
		b := []byte(enc)
		encLen, decLen, err := Cut(nil, b, i)
		if err != nil {
			tt.Errorf("i=%d: %v", i, err)
			continue
		}
		if encLen < 1 {
			tt.Errorf("i=%d: encLen: got %d, want >= 1", i, encLen)
			continue
		} else if encLen > len(enc) {
			tt.Errorf("i=%d: encLen: got %d, want <= %d", i, encLen, len(enc))
			continue
		}
		// If we can make some progress (decLen > 0), even if the input uses a
		// Huffman block, one option is to re-encode to a single Stored block
		// (for 5 bytes of overhead). It's single because len(dec) < 0xFFFF.
		// Regardless of whether the cut form uses a Huffman or Stored block,
		// we should be able to produce at least i-5 bytes of decoded output.
		if (decLen > 0) && (i > 5) && (decLen < i-5) {
			tt.Errorf("i=%d: decLen: got %d, want >= %d", i, decLen, i-5)
			continue
		} else if decLen > len(dec) {
			tt.Errorf("i=%d: decLen: got %d, want <= %d", i, decLen, len(dec))
			continue
		}

		if got, want := decodeFlate(b[:encLen]), dec[:decLen]; got != want {
			tt.Errorf("i=%d: after Cut: got %q, want %q", i, got, want)
			continue
		}

		// Check that we are using a space-efficient block type.
		{
			got := (b[0] >> 1) & 3
			want := uint8(0xFF)

			switch i {
			case 4:
				want = 1 // Static Huffman, for a decLen of 0.
			case 9:
				want = 0 // Stored.
			case 59:
				want = 2 // Dynamic Huffman.
			default:
				continue
			}

			if got != want {
				tt.Errorf("i=%d: block type: got %d, want %d", i, got, want)
			}
		}
	}
}

func TestDegenerateHuffmanUnused(tt *testing.T) {
	const dec = degenerateHuffmanDec
	const enc = degenerateHuffmanEnc

	// Cutting 1 byte off the end of the encoded form will lead to cutting n
	// bytes off the decoded form. Coincidentally, n equals 1, even though each
	// decoded byte (8 bits) is packed into smaller number of bits, as most of
	// the final encoded byte's bits are unused padding.
	const n = 1

	b := []byte(enc)
	encLen, decLen, err := Cut(nil, b, len(enc)-1)
	if err != nil {
		tt.Fatalf("Cut: %v", err)
	} else if encLen != len(enc)-1 {
		tt.Fatalf("encLen: got %d, want %d", encLen, len(enc)-1)
	} else if decLen != len(dec)-n {
		tt.Fatalf("decLen: got %d, want %d", decLen, len(dec)-n)
	}

	if got, want := decodeFlate(b[:encLen]), dec[:decLen]; got != want {
		tt.Fatalf("after Cut: got %q, want %q", got, want)
	}
}
