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

// print-bits.go prints stdin's bytes in hexadecimal and binary.
//
// Usage: go run print-bits.go < foo.bin

import (
	"fmt"
	"io"
	"os"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	in := make([]byte, 4096)
	bits := []byte("...._....\n")
	os.Stdout.WriteString("offset  xoffset ASCII   hex     binary\n")
	for iBase := 0; ; {
		n, err := os.Stdin.Read(in)
		for i, x := range in[:n] {
			bits[0] = bit(x >> 7)
			bits[1] = bit(x >> 6)
			bits[2] = bit(x >> 5)
			bits[3] = bit(x >> 4)
			bits[5] = bit(x >> 3)
			bits[6] = bit(x >> 2)
			bits[7] = bit(x >> 1)
			bits[8] = bit(x >> 0)
			ascii := rune(x)
			if (x < 0x20) || (0x7F <= x) {
				ascii = '.'
			}
			fmt.Printf("%06d  0x%04X  %c       0x%02X    0b_%s", iBase+i, iBase+i, ascii, x, bits)
		}
		if err == io.EOF {
			return nil
		}
		if err != nil {
			return err
		}
		iBase += n
	}
}

func bit(x byte) byte {
	if x&1 != 0 {
		return '1'
	}
	return '0'
}
