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

// This program exercises the Go standard library's Deflate decoder.
//
// Wuffs' C code doesn't depend on Go per se, but this program gives some
// performance data for specific Go Deflate implementations. The equivalent
// Wuffs benchmarks (on the same test files) are run via:
//
// wuffs bench std/deflate

import (
	"bytes"
	"compress/flate"
	"fmt"
	"io"
	"os"
	"runtime"
	"time"
)

const (
	iterscale = 10
	reps      = 5
)

type testCase = struct {
	benchname     string
	src           []byte
	itersUnscaled uint32
}

// The various magic constants below are copied from test/c/std/deflate.c
var testCases = []testCase{{
	benchname:     "go_deflate_decode_1k_full_init",
	src:           mustLoad("test/data/romeo.txt.gz")[20:550],
	itersUnscaled: 2000,
}, {
	benchname:     "go_deflate_decode_10k_full_init",
	src:           mustLoad("test/data/midsummer.txt.gz")[24:5166],
	itersUnscaled: 300,
}, {
	benchname:     "go_deflate_decode_100k_just_one_read",
	src:           mustLoad("test/data/pi.txt.gz")[17:48335],
	itersUnscaled: 30,
}}

func mustLoad(filename string) []byte {
	src, err := os.ReadFile("../../" + filename)
	if err != nil {
		panic(err.Error())
	}
	return src
}

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	fmt.Printf("# Go %s\n", runtime.Version())
	fmt.Printf("#\n")
	fmt.Printf("# The output format, including the \"Benchmark\" prefixes, is compatible with the\n")
	fmt.Printf("# https://godoc.org/golang.org/x/perf/cmd/benchstat tool. To install it, first\n")
	fmt.Printf("# install Go, then run \"go install golang.org/x/perf/cmd/benchstat\".\n")

	for i := -1; i < reps; i++ {
		for _, tc := range testCases {
			runtime.GC()

			start := time.Now()

			iters := uint64(tc.itersUnscaled) * iterscale
			numBytes, err := decode(tc.src)
			if err != nil {
				return err
			}
			for j := uint64(1); j < iters; j++ {
				decode(tc.src)
			}

			elapsedNanos := time.Since(start)

			kbPerS := numBytes * uint64(iters) * 1000000 / uint64(elapsedNanos)

			if i < 0 {
				continue // Warm up rep.
			}

			fmt.Printf("Benchmark%-30s %8d %12d ns/op %8d.%03d MB/s\n",
				tc.benchname, iters, uint64(elapsedNanos)/iters, kbPerS/1000, kbPerS%1000)
		}
	}

	return nil
}

func decode(src []byte) (numBytes uint64, retErr error) {
	r := flate.NewReader(bytes.NewReader(src))
	defer r.Close()
	n, err := io.Copy(io.Discard, r)
	return uint64(n), err
}
