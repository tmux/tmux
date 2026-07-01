// Copyright 2023 The Wuffs Authors.
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

// truncate-progressive-jpeg.go saves truncated editions of a JPEG, cutting
// just after every SOS (Start Of Scan) marker, appending an EOI (End Of Image)
// marker after the cut.
//
// Usage: go run truncate-progressive-jpeg.go foo.jpeg

import (
	"fmt"
	"os"
	"strings"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	if len(os.Args) < 2 {
		return fmt.Errorf("usage: progname filename.jpeg")
	}
	src, err := os.ReadFile(os.Args[1])
	if err != nil {
		return err
	}
	prefix, suffix := os.Args[1], ".jpeg"
	if strings.HasSuffix(prefix, ".jpg") {
		prefix = prefix[:len(prefix)-4]
		suffix = ".jpg"
	} else if strings.HasSuffix(prefix, ".jpeg") {
		prefix = prefix[:len(prefix)-5]
	}

	pos := 0
	if len(src) < 2 {
		return posError(pos)
	}
	scanNumber := 0
	for pos <= (len(src) - 2) {
		if src[pos] != 0xFF {
			return posError(pos)
		}
		marker := src[pos+1]
		pos += 2
		switch {
		case (0xD0 <= marker) && (marker < 0xD9):
			// RSTn (Restart) and SOI (Start Of Image) markers have no payload.
			continue
		case marker == 0xD9:
			// EOI (End Of Image) marker.
			return nil
		}

		if pos >= (len(src) - 2) {
			return posError(pos)
		}
		payloadLength := (int(src[pos]) << 8) | int(src[pos+1])
		pos += payloadLength
		if (payloadLength < 2) || (pos < 0) || (pos > len(src)) {
			return posError(pos)
		}

		if marker != 0xDA { // SOS (Start Of Scan) marker.
			continue
		}
		for pos < len(src) {
			if src[pos] != 0xFF {
				pos += 1
			} else if pos >= (len(src) - 1) {
				return posError(pos)
			} else if m := src[pos+1]; (m != 0x00) && ((m < 0xD0) || (0xD7 < m)) {
				break
			} else {
				pos += 2
			}
		}
		outName := fmt.Sprintf("%s.scan%03d%s", prefix, scanNumber, suffix)
		scanNumber++
		data := ([]byte)(nil)
		data = append(data, src[:pos]...)
		data = append(data, "\xFF\xD9"...) // Append an EOI marker.
		if err := os.WriteFile(outName, data, 0644); err != nil {
			return err
		}
		fmt.Printf("Wrote %s (cut at 0x%08X = %6d bytes).\n", outName, pos, pos)
	}
	return posError(pos)
}

func posError(pos int) error {
	return fmt.Errorf("invalid JPEG at pos = 0x%08X = %10d", pos, pos)
}
