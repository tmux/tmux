// Copyright 2017 The Wuffs Authors.
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

// extract-giflzw.go extracts or analyzes the LZW-compressed block data in a
// GIF image.
//
//   - Add the "-extract" flag to write the LZW data to a file. The initial
//     byte in each written file is the LZW literal width.
//
//   - Add the "-histogram" flag to print emission length histograms instead of
//     writing out *.indexes.giflzw files.
//
// Add the optional "-allframes" flag to process all frames of an animated GIF,
// not just the first frame. It applies to both -extract and -histogram.
//
// Usage: go run extract-giflzw.go -extract              foo.gif bar.gif
//        go run extract-giflzw.go -histogram -allframes foo.gif bar.gif

import (
	"bytes"
	"compress/lzw"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
)

var (
	allframesFlag = flag.Bool("allframes", false, "process all frames, not just the first")
	extractFlag   = flag.Bool("extract", false, "write LZW data to a file")
	histogramFlag = flag.Bool("histogram", false, "print emission length histograms")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()
	if !*extractFlag && !*histogramFlag {
		return fmt.Errorf("none of -extract or -histogram given")
	}

	for argIndex, arg := range flag.Args() {
		frames, err := extractLZWFrames(arg)
		if err != nil {
			return err
		}

		if *histogramFlag {
			which := "first frame"
			if *allframesFlag {
				which = "all frames"
			}
			fmt.Printf("\n%s (%s)\n", arg, which)

			histogramTotal = 0
			for i := range histogram {
				histogram[i] = 0
			}
		}

		for frameIndex, frame := range frames {
			if err := checkLZW(frame); err != nil {
				fmt.Printf("Warning: frame #%d: %v\n", frameIndex, err)
			}

			if *extractFlag {
				filename := arg[:len(arg)-len(filepath.Ext(arg))]
				if *allframesFlag {
					filename = fmt.Sprintf("%s-frame-%03d", filename, argIndex)
				}
				filename += ".indexes.giflzw"
				if err := os.WriteFile(filename, frame, 0644); err != nil {
					return err
				}
			}
			if *histogramFlag {
				// Ignore any LZW format errors. Decode as much as we can.
				_ = buildHistogram(frame)
			}

			if !*allframesFlag {
				break
			}
		}

		if *histogramFlag {
			fmt.Printf(" Percent   Count  Emission_Length_Bucket\n")
			for i, h := range histogram {
				percent := float64(100*h) / float64(histogramTotal)
				fmt.Printf("  %6.2f  %6d  %s\n", percent, h, bucketNames[i])
			}
		}
	}
	return nil
}

func extractLZWFrames(filename string) (ret [][]byte, err error) {
	src, err := os.ReadFile(filename)
	if err != nil {
		return nil, err
	}

	// Read the header (6 bytes) and screen descriptor (7 bytes).
	if len(src) < 6+7 {
		return nil, fmt.Errorf("not a GIF")
	}
	switch string(src[:6]) {
	case "GIF87a", "GIF89a":
		// No-op.
	default:
		return nil, fmt.Errorf("not a GIF")
	}
	ctSize := 0
	if src[10]&fColorTable != 0 {
		ctSize = 1 << (1 + src[10]&fColorTableBitsMask)
	}
	if src, err = skipColorTable(src[13:], ctSize); err != nil {
		return nil, err
	}

	for len(src) > 0 {
		switch src[0] {
		case sExtension:
			if src, err = skipExtension(src[1:]); err != nil {
				return nil, err
			}
		case sImageDescriptor:
			compressed := []byte(nil)
			if src, compressed, err = readImageDescriptor(src[1:]); err != nil {
				return nil, err
			}
			ret = append(ret, compressed)
		case sTrailer:
			if len(src) != 1 {
				return nil, fmt.Errorf("extraneous data after GIF trailer section")
			}
			return ret, nil
		default:
			return nil, fmt.Errorf("unsupported GIF section")
		}
	}
	return nil, fmt.Errorf("missing GIF trailer section")
}

func skipColorTable(src []byte, ctSize int) (src1 []byte, err error) {
	if ctSize == 0 {
		return src, nil
	}
	n := 3 * ctSize
	if len(src) < n {
		return nil, fmt.Errorf("short color table")
	}
	return src[n:], nil
}

func skipExtension(src []byte) (src1 []byte, err error) {
	if len(src) < 2 {
		return nil, fmt.Errorf("bad GIF extension")
	}
	ext := src[0]
	blockSize := int(src[1])
	if len(src) < 2+blockSize {
		return nil, fmt.Errorf("bad GIF extension")
	}
	src = src[2+blockSize:]
	switch ext {
	case ePlainText, eGraphicControl, eComment, eApplication:
		src1, _, err = readBlockData(src, nil)
		return src1, err
	}
	return nil, fmt.Errorf("unsupported GIF extension")
}

func readImageDescriptor(src []byte) (src1 []byte, ret1 []byte, err error) {
	if len(src) < 9 {
		return nil, nil, fmt.Errorf("bad GIF image descriptor")
	}
	ctSize := 0
	if src[8]&fColorTable != 0 {
		ctSize = 1 << (1 + src[8]&fColorTableBitsMask)
	}
	if src, err = skipColorTable(src[9:], ctSize); err != nil {
		return nil, nil, err
	}

	if len(src) < 1 {
		return nil, nil, fmt.Errorf("bad GIF image descriptor")
	}
	literalWidth := src[0]
	if literalWidth < 2 || 8 < literalWidth {
		return nil, nil, fmt.Errorf("bad GIF literal width")
	}
	return readBlockData(src[1:], []byte{literalWidth})
}

func readBlockData(src []byte, ret []byte) (src1 []byte, ret1 []byte, err error) {
	for {
		if len(src) < 1 {
			return nil, nil, fmt.Errorf("bad GIF block")
		}
		n := int(src[0]) + 1
		if len(src) < n {
			return nil, nil, fmt.Errorf("bad GIF block")
		}
		ret = append(ret, src[1:n]...)
		src = src[n:]
		if n == 1 {
			return src, ret, nil
		}
	}
}

func checkLZW(data []byte) error {
	if len(data) == 0 {
		return fmt.Errorf("missing GIF literal width")
	}
	rc := lzw.NewReader(bytes.NewReader(data[1:]), lzw.LSB, int(data[0]))
	defer rc.Close()
	_, err := io.ReadAll(rc)
	if err != nil {
		return fmt.Errorf("block data is not valid LZW: %v", err)
	}
	return nil
}

const numBuckets = 14

var (
	histogramTotal = uint32(0)
	histogram      = [numBuckets]uint32{}
	bucketNames    = [numBuckets]string{
		"         1 (Literal Code)",
		"         2",
		"         3",
		"         4",
		"         5",
		"         6",
		"         7",
		"         8",
		"  9 ..  16",
		" 17 ..  32",
		" 33 ..  64",
		" 65 .. 128",
		"127 .. 256",
		"256+   ",
	}
)

func bucket(emissionLength uint32) uint32 {
	switch {
	case emissionLength <= 0:
		panic("bad emissionLength")
	case emissionLength <= 8:
		return emissionLength - 1
	case emissionLength <= 16:
		return 8
	case emissionLength <= 32:
		return 9
	case emissionLength <= 64:
		return 10
	case emissionLength <= 128:
		return 11
	case emissionLength <= 256:
		return 12
	default:
		return 13
	}
}

func buildHistogram(data []byte) error {
	if len(data) == 0 {
		return fmt.Errorf("missing GIF literal width")
	}

	literalWidth, data := uint32(data[0]), data[1:]
	clearCode := uint32(1) << literalWidth
	endCode := clearCode + 1

	saveCode := endCode
	prevCode := uint32(0)
	width := literalWidth + 1

	prefixes := [4096]uint32{}
	emissionLengths := [4096]uint32{}
	for i := uint32(0); i < clearCode; i++ {
		emissionLengths[i] = 1
	}

	bits := uint32(0)
	nBits := uint32(0)

	for {
		for nBits < width {
			if len(data) == 0 {
				return fmt.Errorf("bad LZW data: short read")
			}
			bits |= uint32(data[0]) << nBits
			data = data[1:]
			nBits += 8
		}

		code := bits & ((uint32(1) << width) - 1)
		bits >>= width
		nBits -= width

		if code < clearCode {
			// Literal, emitting exactly 1 byte.

		} else if code == clearCode {
			saveCode = endCode
			prevCode = 0
			width = literalWidth + 1
			continue

		} else if code == endCode {
			break

		} else if code <= saveCode {
			// Copy, emitting 2 or more bytes.

		} else {
			return fmt.Errorf("bad LZW data: code out of range")
		}

		n := emissionLengths[code]
		if code == saveCode {
			n = emissionLengths[prevCode] + 1
		}
		histogram[bucket(n)]++
		histogramTotal++

		if saveCode <= 4095 {
			prefixes[saveCode] = prevCode
			emissionLengths[saveCode] = emissionLengths[prevCode] + 1
			saveCode++
			if (saveCode == (1 << width)) && (width < 12) {
				width++
			}
			prevCode = code
		}
	}

	return nil
}

// The constants below are intrinsic to the GIF file format. The spec is
// http://www.w3.org/Graphics/GIF/spec-gif89a.txt
const (
	// Flags.
	fColorTableBitsMask = 0x07
	fColorTable         = 0x80

	// Sections.
	sExtension       = 0x21
	sImageDescriptor = 0x2C
	sTrailer         = 0x3B

	// Extensions.
	ePlainText      = 0x01
	eGraphicControl = 0xF9
	eComment        = 0xFE
	eApplication    = 0xFF
)
