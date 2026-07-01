// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package rac

import (
	"bytes"
	"encoding/hex"
	"fmt"
	"hash/crc32"
	"io"
	"math/rand"
	"os"
	"strings"
	"testing"
)

const bytesPerHexDumpLine = 79

// fakeCodec is the codec used by this file's low level tests. The compressed
// data is not actually in the Zlib format, but this Codec's value is
// CodecZlib, since we need a valid Codec value, and CodecZlib is as good a
// value as any other.
const fakeCodec = CodecZlib

var unhex = [256]uint8{
	'0': 0x00, '1': 0x01, '2': 0x02, '3': 0x03, '4': 0x04,
	'5': 0x05, '6': 0x06, '7': 0x07, '8': 0x08, '9': 0x09,
	'A': 0x0A, 'B': 0x0B, 'C': 0x0C, 'D': 0x0D, 'E': 0x0E, 'F': 0x0F,
	'a': 0x0A, 'b': 0x0B, 'c': 0x0C, 'd': 0x0D, 'e': 0x0E, 'f': 0x0F,
}

func undoHexDump(s string) (ret []byte) {
	for s != "" {
		for i := 0; i < 16; i++ {
			pos := 10 + 3*i
			if i > 7 {
				pos++
			}
			c0 := s[pos+0]
			c1 := s[pos+1]
			if c0 == ' ' {
				break
			}
			ret = append(ret, (unhex[c0]<<4)|unhex[c1])
		}
		if n := strings.IndexByte(s, '\n'); n >= 0 {
			s = s[n+1:]
		} else {
			break
		}
	}
	return ret
}

const writerWantEmpty = "" +
	"00000000  72 c3 63 01 0d f8 00 ff  00 00 00 00 00 00 00 00  |r.c.............|\n" +
	"00000010  20 00 00 00 00 00 01 ff  20 00 00 00 00 00 01 01  | ....... .......|\n"

const writerWantILAEnd = "" +
	"00000000  72 c3 63 00 52 72 72 53  73 41 61 61 42 62 62 62  |r.c.RrrSsAaaBbbb|\n" +
	"00000010  43 63 63 63 63 63 63 63  63 63 31 32 72 c3 63 05  |Cccccccccc12r.c.|\n" +
	"00000020  2d 9a 00 ff 00 00 00 00  00 00 00 ff 00 00 00 00  |-...............|\n" +
	"00000030  00 00 00 ff 11 00 00 00  00 00 00 ff 33 00 00 00  |............3...|\n" +
	"00000040  00 00 00 01 77 00 00 00  00 00 00 01 04 00 00 00  |....w...........|\n" +
	"00000050  00 00 01 ff 07 00 00 00  00 00 01 ff 09 00 00 00  |................|\n" +
	"00000060  00 00 01 ff 0c 00 00 00  00 00 01 00 10 00 00 00  |................|\n" +
	"00000070  00 00 01 00 7c 00 00 00  00 00 01 05              |....|.......|\n"

const writerWantILAEndCPageSize8 = "" +
	"00000000  72 c3 63 00 52 72 72 00  53 73 41 61 61 00 00 00  |r.c.Rrr.SsAaa...|\n" +
	"00000010  42 62 62 62 43 63 63 63  63 63 63 63 63 63 31 32  |BbbbCccccccccc12|\n" +
	"00000020  72 c3 63 05 a0 81 00 ff  00 00 00 00 00 00 00 ff  |r.c.............|\n" +
	"00000030  00 00 00 00 00 00 00 ff  11 00 00 00 00 00 00 ff  |................|\n" +
	"00000040  33 00 00 00 00 00 00 01  77 00 00 00 00 00 00 01  |3.......w.......|\n" +
	"00000050  04 00 00 00 00 00 01 ff  08 00 00 00 00 00 01 ff  |................|\n" +
	"00000060  0a 00 00 00 00 00 01 ff  10 00 00 00 00 00 01 00  |................|\n" +
	"00000070  14 00 00 00 00 00 01 00  80 00 00 00 00 00 01 05  |................|\n"

const writerWantILAStart = "" +
	"00000000  72 c3 63 05 8c 03 00 ff  00 00 00 00 00 00 00 ff  |r.c.............|\n" +
	"00000010  00 00 00 00 00 00 00 ff  11 00 00 00 00 00 00 ff  |................|\n" +
	"00000020  33 00 00 00 00 00 00 01  77 00 00 00 00 00 00 01  |3.......w.......|\n" +
	"00000030  60 00 00 00 00 00 01 ff  63 00 00 00 00 00 01 ff  |`.......c.......|\n" +
	"00000040  65 00 00 00 00 00 01 ff  68 00 00 00 00 00 01 00  |e.......h.......|\n" +
	"00000050  6c 00 00 00 00 00 01 00  78 00 00 00 00 00 01 05  |l.......x.......|\n" +
	"00000060  52 72 72 53 73 41 61 61  42 62 62 62 43 63 63 63  |RrrSsAaaBbbbCccc|\n" +
	"00000070  63 63 63 63 63 63 31 32                           |cccccc12|\n"

const writerWantILAStartCPageSize4 = "" +
	"00000000  72 c3 63 05 cc 93 00 ff  00 00 00 00 00 00 00 ff  |r.c.............|\n" +
	"00000010  00 00 00 00 00 00 00 ff  11 00 00 00 00 00 00 ff  |................|\n" +
	"00000020  33 00 00 00 00 00 00 01  77 00 00 00 00 00 00 01  |3.......w.......|\n" +
	"00000030  60 00 00 00 00 00 01 ff  64 00 00 00 00 00 01 ff  |`.......d.......|\n" +
	"00000040  68 00 00 00 00 00 01 ff  6c 00 00 00 00 00 01 00  |h.......l.......|\n" +
	"00000050  70 00 00 00 00 00 01 00  7c 00 00 00 00 00 01 05  |p.......|.......|\n" +
	"00000060  52 72 72 00 53 73 00 00  41 61 61 00 42 62 62 62  |Rrr.Ss..Aaa.Bbbb|\n" +
	"00000070  43 63 63 63 63 63 63 63  63 63 31 32              |Cccccccccc12|\n"

const writerWantILAStartCPageSize128 = "" +
	"00000000  72 c3 63 05 e8 00 00 ff  00 00 00 00 00 00 00 ff  |r.c.............|\n" +
	"00000010  00 00 00 00 00 00 00 ff  11 00 00 00 00 00 00 ff  |................|\n" +
	"00000020  33 00 00 00 00 00 00 01  77 00 00 00 00 00 00 01  |3.......w.......|\n" +
	"00000030  80 00 00 00 00 00 01 ff  83 00 00 00 00 00 01 ff  |................|\n" +
	"00000040  85 00 00 00 00 00 01 ff  88 00 00 00 00 00 01 00  |................|\n" +
	"00000050  8c 00 00 00 00 00 01 00  98 00 00 00 00 00 01 05  |................|\n" +
	"00000060  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|\n" +
	"00000070  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|\n" +
	"00000080  52 72 72 53 73 41 61 61  42 62 62 62 43 63 63 63  |RrrSsAaaBbbbCccc|\n" +
	"00000090  63 63 63 63 63 63 31 32                           |cccccc12|\n"

func TestWriterILAEndEmpty(tt *testing.T) {
	if err := testWriter(IndexLocationAtEnd, nil, 0, true); err != nil {
		tt.Fatal(err)
	}
}

func TestWriterILAStartEmpty(tt *testing.T) {
	tempFile := &bytes.Buffer{}
	if err := testWriter(IndexLocationAtStart, tempFile, 0, true); err != nil {
		tt.Fatal(err)
	}
}

func TestWriterILAEndNoTempFile(tt *testing.T) {
	if err := testWriter(IndexLocationAtEnd, nil, 0, false); err != nil {
		tt.Fatal(err)
	}
}

func TestWriterILAEndMemTempFile(tt *testing.T) {
	tempFile := &bytes.Buffer{}
	if err := testWriter(IndexLocationAtEnd, tempFile, 0, false); err == nil {
		tt.Fatal("err: got nil, want non-nil")
	} else if !strings.HasPrefix(err.Error(), "rac: IndexLocationAtEnd requires") {
		tt.Fatal(err)
	}
}

func TestWriterILAStartNoTempFile(tt *testing.T) {
	if err := testWriter(IndexLocationAtStart, nil, 0, false); err == nil {
		tt.Fatal("err: got nil, want non-nil")
	} else if !strings.HasPrefix(err.Error(), "rac: IndexLocationAtStart requires") {
		tt.Fatal(err)
	}
}

func TestWriterILAStartMemTempFile(tt *testing.T) {
	tempFile := &bytes.Buffer{}
	if err := testWriter(IndexLocationAtStart, tempFile, 0, false); err != nil {
		tt.Fatal(err)
	}
}

func TestWriterILAStartRealTempFile(tt *testing.T) {
	f, err := os.CreateTemp("", "rac_test")
	if err != nil {
		tt.Fatalf("TempFile: %v", err)
	}
	defer os.Remove(f.Name())
	defer f.Close()

	if err := testWriter(IndexLocationAtStart, f, 0, false); err != nil {
		tt.Fatal(err)
	}
}

func TestWriterILAEndCPageSize8(tt *testing.T) {
	if err := testWriter(IndexLocationAtEnd, nil, 8, false); err != nil {
		tt.Fatal(err)
	}
}

func TestWriterILAStartCPageSize4(tt *testing.T) {
	tempFile := &bytes.Buffer{}
	if err := testWriter(IndexLocationAtStart, tempFile, 4, false); err != nil {
		tt.Fatal(err)
	}
}

func TestWriterILAStartCPageSize128(tt *testing.T) {
	tempFile := &bytes.Buffer{}
	if err := testWriter(IndexLocationAtStart, tempFile, 128, false); err != nil {
		tt.Fatal(err)
	}
}

func testWriter(iloc IndexLocation, tempFile io.ReadWriter, cPageSize uint64, empty bool) error {
	buf := &bytes.Buffer{}
	w := &ChunkWriter{
		Writer:        buf,
		IndexLocation: iloc,
		TempFile:      tempFile,
		CPageSize:     cPageSize,
	}

	if !empty {
		// We ignore errors (assigning them to _) from the AddXxx calls. Any
		// non-nil errors are sticky, and should be returned by Close.
		//
		// These {Aa,Bb,Cc} chunks are also used in the reader test.
		res0, _ := w.AddResource([]byte("Rrr"))
		res1, _ := w.AddResource([]byte("Ss"))
		_ = w.AddChunk(0x11, fakeCodec, []byte("Aaa"), 0, 0)
		_ = w.AddChunk(0x22, fakeCodec, []byte("Bbbb"), res0, 0)
		_ = w.AddChunk(0x44, fakeCodec, []byte("Cccccccccc12"), res0, res1)
	}

	if err := w.Close(); err != nil {
		return err
	}
	got := hex.Dump(buf.Bytes())

	want := ""
	switch {
	case empty:
		want = writerWantEmpty
	case (iloc == IndexLocationAtEnd) && (cPageSize == 0):
		want = writerWantILAEnd
	case (iloc == IndexLocationAtEnd) && (cPageSize == 8):
		want = writerWantILAEndCPageSize8
	case (iloc == IndexLocationAtStart) && (cPageSize == 0):
		want = writerWantILAStart
	case (iloc == IndexLocationAtStart) && (cPageSize == 4):
		want = writerWantILAStartCPageSize4
	case (iloc == IndexLocationAtStart) && (cPageSize == 128):
		want = writerWantILAStartCPageSize128
	default:
		return fmt.Errorf("unsupported iloc/cPageSize combination")
	}

	if got != want {
		return fmt.Errorf("\ngot:\n%s\nwant:\n%s", got, want)
	}
	return nil
}

func TestMultiLevelIndex(tt *testing.T) {
	buf := &bytes.Buffer{}
	w := &ChunkWriter{
		Writer:        buf,
		IndexLocation: IndexLocationAtStart,
		TempFile:      &bytes.Buffer{},
	}

	// Write 260 chunks with 3 resources. With the current "func gather"
	// algorithm, this results in a root node with two children, both of which
	// are branch nodes. The first branch contains 252 chunks and refers to 3
	// resources (so that its arity is 255). The second branch contains 8
	// chunks and refers to 1 resource (so that its arity is 9).
	xRes := OptResource(0)
	yRes := OptResource(0)
	zRes := OptResource(0)
	primaries := []byte(nil)
	for i := 0; i < 260; i++ {
		secondary := OptResource(0)
		tertiary := OptResource(0)

		switch i {
		case 3:
			xRes, _ = w.AddResource([]byte("XX"))
			yRes, _ = w.AddResource([]byte("YY"))
			secondary = xRes
			tertiary = yRes

		case 4:
			zRes, _ = w.AddResource([]byte("ZZ"))
			secondary = yRes
			tertiary = zRes

		case 259:
			secondary = yRes
		}

		primary := []byte(fmt.Sprintf("p%02x", i&0xFF))
		if i > 255 {
			primary[0] = 'q'
		}
		primaries = append(primaries, primary...)
		_ = w.AddChunk(0x10000, fakeCodec, primary, secondary, tertiary)
	}

	if err := w.Close(); err != nil {
		tt.Fatalf("Close: %v", err)
	}

	encoded := buf.Bytes()
	if got, want := len(encoded), 0x13E2; got != want {
		tt.Fatalf("len(encoded): got 0x%X, want 0x%X", got, want)
	}

	gotHexDump := hex.Dump(encoded)
	gotHexDump = "" +
		gotHexDump[0x000*bytesPerHexDumpLine:0x008*bytesPerHexDumpLine] +
		"...\n" +
		gotHexDump[0x080*bytesPerHexDumpLine:0x088*bytesPerHexDumpLine] +
		"...\n" +
		gotHexDump[0x100*bytesPerHexDumpLine:0x110*bytesPerHexDumpLine] +
		"...\n" +
		gotHexDump[0x13C*bytesPerHexDumpLine:]

	const wantHexDump = "" +
		"00000000  72 c3 63 02 ec 35 00 fe  00 00 fc 00 00 00 00 fe  |r.c..5..........|\n" +
		"00000010  00 00 04 01 00 00 00 01  30 00 00 00 00 00 04 ff  |........0.......|\n" +
		"00000020  30 10 00 00 00 00 01 ff  e2 13 00 00 00 00 01 02  |0...............|\n" +
		"00000030  72 c3 63 ff 81 14 00 ff  00 00 00 00 00 00 00 ff  |r.c.............|\n" +
		"00000040  00 00 00 00 00 00 00 ff  00 00 00 00 00 00 00 ff  |................|\n" +
		"00000050  00 00 01 00 00 00 00 ff  00 00 02 00 00 00 00 ff  |................|\n" +
		"00000060  00 00 03 00 00 00 00 01  00 00 04 00 00 00 00 02  |................|\n" +
		"00000070  00 00 05 00 00 00 00 ff  00 00 06 00 00 00 00 ff  |................|\n" +
		"...\n" +
		"00000800  00 00 f7 00 00 00 00 ff  00 00 f8 00 00 00 00 ff  |................|\n" +
		"00000810  00 00 f9 00 00 00 00 ff  00 00 fa 00 00 00 00 ff  |................|\n" +
		"00000820  00 00 fb 00 00 00 00 ff  00 00 fc 00 00 00 00 01  |................|\n" +
		"00000830  d9 10 00 00 00 00 01 ff  db 10 00 00 00 00 01 ff  |................|\n" +
		"00000840  e0 10 00 00 00 00 01 ff  d0 10 00 00 00 00 01 ff  |................|\n" +
		"00000850  d3 10 00 00 00 00 01 ff  d6 10 00 00 00 00 01 ff  |................|\n" +
		"00000860  dd 10 00 00 00 00 01 00  e2 10 00 00 00 00 01 01  |................|\n" +
		"00000870  e5 10 00 00 00 00 01 ff  e8 10 00 00 00 00 01 ff  |................|\n" +
		"...\n" +
		"00001000  bb 13 00 00 00 00 01 ff  be 13 00 00 00 00 01 ff  |................|\n" +
		"00001010  c1 13 00 00 00 00 01 ff  c4 13 00 00 00 00 01 ff  |................|\n" +
		"00001020  c7 13 00 00 00 00 01 ff  e2 13 00 00 00 00 01 ff  |................|\n" +
		"00001030  72 c3 63 09 53 ad 00 ff  00 00 00 00 00 00 00 ff  |r.c.S...........|\n" +
		"00001040  00 00 01 00 00 00 00 ff  00 00 02 00 00 00 00 ff  |................|\n" +
		"00001050  00 00 03 00 00 00 00 ff  00 00 04 00 00 00 00 ff  |................|\n" +
		"00001060  00 00 05 00 00 00 00 ff  00 00 06 00 00 00 00 ff  |................|\n" +
		"00001070  00 00 07 00 00 00 00 ff  00 00 08 00 00 00 00 01  |................|\n" +
		"00001080  db 10 00 00 00 00 01 ff  ca 13 00 00 00 00 01 ff  |................|\n" +
		"00001090  cd 13 00 00 00 00 01 ff  d0 13 00 00 00 00 01 ff  |................|\n" +
		"000010a0  d3 13 00 00 00 00 01 ff  d6 13 00 00 00 00 01 ff  |................|\n" +
		"000010b0  d9 13 00 00 00 00 01 ff  dc 13 00 00 00 00 01 ff  |................|\n" +
		"000010c0  df 13 00 00 00 00 01 00  e2 13 00 00 00 00 01 09  |................|\n" +
		"000010d0  70 30 30 70 30 31 70 30  32 58 58 59 59 70 30 33  |p00p01p02XXYYp03|\n" +
		"000010e0  5a 5a 70 30 34 70 30 35  70 30 36 70 30 37 70 30  |ZZp04p05p06p07p0|\n" +
		"000010f0  38 70 30 39 70 30 61 70  30 62 70 30 63 70 30 64  |8p09p0ap0bp0cp0d|\n" +
		"...\n" +
		"000013c0  38 70 66 39 70 66 61 70  66 62 70 66 63 70 66 64  |8pf9pfapfbpfcpfd|\n" +
		"000013d0  70 66 65 70 66 66 71 30  30 71 30 31 71 30 32 71  |pfepffq00q01q02q|\n" +
		"000013e0  30 33                                             |03|\n"

	if gotHexDump != wantHexDump {
		tt.Fatalf("\ngot:\n%s\nwant:\n%s", gotHexDump, wantHexDump)
	}

	r := &ChunkReader{
		ReadSeeker:     bytes.NewReader(encoded),
		CompressedSize: int64(len(encoded)),
	}
	gotPrimaries := []byte(nil)
	for {
		c, err := r.NextChunk()
		if err == io.EOF {
			break
		} else if err != nil {
			tt.Fatalf("NextChunk: %v", err)
		}
		p0, p1 := c.CPrimary[0], c.CPrimary[1]
		if (0 <= p0) && (p0 <= p1) && (p1 <= int64(len(encoded))) {
			primary := encoded[p0:p1]
			if len(primary) > 3 {
				primary = primary[:3]
			}
			gotPrimaries = append(gotPrimaries, primary...)
		}
	}
	if !bytes.Equal(gotPrimaries, primaries) {
		tt.Fatalf("\ngot:\n%s\nwant:\n%s", gotPrimaries, primaries)
	}
}

func TestWriter1000Chunks(tt *testing.T) {
loop:
	for i := 0; i < 2; i++ {
		buf := &bytes.Buffer{}
		w := &ChunkWriter{
			Writer: buf,
		}
		if i > 0 {
			w.IndexLocation = IndexLocationAtStart
			w.TempFile = &bytes.Buffer{}
		}

		data := make([]byte, 1)
		res, _ := w.AddResource(data)
		for i := 0; i < 1000; i++ {
			if i == 2*255 {
				_ = w.AddChunk(1, fakeCodec, data, res, 0)
			} else {
				_ = w.AddChunk(1, fakeCodec, data, 0, 0)
			}
		}
		if err := w.Close(); err != nil {
			tt.Errorf("i=%d: Close: %v", i, err)
			continue loop
		}

		encoded := buf.Bytes()
		r := &ChunkReader{
			ReadSeeker:     bytes.NewReader(encoded),
			CompressedSize: int64(len(encoded)),
		}
		for n := 0; ; n++ {
			if _, err := r.NextChunk(); err == io.EOF {
				if n != 1000 {
					tt.Errorf("i=%d: number of chunks: got %d, want %d", i, n, 1000)
					continue loop
				}
				break
			} else if err != nil {
				tt.Errorf("i=%d: NextChunk: %v", i, err)
				continue loop
			}
		}
	}
}

func printChunks(chunks []Chunk) string {
	ss := make([]string, len(chunks))
	for i, c := range chunks {
		ss[i] = fmt.Sprintf("D:%#x, C0:%#x, C1:%#x, C2:%#x, S:%#x, T:%#x, C:%#x",
			c.DRange, c.CPrimary, c.CSecondary, c.CTertiary, c.STag, c.TTag, c.Codec)
	}
	return strings.Join(ss, "\n")
}

func TestChunkReader(tt *testing.T) {
	testCases := []struct {
		name       string
		compressed []byte
	}{
		{"Empty", undoHexDump(writerWantEmpty)},
		{"ILAEnd", undoHexDump(writerWantILAEnd)},
		{"ILAEndCPageSize8", undoHexDump(writerWantILAEndCPageSize8)},
		{"ILAStart", undoHexDump(writerWantILAStart)},
		{"ILAStartCPageSize4", undoHexDump(writerWantILAStartCPageSize4)},
		{"ILAStartCPageSize128", undoHexDump(writerWantILAStartCPageSize128)},
	}

loop:
	for _, tc := range testCases {
		snippet := func(r Range) string {
			if r.Empty() {
				return ""
			}
			if r[1] > int64(len(tc.compressed)) {
				return "CRange goes beyond COffMax"
			}
			s := tc.compressed[r[0]:r[1]]
			if len(s) > 2 {
				return string(s[:2]) + "..."
			}
			return string(s)
		}

		wantDecompressedSize := int64(0)
		wantDescription := ""
		if tc.name != "Empty" {
			// These {Aa,Bb,Cc} chunks are also used in the writer test.
			wantDecompressedSize = 0x11 + 0x22 + 0x44
			wantDescription = `` +
				`DRangeSize:0x11, C0:"Aa...", C1:"", C2:""` + "\n" +
				`DRangeSize:0x22, C0:"Bb...", C1:"Rr...", C2:""` + "\n" +
				`DRangeSize:0x44, C0:"Cc...", C1:"Rr...", C2:"Ss..."`
		}

		r := &ChunkReader{
			ReadSeeker:     bytes.NewReader(tc.compressed),
			CompressedSize: int64(len(tc.compressed)),
		}

		if gotDecompressedSize, err := r.DecompressedSize(); err != nil {
			tt.Errorf("%q test case: %v", tc.name, err)
			continue loop
		} else if gotDecompressedSize != wantDecompressedSize {
			tt.Errorf("%q test case: DecompressedSize: got %d, want %d",
				tc.name, gotDecompressedSize, wantDecompressedSize)
			continue loop
		}

		gotChunks := []Chunk(nil)
		description := &bytes.Buffer{}
		prevDRange1 := int64(0)
		for {
			c, err := r.NextChunk()
			if err == io.EOF {
				break
			} else if err != nil {
				tt.Errorf("%q test case: NextChunk: %v", tc.name, err)
				continue loop
			}

			if c.DRange[0] != prevDRange1 {
				tt.Errorf("%q test case: NextChunk: DRange[0]: got %d, want %d",
					tc.name, c.DRange[0], prevDRange1)
				continue loop
			}
			prevDRange1 = c.DRange[1]

			gotChunks = append(gotChunks, c)
			if description.Len() > 0 {
				description.WriteByte('\n')
			}
			fmt.Fprintf(description, "DRangeSize:0x%X, C0:%q, C1:%q, C2:%q",
				c.DRange.Size(), snippet(c.CPrimary), snippet(c.CSecondary), snippet(c.CTertiary))
		}

		if tc.name == "Empty" {
			if len(gotChunks) != 0 {
				tt.Errorf("%q test case: NextChunk: got non-empty, want empty", tc.name)
			}
			continue loop
		}

		gotDescription := description.String()
		if gotDescription != wantDescription {
			tt.Errorf("%q test case: NextChunk:\n    got\n%s\n    which is\n%s\n    want\n%s",
				tc.name, printChunks(gotChunks), gotDescription, wantDescription)
			continue loop
		}

		// NextChunk should return io.EOF.
		if _, err := r.NextChunk(); err != io.EOF {
			tt.Errorf("%q test case: NextChunk: got %v, want io.EOF", tc.name, err)
			continue loop
		}

		if err := r.SeekToChunkContaining(0x30); err != nil {
			tt.Errorf("%q test case: SeekToChunkContaining: %v", tc.name, err)
			continue loop
		}

		// NextChunk should return the "Bb..." chunk.
		if c, err := r.NextChunk(); err != nil {
			tt.Errorf("%q test case: NextChunk: %v", tc.name, err)
			continue loop
		} else if got, want := snippet(c.CPrimary), "Bb..."; got != want {
			tt.Errorf("%q test case: NextChunk: got %q, want %q", tc.name, got, want)
			continue loop
		}
	}
}

func TestReaderEmpty(tt *testing.T) {
	encoded := undoHexDump(writerWantEmpty)
	r := &Reader{
		ReadSeeker:     bytes.NewReader(encoded),
		CompressedSize: int64(len(encoded)),
	}
	defer r.Close()
	got, err := io.ReadAll(r)
	if err != nil {
		tt.Fatalf("ReadAll: %v", err)
	}
	if len(got) != 0 {
		tt.Fatalf("got %q, want %q", got, []byte(nil))
	}
}

func TestReaderZeroes(tt *testing.T) {
	const dSize = 7
	buf := &bytes.Buffer{}
	w := &ChunkWriter{
		Writer: buf,
	}
	if err := w.AddChunk(dSize, CodecZeroes, nil, 0, 0); err != nil {
		tt.Fatalf("AddChunk: %v", err)
	}
	if err := w.Close(); err != nil {
		tt.Fatalf("Close: %v", err)
	}
	encoded := buf.Bytes()
	r := &Reader{
		ReadSeeker:     bytes.NewReader(encoded),
		CompressedSize: int64(len(encoded)),
	}
	defer r.Close()
	got, err := io.ReadAll(r)
	if err != nil {
		tt.Fatalf("ReadAll: %v", err)
	}
	want := make([]byte, dSize)
	if !bytes.Equal(got, want) {
		tt.Fatalf("got %q, want %q", got, want)
	}
}

func TestLongCodec(tt *testing.T) {
	const codec = Codec(0x80000000326F646D) // "mdo2" backwards, with a high bit.
	buf := &bytes.Buffer{}
	w := &ChunkWriter{
		Writer:        buf,
		IndexLocation: IndexLocationAtStart,
		TempFile:      &bytes.Buffer{},
	}
	if err := w.AddChunk(0x66, codec, []byte{0xAA, 0xBB}, 0, 0); err != nil {
		tt.Fatalf("AddChunk: %v", err)
	}
	if err := w.AddChunk(0x77, codec, []byte{0xCC}, 0, 0); err != nil {
		tt.Fatalf("AddChunk: %v", err)
	}
	if err := w.Close(); err != nil {
		tt.Fatalf("Close: %v", err)
	}

	encoded := buf.Bytes()
	gotHexDump := hex.Dump(encoded)

	const wantHexDump = "" +
		"00000000  72 c3 63 03 3d c9 00 fd  00 00 00 00 00 00 00 ff  |r.c.=...........|\n" +
		"00000010  66 00 00 00 00 00 00 ff  dd 00 00 00 00 00 00 80  |f...............|\n" +
		"00000020  6d 64 6f 32 00 00 00 00  40 00 00 00 00 00 01 ff  |mdo2....@.......|\n" +
		"00000030  42 00 00 00 00 00 01 ff  43 00 00 00 00 00 01 03  |B.......C.......|\n" +
		"00000040  aa bb cc                                          |...|\n"

	if gotHexDump != wantHexDump {
		tt.Fatalf("\ngot:\n%s\nwant:\n%s", gotHexDump, wantHexDump)
	}

	r := &ChunkReader{
		ReadSeeker:     bytes.NewReader(encoded),
		CompressedSize: int64(len(encoded)),
	}
	for i := 0; i < 2; i++ {
		c, err := r.NextChunk()
		if err != nil {
			tt.Fatalf("i=%d: %v", i, err)
		}
		if got, want := c.Codec, codec; got != want {
			tt.Fatalf("i=%d: Codec: got 0x%X, want 0x%X", i, got, want)
		}
	}
	if _, err := r.NextChunk(); err != io.EOF {
		tt.Fatalf("got %v, want %v", err, io.EOF)
	}
}

func TestFindChunkContaining(tt *testing.T) {
	rng := rand.New(rand.NewSource(1))
	arity, dptrs := 0, [256]int64{}

	// simpleFCC is a simple implementation (linear search) of
	// findChunkContaining.
	simpleFCC := func(dptr int64) int {
		if dptrs[0] != 0 {
			panic("unreachable")
		}
		for i, n := 0, arity; i < n; i++ {
			if dptr < dptrs[i+1] {
				return i
			}
		}
		panic("unreachable")
	}

	for i := 0; i < 100; i++ {
		arity = rng.Intn(255) + 1
		size := (16 * arity) + 16

		node := rNode{}

		dptrMax := 0
		for j := 1; j <= arity; j++ {
			dptrMax += rng.Intn(5)
			dptrs[j] = int64(dptrMax)
			putU64LE(node[8*j:], uint64(dptrMax))
		}
		if dptrMax == 0 {
			continue
		}

		node[0] = magic[0]
		node[1] = magic[1]
		node[2] = magic[2]
		node[3] = uint8(arity)
		node[size-2] = 0x01 // Version.
		node[size-1] = uint8(arity)

		checksum := crc32.ChecksumIEEE(node[6:size])
		checksum ^= checksum >> 16
		node[4] = uint8(checksum >> 0)
		node[5] = uint8(checksum >> 8)

		if !node.valid() {
			tt.Fatalf("i=%d: invalid node", i)
		}

		for k := 0; k < 100; k++ {
			dptr := int64(rng.Intn(dptrMax))
			got := node.findChunkContaining(dptr, 0)
			want := simpleFCC(dptr)
			if got != want {
				tt.Fatalf("i=%d, k=%d: got %d, want %d", i, k, got, want)
			}
		}
	}
}
