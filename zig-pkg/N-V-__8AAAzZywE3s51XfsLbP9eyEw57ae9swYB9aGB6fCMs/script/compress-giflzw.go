// Copyright 2017 The Wuffs Authors.
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

// compress-giflzw.go applies GIF's LZW-compression. The initial byte in each
// written file is the LZW literal width.
//
// Usage: go run compress-giflzw.go < pi.txt > pi.txt.giflzw

import (
	"compress/lzw"
	"io"
	"os"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	os.Stdout.WriteString("\x08")
	w := lzw.NewWriter(os.Stdout, lzw.LSB, 8)
	if _, err := io.Copy(w, os.Stdin); err != nil {
		w.Close()
		return err
	}
	return w.Close()
}
