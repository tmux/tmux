// Copyright 2021 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//go:build ignore
// +build ignore

package main

// make-red-blue-gradient.go makes a simple PNG image with optional color
// correction chunks.
//
// Usage: go run make-red-blue-gradient.go > foo.png

import (
	"bytes"
	"compress/zlib"
	"errors"
	"flag"
	"hash/crc32"
	"image"
	"image/color"
	"image/png"
	"math"
	"os"
	"path/filepath"
	"strings"
)

var (
	checkerboard = flag.Bool("checkerboard", false, "Create a checkerboard pattern instead")
	gama         = flag.Float64("gama", 0, "Gamma correction value (e.g. 2.2)")
	iccp         = flag.String("iccp", "", "ICC filename (e.g. foo/bar.icc)")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()

	// Make a checkerboard or red-blue gradient.
	src := image.Image(nil)
	if *checkerboard {
		// With -gama=1.0 (or -gama=2.2), the two halves of this image should
		// have roughly equal (or un-equal) brightness.
		m := image.NewGray(image.Rect(0, 0, 256, 256))
		for y := 0; y < 256; y++ {
			for x := 0; x < 256; x++ {
				if x < 128 {
					m.SetGray(x, y, color.Gray{0x80})
				} else if ((x ^ y) & 1) == 1 {
					m.SetGray(x, y, color.Gray{0xFF})
				} else {
					m.SetGray(x, y, color.Gray{0x00})
				}
			}
		}
		src = m
	} else {
		m := image.NewRGBA(image.Rect(0, 0, 256, 256))
		for y := 0; y < 256; y++ {
			for x := 0; x < 256; x++ {
				m.SetRGBA(x, y, color.RGBA{
					R: uint8(x),
					G: 0x00,
					B: uint8(y),
					A: 0xFF,
				})
			}
		}
		src = m
	}

	// Encode to PNG.
	buf := &bytes.Buffer{}
	png.Encode(buf, src)
	original := buf.Bytes()

	// Split the encoding into a magic+IHDR prefix and an IDAT+IEND suffix. The
	// starting magic number is 8 bytes long. The IHDR chunk is 25 bytes long.
	prefix, suffix := original[:33], original[33:]
	out := []byte(nil)
	out = append(out, prefix...)

	// Insert the optional chunks between the prefix and suffix.

	if *gama > 0 {
		g := uint32(math.Round(100000 / *gama))
		out = appendPNGChunk(out, "gAMA",
			byte(g>>24),
			byte(g>>16),
			byte(g>>8),
			byte(g>>0),
		)
	}

	if *iccp != "" {
		raw, err := os.ReadFile(*iccp)
		if err != nil {
			return err
		}
		name, err := iccpName(*iccp)
		if err != nil {
			return err
		}
		data := &bytes.Buffer{}
		data.WriteString(name)
		data.WriteByte(0x00) // The name is NUL terminated.
		data.WriteByte(0x00) // 0x00 means zlib compression.
		w := zlib.NewWriter(data)
		w.Write(raw)
		w.Close()
		out = appendPNGChunk(out, "iCCP", data.Bytes()...)
	}

	// Append the suffix and write to stdout.
	out = append(out, suffix...)
	_, err := os.Stdout.Write(out)
	return err
}

func appendPNGChunk(b []byte, name string, chunk ...byte) []byte {
	n := uint32(len(chunk))
	b = append(b,
		byte(n>>24),
		byte(n>>16),
		byte(n>>8),
		byte(n>>0),
	)
	b = append(b, name...)
	b = append(b, chunk...)
	hasher := crc32.NewIEEE()
	hasher.Write([]byte(name))
	hasher.Write(chunk)
	c := hasher.Sum32()
	return append(b,
		byte(c>>24),
		byte(c>>16),
		byte(c>>8),
		byte(c>>0),
	)
}

// iccpName determines a "bar" iCCP profile name (that satisifes the PNG
// specification re the iCCP chunk) based on the "foo/bar.icc" filename.
func iccpName(filename string) (string, error) {
	s := filepath.Base(filename)
	if i := strings.IndexByte(s, '.'); i >= 0 {
		s = s[:i]
	}
	s = strings.TrimSpace(s)
	if strings.Index(s, "  ") >= 0 {
		return "", errors.New("ICCP name cannot contain consecutive spaces")
	} else if (len(s) < 1) || (79 < len(s)) {
		return "", errors.New("ICCP name has invalid length.")
	}
	for i := 0; i < len(s); i++ {
		if (s[i] < ' ') || ('~' < s[i]) {
			// Strictly speaking, the PNG specification allows the iCCP name to
			// be printable Latin-1, not just printable ASCII, but printable
			// ASCII is easier and avoids any later Latin-1 vs UTF-8 ambiguity.
			return "", errors.New("ICCP name is not printable ASCII")
		}
	}
	return s, nil
}
