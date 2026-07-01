// Copyright 2020 The Wuffs Authors.
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

// print-render-number-f64-tests.go prints the
// test_wuffs_strconv_render_number_f64 test cases.
//
// Usage: go run print-render-number-f64-tests.go

import (
	"fmt"
	"math"
	"os"
	"sort"
	"strconv"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	testCases := append([]uint64(nil), u64TestCases...)
	for _, f := range f64TestCases {
		testCases = append(testCases, math.Float64bits(f))
	}

	sort.Slice(testCases, func(i int, j int) bool {
		return testCases[i] < testCases[j]
	})

	for i, tc := range testCases {
		f := math.Float64frombits(tc)

		if (i > 0) && (tc == testCases[i-1]) {
			return fmt.Errorf("duplicate test case (f=%g, tc=0x%X)", f, tc)
		}

		// Check that calling strconv.FormatFloat with a precision of -1 (round
		// to shortest) does indeed return a string that, when parsed, recovers
		// the original number.
		shortest := strconv.FormatFloat(f, 'g', -1, 64)
		g, err := strconv.ParseFloat(shortest, 64)
		if err != nil {
			return fmt.Errorf("ParseFloat failed (f=%g, tc=0x%X): %v", f, tc, err)
		}
		equal := tc == math.Float64bits(g)
		if math.IsNaN(f) {
			equal = math.IsNaN(g)
		}
		if !equal {
			return fmt.Errorf("round-trip failed (f=%g, tc=0x%X)", f, tc)
		}
	}

	for _, tc := range testCases {
		f := math.Float64frombits(tc)
		fmt.Printf(`{
	.x = 0x%016X,
	.want__e = %s,
	.want__f = %s,
	.want_0g = %s,
	.want_2e = %s,
	.want_3f = %s,
	.want_4g = %s,
},`+"\n",
			tc,
			do(f, -1, 'e'),
			do(f, -1, 'f'),
			do(f, +0, 'g'),
			do(f, +2, 'e'),
			do(f, +3, 'f'),
			do(f, +4, 'g'),
		)
	}
	return nil
}

func do(f float64, precision int, format byte) (ret string) {
	s := strconv.FormatFloat(f, format, precision, 64)
	for ; len(s) > 50; s = s[50:] {
		ret += fmt.Sprintf("%q\n\t\t", s[:50])
	}
	ret += fmt.Sprintf("%q", s)
	if ret == `"+Inf"` {
		ret = `"Inf"`
	}
	return ret
}

var f64TestCases = []float64{
	// Approximations of e, the base of the natural logarithm.
	2.7,
	2.72,
	2.718,
	2.7183,
	2.71828,
	2.718282,
	2.7182818,
	2.71828183,

	// Approximations of N_A, the Avogadro constant.
	6.0e23,
	6.02e23,
	6.022e23,
	6.0221e23,
	6.02214e23,
	6.022141e23,
	6.0221408e23,
	6.02214076e23,

	// Some "parse to float64" implementations find this one tricky.
	// https://github.com/serde-rs/json/issues/707
	122.416294033786585,
}

var u64TestCases = []uint64{
	0x0000000000000000,
	0x0000000000000001,
	0x0000000000000002,
	0x0000000000000003,
	0x000730D67819E8D2,
	0x000FFFFFFFFFFFFF,
	0x0010000000000000,
	0x0031FA182C40C60D,
	0x369C2DF8DA5B6CA8,
	0x369C314ABE948EB1,
	0x3E70000000000000,
	0x3F88000000000000,
	0x3FD0000000000000,
	0x3FD3333333333333,
	0x3FD3333333333334,
	0x3FD5555555555555,
	0x3FEFFFFFFFFFFFFF,
	0x3FF0000000000000,
	0x3FF0000000000001,
	0x3FF0000000000002,
	0x3FF4000000000000,
	0x3FF8000000000000,
	0x4008000000000000,
	0x400921F9F01B866E,
	0x400921FB54442D11,
	0x400921FB54442D18,
	0x400C000000000000,
	0x4014000000000000,
	0x4036000000000000,
	0x4037000000000000,
	0x4038000000000000,
	0x40FE240C9FCB0C02,
	0x41E0246690000001,
	0x4202A05F20000000,
	0x4330000000000000,
	0x4330000000000001,
	0x4330000000000002,
	0x433FFFFFFFFFFFFE,
	0x433FFFFFFFFFFFFF,
	0x4340000000000000,
	0x4340000000000001,
	0x4340000000000002,
	0x4370000000000000,
	0x43E158E460913D00,
	0x43F002F1776DDA67,
	0x4415AF1D78B58C40,
	0x44B52D02C7E14AF6,
	0x46293E5939A08CEA,
	0x54B249AD2594C37D,
	0x54B2987670ADB613,
	0x7BBA44DF832B8D46,
	0x7BF06B0BB1FB384C,
	0x7C2485CE9E7A065F,
	0x7FAC7B1F3CAC7433,
	0x7FE1CCF385EBC8A0,
	0x7FEFFFFFFFFFFFFF,
	0x7FF0000000000000,
	0x7FFFFFFFFFFFFFFF,
	0x8000000000000000,
	0xC008000000000000,
	0xFFF0000000000000,
	0xFFFFFFFFFFFFFFFF,
}
