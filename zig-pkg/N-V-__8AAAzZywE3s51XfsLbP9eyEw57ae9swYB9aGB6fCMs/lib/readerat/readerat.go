// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// ----------------

// Package readerat provides utilities for the io.ReaderAt type.
package readerat

import (
	"errors"
	"io"
)

var (
	errInvalidSize            = errors.New("readerat: invalid size")
	errSeekToInvalidWhence    = errors.New("readerat: seek to invalid whence")
	errSeekToNegativePosition = errors.New("readerat: seek to negative position")
)

// ReadSeeker is an io.ReadSeeker implementation based on an io.ReaderAt (and
// an int64 size).
//
// For example, an os.File is both an io.ReaderAt and an io.ReadSeeker, but its
// io.ReadSeeker methods are not safe to use concurrently. In comparison,
// multiple readerat.ReadSeeker values (using the same os.File as their
// io.ReaderAt) are safe to use concurrently. Each can Read and Seek
// independently.
//
// A single readerat.ReadSeeker is not safe to use concurrently.
//
// Do not modify its exported fields after calling any of its methods.
type ReadSeeker struct {
	ReaderAt io.ReaderAt
	Size     int64
	offset   int64
}

// Read implements io.Reader.
func (r *ReadSeeker) Read(p []byte) (int, error) {
	if r.Size < 0 {
		return 0, errInvalidSize
	}
	if r.Size <= r.offset {
		return 0, io.EOF
	}
	length := r.Size - r.offset
	if int64(len(p)) > length {
		p = p[:length]
	}
	if len(p) == 0 {
		return 0, nil
	}

	actual, err := r.ReaderAt.ReadAt(p, r.offset)
	r.offset += int64(actual)
	if (err == nil) && (r.offset == r.Size) {
		err = io.EOF
	}
	return actual, err
}

// Seek implements io.Seeker.
func (r *ReadSeeker) Seek(offset int64, whence int) (int64, error) {
	if r.Size < 0 {
		return 0, errInvalidSize
	}

	switch whence {
	case io.SeekStart:
		// No-op.
	case io.SeekCurrent:
		offset += r.offset
	case io.SeekEnd:
		offset += r.Size
	default:
		return 0, errSeekToInvalidWhence
	}

	if offset < 0 {
		return 0, errSeekToNegativePosition
	}
	r.offset = offset
	return r.offset, nil
}
