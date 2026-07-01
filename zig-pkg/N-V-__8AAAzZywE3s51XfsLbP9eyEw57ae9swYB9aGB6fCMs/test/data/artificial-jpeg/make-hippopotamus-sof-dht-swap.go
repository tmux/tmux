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

// Usage: go run make-hippopotamus-sof-dht-swap.go
//
// The test/data/hippopotamus.jpeg file has one SOF0 (Start Of Frame type 0)
// and then four DHT (Define Huffman Table) markers. This program swaps the
// order of the SOF0 and first DHT fragments.
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
// $ go run script/print-jpeg-markers.go test/data/artificial-jpeg/hippopotamus-sof-dht-swap.jpeg
// pos = 0x00000000 =          0    marker = 0xFF 0xD8  SOI
// pos = 0x00000002 =          2    marker = 0xFF 0xE0  APP0
// pos = 0x00000014 =         20    marker = 0xFF 0xDB  DQT
// pos = 0x00000059 =         89    marker = 0xFF 0xDB  DQT
// pos = 0x0000009E =        158    marker = 0xFF 0xC4  DHT
// pos = 0x000000B9 =        185    marker = 0xFF 0xC0  SOF0 (Sequential/Baseline)
// pos = 0x000000CC =        204    marker = 0xFF 0xC4  DHT
// pos = 0x000000FC =        252    marker = 0xFF 0xC4  DHT
// pos = 0x00000119 =        281    marker = 0xFF 0xC4  DHT
// pos = 0x0000014B =        331    marker = 0xFF 0xDA  SOS
// pos = 0x000004C1 =       1217    marker = 0xFF 0xD9  EOI

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

	part0 := src[0x000:0x09E]
	part1 := src[0x09E:0x0B1] // SOF0
	part2 := src[0x0B1:0x0CC] // DHT
	part3 := src[0x0CC:0x4C3]

	dst := []byte(nil)
	dst = append(dst, part0...)
	dst = append(dst, part2...) // Note the re-order: it's not part1.
	dst = append(dst, part1...) // Note the re-order: it's not part2.
	dst = append(dst, part3...)

	return os.WriteFile("hippopotamus-sof-dht-swap.jpeg", dst, 0666)
}
