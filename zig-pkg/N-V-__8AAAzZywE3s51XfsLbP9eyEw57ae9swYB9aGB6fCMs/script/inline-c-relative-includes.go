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

// inline-c-relative-includes.go echoes the given C file to stdout, expanding
// #include lines that uses "quoted files" (but not <bracketed files>).
//
// The output should be a stand-alone C file that other people can easily
// compile and run with no further dependencies, other than test/data files.
//
// Usage: go run inline-c-relative-includes.go path/to/foo.c

import (
	"bufio"
	"bytes"
	"fmt"
	"os"
	"path/filepath"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	if len(os.Args) != 2 {
		return fmt.Errorf("inline-c-relative-includes takes exactly 1 argument")
	}
	w := bufio.NewWriter(os.Stdout)
	defer w.Flush()
	return do(w, ".", os.Args[1], 0)
}

var (
	prefix = []byte(`#include "`)
	suffix = []byte(`"`)

	seen = map[string]bool{}
)

func do(w *bufio.Writer, dir string, filename string, depth int) error {
	if depth == 100 {
		return fmt.Errorf("recursion too deep")
	}
	if depth != 0 {
		fmt.Fprintf(w, "// BEGIN INLINE #include %q\n", filename)
		defer fmt.Fprintf(w, "// END   INLINE #include %q\n", filename)
	}
	depth++

	filename = filepath.Join(dir, filename)
	if seen[filename] {
		return fmt.Errorf("duplicate filename %q", filename)
	}
	seen[filename] = true

	dir, _ = filepath.Split(filename)
	f, err := os.Open(filename)
	if err != nil {
		return err
	}
	defer f.Close()

	r := bufio.NewScanner(f)
	for r.Scan() {
		line := r.Bytes()

		if s := line; bytes.HasPrefix(s, prefix) {
			s = s[len(prefix):]
			if bytes.HasSuffix(s, suffix) {
				s = s[:len(s)-len(suffix)]
				if err := do(w, dir, string(s), depth); err == nil {
					continue
				} else if os.IsNotExist(err) {
					// This is probably an #include of a system header, like
					// `#include "zlib.h"`, and not of Wuffs code. Fall through
					// and print the #include line as normal.
				} else {
					return err
				}
			}
		}

		w.Write(line)
		w.WriteByte('\n')
	}
	return r.Err()
}
