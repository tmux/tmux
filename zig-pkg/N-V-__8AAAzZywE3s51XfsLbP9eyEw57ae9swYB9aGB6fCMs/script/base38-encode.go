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

// base38-encode.go prints the base38 encoding of each argument.
//
// Usage: go run base38-encode.go gif json

import (
	"fmt"
	"os"
	"strings"

	"github.com/google/wuffs/lib/base38"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	args := os.Args
	if len(args) > 1 {
		for _, arg := range args[1:] {
			arg := (strings.ToLower(arg) + "....")[:4]
			if code, ok := base38.Encode(arg); ok {
				code0 := fmt.Sprintf("0x%06X", code)
				code1 := fmt.Sprintf("0x%08X", code<<10)
				fmt.Printf("%s    code: %s %s    code<<10: %s %s\n",
					arg, code0, underscore(code0), code1, underscore(code1))
			} else {
				fmt.Printf("%s -\n", arg)
			}
		}
	}
	return nil
}

func underscore(s string) string {
	if n := len(s) - 4; n > 0 {
		return s[:n] + "_" + s[n:]
	}
	return s
}
