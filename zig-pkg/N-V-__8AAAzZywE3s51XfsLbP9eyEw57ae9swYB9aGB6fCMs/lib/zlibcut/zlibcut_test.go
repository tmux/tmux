// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

package zlibcut

import (
	"compress/zlib"
	"testing"

	"github.com/google/wuffs/internal/testcut"
)

func TestCut(tt *testing.T) {
	testcut.Test(tt, SmallestValidMaxEncodedLen, Cut, zlib.NewReader, []string{
		"midsummer.txt.zlib",
		"pi.txt.zlib",
		"romeo.txt.zlib",
	})
}

func BenchmarkCut(b *testing.B) {
	testcut.Benchmark(b, SmallestValidMaxEncodedLen, Cut, zlib.NewReader,
		"pi.txt.zlib", 0, 0, 100003)
}
