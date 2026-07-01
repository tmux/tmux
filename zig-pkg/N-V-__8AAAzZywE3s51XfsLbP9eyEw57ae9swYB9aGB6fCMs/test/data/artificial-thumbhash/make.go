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

// make.go writes the https://evanw.github.io/thumbhash/ examples to individual
// files, prepending a 3-byte magic string so that /usr/bin/file or similar
// programs can identify the data as thumbhash-formatted data.
//
// That 3-byte magic string is "\xC3\xBE\xFE", which is arbitrary and not part
// of the original thumbhash description or implementation. But "\xC3\xBE" is
// the UTF-8 encoding of 'þ' (U+00FE LATIN SMALL LETTER THORN) and "\xFE" is
// invalid UTF-8 but is the ISO-8859-1 encoding of 'þ'. The Old English letter
// 'þ' is pronounced like the "th" that starts "thumbhash".
//
// The filenames are the base64.RawURLEncoding of the data (similar to but
// slightly different from the base64.RawStdEncoding used on that web page)
// plus a ".th" extension.
//
// The foobar.png files that correspond to each foobar.th file were manually
// scraped from the https://evanw.github.io/thumbhash/ page. These PNG files
// were written by thumbhash's original JavaScript reference implementation,
// exercised by that page. This small JavaScript library favors implementation
// simplicity over maximizing the compression ratio.
//
// For any given foobar.th input file, that library's RGBA output and Wuffs'
// RGBA output may differ slightly, due to floating point versus fixed point
// rounding errors, but they're pretty close.
//
// Usage: go run make.go

import (
	"encoding/base64"
	"os"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	for _, d := range data {
		filename := convertBase64StdToBase64URL(d) + ".th"
		in, err := base64.RawStdEncoding.DecodeString(d)
		if err != nil {
			return err
		}
		out := append([]byte(magic), in...)
		if err := os.WriteFile(filename, out, 0666); err != nil {
			return err
		}
	}
	return nil

}

func convertBase64StdToBase64URL(s string) string {
	b := []byte(s)
	for i, c := range b {
		if c == '+' {
			b[i] = '-'
		} else if c == '/' {
			b[i] = '_'
		}
	}
	return string(b)
}

const magic = "\xC3\xBE\xFE"

var data = []string{
	"1QcSHQRnh493V4dIh4eXh1h4kJUI",
	"3PcNNYSFeXh/d3eld0iHZoZgVwh2",
	"3OcRJYB4d3h/iIeHeEh3eIhw+j3A",
	"HBkSHYSIeHiPiHh8eJd4eTN0EEQG",
	"VggKDYAW6lZvdYd6d2iZh/p4GE/k",
	"2fcZFIB3iId/h3iJh4aIYJ2V8g",
	"IQgSLYZ6iHePh4h1eFeHh4dwgwg3",
	"YJqGPQw7sFlslqhFafSE+Q6oJ1h2iHB2Rw",
	"2IqDBQQnxnj0JoLYdM3f8ahpuDeHiHdwZw",
}
