// Copyright 2022 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package litonlylzma

import (
	"bytes"
	"os"
	"os/exec"
	"testing"
)

const (
	oneThousand0x00s = "oneThousand0x00s"
	oneThousand0xFFs = "oneThousand0xFFs"
)

func testRoundTrip(tt *testing.T, filename string) {
	original := []byte(nil)
	switch filename {
	case oneThousand0x00s:
		original = make([]byte, 1000)
	case oneThousand0xFFs:
		original = make([]byte, 1000)
		for i := range original {
			original[i] = 0xFF
		}
	default:
		o, err := os.ReadFile("../../test/data/" + filename)
		if err != nil {
			tt.Fatalf("ReadFile: %v", err)
		}
		original = o
	}

	fileFormats := [...]FileFormat{
		FileFormatLZMA,
		FileFormatXz,
	}

	for _, f := range fileFormats {
		compressed, err := f.Encode(nil, original)
		if err != nil {
			tt.Fatalf("%v.Encode: %v", f, err)
		}
		recovered, _, err := f.Decode(nil, compressed)
		if err != nil {
			tt.Fatalf("%v.Decode: %v", f, err)
		}
		if !bytes.Equal(original, recovered) {
			tt.Fatalf("%v: round trip produced different bytes", f)
		}
	}
}

func TestRoundTrip0Bytes(tt *testing.T)           { testRoundTrip(tt, "0.bytes") }
func TestRoundTripArchiveTar(tt *testing.T)       { testRoundTrip(tt, "archive.tar") }
func TestRoundTripOneThousand0x00s(tt *testing.T) { testRoundTrip(tt, oneThousand0x00s) }
func TestRoundTripOneThousand0xFFs(tt *testing.T) { testRoundTrip(tt, oneThousand0xFFs) }
func TestRoundTripPiTxt(tt *testing.T)            { testRoundTrip(tt, "pi.txt") }
func TestRoundTripRomeoTxt(tt *testing.T)         { testRoundTrip(tt, "romeo.txt") }

func TestUsrBinFoocatCompat(tt *testing.T) {
	// Set "allowTestsToExecOtherPrograms = true" for more test coverage,
	// checking compatibility with third-party decompression tools that are
	// often available (at a well known location) on Linux.
	const allowTestsToExecOtherPrograms = false
	if !allowTestsToExecOtherPrograms {
		tt.Skip("not configured to exec third-party programs")
	}

	testCases := [...]struct {
		fileFormat FileFormat
		command    string
	}{
		{FileFormatLZMA, "/usr/bin/lzcat"},
		{FileFormatXz, "/usr/bin/xzcat"},
	}

	// Not every system has "/usr/bin/{lzcat,xzcat}" commands.
	for _, tc := range testCases {
		if _, err := os.Stat(tc.command); err != nil {
			tt.Skipf("%v", err)
		}
	}

	pi, err := os.ReadFile("../../test/data/pi.txt")
	if err != nil {
		tt.Fatalf("ReadFile: %v", err)
	} else if len(pi) != 100003 {
		tt.Fatalf("ReadFile: unexpected pi.txt file length")
	}

	lengths := [...]int{0, 1, 10, 100, 1e3, 1e4, 1e5}

	for _, tc := range testCases {
		for _, length := range lengths {
			original := pi[:length]

			compressed, err := tc.fileFormat.Encode(nil, original)
			if err != nil {
				tt.Fatalf("%v, length=%d: Encode: %v", tc.fileFormat, length, err)
			}

			stdout := bytes.Buffer{}
			stderr := bytes.Buffer{}

			cmd := exec.Command(tc.command)
			cmd.Stdin = bytes.NewReader(compressed)
			cmd.Stdout = &stdout
			cmd.Stderr = &stderr
			if err := cmd.Run(); err != nil {
				tt.Fatalf("%v, length=%d: Run: %v\nstderr:\n%s", tc.fileFormat, length, err, stderr.Bytes())
			}

			recovered := stdout.Bytes()
			if !bytes.Equal(original, recovered) {
				tt.Fatalf("%v, length=%d: round trip produced different bytes", tc.fileFormat, length)
			}
		}
	}
}
