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

// Package raczstd provides access to RAC (Random Access Compression) files
// with the Zstd compression codec.
//
// The RAC specification is at
// https://github.com/google/wuffs/blob/main/doc/spec/rac-spec.md
package raczstd

import (
	"bytes"
	"errors"
	"io"

	"github.com/google/wuffs/lib/cgozstd"
	"github.com/google/wuffs/lib/compression"
	"github.com/google/wuffs/lib/internal/racdict"
	"github.com/google/wuffs/lib/rac"
)

var (
	errCannotCut = errors.New("raczstd: cannot cut")
)

func refine(b []byte) []byte {
	if len(b) > racdict.MaxInclLength {
		return b[len(b)-racdict.MaxInclLength:]
	}
	return b
}

// CodecReader specializes a rac.Reader to decode Zstd-compressed chunks.
type CodecReader struct {
	// cachedReader lets us re-use the memory allocated for a zstd reader, when
	// decompressing multiple chunks.
	cachedReader compression.Reader
	recycler     cgozstd.ReaderRecycler

	// lim provides a limited view of a RAC file.
	lim io.LimitedReader

	// dictLoader loads shared dictionaries.
	dictLoader racdict.Loader
}

// Close implements rac.CodecReader.
func (r *CodecReader) Close() error {
	return r.recycler.Close()
}

// Accepts implements rac.CodecReader.
func (r *CodecReader) Accepts(c rac.Codec) bool {
	return c == rac.CodecZstandard
}

// Clone implements rac.CodecReader.
func (r *CodecReader) Clone() rac.CodecReader {
	return &CodecReader{}
}

// MakeDecompressor implements rac.CodecReader.
func (r *CodecReader) MakeDecompressor(racFile io.ReadSeeker, chunk rac.Chunk) (io.Reader, error) {
	dict, err := r.dictLoader.Load(racFile, chunk)
	if err != nil {
		return nil, err
	}
	if _, err := racFile.Seek(chunk.CPrimary[0], io.SeekStart); err != nil {
		return nil, err
	}
	r.lim.R = racFile
	r.lim.N = chunk.CPrimary.Size()

	if r.cachedReader == nil {
		zr := &cgozstd.Reader{}
		r.cachedReader = zr
		r.recycler.Bind(zr)
	}
	if err := r.cachedReader.Reset(&r.lim, dict); err != nil {
		return nil, err
	}
	return r.cachedReader, nil
}

// CodecWriter specializes a rac.Writer to encode Zstd-compressed chunks.
type CodecWriter struct {
	compressed   bytes.Buffer
	cachedWriter compression.Writer
	recycler     cgozstd.WriterRecycler

	// dictSaver saves shared dictionaries.
	dictSaver racdict.Saver
}

// Close implements rac.CodecWriter.
func (w *CodecWriter) Close() error {
	return w.recycler.Close()
}

// Clone implements rac.CodecWriter.
func (w *CodecWriter) Clone() rac.CodecWriter {
	return &CodecWriter{}
}

// Compress implements rac.CodecWriter.
func (w *CodecWriter) Compress(p []byte, q []byte, resourcesData [][]byte) (
	codec rac.Codec, compressed []byte, secondaryResource int, tertiaryResource int, retErr error) {
	return w.dictSaver.Compress(
		p, q, resourcesData,
		rac.CodecZstandard, w.compress, refine,
	)
}

func (w *CodecWriter) compress(p []byte, q []byte, dict []byte) ([]byte, error) {
	w.compressed.Reset()
	if w.cachedWriter == nil {
		zw := &cgozstd.Writer{}
		w.cachedWriter = zw
		w.recycler.Bind(zw)
	}
	if err := w.cachedWriter.Reset(&w.compressed, dict, compression.LevelSmall); err != nil {
		return nil, err
	}

	if len(p) > 0 {
		if _, err := w.cachedWriter.Write(p); err != nil {
			w.cachedWriter.Close()
			return nil, err
		}
	}
	if len(q) > 0 {
		if _, err := w.cachedWriter.Write(q); err != nil {
			w.cachedWriter.Close()
			return nil, err
		}
	}

	if err := w.cachedWriter.Close(); err != nil {
		return nil, err
	}
	return w.compressed.Bytes(), nil
}

// CanCut implements rac.CodecWriter.
func (w *CodecWriter) CanCut() bool {
	return false
}

// Cut implements rac.CodecWriter.
func (w *CodecWriter) Cut(codec rac.Codec, encoded []byte, maxEncodedLen int) (encodedLen int, decodedLen int, retErr error) {
	return 0, 0, errCannotCut
}

// WrapResource implements rac.CodecWriter.
func (w *CodecWriter) WrapResource(raw []byte) ([]byte, error) {
	return w.dictSaver.WrapResource(raw, refine)
}
