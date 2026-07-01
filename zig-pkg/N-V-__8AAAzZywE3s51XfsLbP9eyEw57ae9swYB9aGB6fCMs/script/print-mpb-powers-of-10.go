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

// print-mpb-powers-of-10.go prints the medium-precision (128-bit mantissa)
// binary (base-2) wuffs_base__private_implementation__powers_of_10 tables.
//
// When the approximation to (10 ** N) is not exact, the mantissa is truncated,
// not rounded to nearest. The base-2 exponent (an implicit third column) is
// chosen so that the mantissa's most signficant bit (bit 127) is set.
//
// Usage: go run print-mpb-powers-of-10.go -detail
//
// With -detail set, its output should include:
//
// {0xA5D3B6D479F8E056, 0x8FD0C16206306BAB},
//    // 1e-307 ≈ (0x8FD0C16206306BABA5D3B6D479F8E056 >> 1147)
//
// {0x8F48A4899877186C, 0xB3C4F1BA87BC8696},
//    // 1e-306 ≈ (0xB3C4F1BA87BC86968F48A4899877186C >> 1144)
//
// ...
//
// {0x3D70A3D70A3D70A3, 0xA3D70A3D70A3D70A},
//    // 1e-2   ≈ (0xA3D70A3D70A3D70A3D70A3D70A3D70A3 >>  134)
//
// {0xCCCCCCCCCCCCCCCC, 0xCCCCCCCCCCCCCCCC},
//    // 1e-1   ≈ (0xCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC >>  131)
//
// {0x0000000000000000, 0x8000000000000000},
//    // 1e0    ≈ (0x80000000000000000000000000000000 >>  127)
//
// {0x0000000000000000, 0xA000000000000000},
//    // 1e1    ≈ (0xA0000000000000000000000000000000 >>  124)
//
// {0x0000000000000000, 0xC800000000000000},
//    // 1e2    ≈ (0xC8000000000000000000000000000000 >>  121)
//
// ...
//
// {0x5C68F256BFFF5A74, 0xA81F301449EE8C70},
//    // 1e287  ≈ (0xA81F301449EE8C705C68F256BFFF5A74 <<  826)
//
// {0x73832EEC6FFF3111, 0xD226FC195C6A2F8C},
//    // 1e288  ≈ (0xD226FC195C6A2F8C73832EEC6FFF3111 <<  829)

import (
	"flag"
	"fmt"
	"math/big"
	"os"
)

var (
	detail = flag.Bool("detail", false, "whether to print detailed comments")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()

	const count = 1 + (+288 - -307)
	fmt.Printf("static const uint64_t "+
		"wuffs_base__private_implementation__powers_of_10[%d][2] = {\n", count)
	for e := -307; e <= +288; e++ {
		if err := do(e); err != nil {
			return err
		}
	}
	fmt.Printf("};\n\n")

	return nil
}

var (
	one    = big.NewInt(1)
	ten    = big.NewInt(10)
	two128 = big.NewInt(0).Lsh(one, 128)
)

// N is large enough so that (1<<N) is easily bigger than 1e310.
const N = 2048

// 1214 is 1023 + 191. 1023 is the bias for IEEE 754 double-precision floating
// point. 191 is ((3 * 64) - 1) and we work with multiples-of-64-bit mantissas.
const bias = 1214

func do(e int) error {
	z := big.NewInt(0).Lsh(one, N)
	if e >= 0 {
		exp := big.NewInt(0).Exp(ten, big.NewInt(int64(+e)), nil)
		z.Mul(z, exp)
	} else {
		exp := big.NewInt(0).Exp(ten, big.NewInt(int64(-e)), nil)
		z.Div(z, exp)
	}

	n := int32(-N)
	for z.Cmp(two128) >= 0 {
		z.Rsh(z, 1)
		n++
	}
	hex := fmt.Sprintf("%X", z)
	if len(hex) != 32 {
		return fmt.Errorf("invalid hexadecimal representation %q", hex)
	}

	// Confirm that the linear approximation to the biased-value-of-n is
	// correct for this particular value of e.
	approxN := uint32(((217706 * e) >> 16) + 1087)
	biasedN := bias + uint32(n)
	if approxN != biasedN {
		return fmt.Errorf("biased-n approximation: have %d, want %d", approxN, biasedN)
	}

	fmt.Printf("    {0x%s, 0x%s},  // 1e%-04d",
		hex[16:], hex[:16], e)
	if *detail {
		fmt.Printf(" ≈ (0x%s ", hex)
		if n >= 0 {
			fmt.Printf("<< %4d)", +n)
		} else {
			fmt.Printf(">> %4d)", -n)
		}
	}

	fmt.Println()
	return nil
}
