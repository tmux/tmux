// Copyright 2018 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package wuffsroot

import (
	"errors"
	"go/build"
	"os"
	"path/filepath"
	"sync"
)

var initialWorkingDirectory = ""

func init() {
	initialWorkingDirectory, _ = os.Getwd()
}

var cache struct {
	mu    sync.Mutex
	value string
}

func setValue(value string) (string, error) {
	cache.mu.Lock()
	cache.value = value
	cache.mu.Unlock()

	return value, nil
}

func Value() (string, error) {
	cache.mu.Lock()
	value := cache.value
	cache.mu.Unlock()

	if value != "" {
		return value, nil
	}

	const wrdTxt = "wuffs-root-directory.txt"

	// Look for "w-r-d.txt" in the working directory or its ancestors.
	for p, q := initialWorkingDirectory, ""; p != q; p, q = filepath.Dir(p), p {
		if _, err := os.Stat(filepath.Join(p, wrdTxt)); err == nil {
			return setValue(p)
		}
	}

	// Look for "github.com/google/wuffs/w-r-d.txt" in the Go source
	// directories.
	for _, p := range build.Default.SrcDirs() {
		p = filepath.Join(p, "github.com", "google", "wuffs")
		if _, err := os.Stat(filepath.Join(p, wrdTxt)); err == nil {
			return setValue(p)
		}
	}

	return "", errors.New("could not find Wuffs root directory")
}
