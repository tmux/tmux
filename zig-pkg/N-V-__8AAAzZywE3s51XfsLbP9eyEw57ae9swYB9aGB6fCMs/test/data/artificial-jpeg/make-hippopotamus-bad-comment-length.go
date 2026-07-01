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

// Usage: go run make-hippopotamus-bad-comment-length.go
//
// This program inserts a four-byte "FF FE 00 00" fragment into the
// test/data/hippopotamus.jpeg file. The "FF FE" is a COM (comment) marker. The
// "00 00" payload length is technically invalid (the minimum length is 2, per
// the itu-t81.pdf spec section B.1.1.4 "Marker segments") but libjpeg-turbo is
// more lenient and allows less-than-2 (for COM and APPn markers).
//
// https://www.w3.org/Graphics/JPEG/itu-t81.pdf
//
// https://github.com/libjpeg-turbo/libjpeg-turbo/blob/2192560d74e6e6cf99dd05928885573be00a8208/jdmarker.c#L869-L876
//
// $ go run script/print-jpeg-markers.go test/data/hippopotamus.jpeg
// pos = 0x00000000 =          0    marker = 0xFF 0xD8  SOI
// pos = 0x00000002 =          2    marker = 0xFF 0xE0  APP0
// pos = 0x00000014 =         20    marker = 0xFF 0xDB  DQT
// pos = 0x00000059 =         89    marker = 0xFF 0xDB  DQT
// pos = 0x0000009E =        158    marker = 0xFF 0xC0  SOF0 (Sequential/Baseline)
// pos = 0x000000B1 =        177    marker = 0xFF 0xC4  DHT
// pos = 0x000000CC =        204    marker = 0xFF 0xC4  DHT
// pos = 0x000000FC =        252    marker = 0xFF 0xC4  DHT
// pos = 0x00000119 =        281    marker = 0xFF 0xC4  DHT
// pos = 0x0000014B =        331    marker = 0xFF 0xDA  SOS
// pos = 0x000004C1 =       1217    marker = 0xFF 0xD9  EOI
//
// $ go run script/print-jpeg-markers.go test/data/artificial-jpeg/hippopotamus-bad-comment-length.jpeg
// pos = 0x00000000 =          0    marker = 0xFF 0xD8  SOI
// pos = 0x00000002 =          2    marker = 0xFF 0xE0  APP0
// pos = 0x00000014 =         20    marker = 0xFF 0xDB  DQT
// pos = 0x00000059 =         89    marker = 0xFF 0xFE  COM
// bad JPEG, pos = 0x0000005B =         91, len(src) = 0x000004C7 =       1223

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
	src, err := os.ReadFile("../hippopotamus.jpeg")
	if err != nil {
		return err
	} else if len(src) != 0x4C3 {
		return fmt.Errorf("bad input length: 0x%X", len(src))
	}

	part0 := src[0x000:0x059]
	part1 := src[0x059:0x4C3]

	dst := []byte(nil)
	dst = append(dst, part0...)
	dst = append(dst, 0xFF, 0xFE, 0x00, 0x00)
	dst = append(dst, part1...)

	return os.WriteFile("hippopotamus-bad-comment-length.jpeg", dst, 0666)
}
