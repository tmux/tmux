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

// This program is obsolete.
//
// print-crc32-x86-sse42-magic-numbers.go prints the std/crc32
// IEEE_X86_SSE42_ETC magic number tables.
//
// This reproduces the numbers on pages 16 and 22 of Gopal et al. "Fast CRC
// Computation for Generic Polynomials Using PCLMULQDQ Instruction":
// https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/fast-crc-computation-generic-polynomials-pclmulqdq-paper.pdf
//
// Usage: go run print-crc32-x86-sse42-magic-numbers.go
//
// Output:
// The numbers from page 16 (the regular format).
// Px  = 0x1_04C1_1DB7
// k1  = 0x0_8833_794C
// k2  = 0x0_E622_8B11
// k3  = 0x0_C5B9_CD4C
// k4  = 0x0_E8A4_5605
// k5  = 0x0_F200_AA66
// k6  = 0x0_490D_678D
// μ   = 0x1_04D1_01DF
//
// The numbers from page 22 (the bit-reflected format).
// Px' = 0x1_DB71_0641
// k1' = 0x1_5444_2BD4
// k2' = 0x1_C6E4_1596
// k3' = 0x1_7519_97D0
// k4' = 0x0_CCAA_009E
// k5' = 0x1_63CD_6124
// k6' = 0x1_DB71_0640
// μ'  = 0x1_F701_1641

import (
	"fmt"
	"strings"
)

// px is the P(x) polynomial given on page 16, a bit-reversal (with explicit
// high bit) of the CRC-32/IEEE polynomial sometimes written as 0xEDB8_8320.
// P(x)  = 0x1_04C1_1DB7
// P(x)  = 0b1_00000100_11000001_00011101_10110111
const px = "100000100110000010001110110110111"

// pxdash is P(x)', the bit-reversed format for P(x), given on page 22. This
// constant is not used by this program, but it is provided for completeness.
// P(x)' = 0x1_DB71_0641 = ((0xEDB8_8320 << 1) | 1)
// P(x)' = 0x1_11011011_01110001_00000110_01000001
const pxdash = "111011011011100010000011001000001"

var spaces = strings.Repeat(" ", 9999)

func debugf(format string, a ...interface{}) {
	if false { // Change false to true to show the long divisions.
		fmt.Printf(format, a...)
	}
}

func show(name string, value string) {
	v := uint64(0)
	if strings.Contains(name, "'") {
		for i := len(value) - 1; i >= 0; i-- {
			v = (v << 1) | uint64(value[i]&1)
		}
	} else {
		for i := 0; i < len(value); i++ {
			v = (v << 1) | uint64(value[i]&1)
		}
	}
	fmt.Printf("%s = 0x%X_%04X_%04X\n", name, v>>32, 0xFFFF&(v>>16), 0xFFFF&(v>>0))
}

func calcKn(name string, power int) {
	numerator := "1" + strings.Repeat("0", power)
	b := []byte(numerator)
	i := 0
	debugf("      %s\n", numerator)
	for i+len(px) <= len(numerator) {
		for j := 0; j < len(px); j++ {
			b[i+j] ^= 1 & px[j]
		}
		debugf("      %s%s\n", spaces[:i], px)
		for ; (i < len(b)) && (b[i] == '0'); i++ {
			b[i] = ' '
		}
		debugf("      %s\n", b)
	}
	show(name, string(b[len(b)-len(px):]))
}

func calcMu(name string) {
	numerator := "1" + strings.Repeat("0", 64)
	mu := make([]byte, 65)
	b := []byte(numerator)
	i := 0
	debugf("      %s\n", numerator)
	for i+len(px) <= len(numerator) {
		for j := 0; j < len(px); j++ {
			b[i+j] ^= 1 & px[j]
		}
		debugf("      %s%s\n", spaces[:i], px)
		b[i] = ':'
		mu[i] = '1'
		i++
		for ; (i < len(b)) && (b[i] == '0'); i++ {
			b[i] = ' '
			mu[i] = '0'
		}
		debugf("      %s\n", b)
	}
	show(name, string(mu[:33]))
}

func main() {
	fmt.Println("The numbers from page 16 (the regular format).")
	show("Px ", px)
	calcKn("k1 ", 512+64)
	calcKn("k2 ", 512)
	calcKn("k3 ", 128+64)
	calcKn("k4 ", 128)
	calcKn("k5 ", 96)
	calcKn("k6 ", 64)
	calcMu("μ  ")

	fmt.Println()

	fmt.Println("The numbers from page 22 (the bit-reflected format).")
	show("Px'", px)
	calcKn("k1'", 512+32)
	calcKn("k2'", 512-32)
	calcKn("k3'", 128+32)
	calcKn("k4'", 128-32)
	calcKn("k5'", 64)
	calcKn("k6'", 32)
	calcMu("μ' ")
}
