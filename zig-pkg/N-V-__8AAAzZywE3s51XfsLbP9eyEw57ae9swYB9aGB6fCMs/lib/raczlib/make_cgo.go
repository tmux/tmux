// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

//go:build cgo
// +build cgo

package raczlib

import (
	"io"

	"github.com/google/wuffs/lib/cgozlib"
)

func (r *CodecReader) makeDecompressor(compressed io.Reader, dict []byte) (io.Reader, error) {
	if r.cachedReader == nil {
		r.cachedReader = &cgozlib.Reader{}
	}
	if err := r.cachedReader.Reset(compressed, dict); err != nil {
		return nil, err
	}
	return r.cachedReader, nil
}
