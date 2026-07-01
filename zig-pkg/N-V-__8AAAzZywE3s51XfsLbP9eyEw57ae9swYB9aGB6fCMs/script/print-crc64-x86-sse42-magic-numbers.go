// Copyright 2024 The Wuffs Authors.
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

// print-crc64-x86-sse42-magic-numbers.go prints the std/crc64
// ECMA_X86_SSE42_ETC magic number tables.
//
// It is like print-crc32-x86-sse42-magic-numbers.go but for CRC-64/ECMA, not
// CRC-32/IEEE.
//
// Usage: go run print-crc64-x86-sse42-magic-numbers.go
//
// Output:
// Px' = 0x92D8_AF2B_AF0E_1E85
// k1' = 0x6AE3_EFBB_9DD4_41F3
// k2' = 0x081F_6054_A784_2DF4
// k3' = 0xE05D_D497_CA39_3AE4
// k4' = 0xDABE_95AF_C787_5F40
// μ'  = 0x9C3E_466C_1729_63D5

import (
	"fmt"
	"strings"
)

// px is the P(x) polynomial, a bit-reversal (with explicit high bit) of the
// CRC-64/ECMA polynomial sometimes written as 0xC96C_5795_D787_0F42.
// P(x)  = 0x1_42F0_E1EB_A9EA_3693
// P(x)  = 0b1_01000010_11110000_11100001_11101011_10101001_11101010_00110110_10010011
const px = "10100001011110000111000011110101110101001111010100011011010010011"

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
	fmt.Printf("%s = 0x%04X_%04X_%04X_%04X\n", name,
		0xFFFF&(v>>48), 0xFFFF&(v>>32), 0xFFFF&(v>>16), 0xFFFF&(v>>0))
}

func calcKn(name string, power int) {
	numerator := "1" + strings.Repeat("0", power)
	b := []byte(numerator)
	i := 0
	debugf("      %s\n", numerator)
	for i+len(px) < len(numerator) {
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
	numerator := "1" + strings.Repeat("0", 128)
	mu := make([]byte, 129)
	b := []byte(numerator)
	i := 0
	debugf("      %s\n", numerator)
	for i+len(px) < len(numerator) {
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
	show(name, string(mu[:65]))
}

func main() {
	calcKn("fold1a'", 128+64)
	calcKn("fold1b'", 128)
	calcKn("fold2a'", 256+64)
	calcKn("fold2b'", 256)
	calcKn("fold4a'", 512+64)
	calcKn("fold4b'", 512)
	calcKn("fold8a'", 1024+64)
	calcKn("fold8b'", 1024)
	calcMu("μ' ")
	show("Px'", px)
}
