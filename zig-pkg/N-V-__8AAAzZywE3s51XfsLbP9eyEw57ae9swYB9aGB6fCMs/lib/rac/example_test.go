// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package rac_test

import (
	"bytes"
	"compress/zlib"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"hash/adler32"
	"hash/crc32"
	"io"
	"log"
	"os"

	"github.com/google/wuffs/lib/rac"
)

// Example_indexLocationAtEnd demonstrates using the low level "rac" package to
// encode a RAC+Zlib formatted file with IndexLocationAtEnd.
//
// The sibling "raczlib" package provides a higher level API that is easier to
// use.
//
// See the RAC specification for an explanation of the file format.
func Example_indexLocationAtEnd() {
	// Manually construct a zlib encoding of "More!\n", one that uses a literal
	// block (that's easy to see in a hex dump) instead of a more compressible
	// Huffman block.
	const src = "More!\n"
	hasher := adler32.New()
	hasher.Write([]byte(src))
	enc := []byte{ // See RFCs 1950 and 1951 for details.
		0x78,       // Deflate compression method; 32KiB window size.
		0x9C,       // Default encoding algorithm; FCHECK bits.
		0x01,       // Literal block (final).
		0x06, 0x00, // Literal length.
		0xF9, 0xFF, // Inverse of the literal length.
	}
	enc = append(enc, src...) // Literal bytes.
	enc = hasher.Sum(enc)     // Adler-32 hash.

	// Check that we've constructed a valid zlib-formatted encoding, by
	// checking that decoding enc produces src.
	{
		b := &bytes.Buffer{}
		r, err := zlib.NewReader(bytes.NewReader(enc))
		if err != nil {
			log.Fatalf("NewReader: %v", err)
		}
		if _, err := io.Copy(b, r); err != nil {
			log.Fatalf("Copy: %v", err)
		}
		if got := b.String(); got != src {
			log.Fatalf("zlib check: got %q, want %q", got, src)
		}
	}

	buf := &bytes.Buffer{}
	w := &rac.ChunkWriter{
		Writer: buf,
	}
	if err := w.AddChunk(uint64(len(src)), rac.CodecZlib, enc, 0, 0); err != nil {
		log.Fatalf("AddChunk: %v", err)
	}
	if err := w.Close(); err != nil {
		log.Fatalf("Close: %v", err)
	}

	fmt.Printf("RAC file:\n%s", hex.Dump(buf.Bytes()))

	// Output:
	// RAC file:
	// 00000000  72 c3 63 00 78 9c 01 06  00 f9 ff 4d 6f 72 65 21  |r.c.x......More!|
	// 00000010  0a 07 42 01 bf 72 c3 63  01 65 a9 00 ff 06 00 00  |..B..r.c.e......|
	// 00000020  00 00 00 00 01 04 00 00  00 00 00 01 ff 35 00 00  |.............5..|
	// 00000030  00 00 00 01 01                                    |.....|
}

// Example_indexLocationAtStart demonstrates using the low level "rac" package
// to encode and then decode a RAC+Zlib formatted file with
// IndexLocationAtStart.
//
// The sibling "raczlib" package provides a higher level API that is easier to
// use.
//
// See the RAC specification for an explanation of the file format.
func Example_indexLocationAtStart() {
	buf := &bytes.Buffer{}
	w := &rac.ChunkWriter{
		Writer:        buf,
		IndexLocation: rac.IndexLocationAtStart,
		TempFile:      &bytes.Buffer{},
	}

	dict := []byte(" sheep.\n")
	if len(dict) >= (1 << 30) {
		log.Fatal("len(dict) is too large")
	}
	encodedDict := []byte{
		uint8(len(dict) >> 0),
		uint8(len(dict) >> 8),
		uint8(len(dict) >> 16),
		uint8(len(dict) >> 24),
	}
	encodedDict = append(encodedDict, dict...)
	checksum := crc32.ChecksumIEEE(dict)
	encodedDict = append(encodedDict,
		uint8(checksum>>0),
		uint8(checksum>>8),
		uint8(checksum>>16),
		uint8(checksum>>24),
	)
	fmt.Printf("Encoded dictionary resource:\n%s\n", hex.Dump(encodedDict))

	dictResource, err := w.AddResource(encodedDict)
	if err != nil {
		log.Fatalf("AddResource: %v", err)
	}

	chunks := []string{
		"One sheep.\n",
		"Two sheep.\n",
		"Three sheep.\n",
	}

	for i, chunk := range chunks {
		b := &bytes.Buffer{}
		if z, err := zlib.NewWriterLevelDict(b, zlib.BestCompression, dict); err != nil {
			log.Fatalf("NewWriterLevelDict: %v", err)
		} else if _, err := z.Write([]byte(chunk)); err != nil {
			log.Fatalf("Write: %v", err)
		} else if err := z.Close(); err != nil {
			log.Fatalf("Close: %v", err)
		}
		encodedChunk := b.Bytes()

		if err := w.AddChunk(uint64(len(chunk)), rac.CodecZlib, encodedChunk, dictResource, 0); err != nil {
			log.Fatalf("AddChunk: %v", err)
		}

		fmt.Printf("Encoded chunk #%d:\n%s\n", i, hex.Dump(encodedChunk))
	}

	if err := w.Close(); err != nil {
		log.Fatalf("Close: %v", err)
	}

	encoded := buf.Bytes()
	fmt.Printf("RAC file:\n%s\n", hex.Dump(encoded))

	// Decode the encoded bytes (the RAC-formatted bytes) to recover the
	// original "One sheep.\nTwo sheep\.Three sheep.\n" source.

	fmt.Printf("Decoded:\n")
	r := &rac.ChunkReader{
		ReadSeeker:     bytes.NewReader(encoded),
		CompressedSize: int64(len(encoded)),
	}
	zr := io.ReadCloser(nil)
	for {
		chunk, err := r.NextChunk()
		if err == io.EOF {
			break
		} else if err != nil {
			log.Fatalf("NextChunk: %v", err)
		}
		if chunk.Codec != rac.CodecZlib {
			log.Fatalf("unexpected chunk codec")
		}
		fmt.Printf("[%2d, %2d): ", chunk.DRange[0], chunk.DRange[1])

		// Parse the RAC+Zlib secondary data. For details, see
		// https://github.com/google/wuffs/blob/main/doc/spec/rac-spec.md#rac--zlib
		dict := []byte(nil)
		if secondary := encoded[chunk.CSecondary[0]:chunk.CSecondary[1]]; len(secondary) > 0 {
			if len(secondary) < 8 {
				log.Fatalf("invalid secondary data")
			}
			dictLen := int(binary.LittleEndian.Uint32(secondary))
			secondary = secondary[4:]
			if (dictLen >= (1 << 30)) || ((dictLen + 4) > len(secondary)) {
				log.Fatalf("invalid secondary data")
			}
			checksum := binary.LittleEndian.Uint32(secondary[dictLen:])
			dict = secondary[:dictLen]
			if checksum != crc32.ChecksumIEEE(dict) {
				log.Fatalf("invalid checksum")
			}
		}

		// Decompress the Zlib-encoded primary payload.
		primary := encoded[chunk.CPrimary[0]:chunk.CPrimary[1]]
		if zr == nil {
			if zr, err = zlib.NewReaderDict(bytes.NewReader(primary), dict); err != nil {
				log.Fatalf("zlib.NewReader: %v", err)
			}
		} else if err := zr.(zlib.Resetter).Reset(bytes.NewReader(primary), dict); err != nil {
			log.Fatalf("zlib.Reader.Reset: %v", err)
		}
		if n, err := io.Copy(os.Stdout, zr); err != nil {
			log.Fatalf("io.Copy: %v", err)
		} else if n != chunk.DRange.Size() {
			log.Fatalf("inconsistent DRange size")
		}
		if err := zr.Close(); err != nil {
			log.Fatalf("zlib.Reader.Close: %v", err)
		}
	}

	// Note that these exact bytes depends on the zlib encoder's algorithm, but
	// there is more than one valid zlib encoding of any given input. This
	// "compare to golden output" test is admittedly brittle, as the standard
	// library's zlib package's output isn't necessarily stable across Go
	// releases.

	// Output:
	// Encoded dictionary resource:
	// 00000000  08 00 00 00 20 73 68 65  65 70 2e 0a d0 8d 7a 47  |.... sheep....zG|
	//
	// Encoded chunk #0:
	// 00000000  78 f9 0b e0 02 6e f2 cf  4b 85 31 01 01 00 00 ff  |x....n..K.1.....|
	// 00000010  ff 17 21 03 90                                    |..!..|
	//
	// Encoded chunk #1:
	// 00000000  78 f9 0b e0 02 6e 0a 29  cf 87 31 01 01 00 00 ff  |x....n.)..1.....|
	// 00000010  ff 18 0c 03 a8                                    |.....|
	//
	// Encoded chunk #2:
	// 00000000  78 f9 0b e0 02 6e 0a c9  28 4a 4d 85 71 00 01 00  |x....n..(JM.q...|
	// 00000010  00 ff ff 21 6e 04 66                              |...!n.f|
	//
	// RAC file:
	// 00000000  72 c3 63 04 37 39 00 ff  00 00 00 00 00 00 00 ff  |r.c.79..........|
	// 00000010  0b 00 00 00 00 00 00 ff  16 00 00 00 00 00 00 ff  |................|
	// 00000020  23 00 00 00 00 00 00 01  50 00 00 00 00 00 01 ff  |#.......P.......|
	// 00000030  60 00 00 00 00 00 01 00  75 00 00 00 00 00 01 00  |`.......u.......|
	// 00000040  8a 00 00 00 00 00 01 00  a1 00 00 00 00 00 01 04  |................|
	// 00000050  08 00 00 00 20 73 68 65  65 70 2e 0a d0 8d 7a 47  |.... sheep....zG|
	// 00000060  78 f9 0b e0 02 6e f2 cf  4b 85 31 01 01 00 00 ff  |x....n..K.1.....|
	// 00000070  ff 17 21 03 90 78 f9 0b  e0 02 6e 0a 29 cf 87 31  |..!..x....n.)..1|
	// 00000080  01 01 00 00 ff ff 18 0c  03 a8 78 f9 0b e0 02 6e  |..........x....n|
	// 00000090  0a c9 28 4a 4d 85 71 00  01 00 00 ff ff 21 6e 04  |..(JM.q......!n.|
	// 000000a0  66                                                |f|
	//
	// Decoded:
	// [ 0, 11): One sheep.
	// [11, 22): Two sheep.
	// [22, 35): Three sheep.
}
