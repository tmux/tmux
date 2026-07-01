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

// rac-random-seek-test.go compresses stdin to the RAC file format, then checks
// that repeatedly randomly accessing that RAC file recovers the original input.
//
// Usage: go run rac-random-seek-test.go < input

import (
	"bytes"
	"fmt"
	"io"
	"math/rand"
	"os"

	"github.com/google/wuffs/lib/rac"
	"github.com/google/wuffs/lib/raczlib"
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	input, err := io.ReadAll(os.Stdin)
	if err != nil {
		return err
	}
	buf := &bytes.Buffer{}
	const dChunkSize = 256
	w := &rac.Writer{
		Writer:      buf,
		CodecWriter: &raczlib.CodecWriter{},
		DChunkSize:  dChunkSize,
	}
	if _, err := w.Write(input); err != nil {
		return err
	}
	if err := w.Close(); err != nil {
		return err
	}
	compressed := buf.Bytes()
	r := &rac.Reader{
		ReadSeeker:     bytes.NewReader(compressed),
		CompressedSize: int64(buf.Len()),
		CodecReaders:   []rac.CodecReader{&raczlib.CodecReader{}},
	}
	numChunks, err := countRACChunks(compressed)
	if err != nil {
		return err
	}
	if want := (len(input) + dChunkSize - 1) / dChunkSize; numChunks != want {
		return fmt.Errorf("numChunks: got %d, want %d", numChunks, want)
	}

	fmt.Printf("%8d uncompressed bytes\n", len(input))
	fmt.Printf("%8d   compressed bytes\n", len(compressed))
	fmt.Printf("%8d          RAC chunks\n", numChunks)

	got := make([]byte, len(input))
	if _, err := io.ReadFull(r, got); err != nil {
		return fmt.Errorf("ReadFull: %v", err)
	} else if !bytes.Equal(got, input) {
		return fmt.Errorf("ReadFull: mismatch")
	}

	for i, rng := uint64(0), rand.New(rand.NewSource(1)); ; i++ {
		m := rng.Intn(len(input) + 1)
		n := rng.Intn(len(input) + 1)
		if m > n {
			m, n = n, m
		}

		got = got[:n-m]
		if _, err := r.Seek(int64(m), io.SeekStart); err != nil {
			return fmt.Errorf("i=%d, m=%d, n=%d: Seek: %v", i, m, n, err)
		}
		if _, err := io.ReadFull(r, got); err != nil {
			return fmt.Errorf("i=%d, m=%d, n=%d: ReadFull: %v", i, m, n, err)
		}

		want := input[m:n]
		if !bytes.Equal(got, want) {
			return fmt.Errorf("i=%d, m=%d, n=%d: mismatch", i, m, n)
		}

		if i%1024 == 0 {
			fmt.Printf("i=%d: ok\n", i)
		}
	}
}

func countRACChunks(compressed []byte) (int, error) {
	r := &rac.ChunkReader{
		ReadSeeker:     bytes.NewReader(compressed),
		CompressedSize: int64(len(compressed)),
	}
	for n := 0; ; n++ {
		if _, err := r.NextChunk(); err == io.EOF {
			return n, nil
		} else if err != nil {
			return 0, err
		}
	}
}
