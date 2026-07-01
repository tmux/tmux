// Copyright 2019 The Wuffs Authors.
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

// print-byte-frequencies.go prints the relative frequencies of stdin's bytes.
//
// Usage: go run print-byte-frequencies.go < foo.bin

import (
	"flag"
	"fmt"
	"io"
	"os"
)

var (
	showZeroes = flag.Bool("show-zeroes", false, "whether to print zero-frequency bytes")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() (retErr error) {
	flag.Parse()
	os.Stdout.WriteString("byte           count /        total =    frequency heat\n")

	in := make([]byte, 4096)
	counts := [256]uint64{}
	total := uint64(0)
	for {
		n, err := os.Stdin.Read(in)
		for _, x := range in[:n] {
			counts[x]++
		}
		total += uint64(n)
		if err != nil {
			if err != io.EOF {
				retErr = err
			}
			break
		}
	}

	for c, count := range counts {
		if (count == 0) && !*showZeroes {
			continue
		}
		freq := float64(0)
		if total > 0 {
			freq = float64(count) / float64(total)
		}
		fmt.Printf("0x%02X  %c %12d / %12d =   %.08f %s\n",
			c, printable(c), count, total, freq, heat(freq))
	}

	return retErr
}

func printable(c int) rune {
	if (0x20 <= c) && (c < 0x7F) {
		return rune(c)
	}
	return '.'
}

func heat(f float64) string {
	const s = "++++++++"
	i := int(f * 256)
	for n, threshold := len(s), 128; n > 0; n, threshold = n-1, threshold>>1 {
		if i >= threshold {
			return s[:n]
		}
	}
	return ""
}
