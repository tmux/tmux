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

// print-hpd-left-shift.go prints the
// wuffs_base__private_implementation__high_prec_dec__lshift_num_new_digits
// tables.
//
// Usage: go run print-hpd-left-shift.go -comments

import (
	"flag"
	"fmt"
	"math"
	"math/big"
	"os"
)

var (
	comments = flag.Bool("comments", false, "whether to print comments")
)

// powerOf5 return "5 ** n" as a string.
func powerOf5(n int64) string {
	x := big.NewInt(5)
	x.Exp(x, big.NewInt(n), nil)
	return x.String()
}

func ellipsize(s string) string {
	if len(s) <= 16 {
		return s
	}
	return s[:8] + "..." + s[len(s)-5:]
}

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()

	const WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL = 60
	const log2log10 = math.Ln2 / math.Ln10
	data := []byte(nil)

	fmt.Printf("static const uint16_t " +
		"wuffs_base__private_implementation__hpd_left_shift[65] = {\n")
	fmt.Printf("    0x0000,")
	if *comments {
		fmt.Printf("// i= 0\n")
	}
	for i := int64(1); i <= WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL; i++ {
		offset := int64(len(data))
		if offset > 0x07FF {
			panic("offset requires more than 11 bits")
		}
		numNewDigits := int64(log2log10*float64(i)) + 1
		if numNewDigits > 31 {
			panic("numNewDigits requires more than 5 bits")
		}
		code := (numNewDigits << 11) | offset

		p := powerOf5(i)
		data = append(data, p...)
		fmt.Printf("    0x%04X,", code)
		if *comments {
			fmt.Printf("  // i=%2d, num_new_digits=%2d, offset=0x%04X, 5**i=%s\n",
				i, numNewDigits, offset, ellipsize(p))
		} else if i&3 == 3 {
			fmt.Println()
		}
	}
	for i := 1 + WUFFS_BASE__PRIVATE_IMPLEMENTATION__HPD__SHIFT__MAX_INCL; i < 65; i++ {
		fmt.Printf("    0x%04X,", len(data))
		if *comments {
			fmt.Printf("  // i=%2d\n", i)
		}
	}
	fmt.Printf("};\n\n")
	if len(data) > 0x07FF {
		panic("offset requires more than 11 bits")
	}

	fmt.Printf("static const uint8_t "+
		"wuffs_base__private_implementation__powers_of_5[0x%04X] = {\n", len(data))
	for i, x := range data {
		if (i & 15) == 0 {
			fmt.Printf("    ")
		}
		fmt.Printf("%d, ", x&0x0F)
		if (i & 15) == 15 {
			if *comments {
				fmt.Printf(" // offset=0x%04X\n", i&^15)
			} else {
				fmt.Println()
			}
		}
	}
	if *comments {
		fmt.Printf("             // offset=0x%04X\n", len(data)&^15)
	}
	fmt.Printf("};\n")
	return nil
}
