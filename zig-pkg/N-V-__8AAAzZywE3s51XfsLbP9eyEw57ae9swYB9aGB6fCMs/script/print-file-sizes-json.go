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

// print-file-sizes-json.go prints a JSON object containing the names and sizes
// of the files and directories under the current working directory,
// recursively.
//
// A directory is printed as a JSON object whose keys are its children's names.
// Child directories' values are also JSON objects. Child files' values are the
// file size as a JSON number.
//
// A directory's JSON object also contains a key-value pair for the empty
// string key. Its value is the sum of all children and descendent file sizes.
// Note that this is just the total raw file size, and does not account for any
// file system overhead for directories or for rounding up a file sizes to a
// multiple of a disk block size.

import (
	"encoding/json"
	"os"
	"path/filepath"
	"strings"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	root := dir{"": int64(0)}

	if err := filepath.Walk(".", func(path string, info os.FileInfo, walkErr error) error {
		if walkErr != nil {
			return walkErr
		}
		if (path != "") && (path[0] == '.') {
			if info.IsDir() && (path != ".") {
				return filepath.SkipDir
			}
			return nil
		}
		path = filepath.ToSlash(path)

		ancestors := []dir{}
		d := root
		for remaining := ""; len(path) > 0; path = remaining {
			ancestors = append(ancestors, d)
			if i := strings.IndexByte(path, '/'); i >= 0 {
				path, remaining = path[:i], path[i+1:]
			} else {
				remaining = ""
			}
			if (remaining == "") && !info.IsDir() {
				n := info.Size()
				d[path] = n
				for _, a := range ancestors {
					a[""] = a[""].(int64) + n
				}
			} else if next, ok := d[path]; ok {
				d = next.(dir)
			} else {
				next := dir{"": int64(0)}
				d[path] = next
				d = next
			}
		}

		return nil
	}); err != nil {
		return err
	}

	if enc, err := json.Marshal(root); err != nil {
		return err
	} else {
		os.Stdout.Write(enc)
	}
	return nil
}

type dir map[string]interface{}
