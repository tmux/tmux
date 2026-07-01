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

// print-deflate-magic-numbers.go prints the std/deflate lcode_magic_numbers
// and dcode_magic_numbers values based on the tables in RFC 1951 secion 3.2.5.
//
// The lcode base numbers are biased by -3 so that (base_number_minus_3 +
// extra_bits) fits in the range [0, 255]. This makes a bitwise-and with 0xFF a
// no-op, in terms of computed value, but proves to the compiler that the
// result is within a certain range.
//
// The dcode base numbers are biased by -1 so that (base_number_minus_1 +
// extra_bits) fits in the range [0, 32767]. This makes a bitwise-and with
// 0x7FFF a no-op, in terms of computed value, but proves to the compiler that
// the result is within a certain range. Furthermore, proving that (d + 1) > 0
// is trivial, for d of type u32[..something], compared to proving that d > 0,
// which usually requires a runtime check (an if branch).
//
// Usage: go run print-deflate-magic-numbers.go

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
	// See the "base numbers" comment above.
	biases := [2]uint32{3, 1}

	for i := 0; i < 2; i++ {
		if i != 0 {
			fmt.Println()
		}
		for j := 0; j < 32; j++ {
			x := uint32(0x08000000)
			if bn := baseNumbers[i][j]; bn != bad {
				x = 0x40000000 | ((bn - biases[i]) << 8) | (extraBits[i][j] << 4)
			}
			fmt.Printf("0x%08X,", x)
			if j&7 == 7 {
				fmt.Println()
			}
		}
	}
	return nil
}

const bad = 0xFFFFFFFF

var (
	baseNumbers = [2][32]uint32{{
		3, 4, 5, 6, 7, 8, 9, 10,
		11, 13, 15, 17, 19, 23, 27, 31,
		35, 43, 51, 59, 67, 83, 99, 115,
		131, 163, 195, 227, 258, bad, bad, bad,
	}, {
		1, 2, 3, 4, 5, 7, 9, 13,
		17, 25, 33, 49, 65, 97, 129, 193,
		257, 385, 513, 769, 1025, 1537, 2049, 3073,
		4097, 6145, 8193, 12289, 16385, 24577, bad, bad,
	}}

	extraBits = [2][32]uint32{{
		0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 1, 1, 2, 2, 2, 2,
		3, 3, 3, 3, 4, 4, 4, 4,
		5, 5, 5, 5, 0, bad, bad, bad,
	}, {
		0, 0, 0, 0, 1, 1, 2, 2,
		3, 3, 4, 4, 5, 5, 6, 6,
		7, 7, 8, 8, 9, 9, 10, 10,
		11, 11, 12, 12, 13, 13, bad, bad,
	}}
)
