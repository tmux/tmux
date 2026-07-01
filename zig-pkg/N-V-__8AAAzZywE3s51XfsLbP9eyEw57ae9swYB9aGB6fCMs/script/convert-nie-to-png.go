// Copyright 2022 The Wuffs Authors.
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

// convert-nie-to-png.go decodes NIE from stdin and encodes PNG to stdout.
//
// Usage: go run convert-nie-to-png.go < foo.nie > foo.png

import (
	"image/png"
	"os"

	"github.com/google/wuffs/lib/nie"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	img, err := nie.Decode(os.Stdin)
	if err != nil {
		return err
	}
	return png.Encode(os.Stdout, img)
}
