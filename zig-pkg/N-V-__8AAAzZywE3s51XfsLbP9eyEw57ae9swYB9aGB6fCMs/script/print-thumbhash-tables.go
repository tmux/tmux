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

// print-thumbhash-tables.go prints the std/thumbhash tables.
//
// Usage: go run print-thumbhash-tables.go

import (
	"fmt"
	"math"
	"os"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	printLxLy()
	fmt.Println()
	printFrom4Bits(1.00)
	fmt.Println()
	printFrom4Bits(1.25)
	fmt.Println()
	printCosines()
	return nil
}

func printLxLy() {
	for bits := 0; bits < 16; bits++ {
		lCount := bits & 7
		if lCount < 3 {
			continue
		} else if lCount == 3 {
			fmt.Println()
		}
		hasAlpha := (bits >> 3) & 1
		isLandscape := (bits >> 4) & 1

		lx := lCount
		ly := 7
		if hasAlpha != 0 {
			ly = 5
		}
		if isLandscape != 0 {
			lx, ly = ly, lx
		}
		ratio := float64(lx) / float64(ly)

		w, h := 32, 32
		if ratio > 1 {
			h = int(math.Round(32 / ratio))
		} else {
			w = int(math.Round(32 * ratio))
		}

		lenlac := 0
		for cy := 0; cy < ly; cy++ {
			for cx := ifElse(cy, 0, 1); (cx * ly) < (lx * (ly - cy)); cx++ {
				lenlac++
			}
		}

		lenpac := 0
		for cy := 0; cy < 3; cy++ {
			for cx := ifElse(cy, 0, 1); (cx * 3) < (3 * (3 - cy)); cx++ {
				lenpac++
			}
		}

		lenaac := 0
		if hasAlpha != 0 {
			for cy := 0; cy < 5; cy++ {
				for cx := ifElse(cy, 0, 1); (cx * 5) < (5 * (5 - cy)); cx++ {
					lenaac++
				}
			}
		}

		fileSize := 5 + hasAlpha + (lenlac+lenpac+lenpac+lenaac+1)/2

		fmt.Printf("l_count: %d   lx / ly = %d / %d   w = %2d   h = %2d   #lac = %2d   #pac = #qac = %d   #aac = %2d   file_size = 3 + %2d\n",
			lCount, lx, ly, w, h, lenlac, lenpac, lenaac, fileSize)
	}
}

func printFrom4Bits(scale float64) {
	for i := 0; i < 16; i++ {
		x := (float64(i)/7.5 - 1) * scale
		y := (x * (1 << 14))
		fmt.Printf("0x%04X,  // %s%0.3f\n", 0xFFFF&int(math.Round(y)), plusSign(y), x)
	}
}

func printCosines() {
	widths := []int{
		0, 14, 18, 19, 23, 26, 27, 32,
	}

	for _, w := range widths {
		fmt.Printf("\n// w = %2d\n", w)
		for x := 0; x < w; x++ {
			fmt.Printf("[")
			for cx := 1; cx < 7; cx++ {
				u := math.Cos(math.Pi * float64(cx) * (float64(x) + 0.5) / float64(w))
				v := u * (1 << 14)
				fmt.Printf("0x%04X", 0xFFFF&int(math.Round(v)))
				if cx < 6 {
					fmt.Printf(",")
				}
			}
			fmt.Printf("],  // x = %2d\n", x)
		}
	}
}

func ifElse(c int, x int, y int) int {
	if c != 0 {
		return x
	}
	return y
}

func plusSign(x float64) string {
	if x >= 0 {
		return "+"
	}
	return ""
}
