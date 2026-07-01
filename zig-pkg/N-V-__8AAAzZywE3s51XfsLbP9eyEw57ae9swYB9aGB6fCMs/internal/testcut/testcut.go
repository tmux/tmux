// Copyright 2019 The Wuffs Authors.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT

// Package testcut provides support for testing flatecut and zlibcut.
package testcut

import (
	"bytes"
	"io"
	"os"
	"testing"
)

func calculateDecodedLen(b []byte,
	newReader func(io.Reader) (io.ReadCloser, error)) (int64, error) {

	r, err := newReader(bytes.NewReader(b))
	if err != nil {
		return 0, err
	}
	n, err := io.Copy(io.Discard, r)
	if err != nil {
		r.Close()
		return 0, err
	}
	return n, r.Close()
}

func clone(b []byte) []byte {
	return append([]byte(nil), b...)
}

func Test(tt *testing.T,
	smallestValidMaxEncodedLen int,
	cut func(io.Writer, []byte, int) (int, int, error),
	newReader func(io.Reader) (io.ReadCloser, error),
	filenames []string) {

	for _, filename := range filenames {
		full, err := os.ReadFile("../../test/data/" + filename)
		if err != nil {
			tt.Errorf("f=%q: ReadFile: %v", filename, err)
			continue
		}
		fullDecodedLen, err := calculateDecodedLen(full, newReader)
		if err != nil {
			tt.Errorf("f=%q: calculateDecodedLen: %v", filename, err)
			continue
		}

		maxEncodedLens := []int{
			smallestValidMaxEncodedLen + 0,
			smallestValidMaxEncodedLen + 1,
			smallestValidMaxEncodedLen + 2,
			smallestValidMaxEncodedLen + 3,
			smallestValidMaxEncodedLen + 4,
			smallestValidMaxEncodedLen + 5,
			smallestValidMaxEncodedLen + 6,
			smallestValidMaxEncodedLen + 7,
			20,
			77,
			234,
			512,
			1999,
			8192,
			len(full) - 7,
			len(full) - 6,
			len(full) - 5,
			len(full) - 4,
			len(full) - 3,
			len(full) - 2,
			len(full) - 1,
			len(full) + 0,
			len(full) + 1,
			len(full) + 2,
		}

		for _, maxEncodedLen := range maxEncodedLens {
			if maxEncodedLen < smallestValidMaxEncodedLen {
				continue
			}
			w0 := &bytes.Buffer{}
			encoded := clone(full)
			encLen, decLen, err := cut(w0, encoded, maxEncodedLen)
			if err != nil {
				tt.Errorf("f=%q, mEL=%d: cut: %v", filename, maxEncodedLen, err)
				continue
			}
			if encLen > maxEncodedLen {
				tt.Errorf("f=%q, mEL=%d: encLen vs max: got %d, want <= %d",
					filename, maxEncodedLen, encLen, maxEncodedLen)
				continue
			}
			if encLen > len(encoded) {
				tt.Errorf("f=%q, mEL=%d: encLen vs len: got %d, want <= %d",
					filename, maxEncodedLen, encLen, len(encoded))
				continue
			}

			w1 := &bytes.Buffer{}
			r, err := newReader(bytes.NewReader(encoded[:encLen]))
			if err != nil {
				tt.Errorf("f=%q, mEL=%d: newReader: %v", filename, maxEncodedLen, err)
				continue
			}
			if n, err := io.Copy(w1, r); err != nil {
				tt.Errorf("f=%q, mEL=%d: io.Copy: %v", filename, maxEncodedLen, err)
				continue
			} else if n != int64(decLen) {
				tt.Errorf("f=%q, mEL=%d: io.Copy: got %d, want %d",
					filename, maxEncodedLen, n, decLen)
				continue
			}

			if !bytes.Equal(w0.Bytes(), w1.Bytes()) {
				tt.Errorf("f=%q, mEL=%d: decoded bytes were not equal", filename, maxEncodedLen)
				continue
			}

			if (maxEncodedLen == len(encoded)) && (int64(decLen) != fullDecodedLen) {
				tt.Errorf("f=%q, mEL=%d: full decode: got %d, want %d",
					filename, maxEncodedLen, decLen, fullDecodedLen)
				continue
			}

			if err := r.Close(); err != nil {
				tt.Errorf("f=%q, mEL=%d: r.Close: %v", filename, maxEncodedLen, err)
				continue
			}
		}
	}
}

func Benchmark(b *testing.B,
	smallestValidMaxEncodedLen int,
	cut func(io.Writer, []byte, int) (int, int, error),
	newReader func(io.Reader) (io.ReadCloser, error),
	filename string,
	trimPrefix int,
	trimSuffix int,
	decodedLen int64) {

	full, err := os.ReadFile("../../test/data/" + filename)
	if err != nil {
		b.Fatalf("ReadFile: %v", err)
	}

	if len(full) < (trimPrefix + trimSuffix) {
		b.Fatalf("len(full): got %d, want >= %d", len(full), trimPrefix+trimSuffix)
	}
	full = full[trimPrefix : len(full)-trimSuffix]

	if n, err := calculateDecodedLen(full, newReader); err != nil {
		b.Fatalf("calculateDecodedLen: %v", err)
	} else if n != decodedLen {
		b.Fatalf("calculateDecodedLen: got %d, want %d", n, decodedLen)
	}

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		for maxEncodedLen := 10; ; maxEncodedLen *= 10 {
			if maxEncodedLen < smallestValidMaxEncodedLen {
				continue
			}
			encoded := clone(full)
			if _, _, err := cut(nil, encoded, maxEncodedLen); err != nil {
				b.Fatalf("cut: %v", err)
			}
			if maxEncodedLen >= len(full) {
				break
			}
		}
	}
}
