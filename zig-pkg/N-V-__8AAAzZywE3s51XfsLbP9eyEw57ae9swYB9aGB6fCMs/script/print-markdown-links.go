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

// print-markdown-links.go prints a sorted list of the link targets in the
// given .md files (or directories containing .md files).
//
// Usage: go run print-markdown-links.go dirname0 filename1 etc
//
// For link targets that look like "/file/names" (as opposed to starting with
// web URLs starting with "http:" or "https:"), they are preceded by a '+' or a
// '-' depending on whether or not that file or directory exists, assuming that
// this script is run in the root directory to resolve filename links that
// start with a slash.
//
// Printing web URLs can be skipped entirely by passing -skiphttp
//
// For example, running:
//   go run script/print-markdown-links.go -skiphttp doc | grep "^-"
// can find broken Markdown links in the documentation.

import (
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/russross/blackfriday/v2"
)

var skiphttp = flag.Bool("skiphttp", false, `skip "http:" or "https:" links`)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()
	r := &renderer{
		links: map[string]struct{}{},
	}

	for _, arg := range flag.Args() {
		err := filepath.Walk(arg, func(path string, info os.FileInfo, walkErr error) error {
			if walkErr != nil {
				return walkErr
			}
			if info.IsDir() || !strings.HasSuffix(path, ".md") {
				return nil
			}
			return visit(r, path)
		})
		if err != nil {
			return err
		}
	}

	sorted := make([]string, 0, len(r.links))
	for k := range r.links {
		sorted = append(sorted, k)
	}
	sort.Strings(sorted)

	for _, s := range sorted {
		mark := '-'
		switch {
		case strings.HasPrefix(s, "http:"), strings.HasPrefix(s, "https:"):
			if *skiphttp {
				continue
			}
			mark = ':'
		case (len(s) > 0) && (s[0] == '/'):
			filename := s[1:]
			if i := strings.IndexByte(filename, '#'); i >= 0 {
				filename = filename[:i]
			}
			if _, err := os.Stat(filename); err == nil {
				mark = '+'
			}
		}
		fmt.Printf("%c %s\n", mark, s)
	}
	return nil
}

func visit(r *renderer, filename string) error {
	src, err := os.ReadFile(filename)
	if err != nil {
		return err
	}
	blackfriday.Run(src, blackfriday.WithRenderer(r))
	return nil
}

type renderer struct {
	links map[string]struct{}
}

func (r *renderer) RenderHeader(w io.Writer, n *blackfriday.Node) {}
func (r *renderer) RenderFooter(w io.Writer, n *blackfriday.Node) {}

func (r *renderer) RenderNode(w io.Writer, n *blackfriday.Node, entering bool) blackfriday.WalkStatus {
	if entering {
		if dst := n.LinkData.Destination; len(dst) != 0 {
			r.links[string(dst)] = struct{}{}
		}
	}
	return blackfriday.GoToNext
}
