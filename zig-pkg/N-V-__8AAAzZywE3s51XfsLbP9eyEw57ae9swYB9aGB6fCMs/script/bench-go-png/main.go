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

// This program exercises the Go standard library's PNG decoder.
//
// Wuffs' C code doesn't depend on Go per se, but this program gives some
// performance data for specific Go PNG implementations. The equivalent Wuffs
// benchmarks (on the same test images) are run via:
//
// wuffs bench std/png

import (
	"bytes"
	"fmt"
	"image"
	"image/draw"
	"image/png"
	"os"
	"runtime"
	"strings"
	"time"
)

const (
	iterscale = 20
	reps      = 5
)

type testCase = struct {
	benchname     string
	src           []byte
	itersUnscaled uint32
}

var testCases = []testCase{{
	benchname:     "go_png_decode_image_19k_8bpp",
	src:           mustLoad("test/data/bricks-gray.no-ancillary.png"),
	itersUnscaled: 50,
}, {
	benchname:     "go_png_decode_image_40k_24bpp",
	src:           mustLoad("test/data/hat.png"),
	itersUnscaled: 50,
}, {
	benchname:     "go_png_decode_image_77k_8bpp",
	src:           mustLoad("test/data/bricks-dither.png"),
	itersUnscaled: 50,
}, {
	benchname:     "go_png_decode_image_552k_32bpp_verify_checksum",
	src:           mustLoad("test/data/hibiscus.primitive.png"),
	itersUnscaled: 4,
}, {
	benchname:     "go_png_decode_image_4002k_24bpp",
	src:           mustLoad("test/data/harvesters.png"),
	itersUnscaled: 1,
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
			expandPalette := strings.HasSuffix(tc.benchname, "_77k_8bpp")
			numBytes, err := decode(tc.src, expandPalette)
			if err != nil {
				return err
			}
			for j := uint64(1); j < iters; j++ {
				decode(tc.src, expandPalette)
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

func decode(src []byte, expandPalette bool) (numBytes uint64, retErr error) {
	m, err := png.Decode(bytes.NewReader(src))
	if err != nil {
		return 0, err
	}

	b := m.Bounds()
	n := uint64(b.Dx()) * uint64(b.Dy())

	if expandPalette {
		dst := image.NewRGBA(b)
		draw.Draw(dst, b, m, b.Min, draw.Src)
		m = dst
	}

	pix := []byte(nil)
	switch m := m.(type) {
	case *image.Gray:
		n *= 1
	case *image.NRGBA:
		n *= 4
		pix = m.Pix
	case *image.RGBA:
		n *= 4
		pix = m.Pix
	default:
		return 0, fmt.Errorf("unexpected image type %T", m)
	}

	// Convert RGBA => BGRA.
	if pix != nil {
		for i, iEnd := 0, len(pix)/4; i < iEnd; i += 4 {
			pix[(4*i)+0], pix[(4*i)+2] = pix[(4*i)+2], pix[(4*i)+0]
		}
	}

	return n, nil
}
