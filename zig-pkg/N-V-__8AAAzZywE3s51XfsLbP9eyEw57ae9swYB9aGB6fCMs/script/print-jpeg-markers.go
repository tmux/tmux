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

// print-jpeg-markers.go prints a JPEG's markers' position and type.
//
// Usage: go run print-jpeg-markers.go foo.jpeg

import (
	"fmt"
	"os"
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

	pos := 0
	if len(src) < 2 {
		return posError(pos, len(src))
	}
	for pos <= (len(src) - 2) {
		if src[pos] != 0xFF {
			return posError(pos, len(src))
		}
		marker := src[pos+1]
		fmt.Printf("pos = 0x%08X = %10d    marker = 0xFF 0x%02X  %s\n", pos, pos, marker, names[marker])
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
			return posError(pos, len(src))
		}
		payloadLength := (int(src[pos]) << 8) | int(src[pos+1])
		pos += payloadLength
		if (payloadLength < 2) || (pos < 0) || (pos > len(src)) {
			return posError(pos, len(src))
		}

		if marker != 0xDA { // SOS (Start Of Scan) marker.
			continue
		}
		for pos < len(src) {
			if src[pos] != 0xFF {
				pos += 1
			} else if pos >= (len(src) - 1) {
				return posError(pos, len(src))
			} else if m := src[pos+1]; (m != 0x00) && ((m < 0xD0) || (0xD7 < m)) {
				break
			} else {
				pos += 2
			}
		}
	}
	return posError(pos, len(src))
}

func posError(pos int, lenSrc int) error {
	return fmt.Errorf("bad JPEG, pos = 0x%08X = %10d, len(src) = 0x%08X = %10d",
		pos, pos, lenSrc, lenSrc)
}

var names = [256]string{
	0xC0: "SOF0 (Sequential/Baseline)",
	0xC1: "SOF1 (Sequential/Extended)",
	0xC2: "SOF2 (Progressive)",
	0xC3: "SOF3",
	0xC4: "DHT",
	0xC5: "SOF5",
	0xC6: "SOF6",
	0xC7: "SOF7",

	0xC8: "JPG",
	0xC9: "SOF9",
	0xCA: "SOF10",
	0xCB: "SOF11",
	0xCC: "DAC",
	0xCD: "SOF13",
	0xCE: "SOF14",
	0xCF: "SOF15",

	0xD0: "RST0",
	0xD1: "RST1",
	0xD2: "RST2",
	0xD3: "RST3",
	0xD4: "RST4",
	0xD5: "RST5",
	0xD6: "RST6",
	0xD7: "RST7",

	0xD8: "SOI",
	0xD9: "EOI",
	0xDA: "SOS",
	0xDB: "DQT",
	0xDC: "DNL",
	0xDD: "DRI",
	0xDE: "DHP",
	0xDF: "EXP",

	0xE0: "APP0",
	0xE1: "APP1",
	0xE2: "APP2",
	0xE3: "APP3",
	0xE4: "APP4",
	0xE5: "APP5",
	0xE6: "APP6",
	0xE7: "APP7",

	0xE8: "APP8",
	0xE9: "APP9",
	0xEA: "APP10",
	0xEB: "APP11",
	0xEC: "APP12",
	0xED: "APP13",
	0xEE: "APP14",
	0xEF: "APP15",

	0xFE: "COM",
}
