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

// print-json-ascii-escapes.go prints the JSON escapes for ASCII characters.
// For example, the JSON escapes for the ASCII "horizontal tab" and
// "substitute" control characters are "\t" and "\u001A".
//
// Usage: go run print-json-ascii-escapes.go

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
	for c := 0; c < 0x80; c++ {
		s := ""
		switch c {
		case '\b':
			s = "\\b"
		case '\f':
			s = "\\f"
		case '\n':
			s = "\\n"
		case '\r':
			s = "\\r"
		case '\t':
			s = "\\t"
		case '"':
			s = "\\\""
		case '\\':
			s = "\\\\"
		default:
			if c > 0x20 {
				s = string(c)
			} else {
				const hex = "0123456789ABCDEF"
				s = string([]byte{
					'\\',
					'u',
					'0',
					'0',
					hex[c>>4],
					hex[c&0x0F],
				})
			}
		}
		b := make([]byte, 8)
		b[0] = byte(len(s))
		copy(b[1:], s)
		description := s
		if c == 0x7F {
			description = "<DEL>"
		}
		for _, x := range b {
			fmt.Printf("0x%02X, ", x)
		}
		fmt.Printf("// 0x%02X: %q\n", c, description)
	}
	return nil
}
