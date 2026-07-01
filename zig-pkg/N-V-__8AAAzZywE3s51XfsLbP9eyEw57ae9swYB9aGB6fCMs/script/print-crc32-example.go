// Copyright 2018 The Wuffs Authors.
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

// print-crc32-example.go prints the worked examples from std/crc32/README.md.
//
// Usage: go run print-crc32-example.go

import (
	"bytes"
	"os"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	do(false, 4, 0xC, makeBinaryData("H"))
	os.Stdout.WriteString("\n")
	do(true, 32, 0xEDB88320, makeBinaryData("Hi\n"))
	return nil
}

func bit(b bool) byte {
	if b {
		return '1'
	}
	return '0'
}

func hex(b []byte) byte {
	x := byte(0)
	x |= (b[0] - '0') << 0
	x |= (b[1] - '0') << 1
	x |= (b[2] - '0') << 2
	x |= (b[3] - '0') << 3
	if x < 10 {
		return '0' + x
	}
	return 'A' + (x - 10)
}

func makeBinaryData(data string) []byte {
	b := make([]byte, len(data)*8)
	for i := range b {
		mask := uint8(1) << uint(i&7)
		b[i] = bit(data[i>>3]&mask != 0)
	}
	return b
}

func makePoly(n int, bits uint64) []byte {
	b := make([]byte, n+1)
	b[0] = '1'
	for i := 0; i < n; i++ {
		mask := uint64(1) << uint(i)
		b[i+1] = bit(bits&mask != 0)
	}
	return b
}

func show(prefix string, indent int, binaryData []byte) {
	const nineSpaces = "         "
	os.Stdout.WriteString(prefix)
	for ; indent >= 8; indent -= 8 {
		os.Stdout.WriteString(nineSpaces)
	}
	os.Stdout.WriteString(nineSpaces[:indent])
	for {
		b, remaining := binaryData, []byte(nil)
		if len(b) > 8-indent {
			b, remaining = b[:8-indent], b[8-indent:]
			indent = 0
		}
		os.Stdout.WriteString(string(b))
		if len(remaining) == 0 {
			break
		}
		os.Stdout.WriteString(" ")
		binaryData = remaining
	}
	os.Stdout.WriteString("\n")
}

func do(invert bool, polyN int, polyBits uint64, binaryData []byte) {
	poly := makePoly(polyN, polyBits)

	indent := 0
	n := len(binaryData)
	show("input   ", 0, binaryData)
	binaryData = append(binaryData, bytes.Repeat([]byte("0"), polyN)...)
	show("pad     ", 0, binaryData)

	if invert {
		for i := 0; i < polyN; i++ {
			binaryData[i] ^= 1
		}
		show("invert  ", 0, binaryData)
	}

	for {
		indent = bytes.IndexByte(binaryData[:n], '1')
		if indent < 0 {
			break
		}
		show("divisor ", indent, poly)
		for i := range poly {
			binaryData[indent+i] = bit(binaryData[indent+i] != poly[i])
		}
		show("xor     ", 0, binaryData)
	}

	for i := 0; i < n; i++ {
		binaryData[i] = ' '
	}
	show("remain  ", 0, binaryData)

	if invert {
		for i := 0; i < polyN; i++ {
			binaryData[n+i] ^= 1
		}
		show("invert  ", 0, binaryData)

		if polyN&3 == 0 {
			for i := 0; i < polyN; i += 4 {
				h := hex(binaryData[n+i : n+i+4])
				binaryData[n+i+0] = ' '
				binaryData[n+i+1] = ' '
				binaryData[n+i+2] = ' '
				binaryData[n+i+3] = h
			}
			show("hex     ", 0, binaryData)
		}
	}
}
